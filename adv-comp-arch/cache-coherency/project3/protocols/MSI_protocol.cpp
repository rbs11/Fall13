#include "MSI_protocol.h"
#include "../sim/mreq.h"
#include "../sim/sim.h"
#include "../sim/hash_table.h"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MSI_protocol::MSI_protocol (Hash_table *my_table, Hash_entry *my_entry)
    : Protocol (my_table, my_entry)
{
	// Initialize lines to not have the data yet!
	 this->state = MSI_CACHE_I;
}

MSI_protocol::~MSI_protocol ()
{
}

void MSI_protocol::dump (void)
{
    const char *block_states[5] = {"X","I","IM","S","M"};
    fprintf (stderr, "MSI_protocol - state: %s\n", block_states[state]);
}

void MSI_protocol::process_cache_request (Mreq *request)
{
	switch (state) {
    case MSI_CACHE_I:  do_cache_I (request); break;
    case MSI_CACHE_IM: do_cache_IM (request); break;
    case MSI_CACHE_M:  do_cache_M (request); break;
    case MSI_CACHE_S:  do_cache_S (request); break;
    default:
        fatal_error ("Invalid Cache State for MSI Protocol\n");
    }
}

void MSI_protocol::process_snoop_request (Mreq *request)
{
	switch (state) {
		case MSI_CACHE_I:  do_snoop_I (request); break;
	    case MSI_CACHE_IM: do_snoop_IM (request); break;
	    case MSI_CACHE_M:  do_snoop_M (request); break;
	    case MSI_CACHE_S:  do_snoop_S (request); break;
		default:
			fatal_error ("Invalid Cache State for MSI Protocol\n");
    }
}

inline void MSI_protocol::do_cache_I (Mreq *request)
{
	switch (request->msg) {
	    // If we get a request from the processor we need to get the data
	    case STORE:
	    	send_GETM(request->addr);
	    	state = MSI_CACHE_IM;
	    	Sim->cache_misses++;
	    	break;
	    case LOAD:
	    	/* Line up the GETM in the Bus' queue */
	    	send_GETS(request->addr);
	    	/* The IM state means that we have sent the GET message and we are now waiting
	    	 * on DATA
	    	 */
	    	state = MSI_CACHE_IM;
	    	/* This is a cache miss */
	    	Sim->cache_misses++;
	    	break;
	    default:
	        request->print_msg (my_table->moduleID, "ERROR");
	        fatal_error ("Client: I state shouldn't see this message\n");
	    }
}

inline void MSI_protocol::do_cache_IM (Mreq *request)
{
	switch (request->msg) {
	/* If the block is in the IM state that means it sent out a GET message
	 * and is waiting on DATA.  Therefore the processor should be waiting
	 * on a pending request. Therefore we should not be getting any requests from
	 * the processor.
	 */
	case LOAD:
	case STORE:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error("Should only have one outstanding request per processor!");
	default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: I state shouldn't see this message\n");
	}
}
inline void MSI_protocol::do_cache_S (Mreq *request)
{
	switch (request->msg) {
		case LOAD: /*Nothing else to be done*/
			send_DATA_to_proc(request->addr);
			break;
		case STORE:
			send_GETM(request->addr);
			/*This is cuz we are expecting data in case
			this line has been modified by another proc*/
			state = MSI_CACHE_IM;
			Sim->cache_misses++;
			break;
		default:
	        request->print_msg (my_table->moduleID, "ERROR");
	        fatal_error ("Client: I state shouldn't see this message\n");
		}
}
inline void MSI_protocol::do_cache_M (Mreq *request)
{
	switch (request->msg) {
	/* The M state means we have the data and we can modify it.  Therefore any request
	 * from the processor (read or write) can be immediately satisfied.
	 */
	case STORE:
		send_DATA_to_proc(request->addr);
		break;
	case LOAD:
		// This is how you send data back to the processor to finish the request
		// Note: There was no need to send anything on the bus on a hit.
		send_DATA_to_proc(request->addr);
		break;
	default:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error ("Client: M state shouldn't see this message\n");
	}
}

inline void MSI_protocol::do_snoop_I (Mreq *request)
{
	switch (request->msg) {
	    case GETS:
	    case GETM:
	    case DATA:
	    	/**
	    	 * If we snoop a message from another cache and we are in I, then we don't
	    	 * need to do anything!  We obviously cannot supply data since we don't have
	    	 * it, and we don't need to downgrade our state since we are already in I.
	    	 */
	    	break;
	    default:
	        request->print_msg (my_table->moduleID, "ERROR");
	        fatal_error ("Client: I state shouldn't see this message\n");
	}
}


inline void MSI_protocol::do_snoop_IM (Mreq *request)
{
	switch (request->msg) {
	case GETS:
		nxt_state = MSI_CACHE_S;
		break;
	case GETM:
		/** While in IM we will see our own GETS or GETM on the bus.  We should just
		 * ignore it and wait for DATA to show up.
		 */
		nxt_state = MSI_CACHE_M;
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
		state = nxt_state;
		break;
	default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: I state shouldn't see this message\n");
	}
}


inline void MSI_protocol::do_snoop_IS (Mreq *request)
{
	switch (request->msg) {
	case GETS:
	case GETM:
		break;
	case DATA:
		send_DATA_to_proc(request->addr);
		state = MSI_CACHE_S;
		break;
	default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: I state shouldn't see this message\n");
	}
}
inline void MSI_protocol::do_snoop_S (Mreq *request)
{
	switch (request->msg) {
		case GETS:
			break;
		case GETM:
			state = MSI_CACHE_I;
			break;
		case DATA:
			break;
		default:
	        request->print_msg (my_table->moduleID, "ERROR");
	        fatal_error ("Client: I state shouldn't see this message\n");
		}

}

inline void MSI_protocol::do_snoop_M (Mreq *request)
{
	switch (request->msg) {
	    case GETS:
	    	set_shared_line();
	    	send_DATA_on_bus(request->addr,request->src_mid);
	    	state = MSI_CACHE_S;
	    	break;
	    case GETM:
	    	/**
	    	 * Another cache wants the data so we send it to them and transition to
	    	 * Invalid since they will be transitioning to M.  When we send the DATA
	    	 * it will go on the bus the next cycle and the memory will see it and cancel
	    	 * its lookup for the DATA.
	    	 */
	    	/**
	    	 * We use set_shared_line() here to demonstrate its use.  Since we had a copy
	    	 * of the line being requested, we should indicate that the data is shared on
	    	 * chip.  (This will be essential to understand in order to implement MESI/
	    	 * MOSI/MOESI)
	    	 */
	    	set_shared_line();
	    	send_DATA_on_bus(request->addr,request->src_mid);
	    	state = MSI_CACHE_I;
	    	break;
	    case DATA:
	    	fatal_error ("Should not see data for this line!  I have the line!");
	    	break;
	    default:
	        request->print_msg (my_table->moduleID, "ERROR");
	        fatal_error ("Client: M state shouldn't see this message\n");
	    }
}
