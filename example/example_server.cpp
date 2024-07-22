#include "ipc_bus.h"
#include <iostream>
#include <msg.h>
#include <regex>
#include <signal.h>
#include <string>
#include <thread>
using namespace ipcbus;
std::atomic<bool> stop = false;
void handle_ipc_msg(const Person_t &person)
{
    std::cout << "Received person info: " << person.name << ", " << person.age << ", " << person.id << std::endl;
    Response_t response{0x99};
    IpcBus::instance().publish(response);
}
void handle_signal(int signal)
{
    std::cout << "Received signal: " << signal << std::endl;
    stop = true;
}
int main()
{
    ::signal(SIGINT, handle_signal);
    ::signal(SIGABRT, handle_signal);
    ::signal(SIGSEGV, handle_signal);
    ::signal(SIGTERM, handle_signal);
    IpcBus::instance().subscribe<Person_t>(handle_ipc_msg);
    IpcBus::instance().add_service("example");
    while (!stop)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << " receiver is running..." << std::endl;
    }
    std::cout << " sender is quit..." << std::endl;
    return 0;
}