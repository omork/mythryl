// ctermid.c


#include "../../mythryl-config.h"

#include <stdio.h>
#include <string.h>

#include "system-dependent-unix-stuff.h"
#include "runtime-base.h"
#include "runtime-values.h"
#include "make-strings-and-vectors-etc.h"
#include "raise-error.h"
#include "cfun-proto-list.h"



// One of the library bindings exported via
//     src/c/lib/posix-process-environment/cfun-list.h
// and thence
//     src/c/lib/posix-process-environment/libmythryl-posix-process-environment.c



Val   _lib7_P_ProcEnv_ctermid   (Task* task,  Val arg)   {
    //=======================
    //
    // Mythryl type:   Void -> String
    //
    // Return pathname of controlling terminal.
    //
    // This fn gets bound as   ctermid   in:
    //
    //     src/lib/std/src/posix-1003.1b/posix-id.pkg

									    ENTER_MYTHRYL_CALLABLE_C_FN("_lib7_P_ProcEnv_ctermid");

    char* status;
    char  name[ L_ctermid ];

    RELEASE_MYTHRYL_HEAP( task->pthread, "_lib7_P_ProcEnv_ctermid", NULL );
	//
	status = ctermid( name );
	//
    RECOVER_MYTHRYL_HEAP( task->pthread, "_lib7_P_ProcEnv_ctermid" );

    if (status == NULL || *status == '\0') {
        //
	return RAISE_ERROR__MAY_HEAPCLEAN(task, "cannot determine controlling terminal", NULL);
    }
  
    return   make_ascii_string_from_c_string__may_heapclean( task, name, NULL );		// make_ascii_string_from_c_string__may_heapclean	def in    src/c/heapcleaner/make-strings-and-vectors-etc.c
}



/* COPYRIGHT (c) 1995 by AT&T Bell Laboratories.
 * Subsequent changes by Jeff Prothero Copyright (c) 2010-2012,
 * released under Gnu Public Licence version 3.
 */
