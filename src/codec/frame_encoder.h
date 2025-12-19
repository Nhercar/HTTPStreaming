#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <cstdint>

class FrameEncoder {
public:
    explicit FrameEncoder(int jpegQuality = 80);
    bool encode(const cv::Mat& frame, std::vector<uint8_t>& buffer);
    void setQuality(int quality);
    bool readJpegFromDisk(const std::string& filename, std::vector<uint8_t>& buffer);
    
private:
    int jpegQuality_;
};
