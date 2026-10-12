#ifndef _DEFINES_H
#define _DEFINES_H

#include <stdint.h>

#define DIV_2       (1U)
#define DIV_4       (2U)
#define DIV_8       (3U)

#define SYSCLK_Hz       (100000000U)
#define APB2_CLK_RATE   (SYSCLK_Hz)
#define APB1_CLK_RATE   (SYSCLK_Hz >> DIV_2)

#define FALSE   (0U)
#define TRUE    (!FALSE)
// #define SET     (TRUE)
#define CLR     (FALSE)

#define INITIALIZED (1U)
#define UNITIALIZED (0U)

#define STATUS_OK   (0U)
#define STATUS_WAIT (2U)
#define STATUS_ERR  (1U)

#define NODE_ID (0x00U)

#define HASH_SEED   (0xDEADBEEFU)
#define HASH_VALUE(val, len) _murmurhash3_32((const void *)val, (uint32_t)len, HASH_SEED)

typedef enum {
    ALERT_SYS_TEMP,
    ALERT_SYS_HUM,
    ALERT_PRESENCE,
    ALERT_SECURITY_BREACH,
    ALERT_SEC_STATUS_CHANGE
} alert_type_e;

typedef enum {
    MSG_EVENT_DISARMED,
    MSG_EVENT_UNIT_OPEN,
    MSG_EVENT_UNIT_CLOSE,
    MSG_EVENT_UNIT_MOVE,
    MSG_EVENT_PRESENCE,
    MSG_EVENT_NO_PRESENCE
} event_type_e;

static uint32_t _murmurhash3_32(const void* key, uint32_t len, uint32_t seed) {
    const uint8_t* data = (const uint8_t*)key;
    const int nblocks = len / 4;
    uint32_t h1 = seed;

    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    // ---------- body ----------
    const uint32_t* blocks = (const uint32_t*)(data + nblocks * 4);

    for (int i = -nblocks; i; i++) {
        uint32_t k1 = blocks[i];

        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17); // ROTL32(k1, 15)
        k1 *= c2;

        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19); // ROTL32(h1, 13)
        h1 = h1 * 5 + 0xe6546b64;
    }

    // ---------- tail ----------
    const uint8_t* tail = (const uint8_t*)(data + nblocks * 4);
    uint32_t k1 = 0;

    switch (len & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
                k1 *= c1; 
                k1 = (k1 << 15) | (k1 >> 17); 
                k1 *= c2; 
                h1 ^= k1;
    };

    // ---------- finalization ----------
    h1 ^= len;

    // fmix32 avalanche step
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}

#endif