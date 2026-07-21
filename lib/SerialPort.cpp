/**
 * @file SerialPort.cpp
 * @brief Cross-platform Serial Port implementation using Win32 API and termios.
 */

#include "SerialPort.h"
#include <array>
#include <cmath>
#include <cstring>
#include <glog/logging.h>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace Communication {

SerialPort::SerialPort(SerialConfig config)
    : m_config(std::move(config))
{
}

SerialPort::~SerialPort()
{
    close();
}

SerialPort::SerialPort(SerialPort&& other) noexcept
{
    std::lock_guard<std::mutex> lockOtherPort(other.m_portMutex);
    std::lock_guard<std::mutex> lockOtherConfig(other.m_configMutex);

    other.stopReceiveThread();

    m_config = std::move(other.m_config);
    m_handle = other.m_handle;
    m_isOpen.store(other.m_isOpen.load());

    other.m_handle = INVALID_SERIAL_HANDLE;
    other.m_isOpen.store(false);

    {
        std::lock_guard<std::mutex> lockOtherCb(other.m_callbackMutex);
        m_callback = std::move(other.m_callback);
    }

    if (m_isOpen.load()) {
        m_isRunning.store(true);
        m_receiveThread = std::thread(&SerialPort::receiveLoop, this);
    }
}

SerialPort& SerialPort::operator=(SerialPort&& other) noexcept
{
    if (this != &other) {
        close();

        std::lock_guard<std::mutex> lockOtherPort(other.m_portMutex);
        std::lock_guard<std::mutex> lockOtherConfig(other.m_configMutex);

        other.stopReceiveThread();

        m_config = std::move(other.m_config);
        m_handle = other.m_handle;
        m_isOpen.store(other.m_isOpen.load());

        other.m_handle = INVALID_SERIAL_HANDLE;
        other.m_isOpen.store(false);

        {
            std::lock_guard<std::mutex> lockOtherCb(other.m_callbackMutex);
            m_callback = std::move(other.m_callback);
        }

        if (m_isOpen.load()) {
            m_isRunning.store(true);
            m_receiveThread = std::thread(&SerialPort::receiveLoop, this);
        }
    }
    return *this;
}

void SerialPort::registerConnectionStateCallback(ConnectionStateCallback callback)
{
    std::lock_guard<std::mutex> lock(m_stateCallbackMutex);
    m_stateCallback = std::move(callback);
}

void SerialPort::notifyConnectionState(bool connected)
{
    ConnectionStateCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_stateCallbackMutex);
        cb = m_stateCallback;
    }
    if (cb) {
        cb(connected);
    }
}

bool SerialPort::openInternal()
{
    std::lock_guard<std::mutex> lockPort(m_portMutex);
    if (m_isOpen.load()) {
        return true;
    }

    SerialConfig cfg;
    {
        std::lock_guard<std::mutex> lockConfig(m_configMutex);
        cfg = m_config;
    }

#ifdef _WIN32
    std::string portPath = cfg.portName;
    if (portPath.rfind("\\\\.\\", 0) != 0 && portPath.rfind("COM", 0) == 0) {
        portPath = "\\\\.\\" + portPath;
    }

    HANDLE hSerial = CreateFileA(portPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hSerial == INVALID_HANDLE_VALUE) {
        LOG(ERROR) << "SerialPort failed to open port " << cfg.portName;
        return false;
    }

    m_handle = hSerial;
#else
    int fd = ::open(cfg.portName.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        LOG(ERROR) << "SerialPort failed to open device " << cfg.portName;
        return false;
    }

    // Reset flags to blocking mode for read timeouts
    fcntl(fd, F_SETFL, 0);
    m_handle = fd;
#endif

    if (!configurePlatformPort()) {
#ifdef _WIN32
        CloseHandle(m_handle);
#else
        ::close(m_handle);
#endif
        m_handle = INVALID_SERIAL_HANDLE;
        return false;
    }

    m_isOpen.store(true);
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_connectTime = std::chrono::system_clock::now();
    }
    LOG(INFO) << "SerialPort " << cfg.portName << " opened successfully.";
    return true;
}

bool SerialPort::open()
{
    if (m_isOpen.load()) {
        LOG(INFO) << "SerialPort already open.";
        return true;
    }

    if (!openInternal()) {
        return false;
    }

    m_isRunning.store(true);
    m_receiveThread = std::thread(&SerialPort::receiveLoop, this);
    notifyConnectionState(true);
    return true;
}

bool SerialPort::connect()
{
    return open();
}

