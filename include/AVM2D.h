#pragma once
#include "AVM.h"
#include "StitchBinLoader.h"
#include "PlatformGL.h"
#include <array>

/**
 * AVM2D — 2D 环视鸟瞰图实现
 *
 * 使用 OpenGL 在 GPU 上完成：
 *   remap（畸变校正 + 投影）→ 区域融合 → 车辆图叠加
 *
 * 渲染流程：
 *   1. 接收外部统一上传的 4 路源帧纹理（SourceTextures）
 *   2. 在内部 FBO 上完成全部渲染
 *   3. 通过 getOutputTexture() 暴露结果纹理
 *
 * 纹理布局 (9 个静态 + 4 个外部)：
 *   外部:  srcFront/Back/Left/Right      (由 SharedTextureManager 管理)
 *   4-7:   remapFront/Back/Left/Right     (RG32F, 一次上传)
 *   8-11:  weightFL/FR/BL/BR             (R32F,  一次上传)
 *   12:    carTex                         (一次上传)
 */
class AVM2D : public AVM {
public:
    explicit AVM2D(const std::string& binPath);
    ~AVM2D() override;

    bool init() override;
    bool render(const SourceTextures& src,
                const SensorData& sensorData) override;
    GLuint getOutputTexture() const override { return fboTex_; }
    bool readPixels(cv::Mat& output) override;
    void destroy() override;

    int getWidth()  const override { return binData_.total_w; }
    int getHeight() const override { return binData_.total_h; }

private:
    // bin 数据
    StitchBinData binData_;

    // OpenGL 资源
    GLuint program_ = 0;
    std::array<GLuint, 4> remapTex_{};
    std::array<GLuint, 4> weightTex_{};
    GLuint carTex_ = 0;

    // 全屏四边形
    GLuint quadVAO_ = 0, quadVBO_ = 0, quadEBO_ = 0;

    // FBO（离屏渲染）
    GLuint fbo_ = 0, fboTex_ = 0;

    bool initialized_ = false;
    bool srcUniformSet_ = false;
    GLint exposureLoc_ = -1;

    // 内部方法
    void initQuad();
    void initFBO();
    void uploadStaticTextures();
    void bindTextures(const GLuint srcTextures[4]);
};
