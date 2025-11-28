#include <iostream>
#include <cstdio>
#include <vector>

int main() {
    FILE* f = fopen("apex_blackbox.log", "rb");
    if (!f) {
        std::cout << "❌ ARQUIVO NÃO ENCONTRADO" << std::endl;
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long long size = ftell(f);
    rewind(f);

    std::cout << "📦 APEX BLACKBOX INSPECTOR" << std::endl;
    std::cout << "💾 TAMANHO TOTAL: " << size / (1024.0 * 1024.0) << " MB" << std::endl;
    std::cout << "👀 PRIMEIROS 256 BYTES:" << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    char buffer[257];
    size_t read = fread(buffer, 1, 256, f);
    buffer[read] = '\0';

    std::cout << buffer << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    fclose(f);
    return 0;
}
