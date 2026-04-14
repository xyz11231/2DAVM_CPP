#include "AVM2D.h"
#include <GLFW/glfw3.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

// ═══════════════════════════════════════════════
//  内部辅助函数（文件作用域）
// ═══════════════════════════════════════════════

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[Shader] 无法打开文件: " << path << "\n";
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compileShader(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; glGetShaderInfoLog(s, 1024, nullptr, log);
               std::cerr << "[Shader] " << log << "\n"; }
    return s;
}

static GLuint linkProgram(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[1024]; glGetProgramInfoLog(p, 1024, nullptr, log);
               std::cerr << "[Link] " << log << "\n"; }
    return p;
}

static GLuint makeTex(int w, int h, GLint ifmt, GLenum fmt, GLenum type,
                      const void* data, GLint filter) {
    GLuint t; glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, type, data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

// 全屏四边形顶点数据
static const float kQuad[] = {
    -1, 1, 0, 0,   -1,-1, 0, 1,   1,-1, 1, 1,   1, 1, 1, 0 };
static const unsigned kIdx[] = { 0,1,2, 0,2,3 };

// ═══════════════════════════════════════════════
//  AVM2D 实现
// ═══════════════════════════════════════════════

AVM2D::AVM2D(const std::string& binPath)
    : AVM(binPath) {}

AVM2D::~AVM2D() {
    if (initialized_) {
        destroy();
    }
}

bool AVM2D::init() {
    // 1. 加载 bin 数据
    if (!StitchBinLoader::load(binPath_, binData_)) {
        std::cerr << "[AVM2D] 加载 bin 文件失败: " << binPath_ << "\n";
        return false;
    }

    // 2. 编译着色器
    std::string shaderDir = "/home/ld/new_project/2DAVM_CPP/shaders/";
    std::string vsSrc = readFile(shaderDir + "vertex.glsl");
    std::string fsSrc = readFile(shaderDir + "fragment.glsl");
    if (vsSrc.empty() || fsSrc.empty()) {
        std::cerr << "[AVM2D] 着色器文件加载失败\n";
        return false;
    }
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    program_ = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // 3. 初始化全屏四边形
    initQuad();

    // 4. 上传静态纹理
    uploadStaticTextures();

    // 5. 初始化 FBO（离屏渲染目标）
    initFBO();

    // 6. 设置 uniform 纹理单元绑定
    glUseProgram(program_);
    auto setI = [&](const char* n, int v){ glUniform1i(glGetUniformLocation(program_,n),v); };
    setI("srcFront",0);   setI("srcBack",1);   setI("srcLeft",2);   setI("srcRight",3);
    setI("remapFront",4); setI("remapBack",5);  setI("remapLeft",6); setI("remapRight",7);
    setI("weightFL",8);   setI("weightFR",9);   setI("weightBL",10); setI("weightBR",11);
    setI("carTex",12);

    // 7. 设置布局参数 uniform
    auto setF = [&](const char* n, float v){ glUniform1f(glGetUniformLocation(program_,n),v); };
    setF("total_w", (float)binData_.total_w); setF("total_h", (float)binData_.total_h);
    setF("xl", (float)binData_.xl_px); setF("xr", (float)binData_.xr_px);
    setF("yt", (float)binData_.yt_px); setF("yb", (float)binData_.yb_px);

    setF("proj_fw",(float)binData_.proj_shapes[0].width); setF("proj_fh",(float)binData_.proj_shapes[0].height);
    setF("proj_bw",(float)binData_.proj_shapes[1].width); setF("proj_bh",(float)binData_.proj_shapes[1].height);
    setF("proj_lw",(float)binData_.proj_shapes[2].width); setF("proj_lh",(float)binData_.proj_shapes[2].height);
    setF("proj_rw",(float)binData_.proj_shapes[3].width); setF("proj_rh",(float)binData_.proj_shapes[3].height);

    initialized_ = true;
    std::cout << "[AVM2D] 初始化完成 (" << binData_.total_w << "×" << binData_.total_h << ")\n";
    return true;
}

bool AVM2D::render(const SourceTextures& src, const SensorData& /*sensorData*/) {
    if (!initialized_) return false;

    // ── 绑定内部 FBO（离屏渲染）──
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, binData_.total_w, binData_.total_h);

    glUseProgram(program_);

    // 设置源帧分辨率 uniform（仅首次）
    if (!srcUniformSet_) {
        auto setF = [&](const char* n, float v){ glUniform1f(glGetUniformLocation(program_,n),v); };
        setF("src_w", (float)src.width);
        setF("src_h", (float)src.height);
        srcUniformSet_ = true;
    }

    // GPU 渲染
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    bindTextures(src.textures);

    // 绘制全屏四边形
    glBindVertexArray(quadVAO_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // ── 解绑 FBO ──
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

bool AVM2D::readPixels(cv::Mat& output) {
    if (!initialized_) return false;

    int w = binData_.total_w;
    int h = binData_.total_h;
    output.create(h, w, CV_8UC3);

    // 绑定内部 FBO 读取
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glReadPixels(0, 0, w, h, GL_BGR, GL_UNSIGNED_BYTE, output.data);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // OpenGL 坐标系 Y 轴向上，需要翻转
    cv::flip(output, output, 0);
    return true;
}

void AVM2D::destroy() {
    if (!initialized_) return;

    for (auto& t : remapTex_)  glDeleteTextures(1, &t);
    for (auto& t : weightTex_) glDeleteTextures(1, &t);
    glDeleteTextures(1, &carTex_);

    if (fboTex_) glDeleteTextures(1, &fboTex_);
    if (fbo_)    glDeleteFramebuffers(1, &fbo_);

    glDeleteVertexArrays(1, &quadVAO_);
    glDeleteBuffers(1, &quadVBO_);
    glDeleteBuffers(1, &quadEBO_);
    glDeleteProgram(program_);

    initialized_ = false;
    std::cout << "[AVM2D] 资源已释放\n";
}

// ─── 内部方法 ───

void AVM2D::initQuad() {
    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);
    glGenBuffers(1, &quadEBO_);
    glBindVertexArray(quadVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIdx), kIdx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, 0, 16, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, 0, 16, (void*)8);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void AVM2D::initFBO() {
    int w = binData_.total_w;
    int h = binData_.total_h;

    glGenFramebuffers(1, &fbo_);
    fboTex_ = makeTex(w, h, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, nullptr, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex_, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[AVM2D] FBO 创建失败\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void AVM2D::uploadStaticTextures() {
    // Remap 纹理 (RG32F)
    for (int k = 0; k < 4; ++k) {
        int pw = binData_.proj_shapes[k].width;
        int ph = binData_.proj_shapes[k].height;
        std::vector<float> buf(pw * ph * 2);
        for (int r = 0; r < ph; ++r)
            for (int c = 0; c < pw; ++c) {
                int idx = (r * pw + c) * 2;
                buf[idx]     = binData_.map_x[k].at<float>(r, c);
                buf[idx + 1] = binData_.map_y[k].at<float>(r, c);
            }
        remapTex_[k] = makeTex(pw, ph, GL_RG32F, GL_RG, GL_FLOAT, buf.data(), GL_NEAREST);
    }

    // Weight 纹理 (R32F)
    for (int k = 0; k < 4; ++k) {
        cv::Mat w = binData_.weights[k];
        if (!w.isContinuous()) w = w.clone();
        weightTex_[k] = makeTex(w.cols, w.rows, GL_R32F, GL_RED, GL_FLOAT, w.data, GL_LINEAR);
    }

    // Car 纹理 (RGB8)
    cv::Mat carRgb;
    cv::cvtColor(binData_.car_image, carRgb, cv::COLOR_BGR2RGB);
    carTex_ = makeTex(carRgb.cols, carRgb.rows, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE,
                      carRgb.data, GL_LINEAR);
}

void AVM2D::bindTextures(const GLuint srcTextures[4]) {
    // 外部传入的源帧纹理绑定到 unit 0-3
    for (int i = 0; i < 4; ++i) { glActiveTexture(GL_TEXTURE0+i);  glBindTexture(GL_TEXTURE_2D, srcTextures[i]); }
    // 静态纹理绑定到 unit 4-12
    for (int i = 0; i < 4; ++i) { glActiveTexture(GL_TEXTURE4+i);  glBindTexture(GL_TEXTURE_2D, remapTex_[i]); }
    for (int i = 0; i < 4; ++i) { glActiveTexture(GL_TEXTURE8+i);  glBindTexture(GL_TEXTURE_2D, weightTex_[i]); }
    glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, carTex_);
}
