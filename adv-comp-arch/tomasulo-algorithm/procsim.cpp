#include "procsim.hpp"
#include <iostream>
#include <cstring>

#define nFU_TYPE	3
#define nREGS		32
//#define DEBUG
#define OUTPUT

#ifdef DEBUG
#define	print_dbg(fmt, arg...)  printf(fmt, ## arg);
#else
#define	print_dbg(fmt, arg...)
#endif
#ifdef OUTPUT
#define	print_out(fmt, arg...) 	printf(fmt, ## arg);
#else
#define	print_out(fmt, arg...)
#endif

uint32_t R = 0, F = 0, M = 0, K0 = 0, K1 = 0, K2 = 0 ;
uint64_t gline = 0, total_cycles = 0;

typedef struct rob_ent_st{
	uint64_t line;
	uint64_t completion_cycle;
	bool retire;
}rob_ent_t;

struct reorder_buffer{
	uint32_t head;
	uint32_t tail;
	uint32_t num_used;
	rob_ent_t* rob_ent;
}rob;

typedef struct print_ent_st{
	uint32_t fetch;
	uint32_t disp;
	uint32_t sch;
	uint32_t ex;
	uint32_t state;
}print_ent_t;

struct print_buffer{
	uint32_t size;
	print_ent_t* print_ent;
}buf;

struct reg_file_entry{
	uint64_t tag;
	bool busy;
}reg[nREGS];

typedef struct dispatchq_entry{
	uint64_t line;
	int32_t fu_type;
	int32_t dest_reg;
	int32_t s1_reg;
	int32_t s2_reg;
	bool disp_done;
	bool stall;
	bool inROB;
}dispq_entry_t;

/*Global*/
struct dispatch_q{
	uint32_t num_used;
	dispq_entry_t* disp_ent;
}dispq;

typedef struct scheduler_entry{
	uint64_t line;
	int32_t dest_reg;
	uint32_t dest_tag;
	uint32_t s1_tag;
	uint32_t s2_tag;
	bool s1_busy;
	bool s2_busy;
	bool rdy_to_fire;
	bool complete;
	bool free; 			/*This is to indicate that entry should be deleted*/
	bool busy;
}sch_entry_t;

struct sched_queue_t{
	uint32_t num_used;
	uint32_t tail;
	uint32_t size;
	sch_entry_t* sched_ent;
}sched_q[nFU_TYPE];

typedef struct fu_pipeline{
	uint64_t line;
	int32_t dest_reg;	/*For CDB broadcast*/
	uint32_t dest_tag;
	bool busy;
}fu_stages_t;

typedef struct fu_entry{
	fu_stages_t* fu_stg;
}fu_entry_t;

struct function_unit{
	uint32_t num_used;
	uint32_t size;
	fu_entry_t* fu_ent;
}fu[nFU_TYPE];

volatile bool bEOF = 0;
void dispatch_first_half();
void dispatch_second_half();
void schedule_first_half();
void schedule_second_half();
void execute();
void stateupdate_first_half();
void stateupdate_second_half(proc_stats_t* p_stats);
void add_to_rob(uint64_t line);
void mark_for_del(uint64_t line);
void delete_from_rob(proc_stats_t* p_stats);
void fetch();
/**
 * Subroutine for initializing the processor. You many add and initialize any global or heap
 * variables as needed.
 * XXX: You're responsible for completing this routine
 *
 * @r ROB size
 * @k0 Number of k0 FUs
 * @k1 Number of k1 FUs
 * @k2 Number of k2 FUs
 * @f Number of instructions to fetch
 * @m Schedule queue multiplier
 */
