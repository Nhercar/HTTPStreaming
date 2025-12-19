#include "frame_encoder.h"


#include <fstream>

bool FrameEncoder::readJpegFromDisk(const std::string& filename, std::vector<uint8_t>& buffer) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) return false;
    buffer.assign(std::istreambuf_iterator<char>(file), {});
    return !buffer.empty();
}

FrameEncoder::FrameEncoder(int jpegQuality) 
    : jpegQuality_(jpegQuality) {
}

bool FrameEncoder::encode(const cv::Mat& frame, std::vector<uint8_t>& buffer) {
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, jpegQuality_};
    std::vector<uchar> tempBuffer;
    
    if (!cv::imencode(".jpg", frame, tempBuffer, params)) {
        return false;
    }
    
    buffer.assign(tempBuffer.begin(), tempBuffer.end());
    return true;
}

void FrameEncoder::setQuality(int quality) {
    jpegQuality_ = quality;
}


