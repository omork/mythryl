// libmythryl-posix-process-environment.c

#include "runtime-base.h"
#include "mythryl-callable-c-libraries.h"
#include "cfun-proto-list.h"


// This file defines the "posix_process_environment" library of Mythryl-callable
// C functions, accessible at the Mythryl level via:
//
//     my  get_process_id:  Void -> Sy_Int
//         =
//	   mythryl_callable_c_library_interface::find_c_function { lib_name => "posix_process_environment", fun_name => "getpid" };
// 
// or such -- see src/lib/std/src/posix-1003.1b/posix-id.pkg

#define CFUNC(NAME, NAME2, FUNC, LIB7TYPE)	CFUNC_BIND(NAME, NAME2, FUNC, LIB7TYPE)
static Mythryl_Name_With_C_Function CFunTable[] = {
#include "cfun-list.h"													// Actual function list is in   src/c/lib/posix-process-environment/cfun-list.h
	CFUNC_NULL_BIND
    };
#undef CFUNC


// The POSIX process environment library.
//
// Our record                Libmythryl_Posix_Process_Environment
// gets compiled into        src/c/lib/mythryl-callable-c-libraries-list.h
// and thus ultimately       mythryl_callable_c_libraries_local []
// in                        src/c/lib/mythryl-callable-c-libraries.c
//
Mythryl_Callable_C_Library	    Libmythryl_Posix_Process_Environment = {						// Mythryl_Callable_C_Library		def in    src/c/h/mythryl-callable-c-libraries.h
    //                              ====================================
    CLIB_NAME,
    CLIB_VERSION,
    CLIB_DATE,
    NULL,
    CFunTable
};



// COPYRIGHT (c) 1995 AT&T Bell Laboratories.
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2011,
// released under Gnu Public Licence version 3.
