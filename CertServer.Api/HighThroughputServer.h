// HighThroughputServer - multi-threaded, high-throughput REST server example
// Uses std::stop_token for lifecycle control. Integration points for Glaze and AERONET
// are provided as stubs; replace with real library calls when libraries are available.
#pragma once

#include <string>
#include <functional>
#include <memory>

class HighThroughputServer {
public:
	// Create server with specified port and worker thread count
	HighThroughputServer(unsigned short port = 8080, size_t workerThreads = 4);
	~HighThroughputServer();

	// Start server: returns true if listening started successfully
	bool Start();

	// Stop server via stop request (uses std::stop_token internally from jthread)
	void Stop();

	// Returns the listening port (useful if 0 was passed to auto-select)
	unsigned short Port() const noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
