/**
 * @file main_gui.cpp
 * @brief Qt6 QML GUI Client application entry point.
 */

#include "CommunicationBridge.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <glog/logging.h>

int main(int argc, char* argv[])
{
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1;

    QGuiApplication app(argc, argv);
    app.setOrganizationName("CommunicationLib");
    app.setApplicationName("Qt6 Communication Client");

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/qt/qml/CommunicationGui/gui/Main.qml"));

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
