#include <stdio.h>
#include <stdint.h>
#include "md5calc.h"


static uint32_t rol(uint32_t a, uint32_t s)
{
    return (a << s | a >> (32-s));
}

void md5_calculate(char *result, const char *bytes, unsigned count)
{
    static const uint32_t T1[16] = { 0xd76aa478,
        0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
        0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1,
        0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821
    };
    static const uint32_t T2[16] = { 0xf61e2562,
        0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x2441453,
        0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87,
        0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a
    };
    static const uint32_t T3[16] = { 0xfffa3942,
        0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9,
        0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa, 0xd4ef3085,
        0x4881d05,  0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    };
    static const uint32_t T4[16] = { 0xf4292244,
        0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
        0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314,
        0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
    };
    uint32_t A = 0x67452301, B = 0xefcdab89, C = 0x98badcfe, D = 0x10325476;
    uint32_t AA, BB, CC, DD, X[16];
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
        AA = A;
        BB = B;
        CC = C;
        DD = D;
        for(i = 0; i < 16; i += 4) {
            A = B + rol(A + ((B & C) | (~B & D)) + X[i + 0] + T1[i+0],   7);
            D = A + rol(D + ((A & B) | (~A & C)) + X[i + 1] + T1[i+1],  12);
            C = D + rol(C + ((D & A) | (~D & B)) + X[i + 2] + T1[i+2],  17);
            B = C + rol(B + ((C & D) | (~C & A)) + X[i + 3] + T1[i+3],  22);
        }
        for(i = 0; i < 16; i += 4) {
            A = B + rol(A + ((B & D)|(C & ~D))+ X[(5*i+ 1) & 0xf] + T2[i+0], 5);
            D = A + rol(D + ((A & C)|(B & ~C))+ X[(5*i+ 6) & 0xf] + T2[i+1], 9);
            C = D + rol(C + ((D & B)|(A & ~B))+ X[(5*i+11) & 0xf] + T2[i+2],14);
            B = C + rol(B + ((C & A)|(D & ~A))+ X[(5*i+ 0) & 0xf] + T2[i+3],20);
        }
        for(i = 0; i < 16; i += 4) {
            A = B + rol(A + (B ^ C ^ D) + X[(3*i+ 5) & 0xf] + T3[i+0],  4);
            D = A + rol(D + (A ^ B ^ C) + X[(3*i+ 8) & 0xf] + T3[i+1], 11);
            C = D + rol(C + (D ^ A ^ B) + X[(3*i+11) & 0xf] + T3[i+2], 16);
            B = C + rol(B + (C ^ D ^ A) + X[(3*i+14) & 0xf] + T3[i+3], 23);
        }
        for(i = 0; i < 16; i += 4) {
            A = B + rol(A + (C ^ (B | ~D)) + X[(7*i+ 0) & 0xf] + T4[i+0],  6);
            D = A + rol(D + (B ^ (A | ~C)) + X[(7*i+ 7) & 0xf] + T4[i+1], 10);
            C = D + rol(C + (A ^ (D | ~B)) + X[(7*i+14) & 0xf] + T4[i+2], 15);
            B = C + rol(B + (D ^ (C | ~A)) + X[(7*i+ 5) & 0xf] + T4[i+3], 21);
        }
        A += AA;
        B += BB;
        C += CC;
        D += DD;
    }
    sprintf(result,
            "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
            A & 0xff, A >> 8 & 0xff, A >> 16 & 0xff, A >> 24 & 0xff,
            B & 0xff, B >> 8 & 0xff, B >> 16 & 0xff, B >> 24 & 0xff,
            C & 0xff, C >> 8 & 0xff, C >> 16 & 0xff, C >> 24 & 0xff,
            D & 0xff, D >> 8 & 0xff, D >> 16 & 0xff, D >> 24 & 0xff);
}

