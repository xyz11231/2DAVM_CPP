#version 330 core
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

vec3 remapSample(sampler2D src, sampler2D rmap, vec2 ruv) {
    vec2 px = texture(rmap, ruv).rg;
    if (px.x < 0.0 || px.y < 0.0) return vec3(0.0);
    vec2 uv = vec2(px.x / src_w, px.y / src_h);
    return texture(src, uv).rgb;
}

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
        vec3 fc  = remapSample(srcFront, remapFront, fuv);

        if (col < xl) {
            // FL 融合区（已有权重融合）
            vec2 luv = vec2(col / proj_lw, row / proj_lh);
            vec3 lc  = remapSample(srcLeft, remapLeft, luv);
            float w  = texture(weightFL, vec2(col / xl, row / yt)).r;
            color = fc * w + lc * (1.0 - w);
        } else if (col >= xr) {
            // FR 融合区（已有权重融合）
            vec2 ruv2 = vec2((col - xr) / proj_rw, row / proj_rh);
            vec3 rc  = remapSample(srcRight, remapRight, ruv2);
            float w  = texture(weightFR, vec2((col - xr) / (total_w - xr), row / yt)).r;
            color = fc * w + rc * (1.0 - w);
        } else {
            // Front 纯区
            color = fc;

            // 缝隙1: 左侧羽化 (col ≈ xl)
            if (col < xl + fw) {
                vec2 luv = vec2(col / proj_lw, row / proj_lh);
                vec3 lc  = remapSample(srcLeft, remapLeft, luv);
                float wE = texture(weightFL, vec2(1.0, row / yt)).r;
                float t  = smoothstep(0.0, fw, col - xl);
                color = mix(fc * wE + lc * (1.0 - wE), fc, t);
            }
            // 缝隙2: 右侧羽化 (col ≈ xr)
            else if (col >= xr - fw) {
                vec2 ruv2 = vec2((col - xr) / proj_rw, row / proj_rh);
                vec3 rc  = remapSample(srcRight, remapRight, ruv2);
                float wE = texture(weightFR, vec2(0.0, row / yt)).r;
                float t  = smoothstep(0.0, fw, xr - col);
                color = mix(fc * wE + rc * (1.0 - wE), fc, t);
            }
        }

    } else if (row >= yb) {
        // ═══ 底部条带 ═══
        vec2 buv = vec2(col / proj_bw, (row - yb) / proj_bh);
        vec3 bc  = remapSample(srcBack, remapBack, buv);

        if (col < xl) {
            // BL 融合区
            vec2 luv = vec2(col / proj_lw, row / proj_lh);
            vec3 lc  = remapSample(srcLeft, remapLeft, luv);
            float w  = texture(weightBL, vec2(col / xl, (row - yb) / (total_h - yb))).r;
            color = bc * w + lc * (1.0 - w);
        } else if (col >= xr) {
            // BR 融合区
            vec2 ruv2 = vec2((col - xr) / proj_rw, row / proj_rh);
            vec3 rc  = remapSample(srcRight, remapRight, ruv2);
            float w  = texture(weightBR, vec2((col - xr) / (total_w - xr),
                                              (row - yb) / (total_h - yb))).r;
            color = bc * w + rc * (1.0 - w);
        } else {
            // Back 纯区
            color = bc;

            // 缝隙3: 左侧羽化 (col ≈ xl)
            if (col < xl + fw) {
                vec2 luv = vec2(col / proj_lw, row / proj_lh);
                vec3 lc  = remapSample(srcLeft, remapLeft, luv);
                float wE = texture(weightBL, vec2(1.0, (row - yb) / (total_h - yb))).r;
                float t  = smoothstep(0.0, fw, col - xl);
                color = mix(bc * wE + lc * (1.0 - wE), bc, t);
            }
            // 缝隙4: 右侧羽化 (col ≈ xr)
            else if (col >= xr - fw) {
                vec2 ruv2 = vec2((col - xr) / proj_rw, row / proj_rh);
                vec3 rc  = remapSample(srcRight, remapRight, ruv2);
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
            vec3 lc  = remapSample(srcLeft, remapLeft, luv);
            color = lc;

            // 缝隙5: 上侧羽化 (row ≈ yt)
            if (row < yt + fw) {
                vec2 fuv = vec2(col / proj_fw, row / proj_fh);
                vec3 fc  = remapSample(srcFront, remapFront, fuv);
                float wE = texture(weightFL, vec2(col / xl, 1.0)).r;
                float t  = smoothstep(0.0, fw, row - yt);
                color = mix(fc * wE + lc * (1.0 - wE), lc, t);
            }
            // 缝隙7: 下侧羽化 (row ≈ yb)
            else if (row >= yb - fw) {
                vec2 buv = vec2(col / proj_bw, (row - yb) / proj_bh);
                vec3 bc  = remapSample(srcBack, remapBack, buv);
                float wE = texture(weightBL, vec2(col / xl, 0.0)).r;
                float t  = smoothstep(0.0, fw, yb - row);
                color = mix(bc * wE + lc * (1.0 - wE), lc, t);
            }

        } else if (col >= xr) {
            // Right 纯区
            vec2 ruv2 = vec2((col - xr) / proj_rw, row / proj_rh);
            vec3 rc   = remapSample(srcRight, remapRight, ruv2);
            color = rc;

            // 缝隙6: 上侧羽化 (row ≈ yt)
            if (row < yt + fw) {
                vec2 fuv = vec2(col / proj_fw, row / proj_fh);
                vec3 fc  = remapSample(srcFront, remapFront, fuv);
                float wE = texture(weightFR, vec2((col - xr) / (total_w - xr), 1.0)).r;
                float t  = smoothstep(0.0, fw, row - yt);
                color = mix(fc * wE + rc * (1.0 - wE), rc, t);
            }
            // 缝隙8: 下侧羽化 (row ≈ yb)
            else if (row >= yb - fw) {
                vec2 buv = vec2(col / proj_bw, (row - yb) / proj_bh);
                vec3 bc  = remapSample(srcBack, remapBack, buv);
                float wE = texture(weightBR, vec2((col - xr) / (total_w - xr), 0.0)).r;
                float t  = smoothstep(0.0, fw, yb - row);
                color = mix(bc * wE + rc * (1.0 - wE), rc, t);
            }
        }
    }

    FragColor = vec4(color, 1.0);
}
