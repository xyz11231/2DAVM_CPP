#include "TransparentChassis.h"
#include <iostream>

/**
 * 透明底盘处理 — 桩实现
 * 当前直接返回输入图像，后续补充具体算法。
 */
cv::Mat TransparentChassis::process(const cv::Mat& avmImage, const SensorData& /*sensorData*/) {
    // TODO: 实现透明底盘效果
    // 1. 根据传感器数据（车速、方向盘转角等）计算历史轨迹
    // 2. 将底盘区域替换为历史帧数据
    return avmImage.clone();
}
