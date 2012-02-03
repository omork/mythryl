// posix-signal.c
//
// Posix-specific code to support Mythryl-level handling of posix signals.


#include "../mythryl-config.h"

#include <stdio.h>

#include "system-dependent-unix-stuff.h"
#include "system-dependent-signal-get-set-etc.h"
#include "runtime-base.h"
#include "runtime-configuration.h"
#include "make-strings-and-vectors-etc.h"
#include "system-dependent-signal-stuff.h"
#include "system-signals.h"
#include "runtime-globals.h"



// The generated System_Constant table for UNIX signals:
//
#include "posix-signal-table--autogenerated.c"



// SELF_PTHREAD is used in
//
//     arithmetic_fault_handler					// arithmetic_fault_handler	def in   src/c/machine-dependent/posix-arithmetic-trap-handlers.c
//
// to supply the SELF_PTHREAD->task
// and           SELF_PTHREAD->->executing_mythryl_code
// values for handling
// a divide-by-zero or whatever.
//
// This is probably pretty BOGUS ON LINUX -- I think
// it means a divide-by-zero in any thread will always
// be reported as being in thread zero.
//
// (Can't we just map our pid to our pthread_table__global entry,
// by linear scan if nothing else?  -- 2011-11-03 CrT)
// 
// 
#if NEED_PTHREAD_SUPPORT
    #define SELF_PTHREAD	(pth__get_pthread())
#else
    #define SELF_PTHREAD	(pthread_table__global[ 0 ])
#endif


#ifdef USE_ZERO_LIMIT_PTR_FN
Punt		SavedPC;
extern		Zero_Heap_Allocation_Limit[];					// Actually a pointer, not an array.
#endif


static void   c_signal_handler   (/* int sig,  Signal_Handler_Info_Arg info,  Signal_Handler_Context_Arg* scp */);


Val   list_signals   (Task* task)   {						// Called from src/c/lib/signal/listsignals.c
    //============
    //
    return dump_table_as_system_constants_list (task, &SigTable);		// See src/c/heapcleaner/make-strings-and-vectors-etc.c
}

void   pause_until_signal   (Pthread* pthread) {
    // ==================
    //
    // Suspend the given Pthread
    // until a signal is received:
    pause ();									// pause() is a clib function, see pause(2).
}

void   set_signal_state   (Pthread* pthread,  int sig_num,  int signal_state) {
    // ================
    //
    // QUESTIONS:
    //
    // If we disable a signal that has pending signals,
    // should the pending signals be discarded?
    //
    // How do we keep track of the state
    // of non-UNIX signals (e.g., GC)?				XXX BUGGO FIXME


    switch (sig_num) {
	//
    case RUNSIG_GC:
	pthread->cleaning_signal_handler_state = signal_state;
	break;

    default:
	if (IS_SYSTEM_SIG(sig_num)) {
	    //
	    switch (signal_state) {
	        //
	    case LIB7_SIG_IGNORE:
		SELECT_SIG_IGN_HANDLING_FOR_SIGNAL (sig_num);
		break;

	    case LIB7_SIG_DEFAULT:
		SELECT_SIG_DFL_HANDLING_FOR_SIGNAL( sig_num );
		break;

	    case LIB7_SIG_ENABLED:
		SET_SIGNAL_HANDLER( sig_num, c_signal_handler );			// SET_SIGNAL_HANDLER 	#define in   src/c/machine-dependent/system-dependent-signal-get-set-etc.h
		break;

	    default:
		die ("bogus signal state: sig = %d, state = %d\n",
		    sig_num, signal_state);
	    }

	} else {

            die ("set_signal_state: unknown signal %d\n", sig_num);
	}
    }
}


int   get_signal_state   (Pthread* pthread,  int sig_num)   {
    //================
    //
    switch (sig_num) {
        //
    case RUNSIG_GC:
	return pthread->cleaning_signal_handler_state;

    default:
	if (!IS_SYSTEM_SIG(sig_num))   die ("get_signal_state: unknown signal %d\n", sig_num);
	//
        {   void    (*handler)();
	    //
	    GET_SIGNAL_HANDLER( sig_num, handler );				// Store it into 'handler'.
	    //
	    if      (handler == SIG_IGN)	return LIB7_SIG_IGNORE;
	    else if (handler == SIG_DFL)	return LIB7_SIG_DEFAULT;
	    else				return LIB7_SIG_ENABLED;
	}
    }
}


