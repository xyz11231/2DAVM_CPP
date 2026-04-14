#include "PlatformWindow.h"
#include <iostream>
#include <cstring>

PlatformWindow::~PlatformWindow() {
    if (initialized_) destroy();
}

// ═══════════════════════════════════════════════════
//  桌面 GLFW 实现
// ═══════════════════════════════════════════════════
#ifndef QNX_PLATFORM

void PlatformWindow::glfwKeyCallback(GLFWwindow* win, int key, int /*scancode*/,
                                     int action, int /*mods*/) {
    auto* self = static_cast<PlatformWindow*>(glfwGetWindowUserPointer(win));
    if (self && self->keyCb_) {
        self->keyCb_(key, action);
    }
}

bool PlatformWindow::init(int width, int height, const std::string& title) {
    if (!glfwInit()) {
        std::cerr << "[PlatformWindow] glfwInit 失败\n";
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window_) {
        std::cerr << "[PlatformWindow] 创建窗口失败\n";
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window_);
    glfwSetWindowUserPointer(window_, this);
    glfwSetKeyCallback(window_, glfwKeyCallback);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "[PlatformWindow] glewInit 失败\n";
        return false;
    }

    initialized_ = true;
    std::cout << "[PlatformWindow] GLFW+GLEW 初始化完成 (" << width << "×" << height << ")\n";
    return true;
}

void PlatformWindow::swapBuffers() {
    if (window_) glfwSwapBuffers(window_);
}

bool PlatformWindow::shouldClose() const {
    return closeRequested_ || (window_ && glfwWindowShouldClose(window_));
}

void PlatformWindow::pollEvents() {
    glfwPollEvents();
}

void PlatformWindow::requestClose() {
    closeRequested_ = true;
}

void PlatformWindow::setSize(int width, int height) {
    if (window_) glfwSetWindowSize(window_, width, height);
}

void PlatformWindow::setTitle(const std::string& title) {
    if (window_) glfwSetWindowTitle(window_, title.c_str());
}

void PlatformWindow::destroy() {
    if (!initialized_) return;
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
    initialized_ = false;
    std::cout << "[PlatformWindow] GLFW 资源已释放\n";
}

void PlatformWindow::setKeyCallback(KeyCallback cb) {
    keyCb_ = std::move(cb);
}

// ═══════════════════════════════════════════════════
//  QNX Screen + EGL 实现
// ═══════════════════════════════════════════════════
#else

bool PlatformWindow::initScreen(int width, int height, const std::string& title) {
    if (screen_create_context(&screenCtx_, SCREEN_APPLICATION_CONTEXT)) {
        printf("[PlatformWindow] Failed to create screen context\n");
        return false;
    }

    if (screen_create_event(&screenEvt_) != 0) {
        printf("[PlatformWindow] Failed to create screen event: %s\n", strerror(errno));
        return false;
    }

    // 查找目标显示器
    screen_display_t target_display = nullptr;
    int display_count = 0;
    screen_get_context_property_iv(screenCtx_, SCREEN_PROPERTY_DISPLAY_COUNT, &display_count);

    screen_display_t* displays = new screen_display_t[display_count];
    screen_get_context_property_pv(screenCtx_, SCREEN_PROPERTY_DISPLAYS, (void**)displays);

    for (int i = 0; i < display_count; ++i) {
        int id;
        screen_get_display_property_iv(displays[i], SCREEN_PROPERTY_ID, &id);
        if (id == TARGET_DISPLAY_ID) {
            target_display = displays[i];
            break;
        }
    }
    delete[] displays;

    if (!target_display) {
        printf("[PlatformWindow] Target display (ID=%d) not found\n", TARGET_DISPLAY_ID);
        return false;
    }

    if (screen_create_window(&screenWin_, screenCtx_)) {
        printf("[PlatformWindow] Failed to create window\n");
        return false;
    }

    const int usage = SCREEN_USAGE_OPENGL_ES2 | SCREEN_USAGE_NATIVE | SCREEN_USAGE_WRITE;
    const int format = SCREEN_FORMAT_RGBA8888;
    const int size[2] = {width, height};
    const int pos[2] = {0, 0};
    const int zorder = 3100;
    const int pipeline = 3;

    screen_set_window_property_iv(screenWin_, SCREEN_PROPERTY_USAGE, &usage);
    screen_set_window_property_pv(screenWin_, SCREEN_PROPERTY_DISPLAY, (void**)&target_display);
    screen_set_window_property_iv(screenWin_, SCREEN_PROPERTY_BUFFER_SIZE, size);
    screen_set_window_property_iv(screenWin_, SCREEN_PROPERTY_SIZE, size);
    screen_set_window_property_iv(screenWin_, SCREEN_PROPERTY_POSITION, pos);
    screen_set_window_property_iv(screenWin_, SCREEN_PROPERTY_FORMAT, &format);
    screen_set_window_property_iv(screenWin_, SCREEN_PROPERTY_ZORDER, &zorder);
    screen_set_window_property_iv(screenWin_, SCREEN_PROPERTY_PIPELINE, &pipeline);
    screen_set_window_property_cv(screenWin_, SCREEN_PROPERTY_ID_STRING,
                                  title.length(), title.c_str());

    if (screen_create_window_buffers(screenWin_, 2)) {
        printf("[PlatformWindow] Failed to create window buffers\n");
        return false;
    }

    printf("[PlatformWindow] Screen initialization complete (%dx%d)\n", width, height);
    return true;
}

