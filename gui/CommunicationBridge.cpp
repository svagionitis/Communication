/**
 * @file CommunicationBridge.cpp
 * @brief Implementation of Qt QML bridge connecting backend ICommunication channels safely across threads.
 */

#include "CommunicationBridge.h"
#include "SerialPort.h"
#include "TcpClient.h"
#include "UdpSocket.h"
#include <QMetaObject>
#include <glog/logging.h>

namespace Communication {
namespace Gui {

CommunicationBridge::CommunicationBridge(QObject* parent)
    : QObject(parent)
{
}

CommunicationBridge::~CommunicationBridge()
{
    disconnectChannel();
}

QString CommunicationBridge::mode() const
{
    return m_mode;
}

void CommunicationBridge::setMode(const QString& mode)
{
    if (m_mode != mode) {
        m_mode = mode;
        emit modeChanged();
    }
}

bool CommunicationBridge::isConnected() const
{
    std::lock_guard<std::mutex> lock(m_channelMutex);
    return m_channel && m_channel->isOpen();
}

QString CommunicationBridge::host() const
{
    return m_host;
}

void CommunicationBridge::setHost(const QString& host)
{
    if (m_host != host) {
        m_host = host;
        emit hostChanged();
    }
}

int CommunicationBridge::port() const
{
    return m_port;
}

void CommunicationBridge::setPort(int port)
{
    if (m_port != port) {
        m_port = port;
        emit portChanged();
    }
}

QString CommunicationBridge::serialDevice() const
{
    return m_serialDevice;
}

void CommunicationBridge::setSerialDevice(const QString& device)
{
    if (m_serialDevice != device) {
        m_serialDevice = device;
        emit serialDeviceChanged();
    }
}

int CommunicationBridge::baudRate() const
{
    return m_baudRate;
}

void CommunicationBridge::setBaudRate(int baud)
{
    if (m_baudRate != baud) {
        m_baudRate = baud;
        emit baudRateChanged();
    }
}

QString CommunicationBridge::logOutput() const
{
    return m_logOutput;
}

bool CommunicationBridge::connectChannel()
{
    std::lock_guard<std::mutex> lock(m_channelMutex);
    if (m_channel && m_channel->isOpen()) {
        return true;
    }

    if (m_mode.toUpper() == "TCP") {
        TcpConfig config;
        config.host = m_host.toStdString();
        config.port = static_cast<uint16_t>(m_port);
        m_channel = std::make_unique<TcpClient>(config);
    } else if (m_mode.toUpper() == "UDP") {
        UdpConfig config;
        config.remoteHost = m_host.toStdString();
        config.remotePort = static_cast<uint16_t>(m_port);
        m_channel = std::make_unique<UdpSocket>(config);
    } else if (m_mode.toUpper() == "SERIAL") {
        SerialConfig config;
        config.portName = m_serialDevice.toStdString();
        config.baudRate = static_cast<BaudRate>(m_baudRate);
        m_channel = std::make_unique<SerialPort>(config);
    } else {
        appendLog("[Bridge] Unknown mode requested: " + m_mode);
        return false;
    }

    m_channel->registerReceiveCallback([this](const std::vector<uint8_t>& data) { handleDataReceived(data); });

    bool success = m_channel->open();
    if (success) {
        appendLog(QString("[Bridge] Connected successfully (%1 mode)").arg(m_mode));
    } else {
        appendLog(QString("[Bridge] Failed to connect (%1 mode)").arg(m_mode));
        m_channel.reset();
    }

    emit connectionStatusChanged();
    return success;
}

void CommunicationBridge::disconnectChannel()
{
    {
        std::lock_guard<std::mutex> lock(m_channelMutex);
        if (m_channel) {
            m_channel->close();
            m_channel.reset();
        }
    }
    appendLog("[Bridge] Disconnected.");
    emit connectionStatusChanged();
}

bool CommunicationBridge::sendMessage(const QString& message)
{
    std::lock_guard<std::mutex> lock(m_channelMutex);
    if (!m_channel || !m_channel->isOpen()) {
        appendLog("[Bridge] Cannot send message: Channel is disconnected.");
        return false;
    }

    std::string text = message.toStdString();
    bool sent = m_channel->send(text);
    if (sent) {
        appendLog("[TX]: " + message);
    } else {
        appendLog("[Error]: Failed to send message.");
    }
    return sent;
}

void CommunicationBridge::clearLogs()
{
    m_logOutput.clear();
    emit logOutputChanged();
}

void CommunicationBridge::handleDataReceived(const std::vector<uint8_t>& data)
{
    QString text = QString::fromUtf8(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()));

    // Safely invoke on Qt UI main thread
    QMetaObject::invokeMethod(
        this,
        [this, text]() {
            appendLog("[RX]: " + text);
            emit dataReceived(text);
        },
        Qt::QueuedConnection);
}

void CommunicationBridge::appendLog(const QString& text)
{
    m_logOutput.append(text + "\n");
    emit logOutputChanged();
}

} // namespace Gui
} // namespace Communication
