#pragma once
#include <opencv2/core.hpp>
#include <array>

/**
 * LuminanceBalancer — 4路摄像头亮度均衡
 *
 * 每帧计算4路图像的灰度均值，以全局均值为目标，
 * 计算每路的亮度校正因子（gain），通过 EMA 平滑防止闪烁。
 *
 * 校正因子通过 SourceTextures::gains 传递给 Shader，
 * 在 GPU 端对采样结果做乘法校正，2D/3D 通用。
 */
class LuminanceBalancer {
public:
    /**
     * 每帧调用：计算并更新平滑后的校正因子
     * @param frames  4路输入帧 (BGR)，顺序: front/back/left/right
     */
    void update(const cv::Mat frames[4]);

    /** 获取当前平滑后的校正因子（4路） */
    const float* gains() const { return gains_.data(); }

    /** 开/关光照平衡 */
    void setEnabled(bool on) { enabled_ = on; }
    bool isEnabled() const { return enabled_; }
    void toggle() { enabled_ = !enabled_; }

    /** 设置 EMA 平滑系数 (0,1]，越大响应越快 */
    void setAlpha(float a) { alpha_ = a; }

private:
    std::array<float, 4> gains_ = {1.f, 1.f, 1.f, 1.f};
    float alpha_   = 0.1f;   // EMA 系数
    bool  enabled_ = true;   // 是否启用
    bool  inited_  = false;  // 首帧标记（跳过 EMA）
};
