#include "WhiteBalance.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace wb {

QString algorithmName(int algorithm)
{
    switch (algorithm) {
    case GreyWorld:      return QStringLiteral("Серый мир (Grey World)");
    case WhitePatch:     return QStringLiteral("Белое пятно (White Patch)");
    case PerfectReflector: return QStringLiteral("Идеальный отражатель (Perfect Reflector)");
    case ShadesOfGrey:   return QStringLiteral("Оттенки серого (Shades of Grey)");
    case GreyEdge:       return QStringLiteral("Серая граница (Grey Edge)");
    default:             return QStringLiteral("Неизвестно");
    }
}

QString algorithmDescription(int algorithm)
{
    switch (algorithm) {
    case GreyWorld:
        return QStringLiteral("Предполагает, что среднее значение всех пикселей кадра — "
                              "нейтрально-серое. Каждый канал растягивается так, чтобы "
                              "средние совпали. Хорошо работает на богатых цветами сценах.");
    case WhitePatch:
        return QStringLiteral("Считает, что самый яркий пиксель кадра — белый: каждый канал "
                              "масштабируется по своему максимуму. Эффективен, когда в кадре "
                              "есть близкий к белому объект, иначе возможны пересветы.");
    case PerfectReflector:
        return QStringLiteral("Усовершенствованный White Patch: вместо абсолютного максимума "
                              "используется среднее верхних N% самых ярких пикселей — "
                              "устойчивее к шуму и выбросам.");
    case ShadesOfGrey:
        return QStringLiteral("Обобщение Grey World через норму Минковского степени p: "
                              "p=1 — это Grey World, p→∞ — White Patch. При p=4…10 обычно "
                              "даёт наиболее естественные оттенки.");
    case GreyEdge:
        return QStringLiteral("Метод van de Weijer et al.: норма Минковского вычисляется не "
                              "по яркостям, а по производным изображения (границам объектов). "
                              "Хорошо сохраняет насыщенные цвета, так как опирается на "
                              "нейтральные края сцены.");
    default:
        return QString();
    }
}

// Уменьшаем кадр — статистика по 320×180 достаточно стабильна и считается мгновенно.
static cv::Mat downscale(const cv::Mat& bgr)
{
    cv::Mat small;
    const double maxDim = 320.0;
    double scale = std::min(1.0, maxDim / std::max(bgr.cols, bgr.rows));
    if (scale < 1.0)
        cv::resize(bgr, small, cv::Size(), scale, scale, cv::INTER_AREA);
    else
        small = bgr;
    return small;
}

// Норма Минковского степени p для одного канала.
static double minkowskiNorm(const cv::Mat& ch, double p)
{
    CV_Assert(ch.type() == CV_8UC1);
    double acc = 0.0;
    const int n = ch.rows * ch.cols;
    const uchar* data = ch.ptr<uchar>(0);
    for (int i = 0; i < n; ++i)
        acc += std::pow(static_cast<double>(data[i]), p);
    return std::pow(acc / n, 1.0 / p);
}

// Среднее значение верхних pct%% пикселей канала.
static double topPercentMean(const cv::Mat& ch, double pct)
{
    std::vector<uchar> vals(ch.ptr<uchar>(0), ch.ptr<uchar>(0) + ch.rows * ch.cols);
    size_t k = std::max<size_t>(1, static_cast<size_t>(vals.size() * std::min(50.0, pct) / 100.0));
    std::nth_element(vals.begin(), vals.begin() + k, vals.end(), std::greater<uchar>());
    double sum = 0.0;
    for (size_t i = 0; i < k; ++i)
        sum += vals[i];
    return sum / k;
}