void SerialPort::close()
{
    stopReceiveThread();

    std::lock_guard<std::mutex> lockPort(m_portMutex);
    if (m_handle != INVALID_SERIAL_HANDLE) {
#ifdef _WIN32
        CloseHandle(m_handle);
#else
        ::close(m_handle);
#endif
        m_handle = INVALID_SERIAL_HANDLE;
    }
    bool wasOpen = m_isOpen.exchange(false);
    if (wasOpen) {
        notifyConnectionState(false);
    }
    LOG(INFO) << "SerialPort closed.";
}

void SerialPort::disconnect()
{
    close();
}

bool SerialPort::send(const std::vector<uint8_t>& data)
{
    if (data.empty()) {
        return true;
    }
    return send(std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
}

bool SerialPort::send(std::string_view data)
{
    std::lock_guard<std::mutex> lockPort(m_portMutex);
    if (!m_isOpen.load() || m_handle == INVALID_SERIAL_HANDLE) {
        LOG(ERROR) << "SerialPort send failed: Port is not open.";
        return false;
    }

    bool success = false;
    size_t bytesSentCount = 0;

#ifdef _WIN32
    DWORD bytesWritten = 0;
    BOOL result = WriteFile(m_handle, data.data(), static_cast<DWORD>(data.size()), &bytesWritten, nullptr);
    success = result && (bytesWritten == data.size());
    bytesSentCount = static_cast<size_t>(bytesWritten);
#else
    ssize_t bytesWritten = ::write(m_handle, data.data(), data.size());
    success = (bytesWritten == static_cast<ssize_t>(data.size()));
    if (bytesWritten > 0) {
        bytesSentCount = static_cast<size_t>(bytesWritten);
    }
#endif

    if (bytesSentCount > 0) {
        m_bytesSent.fetch_add(bytesSentCount);
        m_packetsSent.fetch_add(1);
    }
    return success;
}

void SerialPort::registerReceiveCallback(DataReceivedCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_callback = std::move(callback);
}

void SerialPort::registerReceiveViewCallback(DataViewCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_viewCallback = std::move(callback);
}

bool SerialPort::isOpen() const
{
    return m_isOpen.load();
}

bool SerialPort::isConnected() const
{
    return isOpen();
}

ConnectionStats SerialPort::stats() const
{
    ConnectionStats s;
    s.bytesSent = m_bytesSent.load();
    s.bytesReceived = m_bytesReceived.load();
    s.packetsSent = m_packetsSent.load();
    s.packetsReceived = m_packetsReceived.load();
    s.reconnectCount = m_reconnectCount.load();

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        s.connectTimestamp = m_connectTime;
    }

    if (m_isOpen.load() && s.connectTimestamp != std::chrono::system_clock::time_point {}) {
        auto durationSec = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::system_clock::now() -
                                                                                     s.connectTimestamp)
                               .count();
        if (durationSec > 0.0001) {
            s.sendBytesPerSec = static_cast<double>(s.bytesSent) / durationSec;
            s.rxBytesPerSec = static_cast<double>(s.bytesReceived) / durationSec;
        }
    }
    return s;
}

void SerialPort::resetStats()
{
    m_bytesSent.store(0);
    m_bytesReceived.store(0);
    m_packetsSent.store(0);
    m_packetsReceived.store(0);
    m_reconnectCount.store(0);
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_connectTime = std::chrono::system_clock::now();
    }
}

void SerialPort::setConfig(const SerialConfig& config)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config = config;
}

SerialConfig SerialPort::config() const
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

void SerialPort::stopReceiveThread()
{
    if (m_isRunning.exchange(false)) {
        m_reconnectCv.notify_all();
        if (m_receiveThread.joinable()) {
            m_receiveThread.join();
        }
    }
}

bool SerialPort::configurePlatformPort()
{
    SerialConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        cfg = m_config;
    }

