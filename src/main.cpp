/**
 * main.cpp — 环视AVM (GPU 版)
 * 所有 remap / 融合 / 车辆图绘制均在 fragment shader 中完成。
 *
 * 纹理布局 (13 个):
 *   0-3:  srcFront/Back/Left/Right  (每帧更新)
 *   4-7:  remapFront/Back/Left/Right (RG32F, 一次上传)
 *   8-11: weightFL/FR/BL/BR         (R32F,  一次上传)
 *   12:   carTex                     (一次上传)
 */

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <chrono>

#include "StitchBinLoader.h"
#include "VideoReader.h"

// ─── 配置 ───
struct Config {
    std::string bin_path  = "/home/ld/new_project/2DAVM_PY/stitch_data.bin";
    std::string video_dir = "/home/ld/data/双目拼接/20260320_2/video";
    std::string front, back, left, right;
    int win_w = 0, win_h = 0;
    bool loop = true;
    Config() {
        front = video_dir + "/front.mp4";
        back  = video_dir + "/back.mp4";
        left  = video_dir + "/left.mp4";
        right = video_dir + "/right.mp4";
    }
};

// ─── OpenGL 辅助 ───
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

// 全屏四边形
static const float kQuad[] = {
    -1, 1, 0, 0,   -1,-1, 0, 1,   1,-1, 1, 1,   1, 1, 1, 0 };
static const unsigned kIdx[] = { 0,1,2, 0,2,3 };

struct Quad {
    GLuint vao=0, vbo=0, ebo=0;
    void init() {
        glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo); glGenBuffers(1,&ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIdx), kIdx, GL_STATIC_DRAW);
        glVertexAttribPointer(0,2,GL_FLOAT,0,16,(void*)0);  glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,2,GL_FLOAT,0,16,(void*)8);  glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }
    void draw() { glBindVertexArray(vao); glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0); }
    void destroy() { glDeleteVertexArrays(1,&vao); glDeleteBuffers(1,&vbo); glDeleteBuffers(1,&ebo); }
};

// ─── GLFW 回调 ───
static bool g_quit=false, g_paused=false, g_save=false;
static void keyCb(GLFWwindow*, int key, int, int act, int) {
    if (act!=GLFW_PRESS) return;
    if (key==GLFW_KEY_ESCAPE||key==GLFW_KEY_Q) g_quit=true;
    if (key==GLFW_KEY_SPACE) g_paused=!g_paused;
    if (key==GLFW_KEY_S) g_save=true;
}

