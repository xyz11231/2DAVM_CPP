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
 *
 * 双平台支持：
 *   - 桌面 Linux: GLFW + GLEW + OpenCV VideoCapture
 *   - QNX 8255:   QNX Screen + EGL + VDB StreamHandler
 */

#include "PlatformGL.h"
#include "PlatformWindow.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <chrono>
#include <memory>

#include "AVM2D.h"
#include "SensorData.h"
#include "TransparentChassis.h"
#include "SharedTextureManager.h"
#include "ScreenCompositor.h"
#include "LuminanceBalancer.h"

#ifndef QNX_PLATFORM
#include "VideoReader.h"
#else
#include "QnxStreamAdapter.h"
#include "QnxVehicleAdapter.h"
#endif

// ─── 配置 ───
struct Config {
    std::string bin_path  = "/home/ld/new_project/2DAVM_PY/stitch_data.bin";

#ifndef QNX_PLATFORM
    // 桌面模式: 视频文件路径
    std::string video_dir = "/home/ld/data/双目拼接/20260320_2/video";
    std::string front, back, left, right;
    bool loop = true;
#endif

    int win_w = 0, win_h = 0;

    Config() {
#ifndef QNX_PLATFORM
        front = video_dir + "/front.mp4";
        back  = video_dir + "/back.mp4";
        left  = video_dir + "/left.mp4";
        right = video_dir + "/right.mp4";
#else
        // QNX 模式: 使用 SDK 摄像头分辨率
        win_w = 2560;
        win_h = 1440;
#endif
    }
};

// ─── 全局状态 ───
static bool g_quit = false, g_paused = false;
static LuminanceBalancer* g_balancer = nullptr;

// ─── 键盘回调 ───
static void onKey(int key, int action) {
    if (action != 1) return;  // 1=press

#ifdef QNX_PLATFORM
    // QNX keyboard sym codes
    if (key == 0x1B) g_quit = true;  // ESC
#else
    // GLFW key codes
    if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) g_quit = true;
    if (key == GLFW_KEY_SPACE) g_paused = !g_paused;
    if (key == GLFW_KEY_L && g_balancer) {
        g_balancer->toggle();
        std::cout << "[光照平衡] " << (g_balancer->isEnabled() ? "已开启" : "已关闭") << "\n";
    }
#endif
}

// ═══════════════ main ═══════════════
int main(int /*argc*/, char** /*argv*/) {
    Config cfg;

    // ── 1. 创建 AVM2D 实例 ──
    auto avm2d = std::make_unique<AVM2D>(cfg.bin_path);
    // 未来: auto avm3d = std::make_unique<AVM3D>(bin3d_path);

#ifndef QNX_PLATFORM
    // ── 桌面: 打开视频 ──
    std::string vidPaths[] = {cfg.front, cfg.back, cfg.left, cfg.right};
    VideoReader readers[4];
    for (int i = 0; i < 4; ++i)
        if (!readers[i].open(vidPaths[i], cfg.loop)) {
            std::cerr << "[Main] 无法打开: " << vidPaths[i] << "\n"; return 1; }
#else
    // ── QNX: 初始化摄像头流 ──
    QnxStreamAdapter streamAdapter;
    if (!streamAdapter.init()) {
        printf("[Main] QNX camera stream init failed\n");
        return 1;
    }
    QnxVehicleAdapter vehicleAdapter;
#endif

    // ── 2. 创建窗口和 GL 上下文 ──
    PlatformWindow window;
    // 窗口初始尺寸 (桌面模式后续会调整为鸟瞰图大小)
    int initW = cfg.win_w > 0 ? cfg.win_w : 800;
    int initH = cfg.win_h > 0 ? cfg.win_h : 600;
    if (!window.init(initW, initH, "环视AVM GPU")) {
        std::cerr << "[Main] 窗口初始化失败\n";
        return 1;
    }
    window.setKeyCallback(onKey);

    // ── 3. 初始化各模块（需要 GL 上下文就绪） ──
    if (!avm2d->init()) {
        std::cerr << "[Main] AVM2D 初始化失败\n";
        window.destroy();
        return 1;
    }

    SharedTextureManager texMgr;
    texMgr.init();

    ScreenCompositor compositor;
    if (!compositor.init()) {
        std::cerr << "[Main] Compositor 初始化失败\n";
        avm2d->destroy();
        window.destroy();
        return 1;
    }

    // 调整窗口至鸟瞰图尺寸
    int wW = cfg.win_w > 0 ? cfg.win_w : avm2d->getWidth();
    int wH = cfg.win_h > 0 ? cfg.win_h : avm2d->getHeight();
    window.setSize(wW, wH);

    // ── 4. 传感器数据和附加模块 ──
    SensorData sensorData;
    TransparentChassis transparentChassis;

    // 光照平衡
    LuminanceBalancer balancer;
    g_balancer = &balancer;

    using Clock = std::chrono::steady_clock;
    auto lastFps = Clock::now();
    int frameCount = 0;

    std::cout << "[Main] GPU 渲染就绪. ESC/Q退出 Space暂停 L光照平衡\n";

    // ═══ 主循环 ═══
    while (!window.shouldClose() && !g_quit) {
        window.pollEvents();

        if (g_paused) {
            window.swapBuffers();
            continue;
        }

#ifndef QNX_PLATFORM
        // ── 桌面: 读取 4 路视频帧 ──
        cv::Mat frames[4];
        bool allOk = true;
        for (int i = 0; i < 4; ++i)
            if (!readers[i].read(frames[i])) { allOk = false; break; }
        if (!allOk) { if (!cfg.loop) break; continue; }

        // 光照平衡计算
        balancer.update(frames);

        // 上传源帧纹理
        SourceTextures src = texMgr.upload(frames);
        std::copy(balancer.gains(), balancer.gains() + 4, src.gains);

#else
        // ── QNX: 获取摄像头帧 ──
        CameraFrame camFrames[4];
        if (!streamAdapter.getFrames(camFrames)) {
            continue;  // 等待帧就绪
        }

        // QNX 模式: YUV 上传 (光照平衡在 YUV 模式下可选跳过)
        SourceTextures src = texMgr.uploadYUV(camFrames);

        // 获取车辆信号
        vehicleAdapter.update(sensorData);
#endif

        // ── 渲染 ──
        avm2d->render(src, sensorData);
        // 未来: avm3d->render(src, sensorData);

        // 合成到屏幕
        compositor.begin();
        compositor.draw(avm2d->getOutputTexture(), {0, 0, wW, wH});
        // 未来分屏:
        // compositor.draw(avm2d->getOutputTexture(), {0, 0, wW/2, wH});
        // compositor.draw(avm3d->getOutputTexture(), {wW/2, 0, wW/2, wH});

        window.swapBuffers();

        // FPS
        ++frameCount;
        auto now = Clock::now();
        double dt = std::chrono::duration<double>(now - lastFps).count();
        if (dt >= 0.1) {
            std::string title = "环视AVM GPU  FPS: " + std::to_string((int)(frameCount / dt));
            window.setTitle(title);
            frameCount = 0;
            lastFps = now;
        }
    }

    // 清理
    avm2d->destroy();
    avm2d.reset();
    texMgr.destroy();
    compositor.destroy();
    window.destroy();

#ifdef QNX_PLATFORM
    streamAdapter.destroy();
#endif

    std::cout << "[Main] 退出\n";
    return 0;
}