#ifdef _WIN32
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(m_handle, &dcbSerialParams)) {
        LOG(ERROR) << "SerialPort GetCommState failed.";
        return false;
    }

    dcbSerialParams.BaudRate = static_cast<DWORD>(cfg.baudRate);

    switch (cfg.dataBits) {
    case DataBits::Five:
        dcbSerialParams.ByteSize = 5;
        break;
    case DataBits::Six:
        dcbSerialParams.ByteSize = 6;
        break;
    case DataBits::Seven:
        dcbSerialParams.ByteSize = 7;
        break;
    case DataBits::Eight:
    default:
        dcbSerialParams.ByteSize = 8;
        break;
    }

    switch (cfg.stopBits) {
    case StopBits::One:
        dcbSerialParams.StopBits = ONESTOPBIT;
        break;
    case StopBits::OnePointFive:
        dcbSerialParams.StopBits = ONE5STOPBITS;
        break;
    case StopBits::Two:
        dcbSerialParams.StopBits = TWOSTOPBITS;
        break;
    }

    switch (cfg.parity) {
    case Parity::None:
        dcbSerialParams.Parity = NOPARITY;
        break;
    case Parity::Odd:
        dcbSerialParams.Parity = ODDPARITY;
        break;
    case Parity::Even:
        dcbSerialParams.Parity = EVENPARITY;
        break;
    case Parity::Mark:
        dcbSerialParams.Parity = MARKPARITY;
        break;
    case Parity::Space:
        dcbSerialParams.Parity = SPACEPARITY;
        break;
    }

    switch (cfg.flowControl) {
    case FlowControl::Hardware:
        dcbSerialParams.fOutxCtsFlow = TRUE;
        dcbSerialParams.fRtsControl = RTS_CONTROL_HANDSHAKE;
        dcbSerialParams.fOutX = FALSE;
        dcbSerialParams.fInX = FALSE;
        break;
    case FlowControl::Software:
        dcbSerialParams.fOutxCtsFlow = FALSE;
        dcbSerialParams.fRtsControl = RTS_CONTROL_DISABLE;
        dcbSerialParams.fOutX = TRUE;
        dcbSerialParams.fInX = TRUE;
        break;
    case FlowControl::None:
    default:
        dcbSerialParams.fOutxCtsFlow = FALSE;
        dcbSerialParams.fRtsControl = RTS_CONTROL_DISABLE;
        dcbSerialParams.fOutX = FALSE;
        dcbSerialParams.fInX = FALSE;
        break;
    }

    if (!SetCommState(m_handle, &dcbSerialParams)) {
        LOG(ERROR) << "SerialPort SetCommState failed.";
        return false;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = cfg.timeoutMs;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 100;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(m_handle, &timeouts)) {
        LOG(ERROR) << "SerialPort SetCommTimeouts failed.";
        return false;
    }
#else
    struct termios options { };
    if (tcgetattr(m_handle, &options) != 0) {
        LOG(ERROR) << "SerialPort tcgetattr failed.";
        return false;
    }

    speed_t speed;
    switch (static_cast<uint32_t>(cfg.baudRate)) {
    case 110:
        speed = B110;
        break;
    case 300:
        speed = B300;
        break;
    case 600:
        speed = B600;
        break;
    case 1200:
        speed = B1200;
        break;
    case 2400:
        speed = B2400;
        break;
    case 4800:
        speed = B4800;
        break;
    case 9600:
        speed = B9600;
        break;
    case 19200:
        speed = B19200;
        break;
    case 38400:
        speed = B38400;
        break;
    case 57600:
        speed = B57600;
        break;
    case 115200:
        speed = B115200;
        break;
    case 230400:
        speed = B230400;
        break;
#ifdef B460800
    case 460800:
        speed = B460800;
        break;
#endif
#ifdef B500000
    case 500000:
        speed = B500000;
        break;
#endif
#ifdef B576000
    case 576000:
        speed = B576000;
        break;
#endif
#ifdef B921600
    case 921600:
        speed = B921600;
        break;
#endif
#ifdef B1000000
    case 1000000:
        speed = B1000000;
        break;
#endif
#ifdef B1152000
    case 1152000:
        speed = B1152000;
        break;
#endif
#ifdef B1500000
    case 1500000:
        speed = B1500000;
        break;
#endif
#ifdef B2000000
    case 2000000:
        speed = B2000000;
        break;
#endif
#ifdef B2500000
    case 2500000:
        speed = B2500000;
        break;
#endif
#ifdef B3000000
    case 3000000:
        speed = B3000000;
        break;
#endif
#ifdef B3500000
    case 3500000:
        speed = B3500000;
        break;
#endif
#ifdef B4000000
    case 4000000:
        speed = B4000000;
        break;
#endif
    default:
        speed = B115200;
        break;
    }

    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);

    options.c_cflag &= ~CSIZE;
    switch (cfg.dataBits) {
    case DataBits::Five:
        options.c_cflag |= CS5;
        break;
    case DataBits::Six:
        options.c_cflag |= CS6;
        break;
    case DataBits::Seven:
        options.c_cflag |= CS7;
        break;
    case DataBits::Eight:
    default:
        options.c_cflag |= CS8;
        break;
    }

    switch (cfg.parity) {
    case Parity::None:
        options.c_cflag &= ~PARENB;
        break;
    case Parity::Odd:
        options.c_cflag |= (PARENB | PARODD);
        break;
    case Parity::Even:
        options.c_cflag |= PARENB;
        options.c_cflag &= ~PARODD;
        break;
    default:
        options.c_cflag &= ~PARENB;
        break;
    }

    if (cfg.stopBits == StopBits::Two) {
        options.c_cflag |= CSTOPB;
    } else {
        options.c_cflag &= ~CSTOPB;
    }

    switch (cfg.flowControl) {
    case FlowControl::Hardware:
#ifdef CRTSCTS
        options.c_cflag |= CRTSCTS;
#endif
        options.c_iflag &= ~(IXON | IXOFF | IXANY);
        break;
    case FlowControl::Software:
#ifdef CRTSCTS
        options.c_cflag &= ~CRTSCTS;
#endif
        options.c_iflag |= (IXON | IXOFF);
        break;
    case FlowControl::None:
    default:
#ifdef CRTSCTS
        options.c_cflag &= ~CRTSCTS;
#endif
        options.c_iflag &= ~(IXON | IXOFF | IXANY);
        break;
    }

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;

    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = static_cast<cc_t>(cfg.timeoutMs / 100);

    if (tcsetattr(m_handle, TCSANOW, &options) != 0) {
        LOG(ERROR) << "SerialPort tcsetattr failed.";
        return false;
    }
