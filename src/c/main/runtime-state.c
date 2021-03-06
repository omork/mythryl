// runtime-state.c

#include "../mythryl-config.h"

#include <stdarg.h>
#include <string.h>
#include "runtime-base.h"
#include "system-signals.h"
#include "heap-tags.h"
#include "runtime-values.h"
#include "make-strings-and-vectors-etc.h"
#include "runtime-globals.h"
#include "heapcleaner.h"
#include "runtime-timer.h"
#include "runtime-configuration.h"


												// struct pthread_state_struct { 			def in   src/c/h/runtime-base.h
												// typedef struct pthread_state_struct	Pthread;	def in   src/c/h/runtime-base.h
Pthread*   pthread_table__global[  MAX_PTHREADS  ];						// pthread_table__global[] is exported			via      src/c/h/runtime-base.h
    //     =====================
    //
    // Table of all active posix threads in process.
    // (Or at least, all posix threads running Mythryl
    // code or accessing the Mythryl heap.)
    //
    // This table is initialized by   make_task()   (below).
    //
    // In multithreaded operation this table is modified
    // by the heapcleaner -- of course -- but otherwise
    // only by code in   src/c/pthread/pthread-on-posix-threads.c
    // serialized by the pthread_table_mutex__local.


static void   set_up_pthread_state   (Pthread* pthread);

Task*   make_task   (Bool is_boot,  Heapcleaner_Args* cleaner_args)    {
    //  =========
    //
    // This function is called two places, one each in:
    //
    //     src/c/heapcleaner/import-heap.c
    //     src/c/main/load-compiledfiles.c

    static int last_id_issued = 0;

    Task* task =  NULL;

    //
    for (int i = 0;   i < MAX_PTHREADS;   i++) {
	//
	if (((pthread_table__global[i] = MALLOC_CHUNK(Pthread)) == NULL)
	||  ((task = MALLOC_CHUNK(Task)) == NULL)
	){
	    die ("runtime-state.c: unable to allocate pthread_table__global entry");
	}

	pthread_table__global[i]->task =  task;
    }
    task =  pthread_table__global[0]->task;

    // Allocate and initialize the heap data structures:
    //
    set_up_heap( task, is_boot, cleaner_args );							// set_up_heap					def in    src/c/heapcleaner/heapcleaner-initialization.c

    // 'set_up_heap' has created an agegroup0 buffer;
    //  partition it between our MAX_PTHREADS pthreads:
    //
    partition_agegroup0_buffer_between_pthreads( pthread_table__global );			// partition_agegroup0_buffer_between_pthreads	def in   src/c/heapcleaner/pthread-heapcleaner-stuff.c

    // Initialize the per-Pthread Mythryl state:
    //
    for (int i = 0;  i < MAX_PTHREADS;  i++) {
	//
	set_up_pthread_state( pthread_table__global[i] );

	// Single timers are currently shared
	// among multiple pthreads:
	//
	if (i != 0) {
	    pthread_table__global[ i ] -> cpu_time_at_start_of_last_heapclean
	  = pthread_table__global[ 0 ] -> cpu_time_at_start_of_last_heapclean;
	    //
	    pthread_table__global[ i ] -> cumulative_cleaning_cpu_time
	  = pthread_table__global[ 0 ] -> cumulative_cleaning_cpu_time;
	}
    }

    // Initialize the first Pthread here:
    //
    pthread_table__global[0]->id   =  ++last_id_issued;					// pth__get_pthread_id () returns huge numbers, this gives us small pthread ids.
    pthread_table__global[0]->ptid =  pth__get_pthread_id ();				// pth__get_pthread_id				def in    src/c/pthread/pthread-on-posix-threads.c
    pthread_table__global[0]->mode =  PTHREAD_IS_RUNNING;

    // Initialize the timers:
    //
    reset_timers( pthread_table__global[0] );						// "Pthread support note: For now, only Pthread 0 has timers." (Ancient comment, not sure it is true. -- 2012-03-02 CrT)

    return task;
}							// fun make_task



