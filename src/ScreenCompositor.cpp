#include "ScreenCompositor.h"
#include <iostream>
#include <fstream>
#include <sstream>

// ─── 内部辅助 ───

static std::string readFile_(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[Compositor] 无法打开文件: " << path << "\n";
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compileShader_(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; glGetShaderInfoLog(s, 1024, nullptr, log);
               std::cerr << "[Compositor Shader] " << log << "\n"; }
    return s;
}

static GLuint linkProgram_(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[1024]; glGetProgramInfoLog(p, 1024, nullptr, log);
               std::cerr << "[Compositor Link] " << log << "\n"; }
    return p;
}

// 全屏四边形（V 坐标翻转，修正 FBO 纹理 Y 轴方向）
static const float kQuad[] = {
    -1, 1, 0, 1,   -1,-1, 0, 0,   1,-1, 1, 0,   1, 1, 1, 1 };
static const unsigned kIdx[] = { 0,1,2, 0,2,3 };

// ═══════════════════════════════════════════════

ScreenCompositor::~ScreenCompositor() {
    if (initialized_) destroy();
}

bool ScreenCompositor::init() {
    std::string shaderDir = "/home/ld/new_project/2DAVM_CPP/shaders/";
    std::string vsSrc = readFile_(shaderDir + "compositor_vertex.glsl");
    std::string fsSrc = readFile_(shaderDir + "compositor_fragment.glsl");
    if (vsSrc.empty() || fsSrc.empty()) {
        std::cerr << "[Compositor] 着色器加载失败\n";
        return false;
    }
    GLuint vs = compileShader_(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader_(GL_FRAGMENT_SHADER, fsSrc);
    program_ = linkProgram_(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // 设置纹理单元
    glUseProgram(program_);
    glUniform1i(glGetUniformLocation(program_, "uTexture"), 0);

    // 初始化四边形
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

    initialized_ = true;
    std::cout << "[Compositor] 初始化完成\n";
    return true;
}

void ScreenCompositor::begin() {
    // 渲染到默认帧缓冲（屏幕）
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
}

void ScreenCompositor::draw(GLuint texture, const Viewport& vp) {
    glViewport(vp.x, vp.y, vp.w, vp.h);

    glUseProgram(program_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glBindVertexArray(quadVAO_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void ScreenCompositor::destroy() {
    if (!initialized_) return;
    glDeleteVertexArrays(1, &quadVAO_);
    glDeleteBuffers(1, &quadVBO_);
    glDeleteBuffers(1, &quadEBO_);
    glDeleteProgram(program_);
    initialized_ = false;
    std::cout << "[Compositor] 资源已释放\n";
}
