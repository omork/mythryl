// export-heap.c   -- Routines to export a Mythryl heap image.
//
// All of our Mythryl 'executables' like
//
//     bin/mythryld
//     bin/mythryl-lex
//     bin/mythryl-yacc
//
// are actually heap images, with a
//
//     #!/usr/bin/mythryl-runtime-intel32
//
// shebang line at the start to make them 'scripts'
// as far as the host operating system is concerned.
//
// (/usr/bin/mythryl-runtime-intel32 itself is a C program containing
// essentially the Mythryl garbage collector plus enough
// logic to load and run a Mythryl heap image.)
//
// The basic layout of a Mythryl heap image is:
//
//   Header (Image header + Heap header)
//   External reference table
//   Mythryl state info
//   Mythryl heap:
//     hugechunk region descriptors
//     Agegroup descriptors
//     Heap image
//
//
// Note that this will change once multiple Pthreads are supported.	XXX BUGGO FIXME


/*
###                       "A good warrior is not bellicose,
###                        A good fighter does not anger,
###                        A good conqueror does not contest his enemy,
###                        One who is good at using others puts himself below them."
###
###                                                         -Lao Tzu
*/


#include "../mythryl-config.h"

#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

#include "system-dependent-stuff.h"
#include "runtime-base.h"
#include "runtime-configuration.h"
#include "get-quire-from-os.h"
#include "make-strings-and-vectors-etc.h"
#include "runtime-globals.h"
#include "runtime-heap-image.h"
#include "heap.h"
#include "mythryl-callable-cfun-hashtable.h"
#include "writer.h"
#include "heap-io.h"
#include "export-heap-stuff.h"

#define IS_EXTERN_POINTER(book2sibid, w)	(IS_POINTER(w) && (SIBID_FOR_POINTER(book2sibid, w) == UNMAPPED_BOOK_SIBID))

 static Status         write_heap_image_to_file		(Task* task,  int kind,  FILE* file);
 static Status         write_heap			(Writer* writer,  Heap* heap);

 static Heapfile_Cfun_Table*  build_export_table	(Heap* heap);

 static void   repair_heap				(Heapfile_Cfun_Table* table,  Heap* heap);


Status   export_heap_image   (Task* task,  FILE* file) {
    //   =================
    //
    // This fn gets bound to   export_heap   in:
    //
    //     src/lib/std/src/nj/export.pkg
    //
    // via
    //
    //     src/c/lib/heap/export-heap.c

    return   write_heap_image_to_file( task, EXPORT_HEAP_IMAGE, file );		// Defined below.	EXPORT_HEAP_IMAGE	def in    src/c/heapcleaner/runtime-heap-image.hexpor
} 


Status   export_fn_image   (
    //   ===============
    //
    Task*  task,
    Val    function,
    FILE*  file
){
    // Zero out the saved parts of the task state,
    // and use the standard 'argument' register to
    // hold the exported function closure.
    //
    // This fn gets exported to the Mythryl level as   spawn_to_disk'   via:
    //
    //     src/c/lib/heap/export-fun.c
    //
    // and then
    //
    //     src/lib/std/src/nj/export.pkg 

    task->argument =   function;

    task->fate				= HEAP_VOID;
    task->current_closure		= HEAP_VOID;
    task->link_register			= HEAP_VOID;
    task->exception_fate		= HEAP_VOID;
    task->current_thread		= HEAP_VOID;	// ???
    task->callee_saved_registers[0]	= HEAP_VOID;
    task->callee_saved_registers[1]	= HEAP_VOID;
    task->callee_saved_registers[2]	= HEAP_VOID;
											// EXPORT_HEAP_IMAGE		def in    src/c/heapcleaner/runtime-heap-image.hexpor
    return   write_heap_image_to_file( task, EXPORT_FN_IMAGE, file );			// write_heap_image_to_file	def below.
}

inline static Val   write_register   (Heapfile_Cfun_Table* export_table,  Val val)   {
    //              ==============
    //
    if (IS_EXTERN_POINTER(book_to_sibid__global, val))   {
	//
	val = add_cfun_to_heapfile_cfun_table(export_table, val);
    }
    return val;
}

