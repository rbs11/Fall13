#ifndef CACHESIM_HPP
#define CACHESIM_HPP
#include <inttypes.h>
#include <stdint.h>

struct cache_stats_t {
	uint64_t accesses;
    uint64_t reads;
    uint64_t writes;
    uint64_t L1_accesses;
    uint64_t L1_read_misses;
    uint64_t L1_write_misses;
    uint64_t L2_read_misses;
    uint64_t L2_write_misses;
    uint64_t TLB_Misses;
    uint64_t write_backs;
    uint64_t prefetched_blocks;
    uint64_t evicted_blocks;
    uint64_t successful_prefetches; // The number of cache misses reduced by prefetching
    double   avg_access_time;
    double	modified_AAT;

};

struct pf //This is the entry in the free page data structure
{
	uint64_t frame_number;
	bool free;
};
struct physical_frame_LRU //This is the entry in the hash table
{
	uint64_t ppf; //physical page frame
	uint64_t time_stamp;
	uint64_t i; //This variable is so that i can reference into the physical_frame structrue to get the next free physical frame
};
struct virtual_frame //This is the entry in the hash table
{
	uint64_t vpf; //virtual page frame
	uint32_t pref_strength;
	uint64_t time_stamp;
	uint64_t counter;
};

struct table
{
	uint64_t pfn;
	uint64_t counter;
};

void cache_access(char rw, uint64_t address, cache_stats_t* p_stats);
void setup_cache(uint64_t c1, uint64_t b1, uint64_t s1, uint64_t c2, uint64_t b2, uint64_t s2, uint32_t k);
void complete_cache(cache_stats_t *p_stats);

static const uint64_t DEFAULT_C1 = 12;   /* 4KB Cache */
static const uint64_t DEFAULT_B1 = 5;    /* 32-byte blocks */
static const uint64_t DEFAULT_S1 = 3;    /* 8 blocks per set */
static const uint64_t DEFAULT_C2 = 15;   /* 32KB Cache */
static const uint64_t DEFAULT_B2 = 6;    /* 64-byte blocks */
static const uint64_t DEFAULT_S2 = 5;    /* 32 blocks per set */
static const uint32_t DEFAULT_K = 2;    /* prefetch 2 subsequent blocks */
//extern uint64_t count;
/** Argument to cache_access rw. Indicates a load */
static const char     READ = 'r';
/** Argument to cache_access rw. Indicates a store */
static const char     WRITE = 'w';
#define PRIu64 "llu"
#define PRIu32 "u"
#define PRIx64 "llx"
#define STRENGTH_START_VAL 10
#define IPT_PREFETCH 1
#define AGGR_PREFETCH 0
#define MAPPING	1
#define IPT_LRU 1
#define physical_address_size 1024*1024*1024*4
#define page_size 4096
#define MAX_COUNTER_SIZE 4
#define MAX_TABLE_SIZE 20

#endif /* CACHESIM_HPP */
