#version 330 core
/**
 * tc_update.glsl — 地面记忆更新着色器
 *
 * 逻辑:
 *   - 非车体区域: 直接从当前 AVM 渲染结果取新鲜地面数据
 *   - 车体区域:   从上一帧 history 中取数据，加上运动偏移补偿 (平移+旋转)
 */
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D avmTex;       // 当前 AVM 渲染结果
uniform sampler2D historyTex;   // 上一帧地面记忆
uniform vec4  carBounds;        // (left, bottom, right, top) UV 空间
uniform mat3  motionMatrix;     // 运动补偿 3x3 矩阵 (UV空间)

void main() {
    bool inCar = vUV.x >= carBounds.x && vUV.x <= carBounds.z &&
                 vUV.y >= carBounds.y && vUV.y <= carBounds.w;

    if (inCar) {
        // 车体区域: 使用 3x3 矩阵从前一帧采样 (平移+旋转补偿)
        vec3 shiftedUVW = motionMatrix * vec3(vUV, 1.0);
        vec2 shifted = shiftedUVW.xy / shiftedUVW.z;
        shifted = clamp(shifted, vec2(0.0), vec2(1.0));
        FragColor = texture(historyTex, shifted);
    } else {
        // 地面区域: 用当前 AVM 新鲜数据
        FragColor = texture(avmTex, vUV);
    }
}
