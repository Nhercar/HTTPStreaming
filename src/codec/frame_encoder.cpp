#include "frame_encoder.h"

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
