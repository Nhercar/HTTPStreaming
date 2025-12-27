#include "linux_server.h"
#include "../logger.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sstream>
#include <chrono>
#include "../streaming/stateless_handlers.h"

LinuxSocketServer::LinuxSocketServer(int port)
	: port_(port), serverSocket_(-1) {}

LinuxSocketServer::~LinuxSocketServer() {
	stop();
}

bool LinuxSocketServer::start() {
	serverSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket_ == INVALID_SOCKET_T) {
		Logger::getInstance().error(std::string("socket() failed: ") + std::strerror(errno));
		return false;
	}

	int opt = 1;
	if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		Logger::getInstance().error(std::string("setsockopt(SO_REUSEADDR) failed: ") + std::strerror(errno));
		closeSocket(serverSocket_);
		serverSocket_ = INVALID_SOCKET_T;
		return false;
	}

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(static_cast<u_short>(port_));
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(serverSocket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR_T) {
		Logger::getInstance().error(std::string("bind() failed: ") + std::strerror(errno));
		closeSocket(serverSocket_);
		serverSocket_ = INVALID_SOCKET_T;
		return false;
	}

	if (listen(serverSocket_, SOMAXCONN) == SOCKET_ERROR_T) {
		Logger::getInstance().error(std::string("listen() failed: ") + std::strerror(errno));
		closeSocket(serverSocket_);
		serverSocket_ = INVALID_SOCKET_T;
		return false;
	}

	startMonitor();
	running_.store(true);
	Logger::getInstance().info("LinuxSocketServer listening on port " + std::to_string(port_));
	return true;
}

void LinuxSocketServer::run(RequestHandlerStop handler) {
	while (running_) {
		cleanupFinishedThreads();

		sockaddr_in clientAddr{};
		socklen_t addrLen = sizeof(clientAddr);
		socket_t client = accept(serverSocket_, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
		if (client == INVALID_SOCKET_T) {
			if (running_) Logger::getInstance().error(std::string("accept() failed: ") + std::strerror(errno));
			continue;
		}

		if (getActiveClients() >= MAX_CLIENTS) {
			MaxClientsReached maxClientesHandler;
			maxClientesHandler.handle(client, "", std::stop_token{});
			continue;
		}

		auto finished = std::make_shared<std::atomic<bool>>(false);
		std::jthread th([this, client, handler, finished](std::stop_token st){
			handleClient(st, client, handler);
			finished->store(true, std::memory_order_relaxed);
		});

		{
			std::lock_guard<std::mutex> lg(clientsMutex_);
			clients_.emplace(client, ClientRecord{ std::move(th), finished });
		}
	}
}

void LinuxSocketServer::stop() {
	if (!running_) return;
	running_.store(false);
	if (serverSocket_ != INVALID_SOCKET_T) {
		closeSocket(serverSocket_);
		serverSocket_ = INVALID_SOCKET_T;
	}
	stopMonitor();
	shutdownAllClients();
	cleanupFinishedThreads();
	joinAllThreads();
	Logger::getInstance().info("LinuxSocketServer stopped");
}

size_t LinuxSocketServer::getActiveClients() {
	std::scoped_lock lk(clientsMutex_);
	return clients_.size();
}

void LinuxSocketServer::cleanupFinishedThreads() {
	std::vector<int> toErase;
	std::vector<std::jthread> toJoin;
	{
		std::lock_guard<std::mutex> lock(clientsMutex_);
		for (auto &kv : clients_) {
			if (kv.second.finished && kv.second.finished->load(std::memory_order_relaxed)) {
				toErase.push_back(kv.first);
			}
		}
		for (int s : toErase) {
			auto it = clients_.find(s);
			if (it != clients_.end()) {
				toJoin.push_back(std::move(it->second.thread));
				clients_.erase(it);
			}
		}
	}
	// Al destruirse los jthread, se sincroniza con los hilos ya finalizados
}

void LinuxSocketServer::joinAllThreads() {
	std::vector<std::jthread> toJoin;
	{
		std::lock_guard<std::mutex> lock(clientsMutex_);
		toJoin.reserve(clients_.size());
		for (auto &kv : clients_) {
			toJoin.push_back(std::move(kv.second.thread));
		}
		clients_.clear();
	}
	// Al salir del scope, los jthread se unen automáticamente (RAII)
}

void LinuxSocketServer::shutdownClient(int clientSocket) {
	std::lock_guard<std::mutex> lock(clientsMutex_);
	auto it = clients_.find(clientSocket);
	if (it != clients_.end()) {
		it->second.thread.request_stop();
		shutdown(clientSocket, SHUT_RDWR);
		closeSocket(clientSocket);
		Logger::getInstance().info(std::string("Cliente ") + std::to_string((uintptr_t)clientSocket) + " marcado para cierre");
	}
}

void LinuxSocketServer::shutdownAllClients() {
	size_t count = 0;
	{
		std::lock_guard<std::mutex> lock(clientsMutex_);
		for (auto &kv : clients_) {
			socket_t sock = kv.first;
			kv.second.thread.request_stop();
			shutdown(sock, SHUT_RDWR);
			closeSocket(sock);
			++count;
		}
	}
	Logger::getInstance().info("Cerradas " + std::to_string(count) + " conexiones activas");
}

void LinuxSocketServer::handleClient(std::stop_token st, int clientSocket, const RequestHandlerStop& handler) {
	bool socketClosed = false;
	char buffer[4096];
	if (st.stop_requested()) {
		shutdown(clientSocket, SHUT_RDWR);
		closeSocket(clientSocket);
		socketClosed = true;
		return;
	}

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(clientSocket, &readfds);

	timeval timeout;
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;

	int sel = select(clientSocket + 1, &readfds, nullptr, nullptr, &timeout);
	if (sel > 0 && FD_ISSET(clientSocket, &readfds)) {
		ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
		Logger::getInstance().info("handleClient: bytesReceived = " + std::to_string(bytesReceived) + " en socket " + std::to_string(clientSocket));
		if (bytesReceived > 0) {
			buffer[bytesReceived] = '\0';
			std::string request(buffer, bytesReceived);
			handler(clientSocket, request, st);
		} else if (bytesReceived == 0) {
			Logger::getInstance().info("Cliente cerro la conexion sin enviar datos");
		} else {
			Logger::getInstance().error(std::string("Error en recv: ") + std::strerror(errno));
		}
	} else if (sel == 0) {
		Logger::getInstance().info("Timeout esperando datos del cliente en socket " + std::to_string(clientSocket));
	} else {
		Logger::getInstance().error(std::string("Error en select: ") + std::strerror(errno) + " en socket " + std::to_string(clientSocket));
	}

	if (!socketClosed) {
		closeSocket(clientSocket);
		Logger::getInstance().info("Conexion cerrada en " + std::to_string(clientSocket));
	}
}

void LinuxSocketServer::startMonitor() {
	monitorThread_ = std::jthread([this](std::stop_token st){
		using namespace std::chrono_literals;
		while (!st.stop_requested()) {
			cleanupFinishedThreads();
			std::this_thread::sleep_for(1s);
			Logger::getInstance().debug("Hay " + std::to_string(getActiveClients()) + " Clientes conectados");
		}
	});
}

void LinuxSocketServer::stopMonitor() {
	if (monitorThread_.joinable()) monitorThread_.request_stop();
}
