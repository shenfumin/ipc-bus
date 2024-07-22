

#include "ipc_bus.h"
#include <chrono>
#include <iostream>
#include <msg.h>
#include <regex>
#include <signal.h>
#include <string>
#include <thread>
using namespace ipcbus;
std::atomic<bool> stop = false;
void handle_ipc_msg(const Response_t &reponse) { std::cout << "Received reponse result: " << reponse.result << std::endl; }
void handle_signal(int signal)
{
    std::cout << "Received signal: " << signal << std::endl;
    stop = true;
}
int main()
{
    Person_t person = {134, "Jack", 40};
    ::signal(SIGINT, handle_signal);
    ::signal(SIGABRT, handle_signal);
    ::signal(SIGSEGV, handle_signal);
    ::signal(SIGTERM, handle_signal);
    IpcBus::instance().subscribe<Response_t>(handle_ipc_msg);
    while (!stop)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << " sender is running..." << std::endl;
        IpcBus::instance().publish("example", person);
    }
    std::cout << " sender is quit..." << std::endl;
    return 0;
}