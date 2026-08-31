#include <iostream>
#include "HighThroughputServer.h"

int main()
{
    // Start a high-throughput REST server on port 8080 with 8 worker threads
	// EV: port and threads should be configurable but for this example hardcoded is fine
    // EV: this server has a problem: it works only the first time, the next request always fails :-(
    HighThroughputServer server(8080, 8);
    if (!server.Start()) {
        std::cerr << "Failed to start HighThroughputServer" << std::endl;
        return 1;
    }

    std::cout << "HighThroughputServer running on http://localhost:8080" << std::endl;
    std::cout << "Press Enter to stop..." << std::endl;
    std::cin.get();

    server.Stop();
    return 0;
}