void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f, uint64_t m) {
	R = r;
	M = m;
	K0 = k0;
	K1 = k1;
	K2 = k2;
	F = f;
	dispq.disp_ent = (dispq_entry_t*)calloc(1, sizeof(dispq_entry_t)*R);/*Dispatch Queue allocate*/
	rob.rob_ent = (rob_ent_t*)calloc(1, sizeof(rob_ent_t)*R);/*ROB allocate*/
	dispq.num_used = rob.num_used = rob.head = rob.tail = 0;
	buf.size = R*2;/*Print buffer size. At any point, no more than 2*R instructions need to be traced.*/
	buf.print_ent = (print_ent_t*)calloc(1, sizeof(print_ent_t)*buf.size);
	sched_q[0].sched_ent = (sch_entry_t*)calloc(1, sizeof(sch_entry_t)*K0*M);/*Schedule Queue allocate*/
	sched_q[0].size = K0*M;
	sched_q[1].sched_ent = (sch_entry_t*)calloc(1, sizeof(sch_entry_t)*K1*M);
	sched_q[1].size = K1*M;
	sched_q[2].sched_ent = (sch_entry_t*)calloc(1, sizeof(sch_entry_t)*K2*M);
	sched_q[2].size = K2*M;
	fu[0].fu_ent = (fu_entry_t*)calloc(1, sizeof(fu_entry_t)*K0);/*FU allocate*/
	fu[0].size = K0;
	fu[1].fu_ent = (fu_entry_t*)calloc(1, sizeof(fu_entry_t)*K1);
	fu[1].size = K1;
	fu[2].fu_ent = (fu_entry_t*)calloc(1, sizeof(fu_entry_t)*K2);
	fu[2].size = K2;
	for (uint32_t i = 0; i < K0; i++)
		fu[0].fu_ent[i].fu_stg = (fu_stages_t*)calloc(1, sizeof(fu_stages_t)*1);
	for (uint32_t i = 0; i < K1; i++)
		fu[1].fu_ent[i].fu_stg = (fu_stages_t*)calloc(1, sizeof(fu_stages_t)*2);
	for (uint32_t i = 0; i < K2; i++)
		fu[2].fu_ent[i].fu_stg = (fu_stages_t*)calloc(1, sizeof(fu_stages_t)*3);
	print_out("INST\tFETCH\tDISP\tSCHED\tEXEC\tSTATE\tRETIRE");
}

/**
 * Subroutine that simulates the processor.x
 *   The processor should fetch instructions as appropriate, until all instructions have executed
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 *
 */
void run_proc(proc_stats_t* p_stats)
{
	while (!bEOF || rob.num_used)
	{
		total_cycles = ++p_stats->cycle_count;
		stateupdate_first_half();
		execute();
		schedule_first_half();
		dispatch_first_half();
		dispatch_second_half();
		fetch();
		stateupdate_second_half(p_stats);
		schedule_second_half();
	}
	print_out("\n\n");
}
void stateupdate_first_half()
{
	for (uint32_t i = 0; i < nFU_TYPE; i++)
	{
		for (uint32_t j =0; j < sched_q[i].num_used; j++)
			if (sched_q[i].sched_ent[j].complete)
			{
				print_dbg("\n%lld\tSTATE UPDATE\t%lld", total_cycles, sched_q[i].sched_ent[j].line);
				buf.print_ent[(sched_q[i].sched_ent[j].line-1)%buf.size].state = total_cycles;
				sched_q[i].sched_ent[j].free = 1;
				sched_q[i].sched_ent[j].complete = 0;
				mark_for_del(sched_q[i].sched_ent[j].line);
			}
	}
}
void execute()
{
	for (uint32_t i = 0; i < nFU_TYPE; i++)
	{
		for (uint32_t j=0; j < fu[i].size; j++)
		{
			if (fu[i].fu_ent[j].fu_stg[i].busy)
			{/*1. Update register file 2. Delete fu entry 3. mark schq as complete*/
				if (fu[i].fu_ent[j].fu_stg[i].dest_reg >= 0)
					if (reg[fu[i].fu_ent[j].fu_stg[i].dest_reg].tag == fu[i].fu_ent[j].fu_stg[i].dest_tag)/*Update future file here*/
						reg[fu[i].fu_ent[j].fu_stg[i].dest_reg].busy = 0;
				for (uint32_t p =0; p < nFU_TYPE; p++)
				{
					for (uint32_t k =0; k < sched_q[p].num_used; k++)
					{
						if (sched_q[p].sched_ent[k].line == fu[i].fu_ent[j].fu_stg[i].line)
							sched_q[p].sched_ent[k].complete = 1;
						if (sched_q[p].sched_ent[k].s1_tag == fu[i].fu_ent[j].fu_stg[i].dest_tag)
							sched_q[p].sched_ent[k].s1_busy = 0;
						if (sched_q[p].sched_ent[k].s2_tag == fu[i].fu_ent[j].fu_stg[i].dest_tag)
							sched_q[p].sched_ent[k].s2_busy = 0;
					}
				}
				memset(&fu[i].fu_ent[j].fu_stg[i], 0, sizeof(fu_stages_t));/*Set to zero*/
			}/*Move instructions to next stage*/
			if ((i == 2) && (fu[i].fu_ent[j].fu_stg[1].busy))
			{
				fu[i].fu_ent[j].fu_stg[2] = fu[i].fu_ent[j].fu_stg[1];
				memset(&fu[i].fu_ent[j].fu_stg[1], 0, sizeof(fu_stages_t));/*Set to zero*/
			}
			if (fu[i].fu_ent[j].fu_stg[0].busy)/*Move to next stage*/
			{/*Make this generic*/
				fu[i].fu_ent[j].fu_stg[1] = fu[i].fu_ent[j].fu_stg[0];
				memset(&fu[i].fu_ent[j].fu_stg[0], 0, sizeof(fu_stages_t));/*Set to zero*/
			}

		}
	}
}

