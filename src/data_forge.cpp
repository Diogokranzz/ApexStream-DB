#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <cstdio>
#include <chrono>

int main() {
    const size_t TARGET_SIZE = 1024 * 1024 * 1024; 
    const char* filename = "apex_dataset.csv";
    
    FILE* f = fopen(filename, "wb");
    if (!f) return 1;

    char buffer[65536];
    size_t current_size = 0;
    long long id = 0;
    
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(20.0, 90.0);

    std::cout << "🔥 FORJANDO 1GB DE DADOS CSV..." << std::endl;

    while (current_size < TARGET_SIZE) {
        int len = sprintf(buffer, "%lld,1700000000,%.2f\n", ++id, dist(rng));
        fwrite(buffer, 1, len, f);
        current_size += len;
    }

    fclose(f);
    std::cout << "✅ FEITO. ARQUIVO: " << filename << std::endl;
    return 0;
}