static Status   write_heap_image_to_file   (
    //          ========================
    //
    Task* task,
    int	  kind,						// EXPORT_FN_IMAGE | EXPORT_HEAP_IMAGE | NORMAL_DATASTRUCTURE_PICKLE | UNBOXED_PICKLE  -- defs in    src/c/heapcleaner/runtime-heap-image.h
    FILE* file
){
    Heap*  heap   =  task->heap;
    //
    Status status =  TRUE;

    Writer*   wr;

    if (!(wr = WR_OpenFile( file )))    return FALSE;

    // Shed any and all garbage:
    //
    call_heapcleaner( task, 0			 );		// Clean (only) heap generation zero.		// call_heapcleaner				def in    src/c/heapcleaner/call-heapcleaner.c
    call_heapcleaner( task, MAX_ACTIVE_AGEGROUPS );		// Clean all (remaining) heap generations.


    Heapfile_Cfun_Table*  export_table =  build_export_table( heap );						// build_export_table			def below.

    {   Heap_Header	hh;
	//
	hh.pthread_count		= 1;
	hh.active_agegroups		= heap->active_agegroups;
	//
	hh.smallchunk_sibs_count	= MAX_PLAIN_SIBS;						// MAX_PLAIN_SIBS			def in    src/c/h/sibid.h
	hh.hugechunk_sibs_count		= MAX_HUGE_SIBS;						// MAX_HUGE_SIBS			def in    src/c/h/sibid.h
	hh.hugechunk_quire_count	= heap->hugechunk_quire_count;
	//
	hh.oldest_agegroup_retaining_fromspace_sibs_between_heapcleanings
	    =
	    heap->oldest_agegroup_retaining_fromspace_sibs_between_heapcleanings;

	hh.agegroup0_buffer_bytesize
	    =
	    agegroup0_buffer_size_in_bytes(task); 

	hh.pervasive_package_pickle_list =   write_register(export_table,  *PTR_CAST(Val*, PERVASIVE_PACKAGE_PICKLE_LIST_REFCELL__GLOBAL));
        hh.runtime_pseudopackage         =   write_register(export_table,  runtime_package__global );
#ifdef ASM_MATH
	hh.math_package                  =   write_register(export_table,  mathvec__global );
#else
	hh.math_package                  =   HEAP_VOID;
#endif

	heapio__write_image_header( wr, kind );								// heapio__write_image_header		def in    src/c/heapcleaner/export-heap-stuff.c

	WR_WRITE(wr, &hh, sizeof(hh));
	if (WR_ERROR(wr)) {
	    WR_FREE(wr);
	    return FALSE;
	}
    }

    // Export the Mythryl state info:
    //
    {   Pthread_Image	image;

        // Save the live registers:
	//
	image.posix_interprocess_signal_handler =   write_register(export_table,  DEREF( POSIX_INTERPROCESS_SIGNAL_HANDLER_REFCELL__GLOBAL )	);
	image.stdArg				=   write_register(export_table,  task->argument						);
	image.stdCont				=   write_register(export_table,  task->fate							);
	//
	image.stdClos				=   write_register(export_table,  task->current_closure						);
	image.pc				=   write_register(export_table,  task->program_counter						);
	//
	image.exception_fate			=   write_register(export_table,  task->exception_fate						);
	image.current_thread			=   write_register(export_table,  task->current_thread						);
	//
	image.calleeSave[0]			=   write_register(export_table,  task->callee_saved_registers[0] 				);
	image.calleeSave[1]			=   write_register(export_table,  task->callee_saved_registers[1] 				);
	image.calleeSave[2]			=   write_register(export_table,  task->callee_saved_registers[2] 				);


	{   int bytes_written =  heapio__write_cfun_table( wr, export_table );				// heapio__write_cfun_table		def in    src/c/heapcleaner/export-heap-stuff.c
	    //
	    if (bytes_written == -1) {
		if (kind != EXPORT_FN_IMAGE)   repair_heap( export_table, heap );
		WR_FREE( wr );
		return FALSE;
	    }
	}

	WR_WRITE( wr, &image, sizeof(image) );

	if (WR_ERROR(wr)) {
	    if (kind != EXPORT_FN_IMAGE)   repair_heap( export_table, heap );
	    WR_FREE(wr);
	    return FALSE;
	}
    }

    // Write out the heap image:
    //
    if (write_heap(wr, heap) == FALSE)   status = FALSE;

    if (kind != EXPORT_FN_IMAGE)   repair_heap( export_table, heap );

    WR_FREE( wr );

    return status;
}