static void   set_up_pthread_state   (Pthread* pthread)   {
    //        ====================
    //
    pthread->heap				= pthread->task->heap;
    pthread->task->pthread			= pthread;
    //
    pthread->executing_mythryl_code		= FALSE;
    pthread->posix_signal_pending		= FALSE;
    pthread->mythryl_handler_for_posix_signal_is_running		= FALSE;
    //
    pthread->all_posix_signals.seen_count	= 0;
    pthread->all_posix_signals.done_count	= 0;
    pthread->next_posix_signal_id		= 0;
    pthread->next_posix_signal_count			= 0;
    //
    pthread->posix_signal_rotor		= MIN_SYSTEM_SIG;
    //
    pthread->heapcleaning_done_signal_handler_state		= LIB7_SIG_IGNORE;
    pthread->thread_scheduler_timeslice_signal_handler_state	= LIB7_SIG_IGNORE;
    //
    pthread->cpu_time_at_start_of_last_heapclean		= MALLOC_CHUNK(Time);
    pthread->cumulative_cleaning_cpu_time			= MALLOC_CHUNK(Time);

    for (int i = 0;  i < MAX_POSIX_SIGNALS;  i++) {
	//
	pthread->posix_signal_counts[i].seen_count = 0;
	pthread->posix_signal_counts[i].done_count = 0;
    }

    // Initialize the Mythryl state, including the roots:
    //
    initialize_task( pthread->task );
    //
    pthread->task->argument			= HEAP_VOID;
    pthread->task->fate				= HEAP_VOID;
    pthread->task->current_closure		= HEAP_VOID;
    pthread->task->link_register		= HEAP_VOID;
    pthread->task->program_counter		= HEAP_VOID;
    pthread->task->exception_fate		= HEAP_VOID;
    pthread->task->current_thread		= HEAP_VOID;
    pthread->task->callee_saved_registers[0]	= HEAP_VOID;
    pthread->task->callee_saved_registers[1]	= HEAP_VOID;
    pthread->task->callee_saved_registers[2]	= HEAP_VOID;
    pthread->task->heapvoid			= HEAP_VOID;			// Something for protected_c_arg to point to when not being used.
    pthread->task->protected_c_arg		= &pthread->task->heapvoid;	// Support for  RELEASE_MYTHRYL_HEAP  in  src/c/h/runtime-base.h

    pthread->ptid		= 0;						// Note that '0' works as an initializer whether pthread_t is an int or pointer on the host OS.
    pthread->mode		= PTHREAD_IS_VOID;
}										// fun set_up_pthread_state

void   initialize_task   (Task* task)   {
    // ===============
    //
    // Initialize the Mythryl state vector.
    //
    // Note that we do not initialize the root registers here,
    // since this is sometimes called when the roots are live
    // (from run_mythryl_function).					// run_mythryl_function__may_heapclean		def in    src/c/main/run-mythryl-code-and-runtime-eventloop.c
    //
    // This fn is called two places, above and in
    //     src/c/main/run-mythryl-code-and-runtime-eventloop.c

    task->heap_changelog =   HEAP_VOID;

    #if NEED_SOFTWARE_GENERATED_PERIODIC_EVENTS
        //
	task->software_generated_periodic_event_is_pending	= FALSE;
	task->in_software_generated_periodic_event_handler	= FALSE;
    #endif
}

void   save_c_state   (Task* task, Roots* extra_roots)   {
    // ============
    // 
    // Build a return closure that will save
    // a collection of Mythryl values being
    // used by C.  The Mythryl values are
    // passed by reference with NULL as termination.
    //
    // This fn is called only in:
    //
    //     src/c/main/load-compiledfiles.c

    int  n = 0;

    Roots* r;
    for (r = extra_roots;   r;   r = r->next)  ++n;

    set_slot_in_nascent_heapchunk (task, 0, MAKE_TAGWORD(n, PAIRS_AND_RECORDS_BTAG));
    int  i = 1;
    for (r = extra_roots;   r;   r = r->next, ++i) {
	//
        set_slot_in_nascent_heapchunk (task, i, *r->root);
    }    
    task->callee_saved_registers[0] =  commit_nascent_heapchunk(task, n);

    task->fate =  PTR_CAST( Val, return_to_c_level_c);
}

