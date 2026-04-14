#pragma once

/**
 * QnxStreamAdapter — QNX 摄像头流适配器
 *
 * 封装 SDK StreamHandler，提供统一的 4 路摄像头帧获取接口。
 * 仅在 QNX 平台编译。
 *
 * 摄像头数据格式: YUV422 (UYVY), width*height*2 字节
 */

#ifdef QNX_PLATFORM

#include <cstdint>

/**
 * CameraFrame — 单路摄像头帧数据
 */
struct CameraFrame {
    void*    data   = nullptr;  // YUV422 数据指针 (物理地址映射)
    int      width  = 0;
    int      height = 0;
    size_t   size   = 0;        // 数据字节数 (width * height * 2)
    bool     valid  = false;
};

/**
 * QnxStreamAdapter — 摄像头流适配器
 *
 * 封装 VDB 通信 + StreamHandler 的初始化和帧获取。
 */
class QnxStreamAdapter {
public:
    QnxStreamAdapter() = default;
    ~QnxStreamAdapter();

    // 禁止拷贝
    QnxStreamAdapter(const QnxStreamAdapter&) = delete;
    QnxStreamAdapter& operator=(const QnxStreamAdapter&) = delete;

    /**
     * 初始化 VDB 通信并启动摄像头流
     * @return true 成功
     */
    bool init();

    /**
     * 获取当前 4 路摄像头帧
     * @param frames  输出数组 [front, back, left, right]
     * @return true 所有帧有效
     */
    bool getFrames(CameraFrame frames[4]);

    /**
     * 停止流并释放资源
     */
    void destroy();

private:
    bool initialized_ = false;
};

#endif // QNX_PLATFORM
