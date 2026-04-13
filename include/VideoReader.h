#pragma once
#include <string>
#include <opencv2/opencv.hpp>
#include <vector>
/**
 * VideoReader
 * 封装 OpenCV VideoCapture，从视频文件（或摄像头）读取帧。
 * 支持循环播放（loop=true），适合用单个测试视频反复验证。
 */
class VideoReader {
public:
    VideoReader() = default;
    ~VideoReader() { release(); }

    /**
     * 打开视频文件或摄像头
     * @param source  文件路径字符串（或摄像头索引字符串，如 "0"）
     * @param loop    是否循环播放
     */
    bool open(const std::string& source, bool loop = true);

    /**
     * 读取下一帧，返回 BGR cv::Mat
     * 若到达末尾且 loop=true，自动重新打开
     */
    bool read(cv::Mat& frame);

    void release();
    bool isOpened() const;

    int width()  const;
    int height() const;
    double fps() const;

private:
    cv::VideoCapture cap_;
    std::string source_;
    bool loop_ = true;

    // 图片目录模式支持
    bool is_image_dir_ = false;
    std::vector<std::string> image_files_;
    int image_index_ = 0;
    int img_w_ = 0;
    int img_h_ = 0;
};
