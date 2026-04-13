#include "StitchBinLoader.h"
#include <fstream>
#include <iostream>
#include <cstring>

// bin 文件 header 常量
static const char MAGIC[4] = {'S', 'V', '2', 'D'};
static const uint32_t EXPECTED_VERSION = 2;

// Header 布局 (小端):
//   magic(4) + version(4) + total_w(4) + total_h(4) +
//   xl(4) + xr(4) + yt(4) + yb(4) +
//   wm_w(4) + wm_h(4) + car_w(4) + car_h(4) +
//   proj_shapes: 4 x (pw(4) + ph(4)) = 32
// 总计: 4 + 11*4 + 8*4 = 4 + 44 + 32 = 80 bytes
// 注: Python struct.calcsize("<4s11I8I") = 4 + 11*4 + 8*4 = 80
//   但 Python pack 实际是 4s + 19I = 4 + 19*4 = 80

static const size_t HEADER_SIZE = 4 + 19 * 4;  // 80 bytes

bool StitchBinLoader::load(const std::string& binPath, StitchBinData& data) {
    std::ifstream f(binPath, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "[BinLoader] 无法打开文件: " << binPath << "\n";
        return false;
    }

    // ── 读取 Header ──
    std::vector<uint8_t> headerBuf(HEADER_SIZE);
    f.read(reinterpret_cast<char*>(headerBuf.data()), HEADER_SIZE);
    if (!f) {
        std::cerr << "[BinLoader] Header 读取失败\n";
        return false;
    }

    // 解析 header
    const uint8_t* p = headerBuf.data();

    // magic
    if (std::memcmp(p, MAGIC, 4) != 0) {
        std::cerr << "[BinLoader] 无效的 bin 文件 (magic 不匹配)\n";
        return false;
    }
    p += 4;

    auto readU32 = [&]() -> uint32_t {
        uint32_t v;
        std::memcpy(&v, p, 4);
        p += 4;
        return v;
    };

    uint32_t version = readU32();
    if (version != EXPECTED_VERSION) {
        std::cerr << "[BinLoader] 版本不匹配 (文件=" << version
                  << ", 期望=" << EXPECTED_VERSION << ")\n";
        return false;
    }

    data.total_w = (int)readU32();
    data.total_h = (int)readU32();
    data.xl_px   = (int)readU32();
    data.xr_px   = (int)readU32();
    data.yt_px   = (int)readU32();
    data.yb_px   = (int)readU32();

    int wm_w = (int)readU32();
    int wm_h = (int)readU32();
    int car_w = (int)readU32();
    int car_h = (int)readU32();

    for (int i = 0; i < 4; ++i) {
        int pw = (int)readU32();
        int ph = (int)readU32();
        data.proj_shapes[i] = cv::Size(pw, ph);
    }

    std::cout << "[BinLoader] Header 解析完成:\n"
              << "  鸟瞰图: " << data.total_w << "×" << data.total_h << "\n"
              << "  xl=" << data.xl_px << " xr=" << data.xr_px
              << " yt=" << data.yt_px << " yb=" << data.yb_px << "\n"
              << "  weights/masks: " << wm_w << "×" << wm_h << "\n"
              << "  car: " << car_w << "×" << car_h << "\n";

    for (int i = 0; i < 4; ++i) {
        std::cout << "  proj[" << i << "]: "
                  << data.proj_shapes[i].width << "×"
                  << data.proj_shapes[i].height << "\n";
    }

    // ── 读取 Remap Maps ──
    for (int i = 0; i < 4; ++i) {
        int pw = data.proj_shapes[i].width;
        int ph = data.proj_shapes[i].height;
        size_t n = (size_t)pw * ph;

        data.map_x[i] = cv::Mat(ph, pw, CV_32FC1);
        data.map_y[i] = cv::Mat(ph, pw, CV_32FC1);

        f.read(reinterpret_cast<char*>(data.map_x[i].data), n * sizeof(float));
        f.read(reinterpret_cast<char*>(data.map_y[i].data), n * sizeof(float));

        if (!f) {
            std::cerr << "[BinLoader] remap 数据读取失败 (camera " << i << ")\n";
            return false;
        }
    }

    // ── 读取 Weights (float32, H×W×4 交错存储) ──
    {
        size_t n = (size_t)wm_w * wm_h * 4;
        std::vector<float> wBuf(n);
        f.read(reinterpret_cast<char*>(wBuf.data()), n * sizeof(float));
        if (!f) {
            std::cerr << "[BinLoader] weights 数据读取失败\n";
            return false;
        }

        // 拆分为 4 个单通道 Mat
        for (int k = 0; k < 4; ++k) {
            data.weights[k] = cv::Mat(wm_h, wm_w, CV_32FC1);
        }
        for (int y = 0; y < wm_h; ++y) {
            for (int x = 0; x < wm_w; ++x) {
                size_t base = ((size_t)y * wm_w + x) * 4;
                for (int k = 0; k < 4; ++k) {
                    data.weights[k].at<float>(y, x) = wBuf[base + k];
                }
            }
        }
    }

    // ── 读取 Masks (uint8, H×W×4 交错存储) ──
    {
        size_t n = (size_t)wm_w * wm_h * 4;
        std::vector<uint8_t> mBuf(n);
        f.read(reinterpret_cast<char*>(mBuf.data()), n);
        if (!f) {
            std::cerr << "[BinLoader] masks 数据读取失败\n";
            return false;
        }

        for (int k = 0; k < 4; ++k) {
            data.masks[k] = cv::Mat(wm_h, wm_w, CV_8UC1);
        }
        for (int y = 0; y < wm_h; ++y) {
            for (int x = 0; x < wm_w; ++x) {
                size_t base = ((size_t)y * wm_w + x) * 4;
                for (int k = 0; k < 4; ++k) {
                    data.masks[k].at<uint8_t>(y, x) = mBuf[base + k];
                }
            }
        }
    }

    // ── 读取 Car Image (uint8, H×W×3, RGB 存储 → 转 BGR) ──
    {
        size_t n = (size_t)car_w * car_h * 3;
        std::vector<uint8_t> carBuf(n);
        f.read(reinterpret_cast<char*>(carBuf.data()), n);
        if (!f) {
            std::cerr << "[BinLoader] car_image 数据读取失败\n";
            return false;
        }

        // Python 中 car_image 是 BGR (由 OpenCV 加载)，直接用
        cv::Mat carRaw(car_h, car_w, CV_8UC3, carBuf.data());
        data.car_image = carRaw.clone();
    }

    std::cout << "[BinLoader] bin 文件加载完成: " << binPath << "\n";
    return true;
}
