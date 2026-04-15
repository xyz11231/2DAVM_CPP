#include "TransparentChassis.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

// ═══════════════════════════════════════════════
//  内部辅助函数
// ═══════════════════════════════════════════════

static std::string tc_readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[TC] 无法打开文件: " << path << "\n";
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint tc_compileShader(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetShaderInfoLog(s, 1024, nullptr, log);
        std::cerr << "[TC Shader] " << log << "\n";
    }
    return s;
}

static GLuint tc_linkProgram(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetProgramInfoLog(p, 1024, nullptr, log);
        std::cerr << "[TC Link] " << log << "\n";
    }
    return p;
}

// 全屏四边形 (compositor UV 约定: y=0 底, y=1 顶; 与 FBO 纹理原生一致)
static const float kTCQuad[] = {
    // pos(xy)  uv
    -1,  1,  0, 1,   // 左上  → UV (0,1) → 纹理顶部 → 鸟瞰前方
    -1, -1,  0, 0,   // 左下  → UV (0,0) → 纹理底部 → 鸟瞰后方
     1, -1,  1, 0,   // 右下
     1,  1,  1, 1    // 右上
};
static const GLindex kTCIdx[] = { 0,1,2, 0,2,3 };

// ═══════════════════════════════════════════════
//  TransparentChassis 实现
// ═══════════════════════════════════════════════

TransparentChassis::~TransparentChassis() {
    if (initialized_) destroy();
}

