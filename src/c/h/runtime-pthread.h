// runtime-pthread.h
//
// Support for multicore operation.
// This stuff is (in part) exported to
// the Mythrl world as
//
//     src/lib/std/src/pthread.api
//     src/lib/std/src/pthread.pkg
//
// via
//
//     src/c/lib/pthread/libmythryl-pthread.c
//
// Platform-specific implementations of
// this functionality are:
//
//     src/c/pthread/pthread-on-posix-threads.c
//     src/c/pthread/pthread-on-sgi.c
//     src/c/pthread/pthread-on-solaris.c

#ifndef RUNTIME_MULTICORE_H
#define RUNTIME_MULTICORE_H

#include "../mythryl-config.h"

// Status of a Pthread:
//
typedef enum {
    //
    PTHREAD_IS_RUNNING,
    PTHREAD_IS_SUSPENDED,
    NO_PTHREAD_ALLOCATED
    //
} Pthread_Status;

#if !NEED_PTHREAD_SUPPORT
    //
    #define BEGIN_CRITICAL_SECTION( LOCK )	{
    #define END_CRITICAL_SECTION( LOCK )	}
    #define ACQUIRE_MUTEX(LOCK)		// no-op
    #define RELEASE_MUTEX(LOCK)		// no-op

// These should be in the Linux section, this is very temporary: -- 2011-10-30 CrT
// End of temporary Linux stuff
#if HAVE_SYS_TYPES_H
    #include <sys/types.h>
