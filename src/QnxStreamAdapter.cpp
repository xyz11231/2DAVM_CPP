/**
 * QnxStreamAdapter — QNX 摄像头流适配器实现
 *
 * 通过 VDBMessageManager 初始化 VDB 通信，
 * 通过 StreamHandler 获取4路摄像头的物理地址映射数据。
 */

#ifdef QNX_PLATFORM

#include "QnxStreamAdapter.h"
#include "StreamHandler.h"
#include "VDBMessageManager.h"
#include <cstdio>

// StreamHandler 实例 (全局单例，与 SDK demo 保持一致)
static StreamHandler g_streamHandler;

QnxStreamAdapter::~QnxStreamAdapter() {
    if (initialized_) destroy();
}

bool QnxStreamAdapter::init() {
    if (initialized_) return true;

    // 启动摄像头流 (内部会初始化 VDB 并获取 camera_info)
    if (!g_streamHandler.startStream()) {
        printf("[QnxStreamAdapter] Failed to start camera stream\n");
        return false;
    }

    initialized_ = true;
    printf("[QnxStreamAdapter] Camera stream initialized\n");
    return true;
}

bool QnxStreamAdapter::getFrames(CameraFrame frames[4]) {
    if (!initialized_) return false;

    // 顺序: front=1, back=2, left=0, right=3 (SDK camera 索引映射)
    // SDK 中: index 0=left, 1=front, 2=rear, 3=right
    // 项目中: [0]=front, [1]=back, [2]=left, [3]=right
    struct { int sdkIdx; int projIdx; } mapping[] = {
        {1, 0},  // SDK front(1) → 项目 front(0)
        {2, 1},  // SDK rear(2)  → 项目 back(1)
        {0, 2},  // SDK left(0)  → 项目 left(2)
        {3, 3},  // SDK right(3) → 项目 right(3)
    };

    for (int i = 0; i < 4; ++i) {
        auto* buf = g_streamHandler.getCameraImageBuffer(mapping[i].sdkIdx);
        int idx = mapping[i].projIdx;

        if (buf && buf->virtual_data) {
            frames[idx].data   = buf->virtual_data;
            frames[idx].width  = buf->width;
            frames[idx].height = buf->height;
            frames[idx].size   = buf->image_size;
            frames[idx].valid  = true;
        } else {
            frames[idx].valid = false;
        }
    }

    return frames[0].valid && frames[1].valid && frames[2].valid && frames[3].valid;
}

void QnxStreamAdapter::destroy() {
    if (!initialized_) return;
    g_streamHandler.stopStream();
    initialized_ = false;
    printf("[QnxStreamAdapter] Camera stream stopped\n");
}

#endif // QNX_PLATFORM