#if defined(HAS_POSIX_SIGS) && defined(HAS_UCONTEXT)

static void   c_signal_handler   (int sig,  siginfo_t* si,  void* c)   {
    //        ================
    //
    // This is the C signal handler for
    // signals that are to be passed to
    // the Mythryl level via signal_handler in
    //
    //     src/lib/std/src/nj/runtime-signals-guts.pkg
    //

    ucontext_t* scp		/* This variable is unused on some platforms, so suppress 'unused var' compiler warning: */   __attribute__((unused))
        =
        (ucontext_t*) c;

    Pthread* pthread = SELF_PTHREAD;


    // Sanity check:  We compile in a MAX_POSIX_SIGNALS value but
    // have no way to ensure that we don't wind up getting run
    // on some custom kernel supporting more than MAX_POSIX_SIGNAL,
    // so we check here to be safe:
    //
    if (sig >= MAX_POSIX_SIGNALS)    die ("posix-signal.c: c_signal_handler: sig d=%d >= MAX_POSIX_SIGNAL %d\n", sig, MAX_POSIX_SIGNALS ); 


    // Remember that we have seen signal number 'sig'.
    //
    // This will eventually get noticed by  choose_signal()  in
    //
    //     src/c/machine-dependent/signal-stuff.c
    //
    pthread->posix_signal_counts[sig].seen_count++;
    pthread->all_posix_signals.seen_count++;

    log_if(
        "posix-signal.c/c_signal_handler: signal d=%d  seen_count d=%d  done_count d=%d   diff d=%d",
        sig,
        pthread->posix_signal_counts[sig].seen_count,
        pthread->posix_signal_counts[sig].done_count,
        pthread->posix_signal_counts[sig].seen_count - pthread->posix_signal_counts[sig].done_count
    );

    #ifdef SIGNAL_DEBUG
    debug_say ("c_signal_handler: sig = %d, pending = %d, inHandler = %d\n", sig, pthread->posix_signal_pending, pthread->mythryl_handler_for_posix_signal_is_running);
    #endif

    // The following line is needed only when
    // currently executing "pure" C code, but
    // doing it anyway in all other cases will
    // not hurt:
    //
    pthread->ccall_limit_pointer_mask = 0;

    if (  pthread->executing_mythryl_code
    &&  ! pthread->posix_signal_pending
    &&  ! pthread->mythryl_handler_for_posix_signal_is_running
    ){
	pthread->posix_signal_pending = TRUE;

	#ifdef USE_ZERO_LIMIT_PTR_FN
	    //
	    SIG_SavePC( pthread->task, scp );
	    SET_SIGNAL_PROGRAM_COUNTER( scp, Zero_Heap_Allocation_Limit );
	#else
	    SIG_Zero_Heap_Allocation_Limit( scp );			// OK to adjust the heap limit directly.
	#endif
    }
}

#else

static void   c_signal_handler   (
    //
    int		    sig,
    #if (defined(TARGET_PWRPC32) && defined(OPSYS_LINUX))
	Signal_Handler_Context_Arg*   scp
    #else
	Signal_Handler_Info_Arg	info,
	Signal_Handler_Context_Arg*   scp
    #endif
){
    #if defined(OPSYS_LINUX)  &&  defined(TARGET_INTEL32)  &&  defined(USE_ZERO_LIMIT_PTR_FN)
	//
	Signal_Handler_Context_Arg*  scp =  &sc;
    #endif

    Pthread*  pthread =  SELF_PTHREAD;

    pthread->posix_signal_counts[sig].seen_count++;
    pthread->all_posix_signals.seen_count++;

    #ifdef SIGNAL_DEBUG
    debug_say ("c_signal_handler: sig = %d, pending = %d, inHandler = %d\n",
    sig, pthread->posix_signal_pending, pthread->mythryl_handler_for_posix_signal_is_running);
    #endif

    // The following line is needed only when
    // currently executing "pure" C code, but
    // doing it anyway in all other cases will
    // not hurt:
    //
    pthread->ccall_limit_pointer_mask = 0;

    if (  pthread-> executing_mythryl_code
    && (! pthread-> posix_signal_pending)
    && (! pthread-> mythryl_handler_for_posix_signal_is_running)
    ){
        //
	pthread->posix_signal_pending =  TRUE;

	#ifdef USE_ZERO_LIMIT_PTR_FN
	    //
	    SIG_SavePC( pthread->task, scp );
	    SET_SIGNAL_PROGRAM_COUNTER( scp, Zero_Heap_Allocation_Limit );
	#else
	    SIG_Zero_Heap_Allocation_Limit( scp );		// OK to adjust the heap limit directly.
	#endif
    }
}