void schedule_first_half()
{
	for (uint32_t i = 0; i < nFU_TYPE; i++)
	{
		uint32_t tail = 0;
		for (uint32_t j = 0; j < sched_q[i].num_used; j++)
		{
			if (sched_q[i].sched_ent[j].rdy_to_fire)
			{/*Adding new entry in FU at the tail*/
				print_dbg("\n%lld\tSCHEDULED\t%lld", total_cycles, sched_q[i].sched_ent[j].line);
				buf.print_ent[(sched_q[i].sched_ent[j].line-1)%buf.size].ex = total_cycles+1;
				fu[i].fu_ent[tail].fu_stg[0].line = sched_q[i].sched_ent[j].line;
				fu[i].fu_ent[tail].fu_stg[0].dest_reg = sched_q[i].sched_ent[j].dest_reg;
				if (sched_q[i].sched_ent[j].dest_reg >= 0)
					fu[i].fu_ent[tail].fu_stg[0].dest_tag = sched_q[i].sched_ent[j].dest_tag;
				fu[i].fu_ent[tail].fu_stg[0].busy = 1;
				sched_q[i].sched_ent[j].rdy_to_fire = 0;
				sched_q[i].sched_ent[j].busy = 1;
				tail++;
			}
		}
		fu[i].num_used = 0; /*We can be sure that in the next cycle all FU slots will be free*/
	}
}



void dispatch_first_half()
{
	uint32_t i = 0, cur_fu = 0;

	if (rob.num_used == R)
	{
		for (i = 0; i < dispq.num_used; i++) /*Set instructions as stall if there's no space in shedq*/
		{
			cur_fu = dispq.disp_ent[i].fu_type;
			if ((dispq.disp_ent[i].inROB) && sched_q[cur_fu].num_used < sched_q[cur_fu].size)
			{
				dispq.disp_ent[i].stall = 0;
				sched_q[cur_fu].num_used++;
			}
			else
			{
				dispq.disp_ent[i].stall = 1;
				while (++i < dispq.num_used)/*Stall the instructions which cannot fit into the ROB*/
					dispq.disp_ent[i].stall = 1;
				break;
			}
		}
		return;
	}
	else /*Remove stalls if schedule queue is free*/
	{
		for (i = 0; i < dispq.num_used; i++) /*Set all active instructions as stall if SchQ is busy*/
		{
			if ((rob.num_used) < R)
			{
				if (dispq.disp_ent[i].inROB == 0)
				{
					add_to_rob(dispq.disp_ent[i].line);
					dispq.disp_ent[i].inROB = 1;
				}
				cur_fu = dispq.disp_ent[i].fu_type;
				if (sched_q[cur_fu].num_used == sched_q[cur_fu].size)
				{
					dispq.disp_ent[i].stall = 1;
					while (++i < dispq.num_used)/*Stall the instructions which cannot fit into the ROB*/
						dispq.disp_ent[i].stall = 1;
					break;
				}
				else
				{
					dispq.disp_ent[i].stall = 0;
					sched_q[cur_fu].num_used++;
				}
			}
			else
			{
				while (i < dispq.num_used)/*Stall the instructions which cannot fit into the ROB*/
					dispq.disp_ent[i++].stall = 1;
				break;
			}
		}
	}/*Need to reserve slots in the ROB*/

}