bool TransparentChassis::init(int width, int height, int xl, int xr, int yt, int yb) {
    width_  = width;
    height_ = height;

    // 车体边界转 GL UV 空间 (y 翻转)
    carBounds_[0] = (float)xl / width;            // left
    carBounds_[1] = 1.0f - (float)yb / height;    // bottom (yb 是图像下边 → GL UV 更小)
    carBounds_[2] = (float)xr / width;            // right
    carBounds_[3] = 1.0f - (float)yt / height;    // top    (yt 是图像上边 → GL UV 更大)

    // 1. 编译着色器
    if (!compileShaders()) return false;

    // 2. 初始化全屏四边形
    initQuad();

    // 3. 创建 ping-pong history FBO (×2)
    for (int i = 0; i < 2; ++i) {
        historyFBO_[i] = createFBOWithTexture(historyTex_[i]);
        if (!historyFBO_[i]) return false;
        // 初始化为黑色
        glBindFramebuffer(GL_FRAMEBUFFER, historyFBO_[i]);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // 4. 创建输出 FBO
    outputFBO_ = createFBOWithTexture(outputTex_);
    if (!outputFBO_) return false;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    initialized_ = true;
    firstFrame_  = true;
    std::cout << "[TC] 透明底盘初始化完成 (" << width << "×" << height << ")\n";
    return true;
}

void TransparentChassis::update(GLuint avmTexture, const SensorData& sensor) {
    if (!initialized_ || !enabled_) return;

    // ── 计算 dt ──
    auto now = std::chrono::steady_clock::now();
    float dt = 0.f;
    if (!firstFrame_) {
        dt = std::chrono::duration<float>(now - lastTime_).count();
        dt = std::min(dt, 0.1f);  // 钳位，防止暂停后大跳
    }
    lastTime_   = now;
    firstFrame_ = false;

    // ── 计算运动偏移 (UV 空间 3x3 矩阵) ──
    float speed_ms = sensor.speed / 3.6f;  // km/h → m/s
    if (sensor.gearPosition == 1) speed_ms = -speed_ms;  // R 档反向

    // 前进: 地面特征向下移动 (GL UV y 减小)
    // d_px 是实际运动对应的像素数
    float d_px = speed_ms * dt / metersPerPixel_;
    float dY_uv = d_px / (float)height_;  // UV 空间 Y 偏移
    float dX_uv = 0.f;

    // 旋转 $\theta = \omega \cdot dt$
    float theta = sensor.yawRate * dt;

    // 围绕 pivot 的旋转
    float pivotU = (carBounds_[0] + carBounds_[2]) * 0.5f;
    float pivotV = pivotY_;  // 默认0 (底部中心)

    float W = (float)width_;
    float H = (float)height_;
    float C = std::cos(theta);
    float S = std::sin(theta);

    // 构造 UV 空间的 3x3 变换矩阵 (对旧帧画面进行逆旋转和平移，使得旧地面在当前视角下贴合)
    // P_old = R_theta * (P_cur - Pivot) + Pivot + Trans
    // 由于矩阵按列优先存储 (Column-Major) 给 OpenGL
    float motionMatrix[9] = {
        C,                  S * (W / H),        0.0f,  // col 0
        -S * (H / W),       C,                  0.0f,  // col 1
        pivotU - C * pivotU + S * (H / W) * pivotV + dX_uv,
        pivotV - S * (W / H) * pivotU - C * pivotV + dY_uv,
        1.0f                                           // col 2
    };

    // ── Pass 1: 更新地面记忆 ──
    int src = pingPong_;
    int dst = 1 - pingPong_;

    glBindFramebuffer(GL_FRAMEBUFFER, historyFBO_[dst]);
    glViewport(0, 0, width_, height_);

    glUseProgram(updateProgram_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, avmTexture);
    glUniform1i(glGetUniformLocation(updateProgram_, "avmTex"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, historyTex_[src]);
    glUniform1i(glGetUniformLocation(updateProgram_, "historyTex"), 1);

    glUniform4fv(glGetUniformLocation(updateProgram_, "carBounds"), 1, carBounds_);
    glUniformMatrix3fv(glGetUniformLocation(updateProgram_, "motionMatrix"), 1, GL_FALSE, motionMatrix);

    drawQuad();

    // ── Pass 2: 合成输出 ──
    glBindFramebuffer(GL_FRAMEBUFFER, outputFBO_);
    glViewport(0, 0, width_, height_);

    glUseProgram(compositeProgram_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, avmTexture);
    glUniform1i(glGetUniformLocation(compositeProgram_, "avmTex"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, historyTex_[dst]);
    glUniform1i(glGetUniformLocation(compositeProgram_, "historyTex"), 1);

    glUniform4fv(glGetUniformLocation(compositeProgram_, "carBounds"), 1, carBounds_);
    glUniform1f(glGetUniformLocation(compositeProgram_, "blendWidth"), blendWidth_);

    drawQuad();

    // ── 切换 ping-pong ──
    pingPong_ = dst;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void TransparentChassis::destroy() {
    if (!initialized_) return;

    for (int i = 0; i < 2; ++i) {
        if (historyTex_[i]) glDeleteTextures(1, &historyTex_[i]);
        if (historyFBO_[i]) glDeleteFramebuffers(1, &historyFBO_[i]);
    }
    if (outputTex_) glDeleteTextures(1, &outputTex_);
    if (outputFBO_) glDeleteFramebuffers(1, &outputFBO_);

    if (updateProgram_)    glDeleteProgram(updateProgram_);
    if (compositeProgram_) glDeleteProgram(compositeProgram_);

    glDeleteVertexArrays(1, &quadVAO_);
    glDeleteBuffers(1, &quadVBO_);
    glDeleteBuffers(1, &quadEBO_);

    initialized_ = false;
    std::cout << "[TC] 资源已释放\n";
}

// ─── 内部方法 ───

bool TransparentChassis::compileShaders() {
    std::string shaderDir = SHADER_DIR;

    // 共用 compositor 的顶点着色器 (输出 vUV)
    std::string vsSrc = tc_readFile(shaderDir + "compositor_vertex.glsl");
    if (vsSrc.empty()) return false;

    // 更新着色器
    std::string updateFsSrc = tc_readFile(shaderDir + "tc_update.glsl");
    if (updateFsSrc.empty()) return false;

    GLuint vs = tc_compileShader(GL_VERTEX_SHADER, vsSrc);

    GLuint updateFs = tc_compileShader(GL_FRAGMENT_SHADER, updateFsSrc);
    updateProgram_ = tc_linkProgram(vs, updateFs);
    glDeleteShader(updateFs);

    // 合成着色器
    std::string compositeFsSrc = tc_readFile(shaderDir + "tc_composite.glsl");
    if (compositeFsSrc.empty()) { glDeleteShader(vs); return false; }

    GLuint compositeFs = tc_compileShader(GL_FRAGMENT_SHADER, compositeFsSrc);
    compositeProgram_ = tc_linkProgram(vs, compositeFs);
    glDeleteShader(compositeFs);
    glDeleteShader(vs);

    return true;
}

void TransparentChassis::initQuad() {
    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);
    glGenBuffers(1, &quadEBO_);
    glBindVertexArray(quadVAO_);

    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kTCQuad), kTCQuad, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kTCIdx), kTCIdx, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void TransparentChassis::drawQuad() {
    glBindVertexArray(quadVAO_);
    glDrawElements(GL_TRIANGLES, 6, GL_INDEX_TYPE, 0);
    glBindVertexArray(0);
}

GLuint TransparentChassis::createFBOWithTexture(GLuint& tex) {
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[TC] FBO 创建失败\n";
        return 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return fbo;
}
