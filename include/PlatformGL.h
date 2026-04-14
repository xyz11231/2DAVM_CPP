#pragma once

/**
 * PlatformGL.h — 跨平台 OpenGL 头文件和类型抽象
 *
 * QNX 8255: OpenGL ES 3.0 + EGL
 * 桌面 Linux: OpenGL 3.3 + GLEW
 *
 * 用法: 所有 GL 代码包含此头文件替代直接包含 GL/glew.h 或 GLES3/gl3.h
 */

#ifdef QNX_PLATFORM
    // ── QNX: OpenGL ES 3.0 ──
    #include <EGL/egl.h>
    #include <EGL/eglext.h>
    #include <GLES3/gl3.h>
    #include <GLES3/gl3ext.h>

    // ES3 索引推荐使用 unsigned short（兼容性更好）
    #define GL_INDEX_TYPE   GL_UNSIGNED_SHORT
    typedef unsigned short  GLindex;

    // ES 不支持 GL_BGR，读回像素用 GL_RGBA
    #define GL_READBACK_FORMAT  GL_RGBA
    #define GL_READBACK_CHANNELS 4

#else
    // ── 桌面 Linux: OpenGL 3.3 ──
    #include <GL/glew.h>

    #define GL_INDEX_TYPE   GL_UNSIGNED_INT
    typedef unsigned int    GLindex;

    #define GL_READBACK_FORMAT  GL_BGR
    #define GL_READBACK_CHANNELS 3

#endif

// ── 共用常量 ──

// 着色器目录 (运行时根据平台选择)
#ifdef QNX_PLATFORM
    #define SHADER_DIR "/home/ld/new_project/2DAVM_CPP/shaders_es/"
#else
    #define SHADER_DIR "/home/ld/new_project/2DAVM_CPP/shaders/"
#endif
