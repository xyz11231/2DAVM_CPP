#pragma once
#include "PlatformGL.h"
#include "SensorData.h"
#include <chrono>

/**
 * TransparentChassis — GPU 加速透明底盘
 *
 * 原理：
 *   1. 维护一张"地面记忆"纹理（history），记录车辆掠过区域的地面数据
 *   2. 每帧更新：
 *      - 非车体区域：从当前 AVM 渲染结果复制新鲜地面数据
 *      - 车体区域：  从上一帧 history 中取出数据，根据车辆运动做偏移补偿
 *   3. 合成输出：在车体区域内用 history 地面数据替换车辆图像
 *
 * 接口流程：
 *   init() → update(avmTexture, sensorData) → getOutputTexture() → destroy()
 *
 * 使用 ping-pong FBO 避免同一纹理的读写冲突。
 */
class TransparentChassis {
public:
    TransparentChassis() = default;
    ~TransparentChassis();

    TransparentChassis(const TransparentChassis&) = delete;
    TransparentChassis& operator=(const TransparentChassis&) = delete;

    /**
     * 初始化 GPU 资源
     * @param width, height  鸟瞰图尺寸 (像素)
     * @param xl, xr, yt, yb 车体矩形边界 (像素坐标)
     */
    bool init(int width, int height, int xl, int xr, int yt, int yb);

    /**
     * 更新地面记忆并生成输出
     * @param avmTexture  AVM 渲染结果纹理 (FBO texture)
     * @param sensor      传感器数据 (速度、档位用于运动补偿)
     */
    void update(GLuint avmTexture, const SensorData& sensor);

    /** 获取输出纹理 (含透明底盘效果) */
    GLuint getOutputTexture() const { return outputTex_; }

    /** 开关控制 */
    void setEnabled(bool on) { enabled_ = on; }
    bool isEnabled()   const { return enabled_; }
    void toggle()            { enabled_ = !enabled_; }

    /** 是否已初始化 */
    bool isInitialized() const { return initialized_; }

    /**
     * 配置参数
     * @param mpp  鸟瞰图中每像素对应的米数 (默认 0.01 m/px)
     */
    /**
     * 配置运动补偿参考的中心 (Y 轴 UV 值, 0~1)
     * 默认0，即以底部后轴为旋转中心
     */
    void setPivotY(float py) { pivotY_ = py; }

    /** 释放所有 GPU 资源 */
    void destroy();

private:
    // ── Ping-pong 地面记忆 ──
    GLuint historyFBO_[2] = {};
    GLuint historyTex_[2] = {};
    int    pingPong_       = 0;

    // ── 合成输出 ──
    GLuint outputFBO_ = 0;
    GLuint outputTex_ = 0;

    // ── Shader 程序 ──
    GLuint updateProgram_    = 0;   // 更新地面记忆
    GLuint compositeProgram_ = 0;   // 合成最终输出

    // ── 全屏四边形 ──
    GLuint quadVAO_ = 0, quadVBO_ = 0, quadEBO_ = 0;

    // ── 尺寸 ──
    int width_  = 0;
    int height_ = 0;

    // ── 车体边界 (GL UV 空间: y=0 底, y=1 顶) ──
    float carBounds_[4] = {};  // (left, bottom, right, top)

    // ── 配置 ──
    float metersPerPixel_ = 0.01f;   // 每像素对应的米数
    float blendWidth_     = 0.02f;   // 车体边缘羽化宽度 (UV 空间)
    float pivotY_         = 0.0f;    // 旋转平移中心 Y(UV)
    bool  enabled_        = false;
    bool  initialized_    = false;

    // ── 计时 ──
    std::chrono::steady_clock::time_point lastTime_;
    bool firstFrame_ = true;

    // ── 内部方法 ──
    void initQuad();
    void drawQuad();
    bool compileShaders();
    GLuint createFBOWithTexture(GLuint& tex);
};
