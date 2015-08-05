#include <stdio.h>
#include <stdint.h>
#include "md5calc.h"


void md5_calculate(char *result, const char *bytes, unsigned count)
{
    static const uint32_t SIN[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
        0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
        0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x2441453,  0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
        0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
        0xd4ef3085, 0x4881d05,  0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
        0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
    };
    static const int Shift[4][4] = {
        { 7, 12, 17, 22 }, { 5, 9, 14,  20 },
        { 4, 11, 16, 23 }, { 6, 10, 15, 21 }
    };
    uint32_t Q[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
    uint32_t A, B, C, D, X[16];
    uint64_t bits = 8 * count;
    unsigned i;

    while( bytes || count ) {
        for(i = 0; i < 16; ++i)
            X[i] = 0;
        if( bytes ) {
            for(i = 0; i < (count > 64 ? 64 : count); ++i)
                X[i/4] |= (unsigned char)bytes[i] << 8 * (i % 4);
            if( count >= 64 ) {
                bytes += 64;
                count -= 64;
            }else{
                X[count/4] |= 0x80 << 8 * (count % 4);
                ++count;
                bytes = NULL;
            }
        }else
            count = 0;
        if( bytes == NULL && count <= 56 ) {
            X[14] = bits;
            X[15] = bits >> 32;
            count = 0;
        }
        A = Q[0];
        B = Q[1];
        C = Q[2];
        D = Q[3];
        for(i = 0; i < 64; ++i) {
            uint32_t rol = ((i < 16) ? ((B & C) | (~B & D)) + X[i] :
                (i < 32) ? ((D & B) | (~D & C)) + X[(5 * i + 1) & 0xf] :
                (i < 48) ? (B ^ C ^ D) + X[(3 * i + 5) & 0xf] :
                (C ^ (B | ~D)) + X[(7 * i) & 0xf]) + A + SIN[i];
            A = D; D = C; C = B;
            B += rol << Shift[i/16][i%4] | rol >> (32-Shift[i/16][i%4]);
        }
        Q[0] += A;
        Q[1] += B;
        Q[2] += C;
        Q[3] += D;
    }
    for(i = 0; i < 4; ++i)
        sprintf(result + 8 * i, "%02x%02x%02x%02x", Q[i] & 0xff,
                Q[i] >> 8 & 0xff, Q[i] >> 16 & 0xff, Q[i] >> 24 & 0xff);
}