#endif


void   set_signal_mask   (Task* task, Val arg)   {
    // ===============
    // 
    // Set the signal mask to the list of signals given by 'arg'.
    // The signal_list has the type
    //
    //     "sysconst list option"
    //
    // with the following semantics -- see src/lib/std/src/nj/runtime-signals.pkg
    //
    //	NULL	-- the empty mask
    //	THE[]	-- mask all signals
    //	THE l	-- the signals in l are the mask
    //

									    ENTER_MYTHRYL_CALLABLE_C_FN("set_signal_mask");

    Signal_Set	mask;											// Signal_Set		is from   src/c/h/system-dependent-signal-get-set-etc.h
    int		i;

    CLEAR_SIGNAL_SET(mask);										// CLEAR_SIGNAL_SET	is from   src/c/h/system-dependent-signal-get-set-etc.h

    Val signal_list  = arg;
    if (signal_list != OPTION_NULL) {
	signal_list  = OPTION_GET(signal_list);

	if (LIST_IS_NULL(signal_list)) {
	    //
	    // THE [] -- mask all signals
            //
	    for (i = 0;  i < NUM_SYSTEM_SIGS;  i++) {
	        //
		ADD_SIGNAL_TO_SET( mask, SigInfo[i].id );						// ADD_SIGNAL_TO_SET	is from   src/c/h/system-dependent-signal-get-set-etc.h
	    }

	} else {

	    while (signal_list != LIST_NIL) {
		Val	car = LIST_HEAD(signal_list);
		int	sig = GET_TUPLE_SLOT_AS_INT(car, 0);
		ADD_SIGNAL_TO_SET(mask, sig);
		signal_list = LIST_TAIL(signal_list);
	    }
	}
    }

    // Do the actual host OS syscall
    // to change the signal mask.
    // This is our only invocation of this syscall:
    //
//  log_if("posix-signal.c/set_signal_mask: setting host signal mask for process to x=%x", mask );	// Commented out because it floods mythryl.compile.log -- 2011-10-10 CrT
    //
    RELEASE_MYTHRYL_HEAP( task->pthread, "set_signal_mask", arg );
	//
	SET_PROCESS_SIGNAL_MASK( mask );
	//
    RECOVER_MYTHRYL_HEAP( task->pthread, "set_signal_mask" );
}


Val   get_signal_mask   (Task* task, Val arg)   {		// Called from src/c/lib/signal/getsigmask.c
    //=============== 
    // 
    // Return the current signal mask (only those signals supported by Mythryl);
    // like set_signal_mask, the result has the following semantics:
    //	NULL	-- the empty mask
    //	THE[]	-- mask all signals
    //	THE l	-- the signals in l are the mask

									    ENTER_MYTHRYL_CALLABLE_C_FN("get_signal_mask");

    Signal_Set	mask;
    Val	name, sig, signal_list;
    int		i, n;

    RELEASE_MYTHRYL_HEAP( task->pthread, "_lib7_Sig_getsigmask", arg );
	//
	GET_PROCESS_SIGNAL_MASK( mask );
	//
	// Count the number of masked signals:
	//
	for (i = 0, n = 0;  i < NUM_SYSTEM_SIGS;  i++) {
	    //
	    if (SIGNAL_IS_IN_SET(mask, SigInfo[i].id))   n++;
	}
	//
    RECOVER_MYTHRYL_HEAP( task->pthread, "_lib7_Sig_getsigmask" );

    if (n == 0)   return OPTION_NULL;

    if (n == NUM_SYSTEM_SIGS) {
	//
	signal_list = LIST_NIL;
	//
    } else {
	//
	for (i = 0, signal_list = LIST_NIL;   i < NUM_SYSTEM_SIGS;   i++) {
	    //
	    if (SIGNAL_IS_IN_SET(mask, SigInfo[i].id)) {
	        //
		name = make_ascii_string_from_c_string (task, SigInfo[i].name);

		sig = make_two_slot_record( task, TAGGED_INT_FROM_C_INT(SigInfo[i].id), name);

		signal_list = LIST_CONS(task, sig, signal_list);
	    }
	}
    }

    return  OPTION_THE( task, signal_list );
}


// COPYRIGHT (c) 1992 by AT&T Bell Laboratories.
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2011,
// released under Gnu Public Licence version 3.


