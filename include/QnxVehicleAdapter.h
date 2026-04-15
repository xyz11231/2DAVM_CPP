#pragma once

/**
 * QnxVehicleAdapter — QNX 车辆信号适配器
 *
 * 将 VDBMessageManager 提供的车辆信息映射到项目通用的 SensorData 结构。
 * 仅在 QNX 平台编译。
 */

#ifdef QNX_PLATFORM

#include "SensorData.h"

class QnxVehicleAdapter {
public:
    QnxVehicleAdapter() = default;
    ~QnxVehicleAdapter();

    bool init();
    void destroy();

    /**
     * 从 VDB 获取最新车辆数据并映射到 SensorData
     * @param out  输出的传感器数据
     */
    void update(SensorData& out);

    /**
     * 获取当前档位 (用于渲染切换)
     * @return 档位: 0=P, 1=R, 2=N, 3=D
     */
    int getGear() const;

    /**
     * 获取转向灯状态
     * @return 0=无, 1=左, 2=右, 3=双闪
     */
    int getTurnSignal() const;

private:
    struct Impl;
    Impl* pImpl_ = nullptr;
};

#endif // QNX_PLATFORM