#endif

    return true;
}

void SerialPort::receiveLoop()
{
    std::array<uint8_t, 1024> buffer;

    while (m_isRunning.load()) {
        SerialHandle h;
        {
            std::lock_guard<std::mutex> lock(m_portMutex);
            h = m_handle;
        }

        if (h == INVALID_SERIAL_HANDLE) {
            break;
        }

        bool ioError = false;

#ifdef _WIN32
        DWORD bytesRead = 0;
        BOOL success = ReadFile(h, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr);
        if (success) {
            if (bytesRead > 0) {
                m_bytesReceived.fetch_add(static_cast<uint64_t>(bytesRead));
                m_packetsReceived.fetch_add(1);
                DataReceivedCallback cb;
                DataViewCallback vcb;
                {
                    std::lock_guard<std::mutex> lock(m_callbackMutex);
                    cb = m_callback;
                    vcb = m_viewCallback;
                }
                if (vcb) {
                    vcb(buffer.data(), static_cast<size_t>(bytesRead));
                }
                if (cb) {
                    std::vector<uint8_t> data(buffer.begin(), buffer.begin() + bytesRead);
                    cb(data);
                }
            }
        } else {
            if (m_isRunning.load()) {
                LOG(ERROR) << "SerialPort ReadFile error: " << GetLastError();
                ioError = true;
            }
        }
#else
        ssize_t bytesRead = ::read(h, buffer.data(), buffer.size());
        if (bytesRead > 0) {
            m_bytesReceived.fetch_add(static_cast<uint64_t>(bytesRead));
            m_packetsReceived.fetch_add(1);
            DataReceivedCallback cb;
            DataViewCallback vcb;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_callback;
                vcb = m_viewCallback;
            }
            if (vcb) {
                vcb(buffer.data(), static_cast<size_t>(bytesRead));
            }
            if (cb) {
                std::vector<uint8_t> data(buffer.begin(), buffer.begin() + bytesRead);
                cb(data);
            }
        } else if (bytesRead < 0 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
            if (m_isRunning.load()) {
                LOG(ERROR) << "SerialPort read error: " << strerror(errno);
                ioError = true;
            }
        }
#endif

        if (ioError) {
            m_isOpen.store(false);
            notifyConnectionState(false);

            SerialConfig cfg;
            {
                std::lock_guard<std::mutex> lockConfig(m_configMutex);
                cfg = m_config;
            }

            if (!cfg.autoReconnect.enable || !m_isRunning.load()) {
                break;
            }

            uint32_t attempt = 0;
            bool reconnected = false;

            while (m_isRunning.load()) {
                attempt++;
                m_reconnectCount.fetch_add(1);
                if (cfg.autoReconnect.maxRetries > 0 && attempt > cfg.autoReconnect.maxRetries) {
                    LOG(WARNING) << "SerialPort max reconnect retries reached (" << cfg.autoReconnect.maxRetries
                                 << ").";
                    break;
                }

                double delay = cfg.autoReconnect.initialDelayMs *
                               std::pow(cfg.autoReconnect.backoffMultiplier, static_cast<double>(attempt - 1));
                uint32_t delayMs =
                    static_cast<uint32_t>(std::min(static_cast<double>(cfg.autoReconnect.maxDelayMs), delay));
                LOG(INFO) << "SerialPort reconnect attempt " << attempt << " in " << delayMs << " ms...";

                std::unique_lock<std::mutex> lockRec(m_reconnectMutex);
                if (m_reconnectCv.wait_for(lockRec, std::chrono::milliseconds(delayMs),
                                           [this] { return !m_isRunning.load(); })) {
                    break;
                }

                if (openInternal()) {
                    LOG(INFO) << "SerialPort successfully reconnected on attempt " << attempt;
                    notifyConnectionState(true);
                    reconnected = true;
                    break;
                }
            }

            if (!reconnected) {
                break;
            }
        }
    }
}

} // namespace Communication