// ═══════════════ main ═══════════════
int main(int /*argc*/, char** /*argv*/) {
    Config cfg;

    // 1. 加载 bin
    StitchBinData bin;
    if (!StitchBinLoader::load(cfg.bin_path, bin)) return 1;

    // 2. 打开视频
    const char* camNames[] = {"front","back","left","right"};
    std::string vidPaths[] = {cfg.front, cfg.back, cfg.left, cfg.right};
    VideoReader readers[4];
    for (int i=0;i<4;++i)
        if (!readers[i].open(vidPaths[i], cfg.loop)) {
            std::cerr<<"[Main] 无法打开: "<<vidPaths[i]<<"\n"; return 1; }

    // 3. GLFW / GLEW
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    int wW = cfg.win_w>0 ? cfg.win_w : bin.total_w;
    int wH = cfg.win_h>0 ? cfg.win_h : bin.total_h;
    auto* window = glfwCreateWindow(wW, wH, "环视AVM GPU", nullptr, nullptr);
    if (!window){ glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCb);
    glfwSwapInterval(1);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return 1;
    glViewport(0, 0, wW, wH);

    // 4. 着色器 (从文件加载)
    std::string vsSrc = readFile("/home/ld/new_project/surround_view_cpp/shaders/vertex.glsl");
    std::string fsSrc = readFile("/home/ld/new_project/surround_view_cpp/shaders/fragment.glsl");
    if (vsSrc.empty() || fsSrc.empty()) {
        std::cerr << "[Main] 着色器文件加载失败\n"; return 1;
    }
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    GLuint prog = linkProgram(vs, fs);
    glDeleteShader(vs); glDeleteShader(fs);
    glUseProgram(prog);

    // 5. Uniform: 纹理单元绑定
    auto setI = [&](const char* n, int v){ glUniform1i(glGetUniformLocation(prog,n),v); };
    auto setF = [&](const char* n, float v){ glUniform1f(glGetUniformLocation(prog,n),v); };
    setI("srcFront",0);   setI("srcBack",1);   setI("srcLeft",2);   setI("srcRight",3);
    setI("remapFront",4); setI("remapBack",5);  setI("remapLeft",6); setI("remapRight",7);
    setI("weightFL",8);   setI("weightFR",9);   setI("weightBL",10); setI("weightBR",11);
    setI("carTex",12);

    // 6. Uniform: 布局参数
    setF("total_w", (float)bin.total_w); setF("total_h", (float)bin.total_h);
    setF("xl", (float)bin.xl_px); setF("xr", (float)bin.xr_px);
    setF("yt", (float)bin.yt_px); setF("yb", (float)bin.yb_px);

    // 获取源视频分辨率
    cv::Mat tmpFrame;
    readers[0].read(tmpFrame);
    setF("src_w", (float)tmpFrame.cols);
    setF("src_h", (float)tmpFrame.rows);
    readers[0].open(vidPaths[0], cfg.loop);  // 重置到起始帧

    setF("proj_fw",(float)bin.proj_shapes[0].width); setF("proj_fh",(float)bin.proj_shapes[0].height);
    setF("proj_bw",(float)bin.proj_shapes[1].width); setF("proj_bh",(float)bin.proj_shapes[1].height);
    setF("proj_lw",(float)bin.proj_shapes[2].width); setF("proj_lh",(float)bin.proj_shapes[2].height);
    setF("proj_rw",(float)bin.proj_shapes[3].width); setF("proj_rh",(float)bin.proj_shapes[3].height);

    // 7. 静态纹理: remap (RG32F)
    GLuint remapTex[4];
    for (int k = 0; k < 4; ++k) {
        int pw = bin.proj_shapes[k].width, ph = bin.proj_shapes[k].height;
        std::vector<float> buf(pw * ph * 2);
        for (int r = 0; r < ph; ++r)
            for (int c = 0; c < pw; ++c) {
                int idx = (r * pw + c) * 2;
                buf[idx]     = bin.map_x[k].at<float>(r, c);
                buf[idx + 1] = bin.map_y[k].at<float>(r, c);
            }
        remapTex[k] = makeTex(pw, ph, GL_RG32F, GL_RG, GL_FLOAT, buf.data(), GL_NEAREST);
    }

    // 8. 静态纹理: weights (R32F)
    GLuint weightTex[4];
    for (int k = 0; k < 4; ++k) {
        cv::Mat w = bin.weights[k];
        if (!w.isContinuous()) w = w.clone();
        weightTex[k] = makeTex(w.cols, w.rows, GL_R32F, GL_RED, GL_FLOAT, w.data, GL_LINEAR);
    }

    // 9. 静态纹理: car
    cv::Mat carRgb;
    cv::cvtColor(bin.car_image, carRgb, cv::COLOR_BGR2RGB);
    GLuint carTex = makeTex(carRgb.cols, carRgb.rows, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE,
                            carRgb.data, GL_LINEAR);

    // 10. 动态纹理: 4路源帧 (初始化为 1×1)
    GLuint srcTex[4];
    for (int k = 0; k < 4; ++k)
        srcTex[k] = makeTex(1, 1, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, nullptr, GL_LINEAR);
    bool srcSized = false;

    // 11. Quad
    Quad quad;
    quad.init();

    // 绑定所有纹理到对应 unit
    auto bindAll = [&]() {
        for (int i = 0; i < 4; ++i) { glActiveTexture(GL_TEXTURE0+i);  glBindTexture(GL_TEXTURE_2D, srcTex[i]); }
        for (int i = 0; i < 4; ++i) { glActiveTexture(GL_TEXTURE4+i);  glBindTexture(GL_TEXTURE_2D, remapTex[i]); }
        for (int i = 0; i < 4; ++i) { glActiveTexture(GL_TEXTURE8+i);  glBindTexture(GL_TEXTURE_2D, weightTex[i]); }
        glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, carTex);
    };

    using Clock = std::chrono::steady_clock;
    auto lastFps = Clock::now();
    int frameCount = 0, saveIdx = 0;

    std::cout << "[Main] GPU 渲染就绪. ESC/Q退出 Space暂停 S截图\n";

    // ═══ 主循环 ═══
    while (!glfwWindowShouldClose(window) && !g_quit) {
        glfwPollEvents();

        if (g_paused) {
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(prog); bindAll(); quad.draw();
            glfwSwapBuffers(window);
            continue;
        }

        // 读取 4 路视频帧
        cv::Mat frames[4];
        bool allOk = true;
        for (int i = 0; i < 4; ++i)
            if (!readers[i].read(frames[i])) { allOk = false; break; }
        if (!allOk) { if (!cfg.loop) break; continue; }

        // 上传源帧到 GPU
        for (int i = 0; i < 4; ++i) {
            cv::Mat rgb;
            cv::cvtColor(frames[i], rgb, cv::COLOR_BGR2RGB);
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, srcTex[i]);
            if (!srcSized)
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, rgb.cols, rgb.rows, 0,
                             GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
            else
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rgb.cols, rgb.rows,
                                GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
        }
        srcSized = true;

        // GPU 渲染
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog);
        bindAll();
        quad.draw();

        glfwSwapBuffers(window);

        // FPS
        ++frameCount;
        auto now = Clock::now();
        double dt = std::chrono::duration<double>(now - lastFps).count();
        if (dt >= 0.1) {
            std::string title = "环视AVM GPU  FPS: " + std::to_string((int)(frameCount / dt));
            glfwSetWindowTitle(window, title.c_str());
            frameCount = 0;
            lastFps = now;
        }
    }

    // 清理
    quad.destroy();
    for (auto& t : srcTex)    glDeleteTextures(1, &t);
    for (auto& t : remapTex)  glDeleteTextures(1, &t);
    for (auto& t : weightTex) glDeleteTextures(1, &t);
    glDeleteTextures(1, &carTex);
    glDeleteProgram(prog);
    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "[Main] 退出\n";
    return 0;
}
