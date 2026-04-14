#version 300 es
precision highp float;
precision highp sampler2D;
in  vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D srcFront, srcBack, srcLeft, srcRight;
uniform sampler2D remapFront, remapBack, remapLeft, remapRight;
uniform sampler2D weightFL, weightFR, weightBL, weightBR;
uniform sampler2D carTex;

uniform float total_w, total_h;
uniform float xl, xr, yt, yb;
uniform float src_w, src_h;
uniform float proj_fw, proj_fh;
uniform float proj_bw, proj_bh;
uniform float proj_lw, proj_lh;
uniform float proj_rw, proj_rh;
uniform float feather_w;  // 羽化带宽度（像素）
uniform vec4  exposure;   // 光照平衡增益 (front/back/left/right)

// YUV 输入标志: 0=RGB, 1=YUV422(UYVY)
uniform int   inputYUV;

// ── YUV→RGB 转换 ──
// UYVY 编码: 每 4 字节 = [U, Y0, V, Y1]
// 纹理按 half-width RGBA 上传: 一个 RGBA 像素 = (U, Y0, V, Y1)
vec3 yuvToRgb(float y, float u, float v) {
    // BT.601 标准
    float r = y + 1.402 * (v - 0.5);
    float g = y - 0.344136 * (u - 0.5) - 0.714136 * (v - 0.5);
    float b = y + 1.772 * (u - 0.5);
    return clamp(vec3(r, g, b), 0.0, 1.0);
}

vec3 sampleYUV(sampler2D src, vec2 uv) {
    // UYVY 纹理以 half-width RGBA 上传
    // 原始宽度 = 纹理宽度 * 2
    // uv.x 对应原始像素列号
    float texW = src_w * 0.5;  // 纹理实际宽度 (half)
    float pixelX = uv.x * src_w;  // 原始像素 x
    float macroX = floor(pixelX * 0.5);  // 宏像素索引
    float sampleU = (macroX + 0.5) / texW;

    vec4 yuv_data = texture(src, vec2(sampleU, uv.y));
    float U  = yuv_data.r;
    float Y0 = yuv_data.g;
    float V  = yuv_data.b;
    float Y1 = yuv_data.a;

    // 根据奇偶选 Y
    float isOdd = fract(pixelX) > 0.25 ? 1.0 : 0.0;  // 近似判断
    float Y = mix(Y0, Y1, isOdd);

    return yuvToRgb(Y, U, V);
}

vec3 remapSample(sampler2D src, sampler2D rmap, vec2 ruv, float gain) {
    vec2 px = texture(rmap, ruv).rg;
    if (px.x < 0.0 || px.y < 0.0) return vec3(0.0);
    vec2 uv = vec2(px.x / src_w, px.y / src_h);
    vec3 color;
    if (inputYUV == 1) {
        color = sampleYUV(src, uv);
    } else {
        color = texture(src, uv).rgb;
    }
    return color * gain;
}

// 带光照校正的便捷采样
vec3 sampleF(vec2 ruv) { return remapSample(srcFront, remapFront, ruv, exposure.x); }
vec3 sampleB(vec2 ruv) { return remapSample(srcBack,  remapBack,  ruv, exposure.y); }
vec3 sampleL(vec2 ruv) { return remapSample(srcLeft,  remapLeft,  ruv, exposure.z); }
vec3 sampleR(vec2 ruv) { return remapSample(srcRight, remapRight, ruv, exposure.w); }

