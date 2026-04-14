#include "LuminanceBalancer.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

void LuminanceBalancer::update(const cv::Mat frames[4]) {
    if (!enabled_) {
        // 关闭时重置为 1.0
        gains_ = {1.f, 1.f, 1.f, 1.f};
        inited_ = false;
        return;
    }

    // 1. 计算4路灰度均值
    float means[4];
    for (int i = 0; i < 4; ++i) {
        cv::Scalar s = cv::mean(frames[i]);
        // BGR 通道均值加权转灰度: 0.114*B + 0.587*G + 0.299*R
        means[i] = static_cast<float>(s[0] * 0.114 + s[1] * 0.587 + s[2] * 0.299);
    }

    // 2. 全局目标均值（4路平均）
    float target = (means[0] + means[1] + means[2] + means[3]) * 0.25f;

    // 3. 计算每路校正因子，限制在 [0.5, 2.0]
    float raw[4];
    for (int i = 0; i < 4; ++i) {
        raw[i] = (means[i] > 1.f) ? (target / means[i]) : 1.f;
        raw[i] = std::clamp(raw[i], 0.5f, 2.0f);
    }

    // 4. EMA 时间平滑（首帧直接赋值）
    if (!inited_) {
        for (int i = 0; i < 4; ++i) gains_[i] = raw[i];
        inited_ = true;
    } else {
        for (int i = 0; i < 4; ++i)
            gains_[i] = alpha_ * raw[i] + (1.f - alpha_) * gains_[i];
    }
}
