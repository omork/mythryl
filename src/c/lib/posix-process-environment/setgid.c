// setgid.c



#include "../../mythryl-config.h"

#include "make-strings-and-vectors-etc.h"
#include "lib7-c.h"
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



Val   _lib7_P_ProcEnv_setgid   (Task* task,  Val arg)   {
    //======================
    //
    // Mythryl type:  Unt -> Void
    //
    // Set group id.
    //
    // This fn gets bound as   set_group_id   in:
    //
    //     src/lib/std/src/posix-1003.1b/posix-id.pkg

    ENTER_MYTHRYL_CALLABLE_C_FN("_lib7_P_ProcEnv_setgid");

    RELEASE_MYTHRYL_HEAP( task->pthread, "_lib7_P_ProcEnv_setgid", arg );
	//
	int status =  setgid( WORD_LIB7toC( arg ));
	//
    RECOVER_MYTHRYL_HEAP( task->pthread, "_lib7_P_ProcEnv_setgid" );

    CHECK_RETURN_UNIT(task, status)
}


// COPYRIGHT (c) 1995 by AT&T Bell Laboratories.
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2011,
// released under Gnu Public Licence version 3.

