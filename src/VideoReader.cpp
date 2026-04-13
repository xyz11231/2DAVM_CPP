#include "VideoReader.h"
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
bool VideoReader::open(const std::string& source, bool loop) {
    source_ = source;
    loop_   = loop;

    if (fs::exists(source) && fs::is_directory(source)) {
        is_image_dir_ = true;
        for (const auto& entry : fs::directory_iterator(source)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
                    image_files_.push_back(entry.path().string());
                }
            }
        }
        std::sort(image_files_.begin(), image_files_.end());

        if (image_files_.empty()) {
            std::cerr << "[VideoReader] 目录中没有找到图片: " << source << "\n";
            return false;
        }

        // 预读第一张获取宽高
        cv::Mat first = cv::imread(image_files_[0]);
        if (!first.empty()) {
            img_w_ = first.cols;
            img_h_ = first.rows;
        }
        std::cout << "[VideoReader] 已打开目录: " << source
                  << " 包含 " << image_files_.size() << " 张图片"
                  << "  " << width() << "x" << height() << "\n";
        return true;
    }

    is_image_dir_ = false;
    // 优先尝试按文件名打开，若是纯数字则当摄像头索引
    bool isNum = !source.empty() &&
                 std::all_of(source.begin(), source.end(), ::isdigit);
    if (isNum) {
        cap_.open(std::stoi(source));
    } else {
        cap_.open(source);
    }

    if (!cap_.isOpened()) {
        std::cerr << "[VideoReader] 无法打开: " << source << "\n";
        return false;
    }
    std::cout << "[VideoReader] 已打开: " << source
              << "  " << width() << "x" << height()
              << " @" << fps() << "fps\n";
    return true;
}

bool VideoReader::read(cv::Mat& frame) {
    if (is_image_dir_) {
        if (image_files_.empty()) return false;
        if (image_index_ >= (int)image_files_.size()) {
            if (loop_) {
                image_index_ = 0;
            } else {
                return false;
            }
        }
        frame = cv::imread(image_files_[image_index_]);
        if (frame.empty()) return false;
        image_index_++;
        return true;
    }

    if (!cap_.isOpened()) return false;
    if (!cap_.read(frame) || frame.empty()) {
        if (loop_) {
            cap_.set(cv::CAP_PROP_POS_FRAMES, 0);
            if (!cap_.read(frame) || frame.empty()) return false;
        } else {
            return false;
        }
    }
    return true;
}

void VideoReader::release() {
    if (is_image_dir_) {
        image_files_.clear();
        image_index_ = 0;
    } else {
        cap_.release();
    }
}

bool VideoReader::isOpened() const {
    if (is_image_dir_) return !image_files_.empty();
    return cap_.isOpened();
}

int VideoReader::width()  const {
    if (is_image_dir_) return img_w_;
    return (int)cap_.get(cv::CAP_PROP_FRAME_WIDTH);
}
int VideoReader::height() const {
    if (is_image_dir_) return img_h_;
    return (int)cap_.get(cv::CAP_PROP_FRAME_HEIGHT);
}
double VideoReader::fps() const {
    if (is_image_dir_) return 25.0; // 图片模式下假定默认 fps
    return cap_.get(cv::CAP_PROP_FPS);
}
