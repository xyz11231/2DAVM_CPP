#pragma once
#include "PlatformGL.h"
#include <opencv2/core.hpp>
#include <array>
#include "AVM.h"

#ifdef QNX_PLATFORM
#include "QnxStreamAdapter.h"
#endif

/**
 * SharedTextureManager — 4 路源帧纹理统一管理
 *
 * 将 4 路摄像头帧上传到 GPU 纹理，供所有 AVM 实例共享引用，
 * 避免每个 AVM 实例各自上传一份相同数据。
 */
class SharedTextureManager {
public:
    SharedTextureManager() = default;
    ~SharedTextureManager();

    // 禁止拷贝
    SharedTextureManager(const SharedTextureManager&) = delete;
    SharedTextureManager& operator=(const SharedTextureManager&) = delete;

    /**
     * 初始化（在 GL 上下文就绪后调用）
     * 创建 4 个 1×1 占位纹理
     */
    void init();

    /**
     * 上传 4 路摄像头帧到 GPU (桌面模式: BGR cv::Mat)
     * @param frames  4路输入帧 (BGR, CV_8UC3)，顺序: front/back/left/right
     * @return 包含纹理 ID 和尺寸的 SourceTextures
     */
    SourceTextures upload(cv::Mat frames[4]);

#ifdef QNX_PLATFORM
    /**
     * 上传 4 路摄像头帧到 GPU (QNX 模式: YUV422 UYVY)
     *
     * UYVY 数据以 half-width RGBA 纹理上传:
     *   实际纹理宽度 = 原始宽度/2, 格式 = GL_RGBA
     *   每个 RGBA 像素 = (U, Y0, V, Y1)
     *
     * @param frames  4路 CameraFrame (YUV422 UYVY)
     * @return 包含纹理 ID 和原始尺寸的 SourceTextures
     */
    SourceTextures uploadYUV(const CameraFrame frames[4]);
#endif

    /**
     * 释放所有纹理资源
     */
    void destroy();

private:
    std::array<GLuint, 4> textures_{};
    bool initialized_ = false;
    bool sized_ = false;     // 纹理是否已按实际尺寸分配
    int width_ = 0, height_ = 0;
};
