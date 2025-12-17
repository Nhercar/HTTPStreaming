#pragma once
#include <opencv2/opencv.hpp>

class Webcam {
public:
    explicit Webcam(int deviceIndex = 0);
    ~Webcam();
    
    bool isOpen() const;
    bool read(cv::Mat& frame);
    void setResolution(int width, int height);
    void setFPS(int fps);
    
private:
    cv::VideoCapture cap_;
    int deviceIndex_;
};
