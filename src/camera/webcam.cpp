#include "webcam.h"
#include <iostream>

Webcam::Webcam(int deviceIndex) 
    : cap_(deviceIndex), deviceIndex_(deviceIndex) {
}

Webcam::~Webcam() {
    if (cap_.isOpened()) {
        cap_.release();
    }
}

bool Webcam::isOpen() const {
    return cap_.isOpened();
}

bool Webcam::read(cv::Mat& frame) {
    return cap_.read(frame);
}

void Webcam::setResolution(int width, int height) {
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height);
}

void Webcam::setFPS(int fps) {
    cap_.set(cv::CAP_PROP_FPS, fps);
}
