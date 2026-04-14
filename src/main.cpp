/**
 * main.cpp — 环视AVM (GPU 版)
 *
 * 分屏架构：
 *   - SharedTextureManager: 统一管理 4 路源帧纹理（上传一次）
 *   - AVM2D / AVM3D:        各自渲染到内部 FBO
 *   - ScreenCompositor:     将多个 FBO 纹理合成到屏幕的指定区域
 *
 * 当前只有 AVM2D，显示在全屏。
 * 未来添加 AVM3D 后，左半屏 2D + 右半屏 3D。
 */

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <chrono>
#include <memory>

#include "AVM2D.h"
#include "SensorData.h"
#include "TransparentChassis.h"
#include "VideoReader.h"
#include "SharedTextureManager.h"
#include "ScreenCompositor.h"

// ─── 配置 ───
struct Config {
    std::string bin_path  = "/home/ld/new_project/2DAVM_PY/stitch_data.bin";
    std::string video_dir = "/home/ld/data/双目拼接/20260320_2/video";
    std::string front, back, left, right;
    int win_w = 0, win_h = 0;
    bool loop = true;
    Config() {
        front = video_dir + "/front.mp4";
        back  = video_dir + "/back.mp4";
        left  = video_dir + "/left.mp4";
        right = video_dir + "/right.mp4";
    }
};

// ─── GLFW 回调 ───
static bool g_quit = false, g_paused = false;
static void keyCb(GLFWwindow*, int key, int, int act, int) {
    if (act != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) g_quit = true;
    if (key == GLFW_KEY_SPACE) g_paused = !g_paused;
}

// ═══════════════ main ═══════════════
int main(int /*argc*/, char** /*argv*/) {
    Config cfg;

    // 1. 打开视频
    std::string vidPaths[] = {cfg.front, cfg.back, cfg.left, cfg.right};
    VideoReader readers[4];
    for (int i = 0; i < 4; ++i)
        if (!readers[i].open(vidPaths[i], cfg.loop)) {
            std::cerr << "[Main] 无法打开: " << vidPaths[i] << "\n"; return 1; }

    // 2. 创建 AVM2D 实例
    auto avm2d = std::make_unique<AVM2D>(cfg.bin_path);
    // 未来: auto avm3d = std::make_unique<AVM3D>(bin3d_path);

    // 3. GLFW / GLEW 初始化
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    auto* window = glfwCreateWindow(800, 600, "环视AVM GPU", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCb);
    glfwSwapInterval(1);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return 1;

    // 4. 初始化各模块（需要 GL 上下文就绪）
    if (!avm2d->init()) {
        std::cerr << "[Main] AVM2D 初始化失败\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    SharedTextureManager texMgr;
    texMgr.init();

    ScreenCompositor compositor;
    if (!compositor.init()) {
        std::cerr << "[Main] Compositor 初始化失败\n";
        avm2d->destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // 调整窗口尺寸
    // 当前只有 2D，使用鸟瞰图原始尺寸
    // 未来分屏: win_w = avm2d->getWidth() + avm3d->getWidth()
    int wW = cfg.win_w > 0 ? cfg.win_w : avm2d->getWidth();
    int wH = cfg.win_h > 0 ? cfg.win_h : avm2d->getHeight();
    glfwSetWindowSize(window, wW, wH);

    // 5. 创建传感器数据和透明底盘（桩）
    SensorData sensorData;
    TransparentChassis transparentChassis;

    using Clock = std::chrono::steady_clock;
    auto lastFps = Clock::now();
    int frameCount = 0;

    std::cout << "[Main] GPU 渲染就绪. ESC/Q退出 Space暂停\n";

    // ═══ 主循环 ═══
    while (!glfwWindowShouldClose(window) && !g_quit) {
        glfwPollEvents();

        if (g_paused) {
            glfwSwapBuffers(window);
            continue;
        }

        // 读取 4 路视频帧
        cv::Mat frames[4];
        bool allOk = true;
        for (int i = 0; i < 4; ++i)
            if (!readers[i].read(frames[i])) { allOk = false; break; }
        if (!allOk) { if (!cfg.loop) break; continue; }

        // ── 渲染流程 ──

        // 1) 统一上传源帧纹理（4路共享，上传一次）
        SourceTextures src = texMgr.upload(frames);

        // 2) AVM 渲染到各自的 FBO
        avm2d->render(src, sensorData);
        // 未来: avm3d->render(src, sensorData);

        // 3) 合成到屏幕
        compositor.begin();

        // 当前只有 2D，全屏显示
        compositor.draw(avm2d->getOutputTexture(), {0, 0, wW, wH});

        // 未来分屏:
        // compositor.draw(avm2d->getOutputTexture(), {0, 0, wW/2, wH});
        // compositor.draw(avm3d->getOutputTexture(), {wW/2, 0, wW/2, wH});

        glfwSwapBuffers(window);

        // FPS
        ++frameCount;
        auto now = Clock::now();
        double dt = std::chrono::duration<double>(now - lastFps).count();
        if (dt >= 0.1) {
            std::string title = "环视AVM GPU  FPS: " + std::to_string((int)(frameCount / dt));
            glfwSetWindowTitle(window, title.c_str());
            frameCount = 0;
            lastFps = now;
        }
    }

    // 清理
    avm2d->destroy();
    avm2d.reset();
    texMgr.destroy();
    compositor.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "[Main] 退出\n";
    return 0;
}
