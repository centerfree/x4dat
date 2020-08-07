#include "uwbserver.hpp"
#include <string.h>
#include <chrono>
#include <thread>

int main(int argc, char const *argv[])
{
    /* code */
    const int pause_ms = 10;
    const std::string addr = "127.0.0.1";
    const int port = 5005;
    uwbserver::udp_client client(addr, port);
    char message[128];
    int counter = 0;
    while (true)
    {
        sprintf(message, "%.8dcccccccccccccccc", counter);
        client.send(message, sizeof(char) * strlen(message));
        std::this_thread::sleep_for(std::chrono::milliseconds(pause_ms));
        fprintf(stdout, "Counter: %d | Message: %s\n", counter, message);
        counter++;
    }
    
    return 0;
}
