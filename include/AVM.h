#pragma once
#include <string>
#include "PlatformGL.h"
#include "SensorData.h"
#include <opencv2/opencv.hpp>

/**
 * SourceTextures — 外部统一管理的 4 路源帧纹理
 *
 * 由 SharedTextureManager 上传后传入 AVM::render()，
 * 多个 AVM 实例共享同一组纹理，避免重复上传。
 */
struct SourceTextures {
    GLuint textures[4] = {};  // 纹理 ID，顺序: front/back/left/right
    int width  = 0;           // 源帧宽度（4 路相同）
    int height = 0;           // 源帧高度
    float gains[4] = {1,1,1,1}; // 亮度校正因子（光照平衡）
};

/**
 * AVM — 环视系统虚基类
 *
 * 定义所有 AVM 实现（2D / 3D 等）的统一接口。
 * 每个实例渲染到自己的 FBO，由外部合成器负责分屏显示。
 */
class AVM {
public:
    /**
     * 构造函数
     * @param binPath  预计算数据文件 (.bin) 路径
     */
    explicit AVM(const std::string& binPath) : binPath_(binPath) {}

    virtual ~AVM() = default;

    // 禁止拷贝（持有 GPU 资源）
    AVM(const AVM&) = delete;
    AVM& operator=(const AVM&) = delete;

    /**
     * 初始化 GPU 资源（必须在 OpenGL 上下文就绪后调用）
     * @return true 初始化成功
     */
    virtual bool init() = 0;

    /**
     * 渲染一帧到内部 FBO
     * @param src         4路源帧纹理（外部统一上传）
     * @param sensorData  传感器数据
     * @return true 渲染成功
     */
    virtual bool render(const SourceTextures& src,
                        const SensorData& sensorData) = 0;

    /**
     * 获取离屏渲染结果的纹理 ID（供合成器使用）
     */
    virtual GLuint getOutputTexture() const = 0;

    /**
     * 从 FBO 读回渲染结果（调试/录屏用）
     * @param output  输出图像 (BGR, CV_8UC3)
     * @return true 成功
     */
    virtual bool readPixels(cv::Mat& output) = 0;

    /**
     * 释放所有 GPU 资源
     */
    virtual void destroy() = 0;

    /** 获取鸟瞰图宽度 */
    virtual int getWidth() const = 0;

    /** 获取鸟瞰图高度 */
    virtual int getHeight() const = 0;

    // ─── 透明底盘控制 ───

    /** 开启/关闭透明底盘 */
    void setTransparentChassisEnabled(bool on) { tcEnabled_ = on; }

    /** 是否已开启透明底盘 */
    bool isTransparentChassisEnabled() const   { return tcEnabled_; }

    /** 切换透明底盘 */
    void toggleTransparentChassis()            { tcEnabled_ = !tcEnabled_; }

protected:
    std::string binPath_;
    bool tcEnabled_ = false;  // 透明底盘开关
};
