// fcntl_d.c


#include "../../mythryl-config.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "system-dependent-unix-stuff.h"

#if HAVE_FCNTL_H
    #include <fcntl.h>
#endif

#include "runtime-base.h"
#include "runtime-values.h"
#include "make-strings-and-vectors-etc.h"
#include "raise-error.h"
#include "cfun-proto-list.h"



// One of the library bindings exported via
//     src/c/lib/posix-io/cfun-list.h
// and thence
//     src/c/lib/posix-io/libmythryl-posix-io.c



Val   _lib7_P_IO_fcntl_d   (Task* task,  Val arg)   {
    //==================
    //
    // Mythryl type:   (Int, Int) -> Int
    //
    // Duplicate an open file descriptor
    //
    // This fn gets bound as   fcntl_d   in:
    //
    //     src/lib/std/src/posix-1003.1b/posix-io.pkg
    //     src/lib/std/src/posix-1003.1b/posix-io-64.pkg

									    ENTER_MYTHRYL_CALLABLE_C_FN("_lib7_P_IO_fcntl_d");

    int             fd;

    int             fd0 = GET_TUPLE_SLOT_AS_INT( arg, 0 );
    int             fd1 = GET_TUPLE_SLOT_AS_INT( arg, 1 );

/*  do { */					// Backed out 2010-02-26 CrT: See discussion at bottom of src/c/lib/socket/connect.c

	RELEASE_MYTHRYL_HEAP( task->pthread, "_lib7_P_IO_fcntl_d", NULL );
	    //
	    fd = fcntl(fd0, F_DUPFD, fd1);
	    //
	RECOVER_MYTHRYL_HEAP( task->pthread, "_lib7_P_IO_fcntl_d" );

/*  } while (fd < 0 && errno == EINTR);	*/	// Restart if interrupted by a SIGALRM or SIGCHLD or whatever.

    RETURN_STATUS_EXCEPT_RAISE_SYSERR_ON_NEGATIVE_STATUS__MAY_HEAPCLEAN(task, fd, NULL);
}


// COPYRIGHT (c) 1995 by AT&T Bell Laboratories.
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2012,
// released under Gnu Public Licence version 3.

