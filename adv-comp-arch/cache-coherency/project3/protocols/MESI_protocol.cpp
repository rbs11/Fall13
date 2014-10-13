#include "MESI_protocol.h"
#include "../sim/mreq.h"
#include "../sim/sim.h"
#include "../sim/hash_table.h"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MESI_protocol::MESI_protocol (Hash_table *my_table, Hash_entry *my_entry)
    : Protocol (my_table, my_entry)
{
	 this->state = MESI_CACHE_I;
}

MESI_protocol::~MESI_protocol ()
{    
}

void MESI_protocol::dump (void)
{
    const char *block_states[] = {"X","I","S","E","M","IX","IM"};
    fprintf (stderr, "MESI_protocol - state: %s\n", block_states[state]);
}

void MESI_protocol::process_cache_request (Mreq *request)
{
	switch (state) {
	case MESI_CACHE_I:  do_cache_I (request); break;
	case MESI_CACHE_IX: do_cache_IX (request); break;
	case MESI_CACHE_IM: do_cache_IM (request); break;
	case MESI_CACHE_SM: do_cache_SM (request); break;
	case MESI_CACHE_M:  do_cache_M (request); break;
	case MESI_CACHE_E:  do_cache_E (request); break;
	case MESI_CACHE_S:  do_cache_S (request); break;
    default:
        fatal_error ("Invalid Cache State for MESI Protocol\n");
    }
}

void MESI_protocol::process_snoop_request (Mreq *request)
{
	switch (state) {

	case MESI_CACHE_I:  do_snoop_I (request); break;
	case MESI_CACHE_IM: do_snoop_IM (request); break;
	case MESI_CACHE_SM: do_snoop_SM (request); break;
	case MESI_CACHE_IX: do_snoop_IX (request); break;
	case MESI_CACHE_M:  do_snoop_M (request); break;
	case MESI_CACHE_E:  do_snoop_E (request); break;
	case MESI_CACHE_S:  do_snoop_S (request); break;
    default:
    	fatal_error ("Invalid Cache State for MESI Protocol\n");
    }
}

inline void MESI_protocol::do_cache_I (Mreq *request)
{
	switch (request->msg) {
	// If we get a request from the processor we need to get the data
	case STORE:
		send_GETM(request->addr);
		state = MESI_CACHE_IM;
		Sim->cache_misses++;
		break;
	case LOAD:
		/* Line up the GETM in the Bus' queue */
		send_GETS(request->addr);
		state = MESI_CACHE_IX;
		/* This is a cache miss */
		Sim->cache_misses++;
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
	}
}

inline void MESI_protocol::do_cache_S (Mreq *request)
{
	switch (request->msg) {
	case LOAD: /*Nothing else to be done*/
		send_DATA_to_proc(request->addr);
		break;
	case STORE:
		state = MESI_CACHE_SM;
		Sim->cache_misses++;
		send_GETM(request->addr);
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
	}
}

inline void MESI_protocol::do_cache_E (Mreq *request)
{
	switch (request->msg) {
	case LOAD: /*Nothing else to be done*/
		send_DATA_to_proc(request->addr);
		break;
	case STORE:
		/*Data need not be supplied because we are
		 * the only ones with this cache line*/
		state = MESI_CACHE_M;
		Sim->silent_upgrades++;
		send_DATA_to_proc(request->addr);
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
	}
}

inline void MESI_protocol::do_cache_IX (Mreq *request)
{
	request->print_msg (my_table->moduleID, "ERROR");
	fatal_error ("Client: I state shouldn't see this message\n");
}
inline void MESI_protocol::do_cache_IM (Mreq *request)
{
	request->print_msg (my_table->moduleID, "ERROR");
	fatal_error ("Client: I state shouldn't see this message\n");
}
inline void MESI_protocol::do_cache_SM (Mreq *request)
{
	request->print_msg (my_table->moduleID, "ERROR");
	fatal_error ("Client: I state shouldn't see this message\n");
}
inline void MESI_protocol::do_cache_M (Mreq *request)
{
	switch (request->msg) {
	case LOAD: /*Nothing else to be done*/
	case STORE:
		send_DATA_to_proc(request->addr);
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
	}
}
inline void MESI_protocol::do_snoop_I (Mreq *request)
{
	switch (request->msg) {
	case GETS:
	case GETM:
	case DATA:
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
	}
}

inline void MESI_protocol::do_snoop_IX (Mreq *request)
{
	switch (request->msg) {
	case GETS:
	case GETM: break;
	/*During this time if there already was an exclusive owner,
	we would know by checking the shared line*/
	case DATA:
		send_DATA_to_proc(request->addr);
		if (get_shared_line())
			state = MESI_CACHE_S;
		else /*Alternatively, this means that data has come from the memory*/
			state = MESI_CACHE_E;
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
	}
}

inline void MESI_protocol::do_snoop_IM (Mreq *request)
{
	switch (request->msg) {
		case GETS:
		case GETM:
			break;
		case DATA:
			send_DATA_to_proc(request->addr);
			state = MESI_CACHE_M;
			break;
		default:
	        request->print_msg (my_table->moduleID, "ERROR");
	        fatal_error ("Client: I state shouldn't see this message\n");
		}
}

inline void MESI_protocol::do_snoop_SM (Mreq *request)
{
	switch (request->msg) {
		case GETS:
		case GETM:
			if (request->src_mid != my_table->moduleID)
			{
				set_shared_line();
				state = MESI_CACHE_IM;
			}
			break;
		case DATA:
			send_DATA_to_proc(request->addr);
			state = MESI_CACHE_M;
			break;
		default:
	        request->print_msg (my_table->moduleID, "ERROR");
	        fatal_error ("Client: I state shouldn't see this message\n");
		}
}

inline void MESI_protocol::do_snoop_S (Mreq *request)
{
	switch (request->msg) {
		case GETS:
			set_shared_line();
			break;
		case GETM:
			state = MESI_CACHE_I;
			break;
		case DATA:
			break;
		default:
			request->print_msg (my_table->moduleID, "ERROR");
			fatal_error ("Client: I state shouldn't see this message\n");
		}
}

inline void MESI_protocol::do_snoop_E (Mreq *request)
{
	switch (request->msg) {
	case GETS:
		set_shared_line();
		send_DATA_on_bus(request->addr,request->src_mid);
		state = MESI_CACHE_S;
		break;
	case GETM:
		send_DATA_on_bus(request->addr,request->src_mid);
		state = MESI_CACHE_I;
		break;
	/*During this time if there already was an exclusive owner,
	we would know by checking the shared line*/
	case DATA:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
	}
}

inline void MESI_protocol::do_snoop_M (Mreq *request)
{
	switch (request->msg) {
	case GETS:
		set_shared_line();
		send_DATA_on_bus(request->addr,request->src_mid);
		state = MESI_CACHE_S;
		break;
	case GETM:
		//set_shared_line();
		send_DATA_on_bus(request->addr,request->src_mid);
		state = MESI_CACHE_I;
		break;
	case DATA:
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
	}
}

