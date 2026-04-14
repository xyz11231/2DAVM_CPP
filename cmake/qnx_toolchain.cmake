# QNX 8255 aarch64 交叉编译工具链
#
# 用法:
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=cmake/qnx_toolchain.cmake -DQNX_PLATFORM=ON
#
# 注意: 使用前需设置 QNX_HOST 和 QNX_TARGET 环境变量,
#       或在此文件中直接修改路径

# ── QNX SDP 路径 (需根据实际安装路径修改) ──
# QNX_HOST:  主机工具路径 (编译器、链接器等)
# QNX_TARGET: 目标 sysroot (头文件、库)
if(NOT DEFINED ENV{QNX_HOST})
    set(QNX_HOST "" CACHE PATH "QNX SDP host tools path")
else()
    set(QNX_HOST "$ENV{QNX_HOST}")
endif()

if(NOT DEFINED ENV{QNX_TARGET})
    set(QNX_TARGET "" CACHE PATH "QNX SDP target sysroot path")
else()
    set(QNX_TARGET "$ENV{QNX_TARGET}")
endif()

# ── 系统信息 ──
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_VERSION 8.0)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# ── 编译器 ──
if(QNX_HOST)
    set(CMAKE_C_COMPILER   "${QNX_HOST}/usr/bin/qcc")
    set(CMAKE_CXX_COMPILER "${QNX_HOST}/usr/bin/q++")
else()
    # fallback: 假设 qcc 在 PATH 中
    set(CMAKE_C_COMPILER   "qcc")
    set(CMAKE_CXX_COMPILER "q++")
endif()

# QNX 编译器需要指定架构
set(CMAKE_C_COMPILER_TARGET   "gcc_ntoaarch64le")
set(CMAKE_CXX_COMPILER_TARGET "gcc_ntoaarch64le")

# ── Sysroot ──
if(QNX_TARGET)
    set(CMAKE_SYSROOT "${QNX_TARGET}")
    set(CMAKE_FIND_ROOT_PATH "${QNX_TARGET}")
endif()

# ── 搜索路径策略 ──
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
