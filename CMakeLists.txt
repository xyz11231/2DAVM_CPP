cmake_minimum_required(VERSION 3.16)
project(surround_view_cpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

# ═══ 平台选项 ═══
option(QNX_PLATFORM "Build for QNX 8255 target" OFF)

# ═══ 公共源文件 ═══
set(COMMON_SOURCES
    src/StitchBinLoader.cpp
    src/AVM2D.cpp
    src/SharedTextureManager.cpp
    src/ScreenCompositor.cpp
    src/TransparentChassis.cpp
    src/LuminanceBalancer.cpp
    src/PlatformWindow.cpp
    src/main.cpp
)

# ═══ 平台特定源文件 ═══
if(QNX_PLATFORM)
    set(PLATFORM_SOURCES
        src/QnxStreamAdapter.cpp
        src/QnxVehicleAdapter.cpp
    )
else()
    set(PLATFORM_SOURCES
        src/VideoReader.cpp
    )
endif()

# ═══ 依赖 ═══
if(QNX_PLATFORM)
    # ── QNX 8255: OpenGL ES 3.0 + EGL + QNX Screen ──
    # QNX SDK 路径
    set(QNX_SDK_PATH "/home/ld/new_project/sdk" CACHE PATH "QNX SDK path")

    # QNX SDP sysroot (待配置，EGL/GLES/screen 头文件和库在此路径下)
    set(QNX_SDP_SYSROOT "" CACHE PATH "QNX SDP sysroot path (containing EGL,GLES,screen headers/libs)")

    # OpenCV (QNX 交叉编译版)
    # 需要设置正确的 OpenCV_DIR 指向 QNX 版 OpenCV 的 cmake 配置
    # set(OpenCV_DIR "/path/to/qnx/opencv/lib/cmake/opencv4")
    find_package(OpenCV REQUIRED)

    # VDB 通信库
    set(VDB_INCLUDE_DIR "${QNX_SDK_PATH}/vdb/include" CACHE PATH "VDB include path")
    set(VDB_LIB_DIR     "${QNX_SDK_PATH}/vdb/lib"     CACHE PATH "VDB library path")

    # SDK Demo 头文件 (StreamHandler, VDBMessageManager, etc.)
    set(SDK_DEMO_DIR "${QNX_SDK_PATH}/demo" CACHE PATH "SDK demo source path")
else()
    # ── 桌面 Linux: OpenGL 3.3 + GLFW + GLEW ──
    set(OpenCV_DIR /home/ld/project/opencv/install/lib/cmake/opencv4)
    find_package(OpenCV REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)
    find_package(GLEW REQUIRED)
endif()

# ═══ 主可执行文件 ═══
add_executable(surround_view
    ${COMMON_SOURCES}
    ${PLATFORM_SOURCES}
)

# ═══ Include 路径 ═══
target_include_directories(surround_view PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

if(QNX_PLATFORM)
    target_include_directories(surround_view PRIVATE
        ${VDB_INCLUDE_DIR}
        ${VDB_INCLUDE_DIR}/udt
        ${SDK_DEMO_DIR}
    )
    # 如果 QNX_SDP_SYSROOT 提供了 EGL/GLES/screen 头文件
    if(QNX_SDP_SYSROOT)
        target_include_directories(surround_view PRIVATE
            ${QNX_SDP_SYSROOT}/usr/include
        )
    endif()
else()
    target_include_directories(surround_view PRIVATE
        ${OPENGL_INCLUDE_DIRS}
    )
endif()

# ═══ 编译定义 ═══
if(QNX_PLATFORM)
    target_compile_definitions(surround_view PRIVATE QNX_PLATFORM)
endif()

# ═══ 链接库 ═══
if(QNX_PLATFORM)
    target_link_directories(surround_view PRIVATE
        ${VDB_LIB_DIR}
    )
    if(QNX_SDP_SYSROOT)
        target_link_directories(surround_view PRIVATE
            ${QNX_SDP_SYSROOT}/usr/lib
        )
    endif()

    target_link_libraries(surround_view
        ${OpenCV_LIBS}
        EGL
        GLESv2
        screen
        svp_basic
        svp_comm
        svp_vdb_udt
        pthread
    )
else()
    target_link_libraries(surround_view
        ${OpenCV_LIBS}
        OpenGL::GL
        glfw
        GLEW::GLEW
    )
endif()
