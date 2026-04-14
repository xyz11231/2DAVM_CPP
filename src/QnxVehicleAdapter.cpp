/**
 * QnxVehicleAdapter — QNX 车辆信号适配器实现
 */

#ifdef QNX_PLATFORM

#include "QnxVehicleAdapter.h"
#include "VDBMessageManager.h"

void QnxVehicleAdapter::update(SensorData& out) {
    auto& info = VDBMessageManager::getSingleton().GetVehicleInfoA002();

    out.steeringAngle = info.steering_angle;
    out.speed         = static_cast<float>(info.displaySpeed);

    // 映射档位
    switch (info.gear_state) {
        case ENM_CAR_GEAR_STATE_P: out.gearPosition = 0; break;
        case ENM_CAR_GEAR_STATE_R: out.gearPosition = 1; break;
        case ENM_CAR_GEAR_STATE_N: out.gearPosition = 2; break;
        case ENM_CAR_GEAR_STATE_D: out.gearPosition = 3; break;
        default: out.gearPosition = 0; break;
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
