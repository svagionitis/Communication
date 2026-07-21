/**
 * @file CommunicationBridge.h
 * @brief Qt QObject bridge connecting QML frontend to polymorphic ICommunication backend.
 */

#pragma once

#include "ICommunication.h"
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <memory>
#include <mutex>

namespace Communication {
namespace Gui {

/**
 * @brief Qt Bridge class managing polymorphic ICommunication instances for Qt Quick QML.
 */
class CommunicationBridge : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionStatusChanged)
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString serialDevice READ serialDevice WRITE setSerialDevice NOTIFY serialDeviceChanged)
    Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate NOTIFY baudRateChanged)
    Q_PROPERTY(QString flowControl READ flowControl WRITE setFlowControl NOTIFY flowControlChanged)
    Q_PROPERTY(bool autoReconnect READ autoReconnect WRITE setAutoReconnect NOTIFY autoReconnectChanged)
    Q_PROPERTY(QString logOutput READ logOutput NOTIFY logOutputChanged)

public:
    explicit CommunicationBridge(QObject* parent = nullptr);
    ~CommunicationBridge() override;

    QString mode() const;
    void setMode(const QString& mode);

    bool isConnected() const;

    QString host() const;
    void setHost(const QString& host);

    int port() const;
    void setPort(int port);

    QString serialDevice() const;
    void setSerialDevice(const QString& device);

    int baudRate() const;
    void setBaudRate(int baud);

    QString flowControl() const;
    void setFlowControl(const QString& flow);

    bool autoReconnect() const;
    void setAutoReconnect(bool enable);

    QString logOutput() const;

    Q_INVOKABLE bool connectChannel();
    Q_INVOKABLE void disconnectChannel();
    Q_INVOKABLE bool sendMessage(const QString& message);
    Q_INVOKABLE void clearLogs();

signals:
    void modeChanged();
    void connectionStatusChanged();
    void hostChanged();
    void portChanged();
    void serialDeviceChanged();
    void baudRateChanged();
    void flowControlChanged();
    void autoReconnectChanged();
    void logOutputChanged();
    void dataReceived(const QString& text);

private:
    void handleDataReceived(const std::vector<uint8_t>& data);
    void appendLog(const QString& text);

    QString m_mode {"TCP"};
    QString m_host {"127.0.0.1"};
    int m_port {8080};
    QString m_serialDevice {"/dev/ttyUSB0"};
    int m_baudRate {115200};
    QString m_flowControl {"None"};
    bool m_autoReconnect {false};
    QString m_logOutput;

    mutable std::mutex m_channelMutex;
    std::unique_ptr<ICommunication> m_channel;
};

} // namespace Gui
} // namespace Communication