void   restore_c_state   (Task* task, Roots* extra_roots)   {
    // ===============
    //
    //    Restore a collection of Mythryl values from the return closure.
    //
    // This fn is called only in:
    //
    //     src/c/main/load-compiledfiles.c

    Val*    vp;
    Val	    saved_state;

    saved_state = task->callee_saved_registers[0];

    int n							/* This variable will be unused if ASSERTs, are off, so suppress 'unused var' compiler warning: */   		__attribute__((unused))
        =
        CHUNK_LENGTH(saved_state);

    int  i = 0;

    for (Roots* r = extra_roots;   r;   r = r->next, ++i) {
	//
	vp =  r->root;
       *vp =  GET_TUPLE_SLOT_AS_VAL(saved_state, i);
    }

    ASSERT( i == n );
}


///////////////////////////////////////////////////////////////////////////
// Support for RELEASE_MYTHRYL_HEAP.
//
// See overview comments in   src/c/h/runtime-base.h
//
// (This code should maybe have its own .c file.)
//
void*   buffer_mythryl_heap_value(
	    //
	    Mythryl_Heap_Value_Buffer*	buf,									// Mythryl_Heap_Value_Buffer				def in   src/c/h/runtime-base.h
	    void*			heapval,
	    int				heapval_bytesize
	)
{
    // For speed, we buffer small values on the stack:
    //
    if (heapval_bytesize < MAX_STACK_BUFFERED_MYTHRYL_HEAP_VALUE) {						// A few KB:  MAX_STACK_BUFFERED_MYTHRYL_HEAP_VALUE	def in   src/c/h/runtime-base.h
        //
	buf->heap_space = NULL;											// Make sure this is initialized -- we'll call free() on this in buffer_mythryl_heap_value().
	//
	memcpy( buf->stack_space, heapval, heapval_bytesize );
	return  buf->stack_space;
	//
    } else {
        //
        // Larger values we buffer on the heap.
        // Copying heapval will probably take longer
        // than the malloc() call anyhow, in this size range:
        //
	buf->heap_space =  malloc( heapval_bytesize );
        //
	if (!buf->heap_space)  die( "buffer_mythryl_heap_value: Unable to malloc(%d)\n", heapval_bytesize ); 
        //
	memcpy( buf->heap_space, heapval, heapval_bytesize );
	return  buf->heap_space;
    }
}

void*   buffer_mythryl_heap_nonvalue(										// Same as above, except we're just providing space, not copying anything into it.
	    //
	    Mythryl_Heap_Value_Buffer*	buf,									// Mythryl_Heap_Value_Buffer				def in   src/c/h/runtime-base.h
	    int				bytes
	)
{
    // For speed, we buffer small values on the stack:
    //
    if (bytes < MAX_STACK_BUFFERED_MYTHRYL_HEAP_VALUE) {							// A few KB:  MAX_STACK_BUFFERED_MYTHRYL_HEAP_VALUE	def in   src/c/h/runtime-base.h
        //
	buf->heap_space = NULL;											// Make sure this is initialized -- we'll call free() on this in buffer_mythryl_heap_value().
	//
	return  buf->stack_space;
	//
    } else {
        //
        // Larger values we buffer on the heap.
        // Copying heapval will probably take longer
        // than the malloc() call anyhow, in this size range:
        //
	buf->heap_space =  malloc( bytes );
        //
	if (!buf->heap_space)  die( "buffer_mythryl_heap_value: Unable to malloc(%d)\n", bytes ); 
        //
	return  buf->heap_space;
    }
}

void   unbuffer_mythryl_heap_value(   Mythryl_Heap_Value_Buffer* buf   ) {					// Mythryl_Heap_Value_Buffer				def in   src/c/h/runtime-base.h
    //
    free( buf->heap_space );											// It is ok to call free(NULL).
}



// COPYRIGHT (c) 1993 by AT&T Bell Laboratories.
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2012,
// released under Gnu Public Licence version 3.


