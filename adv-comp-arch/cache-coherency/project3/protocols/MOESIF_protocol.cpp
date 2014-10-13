#include "MOESIF_protocol.h"
#include "../sim/mreq.h"
#include "../sim/sim.h"
#include "../sim/hash_table.h"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MOESIF_protocol::MOESIF_protocol (Hash_table *my_table, Hash_entry *my_entry)
    : Protocol (my_table, my_entry)
{
	this->state = MOESIF_CACHE_I;
}

MOESIF_protocol::~MOESIF_protocol ()
{    
}

void MOESIF_protocol::dump (void)
{
    const char *block_states[] = {"X","I","IS","IM","S","SM","E","O","OM","M","F","FM"};
    fprintf (stderr, "MOESIF_protocol - state: %s\n", block_states[state]);
}

void MOESIF_protocol::process_cache_request (Mreq *request)
{
	switch (state) {
	case MOESIF_CACHE_I:  do_cache_I (request); break;
	case MOESIF_CACHE_IM: do_cache_IM (request); break;
	case MOESIF_CACHE_IS:  do_cache_IS (request); break;
	case MOESIF_CACHE_S:  do_cache_S (request); break;
	case MOESIF_CACHE_SM: do_cache_SM (request); break;
	case MOESIF_CACHE_E: do_cache_E (request); break;
	case MOESIF_CACHE_O:  do_cache_O (request); break;
	case MOESIF_CACHE_OM:  do_cache_OM (request); break;
	case MOESIF_CACHE_M:  do_cache_M (request); break;
	case MOESIF_CACHE_F:  do_cache_F (request); break;
	case MOESIF_CACHE_FM:  do_cache_FM (request); break;
    default:
        fatal_error ("Invalid Cache State for MOESIF Protocol\n");
    }
}

void MOESIF_protocol::process_snoop_request (Mreq *request)
{
	switch (state) {
	case MOESIF_CACHE_I:  do_snoop_I (request); break;
	case MOESIF_CACHE_IM: do_snoop_IM (request); break;
	case MOESIF_CACHE_IS:  do_snoop_IS (request); break;
	case MOESIF_CACHE_S:  do_snoop_S (request); break;
	case MOESIF_CACHE_SM: do_snoop_SM (request); break;
	case MOESIF_CACHE_E: do_snoop_E (request); break;
	case MOESIF_CACHE_O:  do_snoop_O (request); break;
	case MOESIF_CACHE_OM:  do_snoop_OM (request); break;
	case MOESIF_CACHE_M:  do_snoop_M (request); break;
	case MOESIF_CACHE_F:  do_snoop_F (request); break;
	case MOESIF_CACHE_FM:  do_snoop_FM (request); break;
    default:
    	fatal_error ("Invalid Cache State for MOESIF Protocol\n");
    }
}

inline void MOESIF_protocol::do_cache_I (Mreq *request)
{
	switch (request->msg) {
	// If we get a request from the processor we need to get the data
	case STORE:
		send_GETM(request->addr);
		state = MOESIF_CACHE_IM;
		Sim->cache_misses++;
		break;
	case LOAD:
		send_GETS(request->addr);
		state = MOESIF_CACHE_IS;
		/* This is a cache miss */
		Sim->cache_misses++;
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
	}
}

inline void MOESIF_protocol::do_cache_S (Mreq *request)
{
	switch (request->msg) {
		case LOAD: /*Nothing else to be done*/
			send_DATA_to_proc(request->addr);
			break;
		case STORE:
			send_GETM(request->addr);
			state = MOESIF_CACHE_SM;
			Sim->cache_misses++;
			break;
		default:
			request->print_msg (my_table->moduleID, "ERROR");
			fatal_error ("Client: I state shouldn't see this message\n");
		}
}

inline void MOESIF_protocol::do_cache_O (Mreq *request)
{
	switch (request->msg) {
	case LOAD: /*Nothing else to be done*/
		send_DATA_to_proc(request->addr);
		break;
	case STORE:
		/*Data need not be supplied because we are
		 * the only ones with this cache line*/
		Sim->cache_misses++;
		send_GETM(request->addr);
		state = MOESIF_CACHE_FM;
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
	}
}