void dispatch_second_half()
{
	uint32_t i = 0, cur_fu = 0;

	while (i < dispq.num_used)
	{	/*Iterate through all elements of dispatch queue.*/
		if (dispq.disp_ent[i].stall == 0)
		{  /*Add to schedule Q and ROB*/
			print_dbg("\n%lld\tDISPATCHED\t%lld", total_cycles, dispq.disp_ent[i].line);
			buf.print_ent[(dispq.disp_ent[i].line-1)%buf.size].sch = total_cycles+1;
			cur_fu = dispq.disp_ent[i].fu_type;
			uint32_t idx = sched_q[cur_fu].tail;
			sched_q[cur_fu].sched_ent[idx].line = dispq.disp_ent[i].line; //?required??
			if ((dispq.disp_ent[i].s1_reg >= 0) && reg[dispq.disp_ent[i].s1_reg].busy)
			{
				sched_q[cur_fu].sched_ent[idx].s1_tag = reg[dispq.disp_ent[i].s1_reg].tag;
				sched_q[cur_fu].sched_ent[idx].s1_busy = 1;
			}
			else /*This is similar to taking only the register value.*/
				sched_q[cur_fu].sched_ent[idx].s1_busy = 0;
			if ((dispq.disp_ent[i].s2_reg >= 0) && reg[dispq.disp_ent[i].s2_reg].busy)
			{
				sched_q[cur_fu].sched_ent[idx].s2_tag = reg[dispq.disp_ent[i].s2_reg].tag;
				sched_q[cur_fu].sched_ent[idx].s2_busy = 1;
			}
			else /*This is similar to taking only the register value.*/
				sched_q[cur_fu].sched_ent[idx].s2_busy = 0;
			if (dispq.disp_ent[i].dest_reg >= 0)
			{
				sched_q[cur_fu].sched_ent[idx].dest_reg = dispq.disp_ent[i].dest_reg;
				sched_q[cur_fu].sched_ent[idx].dest_tag = dispq.disp_ent[i].line;
				reg[dispq.disp_ent[i].dest_reg].tag = dispq.disp_ent[i].line;
				reg[dispq.disp_ent[i].dest_reg].busy = 1;
			}
			dispq.disp_ent[i].disp_done = 1;
			sched_q[cur_fu].tail++;
			i++;
		}
		else
			i++;
	}/*Delete dispatch entries*/
	i = 0;
	while (i < dispq.num_used)
	{
		if (dispq.disp_ent[i].disp_done)
		{
			uint32_t j;
			for (j = i; j < (dispq.num_used-1); j++)/*Shift entries upwards*/
			{
				dispq.disp_ent[j] = dispq.disp_ent[j+1];
			}/*Reset value of last entry since everything has been moved up by one*/
			memset(&dispq.disp_ent[dispq.num_used-1], 0, sizeof(dispq_entry_t));
			dispq.num_used--;
		}
		else/*Since we are anyway shifting the entries up, we only need to move to next entry if there is no delete*/
			i++;
	}
}

void fetch()
{
	uint32_t fetchWidth = F;
	while (fetchWidth && (dispq.num_used < R))
	{
		proc_inst_t instr;
		int ret = read_instruction(&instr);
		if(ret)
		{
			dispq.disp_ent[dispq.num_used].s1_reg = instr.src_reg[0];
			dispq.disp_ent[dispq.num_used].s2_reg = instr.src_reg[1];
			dispq.disp_ent[dispq.num_used].dest_reg = instr.dest_reg;
			if (instr.op_code >= 0)	dispq.disp_ent[dispq.num_used].fu_type = instr.op_code;
			else	dispq.disp_ent[dispq.num_used].fu_type = 0;
			dispq.disp_ent[dispq.num_used].line = ++gline;
			print_dbg("\n%lld\tFETCHED\t%lld", total_cycles, gline);
			buf.print_ent[(gline-1)%buf.size].fetch = total_cycles;
			buf.print_ent[(gline-1)%buf.size].disp = total_cycles+1;
			dispq.num_used++;
			fetchWidth--;
		}
		else
		{
			bEOF = 1;
			break;
		}
	}
}

