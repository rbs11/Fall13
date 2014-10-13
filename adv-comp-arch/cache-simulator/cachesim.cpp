#include "cachesim.hpp"
#include <assert.h>
#include <math.h>
#include <time.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <cstring>
#include <vector>
#include <sys/time.h>
using namespace std;

struct c_entry {
	uint64_t tag;
	//uint32_t data;
	uint64_t lru_time;
	bool b_dirty;
	bool b_valid;
	bool prefetch;
	bool across_page;
};
//#define DEBUG
#ifdef DEBUG
#define	print_dbg(fmt, arg...) 					\
do {											\
	print_dbg(fmt, ## arg);					\
} while (0);
#else
#define	print_dbg(fmt, arg...)
#endif

typedef std::map<uint64_t, vector<c_entry> > L1CacheMap;
typedef std::map<uint64_t, vector<c_entry> > L2CacheMap;

void L1ReadReq(uint64_t address, cache_stats_t* p_stats);
c_entry L2ReadReq(uint64_t address, cache_stats_t* p_stats);
void L1WriteReq(uint64_t address, cache_stats_t* p_stats);
void L2WriteReq(uint64_t address, cache_stats_t* p_stats);
void Prefetch_Blocks(uint64_t address, cache_stats_t* p_stats);
void Improve_granularity(std::vector<c_entry> *m_entry, uint32_t nCnt,
		uint32_t nWays);
void add_lru_info(uint64_t* lru_time);
uint32_t find_lru_blk(std::vector<c_entry> *m_entry, uint32_t nWays);
extern uint64_t conversion(uint64_t address);
L1CacheMap L1Map;
L2CacheMap L2Map;
uint64_t LastMBlk = 0;
volatile uint64_t glrutime = 10000; /*For prefetched blocks, 0-9999 is used*/
int64_t pending_stride = 0;
int64_t diff = 0;
int k_prefetch = 0;
uint64_t gs1, gs2;
extern int count_across_pages;
extern int count_across_page_successful;
extern std::map<uint64_t, physical_frame_LRU> page_table; //This is the page table
extern std::map<uint64_t, virtual_frame> inverted_pg_table;
extern uint64_t count;
extern uint64_t actual_table_size;
struct _CacheParams {
	uint64_t cache_sz;
	uint64_t tag_mask;
	uint64_t index_mask;
	uint64_t offset_mask;
	uint32_t nWays; /*Associativity*/
	uint8_t offset_bits; /*B*/
	uint8_t tag_bits;
	uint8_t index_bits; /*C-B-S*/
} L1Params, L2Params;

/**
 * Subroutine for initializing the cache. You many add and initialize any global or heap
 * variables as needed.
 *
 * @c1 Total size of L1 in bytes is 2^C1
 * @b1 Size of each block in L1 in bytes is 2^B1
 * @s1 Number of blocks per set in L1 is 2^S1
 * @c2 Total size of L2 in bytes is 2^C2
 * @b2 Size of each block in L2 in bytes is 2^B2
 * @s2 Number of blocks per set in L2 is 2^S2
 * @k Prefetch K subsequent blocks
 */
void setup_cache(uint64_t c1, uint64_t b1, uint64_t s1, uint64_t c2,
		uint64_t b2, uint64_t s2, uint32_t k) {

	L1Params.index_bits = c1 - b1 - s1;
	L1Params.offset_bits = b1;
	L1Params.index_mask = ((uint64_t) pow(2, (c1 - b1 - s1)) - 1) << b1;
	L1Params.offset_mask = pow(2, b1) - 1;
	L1Params.cache_sz = pow(2, c1);
	L1Params.tag_bits = 64 - (c1 - s1);
	L1Params.tag_mask = ((uint64_t) pow(2, (64 - (c1 - s1))) - 1) << (c1 - s1);
	L2Params.offset_mask = pow(2, b2) - 1;
	L2Params.index_mask = ((uint64_t) pow(2, (c2 - b2 - s2)) - 1) << b2;
	L2Params.offset_bits = b2;
	L2Params.index_bits = c2 - b2 - s2;
	L2Params.cache_sz = pow(2, c2);
	L2Params.tag_mask = ((uint64_t) pow(2, (64 - (c2 - s2))) - 1)
			<< ((c2 - s2));
	L1Params.nWays = pow(2, s1);
	L2Params.nWays = pow(2, s2);
	k_prefetch = k;
	gs1 = s1;
	gs2 = s2;
}

/**
 * Subroutine that simulates the cache one trace event at a time.
 *
 * @rw The type of event. Either READ or WRITE
 * @address  The target memory address
 * @p_stats Pointer to the statistics structure
 */
void cache_access(char rw, uint64_t address, cache_stats_t* p_stats) {

	print_dbg("\n"); print_dbg("%lld|%c|%llx", p_stats->accesses, rw, address);
	//p_stats->accesses++;
	switch (rw) {
	case 'r':
		p_stats->reads++;
		L1ReadReq(address, p_stats);

		break;
	case 'w':
		p_stats->writes++;
		L1WriteReq(address, p_stats);
		break;
	default:
		print_dbg("Request Type Error \n");
	}
}
/*L1 Read*/
void L1ReadReq(uint64_t address, cache_stats_t* p_stats) {
	uint64_t CurTag = 0;
	uint64_t L1RowDecoder = 0;
	uint64_t evict_blk = 0;
	struct c_entry L2_RowData;
	L1CacheMap::iterator kpos;
	uint32_t nWays = 0;
	uint32_t nCnt = 0;
	uint32_t nIndex = 0;
	bool bFlag = 0;
	bool hit = 0;
	bool slotavailable = 0;

	p_stats->L1_accesses++;
	/*Construct Index and Tag using pre-set parameters*/
	L1RowDecoder = (address & (L1Params.index_mask)) >> L1Params.offset_bits;
	CurTag = (address & L1Params.tag_mask) >> (L1Params.index_bits
			+ L1Params.offset_bits);
	nWays = L1Params.nWays;

	if ((kpos = L1Map.find(L1RowDecoder)) != L1Map.end()) /*Row already present*/
	{
		/*Row is present, check for tag*/
		for (nCnt = 0; nCnt < nWays; nCnt++) {
			if (CurTag == kpos->second[nCnt].tag) {
				print_dbg("|L1ReadHit(%llx)", (address&(~L1Params.offset_mask)));
				hit = 1;
				add_lru_info(&kpos->second[nCnt].lru_time);/*Set the lru time stamp*/
				break;
			} else if (!kpos->second[nCnt].tag)
				slotavailable = 1; /*Indicates no eviction required*/
		}
		if (!hit) /*L1 Cache Read MISS*/
		{
			p_stats->L1_read_misses++;
			print_dbg("|L1ReadMiss(%llx)", (address&(~L1Params.offset_mask)));
			/*Fetch block from L2 before doing anything else*/
			L2_RowData = L2ReadReq(address, p_stats);
			if (!slotavailable)/*Check if eviction is required*/
			{
				nIndex = find_lru_blk(&kpos->second, nWays);
				evict_blk = ((L1Map[L1RowDecoder][nIndex].tag
						<< (L1Params.offset_bits + L1Params.index_bits))
						| (L1RowDecoder << L1Params.offset_bits));
				if (L1Map[L1RowDecoder][nIndex].b_dirty)/*Write to L2 if dirty*/
				{/*Allocate Block L1 in next available slot, This is the copied block coming from L2. Update TS*/
					L1Map[L1RowDecoder][nIndex].tag = CurTag;
					L1Map[L1RowDecoder][nIndex].b_dirty = 0;
					L1Map[L1RowDecoder][nIndex].b_valid = 1;
					L1Map[L1RowDecoder][nIndex].prefetch = 0;
					add_lru_info(&(L1Map[L1RowDecoder][nIndex].lru_time));
					/*In some cases time-stamp for different blks show up as the same due to lack of granularity*/
					print_dbg("|L1Put[tag=%llx, dirty=%d]", CurTag, L1Map[L1RowDecoder][nIndex].b_dirty); print_dbg("|L1Evict(%llx)", evict_blk); print_dbg("|L1WB(%llx)", evict_blk);
					bFlag = 1;
					L2WriteReq(evict_blk, p_stats);
				}
			} else /*Update index to next available slot*/
			{
				nIndex = 0;
				while (L1Map[L1RowDecoder][nIndex].tag != 0)
					nIndex++;
			}
			if (!bFlag) {/*Allocate Block L1 in next available slot, This is the copied block coming from L2. Update TS*/
				L1Map[L1RowDecoder][nIndex].tag = CurTag;
				L1Map[L1RowDecoder][nIndex].b_dirty = 0;
				L1Map[L1RowDecoder][nIndex].b_valid = 1;
				add_lru_info(&(L1Map[L1RowDecoder][nIndex].lru_time));
				print_dbg("|L1Put[tag=%llx, dirty=%d]", CurTag, L1Map[L1RowDecoder][nIndex].b_dirty);
				if (evict_blk)
					print_dbg("|L1Evict(%llx)", evict_blk);
			}
		}
	} else /*Row being accessed for the first time*/
	{
		/*No eviction required since row is accessed the first time.  */
		p_stats->L1_read_misses++;
		print_dbg("|L1ReadMiss(%llx)", (address&(~L1Params.offset_mask)));
		L2_RowData = L2ReadReq(address, p_stats); /*Fetch block from L2*/
		for (nCnt = 0; nCnt < nWays; nCnt++) /*Allocating row entries depending on associativity*/
		{
			L1Map[L1RowDecoder].push_back(c_entry());
		}
		/*Copy Block to L1, Don't mark as dirty as this is a read operation*/
		L1Map[L1RowDecoder][0].tag = CurTag;
		L1Map[L1RowDecoder][0].b_dirty = 0;
		L1Map[L1RowDecoder][0].b_valid = 1;
		L1Map[L1RowDecoder][0].prefetch = 0;
		add_lru_info(&(L1Map[L1RowDecoder][0].lru_time));
		print_dbg("|L1Put[tag=%llx, dirty=%d]", CurTag, L1Map[L1RowDecoder][0].b_dirty);
	}
}
/*L1 Write*/
void L1WriteReq(uint64_t address, cache_stats_t* p_stats) {
	uint64_t CurTag = 0;
	uint64_t L1RowDecoder = 0;
	uint64_t evict_blk = 0;
	L1CacheMap::iterator kpos;
	struct c_entry L2_RowData;
	uint32_t nWays = 0;
	uint32_t nCnt = 0;
	uint32_t nIndex = 0;
	bool bFlag = 0;
	bool hit = 0;
	bool slotavailable = 0;

	p_stats->L1_accesses++;

	L1RowDecoder = (address & (L1Params.index_mask)) >> L1Params.offset_bits;
	CurTag = (address & L1Params.tag_mask) >> (L1Params.index_bits
			+ L1Params.offset_bits);

	nWays = L1Params.nWays;
	if ((kpos = L1Map.find(L1RowDecoder)) != L1Map.end()) /*Row already present*/
	{
		/*Row is present, check for tag*/
		for (nCnt = 0; nCnt < nWays; nCnt++) {
			if (CurTag == kpos->second[nCnt].tag) {
				print_dbg("|L1WriteHit(%llx)", (address&(~L1Params.offset_mask)));
				/*Cache HIT, Update timestamp and set dirty bit*/
				hit = 1;
				add_lru_info(&kpos->second[nCnt].lru_time);
				kpos->second[nCnt].b_dirty = 1;
				/*In some cases time-stamp for different blks show up as the same due to lack of granularity*/
				break;
			} else if (!kpos->second[nCnt].tag)
				slotavailable = 1; /*Indicates no eviction required*/
		}
		if (!hit) /*L1 Cache Write MISS*/
		{
			p_stats->L1_write_misses++;
			print_dbg("|L1WriteMiss(%llx)", (address&(~L1Params.offset_mask)));
			L2_RowData = L2ReadReq(address, p_stats); /*Fetch block from L2 before anything else*/
			/*Check if eviction is required*/
			if (!slotavailable) {
				nIndex = find_lru_blk(&kpos->second, nWays);
				evict_blk = ((L1Map[L1RowDecoder][nIndex].tag
						<< (L1Params.offset_bits + L1Params.index_bits))
						| (L1RowDecoder << L1Params.offset_bits));
				/*Write to L2 if dirty*/
				if (L1Map[L1RowDecoder][nIndex].b_dirty) {
					/*Allocate Block L1 in next available slot, This is the copied block coming from L2. Update TS*/
					L1Map[L1RowDecoder][nIndex].tag = CurTag;
					L1Map[L1RowDecoder][nIndex].b_dirty = 1;
					L1Map[L1RowDecoder][nIndex].b_valid = 1;
					L1Map[L1RowDecoder][nIndex].prefetch = 0;
					add_lru_info(&(L1Map[L1RowDecoder][nIndex].lru_time));
					print_dbg("|L1Put[tag=%llx, dirty=%d]", CurTag, L1Map[L1RowDecoder][nIndex].b_dirty); print_dbg("|L1Evict(%llx)", evict_blk); print_dbg("|L1WB(%llx)", evict_blk);
					bFlag = 1;
					L2WriteReq(evict_blk, p_stats);
				}
			} else /*Update index to next available slot*/
			{
				nIndex = 0;
				while (L1Map[L1RowDecoder][nIndex].tag != 0)
					nIndex++;
				if (nIndex >= nWays)
					print_dbg("ERROR IN LOGIC[%d]", nIndex);
			}
			if (!bFlag) {
				/*Update L1 Block in next available slot. Set dirty bit. Update TS*/
				L1Map[L1RowDecoder][nIndex].tag = CurTag;
				L1Map[L1RowDecoder][nIndex].b_dirty = 1;
				L1Map[L1RowDecoder][nIndex].b_valid = 1;
				L1Map[L1RowDecoder][nIndex].prefetch = 0;
				add_lru_info(&(L1Map[L1RowDecoder][nIndex].lru_time));
				print_dbg("|L1Put[tag=%llx, dirty=%d]", CurTag, L1Map[L1RowDecoder][nIndex].b_dirty);
				if (evict_blk)
					print_dbg("|L1Evict(%llx)", evict_blk);
			}
		}
	} else /*Add row entry in L1*/
	{
		print_dbg("|L1WriteMiss(%llx)", (address&(~L1Params.offset_mask)));
		p_stats->L1_write_misses++;
		for (nCnt = 0; nCnt < nWays; nCnt++)
			L1Map[L1RowDecoder].push_back(c_entry());
		L2_RowData = L2ReadReq(address, p_stats); /*Fetch block from L2*/
		L1Map[L1RowDecoder][0].tag = CurTag;
		L1Map[L1RowDecoder][0].b_dirty = 1;
		L1Map[L1RowDecoder][0].b_valid = 1;
		L1Map[L1RowDecoder][0].prefetch = 0;
		add_lru_info(&(L1Map[L1RowDecoder][0].lru_time));
		print_dbg("|L1Put[tag=%llx, dirty=%d]", CurTag, L1Map[L1RowDecoder][0].b_dirty);
	}
}
/*L2 Read*/
c_entry L2ReadReq(uint64_t address, cache_stats_t* p_stats) {
	uint64_t CurTag = 0;
	uint64_t L2RowDecoder = 0;
	struct c_entry L2_RowData;
	L1CacheMap::iterator kpos;
	uint64_t evict_blk = 0;
	uint32_t nWays = 0;
	uint32_t nCnt = 0;
	uint32_t nIndex = 0;
	bool hit = 0;
	bool bFlag = 0;
	bool slotavailable = 0;

	//	p_stats->L2_accesses++;
	address = address & ~L1Params.offset_mask;
	L2RowDecoder = (address & (L2Params.index_mask)) >> L2Params.offset_bits;
	CurTag = (address & L2Params.tag_mask) >> (L2Params.index_bits
			+ L2Params.offset_bits);
	nWays = L2Params.nWays;

	if ((kpos = L2Map.find(L2RowDecoder)) != L2Map.end()) /*Row already present*/
	{
		for (nCnt = 0; nCnt < nWays; nCnt++) {
			if (CurTag == kpos->second[nCnt].tag) {
				/*Cache HIT, Update timestamp*/
				hit = 1;
				add_lru_info(&kpos->second[nCnt].lru_time);
				memcpy(&L2_RowData, &kpos->second[nCnt], sizeof(struct c_entry));
				print_dbg("|L2ReadHit(%llx)", (address&(~L2Params.offset_mask)));
				if (kpos->second[nCnt].prefetch) {
					if (kpos->second[nCnt].across_page){
						kpos->second[nCnt].across_page= 0;
						count_across_page_successful++;
					}
					p_stats->successful_prefetches++;
					kpos->second[nCnt].prefetch = 0;
					print_dbg("|PrefetchSuccess(%llx) TraceLine[%d]\n\n", (address&(~L2Params.offset_mask)), count);
				}
				break;
			} else if (!kpos->second[nCnt].tag)
				slotavailable = 1; /*Indicates no eviction required*/
		}
		if (!hit) /*L2 Cache Read MISS*/
		{
			p_stats->L2_read_misses++;
			print_dbg("|L2ReadMiss(%llx)", (address&(~L2Params.offset_mask)));
			/*Fetch from main memory, not required*/
			/*Check if eviction is required*/
			if (!slotavailable) {
				nIndex = find_lru_blk(&kpos->second, nWays);
				evict_blk = ((L2Map[L2RowDecoder][nIndex].tag
						<< (L2Params.offset_bits + L2Params.index_bits))
						| (L2RowDecoder << L2Params.offset_bits));
				/*Write to memory if dirty*/
				if (L2Map[L2RowDecoder][nIndex].b_dirty) {
					bFlag = 1;
					p_stats->write_backs++;
				}
			} else /*Update index to next available slot*/
			{
				nIndex = 0;
				while (L2Map[L2RowDecoder][nIndex].tag != 0)
					nIndex++;
				if (nIndex >= nWays)
					print_dbg("ERROR IN LOGIC[%d]", nIndex);
			}
			/*Allocate Block L2 in next available slot,
			 * This is the copied block coming from memory(in theory). Update TS*/
			L2Map[L2RowDecoder][nIndex].tag = CurTag;
			L2Map[L2RowDecoder][nIndex].b_dirty = 0;
			L2Map[L2RowDecoder][nIndex].b_valid = 1;
			L2Map[L2RowDecoder][nIndex].prefetch = 0;
			add_lru_info(&(L2Map[L2RowDecoder][nIndex].lru_time));
			print_dbg("|L2Put[tag=%llx, dirty=%d]", CurTag, L2Map[L2RowDecoder][nIndex].b_dirty);
			if (evict_blk) {
				print_dbg("|L2Evict(%llx)", evict_blk);
				if (bFlag)
					print_dbg("|L2WB(%llx)", evict_blk);
			}
		}
	} else /*Row being accessed for the first time*/
	{
		/*No eviction required since row is accessed the first time.  */
		p_stats->L2_read_misses++;
		print_dbg("|L2ReadMiss(%llx)", (address&(~L2Params.offset_mask)));
		for (nCnt = 0; nCnt < nWays; nCnt++) /*Adding row entries*/
			L2Map[L2RowDecoder].push_back(c_entry());
		/*Copy Block to L2, Don't mark as dirty as this is a read operation*/
		L2Map[L2RowDecoder][0].tag = CurTag;
		L2Map[L2RowDecoder][0].b_dirty = 0;
		L2Map[L2RowDecoder][0].b_valid = 1;
		L2Map[L2RowDecoder][0].prefetch = 0;
		add_lru_info(&(L2Map[L2RowDecoder][0].lru_time));
		print_dbg("|L2Put[tag=%llx, dirty=%d]", CurTag, L2Map[L2RowDecoder][0].b_dirty);
	}
	/*if (!hit)
	 {

	 uint64_t CurBlk = (address>>L2Params.offset_bits);

	 diff = (CurBlk) - (LastMBlk);
	 print_dbg("|dist=%lld,last_miss_block_addr=%llx,pending_stride=%lld", \
								diff, LastMBlk, pending_stride);

	 if (diff == pending_stride)Prefetch
	 Prefetch_Blocks(CurBlk, p_stats);

	 LastMBlk = CurBlk;
	 pending_stride = diff;
	 }*/
	if (!hit) {
		//uint64_t CurBlk = (address>>L2Params.offset_bits);
		Prefetch_Blocks(address, p_stats);
	}
	return L2_RowData;
}
/*L2 Write*/
void L2WriteReq(uint64_t address, cache_stats_t* p_stats) {
	uint64_t CurTag = 0;
	uint64_t L2RowDecoder = 0;
	uint64_t evict_blk = 0;
	L1CacheMap::iterator kpos;
	uint32_t nWays = 0;
	uint32_t nCnt = 0;
	uint32_t nIndex = 0;
	bool hit = 0;
	bool bFlag = 0;
	bool slotavailable = 0;

	//	p_stats->L2_accesses++;
	address = address & ~L1Params.offset_mask;
	L2RowDecoder = (address & (L2Params.index_mask)) >> L2Params.offset_bits;
	CurTag = (address & L2Params.tag_mask) >> (L2Params.index_bits
			+ L2Params.offset_bits);
	nWays = L2Params.nWays;

	if ((kpos = L2Map.find(L2RowDecoder)) != L2Map.end()) /*Row already present*/
	{
		for (nCnt = 0; nCnt < nWays; nCnt++) {
			if (CurTag == kpos->second[nCnt].tag) {
				print_dbg("|L2WriteHit(%llx)", (address&(~L2Params.offset_mask)));
				/*Cache HIT, Update timestamp, mark as dirty*/
				hit = 1;
				kpos->second[nCnt].b_dirty = 1;
				if (kpos->second[nCnt].prefetch)/*Check if block is a prefetched block*/
				{
					p_stats->successful_prefetches++;
					kpos->second[nCnt].prefetch = 0;
					if (kpos->second[nCnt].across_page){
						kpos->second[nCnt].across_page= 0;
						count_across_page_successful++;
					}
					//count_across_page_successful++;
					//kpos->second[nCnt].across_page = 0;
					print_dbg("|PrefetchSuccess(%llx)\n\n", (address&(~L2Params.offset_mask)));
				}
				add_lru_info(&kpos->second[nCnt].lru_time);
				break;
			} else if (!kpos->second[nCnt].tag)
				slotavailable = 1; /*Indicates no eviction required*/
		}
		if (!hit) /*L2 Cache Write MISS*/
		{
			p_stats->L2_write_misses++;
			print_dbg("|L2WriteMiss(%llx)", (address&(~L2Params.offset_mask)));
			/*Fetch from main memory, not required*/
			/*Check if eviction is required*/
			if (!slotavailable) {
				nIndex = find_lru_blk(&kpos->second, nWays);
				/*Write to memory if dirty*/
				evict_blk = ((L2Map[L2RowDecoder][nIndex].tag
						<< (L2Params.offset_bits + L2Params.index_bits))
						| (L2RowDecoder << L2Params.offset_bits));
				if (L2Map[L2RowDecoder][nIndex].b_dirty) {
					p_stats->write_backs++;
					bFlag = 1;
				}
			} else /*Update index to next available slot*/
			{
				nIndex = 0;
				while (L2Map[L2RowDecoder][nIndex].tag != 0)
					nIndex++;
				if (nIndex >= nWays)
					print_dbg("ERROR IN LOGIC[%d]", nIndex);
			}
			/*Allocate Block L2 in next available slot,
			 * This is the copied block coming from memory(in theory). Update TS
			 * */
			L2Map[L2RowDecoder][nIndex].tag = CurTag;
			L2Map[L2RowDecoder][nIndex].b_dirty = 1;
			L2Map[L2RowDecoder][nIndex].b_valid = 1;
			L2Map[L2RowDecoder][nIndex].prefetch = 0;
			add_lru_info(&(L2Map[L2RowDecoder][nIndex].lru_time));
			if (evict_blk) {
				print_dbg("|L2Evict(%llx)", evict_blk);
				if (bFlag)
					print_dbg("|L2WB(%llx)", evict_blk);
			}
		}
	} else /*Row being accessed for the first time*/
	{
		/*No eviction required since row is accessed the first time.  */
		p_stats->L2_write_misses++;
		print_dbg("L2WriteMiss(%llx)|", (address&(~L2Params.offset_mask)));

		for (nCnt = 0; nCnt < nWays; nCnt++) /*Adding row entries*/
			L2Map[L2RowDecoder].push_back(c_entry());
		/*Copy Block to L2, Don't mark as dirty as this is a read operation*/
		L2Map[L2RowDecoder][0].tag = CurTag;
		L2Map[L2RowDecoder][0].b_dirty = 1;
		L2Map[L2RowDecoder][0].b_valid = 1;
		L2Map[L2RowDecoder][0].prefetch = 0;
		add_lru_info(&(L2Map[L2RowDecoder][0].lru_time));
		print_dbg("L2Put[tag=%llx, dirty=%d]|", CurTag, L2Map[L2RowDecoder][0].b_dirty);
	}
	if (!hit) {
		//uint64_t CurBlk = (address>>L2Params.offset_bits);
		Prefetch_Blocks(address, p_stats);
	}
}
/*Prefetch Blocks if stride matches*/
void Prefetch_Blocks(uint64_t address, cache_stats_t* p_stats) {
	uint64_t trigger = address;
	uint64_t prev_addr;
	uint64_t Curvirtaddr = 0;
	address = address >> L2Params.offset_bits;
	prev_addr = trigger;
#if IPT_PREFETCH && MAPPING

	std::map<uint64_t, virtual_frame>::iterator ipt_itr;
	uint64_t CurBlk = 0;
	uint64_t Blk2Invert = (trigger & ~((uint64_t)(page_size - 1)));//get physical page number

	ipt_itr = inverted_pg_table.find(Blk2Invert);
	if (ipt_itr == inverted_pg_table.end()) {
		/*For now don't prefetch at all*/
		/*TODO: Prefetch successful pages or use linked list*/
		printf("IPT Miss, size[%d]\n", inverted_pg_table.size());
		return;
	} else {
		Curvirtaddr = ipt_itr->second.vpf * page_size + (trigger & (page_size- 1));
		//Curvirtaddr = Blk2Invert;
		CurBlk = Curvirtaddr >> L2Params.offset_bits;
	}
	diff = (CurBlk) - (LastMBlk); //Virtual stride

#if !AGGR_PREFETCH
	if (diff != pending_stride)/*Don't Prefetch*/{
		LastMBlk = CurBlk;
		pending_stride = diff;
		return;
	}
#else
	if (!pending_stride){
		LastMBlk = CurBlk;
		pending_stride = diff;
		return;
	}
#endif

	
	
			
#else
	uint64_t CurBlk = address;

	diff = (CurBlk) - (LastMBlk);
	print_dbg("|dist=%lld,last_miss_block_addr=%llx,pending_stride=%lld",
			diff, LastMBlk, pending_stride);
#if !AGGR_PREFETCH
	if (diff != pending_stride) {/*Prefetch*/
		LastMBlk = CurBlk;
		pending_stride = diff;
		return;
	}
#else
	if (!pending_stride){
		LastMBlk = CurBlk;
		pending_stride = diff;
		return;
	}
#endif
#endif
	print_dbg("trigger=%llx,CurBlk = %llx, dist=%lld,last_miss_block_addr=%llx,pending_stride=%lld\n",trigger, CurBlk,diff,LastMBlk, pending_stride);
	for (int i = 1; i <= k_prefetch; i++) {
		uint64_t L2RowDecoder = 0, curr_addr = 0;
		
		uint64_t evict_blk = 0;
		uint64_t CurTag = 0;
		L1CacheMap::iterator kpos;
		uint32_t nWays = 0;
		uint32_t nCnt = 0;
		uint32_t nIndex = 0;
		bool hit = 0;
		bool bFlag = 0;
		bool slotavailable = 0;
		bool across_pg = 0;

#if IPT_PREFETCH && MAPPING

		std::map<uint64_t, physical_frame_LRU>::iterator pt_itr;
		Curvirtaddr = Curvirtaddr + (pending_stride << L2Params.offset_bits);
		pt_itr = page_table.find(Curvirtaddr/page_size);
		/*TODO: check in IPT also*/
		if (pt_itr == page_table.end()) {
			/*For now don't prefetch at all*/
			conversion(Curvirtaddr);
			pt_itr = page_table.find(Curvirtaddr/page_size);
			p_stats->TLB_Misses++;

			/*LastMBlk = CurBlk;
			 pending_stride = diff;
			 return;*/
		}/*else{
		 address = (pt_itr->second.ppf + (Curvirtaddr & L2Params.offset_mask)) >> L2Params.offset_bits;
		 }
		 */
		//address = (pt_itr->second.ppf + (Curvirtaddr & L2Params.offset_mask)) >> L2Params.offset_bits;
		address = (pt_itr->second.ppf + (Curvirtaddr & (page_size- 1))) >> L2Params.offset_bits;
#else
		address = address + pending_stride;
#endif
		print_dbg("Prefetching %llx <VA [%llx]>, count %lld TraceLine[%d]\n", address, Curvirtaddr, p_stats->prefetched_blocks, count);
		curr_addr = address << L2Params.offset_bits;
		
		if (prev_addr / 4096 != curr_addr / 4096) {
#if IPT_PREFETCH
				std::map<uint64_t, virtual_frame>::iterator ipt_itr_temp;		
				count_across_pages++;	
				ipt_itr_temp = inverted_pg_table.find(prev_addr & ~((uint64_t)(page_size - 1)));
				prev_addr = curr_addr;
				if(ipt_itr_temp->second.counter==0)
				{
					ipt_itr_temp->second.counter++;
					if(ipt_itr_temp->second.counter > MAX_COUNTER_SIZE)
						ipt_itr_temp->second.counter = MAX_COUNTER_SIZE;
					break;
				}
				else
				{
					ipt_itr_temp->second.counter++;
					if(ipt_itr_temp->second.counter > MAX_COUNTER_SIZE)
						ipt_itr_temp->second.counter = MAX_COUNTER_SIZE;
				}
			
#endif							//first time access
			printf("Pending stride: %d \n", pending_stride);
			printf("Prefetching %llx <VA [%llx]>, count %lld TraceLine[%d]\n", address, Curvirtaddr, p_stats->prefetched_blocks, count);
			
			across_pg = 1;
			//printf("Pending stride %lu last miss address %lu address %lu... prefetch from %lu TO %lu \n", pending_stride, temp, address, temp/4096, block_address(address)/4096);
		}
		L2RowDecoder = (address & (L2Params.index_mask >> L2Params.offset_bits));
		nWays = L2Params.nWays;
		CurTag = (address & (L2Params.tag_mask >> L2Params.offset_bits)) >> (L2Params.index_bits);

		if ((kpos = L2Map.find(L2RowDecoder)) != L2Map.end()) /*Row already present*/
		{
			for (nCnt = 0; nCnt < nWays; nCnt++) {
				if (CurTag == kpos->second[nCnt].tag) {
					/*Cache HIT, Don't do anything*/
					hit = 1;
					break;
				} else if (!kpos->second[nCnt].tag)
					slotavailable = 1; /*Indicates no eviction required*/
			}
			if (!hit) /*L2 Cache Prefetch Read MISS*/
			{
				uint32_t itr = 0;
				p_stats->prefetched_blocks++;
				/*Find LRU*/
				for (nCnt = 0; nCnt < nWays; nCnt++) {/*Prefetched blks are given timestamp values from 0 to 2^s2*/
					if (kpos->second[nCnt].prefetch)
						;
					itr++;
				}
				if (!slotavailable)/*Check if eviction is required*/
				{
					nIndex = find_lru_blk(&kpos->second, nWays);
					evict_blk = ((L2Map[L2RowDecoder][nIndex].tag << (L2Params.offset_bits + L2Params.index_bits))
							| (L2RowDecoder << L2Params.offset_bits));
					/*Write to memory if dirty*/
					if (L2Map[L2RowDecoder][nIndex].b_dirty) {
						p_stats->write_backs++;
						bFlag = 1;
					}
				} else /*Update index to next available slot*/
				{
					nIndex = 0;
					while (L2Map[L2RowDecoder][nIndex].tag != 0)
						nIndex++;
				}
				L2Map[L2RowDecoder][nIndex].tag = CurTag;
				L2Map[L2RowDecoder][nIndex].b_dirty = 0;
				L2Map[L2RowDecoder][nIndex].b_valid = 1;
				L2Map[L2RowDecoder][nIndex].prefetch = 1;
				L2Map[L2RowDecoder][nIndex].lru_time = itr;

				if (across_pg) {
					L2Map[L2RowDecoder][nIndex].across_page = 1;
				} print_dbg("|Prefetch(%llx)", address<<L2Params.offset_bits);

				if (evict_blk) {
					print_dbg("|L2Evict(%llx)", evict_blk);
					if (bFlag)
						print_dbg("|L2WB(%llx)", evict_blk);
				} print_dbg("|L2Put[tag=%llx, dirty=%d]", CurTag, L2Map[L2RowDecoder][nIndex].b_dirty);
			}
			else{
				print_dbg("Prefetching block that's already present\n\n");
			}
		} else {
			for (nCnt = 0; nCnt <= nWays; nCnt++) /*Adding row entries*/
				L2Map[L2RowDecoder].push_back(c_entry());
			L2Map[L2RowDecoder][0].tag = CurTag;
			L2Map[L2RowDecoder][0].b_dirty = 0;
			L2Map[L2RowDecoder][0].b_valid = 1;
			L2Map[L2RowDecoder][0].prefetch = 1;
			L2Map[L2RowDecoder][0].lru_time = 0;
			if (across_pg) {
				L2Map[L2RowDecoder][nIndex].across_page = 1;
			} print_dbg("|Prefetch(%llx)", address<<L2Params.offset_bits);
			p_stats->prefetched_blocks++;
			print_dbg("L2Put[tag=%llx, dirty=%d]|", CurTag, L2Map[L2RowDecoder][0].b_dirty);
		}
	}
	LastMBlk = CurBlk;
	pending_stride = diff;
}
/*This is made as a function in case of future changes in LRU implementation*/
void add_lru_info(uint64_t* lru_time) {
	*lru_time = ++glrutime;
}
/*Find LRU block for given array*/
uint32_t find_lru_blk(std::vector<c_entry> *m_entry, uint32_t nWays) {

	uint64_t s_min = 0;
	uint32_t nIndex = 0;
	uint32_t nCnt = 0;

	s_min = m_entry->at(0).lru_time;
	/*Find LRU*/
	for (nCnt = 1; nCnt < nWays; nCnt++) {
		if (s_min >= m_entry->at(nCnt).lru_time) {
			s_min = m_entry->at(nCnt).lru_time;
			nIndex = nCnt;
		}
	}
	return nIndex;
}
/**
 * Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
 * such as miss rate or average access time.
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_cache(cache_stats_t *p_stats) {
	double AAT, MP1, MP2, HT1, HT2, MP1_mod;
	double MissRate_L1 = 0, MissRate_L2 = 0, MR_L2_TLB = 0;

	MissRate_L1 = (double) ((double) (p_stats->L1_write_misses
			+ p_stats->L1_read_misses) / (double) p_stats->L1_accesses);
	MissRate_L2 = (double) ((double) p_stats->L2_read_misses
			/ ((double) (p_stats->L1_write_misses + p_stats->L1_read_misses)));
	MR_L2_TLB = (double) ((double) (p_stats->L2_read_misses + p_stats->TLB_Misses)
				/ ((double) (p_stats->L1_write_misses + p_stats->L1_read_misses)));
	HT1 = (double) ((double) 2 + (double) (0.2 * (double) gs1));
	HT2 = (double) ((double) 4 + (double) (0.4 * (double) gs2));
	MP2 = 500;
	MP1 = HT2 + MissRate_L2 * MP2;
	MP1_mod = HT2 + MR_L2_TLB * MP2;
	AAT = (double) HT1 + (double) (MissRate_L1 * MP1);
	p_stats->modified_AAT = (double) HT1 + (double) (MissRate_L1 * MP1_mod);
	p_stats->avg_access_time = AAT;
}