inline void MOESIF_protocol::do_cache_M (Mreq *request)
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
inline void MOESIF_protocol::do_cache_F (Mreq *request)
{
	switch (request->msg) {
	case LOAD: /*Nothing else to be done*/
		send_DATA_to_proc(request->addr);
		break;
	case STORE:
		send_GETM(request->addr);
		state = MOESIF_CACHE_FM;
		Sim->cache_misses++;
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
	}
}
inline void MOESIF_protocol::do_cache_SM (Mreq *request)
{
	request->print_msg (my_table->moduleID, "ERROR");
	fatal_error ("Client: SM state shouldn't see this message\n");
}
inline void MOESIF_protocol::do_cache_IM (Mreq *request)
{
	request->print_msg (my_table->moduleID, "ERROR");
	fatal_error ("Client: IM state shouldn't see this message\n");
}
inline void MOESIF_protocol::do_cache_IS (Mreq *request)
{
	request->print_msg (my_table->moduleID, "ERROR");
	fatal_error ("Client: IS state shouldn't see this message\n");
}
inline void MOESIF_protocol::do_cache_OM (Mreq *request)
{
	request->print_msg (my_table->moduleID, "ERROR");
	fatal_error ("Client: OM state shouldn't see this message\n");
}
inline void MOESIF_protocol::do_cache_FM (Mreq *request)
{
	request->print_msg (my_table->moduleID, "ERROR");
	fatal_error ("Client: FM state shouldn't see this message\n");
}
inline void MOESIF_protocol::do_snoop_I (Mreq *request)
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

inline void MOESIF_protocol::do_snoop_IM (Mreq *request)
{
	switch (request->msg) {
		case GETS:
			break;
		case GETM:
			break;
		case DATA:
			/** IM state meant that the block had sent the GET and was waiting on DATA.
			 * Now that Data is received we can send the DATA to the processor and finish
			 * the transition to M.
			 */
			/**
			 * Note we use get_shared_line() here to demonstrate its use.
			 * (Hint) The shared line indicates when another cache has a copy and is useful
			 * for knowing when to go to the E/S state.
			 * Since we only have I and M state in the MI protocol, what the shared line
			 * means here is whether a cache sent the data or the memory controller.
			 */
			send_DATA_to_proc(request->addr);
			state = MOESIF_CACHE_M;
			break;
		default:
	        request->print_msg (my_table->moduleID, "ERROR");
	        fatal_error ("Client: I state shouldn't see this message\n");
		}
}

inline void MOESIF_protocol::do_snoop_S (Mreq *request)
{
	switch (request->msg) {
	case GETS:
		set_shared_line();
		break;
	case GETM:
		state = MOESIF_CACHE_I;
		break;
	case DATA:
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
	}
}

inline void MOESIF_protocol::do_snoop_O (Mreq *request)
{
	switch (request->msg) {
	case GETS:
		set_shared_line();
		send_DATA_on_bus(request->addr,request->src_mid);
		break;
	case GETM:
		//set_shared_line();
		send_DATA_on_bus(request->addr,request->src_mid);
		state = MOESIF_CACHE_I;
		break;
	case DATA:
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
	}
}
inline void MOESIF_protocol::do_snoop_F(Mreq *request)
{
	switch (request->msg) {
	case GETS:
		set_shared_line();
		send_DATA_on_bus(request->addr,request->src_mid);
		break;
	case GETM:
		send_DATA_on_bus(request->addr,request->src_mid);
		state = MOESIF_CACHE_I;
		break;
	case DATA:
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
	}
}
inline void MOESIF_protocol::do_snoop_M (Mreq *request)
{
	switch (request->msg) {
	case GETS:
		set_shared_line();
		send_DATA_on_bus(request->addr,request->src_mid);
		state = MOESIF_CACHE_O;
		break;
	case GETM:
		//set_shared_line();
		send_DATA_on_bus(request->addr,request->src_mid);
		state = MOESIF_CACHE_I;
		break;
	case DATA:
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
	}
}

