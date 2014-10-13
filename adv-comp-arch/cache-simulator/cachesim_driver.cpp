#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <time.h>
#include <cstring>
#include <unistd.h>
#include <math.h>
#include "cachesim.hpp"

#define physical_address_size 1024*1024*1024*4
#define page_size 4096
#define page_table_size 1048576
#define inverted_pt_size 1048576
#define mask 0x00000000FFFFFFFF



#include<map>

//#include<conio>
int count_num_pages;
int count_across_pages;
int count_across_page_successful;
uint64_t count = 0;
pf* physical_frame;
std::map<uint64_t, physical_frame_LRU> page_table; //This is the page table
std::map<uint64_t, virtual_frame> inverted_pg_table;

uint64_t actual_table_size;
table actual_table[MAX_TABLE_SIZE];

void initialize() {

	uint64_t i = 0;
	uint64_t temp = 0;
	physical_frame = (pf*) malloc(sizeof(pf) * page_table_size); //long(physical_address_size/page_size));

	for (i = 0; i < page_table_size; i++) //Basic initialization
	{
		physical_frame[i].frame_number = temp;
		temp += page_size;
		physical_frame[i].free = 0;
	}
	
	for(i=0;i<MAX_TABLE_SIZE;i++)
	{
		actual_table[i].pfn = 0;
		actual_table[i].counter = 0;
	}
}

//void debug_print();
uint64_t conversion(uint64_t address) {
	uint64_t i;
	//srand(time(NULL));
	std::map<uint64_t, physical_frame_LRU>::iterator itr;
	std::map<uint64_t, physical_frame_LRU>::iterator itr2, temp_iterator;
	std::map<uint64_t, virtual_frame>::iterator ipt_itr3, min_ipt_val;
	uint64_t vpf, offset;

	physical_frame_LRU pfL;
	virtual_frame vframe;

	vpf = address / page_size; //getting virtual page number
	offset = address % page_size; //getting offset

	itr = page_table.find(vpf); //indexing into table
	if (itr == page_table.end()) {
		if (page_table.size() == page_table_size) //here i am assuming the page table can hold 4 entries
		{
			printf("SIZE REACHED EVICTING A PAGE \n");
			count_num_pages--;
			temp_iterator = page_table.begin();

			for (itr2 = page_table.begin(); itr2 != page_table.end(); itr2++) //evicting page which is LRU
			{
				if (itr2->second.time_stamp < temp_iterator->second.time_stamp)
					temp_iterator = itr2;
			}
			physical_frame[temp_iterator->second.i].free = 0; //Free-ing the page frame the evicted page was mapped to
			page_table.erase(temp_iterator);
		}

		int random_count = 0;
		while (random_count != 1000) {
			i = rand() % page_table_size;
			if (physical_frame[i].free == 0)
				break;
			else
				random_count++;
		}

		if (random_count == 1000) {
			for (i = 0; i < page_table_size; i++) //1024*1024 because i initially assumed 4 GB divided by 4KB				//mapping a virtual page number to physical page number
			{
				if (physical_frame[i].free == 0) {
					break;
				}

			}
		}
		pfL.ppf = physical_frame[i].frame_number;
		pfL.time_stamp = count;
		pfL.i = i;
		physical_frame[i].free = 1;
		count_num_pages++;

		page_table.insert(std::pair<uint64_t, physical_frame_LRU>(vpf, pfL));
		itr = page_table.find(vpf);

		ipt_itr3 = inverted_pg_table.find(itr->second.ppf);
		if (ipt_itr3 == inverted_pg_table.end()) {
			if (inverted_pg_table.size() == inverted_pt_size) //here i am assuming the page table can hold 4 entries
			{
				min_ipt_val = inverted_pg_table.begin();
#if IPT_LRU
				for (ipt_itr3 = inverted_pg_table.begin(); ipt_itr3 != inverted_pg_table.end(); ipt_itr3++) //evicting page which is LRU
				{
					if (ipt_itr3->second.time_stamp
							< min_ipt_val->second.time_stamp)
						min_ipt_val = ipt_itr3;
				}
#else//Eviction based on prefetch counter
				for (ipt_itr3 = inverted_pg_table.begin(); ipt_itr3 != inverted_pg_table.end(); ipt_itr3++) //evicting page which is LRU
				{
					if (ipt_itr3->second.pref_strength
							< min_ipt_val->second.pref_strength)
						min_ipt_val = ipt_itr3;
				}
#endif
				inverted_pg_table.erase(min_ipt_val);
			}
			vframe.vpf = vpf;
			vframe.time_stamp = count;
			vframe.pref_strength = STRENGTH_START_VAL;
			vframe.counter = 0;
			inverted_pg_table.insert(std::pair<uint64_t, virtual_frame>(pfL.ppf, vframe));
		}
	} else {
		//printf("HIT \n");
		itr->second.time_stamp = count;
		ipt_itr3 = inverted_pg_table.find(itr->second.ppf);
		if (ipt_itr3 != inverted_pg_table.end()) {
			ipt_itr3->second.time_stamp = count;
		}
	}
	//itr = page_table.find(vpf);
	//std::cout<<"PHYSICAL PAGE FRAME "<<itr->second.ppf<<" OFFSET "<<offset<<"\n";
	//std::cout<<"PHYSICAL ADDRESS "<<itr->second.ppf + offset<<" LRU "<<itr->second.time_stamp<<" \n";

	return (itr->second.ppf + offset);
}
/*void debug_print()
 {
 std::map<int64_t, physical_frame_LRU>::iterator itr;

 std::cout<<"*******DEBUG PRINT******* \n";
 for(itr=page_table.begin();itr!=page_table.end();itr++)
 {
 std::cout<<"PHYSICAL FRAME "<<itr->second.ppf<<" LRU "<<itr->second.time_stamp<<" \n";
 }
 }*/

