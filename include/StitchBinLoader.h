#pragma once
#include <string>
#include <array>
#include <vector>
#include <opencv2/opencv.hpp>

/**
 * StitchBinLoader
 * 加载 Python run_stitch.py pack 生成的 .bin 文件，
 * 包含预计算的 remap 查找表、weights、masks 和 car_image。
 *
 * bin 文件格式 v2:
 *   Header (84 bytes):
 *     magic(4) + version(4) + total_w/h(8) + xl/xr/yt/yb(16) +
 *     wm_w/h(8) + car_w/h(8) + proj_shapes 4x(w,h)(32)
 *   Body:
 *     remap_maps: 4路 (map_x float32 + map_y float32)
 *     weights:    float32[wm_h, wm_w, 4]
 *     masks:      uint8  [wm_h, wm_w, 4]
 *     car_image:  uint8  [car_h, car_w, 3]
 */
struct StitchBinData {
    // 鸟瞰图参数
    int total_w = 0, total_h = 0;
    int xl_px = 0, xr_px = 0, yt_px = 0, yb_px = 0;

    // 每路相机的投影尺寸 (width, height)，顺序: front/back/left/right
    std::array<cv::Size, 4> proj_shapes;

    // 4路 remap 查找表 (从原始畸变图直接映射到投影图)
    std::array<cv::Mat, 4> map_x;   // float32
    std::array<cv::Mat, 4> map_y;   // float32

    // 权重图 (单通道 float32，对应4个融合区域)
    std::array<cv::Mat, 4> weights;

    // 掩码图 (单通道 uint8)
    std::array<cv::Mat, 4> masks;

    // 车辆图像 (BGR, CV_8UC3)
    cv::Mat car_image;
};

class StitchBinLoader {
public:
    /**
     * 从 bin 文件加载所有拼接数据
     * @param binPath  bin 文件路径
     * @param data     输出数据
     * @return true 成功
     */
    static bool load(const std::string& binPath, StitchBinData& data);
};
