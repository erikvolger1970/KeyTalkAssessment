// Simple REST server example using Winsock (Windows only)
#pragma once

#include <string>
#include <atomic>

class RestServer {
public:
	RestServer();
	~RestServer();

	// Start the server on the given port. Returns true if started successfully.
	bool Start(unsigned short port);

	// Stop the server and join background threads.
	void Stop();

private:
	std::atomic<bool> running_;
	unsigned short port_;
};
