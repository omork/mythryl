// get-or-set-socket-debug-option.c


#include "../../mythryl-config.h"

#include "sockets-osdep.h"
#include INCLUDE_SOCKET_H
#include "runtime-base.h"
#include "runtime-values.h"
#include "raise-error.h"
#include "socket-util.h"
#include "cfun-proto-list.h"



// One of the library bindings exported via
//     src/c/lib/socket/cfun-list.h
// and thence
//     src/c/lib/socket/libmythryl-socket.c



Val   get_or_set_socket_debug_option   (Task* task,  Val arg)   {
    //==============================
    //
    // Mythryl type: (Socket_Fd, Null_Or(Bool)) -> Bool
    //
    // This function gets bound as   ctl_debug   in:
    //
    //     src/lib/std/src/socket/socket-guts.pkg

									    ENTER_MYTHRYL_CALLABLE_C_FN("get_or_set_socket_debug_option");

    return   get_or_set_boolean_socket_option( task, arg, SO_DEBUG );			// get_or_set_boolean_socket_option		def in    src/c/lib/socket/get-or-set-boolean-socket-option.c
	//
	// We do the RELEASE_MYTHRYL_HEAP/RECOVER_MYTHRYL_HEAP stuff in
	// get_or_set_boolean_socket_option().
}


// COPYRIGHT (c) 1995 AT&T Bell Laboratories.
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2012,
// released under Gnu Public Licence version 3.

