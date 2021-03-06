// signal-stuff.c
//
// System independent support fns for
// signals and software polling.

#include "../mythryl-config.h"

#include <stdio.h>
#include "runtime-base.h"
#include "runtime-configuration.h"
#include "make-strings-and-vectors-etc.h"
#include "system-dependent-signal-stuff.h"
#include "system-signals.h"


void   choose_signal   (Pthread* pthread)   {
    // =============
    // 
    // Caller guarantees that at least one Unix signal has been
    // seen at the C level but not yet handled at the Mythryl
    // level.  Our job is to find and return the number of
    // that signal plus the number of times it has fired at
    // the C level since last being handled at the Mythryl level.
    //
    // Choose which signal to pass to the Mythryl-level handler
    // and set up the Mythryl state vector accordingly.
    //
    // This function gets called (only) from
    //
    //     src/c/main/run-mythryl-code-and-runtime-eventloop.c
    //
    // WARNING: This should be called with signals masked
    // to avoid race conditions.

    int delta;

    // Scan the signal counts looking for
    // a signal that needs to be handled.
    //
    // The 'seen_count' field for a signal gets
    // incremented once for each incoming signal
    // in   c_signal_handler()   in
    //
    //     src/c/machine-dependent/posix-signal.c
    //
    // Here we increment the matching 'done_count' field
    // each time we invoke appropriate handling for that
    // signal;  thus, the difference between the two
    // gives the number of pending instances of that signal
    // currently needing to be handled.
    //
    // For fairness we scan for signals round-robin style, using
    //
    //     pthread->posix_signal_rotor
    //
    // to remember where we left off scanning, so we can pick
    // up from there next time:	

    #if NEED_TO_EXECUTE_ASSERTS
    int j = 0;
    #endif

    int i = pthread->posix_signal_rotor;
    do {
	ASSERT (j++ < MAX_POSIX_SIGNALS);				// At worst we have to search all the way through pthread->posix_signal_counts[MAX_POSIX_SIGNALS].

	i++;

	// Wrap circularly around the signal vector:
	//
	if (i == MAX_POSIX_SIGNALS)					// #define MAX_POSIX_SIGNALS  32		in (codemade file:)	src/c/o/system-signals.h
	    i =  MIN_SYSTEM_SIG;					// #define MIN_SYSTEM_SIG      1		in (codemade file:)	src/c/o/system-signals.h

	// Does this signal have pending work? (Nonzero == "yes"):
	//
	delta = pthread->posix_signal_counts[i].seen_count - pthread->posix_signal_counts[i].done_count;

    } while (delta == 0);

    pthread->posix_signal_rotor = i;					// Next signal to scan on next call to this fn.

    // Record the signal to process
    // and how many times it has fired
    // since last being handled at the
    // Mythryl level:
    //
    pthread->next_posix_signal_id    = i;
    pthread->next_posix_signal_count = delta;

//    log_if(
//        "signal-stuff.c/choose_signal: signal d=%d  seen_count d=%d  done_count d=%d   diff d=%d",
//        i,
//        pthread->posix_signal_counts[i].seen_count,
//        pthread->posix_signal_counts[i].done_count,
//        pthread->posix_signal_counts[i].seen_count - pthread->posix_signal_counts[i].done_count
//    );

    // Mark this signal as 'done':
    //
    pthread->posix_signal_counts[i].done_count  += delta;
    pthread->all_posix_signals.done_count       += delta;

    #ifdef SIGNAL_DEBUG
        debug_say ("choose_signal: sig = %d, count = %d\n", pthread->next_posix_signal_id, pthread->next_posix_signal_count);
    #endif
}


