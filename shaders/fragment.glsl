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

vec3 remapSample(sampler2D src, sampler2D rmap, vec2 ruv) {
    vec2 px = texture(rmap, ruv).rg;
    if (px.x < 0.0 || px.y < 0.0) return vec3(0.0);
    vec2 uv = vec2(px.x / src_w, px.y / src_h);
    return texture(src, uv).rgb;
}

void main() {
    float col = TexCoord.x * total_w;
    float row = TexCoord.y * total_h;

    // 车辆区域
    if (row >= yt && row < yb && col >= xl && col < xr) {
        vec2 uv = vec2((col - xl) / (xr - xl), (row - yt) / (yb - yt));
        FragColor = vec4(texture(carTex, uv).rgb, 1.0);
        return;
    }

    vec3 color = vec3(0.0);

    if (row < yt) {
        vec2 fuv = vec2(col / proj_fw, row / proj_fh);
        vec3 fc  = remapSample(srcFront, remapFront, fuv);
        if (col < xl) {
            vec2 luv = vec2(col / proj_lw, row / proj_lh);
            vec3 lc  = remapSample(srcLeft, remapLeft, luv);
            float w  = texture(weightFL, vec2(col / xl, row / yt)).r;
            color = fc * w + lc * (1.0 - w);
        } else if (col >= xr) {
            vec2 ruv2 = vec2((col - xr) / proj_rw, row / proj_rh);
            vec3 rc  = remapSample(srcRight, remapRight, ruv2);
            float w  = texture(weightFR, vec2((col - xr) / (total_w - xr), row / yt)).r;
            color = fc * w + rc * (1.0 - w);
        } else {
            color = fc;
        }
    } else if (row >= yb) {
        vec2 buv = vec2(col / proj_bw, (row - yb) / proj_bh);
        vec3 bc  = remapSample(srcBack, remapBack, buv);
        if (col < xl) {
            vec2 luv = vec2(col / proj_lw, row / proj_lh);
            vec3 lc  = remapSample(srcLeft, remapLeft, luv);
            float w  = texture(weightBL, vec2(col / xl, (row - yb) / (total_h - yb))).r;
            color = bc * w + lc * (1.0 - w);
        } else if (col >= xr) {
            vec2 ruv2 = vec2((col - xr) / proj_rw, row / proj_rh);
            vec3 rc  = remapSample(srcRight, remapRight, ruv2);
            float w  = texture(weightBR, vec2((col - xr) / (total_w - xr),
                                              (row - yb) / (total_h - yb))).r;
            color = bc * w + rc * (1.0 - w);
        } else {
            color = bc;
        }
    } else {
        if (col < xl) {
            vec2 luv = vec2(col / proj_lw, row / proj_lh);
            color = remapSample(srcLeft, remapLeft, luv);
        } else if (col >= xr) {
            vec2 ruv2 = vec2((col - xr) / proj_rw, row / proj_rh);
            color = remapSample(srcRight, remapRight, ruv2);
        }
    }

    FragColor = vec4(color, 1.0);
}
