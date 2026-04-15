#version 330 core
/**
 * tc_composite.glsl — 透明底盘合成着色器
 *
 * 逻辑:
 *   - 非车体区域: 直接输出 AVM 渲染结果
 *   - 车体区域:   用地面记忆替换车辆图像，边缘羽化过渡
 */
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D avmTex;       // 当前 AVM 渲染结果
uniform sampler2D historyTex;   // 更新后的地面记忆
uniform vec4  carBounds;        // (left, bottom, right, top) UV 空间
uniform float blendWidth;       // 边缘羽化宽度 (UV 空间)

void main() {
    vec4 avmColor = texture(avmTex, vUV);

    bool inCar = vUV.x >= carBounds.x && vUV.x <= carBounds.z &&
                 vUV.y >= carBounds.y && vUV.y <= carBounds.w;

    if (!inCar) {
        FragColor = avmColor;
        return;
    }

    vec4 histColor = texture(historyTex, vUV);

    // 边缘距离（到车体边界的最短距离）
    float dx = min(vUV.x - carBounds.x, carBounds.z - vUV.x);
    float dy = min(vUV.y - carBounds.y, carBounds.w - vUV.y);
    float d  = min(dx, dy);

    // 羽化: 边缘平滑过渡到 AVM 原图
    float fadeAlpha = smoothstep(0.0, blendWidth, d);

    // 有效性检测: 仅当 history 有真实数据（非纯黑）时才替换
    float brightness = dot(histColor.rgb, vec3(0.299, 0.587, 0.114));
    float validMask  = smoothstep(0.005, 0.02, brightness);

    FragColor = mix(avmColor, histColor, fadeAlpha * validMask);
}
