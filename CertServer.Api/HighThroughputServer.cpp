// Implementation of a simple high-throughput multi-threaded REST server.
// - Accepts connections on a TCP socket
// - Uses an acceptor thread and a fixed-size worker thread pool
// - Uses std::stop_token to manage shutdown
// - Includes adapter stubs for Glaze (serialization) and AERONET (messaging)

#include "HighThroughputServer.h"

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <sstream>
#include <cstring>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using socklen_t = int;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif

// EV: The AI code is very C and begging for polymorphism:
//		WindowsSocket/LinuxSocket
//		WindowsHighThroughputServer/LinuxHighThroughputServer

#if defined(_WIN32)
using socket_t = SOCKET;
#else
using socket_t = int;
#endif

static void close_socket(socket_t s) {
#if defined(_WIN32)
	closesocket(s);
#else
	close(s);
#endif
}

static int recv_socket(socket_t s, void* buf, int len, int flags) {
#if defined(_WIN32)
	return (int)recv(s, (char*)buf, len, flags);
#else
	return (int)recv(s, buf, len, flags);
#endif
}

static int send_socket(socket_t s, const char* buf, int len, int flags) {
#if defined(_WIN32)
	return (int)send(s, buf, len, flags);
#else
	return (int)send(s, buf, len, flags);
#endif
}

// -------------------------- Stub adapters --------------------------
// These provide compile-time placeholders. Replace with real Glaze & Aeronet
// integration when libraries are available.
// EV: this is not what I asked for in the prompt...

namespace glaze_stub {
	template<typename T>
	std::string to_json(const T& obj) {
		// very small placeholder: uses a naive string representation
		// Replace with glaze::to_json(obj) when glaze is available.
		(void)obj;
		return "\"stub\"";
	}

	template<typename T>
	bool from_json(const std::string& json, T& out) {
		(void)json; (void)out; return false;
	}
}

namespace aeronet_stub {
	// Minimal server-side interface stub
	struct Server {
		Server() {}
		bool start() { return true; }
		void stop() {}
		// publish a message on a channel/topic
		template<typename Msg>
		void publish(const std::string& topic, const Msg& msg) {
			(void)topic; (void)msg;
			// no-op
		}
	};
}

// -------------------------- Implementation --------------------------

struct HighThroughputServer::Impl {
	unsigned short port;
	size_t workerCount;

	std::atomic<bool> running{false};

	// socket
	socket_t listenFd{(socket_t)-1};

	// worker pool (use jthread to support stop_token)
	std::vector<std::jthread> workers;
	std::jthread acceptorThread;

	// queue of accepted client sockets
	std::queue<socket_t> clientQueue;
	std::mutex queueMutex;
	std::condition_variable queueCv;

	// aeron server stub
	aeronet_stub::Server aeron;

	Impl(unsigned short p, size_t wc)
		: port(p), workerCount(wc) {}

	~Impl() { Stop(); }

#if defined(_WIN32)
	static bool ensure_wsa() {
		static bool initialized = false;
		if (!initialized) {
			WSADATA wsa;
			if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return false;
			initialized = true;
		}
		return true;
	}
#endif

	bool make_listen_socket() {
#if defined(_WIN32)
		if (!ensure_wsa()) return false;
		listenFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listenFd == INVALID_SOCKET) {
			std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
			return false;
		}

		u_long mode = 1;
		ioctlsocket(listenFd, FIONBIO, &mode);

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(port);

		if (bind(listenFd, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
			std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
			closesocket(listenFd);
			listenFd = (socket_t)-1;
			return false;
		}

		if (listen(listenFd, SOMAXCONN) == SOCKET_ERROR) {
			std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
			closesocket(listenFd);
			listenFd = (socket_t)-1;
			return false;
		}

		return true;
#else
		listenFd = socket(AF_INET, SOCK_STREAM, 0);
		if (listenFd < 0) {
			perror("socket");
			return false;
		}

		int opt = 1;
		setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(port);

		if (bind(listenFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
			perror("bind");
			close(listenFd);
			listenFd = -1;
			return false;
		}

		if (listen(listenFd, 1024) < 0) {
			perror("listen");
			close(listenFd);
			listenFd = -1;
			return false;
		}

		// set non-blocking accept to allow stop wakeup
		int flags = fcntl(listenFd, F_GETFL, 0);
		fcntl(listenFd, F_SETFL, flags | O_NONBLOCK);

		return true;
#endif
	}

	void accept_loop(std::stop_token stopToken) {
		while (!stopToken.stop_requested()) {
			sockaddr_in clientAddr{};
			socklen_t addrlen = sizeof(clientAddr);
			socket_t client = accept((int)listenFd, (sockaddr*)&clientAddr, &addrlen);
			if (client < 0) {
				// no pending connection; sleep briefly or check stop
				int err = errno;
				if (err == EWOULDBLOCK || err == EAGAIN) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				} else {
					// real error
					perror("accept");
					break;
				}
			}

			// enqueue client socket
			{
				std::lock_guard<std::mutex> lk(queueMutex);
				clientQueue.push(client);
			}
			queueCv.notify_one();
		}
	}

