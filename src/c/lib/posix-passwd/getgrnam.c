// getgrnam.c


#include "../../mythryl-config.h"

#include <stdio.h>
#include <string.h>
#include <grp.h>

#if HAVE_SYS_TYPES_H
    #include <sys/types.h>
#endif

#include "system-dependent-unix-stuff.h"
#include "runtime-base.h"
#include "runtime-values.h"
#include "heap-tags.h"
#include "make-strings-and-vectors-etc.h"
#include "raise-error.h"
#include "cfun-proto-list.h"



// One of the library bindings exported via
//     src/c/lib/posix-passwd/cfun-list.h
// and thence
//     src/c/lib/posix-passwd/libmythryl-posix-passwd-db.c



Val   _lib7_P_SysDB_getgrnam   (Task* task,  Val arg)   {
    //======================
    //
    // Mythryl type:   String -> (String, Unt, List(String))
    //
    // Get group file entry by name.
    //
    // This fn gets bound as   getgrname'   in:
    //
    //     src/lib/std/src/posix-1003.1b/posix-etc.pkg

									    ENTER_MYTHRYL_CALLABLE_C_FN("_lib7_P_SysDB_getgrnam");

    struct group*  info;


    // We cannot reference anything on the Mythryl
    // heap between RELEASE_MYTHRYL_HEAP and RECOVER_MYTHRYL_HEAP
    // because garbage collection might be moving
    // it around, so copy heap_name into C storage: 
    //
    Mythryl_Heap_Value_Buffer  name_buf;
    //
    {   char* heap_name =  HEAP_STRING_AS_C_STRING( arg );
        //
   	char* c_name
	    = 
	    buffer_mythryl_heap_value( &name_buf, (void*) heap_name, strlen( heap_name ) +1 );		// '+1' for terminal NUL on string.


	RELEASE_MYTHRYL_HEAP( task->pthread, "_lib7_P_SysDB_getgrnam", NULL );
	    //
	    info =  getgrnam( c_name );
	    //
	RECOVER_MYTHRYL_HEAP( task->pthread, "_lib7_P_SysDB_getgrnam" );

	unbuffer_mythryl_heap_value( &name_buf );
    }
    if (info == NULL)   return RAISE_SYSERR__MAY_HEAPCLEAN(task, -1, NULL);
  
    Val gr_name =  make_ascii_string_from_c_string__may_heapclean(		task,                  info->gr_name, NULL	);		Roots roots1 = { &gr_name, NULL };
    Val gr_gid  =  make_one_word_unt(						task, (Vunt) (info->gr_gid)		);		Roots roots2 = { &gr_name, &roots1 };
    Val gr_mem  =  make_ascii_strings_from_vector_of_c_strings__may_heapclean(	task,                  info->gr_mem, &roots2	);

    return   make_three_slot_record(task,  gr_name, gr_gid, gr_mem  );
}


// COPYRIGHT (c) 1995 by AT&T Bell Laboratories.
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2012,
// released under Gnu Public Licence version 3.

