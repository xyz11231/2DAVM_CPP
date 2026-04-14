cmake_minimum_required(VERSION 3.16)
project(surround_view_cpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

# --- 依赖 ---
set(OpenCV_DIR /home/ld/project/opencv/install/lib/cmake/opencv4)
find_package(OpenCV REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

# 主可执行文件
add_executable(surround_view
    src/StitchBinLoader.cpp
    src/VideoReader.cpp
    src/AVM2D.cpp
    src/SharedTextureManager.cpp
    src/ScreenCompositor.cpp
    src/TransparentChassis.cpp
    src/LuminanceBalancer.cpp
    src/main.cpp
)

target_link_libraries(surround_view
    ${OpenCV_LIBS}
    OpenGL::GL
    glfw
    GLEW::GLEW
)