Val   make_resumption_fate   (				// Called once from this file, once from   src/c/main/run-mythryl-code-and-runtime-eventloop.c
    //==================== 
    //
    Task* task,
    Val*  resume					// Either   resume_after_handling_signal   or   resume_after_handling_software_generated_periodic_event
){							// from a platform-dependent assembly file like    src/c/machine-dependent/prim.intel32.asm  
    // 
    // Build the resume fate for a signal or poll event handler.
    // This closure contains the address of the resume entry-point and
    // the registers from the Mythryl state.
    //
    // Caller guarantees us roughly 4KB available space.
    //
    // This gets called from make_mythryl_signal_handler_arg() below,
    // and also from  src/c/main/run-mythryl-code-and-runtime-eventloop.c

    // Allocate the resumption closure:
    //
    set_slot_in_nascent_heapchunk(task,  0, MAKE_TAGWORD(10, PAIRS_AND_RECORDS_BTAG));
    set_slot_in_nascent_heapchunk(task,  1, PTR_CAST( Val, resume));
    set_slot_in_nascent_heapchunk(task,  2, task->argument);
    set_slot_in_nascent_heapchunk(task,  3, task->fate);
    set_slot_in_nascent_heapchunk(task,  4, task->current_closure);
    set_slot_in_nascent_heapchunk(task,  5, task->link_register);
    set_slot_in_nascent_heapchunk(task,  6, task->program_counter);
    set_slot_in_nascent_heapchunk(task,  7, task->exception_fate);
    set_slot_in_nascent_heapchunk(task,  8, task->callee_saved_registers[0]);				// John Reppy says not to do: set_slot_in_nascent_heapchunk(task,  8, task->current_thread);
    set_slot_in_nascent_heapchunk(task,  9, task->callee_saved_registers[1]);
    set_slot_in_nascent_heapchunk(task, 10, task->callee_saved_registers[2]);
    //
    return commit_nascent_heapchunk(task, 10);
}


Val   make_mythryl_signal_handler_arg   (
    //=============================== 
    //
    Task* task,
    Val*  resume_after_handling_signal
){
    // We're handling a POSIX inteprocess signal for
    //
    //     src/c/main/run-mythryl-code-and-runtime-eventloop.c
    //
    // Depending on platform,    resume_after_handling_signal
    // is from one of
    //     src/c/machine-dependent/prim.intel32.asm
    //     src/c/machine-dependent/prim.intel32.masm
    //     src/c/machine-dependent/prim.sun.asm
    //     src/c/machine-dependent/prim.pwrpc32.asm
    //
    // Our job is to build the Mythryl argument record for
    // the Mythryl signal handler.  The handler has type
    //
    //   posix_interprocess_signal_handler : (Int, Int, Fate(Void)) -> X
    //
    // where
    //     The first  argument is  the signal id 		// For example SIGALRM,
    //     the second argument is  the signal count		// I.e., number of times signal has been recieved since last handled.
    //     the third  argument is  the resumption fate.
    //
    // The return type is X because the Mythryl
    // signal handler should never return.
    //
    // NOTE: Maybe this should be combined with choose_signal???	XXX BUGGO FIXME


    Pthread* pthread = task->pthread;

    Val run_fate =  make_resumption_fate( task,  resume_after_handling_signal );

    // Allocate the Mythryl signal handler's argument record:
    //
    Val arg = make_three_slot_record( task, 
	//
	TAGGED_INT_FROM_C_INT( pthread->next_posix_signal_id	),
        TAGGED_INT_FROM_C_INT( pthread->next_posix_signal_count	),
	run_fate
    );

    #ifdef SIGNAL_DEBUG
	debug_say( "make_mythryl_signal_handler_arg: resumeC = %#x, arg = %#x\n", run_fate, arg );
    #endif

    return arg;
}


void   load_resume_state   (Task* task) {						// Called exactly once, from   src/c/main/run-mythryl-code-and-runtime-eventloop.c
    // =================
    // 
    // Load the Mythryl state with the state preserved
    // in resumption fate made by make_resumption_fate.
    //
    Val* current_closure;

    #ifdef SIGNAL_DEBUG
        debug_say ("load_resume_state:\n");
    #endif

    current_closure = PTR_CAST(Val*, task->current_closure);

    task->argument		= current_closure[1];
    task->fate			= current_closure[2];
    task->current_closure	= current_closure[3];
    task->link_register		= current_closure[4];
    task->program_counter	= current_closure[5];
    task->exception_fate	= current_closure[6];

    // John (Reppy) says current_thread
    // should not be included here...
    //    task->current_thread	= current_closure[7];

    task->callee_saved_registers[0]	= current_closure[7];
    task->callee_saved_registers[1]	= current_closure[8];
    task->callee_saved_registers[2]	= current_closure[9];
}



// COPYRIGHT (c) 1995 by AT&T Bell Laboratories.
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2012,
// released under Gnu Public Licence version 3.


