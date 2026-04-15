#pragma once
#include <cstdint>

/**
 * SensorData
 * 传感器数据结构体，用于接收车辆传感器信息。
 */
struct SensorData {
    float    steeringAngle = 0.f;   // 方向盘转角 (度)
    float    speed         = 0.f;   // 车速 (km/h)
    int      gearPosition  = 0;     // 档位: 0=P, 1=R, 2=N, 3=D
    float    yawRate       = 0.f;   // 横摆角速度 (rad/s)
    
    // 轮速
    float    wheelSpeedFL  = 0.f;   // 左前轮速 (km/h)
    float    wheelSpeedFR  = 0.f;   // 右前轮速 (km/h)
    float    wheelSpeedRL  = 0.f;   // 左后轮速 (km/h)
    float    wheelSpeedRR  = 0.f;   // 右后轮速 (km/h)

    uint64_t timestamp     = 0;     // 时间戳 (ms)
};
