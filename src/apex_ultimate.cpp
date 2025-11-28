#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <string>
#include <winsock2.h>
#include <windows.h>
#include <chrono>


#include "utils/sha1.h"
#include "utils/base64.h"

#pragma comment(lib, "ws2_32.lib")

#define TCP_PORT 9000
#define WS_PORT 8080
#define MAGIC_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define BINARY_FILE "apex_blackbox.bin"


#pragma pack(push, 1)
struct DataPacket {
    long long id;        
    long long timestamp; 
    double value;        
};
#pragma pack(pop)

struct GlobalState {
    std::atomic<long long> total_lines{0};
    std::atomic<double> current_temp{0.0};
};
GlobalState state;


struct RingBuffer {
    DataPacket data[1024 * 1024]; 
    std::atomic<size_t> head{0};
    std::atomic<size_t> tail{0};
    const size_t size = 1024 * 1024;

    void push(DataPacket p) {
        size_t cur = head.load(std::memory_order_relaxed);
        data[cur % size] = p;
        head.store(cur + 1, std::memory_order_release);
    }
    
    bool pop(DataPacket& p) {
        size_t t = tail.load(std::memory_order_relaxed);
        size_t h = head.load(std::memory_order_acquire);
        if (h == t) return false;
        p = data[t % size];
        tail.store(t + 1, std::memory_order_release);
        return true;
    }
};
RingBuffer rb;
std::atomic<bool> running{true};


void parse_and_push(char* buffer, int len) {
    char* cursor = buffer;
    char* end = buffer + len;
    
    while(cursor < end) {
        char* line_end = (char*)memchr(cursor, '\n', end - cursor);
        if (!line_end) break;
        
        *line_end = '\0';
        
        
        char* p1 = strchr(cursor, ',');
        if(p1) {
            char* p2 = strchr(p1 + 1, ',');
            if(p2) {
                DataPacket pkt;
                pkt.id = _atoi64(cursor);
                pkt.timestamp = _atoi64(p1 + 1);
                pkt.value = strtod(p2 + 1, nullptr);
                
               
                rb.push(pkt);
                
               
                state.total_lines.fetch_add(1, std::memory_order_relaxed);
                state.current_temp.store(pkt.value, std::memory_order_relaxed);
            }
        }
        cursor = line_end + 1;
    }
}


void ingestor_thread() {
    SOCKET server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[65536];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(TCP_PORT);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "🔥 Bind failed on port " << TCP_PORT << std::endl;
        return;
    }
    listen(server_fd, 1);

    std::cout << "🔥 MODO AO VIVO: TCP PORTA " << TCP_PORT << std::endl;

    while (running) {
        client_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        while (true) {
            int valread = recv(client_fd, buffer, 65535, 0);
            if (valread <= 0) break;
            buffer[valread] = '\0'; // Safety
            parse_and_push(buffer, valread);
        }
        closesocket(client_fd);
    }
}


void replay_thread() {
    FILE* f = fopen(BINARY_FILE, "rb");
    if (!f) {
        std::cout << "❌ NÃO ACHEI O ARQUIVO BINÁRIO PRA REPLAY!" << std::endl;
        exit(1);
    }

    std::cout << "🎬 REPLAY MODE: INICIANDO STREAM DO ARQUIVO..." << std::endl;
    DataPacket pkt;
    long long count = 0;

    while (fread(&pkt, sizeof(DataPacket), 1, f) && running) {
        state.current_temp.store(pkt.value, std::memory_order_relaxed);
        state.total_lines.store(pkt.id, std::memory_order_relaxed);
        
        
        if (++count % 1000 == 0) std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    fclose(f);
    std::cout << "🏁 REPLAY FINALIZADO." << std::endl;
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

    std::cout << "🚀 DASHBOARD LINK: ws://localhost:" << WS_PORT << std::endl;

    while (running) {
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

        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // 60 FPS
            long long l = state.total_lines.load();
            double t = state.current_temp.load();
            std::string msg = "{\"lines\": " + std::to_string(l) + ", \"temp\": " + std::to_string(t) + "}";
            unsigned char frame[256];
            frame[0] = 0x81; frame[1] = (unsigned char)msg.size();
            send(new_socket, (const char*)frame, 2, 0);
            if (send(new_socket, msg.c_str(), msg.size(), 0) < 0) break;
        }
        closesocket(new_socket);
    }
}


void archiver_thread() {
    FILE* f = fopen(BINARY_FILE, "ab"); 
    DataPacket batch[1000];
    
    std::cout << "💾 ARCHIVER ONLINE (BINARY MODE)" << std::endl;
    
    while(running) {
        int count = 0;
        while(count < 1000 && rb.pop(batch[count])) {
            count++;
        }
        
        if (count > 0) {
            fwrite(batch, sizeof(DataPacket), count, f);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    if (f) fclose(f);
}

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    
    bool replay_mode = false;
    if (argc > 1 && std::string(argv[1]) == "--replay") replay_mode = true;

    std::thread t_ws(websocket_thread);
    std::thread t_engine;

    if (replay_mode) {
        t_engine = std::thread(replay_thread); 
    } else {
        std::thread t_arch(archiver_thread);
        t_engine = std::thread(ingestor_thread); 
        t_arch.detach(); 
    }

    t_engine.join();
    t_ws.join();
    WSACleanup();
    return 0;
}
