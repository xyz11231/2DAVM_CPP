#pragma once
#include <GL/glew.h>

/**
 * Viewport — 屏幕区域定义
 */
struct Viewport {
    int x, y, w, h;
};

/**
 * ScreenCompositor — 分屏合成器
 *
 * 将多个 AVM 的 FBO 纹理合成到屏幕的不同区域。
 * 使用简单的纹理贴图着色器 + glViewport 控制绘制区域。
 */
class ScreenCompositor {
public:
    ScreenCompositor() = default;
    ~ScreenCompositor();

    // 禁止拷贝
    ScreenCompositor(const ScreenCompositor&) = delete;
    ScreenCompositor& operator=(const ScreenCompositor&) = delete;

    /**
     * 初始化着色器和全屏四边形（GL 上下文就绪后调用）
     */
    bool init();

    /**
     * 开始合成：清除屏幕
     */
    void begin();

    /**
     * 将一个纹理绘制到屏幕的指定区域
     * @param texture   FBO 纹理 ID
     * @param viewport  目标区域 (像素坐标)
     */
    void draw(GLuint texture, const Viewport& viewport);

    /**
     * 释放资源
     */
    void destroy();

private:
    GLuint program_ = 0;
    GLuint quadVAO_ = 0, quadVBO_ = 0, quadEBO_ = 0;
    bool initialized_ = false;
};
