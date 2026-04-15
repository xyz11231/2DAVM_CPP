#version 300 es
precision highp float;
precision highp sampler2D;

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D avmTex;
uniform sampler2D historyTex;
uniform vec4  carBounds;
uniform mat3  motionMatrix;

void main() {
    bool inCar = vUV.x >= carBounds.x && vUV.x <= carBounds.z &&
                 vUV.y >= carBounds.y && vUV.y <= carBounds.w;

    if (inCar) {
        vec3 shiftedUVW = motionMatrix * vec3(vUV, 1.0);
        vec2 shifted = clamp(shiftedUVW.xy / shiftedUVW.z, vec2(0.0), vec2(1.0));
        FragColor = texture(historyTex, shifted);
    } else {
        FragColor = texture(avmTex, vUV);
    }
}