void stateupdate_second_half(proc_stats_t* p_stats)
{
	uint32_t j = 0, k = 0;
	for (uint32_t i = 0; i < nFU_TYPE; i++)
	{
		j = 0;
		while (j < sched_q[i].num_used)
		{
			if (sched_q[i].sched_ent[j].free)/*Delete from sched q*/
			{
				for (k = j; k < (sched_q[i].num_used-1); k++)
					sched_q[i].sched_ent[k] = sched_q[i].sched_ent[k+1];
				memset(&sched_q[i].sched_ent[sched_q[i].num_used-1], 0, sizeof(sch_entry_t));
				sched_q[i].tail--;
				sched_q[i].num_used--;
			}
			else
				j++;
		}
	}
	delete_from_rob(p_stats);
}

void schedule_second_half()
{
	for (uint32_t i = 0; i < nFU_TYPE; i++)
		for (uint32_t j = 0; j < sched_q[i].num_used; j++)
			if ((!sched_q[i].sched_ent[j].busy) && (sched_q[i].sched_ent[j].s1_busy == 0) \
					&& (sched_q[i].sched_ent[j].s2_busy == 0) && (fu[i].num_used < fu[i].size))
			{/*Set as ready to fire*/
				sched_q[i].sched_ent[j].rdy_to_fire = 1;
				fu[i].num_used++;
			}
}

void add_to_rob(uint64_t line)
{
	uint32_t tail = rob.tail;
	rob.rob_ent[tail].line = line;
	rob.tail = (rob.tail + 1) % R;
	rob.num_used++;
}
void mark_for_del(uint64_t line)
{
	uint32_t head = rob.head;
	uint32_t i = 0;

	while (i++ < rob.num_used)/*Check if any entry needs to be retired this cycle*/
	{
		if(rob.rob_ent[head].line == line)
			rob.rob_ent[head].completion_cycle = total_cycles;
		head = (head + 1) % R;
	}
}
void delete_from_rob(proc_stats_t* p_stats)
{
	uint32_t fetchWidth = F;
	uint32_t head = rob.head;
	uint32_t i =0;

	while (i++ < rob.num_used)
	{
		if ((rob.rob_ent[head].completion_cycle == (total_cycles - 1)) && (rob.rob_ent[head].completion_cycle))
			rob.rob_ent[head].retire = 1;
		head = (head + 1) % R;
	}
	while (fetchWidth && rob.rob_ent[rob.head].retire)
	{
		print_dbg("\n%lld\tRETIRED\t%lld", total_cycles, rob.rob_ent[rob.head].line);
		print_out("\n%lld\t%d\t%d\t%d\t%d\t%d\t%lld", rob.rob_ent[rob.head].line, buf.print_ent[(rob.rob_ent[rob.head].line-1)%buf.size].fetch,buf.print_ent[(rob.rob_ent[rob.head].line-1)%buf.size].disp,buf.print_ent[(rob.rob_ent[rob.head].line-1)%buf.size].sch,buf.print_ent[(rob.rob_ent[rob.head].line-1)%buf.size].ex,buf.print_ent[(rob.rob_ent[rob.head].line-1)%buf.size].state,total_cycles);
		memset(&rob.rob_ent[rob.head], 0, sizeof(rob_ent_t));
		rob.head = (rob.head + 1) % R;
		rob.num_used--;
		fetchWidth--;
		p_stats->retired_instruction++;
	}
}
/**
 * Subroutine for cleaning up any outstanding instructions and calculating overall statistics
 * such as average IPC or branch prediction percentage
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_proc(proc_stats_t *p_stats) {
		p_stats->avg_inst_retired = (float)p_stats->retired_instruction / (float)p_stats->cycle_count;
		free(rob.rob_ent);
		for (uint32_t i = 0; i < K0; i++)
			free(fu[0].fu_ent[i].fu_stg);
		for (uint32_t i = 0; i < K1; i++)
			free(fu[1].fu_ent[i].fu_stg);
		for (uint32_t i = 0; i < K2; i++)
			free(fu[2].fu_ent[i].fu_stg);
		for (uint32_t i=0; i < nFU_TYPE; i++){
		free(sched_q[i].sched_ent);
		free(fu[i].fu_ent);
		}
		free(buf.print_ent);
}
