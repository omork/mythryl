// gettime.c


#include "../../mythryl-config.h"

#include <stdio.h>
#include <string.h>

#include "runtime-base.h"
#include "runtime-values.h"
#include "make-strings-and-vectors-etc.h"
#include "runtime-timer.h"
#include "cfun-proto-list.h"



Val   _lib7_Time_gettime   (Task* task,  Val arg)   {
    //==================
    //
    // Mythryl type: Void -> (one_word_int::Int, Int, one_word_int::Int, Int, one_word_int::Int, Int)
    //
    // Return this process's CPU time consumption
    // so far, broken down as:
    //     User-mode          seconds and microseconds.
    //     Kernel-mode        seconds and microseconds.
    //     Garbage collection seconds and microseconds.
    // used by this process so far.
    //
    // This fn gets bound as   gettime'   in:
    //
    //     src/lib/std/src/internal-cpu-timer.pkg

									    ENTER_MYTHRYL_CALLABLE_C_FN("_lib7_Time_gettime");

    Time		usr;					// User-mode   time consumption as reported by os.
    Time		sys;					// Kernel-mode time consumption as reported by os.

    Val		usr_seconds;
    Val		sys_seconds;
    Val		gc_seconds;

    Pthread*	pthread = task->pthread;

								// On posix: get_cpu_time()	def in   src/c/main/posix-timers.c
								// On win32: get_cpu_time()     def in   src/c/main/win32-timers.c
    RELEASE_MYTHRYL_HEAP( task->pthread, "_lib7_Time_gettime", arg );
	//
	get_cpu_time (&usr, &sys);
	//
    RECOVER_MYTHRYL_HEAP( task->pthread, "_lib7_Time_gettime" );

    usr_seconds =  make_one_word_int(task, usr.seconds            );
    sys_seconds =  make_one_word_int(task, sys.seconds            );
    gc_seconds  =  make_one_word_int(task, pthread->cumulative_cleaning_cpu_time->seconds );

    return  make_six_slot_record(task,
		//
		usr_seconds, TAGGED_INT_FROM_C_INT( usr.uSeconds ),
		sys_seconds, TAGGED_INT_FROM_C_INT( sys.uSeconds ),
		gc_seconds,  TAGGED_INT_FROM_C_INT( pthread->cumulative_cleaning_cpu_time->uSeconds )
	    );
}



// COPYRIGHT (c) 1994 by AT&T Bell Laboratories.
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2011,
// released under Gnu Public Licence version 3.

