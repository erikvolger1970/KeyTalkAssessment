#include <iostream>
#include "RestServer.h"

using namespace std;

int main()
{
    // Start a simple REST API server on port 8080
    RestServer server;
    if (!server.Start(8080)) {
        cerr << "Failed to start REST server" << endl;
        return 1;
    }

    cout << "REST server running on http://localhost:8080" << endl;
    cout << "Press Enter to stop..." << endl;
    cin.get();

    server.Stop();
    return 0;
}
