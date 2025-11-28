#include "server.h"
#include "utils/sha1.h"
#include "utils/base64.h"
#include <iostream>
#include <vector>
#include <sstream>

Server::Server(int port) : port(port), server_fd(INVALID_SOCKET) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

Server::~Server() {
    closesocket(server_fd);
    WSACleanup();
}

void Server::start() {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "🔥 Socket failed" << std::endl;
        return;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "🔥 Bind failed" << std::endl;
        return;
    }

    if (listen(server_fd, 3) == SOCKET_ERROR) {
        std::cerr << "🔥 Listen failed" << std::endl;
        return;
    }

    std::cout << "🚀 ApexStream DB Listening on port " << port << std::endl;

    while (true) {
        SOCKET client_socket = accept(server_fd, nullptr, nullptr);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "🔥 Accept failed" << std::endl;
            continue;
        }
        std::cout << "🔌 Client connected" << std::endl;
        handle_client(client_socket);
    }
}

std::string Server::extract_header(const std::string& request, const std::string& key) {
    size_t pos = request.find(key);
    if (pos == std::string::npos) return "";
    pos += key.length() + 2; // ": "
    size_t end = request.find("\r\n", pos);
    return request.substr(pos, end - pos);
}

std::string Server::perform_handshake(const std::string& request) {
    std::string key = extract_header(request, "Sec-WebSocket-Key");
    if (key.empty()) return "";

    std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = key + magic;

    SHA1 sha1;
    sha1.update(combined);
    std::string hash = sha1.final_raw();
    std::string accept_key = base64_encode(hash);

    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << accept_key << "\r\n\r\n";
    
    return response.str();
}

void Server::handle_client(SOCKET client_socket) {
    char buffer[1024];
    int bytes_received = recv(client_socket, buffer, 1024, 0);
    if (bytes_received <= 0) {
        closesocket(client_socket);
        return;
    }

    std::string request(buffer, bytes_received);
    std::string response = perform_handshake(request);
    
    if (response.empty()) {
        std::cerr << "🔥 Handshake failed" << std::endl;
        closesocket(client_socket);
        return;
    }

    send(client_socket, response.c_str(), response.length(), 0);
    std::cout << "🤝 Handshake success" << std::endl;

   
    while (true) {
        char frame_header[2];
        int header_bytes = recv(client_socket, frame_header, 2, 0);
        if (header_bytes <= 0) break;

        bool fin = (frame_header[0] & 0x80) != 0;
        int opcode = frame_header[0] & 0x0F;
        bool masked = (frame_header[1] & 0x80) != 0;
        uint64_t payload_len = frame_header[1] & 0x7F;

        if (payload_len == 126) {
            char len_buf[2];
            recv(client_socket, len_buf, 2, 0);
            payload_len = (len_buf[0] << 8) | len_buf[1];
        } else if (payload_len == 127) {
            char len_buf[8];
            recv(client_socket, len_buf, 8, 0);
           
        }

        char mask_key[4];
        if (masked) {
            recv(client_socket, mask_key, 4, 0);
        }

        std::vector<char> payload(payload_len);
        recv(client_socket, payload.data(), payload_len, 0);

        if (masked) {
            for (size_t i = 0; i < payload_len; i++) {
                payload[i] ^= mask_key[i % 4];
            }
        }

        if (opcode == 8) { // Close
            std::cout << "👋 Client disconnected" << std::endl;
            break;
        }

        std::cout << "📩 Received: " << std::string(payload.begin(), payload.end()) << std::endl;
    }

    closesocket(client_socket);
}
