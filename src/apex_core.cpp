#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <string>
#include <sstream>
#include <winsock2.h>
#include <windows.h>
#include <immintrin.h> // AVX2
#include <chrono>

#include "utils/sha1.h"
#include "utils/base64.h"

#pragma comment(lib, "ws2_32.lib")

#define TCP_PORT 9000
#define WS_PORT 8080
#define BUFFER_SIZE 65536
#define MAGIC_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

struct GlobalState {
    std::atomic<long long> total_lines{0};
    std::atomic<double> current_temp{0.0};
    std::atomic<double> temp_sum{0.0};
    std::atomic<long> temp_count{0};
};

GlobalState state;

struct RingBuffer {
    char data[1024 * 1024 * 10]; 
    std::atomic<size_t> head{0};
    std::atomic<size_t> tail{0};
    const size_t size = 1024 * 1024 * 10;

    void push(const char* src, size_t len) {
        size_t current_head = head.load(std::memory_order_relaxed);
        for (size_t i = 0; i < len; ++i) {
            data[(current_head + i) % size] = src[i];
        }
        head.store(current_head + len, std::memory_order_release);
    }
};

RingBuffer disk_buffer;
std::atomic<bool> running{true};

void archiver_thread() {
    FILE* f = fopen("apex_blackbox.log", "wb");
    if (!f) {
        std::cerr << "❌ Failed to open log file!" << std::endl;
        return;
    }
    char local_buf[4096]; 
    
    std::cout << "💾 ARCHIVER ONLINE: GRAVANDO LOG BINÁRIO..." << std::endl;

    while (running) {
        size_t h = disk_buffer.head.load(std::memory_order_acquire);
        size_t t = disk_buffer.tail.load(std::memory_order_relaxed);

        if (h == t) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        size_t bytes_to_write = h - t;
        if (bytes_to_write > 4096) bytes_to_write = 4096;

        for (size_t i = 0; i < bytes_to_write; ++i) {
            local_buf[i] = disk_buffer.data[(t + i) % disk_buffer.size];
        }

        fwrite(local_buf, 1, bytes_to_write, f);
        
        disk_buffer.tail.store(t + bytes_to_write, std::memory_order_release);
    }
    fclose(f);
}

void fast_process_chunk(char* buffer, int bytes_read) {
    int lines_in_chunk = 0;
    double local_sum = 0;
    int local_count = 0;
    
    char* cursor = buffer;
    char* end = buffer + bytes_read;
    
    while(cursor < end) {
        char* next_line = (char*)memchr(cursor, '\n', end - cursor);
        if (!next_line) break; // Fim do buffer parcial

       
        char* val_end = next_line;
        char* val_start = val_end - 1;
        while (*val_start != ',' && val_start > cursor) val_start--;
        
        if (*val_start == ',') {
            double val = std::strtod(val_start + 1, nullptr);
            local_sum += val;
            local_count++;
            state.current_temp.store(val, std::memory_order_relaxed);
        }

        lines_in_chunk++;
        cursor = next_line + 1;
    }

   
    state.total_lines.fetch_add(lines_in_chunk, std::memory_order_relaxed);
    
  
    if (local_count > 0) {
        double old_sum = state.temp_sum.load();
        while(!state.temp_sum.compare_exchange_weak(old_sum, old_sum + local_sum));
        state.temp_count.fetch_add(local_count);
    }
}


void ingestor_thread() {
    SOCKET server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(TCP_PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "🔥 Bind failed on port " << TCP_PORT << std::endl;
        return;
    }
    listen(server_fd, 1);

    std::cout << "🔥 INGESTOR TCP PRONTO NA PORTA " << TCP_PORT << std::endl;

    while (true) {
        client_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        std::cout << "🔌 SENSOR CONECTADO!" << std::endl;
        
        while (true) {
            int valread = recv(client_fd, buffer, BUFFER_SIZE, 0);
            if (valread <= 0) break;
            
            
            fast_process_chunk(buffer, valread);
            
          
            disk_buffer.push(buffer, valread);
        }
        closesocket(client_fd);
        std::cout << "🔌 SENSOR DESCONECTADO." << std::endl;
    }
}

void websocket_thread() {
    SOCKET server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(WS_PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "🔥 Bind failed on port " << WS_PORT << std::endl;
        return;
    }
    listen(server_fd, 3);

    std::cout << "🚀 WEBSOCKET PRONTO NA PORTA " << WS_PORT << std::endl;

    while (true) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        
        char buffer[2048];
        int bytes = recv(new_socket, buffer, 2048, 0);
        if (bytes <= 0) { closesocket(new_socket); continue; }

        std::string req(buffer, bytes);
        size_t pos = req.find("Sec-WebSocket-Key: ");
        if (pos == std::string::npos) { closesocket(new_socket); continue; }
        
        size_t key_start = pos + 19;
        size_t key_end = req.find("\r\n", key_start);
        if (key_end == std::string::npos) { closesocket(new_socket); continue; }
        
        std::string key = req.substr(key_start, key_end - key_start);
        std::string combined = key + MAGIC_GUID;
        
        SHA1 sha1;
        sha1.update(combined);
        std::string hash = sha1.final_raw();
        std::string accept_key = base64_encode(hash);

        std::string resp = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept_key + "\r\n\r\n";
        send(new_socket, resp.c_str(), resp.size(), 0);

        std::cout << "💀 CLIENTE WEB ONLINE." << std::endl;

        while (true) {
            Sleep(50); 
            long long lines = state.total_lines.load();
            double temp = state.current_temp.load();
            
            std::string msg = "{\"lines\": " + std::to_string(lines) + ", \"temp\": " + std::to_string(temp) + "}";
            
            unsigned char frame[128];
            frame[0] = 0x81;
            frame[1] = (unsigned char)msg.size();
            
            int sent1 = send(new_socket, (const char*)frame, 2, 0);
            int sent2 = send(new_socket, msg.c_str(), msg.size(), 0);
            
            if (sent1 <= 0 || sent2 <= 0) {
                std::cout << "👋 CLIENTE WEB DESCONECTADO." << std::endl;
                break;
            }
        }
        closesocket(new_socket);
    }
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    std::thread t1(ingestor_thread);
    std::thread t2(websocket_thread);
    std::thread t3(archiver_thread);

    t1.join();
    t2.join();
    t3.join();

    WSACleanup();
    return 0;
}