inline static void   patch_sib   (
    //               =========
    Sibid*       b2s,								// book_to_sibid__global from    src/c/heapcleaner/heapcleaner-initialization.c
    Heap*        heap,
    Heapfile_Cfun_Table* table,
    int          age,								// 0 <= age < heap->active_agegroups
    int          ilk								// ilk will be one of RO_CONSCELL_SIB, RO_POINTERS_SIB, RW_POINTERS_SIB from   src/c/h/sibid.h
){
    Sib* sib = heap->agegroup[ age ]->sib[ ilk ];

    Bool heap_needs_repair = FALSE;

    Val* limit = sib->tospace.used_end;				// Cache in register.

    for (Val*
        p = sib->tospace.start;
        p < limit;
	++p
    ){
	if (IS_EXTERN_POINTER(b2s, *p)) {					// IS_EXTERN_POINTER	def above.
	    //
	    *p = add_cfun_to_heapfile_cfun_table( table, *p );			// add_cfun_to_heapfile_cfun_table	def in    src/c/heapcleaner/mythryl-callable-cfun-hashtable.c

	    heap_needs_repair = TRUE;
	}
    }

    sib->heap_needs_repair = heap_needs_repair;
}

static Heapfile_Cfun_Table*   build_export_table   (Heap* heap) {
    //                ==================
    //
    // Scan the heap looking for exported 
    // symbols and return an export table:

    // Cache global in register for speed:
    //
    Sibid*  b2s = book_to_sibid__global;						// book_to_sibid__global	def in    src/c/heapcleaner/heapcleaner-initialization.c

    // Allocate an empty export table:
    //
    Heapfile_Cfun_Table* table = make_heapfile_cfun_table();					// make_heapfile_cfun_table	def in   src/c/heapcleaner/mythryl-callable-cfun-hashtable.c

    // Scan the record, pair and vector sibs
    // for references to external symbols.
    // (The string sib is not an issue because
    // it contains no references whatever.)
    //
    for (int age = 0;  age < heap->active_agegroups;  age++) {
	//
	patch_sib(b2s,heap,table, age, RO_CONSCELL_SIB );
	patch_sib(b2s,heap,table, age, RO_POINTERS_SIB );
	patch_sib(b2s,heap,table, age, RW_POINTERS_SIB );
    }

    return table;
}


