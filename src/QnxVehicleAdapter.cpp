/**
 * QnxVehicleAdapter — QNX 车辆信号适配器实现
 */

#ifdef QNX_PLATFORM

#include "QnxVehicleAdapter.h"
#include "VDBMessageManager.h"
#include "desaysv/udt.h"
#include <mutex>
#include <iostream>

using namespace desaysv::vdb::udt;

struct QnxVehicleAdapter::Impl {
    std::shared_ptr<Participator> participator;
    std::shared_ptr<Subscriber<desaysv::vdb::Vehicle_0xA00E>> subA00E;
    
    std::mutex mutex;
    desaysv::vdb::Vehicle_0xA00E lastA00E{};
};

QnxVehicleAdapter::~QnxVehicleAdapter() {
    destroy();
}

bool QnxVehicleAdapter::init() {
    if (pImpl_) return true;
    pImpl_ = new Impl();
    
    // 初始化独立 Participator 来监听 A00E
    pImpl_->participator = std::make_shared<Participator>();
    pImpl_->subA00E = pImpl_->participator->CreateSubscriber<desaysv::vdb::Vehicle_0xA00E>(
        "0x0016a00e", [this](const std::shared_ptr<const desaysv::vdb::Vehicle_0xA00E>& msg) {
            if (msg && pImpl_) {
                std::lock_guard<std::mutex> lock(pImpl_->mutex);
                pImpl_->lastA00E = *msg;
            }
        });
    
    std::cout << "[QnxVehicleAdapter] Initialized A00E subscriber\n";
    return true;
}

void QnxVehicleAdapter::destroy() {
    if (pImpl_) {
        pImpl_->subA00E.reset();
        pImpl_->participator.reset();
        delete pImpl_;
        pImpl_ = nullptr;
    }
}

void QnxVehicleAdapter::update(SensorData& out) {
    // 1. 获取 A002 常用信息 (通过全局单例)
    auto& infoA002 = VDBMessageManager::getSingleton().GetVehicleInfoA002();
    out.steeringAngle = infoA002.steering_angle;
    out.speed         = static_cast<float>(infoA002.displaySpeed);

    switch (infoA002.gear_state) {
        case ENM_CAR_GEAR_STATE_P: out.gearPosition = 0; break;
        case ENM_CAR_GEAR_STATE_R: out.gearPosition = 1; break;
        case ENM_CAR_GEAR_STATE_N: out.gearPosition = 2; break;
        case ENM_CAR_GEAR_STATE_D: out.gearPosition = 3; break;
        default: out.gearPosition = 0; break;
    }

    // 2. 获取 A00E IMU / 轮速信息
    if (pImpl_) {
        std::lock_guard<std::mutex> lock(pImpl_->mutex);
        const auto& a00e = pImpl_->lastA00E;
        
        // 缩放系数及偏移（预留 0.01 精度，并转换到 rad/s，后续据实调整）
        // 假设原始值为带符号的偏移值：例如实际值 = (RAW - offset) * 0.01 * (PI/180)
        out.yawRate = static_cast<float>(a00e.YawRate_Infocan) * 0.01f * (3.14159265f / 180.f); 
        
        // 轮速可能附带一些精度系数（如 0.01 km/h / bit），此处预留系数 0.01
        out.wheelSpeedFL = static_cast<float>(a00e.LHFWheelSpeed) * 0.01f;
        out.wheelSpeedFR = static_cast<float>(a00e.RHFWheelSpeed) * 0.01f;
        out.wheelSpeedRL = static_cast<float>(a00e.LHRWheelSpeed) * 0.01f;
        out.wheelSpeedRR = static_cast<float>(a00e.RHRWheelSpeed) * 0.01f;
    }
}

int QnxVehicleAdapter::getGear() const {
    auto& info = VDBMessageManager::getSingleton().GetVehicleInfoA002();
    switch (info.gear_state) {
        case ENM_CAR_GEAR_STATE_P: return 0;
        case ENM_CAR_GEAR_STATE_R: return 1;
        case ENM_CAR_GEAR_STATE_N: return 2;
        case ENM_CAR_GEAR_STATE_D: return 3;
        default: return 0;
    }
}

int QnxVehicleAdapter::getTurnSignal() const {
    auto& info = VDBMessageManager::getSingleton().GetVehicleInfoA002();
    switch (info.turn_signal) {
        case ENM_CAR_TURN_SIGNAL_NONE:   return 0;
        case ENM_CAR_TURN_SIGNAL_LEFT:   return 1;
        case ENM_CAR_TURN_SIGNAL_RIGHT:  return 2;
        case ENM_CAR_TURN_SIGNAL_DOUBLE: return 3;
        default: return 0;
    }
}

#endif // QNX_PLATFORM
