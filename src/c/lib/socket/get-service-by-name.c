// get-service-by-name.c


#include "../../mythryl-config.h"

#include <stdio.h>
#include <string.h>
#include <netdb.h>

#include "sockets-osdep.h"
#include INCLUDE_SOCKET_H
#include "runtime-base.h"
#include "runtime-values.h"
#include "make-strings-and-vectors-etc.h"
#include "raise-error.h"
#include "cfun-proto-list.h"
#include "socket-util.h"

/*
###           "I see little commercial potential for
###            the Internet for at least ten years."
###
###                         -- Bill Gates, 1994
*/



// One of the library bindings exported via
//     src/c/lib/socket/cfun-list.h
// and thence
//     src/c/lib/socket/libmythryl-socket.c



Val   _lib7_netdb_get_service_by_name   (Task* task,  Val arg)   {
    //===============================
    //
    // Mythryl type:   (String, Null_Or(String)) ->   Null_Or(   (String, List(String), Int, String)   )
    //
    // This fn gets bound as   get_service_by_name'   in:
    //
    //     src/lib/std/src/socket/net-service-db.pkg

									    ENTER_MYTHRYL_CALLABLE_C_FN("_lib7_netdb_get_service_by_name");

    Val	ml_service  =  GET_TUPLE_SLOT_AS_VAL( arg, 0 );
    Val	ml_protocol =  GET_TUPLE_SLOT_AS_VAL( arg, 1 );

    char* heap_service = HEAP_STRING_AS_C_STRING( ml_service );
    char* heap_protocol;
    //
    if (ml_protocol == OPTION_NULL)   heap_protocol =  NULL;
    else			      heap_protocol =  HEAP_STRING_AS_C_STRING( OPTION_GET( ml_protocol ) );


    struct servent* result;

    // We cannot reference anything on the Mythryl
    // heap between RELEASE_MYTHRYL_HEAP and RECOVER_MYTHRYL_HEAP
    // because garbage collection might be moving
    // it around, so copy heap_service and heap_procotol
    // into C storage: 
    //
    Mythryl_Heap_Value_Buffer   service_buf;
    Mythryl_Heap_Value_Buffer  protocol_buf;
    //
    {   char* c_service  = buffer_mythryl_heap_value(  &service_buf, (void*) heap_service,  strlen( heap_service  ) +1 );		// '+1' for terminal NUL on string.
	char* c_protocol = buffer_mythryl_heap_value( &protocol_buf, (void*) heap_protocol, strlen( heap_protocol ) +1 );		// '+1' for terminal NUL on string.

	RELEASE_MYTHRYL_HEAP( task->pthread, "_lib7_netdb_get_service_by_name", NULL );
	    //
	    result = getservbyname( c_service, c_protocol );
	    //
	RECOVER_MYTHRYL_HEAP( task->pthread, "_lib7_netdb_get_service_by_name" );

	unbuffer_mythryl_heap_value(  &service_buf );
	unbuffer_mythryl_heap_value( &protocol_buf );
    }

    return _util_NetDB_mkservent( task, result );						// _util_NetDB_mkservent	def in   src/c/lib/socket/util-mkservent.c
}


// COPYRIGHT (c) 1995 AT&T Bell Laboratories.
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2012,
// released under Gnu Public Licence version 3.

