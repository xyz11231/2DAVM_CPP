#pragma once
#include <opencv2/core.hpp>
#include "SensorData.h"

/**
 * TransparentChassis — 透明底盘
 *
 * 接收 AVM 渲染输出图像和传感器数据，生成透明底盘效果。
 * 当前为桩实现，后续补充具体算法。
 */
class TransparentChassis {
public:
    TransparentChassis() = default;
    ~TransparentChassis() = default;

    /**
     * 处理透明底盘效果
     * @param avmImage    AVM 渲染输出 (BGR, CV_8UC3)
     * @param sensorData  传感器数据
     * @return 处理后的图像
     */
    cv::Mat process(const cv::Mat& avmImage, const SensorData& sensorData);
};
