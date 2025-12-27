
#pragma once
#include "../ServerInterface/IServer.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <stop_token>
#include <thread>
#include <atomic>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

class LinuxSocketServer : public IServer {
public:
	explicit LinuxSocketServer(int port);
	~LinuxSocketServer() override;

	bool start() override;
	void run(RequestHandlerStop handler) override;
	void stop() override;
	size_t getActiveClients() override;

private:
	struct ClientRecord {
		std::jthread thread;
		std::shared_ptr<std::atomic<bool>> finished;
	};

	int port_;
	int serverSocket_;
	std::atomic<bool> running_{false};

	std::unordered_map<int, ClientRecord> clients_;
	std::mutex clientsMutex_;

	std::jthread monitorThread_;

	static constexpr size_t MAX_CLIENTS = 5;

	void cleanupFinishedThreads();
	void joinAllThreads();
	void shutdownClient(int clientSocket);
	void shutdownAllClients();
	void handleClient(std::stop_token st, int clientSocket, const RequestHandlerStop& handler);
	void startMonitor();
	void stopMonitor();
};