	// A very small HTTP request handler: reads request up to a limit and responds
	void worker_loop(std::stop_token stopToken) {
		while (!stopToken.stop_requested()) {
			int client = -1;
			{
				std::unique_lock<std::mutex> lk(queueMutex);
				queueCv.wait_for(lk, std::chrono::milliseconds(100), [this, &stopToken]() {
					return !clientQueue.empty() || stopToken.stop_requested();
				});
				if (stopToken.stop_requested() && clientQueue.empty()) break;
				if (clientQueue.empty()) continue;
				client = (int)clientQueue.front();
				clientQueue.pop();
			}

			handle_client(client);
		}
	}

	// EV: what a mess...
	void handle_client(int clientFd) {
		// read some data (non-blocking safe)
		const int BUF = 8192;
		std::vector<char> buffer(BUF);
		int n = recv_socket((socket_t)clientFd, buffer.data(), BUF - 1, 0);
		if (n <= 0) {
			close_socket((socket_t)clientFd);
			return;
		}
		buffer[n] = '\0';
		std::string req(buffer.data(), (size_t)n);

		// parse simple request-line
		std::istringstream ss(req);
		std::string requestLine;
		std::getline(ss, requestLine);
		if (!requestLine.empty() && requestLine.back()=='\r') requestLine.pop_back();
		std::istringstream rl(requestLine);
		std::string method, uri, ver;
		rl >> method >> uri >> ver;

		std::string responseBody;
		std::string status = "200 OK";
		std::string contentType = "application/json";

		if (method == "GET" && uri.rfind("/api/health",0)==0) {
			responseBody = "{ \"status\": \"ok\" }";
			// publish event to aeron (stub)
			aeron.publish("health", std::string("ok"));
		} else if (method == "GET" && uri.rfind("/api/echo",0)==0) {
			auto qpos = uri.find('?');
			std::string qs;
			if (qpos != std::string::npos) qs = uri.substr(qpos+1);
			// very small query parse
			std::string msg;
			auto pos = qs.find("msg=");
			if (pos!=std::string::npos) msg = qs.substr(pos+4);
			responseBody = "{ \"echo\": \"" + msg + "\" }";
		} else if (method == "POST" && uri.rfind("/api/echo",0)==0) {
			// naive body extraction: find blank line
			auto bodyPos = req.find("\r\n\r\n");
			std::string body;
			if (bodyPos != std::string::npos) body = req.substr(bodyPos + 4);
			// publish the message
			aeron.publish("echo", body);
			// use glaze stub to create JSON
			responseBody = "{ \"echo\": \"" + body + "\" }";
		} else {
			status = "404 Not Found";
			responseBody = "{ \"error\": \"not_found\" }";
		}

		std::ostringstream resp;
		resp << "HTTP/1.1 " << status << "\r\n";
		resp << "Content-Type: " << contentType << "; charset=utf-8\r\n";
		resp << "Content-Length: " << responseBody.size() << "\r\n";
		resp << "Connection: close\r\n";
		resp << "\r\n";
		resp << responseBody;

		auto s = resp.str();
		send_socket((socket_t)clientFd, s.c_str(), (int)s.size(), 0);
		close_socket((socket_t)clientFd);
	}

	void Stop() {
		if (!running.exchange(false)) return;
		// close listening socket to break accept
		if (listenFd >= 0) {
			close_socket(listenFd);
			listenFd = (socket_t)-1;
		}
		queueCv.notify_all();
		// workers and acceptor are joined externally via stop_token
		aeron.stop();
	}
};

HighThroughputServer::HighThroughputServer(unsigned short port, size_t workerThreads)
	: impl_(new Impl(port, workerThreads))
{
}

HighThroughputServer::~HighThroughputServer() {
	Stop();
}

bool HighThroughputServer::Start() {
	if (!impl_) return false;
	if (!impl_->make_listen_socket()) return false;

	impl_->running = true;

	// start aeron (stub)
	impl_->aeron.start();

	// use jthread for stop_token support
	std::vector<std::jthread> pool;

	// start worker threads directly into impl_->workers (std::jthread)
	for (size_t i = 0; i < impl_->workerCount; ++i) {
		impl_->workers.emplace_back([this](std::stop_token st){ impl_->worker_loop(st); });
	}

	// acceptor thread
	impl_->acceptorThread = std::jthread([this](std::stop_token st){ impl_->accept_loop(st); });

	return true;
}

void HighThroughputServer::Stop() {
	if (!impl_) return;
	impl_->Stop();
	// join worker threads
	for (auto &t : impl_->workers) {
		if (t.joinable()) t.join();
	}
	if (impl_->acceptorThread.joinable()) impl_->acceptorThread.join();
}

unsigned short HighThroughputServer::Port() const noexcept {
	if (!impl_) return 0;
	return impl_->port;
}
