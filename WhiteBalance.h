#pragma once

#include <opencv2/core.hpp>
#include <QString>

namespace wb {

// Популярные алгоритмы автоматического баланса белого
enum Algorithm {
    GreyWorld = 0,      // «Серый мир» (Grey World)
    WhitePatch,         // «Белое пятно» (White Patch / Max RGB)
    PerfectReflector,   // «Идеальный отражатель» (Perfect Reflector)
    ShadesOfGrey,       // «Оттенки серого» (Shades of Grey, норма Минковского)
    GreyEdge,           // «Серая граница» (Grey Edge, производные + норма Минковского)
    AlgorithmCount
};

// Настраиваемые параметры алгоритма
struct Params {
    double strength    = 1.0;  // сила коррекции 0..1 (смешение с исходным кадром)
    double percentile  = 1.0;  // верхние N% ярких пикселей (Perfect Reflector)
    double pnorm       = 6.0;  // показатель Минковского p (Shades of Grey, Grey Edge)
    int    derivOrder  = 1;    // порядок производной 0/1/2 (Grey Edge)
};

QString algorithmName(int algorithm);
QString algorithmDescription(int algorithm);

// Вычисляет множители каналов (B, G, R) по эталонному кадру.
// Кадр внутри уменьшается для скорости — на результат это почти не влияет.
cv::Vec3d computeGains(const cv::Mat& bgr, int algorithm, const Params& params);

// Строит 256-точечную таблицу поиска (LUT) из множителей каналов.
cv::Mat buildLut(const cv::Vec3d& gains);

} // namespace wb
