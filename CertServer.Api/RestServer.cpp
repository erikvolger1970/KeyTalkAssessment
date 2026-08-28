// Minimal HTTP REST server using Winsock. This is intended as an example only.
// Supports GET /api/health and GET/POST /api/echo

#include "RestServer.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>
#include <sstream>
#include <iostream>
#include <map>
#include <cstdio>

#pragma comment(lib, "Ws2_32.lib")

static std::string UrlDecode(const std::string& src) {
	std::string ret;
	char ch;
	int i, ii;
	for (i=0; i<src.length(); i++) {
		if (int(src[i])=='%') {
			unsigned int tmp = 0;
			sscanf_s(src.substr(i+1,2).c_str(), "%x", &tmp);
			ii = (int)tmp;
			ch=static_cast<char>(ii);
			ret+=ch;
			i=i+2;
		} else if (src[i]=='+') {
			ret+=' ';
		} else {
			ret+=src[i];
		}
	}
	return ret;
}

static std::map<std::string,std::string> ParseQuery(const std::string& qs) {
	std::map<std::string,std::string> m;
	std::istringstream iss(qs);
	std::string token;
	while (std::getline(iss, token, '&')) {
		auto pos = token.find('=');
		if (pos!=std::string::npos) {
			std::string k = token.substr(0,pos);
			std::string v = token.substr(pos+1);
			m[k] = UrlDecode(v);
		}
	}
	return m;
}

RestServer::RestServer()
	: running_(false), port_(0)
{
}

RestServer::~RestServer() {
	Stop();
}

bool RestServer::Start(unsigned short port) {
	if (running_) return false;
	port_ = port;

	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) {
		std::cerr << "WSAStartup failed: " << iResult << std::endl;
		return false;
	}

	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSocket == INVALID_SOCKET) {
		std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return false;
	}

	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = htonl(INADDR_ANY);
	service.sin_port = htons(port_);

	if (bind(listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
		std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
		closesocket(listenSocket);
		WSACleanup();
		return false;
	}

	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
		closesocket(listenSocket);
		WSACleanup();
		return false;
	}

	running_ = true;

	std::thread([this, listenSocket]() {
		while (running_) {
			SOCKET client = accept(listenSocket, NULL, NULL);
			if (client == INVALID_SOCKET) {
				if (!running_) break;
				std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
				continue;
			}

			// handle client in detached thread
			std::thread([client, this]() {
				const int bufSize = 8192;
				std::vector<char> buffer(bufSize);
				int received = recv(client, buffer.data(), bufSize, 0);
				if (received <= 0) {
					closesocket(client);
					return;
				}

				std::string req(buffer.data(), received);
				// simple parse: request-line and headers
				std::istringstream reqstream(req);
				std::string requestLine;
				std::getline(reqstream, requestLine);
				if (!requestLine.empty() && requestLine.back()=='\r') requestLine.pop_back();

				std::string method, uri, version;
				std::istringstream rl(requestLine);
				rl >> method >> uri >> version;

				// read headers
				std::map<std::string,std::string> headers;
				std::string line;
				while (std::getline(reqstream, line) && line != "\r" && !line.empty()) {
					if (line.back()=='\r') line.pop_back();
					auto pos = line.find(":");
					if (pos!=std::string::npos) {
						std::string key = line.substr(0,pos);
						std::string val = line.substr(pos+1);
						// trim
						while(!val.empty() && val.front()==' ') val.erase(val.begin());
						headers[key] = val;
					}
				}

				std::string body;
				auto it = headers.find("Content-Length");
				if (it != headers.end()) {
					int contentLength = atoi(it->second.c_str());
					if (contentLength > 0) {
						body.resize(contentLength);
						reqstream.read(&body[0], contentLength);
						// if not all read, try to receive more
						int have = (int)reqstream.gcount();
						while (have < contentLength) {
							int more = recv(client, &body[have], contentLength - have, 0);
							if (more <= 0) break;
							have += more;
						}
					}
				}

				std::string responseBody;
				std::string status = "200 OK";
				std::string contentType = "application/json";

				// route handling
				if (method == "GET" && uri.rfind("/api/health",0)==0) {
					responseBody = "{ \"status\": \"ok\" }";
				} else if (method == "GET" && uri.rfind("/api/echo",0)==0) {
					// parse query
					auto qpos = uri.find('?');
					std::string qs;
					if (qpos != std::string::npos) qs = uri.substr(qpos+1);
					auto params = ParseQuery(qs);
					std::string msg = "";
					auto mit = params.find("msg");
					if (mit!=params.end()) msg = mit->second;
					responseBody = "{ \"echo\": \"" + msg + "\" }";
				} else if (method == "POST" && uri.rfind("/api/echo",0)==0) {
					// return received body as 'echo'
					// assume body is plain text
					std::string esc;
					for (char c: body) {
						if (c=='\"') esc += "\\\"";
						else if (c=='\\') esc += "\\\\";
						else esc += c;
					}
					responseBody = "{ \"echo\": \"" + esc + "\" }";
					contentType = "application/json";
				} else {
					status = "404 Not Found";
					responseBody = "{ \"error\": \"not_found\" }";
				}

				// build response
				std::ostringstream resp;
				resp << "HTTP/1.1 " << status << "\r\n";
				resp << "Content-Type: " << contentType << "; charset=utf-8\r\n";
				resp << "Content-Length: " << responseBody.size() << "\r\n";
				resp << "Connection: close\r\n";
				resp << "\r\n";
				resp << responseBody;

				std::string respStr = resp.str();
				send(client, respStr.c_str(), (int)respStr.size(), 0);
				closesocket(client);
			}).detach();
		}

		closesocket(listenSocket);
		WSACleanup();
	}).detach();

	return true;
}

void RestServer::Stop() {
	if (!running_) return;
	running_ = false;
	// connecting to the listening port to unblock accept is a common technique,
	// but here we leave it since process exit will clean up sockets.
	// For a production server, implement a proper shutdown wakeup.
}
