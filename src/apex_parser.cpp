#include <iostream>
#include <windows.h>
#include <immintrin.h>
#include <chrono>
#include <vector>

size_t parse_avx2(const char* data, size_t size) {
    size_t lines = 0;
    size_t i = 0;
    __m256i newline_vec = _mm256_set1_epi8('\n');

    for (; i + 31 < size; i += 32) {
        __m256i chunk = _mm256_loadu_si256((const __m256i*)(data + i));
        __m256i cmp = _mm256_cmpeq_epi8(chunk, newline_vec);
        unsigned int mask = _mm256_movemask_epi8(cmp);
        lines += __popcnt(mask);
    }

    for (; i < size; i++) {
        if (data[i] == '\n') lines++;
    }

    return lines;
}

int main() {
    const char* filename = "apex_dataset.csv";

    HANDLE hFile = CreateFileA(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cout << "❌ ERRO: Rode o data_forge.cpp primeiro!" << std::endl;
        return 1;
    }

    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    size_t size = (size_t)fileSize.QuadPart;

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    char* data = (char*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);

    std::cout << "🚀 PROCESSANDO " << size / (1024.0 * 1024.0) << " MB VIA AVX2..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    size_t lines = parse_avx2(data, size);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    double throughput = (size / (1024.0 * 1024.0 * 1024.0)) / elapsed.count();

    std::cout << "📊 RESULTADO:" << std::endl;
    std::cout << "   Linhas: " << lines << std::endl;
    std::cout << "   Tempo:  " << elapsed.count() << "s" << std::endl;
    std::cout << "   Speed:  " << throughput << " GB/s" << std::endl;

    UnmapViewOfFile(data);
    CloseHandle(hMap);
    CloseHandle(hFile);

    return 0;
}
