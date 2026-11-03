#include "VideoProcessor.h"
#include "WhiteBalance.h"

#include <QFileInfo>
#include <opencv2/imgproc.hpp>

VideoProcessor::VideoProcessor(QObject* parent)
    : QObject(parent)
{
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &VideoProcessor::onTimer);
}

void VideoProcessor::openFile(const QString& path)
{
    pause();
    m_cap.release();

    if (!m_cap.open(path.toStdString())) {
        emit errorOccurred(QStringLiteral("Не удалось открыть видео: %1").arg(path));
        emit openedChanged();
        return;
    }

    m_fileName = QFileInfo(path).fileName();
    m_fps = m_cap.get(cv::CAP_PROP_FPS);
    if (!(m_fps > 1.0 && m_fps < 240.0))
        m_fps = 25.0;
    m_frameCount = static_cast<int>(m_cap.get(cv::CAP_PROP_FRAME_COUNT));
    m_position = 0;

    cv::Mat first;
    if (m_cap.read(first))
        emitFrame(first);

    emit openedChanged();
    emit positionChanged();
}

void VideoProcessor::play()
{
    if (!m_cap.isOpened() || m_timer.isActive())
        return;
    m_timer.start(qRound(1000.0 / m_fps));
    emit playingChanged();
}

void VideoProcessor::pause()
{
    if (!m_timer.isActive())
        return;
    m_timer.stop();
    emit playingChanged();
}

void VideoProcessor::stop()
{
    pause();
    seekFrame(0);
}

void VideoProcessor::seekFrame(int frame)
{
    if (!m_cap.isOpened())
        return;
    pause();
    frame = qBound(0, frame, qMax(0, m_frameCount - 1));
    m_cap.set(cv::CAP_PROP_POS_FRAMES, frame);
    m_position = frame;
    cv::Mat f;
    if (m_cap.read(f)) {
        emitFrame(f);
        m_position = frame + 1;
    }
    emit positionChanged();
}

void VideoProcessor::applyWhiteBalance(int algorithm, double strength,
                                       double percentile, double pnorm, int derivOrder)
{
    if (!m_cap.isOpened())
        return;

    // Берём текущий кадр как эталонный — без перемотки воспроизведения
    m_cap.set(cv::CAP_PROP_POS_FRAMES, qMax(0, m_position - 1));
    cv::Mat ref;
    if (!m_cap.read(ref))
        return;
    m_cap.set(cv::CAP_PROP_POS_FRAMES, qMax(0, m_position - 1)); // вернём позицию

    wb::Params p;
    p.strength   = strength;
    p.percentile = percentile;
    p.pnorm      = pnorm;
    p.derivOrder = derivOrder;

    m_gains = wb::computeGains(ref, algorithm, p);
    rebuildLut();
    emitFrame(ref); // сразу показать результат на эталонном кадре
    emit gainsChanged();
}

void VideoProcessor::resetWhiteBalance()
{
    m_gains = cv::Vec3d(1.0, 1.0, 1.0);
    m_hasLut = false;
    emit gainsChanged();
}

void VideoProcessor::onTimer()
{
    cv::Mat frame;
    if (!m_cap.read(frame)) { // конец файла
        pause();
        m_cap.set(cv::CAP_PROP_POS_FRAMES, 0);
        m_position = 0;
        emit positionChanged();
        return;
    }
    emitFrame(frame);
    ++m_position;
    emit positionChanged();
}

void VideoProcessor::emitFrame(const cv::Mat& bgr)
{
    cv::Mat out;
    if (m_hasLut) {
        cv::LUT(bgr, m_lut, out);          // быстро: одна таблица на все каналы
    } else {
        out = bgr;
    }

    cv::Mat rgb;
    cv::cvtColor(out, rgb, cv::COLOR_BGR2RGB);
    QImage img(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step),
               QImage::Format_RGB888);
    emit frameReady(img.copy()); // копия: rgb локальна и умрёт в конце блока
}

void VideoProcessor::rebuildLut()
{
    m_lut = wb::buildLut(m_gains);
    m_hasLut = true;
}
