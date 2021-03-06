// getppid.c



#include "../../mythryl-config.h"

#include "runtime-base.h"
#include "runtime-values.h"
#include "cfun-proto-list.h"

#include <stdio.h>
#include <string.h>

#if HAVE_UNISTD_H
    #include <unistd.h>
#endif



// One of the library bindings exported via
//     src/c/lib/posix-process-environment/cfun-list.h
// and thence
//     src/c/lib/posix-process-environment/libmythryl-posix-process-environment.c



Val   _lib7_P_ProcEnv_getppid   (Task* task,  Val arg)   {
    //=======================
    //
    // Mythryl type:   Void -> Int
    //
    // Return the process id of the parent process.
    //
    // This fn gets bound as   get_parent_process_id   in:
    //
    //     src/lib/std/src/posix-1003.1b/posix-id.pkg

									    ENTER_MYTHRYL_CALLABLE_C_FN("_lib7_P_ProcEnv_getppid");

    RELEASE_MYTHRYL_HEAP( task->pthread, "_lib7_P_ProcEnv_getppid", NULL );
	//
	int ppid =  getppid ();
	//
    RECOVER_MYTHRYL_HEAP( task->pthread, "_lib7_P_ProcEnv_getppid" );

    return TAGGED_INT_FROM_C_INT( ppid );
}


// COPYRIGHT (c) 1995 by AT&T Bell Laboratories.
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2012,
// released under Gnu Public Licence version 3.

