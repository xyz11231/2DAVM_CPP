#version 300 es
precision highp float;
precision highp sampler2D;

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D avmTex;
uniform sampler2D historyTex;
uniform vec4  carBounds;
uniform float blendWidth;

void main() {
    vec4 avmColor = texture(avmTex, vUV);

    bool inCar = vUV.x >= carBounds.x && vUV.x <= carBounds.z &&
                 vUV.y >= carBounds.y && vUV.y <= carBounds.w;

    if (!inCar) {
        FragColor = avmColor;
        return;
    }

    vec4 histColor = texture(historyTex, vUV);

    float dx = min(vUV.x - carBounds.x, carBounds.z - vUV.x);
    float dy = min(vUV.y - carBounds.y, carBounds.w - vUV.y);
    float d  = min(dx, dy);

    float fadeAlpha = smoothstep(0.0, blendWidth, d);

    float brightness = dot(histColor.rgb, vec3(0.299, 0.587, 0.114));
    float validMask  = smoothstep(0.005, 0.02, brightness);

    FragColor = mix(avmColor, histColor, fadeAlpha * validMask);
}