void print_help_and_exit(void) {
	printf("cachesim [OPTIONS] < traces/file.trace\n");
	printf("  -c C1\t\tTotal size of L1 in bytes is 2^C1\n");
	printf("  -b B1\t\tSize of each block in L1 in bytes is 2^B1\n");
	printf("  -s S1\t\tNumber of blocks per set in L1 is 2^S1\n");
	printf("  -C C2\t\tTotal size of L2 in bytes is 2^C2\n");
	printf("  -B B2\t\tSize of each block in L2 in bytes is 2^B2\n");
	printf("  -S S2\t\tNumber of blocks per set in L2 is 2^S2\n");
	printf("  -k K\t\tNumber of prefetch blocks\n");
	printf("  -h\t\tThis helpful output\n");
	exit(0);
}

void print_statistics(cache_stats_t* p_stats);

int main(int argc, char* argv[]) {
	int opt;
	uint64_t c1 = DEFAULT_C1;
	uint64_t b1 = DEFAULT_B1;
	uint64_t s1 = DEFAULT_S1;
	uint64_t c2 = DEFAULT_C2;
	uint64_t b2 = DEFAULT_B2;
	uint64_t s2 = DEFAULT_S2;
	uint32_t k = DEFAULT_K;

	FILE* fin = stdin;

	count_num_pages = 0;
	count_across_pages = 0;
	/* Read arguments */
	while (-1 != (opt = getopt(argc, argv, "c:b:s:C:B:S:k:i:h"))) {
		switch (opt) {
		case 'c':
			c1 = atoi(optarg);
			break;
		case 'b':
			b1 = atoi(optarg);
			break;
		case 's':
			s1 = atoi(optarg);
			break;
		case 'C':
			c2 = atoi(optarg);
			break;
		case 'B':
			b2 = atoi(optarg);
			break;
		case 'S':
			s2 = atoi(optarg);
			break;
		case 'k':
			k = atoi(optarg);
			break;
		case 'i':
			fin = fopen(optarg, "r");
			break;
		case 'h':
			/* Fall through */
		default:
			print_help_and_exit();
			break;
		}
	}

	printf("Cache Settings\n");
	printf("C1: %" PRIu64 "\n", c1);
	printf("B1: %" PRIu64 "\n", b1);
	printf("S1: %" PRIu64 "\n", s1);
	printf("C2: %" PRIu64 "\n", c2);
	printf("B2: %" PRIu64 "\n", b2);
	printf("S2: %" PRIu64 "\n", s2);
	printf("K: %" PRIu32 "\n", k);
	printf("\n");

	assert(c2 >= c1);
	assert(b2 >= b1);
	assert(s2 >= s1);
	assert(k >= 0 && k <= 4);

	/* Setup the cache */
	setup_cache(c1, b1, s1, c2, b2, s2, k);

	/* Setup statistics */
	cache_stats_t stats;
	memset(&stats, 0, sizeof(cache_stats_t));

	initialize();
	/* Begin reading the file */
	char rw;
	uint64_t address;
	while (!feof(fin)) {
		int ret = fscanf(fin, "%c %" PRIx64 "\n", &rw, &address);
		if (ret == 2) {
			count++;

			//printf("Converted addresses %lu \n", conversion(address));
#if MAPPING
			cache_access(rw, conversion(address), &stats);
#else
			cache_access(rw, address, &stats);
#endif
			//cache_access(rw, address , &stats);
		}
	}

	complete_cache(&stats);

	print_statistics(&stats);
	printf("NUMBER OF PAGES: %d \n", count_num_pages);
	printf("PREFETCHES ACROSS PAGES: %d \n", count_across_pages);
	printf("PREFETCHES ACROSS PAGES SUCCESSFUL: %d \n",
			count_across_page_successful);
	printf("RATIO OF PREFETCHES ACROSS PAGES %f \n", float(count_across_pages)/float(count_across_page_successful));

	/*for(int i=0;i<page_table_size;i++)	//1024*1024 because i initially assumed 4 GB divided by 4KB				//mapping a virtual page number to physical page number
	 {
	 if(physical_frame[i].free == 1)
	 {
	 printf("PAGE %d occupied---", i);
	 }
	 }*/
	return 0;
}

void print_statistics(cache_stats_t* p_stats) {
	printf("Cache Statistics\n");
	printf("Accesses: %" PRIu64 "\n", p_stats->accesses);
	printf("Reads: %" PRIu64 "\n", p_stats->reads);
	printf("L1 Read misses: %" PRIu64 "\n", p_stats->L1_read_misses);
	printf("L2 Read misses: %" PRIu64 "\n", p_stats->L2_read_misses);
	printf("Writes: %" PRIu64 "\n", p_stats->writes);
	printf("L1 Write misses: %" PRIu64 "\n", p_stats->L1_write_misses);
	printf("L2 Write misses: %" PRIu64 "\n", p_stats->L2_write_misses);
	printf("Write backs: %" PRIu64 "\n", p_stats->write_backs);
	printf("Evicted blocks: %" PRIu64 "\n", p_stats->evicted_blocks);
	printf("Prefetched blocks: %" PRIu64 "\n", p_stats->prefetched_blocks);
	printf("Successful prefetches: %" PRIu64 "\n",
			p_stats->successful_prefetches);
	printf("Average access time (AAT): %f\n", p_stats->avg_access_time);
	printf("Modified AAT): %f\n", p_stats->modified_AAT);

	printf(
			"RATIO: %f \n",
			float(p_stats->prefetched_blocks) / float(
					p_stats->successful_prefetches));

}

