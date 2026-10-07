/*
 * bench.c - a small CPU benchmark shaped like the game logic's work:
 * string hashing and table lookups (C# dictionaries), sorting, pointer
 * chasing through linked objects, indirect calls (virtual dispatch),
 * 64-bit integer math and some floating point.
 *
 * Built with wasi-sdk, translated by w2c2 and run on PC and Xbox 360 to get
 * the Xenon/PC speed ratio for translated code (milestone M0). Prints a
 * checksum per section: it must be identical on every machine.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

static uint64_t now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

static uint32_t rng_state = 12345;
static uint32_t rnd(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

/* ---- 1. string table ---- */
#define TABLE_BITS 16
#define TABLE_SIZE (1u << TABLE_BITS)
typedef struct entry { char key[24]; uint32_t value; struct entry* next; } entry;
static entry* table[TABLE_SIZE];
static entry pool[60000];

static uint32_t hash_str(const char* s) {
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

static uint32_t bench_strings(int rounds) {
    int i, r;
    uint32_t sum = 0;
    for (i = 0; i < 60000; i++) {
        entry* e = &pool[i];
        uint32_t h;
        snprintf(e->key, sizeof(e->key), "CARD.ITEM_%d_%u", i, (unsigned)(i * 2654435761u) % 9973u);
        e->value = (uint32_t)i;
        h = hash_str(e->key) & (TABLE_SIZE - 1);
        e->next = table[h];
        table[h] = e;
    }
    for (r = 0; r < rounds; r++) {
        for (i = 0; i < 60000; i += 3) {
            const entry* e = table[hash_str(pool[i].key) & (TABLE_SIZE - 1)];
            while (e && strcmp(e->key, pool[i].key) != 0) e = e->next;
            if (e) sum += e->value;
        }
    }
    return sum;
}

/* ---- 2. sorting ---- */
static int cmp_u32(const void* a, const void* b) {
    uint32_t x = *(const uint32_t*)a, y = *(const uint32_t*)b;
    return x < y ? -1 : x > y;
}

static uint32_t bench_sort(int rounds) {
    static uint32_t data[200000];
    uint32_t sum = 0;
    int r, i;
    for (r = 0; r < rounds; r++) {
        for (i = 0; i < 200000; i++) data[i] = rnd();
        qsort(data, 200000, sizeof(uint32_t), cmp_u32);
        sum += data[r * 7 % 200000] ^ data[100000];
    }
    return sum;
}

/* ---- 3. objects, pointer chasing and virtual calls ---- */
typedef struct creature creature;
typedef int (*hook_fn)(creature*, int);
struct creature { int hp, block, strength; hook_fn on_damage; creature* next; };

static int hook_plain(creature* c, int dmg) { return dmg; }
static int hook_armor(creature* c, int dmg) { int b = dmg < c->block ? dmg : c->block; c->block -= b; return dmg - b; }
static int hook_weak(creature* c, int dmg) { return (dmg * 3) / 4; }
static int hook_vuln(creature* c, int dmg) { return dmg + dmg / 2; }

static uint32_t bench_objects(int rounds) {
    static creature cs[4096];
    static const hook_fn hooks[4] = { hook_plain, hook_armor, hook_weak, hook_vuln };
    uint32_t sum = 0;
    int r, i;
    for (i = 0; i < 4096; i++) {
        cs[i].hp = 50 + (int)(rnd() % 100);
        cs[i].block = (int)(rnd() % 20);
        cs[i].strength = (int)(rnd() % 5);
        cs[i].on_damage = hooks[rnd() % 4];
        cs[i].next = &cs[rnd() % 4096];
    }
    for (r = 0; r < rounds; r++) {
        creature* c = &cs[r % 4096];
        for (i = 0; i < 100000; i++) {
            int dmg = 6 + c->strength;
            dmg = c->on_damage(c, dmg);
            c->hp -= dmg;
            if (c->hp <= 0) { c->hp += 80; c->block += 5; }
            sum += (uint32_t)c->hp;
            c = c->next;
        }
    }
    return sum;
}

/* ---- 4. 64-bit integer and floating point ---- */
static uint64_t bench_math(int rounds) {
    uint64_t acc = 0x9E3779B97F4A7C15ull;
    double f = 0.0;
    int r, i;
    for (r = 0; r < rounds; r++) {
        for (i = 1; i < 200000; i++) {
            acc = acc * 6364136223846793005ull + (uint64_t)i;
            acc ^= acc >> 29;
            acc += acc / (uint64_t)(i | 1);
            f += sqrt((double)(i & 1023)) * 0.5 + sin((double)i * 0.001);
        }
    }
    return acc ^ (uint64_t)(int64_t)(f * 1000.0);
}

int main(void) {
    uint64_t t0, t1, total = 0;
    uint32_t c;
    uint64_t c64;
    printf("bench start\n");

    t0 = now_us(); c = bench_strings(20); t1 = now_us(); total += t1 - t0;
    printf("strings  %8lu us  check %08x\n", (unsigned long)(t1 - t0), c);

    t0 = now_us(); c = bench_sort(5); t1 = now_us(); total += t1 - t0;
    printf("sort     %8lu us  check %08x\n", (unsigned long)(t1 - t0), c);

    t0 = now_us(); c = bench_objects(30); t1 = now_us(); total += t1 - t0;
    printf("objects  %8lu us  check %08x\n", (unsigned long)(t1 - t0), c);

    t0 = now_us(); c64 = bench_math(10); t1 = now_us(); total += t1 - t0;
    printf("math     %8lu us  check %08x%08x\n", (unsigned long)(t1 - t0),
           (unsigned)(c64 >> 32), (unsigned)c64);

    printf("total    %8lu us\n", (unsigned long)total);
    printf("bench done\n");
    return 0;
}