void main() {
    float col = TexCoord.x * total_w;
    float row = TexCoord.y * total_h;
    float fw  = feather_w;

    // 车辆区域
    if (row >= yt && row < yb && col >= xl && col < xr) {
        vec2 uv = vec2((col - xl) / (xr - xl), (row - yt) / (yb - yt));
        FragColor = vec4(texture(carTex, uv).rgb, 1.0);
        return;
    }

    vec3 color = vec3(0.0);

    if (row < yt) {
        // ═══ 顶部条带 ═══
        vec2 fuv = vec2(col / proj_fw, row / proj_fh);
        vec3 fc  = sampleF(fuv);

        if (col < xl) {
            // FL 融合区（已有权重融合）
            vec2 luv = vec2(col / proj_lw, row / proj_lh);
            vec3 lc  = sampleL(luv);
            float w  = texture(weightFL, vec2(col / xl, row / yt)).r;
            color = fc * w + lc * (1.0 - w);
        } else if (col >= xr) {
            // FR 融合区（已有权重融合）
            vec2 ruv2 = vec2((col - xr) / proj_rw, row / proj_rh);
            vec3 rc  = sampleR(ruv2);
            float w  = texture(weightFR, vec2((col - xr) / (total_w - xr), row / yt)).r;
            color = fc * w + rc * (1.0 - w);
        } else {
            // Front 纯区
            color = fc;

            // 缝隙1: 左侧羽化 (col ≈ xl)
            if (col < xl + fw) {
                vec2 luv = vec2(col / proj_lw, row / proj_lh);
                vec3 lc  = sampleL(luv);
                float wE = texture(weightFL, vec2(1.0, row / yt)).r;
                float t  = smoothstep(0.0, fw, col - xl);
                color = mix(fc * wE + lc * (1.0 - wE), fc, t);
            }
            // 缝隙2: 右侧羽化 (col ≈ xr)
            else if (col >= xr - fw) {
                vec2 ruv2 = vec2((col - xr) / proj_rw, row / proj_rh);
                vec3 rc  = sampleR(ruv2);
                float wE = texture(weightFR, vec2(0.0, row / yt)).r;
                float t  = smoothstep(0.0, fw, xr - col);
                color = mix(fc * wE + rc * (1.0 - wE), fc, t);
            }
        }

    } else if (row >= yb) {
        // ═══ 底部条带 ═══
        vec2 buv = vec2(col / proj_bw, (row - yb) / proj_bh);
        vec3 bc  = sampleB(buv);

        if (col < xl) {
            // BL 融合区
            vec2 luv = vec2(col / proj_lw, row / proj_lh);
            vec3 lc  = sampleL(luv);
            float w  = texture(weightBL, vec2(col / xl, (row - yb) / (total_h - yb))).r;
            color = bc * w + lc * (1.0 - w);
        } else if (col >= xr) {
            // BR 融合区
            vec2 ruv2 = vec2((col - xr) / proj_rw, row / proj_rh);
            vec3 rc  = sampleR(ruv2);
            float w  = texture(weightBR, vec2((col - xr) / (total_w - xr),
                                              (row - yb) / (total_h - yb))).r;
            color = bc * w + rc * (1.0 - w);
        } else {
            // Back 纯区
            color = bc;

            // 缝隙3: 左侧羽化 (col ≈ xl)
            if (col < xl + fw) {
                vec2 luv = vec2(col / proj_lw, row / proj_lh);
                vec3 lc  = sampleL(luv);
                float wE = texture(weightBL, vec2(1.0, (row - yb) / (total_h - yb))).r;
                float t  = smoothstep(0.0, fw, col - xl);
                color = mix(bc * wE + lc * (1.0 - wE), bc, t);
            }
            // 缝隙4: 右侧羽化 (col ≈ xr)
            else if (col >= xr - fw) {
                vec2 ruv2 = vec2((col - xr) / proj_rw, row / proj_rh);
                vec3 rc  = sampleR(ruv2);
                float wE = texture(weightBR, vec2(0.0, (row - yb) / (total_h - yb))).r;
                float t  = smoothstep(0.0, fw, xr - col);
                color = mix(bc * wE + rc * (1.0 - wE), bc, t);
            }
        }

    } else {
        // ═══ 中间条带 (yt ≤ row < yb) ═══
        if (col < xl) {
            // Left 纯区
            vec2 luv = vec2(col / proj_lw, row / proj_lh);
            vec3 lc  = sampleL(luv);
            color = lc;

            // 缝隙5: 上侧羽化 (row ≈ yt)
            if (row < yt + fw) {
                vec2 fuv = vec2(col / proj_fw, row / proj_fh);
                vec3 fc  = sampleF(fuv);
                float wE = texture(weightFL, vec2(col / xl, 1.0)).r;
                float t  = smoothstep(0.0, fw, row - yt);
                color = mix(fc * wE + lc * (1.0 - wE), lc, t);
            }
            // 缝隙7: 下侧羽化 (row ≈ yb)
            else if (row >= yb - fw) {
                vec2 buv = vec2(col / proj_bw, (row - yb) / proj_bh);
                vec3 bc  = sampleB(buv);
                float wE = texture(weightBL, vec2(col / xl, 0.0)).r;
                float t  = smoothstep(0.0, fw, yb - row);
                color = mix(bc * wE + lc * (1.0 - wE), lc, t);
            }

        } else if (col >= xr) {
            // Right 纯区
            vec2 ruv2 = vec2((col - xr) / proj_rw, row / proj_rh);
            vec3 rc   = sampleR(ruv2);
            color = rc;

            // 缝隙6: 上侧羽化 (row ≈ yt)
            if (row < yt + fw) {
                vec2 fuv = vec2(col / proj_fw, row / proj_fh);
                vec3 fc  = sampleF(fuv);
                float wE = texture(weightFR, vec2((col - xr) / (total_w - xr), 1.0)).r;
                float t  = smoothstep(0.0, fw, row - yt);
                color = mix(fc * wE + rc * (1.0 - wE), rc, t);
            }
            // 缝隙8: 下侧羽化 (row ≈ yb)
            else if (row >= yb - fw) {
                vec2 buv = vec2(col / proj_bw, (row - yb) / proj_bh);
                vec3 bc  = sampleB(buv);
                float wE = texture(weightBR, vec2((col - xr) / (total_w - xr), 0.0)).r;
                float t  = smoothstep(0.0, fw, yb - row);
                color = mix(bc * wE + rc * (1.0 - wE), rc, t);
            }
        }
    }

    FragColor = vec4(color, 1.0);
}
