#pragma once

#include <QObject>
#include <QImage>
#include <QTimer>
#include <QString>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

namespace wb { struct Params; }

// Читает видео через OpenCV в отдельном потоке и отдаёт кадры в QML.
//
// Ключевая идея против «плавания» оттенков: множители каналов вычисляются
// ОДИН раз по эталонному кадру (в момент нажатия «Применить»), превращаются
// в таблицу поиска (LUT) и дальше применяются к каждому кадру без изменений.
// Пока пользователь не нажмёт «Применить» снова, коррекция строго постоянна.
class VideoProcessor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool opened  READ opened  NOTIFY openedChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(int  frameCount READ frameCount NOTIFY openedChanged)
    Q_PROPERTY(int  position READ position NOTIFY positionChanged)
    Q_PROPERTY(double fps READ fps NOTIFY openedChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY openedChanged)
    Q_PROPERTY(double gainB READ gainB NOTIFY gainsChanged)
    Q_PROPERTY(double gainG READ gainG NOTIFY gainsChanged)
    Q_PROPERTY(double gainR READ gainR NOTIFY gainsChanged)

public:
    explicit VideoProcessor(QObject* parent = nullptr);

    bool opened() const    { return m_cap.isOpened(); }
    bool playing() const   { return m_timer.isActive(); }
    int  frameCount() const{ return m_frameCount; }
    int  position() const  { return m_position; }
    double fps() const     { return m_fps; }
    QString fileName() const { return m_fileName; }
    double gainB() const   { return m_gains[0]; }
    double gainG() const   { return m_gains[1]; }
    double gainR() const   { return m_gains[2]; }

public slots:
    void openFile(const QString& path);
    void play();
    void pause();
    void stop();
    void seekFrame(int frame);

    // Вычисляет множители по ТЕКУЩЕМУ кадру и фиксирует их на всё видео.
    void applyWhiteBalance(int algorithm, double strength,
                           double percentile, double pnorm, int derivOrder);
    // Сброс коррекции — показ исходного видео.
    void resetWhiteBalance();

signals:
    void frameReady(const QImage& image);
    void openedChanged();
    void playingChanged();
    void positionChanged();
    void gainsChanged();
    void errorOccurred(const QString& message);

private slots:
    void onTimer();

private:
    void emitFrame(const cv::Mat& bgr);
    void rebuildLut();

    cv::VideoCapture m_cap;
    QTimer m_timer;
    cv::Mat m_lut;        // 1x256 CV_8UC3, пересобирается только при applyWhiteBalance
    bool m_hasLut = false;
    cv::Vec3d m_gains{1.0, 1.0, 1.0};

    QString m_fileName;
    double m_fps = 25.0;
    int m_frameCount = 0;
    int m_position = 0;
};