#endif
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif
typedef pid_t 	Pid;			// A process id.
#else // NEED_PTHREAD_SUPPORT

    #if !NEED_SOFTWARE_GENERATED_PERIODIC_EVENTS \
     || !NEED_PTHREAD_SUPPORT_FOR_SOFTWARE_GENERATED_PERIODIC_EVENTS
	//
	#error Multicore runtime currently requires polling support.
    #endif

    #if HAVE_SYS_TYPES_H
	#include <sys/types.h>
    #endif

    #include <sys/prctl.h>

    #if HAVE_UNISTD_H
	#include <unistd.h>
    #endif

    #include <pthread.h>

    typedef pthread_mutex_t		Mutex;		// A mutual-exclusion lock.
    typedef pthread_barrier_t;		Barrier;	// A barrier.
    typedef pid_t	 		Pid;		// A process id.





    ////////////////////////////////////////////////////////////////////////////
    // PACKAGE STARTUP AND SHUTDOWN
    //
    extern void     pth_initialize		(void);					// Called once near the top of main() to initialize the package.  Allocates our static locks, may also mmap() memory for arena or whatever.
    extern void     pth_shut_down		(void);					// Called once just before calling exit(), to release any OS resources.



    ////////////////////////////////////////////////////////////////////////////
    // PTHREAD START/STOP/ETC SUPPORT
    //
    extern Val      pth_acquire_pthread		(Task* task,  Val arg);			// Called with (thread, closure) and if a pthread is available starts arg running on a new pthread and returns TRUE.
    //											// Returns FALSE if we're already maxed out on allowed number of pthreads.
    //											// This gets exported to the Mythryl level as "pthread"::"acquire_pthread"  via   src/c/lib/pthread/cfun-list.h
    //											// There is apparently currently no .pkg file referencing this value.
    //
    extern void     pth_release_pthread		(Task* task);				// Reverse of above, more or less.
    //											// On Solaris this appears to actually stop and kill the thread.
    //											// On SGI this appears to just suspend the thread pending another request to run something on it.
    //											// Presumably the difference is that thread de/allocation is cheaper on Solaris than on SGI...?
    // 
    extern Pid      pth_get_pthread_id		(void);					// Supplies value for pthread_table_global[0]->pid in   src/c/main/runtime-state.c
    //											// This just calls getpid()  in                         src/c/pthread/pthread-on-sgi.c
    //											// This returns thr_self() (I don't wanna know) in      src/c/pthread/pthread-on-solaris.c
    //
    extern int      pth_max_pthreads		();					// Just exports to the Mythryl level the MAX_PTHREADS value from   src/c/h/runtime-configuration.h
    //
    extern int      pth_get_active_pthread_count();					// Just returns (as a C int) the value of   ACTIVE_PTHREADS_COUNT_REFCELL_GLOBAL, which is defined in   src/c/h/runtime-globals.h
											// Used only to set barrier for right number of pthreads in   src/c/heapcleaner/pthread-heapcleaner-stuff.c


    ////////////////////////////////////////////////////////////////////////////
    // MULTICORE GARBAGE COLLECTION SUPPORT
    //
    extern int   pth_start_heapcleaning    (Task*);
    extern void  pth_finish_heapcleaning   (Task*, int);
    //
    extern Val*  pth_extra_heapcleaner_roots_global [];



    ////////////////////////////////////////////////////////////////////////////
    //                   LOCKS
    //
    // We use our "locks" to perform mutual exclusion,
    // ensuring consistency of shared mutable datastructures
    // by ensuring that at most one pthread at a time is
    // updating that datastructure.  Typically we allocate
    // one such lock for each major shared mutable datastructure,
    // which persists for as long as that datastructure.
    //
    extern Mutex pth_make_mutex		();				// Just what you think.
    extern void  pth_free_mutex		(Mutex mutex);				// This call was probably only needed for SGI's daft hardware mutexs, and can be eliminated now. XXX BUGGO FIXME
    //
    extern void  pth_acquire_mutex	(Mutex mutex);				// Used to enter a critical section, preventing any other pthread from proceeding past pth_acquire_mutex() for this mutex until we release.
    extern void  pth_release_mutex	(Mutex mutex);				// Reverse of preceding operation; exits critical section and allows (one) other pthread to proceed past pth_acquire_mutex() on this mutex.
    //
    extern Bool  pth_maybe_acquire_mutex(Mutex mutex);				// This appears to be a non-blocking variant of pth_acquire_mutex, which always returns immediately with either TRUE (mutex acquired) or FALSE.
    //
    // Some statically pre-allocated mutexs:
    //
    extern Mutex	    pth_heapcleaner_mutex_global;
    extern Mutex	    pth_heapcleaner_gen_mutex_global;
    extern Mutex	    pth_timer_mutex_global;
    //
    extern Barrier* pth_cleaner_barrier_global;
    //
    //
    // Some readability tweaks:							// We should probably eliminate these -- 2011-11-01 CrT
    //
    #define BEGIN_CRITICAL_SECTION( mutex )	{ pth_acquire_mutex(mutex); {
    #define END_CRITICAL_SECTION( mutex )	} pth_release_mutex(mutex); }
    //
    #define ACQUIRE_MUTEX(mutex)		pth_acquire_mutex(mutex);
    #define RELEASE_MUTEX(mutex)		pth_release_mutex(mutex);


    ////////////////////////////////////////////////////////////////////////////
    //                   BARRIERS
    //
    // We use our "barriers" to perform essentially the
    // opposite of mutual exclusion, ensuring that all
    // pthreads in a set have completed their part of
    // a shared task before any of them are allowed to
    // proceed past the "barrier".
    //
    // Our only current use of this facility is in
    //
    //     src/c/heapcleaner/pthread-heapcleaner-stuff.c
    //
    // where it serves to ensure that garbage collection
    // does not start until all pthreads have ceased normal
    // processing, and that no pthread resumes normal processing
    // until the garbage collection is complete.
    //
    // The literature distinguishes barriers where waiting is
    // done by blocking from those where waiting is done by spinning;
    // it isn't clear which was intended by the original authors.
    //
    // NB: This facility seems to be implemented directly in hardware in    src/c/pthread/pthread-on-sgi.c
    // but implemented on top of mutexs in                                  src/c/pthread/pthread-on-solaris.c
    //
    extern Barrier* pth_make_barrier 	();					// Allocate a barrier.
    extern void     pth_free_barrier	(Barrier* barrierp);			// Free a barrier.
    //
    extern void     pth_wait_at_barrier	(Barrier* barrierp, unsigned n);	// Should be called 'barrier_wait' or such.  Block pthread until 'n' pthreads are waiting at the barrier, then release them all.
    //										// It is presumed that all threads waiting on a barrier use the same value of 'n'; otherwise behavior is probably undefined. (Poor design IMHO.)
    //
    extern void     pth_reset_barrier	(Barrier* barrierp);			// (Never used.)  Reset barrier to initial state. Presumably any waiting pthreads are released to proceed.





#endif // NEED_PTHREAD_SUPPORT

#endif // RUNTIME_MULTICORE_H



// COPYRIGHT (c) 1994 by AT&T Bell Laboratories.
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2011,
// released under Gnu Public Licence version 3.