static Status   write_heap   (Writer* wr,  Heap* heap)   {
    //          ==========
    //
    Sib_Header*	 p;
    Sib_Header*	 sib_headers;
    Hugechunk* bdp;

    int   sib_headers_size;
    long  offset;

    int pagesize =  GET_HOST_HARDWARE_PAGE_BYTESIZE();			// GET_HOST_HARDWARE_PAGE_BYTESIZE		def in   src/c/h/system-dependent-stuff.h

    // Write the hugechunk quire descriptors:
    {

	#ifdef BO_DEBUG
	    debug_say("%d hugechunk quires\n", heap->hugechunk_quire_count);
	#endif

	int size =  heap->hugechunk_quire_count * sizeof( Hugechunk_Quire_Header );

	Hugechunk_Quire_Header* header =  (Hugechunk_Quire_Header*) MALLOC (size);

        {   int i = 0;
	    //
	    for (Hugechunk_Quire*
		hq = heap->hugechunk_quires;
		hq != NULL;
		hq = hq->next,   i++
	    ){
		#ifdef BO_DEBUG
		    print_hugechunk_quire_map( hq );
		#endif

		header[i].base_address      =  BASE_ADDRESS_OF_QUIRE( hq->quire );
		header[i].bytesize	    =  BYTESIZE_OF_QUIRE(     hq->quire );
		header[i].first_ram_quantum =  hq->first_ram_quantum;
	    }
	}

	WR_WRITE( wr, header, size );

	if (WR_ERROR( wr )) {
	    //
	    FREE( header );
	    return FALSE;
	}

	FREE( header );
    }

    // Initialize the sib headers:
    //
    sib_headers_size =  heap->active_agegroups  *  TOTAL_SIBS  *  sizeof( Sib_Header );

    sib_headers = (Sib_Header*) MALLOC( sib_headers_size );

    offset = WR_TELL(wr) + sib_headers_size;

    offset = ROUND_UP_TO_POWER_OF_TWO(offset, pagesize);

    p = sib_headers;
    for (int age = 0;  age < heap->active_agegroups;  age++) {
	//
	for (int ilk = 0;  ilk < MAX_PLAIN_SIBS;  ilk++, p++) {
	    //
	    Sib* ap =  heap->agegroup[ age ]->sib[ ilk ];
	    //
	    p->age       =  age;
	    p->chunk_ilk =  ilk;
	    //
	    p->info.o.base_address  = (Punt)(ap->tospace.start);
	    p->info.o.bytesize	    = (Punt)(ap->tospace.used_end) - p->info.o.base_address;
	    p->info.o.rounded_bytesize = ROUND_UP_TO_POWER_OF_TWO(p->info.o.bytesize, pagesize);
	    //
	    p->offset =  (Unt1) offset;
	    offset   +=  p->info.o.rounded_bytesize;
	}

	for (int ilk = 0;  ilk < MAX_HUGE_SIBS;  ilk++, p++) {			// MAX_HUGE_SIBS (== 1)			def in    src/c/h/sibid.h
	    //
	    int chunks;
	    int quanta;
	    //
	    bdp = heap->agegroup[age]->hugechunks[ ilk ];			// ilk = 0 == CODE__HUGE_SIB		def in    src/c/h/sibid.h
	    //
	    for (chunks = quanta = 0;  bdp != NULL;  bdp = bdp->next) {
		//
		++ chunks;
		//
		quanta += ( uprounded_hugechunk_bytesize(bdp)		// uprounded_hugechunk_bytesize	def in    src/c/h/heap.h
                           /
                           HUGECHUNK_RAM_QUANTUM_IN_BYTES			// HUGECHUNK_RAM_QUANTUM_IN_BYTES	def in    src/c/h/heap.h
                         );				
	    }
	    p->age	 =  age;
	    p->chunk_ilk =  ilk;
	    //
	    p->info.bo.hugechunk_count        =  chunks;
	    p->info.bo.hugechunk_quanta_count =  quanta;
	    //
	    p->offset = (Unt1)offset;

	    offset   +=  chunks * sizeof( Hugechunk_Header )			// Hugechunk_Header			def in    src/c/heapcleaner/runtime-heap-image.h
		         +
                         quanta * HUGECHUNK_RAM_QUANTUM_IN_BYTES;
	}
    }

    // Write out the sib headers:
    //
    WR_WRITE(wr, sib_headers, sib_headers_size);      if (WR_ERROR(wr)) {  FREE (sib_headers);  return FALSE;  }

    // Write the sibs:
    //
    p = sib_headers;
    for (int age = 0;  age < heap->active_agegroups;  age++) {
	//
	for (int ilk = 0;  ilk < MAX_PLAIN_SIBS;  ilk++) {
	    //
	    if (heapcleaner_messages_are_enabled__global) {
		//
		debug_say("write %d,%d: %d bytes [%#x..%#x) @ %#x\n",
		    age+1, ilk, p->info.o.bytesize,
		    p->info.o.base_address, p->info.o.base_address+p->info.o.bytesize,
		    p->offset);
	    }
	    if (p->info.o.bytesize > 0) {
		//
		WR_SEEK(wr, p->offset);
		WR_WRITE(wr, (void*)(p->info.o.base_address), p->info.o.bytesize);		if (WR_ERROR(wr)) {  FREE(sib_headers); return FALSE; }
	    }
	    p++;
	}

	for (int huge_ilk = 0;  huge_ilk < MAX_HUGE_SIBS;  huge_ilk++) {					// MAX_HUGE_SIBS		def in    src/c/h/sibid.h
	    //
	    int		       header_bytesize;
	    Hugechunk_Header*  header;

	    if (p->info.bo.hugechunk_count > 0) {
		//
		header_bytesize
		    =
		    p->info.bo.hugechunk_count * sizeof(Hugechunk_Header);
		//
		header =  (Hugechunk_Header*) MALLOC( header_bytesize );

		if (heapcleaner_messages_are_enabled__global) {
		    //
		    debug_say("write %d,%d: %d big chunks (%d quanta) @ %#x\n",
			age+1, huge_ilk, p->info.bo.hugechunk_count, p->info.bo.hugechunk_quanta_count,
			p->offset);
		}

	        // Initialize the hugechunk headers:
                //
	        Hugechunk_Header* q =  header;
		//
		for (bdp  =  heap->agegroup[ age ]->hugechunks[ huge_ilk ];				// huge_ilk = 0 == CODE__HUGE_SIB	def in    src/c/h/sibid.h
                     bdp !=  NULL;
                     bdp  = bdp->next,  ++q
                ){
		    //
		    q->age	=  bdp->age;
		    q->huge_ilk =  huge_ilk;
		    //
		    q->base_address  =  (Punt) bdp->chunk;
		    q->bytesize =  bdp->bytesize;
		}

	        // Write the hugechunk headers:
                //
		WR_WRITE( wr, header, header_bytesize );					if (WR_ERROR(wr)) { FREE(header); FREE(sib_headers); return FALSE; }

	        // Write the hugechunk:
                //
		for (bdp  = heap->agegroup[ age ]->hugechunks[ huge_ilk ];
                     bdp != NULL;
                     bdp  = bdp->next
                ){
		    WR_WRITE(wr, (char*)(bdp->chunk), uprounded_hugechunk_bytesize(bdp));						// uprounded_hugechunk_bytesize	def in    src/c/h/heap.h
		    //
		    if (WR_ERROR(wr)) {  FREE (header);  FREE (sib_headers);  return FALSE;  }
		}
		FREE( header );
	    }
	    p++;
	}
    }

    FREE( sib_headers );

    return TRUE;
}