cv::Vec3d computeGains(const cv::Mat& bgr, int algorithm, const Params& params)
{
    if (bgr.empty() || bgr.channels() != 3)
        return cv::Vec3d(1.0, 1.0, 1.0);

    cv::Mat small = downscale(bgr);
    std::vector<cv::Mat> ch(3);
    cv::split(small, ch); // ch[0]=B, ch[1]=G, ch[2]=R

    cv::Vec3d gains(1.0, 1.0, 1.0);

    switch (algorithm) {
    case GreyWorld: {
        cv::Scalar m = cv::mean(small); // (B, G, R)
        double ref = (m[0] + m[1] + m[2]) / 3.0;
        for (int c = 0; c < 3; ++c)
            gains[c] = (m[c] > 1e-6) ? ref / m[c] : 1.0;
        break;
    }
    case WhitePatch: {
        double mx[3] = {0, 0, 0};
        for (int c = 0; c < 3; ++c)
            mx[c] = static_cast<double>(cv::norm(ch[c], cv::NORM_INF));
        double ref = std::max({mx[0], mx[1], mx[2]});
        for (int c = 0; c < 3; ++c)
            gains[c] = (mx[c] > 1e-6) ? ref / mx[c] : 1.0;
        break;
    }
    case PerfectReflector: {
        double mean[3];
        for (int c = 0; c < 3; ++c)
            mean[c] = topPercentMean(ch[c], params.percentile);
        double ref = (mean[0] + mean[1] + mean[2]) / 3.0;
        for (int c = 0; c < 3; ++c)
            gains[c] = (mean[c] > 1e-6) ? ref / mean[c] : 1.0;
        break;
    }
    case ShadesOfGrey: {
        double p = std::max(1.0, params.pnorm);
        double nrm[3];
        for (int c = 0; c < 3; ++c)
            nrm[c] = minkowskiNorm(ch[c], p);
        double ref = (nrm[0] + nrm[1] + nrm[2]) / 3.0;
        for (int c = 0; c < 3; ++c)
            gains[c] = (nrm[c] > 1e-6) ? ref / nrm[c] : 1.0;
        break;
    }
    case GreyEdge: {
        double p = std::max(1.0, params.pnorm);
        int order = std::min(2, std::max(0, params.derivOrder));
        double nrm[3] = {0, 0, 0};
        for (int c = 0; c < 3; ++c) {
            cv::Mat dx, dy;
            if (order == 0) {
                ch[c].convertTo(dx, CV_64F);
                dy = cv::Mat::zeros(dx.size(), CV_64F);
            } else {
                cv::Sobel(ch[c], dx, CV_64F, order, 0); // ∂^order/∂x^order
                cv::Sobel(ch[c], dy, CV_64F, 0, order); // ∂^order/∂y^order
            }
            int n = dx.rows * dx.cols;
            double acc = 0.0;
            const double* px = dx.ptr<double>(0);
            const double* py = dy.ptr<double>(0);
            for (int i = 0; i < n; ++i) {
                // модуль градиента в метрике Минковского
                double e = std::pow(std::pow(std::abs(px[i]), p)
                                  + std::pow(std::abs(py[i]), p), 1.0 / p);
                acc += std::pow(e, p);
            }
            nrm[c] = std::pow(acc / n, 1.0 / p);
        }
        double ref = (nrm[0] + nrm[1] + nrm[2]) / 3.0;
        for (int c = 0; c < 3; ++c)
            gains[c] = (nrm[c] > 1e-6) ? ref / nrm[c] : 1.0;
        break;
    }
    default:
        break;
    }

    // Ограничиваем экстремальные множители, чтобы не получить провалы/пересветы
    for (int c = 0; c < 3; ++c)
        gains[c] = std::min(4.0, std::max(0.25, gains[c]));

    // Сила коррекции: плавный переход от исходного кадра к полной коррекции
    const double s = std::min(1.0, std::max(0.0, params.strength));
    for (int c = 0; c < 3; ++c)
        gains[c] = 1.0 + s * (gains[c] - 1.0);

    return gains;
}

cv::Mat buildLut(const cv::Vec3d& gains)
{
    cv::Mat lut(1, 256, CV_8UC3);
    uchar* p = lut.ptr<uchar>(0);
    for (int i = 0; i < 256; ++i) {
        p[3 * i + 0] = cv::saturate_cast<uchar>(i * gains[0]); // B
        p[3 * i + 1] = cv::saturate_cast<uchar>(i * gains[1]); // G
        p[3 * i + 2] = cv::saturate_cast<uchar>(i * gains[2]); // R
    }
    return lut;
}

} // namespace wb