inline void MOESIF_protocol::do_snoop_SM (Mreq *request)
{
	switch (request->msg) {
		case GETS:
			set_shared_line();
			break;
		case GETM:
			if (request->src_mid != my_table->moduleID)
			{
				set_shared_line();
				state = MOESIF_CACHE_IM;
			}
			break;
		case DATA:
			send_DATA_to_proc(request->addr);
			state = MOESIF_CACHE_M;
			break;
		default:
	        request->print_msg (my_table->moduleID, "ERROR");
	        fatal_error ("Client: I state shouldn't see this message\n");
		}
}

inline void MOESIF_protocol::do_snoop_IS (Mreq *request)
{
	switch (request->msg) {
		case GETS:
		case GETM:
			break;
		case DATA:
			send_DATA_to_proc(request->addr);
			if (get_shared_line())
				state = MOESIF_CACHE_S;
			else /*Alternatively, this means that data has come from the memory*/
				state = MOESIF_CACHE_E;
			break;
		default:
	        request->print_msg (my_table->moduleID, "ERROR");
	        fatal_error ("Client: I state shouldn't see this message\n");
		}
}

inline void MOESIF_protocol::do_snoop_OM (Mreq *request)
{
	switch (request->msg) {
		case GETS:
			/*This will always be from another proc*/
			set_shared_line();
			send_DATA_on_bus(request->addr,request->src_mid);
			break;
		case GETM:
			/*This needs to be done both in case someone else requested data and also to send data
			 * to ourselves in case we sent the getM during O->OM*/
			send_DATA_on_bus(request->addr,request->src_mid);
			/*This is done in the case where I->IM transition
			caused a getM to be generated. In this case, we should provide the data so that IM moves to M
			and move ourself to IM because now we will receive our own getM(which was queued cuz of I's GetM. This should now generate data
			not from OM but instead the guy who just moved to M	*/
			if (request->src_mid != my_table->moduleID)
				state = MOESIF_CACHE_IM;

			break;
		case DATA:
			send_DATA_to_proc(request->addr);
			state = MOESIF_CACHE_M;
			break;
		default:
	        request->print_msg (my_table->moduleID, "ERROR");
	        fatal_error ("Client: I state shouldn't see this message\n");
		}
}
inline void MOESIF_protocol::do_snoop_FM (Mreq *request)
{
	switch (request->msg) {
		case GETS:
			/*This will always be from another proc*/
			set_shared_line();
			send_DATA_on_bus(request->addr,request->src_mid);
			break;
		case GETM:
			if (request->src_mid != my_table->moduleID)
			{
				send_DATA_on_bus(request->addr,request->src_mid);
				state = MOESIF_CACHE_IM;
			}
			else
			{
				send_DATA_on_bus(request->addr,request->src_mid);
			}
			break;
		case DATA:
			send_DATA_to_proc(request->addr);
			state = MOESIF_CACHE_M;
			break;
		default:
	        request->print_msg (my_table->moduleID, "ERROR");
	        fatal_error ("Client: I state shouldn't see this message\n");
		}
}
inline void MOESIF_protocol::do_cache_E (Mreq *request)
{
	switch (request->msg) {
	case LOAD: /*Nothing else to be done*/
		send_DATA_to_proc(request->addr);
		break;
	case STORE:
		/*Data need not be supplied because we are
		 * the only ones with this cache line*/
		state = MOESIF_CACHE_M;
		Sim->silent_upgrades++;
		send_DATA_to_proc(request->addr);
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: I state shouldn't see this message\n");
	}
}

inline void MOESIF_protocol::do_snoop_E (Mreq *request)
{
	switch (request->msg) {
	case GETS:
		set_shared_line();
		send_DATA_on_bus(request->addr,request->src_mid);
		state = MOESIF_CACHE_F;
		break;
	case GETM:
		//set_shared_line();
		send_DATA_on_bus(request->addr,request->src_mid);
		state = MOESIF_CACHE_I;
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



