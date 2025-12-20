#pragma once

#include <stop_token>
#include "../HTTPServer/socket_types.h"
#include "../HTTPServer/handler_interface.h"
#include <string>
#include <chrono>
#include <thread>
#include <sstream>
#include <memory>
#include <atomic>
#include <unordered_map>

#include "logger.h"
#include "http_parser.h"
#include "http_response.h"
#include "../camera/webcam.h"
#include "../codec/frame_encoder.h"

void homeHandler(socket_t socket, const std::string&, std::stop_token st);

void notFoundHandler(socket_t socket, const std::string&, std::stop_token st);

class MJPEGHandler : public IHandler {
public:
	explicit MJPEGHandler(int deviceIndex = 0);
	void operator()(socket_t socket, const std::string& rawRequest, std::stop_token st) override;
	void stopAll() override;
	std::size_t getClientCount() override;

private:
	std::shared_ptr<Webcam> webcam_;
	FrameEncoder encoder_ { 80 };
	std::mutex clientsMutex_;
	// device index for lazy webcam open
	int deviceIndex_ {0};
	// number of active clients streaming
	std::atomic<size_t> activeClients_{0};
	void cleanupFinishedThreads();
};