bool PlatformWindow::initEGL() {
    eglDisplay_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay_ == EGL_NO_DISPLAY) {
        printf("[PlatformWindow] Failed to get EGL display\n");
        return false;
    }

    if (!eglInitialize(eglDisplay_, nullptr, nullptr)) {
        printf("[PlatformWindow] Failed to initialize EGL\n");
        return false;
    }

    const EGLint config_attribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(eglDisplay_, config_attribs, &config, 1, &num_configs)) {
        printf("[PlatformWindow] Failed to choose EGL config\n");
        return false;
    }

    eglSurface_ = eglCreateWindowSurface(eglDisplay_, config, screenWin_, nullptr);
    if (eglSurface_ == EGL_NO_SURFACE) {
        printf("[PlatformWindow] Failed to create EGL surface\n");
        return false;
    }

    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    eglContext_ = eglCreateContext(eglDisplay_, config, EGL_NO_CONTEXT, context_attribs);
    if (eglContext_ == EGL_NO_CONTEXT) {
        printf("[PlatformWindow] Failed to create EGL context\n");
        return false;
    }

    if (!eglMakeCurrent(eglDisplay_, eglSurface_, eglSurface_, eglContext_)) {
        printf("[PlatformWindow] Failed to make EGL context current\n");
        return false;
    }

    printf("[PlatformWindow] EGL initialization complete\n");
    return true;
}

void PlatformWindow::processScreenEvent() {
    while (screen_get_event(screenCtx_, screenEvt_, 0) == 0) {
        int type;
        screen_get_event_property_iv(screenEvt_, SCREEN_PROPERTY_TYPE, &type);
        if (type == SCREEN_EVENT_NONE) break;

        // 处理触摸/键盘事件
        if (type == SCREEN_EVENT_KEYBOARD && keyCb_) {
            int flags;
            screen_get_event_property_iv(screenEvt_, SCREEN_PROPERTY_FLAGS, &flags);
            int sym;
            screen_get_event_property_iv(screenEvt_, SCREEN_PROPERTY_KEY_SYM, &sym);
            keyCb_(sym, (flags & 1) ? 1 : 0);  // 1=press, 0=release
        }
    }
}

bool PlatformWindow::init(int width, int height, const std::string& title) {
    if (!initScreen(width, height, title)) return false;
    if (!initEGL()) return false;
    initialized_ = true;
    return true;
}

void PlatformWindow::swapBuffers() {
    eglSwapBuffers(eglDisplay_, eglSurface_);
}

bool PlatformWindow::shouldClose() const {
    return closeRequested_;
}

void PlatformWindow::pollEvents() {
    // 非阻塞事件轮询
    processScreenEvent();
}

void PlatformWindow::requestClose() {
    closeRequested_ = true;
}

void PlatformWindow::setSize(int /*width*/, int /*height*/) {
    // QNX Screen 窗口大小在 init 时固定
}

void PlatformWindow::setTitle(const std::string& /*title*/) {
    // QNX Screen 标题在 init 时设置
}

void PlatformWindow::destroy() {
    if (!initialized_) return;

    if (eglDisplay_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(eglDisplay_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (eglSurface_ != EGL_NO_SURFACE) eglDestroySurface(eglDisplay_, eglSurface_);
        if (eglContext_ != EGL_NO_CONTEXT) eglDestroyContext(eglDisplay_, eglContext_);
        eglTerminate(eglDisplay_);
    }

    if (screenEvt_) screen_destroy_event(screenEvt_);
    if (screenWin_) screen_destroy_window(screenWin_);
    if (screenCtx_) screen_destroy_context(screenCtx_);

    initialized_ = false;
    printf("[PlatformWindow] QNX Screen+EGL resources released\n");
}

void PlatformWindow::setKeyCallback(KeyCallback cb) {
    keyCb_ = std::move(cb);
}

#endif // QNX_PLATFORM
