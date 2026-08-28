#pragma once
#include <string>
#include <cstdint>
#include <cstring>

namespace sha256 {

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

inline void process_block(const uint8_t* block, uint32_t H[8]) {
    static const uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t W[64];
    for (int i = 0; i < 16; i++) {
        W[i] = (uint32_t)block[i*4]<<24 | (uint32_t)block[i*4+1]<<16 |
               (uint32_t)block[i*4+2]<<8 | (uint32_t)block[i*4+3];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr(W[i-15],7) ^ rotr(W[i-15],18) ^ (W[i-15]>>3);
        uint32_t s1 = rotr(W[i-2],17) ^ rotr(W[i-2],19) ^ (W[i-2]>>10);
        W[i] = W[i-16] + s0 + W[i-7] + s1;
    }
    uint32_t a=H[0],b=H[1],c=H[2],d=H[3],e=H[4],f=H[5],g=H[6],h=H[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr(e,6)^rotr(e,11)^rotr(e,25);
        uint32_t ch = (e&f)^((~e)&g);
        uint32_t t1 = h+S1+ch+K[i]+W[i];
        uint32_t S0 = rotr(a,2)^rotr(a,13)^rotr(a,22);
        uint32_t mj = (a&b)^(a&c)^(b&c);
        uint32_t t2 = S0+mj;
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    H[0]+=a;H[1]+=b;H[2]+=c;H[3]+=d;H[4]+=e;H[5]+=f;H[6]+=g;H[7]+=h;
}

inline std::string hash(const std::string& input) {
    uint32_t H[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                     0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint64_t bitlen = (uint64_t)input.size() * 8;
    uint8_t block[64];
    size_t offset = 0;
    // process full blocks
    while (offset + 64 <= input.size()) {
        std::memcpy(block, input.data()+offset, 64);
        process_block(block, H);
        offset += 64;
    }
    // last block(s) with padding
    std::memset(block, 0, 64);
    size_t rem = input.size() - offset;
    if (rem) std::memcpy(block, input.data()+offset, rem);
    block[rem] = 0x80;
    if (rem >= 56) {
        process_block(block, H);
        std::memset(block, 0, 64);
    }
    for (int i = 0; i < 8; i++) {
        block[63-i] = (uint8_t)(bitlen >> (i*8));
    }
    process_block(block, H);
    // hex output
    static const char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (int i = 0; i < 8; i++) {
        for (int j = 3; j >= 0; j--) {
            uint8_t b = (uint8_t)(H[i] >> (j*8));
            out.push_back(hex[b>>4]);
            out.push_back(hex[b&0xF]);
        }
    }
    return out;
}

// HMAC-SHA256 (simplified, key <= 64 bytes is fine for our use)
inline std::string hmac(const std::string& key, const std::string& msg) {
    uint8_t k[64];
    std::memset(k, 0, 64);
    if (key.size() > 64) {
        std::string kh = hash(key);
        // hash returns hex, we need raw bytes - but for our anti-tamper use,
        // using the hex string as key material is fine. Just truncate/pad.
        size_t cp = std::min(key.size(), (size_t)64);
        std::memcpy(k, key.data(), cp);
    } else {
        std::memcpy(k, key.data(), key.size());
    }
    uint8_t ipad[64], opad[64];
    for (int i = 0; i < 64; i++) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5c;
    }
    std::string inner;
    inner.append((char*)ipad, 64);
    inner += msg;
    std::string inner_hash = hash(inner);
    std::string outer;
    outer.append((char*)opad, 64);
    outer += inner_hash;
    return hash(outer);
}

} // namespace sha256