static void   repair_heap   (
    //        ===========
    //
    Heapfile_Cfun_Table* table,
    Heap*        heap
){
    #define REPAIR_SIB(index)	{					\
	    Sib* __ap = heap->agegroup[ age ]->sib[ index ];		\
	    if (__ap->heap_needs_repair) {				\
		Val	*__p, *__q;					\
		__p = __ap->tospace.start;				\
		__q = __ap->tospace.used_end;				\
		while (__p < __q) {					\
		    Val	__w = *__p;					\
		    if (IS_EXTERNAL_TAG(__w)) {				\
			*__p = get_cfun_address_from_heapfile_cfun_table(table, __w);		\
		    }							\
		    __p++;						\
		}							\
	    }								\
	    __ap->heap_needs_repair = FALSE;				\
	}


    // Repair the in-memory heap:
    //
    for (int age = 0;  age < heap->active_agegroups;  age++) {
	//
	REPAIR_SIB( RO_POINTERS_SIB );
	REPAIR_SIB( RO_CONSCELL_SIB );
	REPAIR_SIB( RW_POINTERS_SIB );
    }

    free_heapfile_cfun_table( table );
}

// COPYRIGHT (c) 1992 by AT&T Bell Laboratories.
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2012,
// released under Gnu Public Licence version 3.





/*
##########################################################################
#   The following is support for outline-minor-mode in emacs.		 #
#  ^C @ ^T hides all Text. (Leaves all headings.)			 #
#  ^C @ ^A shows All of file.						 #
#  ^C @ ^Q Quickfolds entire file. (Leaves only top-level headings.)	 #
#  ^C @ ^I shows Immediate children of node.				 #
#  ^C @ ^S Shows all of a node.						 #
#  ^C @ ^D hiDes all of a node.						 #
#  ^HFoutline-mode gives more details.					 #
#  (Or do ^HI and read emacs:outline mode.)				 #
#									 #
# Local variables:							 #
# mode: outline-minor							 #
# outline-regexp: "[A-Za-z]"			 		 	 #
# End:									 #
##########################################################################
*/


