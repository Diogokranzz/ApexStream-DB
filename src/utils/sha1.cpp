#include "sha1.h"
#include <sstream>
#include <iomanip>
#include <fstream>

static const size_t BLOCK_INTS = 16;
static const size_t BLOCK_BYTES = BLOCK_INTS * 4;

static void reset(uint32_t digest[], std::string &buffer, uint64_t &transforms) {
    digest[0] = 0x67452301;
    digest[1] = 0xefcdab89;
    digest[2] = 0x98badcfe;
    digest[3] = 0x10325476;
    digest[4] = 0xc3d2e1f0;
    buffer = "";
    transforms = 0;
}

static uint32_t rol(const uint32_t value, const size_t bits) {
    return (value << bits) | (value >> (32 - bits));
}

static void transform(uint32_t digest[], uint32_t block[BLOCK_INTS]) {
    uint32_t a = digest[0], b = digest[1], c = digest[2], d = digest[3], e = digest[4];

    for (size_t i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5a827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ed9eba1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8f1bbcdc;
        } else {
            f = b ^ c ^ d;
            k = 0xca62c1d6;
        }
        
        uint32_t temp = rol(a, 5) + f + e + k + block[i % 16];
        e = d;
        d = c;
        c = rol(b, 30);
        b = a;
        a = temp;
        
        size_t j = i % 16;
        size_t next = (j + 13) % 16;
        size_t next2 = (j + 8) % 16;
        size_t next3 = (j + 2) % 16;
        size_t next4 = j;
        block[next4] = rol(block[next3] ^ block[next2] ^ block[next] ^ block[j], 1);
    }

    digest[0] += a;
    digest[1] += b;
    digest[2] += c;
    digest[3] += d;
    digest[4] += e;
}

static void buffer_to_block(const std::string &buffer, uint32_t block[BLOCK_INTS]) {
    for (size_t i = 0; i < BLOCK_INTS; i++) {
        block[i] = (buffer[4 * i + 3] & 0xff)
                 | (buffer[4 * i + 2] & 0xff) << 8
                 | (buffer[4 * i + 1] & 0xff) << 16
                 | (buffer[4 * i + 0] & 0xff) << 24;
    }
}

SHA1::SHA1() {
    reset(digest, buffer, transforms);
}

void SHA1::update(const std::string &s) {
    std::istringstream is(s);
    update(is);
}

void SHA1::update(std::istream &is) {
    while (true) {
        char sbuf[BLOCK_BYTES];
        is.read(sbuf, BLOCK_BYTES - buffer.size());
        buffer.append(sbuf, is.gcount());
        if (buffer.size() != BLOCK_BYTES) {
            return;
        }
        uint32_t block[BLOCK_INTS];
        buffer_to_block(buffer, block);
        transform(digest, block);
        buffer.clear();
        transforms++;
    }
}

std::string SHA1::final() {
    uint64_t total_bits = (transforms * BLOCK_BYTES + buffer.size()) * 8;
    buffer += (char)0x80;
    size_t orig_size = buffer.size();
    while (buffer.size() < BLOCK_BYTES) {
        buffer += (char)0x00;
    }
    uint32_t block[BLOCK_INTS];
    buffer_to_block(buffer, block);
    if (orig_size > BLOCK_BYTES - 8) {
        transform(digest, block);
        for (size_t i = 0; i < BLOCK_INTS - 2; i++) {
            block[i] = 0;
        }
    }
    block[BLOCK_INTS - 1] = total_bits;
    block[BLOCK_INTS - 2] = (total_bits >> 32);
    transform(digest, block);
    std::ostringstream result;
    for (size_t i = 0; i < 5; i++) {
        result << std::hex << std::setfill('0') << std::setw(8);
        result << digest[i];
    }
    reset(digest, buffer, transforms);
    return result.str();
}

std::string SHA1::final_raw() {
    uint64_t total_bits = (transforms * BLOCK_BYTES + buffer.size()) * 8;
    buffer += (char)0x80;
    size_t orig_size = buffer.size();
    while (buffer.size() < BLOCK_BYTES) {
        buffer += (char)0x00;
    }
    uint32_t block[BLOCK_INTS];
    buffer_to_block(buffer, block);
    if (orig_size > BLOCK_BYTES - 8) {
        transform(digest, block);
        for (size_t i = 0; i < BLOCK_INTS - 2; i++) {
            block[i] = 0;
        }
    }
    block[BLOCK_INTS - 1] = total_bits;
    block[BLOCK_INTS - 2] = (total_bits >> 32);
    transform(digest, block);
    
    std::string result;
    result.reserve(20);
    for (size_t i = 0; i < 5; i++) {
        result += (char)((digest[i] >> 24) & 0xFF);
        result += (char)((digest[i] >> 16) & 0xFF);
        result += (char)((digest[i] >> 8) & 0xFF);
        result += (char)(digest[i] & 0xFF);
    }
    reset(digest, buffer, transforms);
    return result;
}

std::string SHA1::from_file(const std::string &filename) {
    std::ifstream stream(filename.c_str(), std::ios::binary);
    SHA1 checksum;
    checksum.update(stream);
    return checksum.final();
}
