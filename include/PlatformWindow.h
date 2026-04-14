#pragma once

/**
 * PlatformWindow — 跨平台窗口/GL上下文抽象
 *
 * QNX 8255: QNX Screen + EGL (参考 SDK RenderBase)
 * 桌面 Linux: GLFW + GLEW
 */

#include "PlatformGL.h"
#include <string>
#include <functional>

#ifdef QNX_PLATFORM
    #include <screen/screen.h>
#else
    #include <GLFW/glfw3.h>
#endif

class PlatformWindow {
public:
    PlatformWindow() = default;
    ~PlatformWindow();

    // 禁止拷贝
    PlatformWindow(const PlatformWindow&) = delete;
    PlatformWindow& operator=(const PlatformWindow&) = delete;

    /**
     * 初始化窗口和 GL 上下文
     * @param width   窗口宽度
     * @param height  窗口高度
     * @param title   窗口标题
     * @return true 成功
     */
    bool init(int width, int height, const std::string& title);

    /** 交换前后缓冲 */
    void swapBuffers();

    /** 检查是否应该关闭 */
    bool shouldClose() const;

    /** 处理事件 */
    void pollEvents();

    /** 设置退出标记 */
    void requestClose();

    /** 设置窗口大小 */
    void setSize(int width, int height);

    /** 设置窗口标题 (仅桌面) */
    void setTitle(const std::string& title);

    /** 释放所有资源 */
    void destroy();

    /** 设置键盘回调 (key, scancode, action, mods) */
    using KeyCallback = std::function<void(int key, int action)>;
    void setKeyCallback(KeyCallback cb);

#ifdef QNX_PLATFORM
    screen_context_t getScreenContext() const { return screenCtx_; }
    screen_window_t  getScreenWindow()  const { return screenWin_; }
    screen_event_t   getScreenEvent()   const { return screenEvt_; }
#else
    GLFWwindow* getGLFWWindow() const { return window_; }
#endif

private:
    bool initialized_ = false;
    bool closeRequested_ = false;
    KeyCallback keyCb_;

#ifdef QNX_PLATFORM
    // ── QNX Screen + EGL ──
    screen_context_t screenCtx_ = nullptr;
    screen_window_t  screenWin_ = nullptr;
    screen_event_t   screenEvt_ = nullptr;
    EGLDisplay eglDisplay_ = EGL_NO_DISPLAY;
    EGLSurface eglSurface_ = EGL_NO_SURFACE;
    EGLContext eglContext_  = EGL_NO_CONTEXT;

    bool initScreen(int width, int height, const std::string& title);
    bool initEGL();
    void processScreenEvent();

    static constexpr int TARGET_DISPLAY_ID = 2;
#else
    // ── GLFW ──
    GLFWwindow* window_ = nullptr;

    static void glfwKeyCallback(GLFWwindow* win, int key, int scancode,
                                int action, int mods);
#endif
};
