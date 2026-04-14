#include "SharedTextureManager.h"
#include <opencv2/imgproc.hpp>
#include <iostream>

SharedTextureManager::~SharedTextureManager() {
    if (initialized_) destroy();
}

void SharedTextureManager::init() {
    for (int i = 0; i < 4; ++i) {
        glGenTextures(1, &textures_[i]);
        glBindTexture(GL_TEXTURE_2D, textures_[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // 1×1 占位
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    sized_ = false;
    initialized_ = true;
    std::cout << "[SharedTexMgr] 初始化完成\n";
}

SourceTextures SharedTextureManager::upload(cv::Mat frames[4]) {
    for (int i = 0; i < 4; ++i) {
        cv::Mat rgb;
        cv::cvtColor(frames[i], rgb, cv::COLOR_BGR2RGB);
        glBindTexture(GL_TEXTURE_2D, textures_[i]);
        if (!sized_)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, rgb.cols, rgb.rows, 0,
                         GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
        else
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rgb.cols, rgb.rows,
                            GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
    }

    if (!sized_) {
        width_  = frames[0].cols;
        height_ = frames[0].rows;
        sized_  = true;
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    SourceTextures src;
    for (int i = 0; i < 4; ++i) src.textures[i] = textures_[i];
    src.width  = width_;
    src.height = height_;
    return src;
}

#ifdef QNX_PLATFORM
SourceTextures SharedTextureManager::uploadYUV(const CameraFrame frames[4]) {
    // UYVY 编码: 每 4 字节 = [U, Y0, V, Y1]
    // 上传为 half-width GL_RGBA 纹理:
    //   纹理宽度 = 原始宽度 / 2
    //   每个 RGBA 像素 = (U, Y0, V, Y1)

    for (int i = 0; i < 4; ++i) {
        if (!frames[i].valid || !frames[i].data) continue;

        int texW = frames[i].width / 2;  // UYVY: 2像素 = 4字节 = 1个RGBA
        int texH = frames[i].height;

        glBindTexture(GL_TEXTURE_2D, textures_[i]);
        if (!sized_)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, frames[i].data);
        else
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texW, texH,
                            GL_RGBA, GL_UNSIGNED_BYTE, frames[i].data);
    }

    if (!sized_) {
        width_  = frames[0].width;
        height_ = frames[0].height;
        sized_  = true;
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    SourceTextures src;
    for (int i = 0; i < 4; ++i) src.textures[i] = textures_[i];
    // 返回原始分辨率 (shader 中需要用于 remap 坐标计算)
    src.width  = width_;
    src.height = height_;
    return src;
}
#endif

void SharedTextureManager::destroy() {
    if (!initialized_) return;
    for (auto& t : textures_) {
        if (t) { glDeleteTextures(1, &t); t = 0; }
    }
    initialized_ = false;
    std::cout << "[SharedTexMgr] 资源已释放\n";
}
