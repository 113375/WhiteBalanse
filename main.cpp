#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QThread>
#include "VideoProcessor.h"

int main(int argc, char* argv[])
{
    // QApplication (а не QGuiApplication): Qt.labs.platform использует виджеты
    // для нативных диалогов открытия файла.
    QApplication app(argc, argv);

    VideoProcessor processor;
    QThread worker;
    processor.moveToThread(&worker);
    QObject::connect(&app, &QApplication::aboutToQuit, [&]() {
        worker.quit();
        worker.wait();
    });
    worker.start();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("processor"), &processor);

    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject* obj, const QUrl& objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
