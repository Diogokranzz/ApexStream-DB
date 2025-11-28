#ifndef SHA1_H
#define SHA1_H

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

class SHA1 {
public:
    SHA1();
    void update(const std::string &s);
    void update(std::istream &is);
    std::string final();
    std::string final_raw();
    static std::string from_file(const std::string &filename);

private:
    uint32_t digest[5];
    std::string buffer;
    uint64_t transforms;
};

#endif
