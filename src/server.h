#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

class Server {
public:
    Server(int port);
    ~Server();
    void start();

private:
    int port;
    SOCKET server_fd;
    
    void handle_client(SOCKET client_socket);
    std::string perform_handshake(const std::string& request);
    std::string extract_header(const std::string& request, const std::string& key);
};

#endif
