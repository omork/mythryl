// heapclean-agegroup0.c
//
// This file contains the code for doing "minor"
// heapcleanings ("garbage collections") -- those
// which recover unused memory only in the agegroup0
// buffer.
//
// For more on the Mythryl heap datastructures see
//
//     src/A.HEAP.OVERVIEW



/*
###            "It goes against the grain of modern
###             education to teach children to program.
###
###            "What fun is there in making plans,
###             acquiring discipline in organizing thoughts,
###             devoting attention to detail, and
###             learning to be self-critical?"
###
###                               -- Alan Perlis
*/



#include "../mythryl-config.h"

#include <stdio.h>

#include "runtime-base.h"
#include "runtime-configuration.h"
#include "runtime-values.h"
#include "make-strings-and-vectors-etc.h"
#include "coarse-inter-agegroup-pointers-map.h"
#include "heap.h"
#include "heap-tags.h"
#include "copy-loop.h"


// Cleaner statistics:
//
extern long	update_count__global;			// update_count__global				def in    src/c/heapcleaner/heapclean-n-agegroups.c
extern long	total_bytes_allocated__global;		// total_bytes_allocated__global		def in    src/c/heapcleaner/heapclean-n-agegroups.c
extern long	total_bytes_copied__global;		// total_bytes_allocated__global		def in    src/c/heapcleaner/heapclean-n-agegroups.c

// heap_changelog operations:
//
#define HEAP_CHANGELOG_NIL	HEAP_VOID
#define HEAP_CHANGELOG_HEAD(p)	GET_TUPLE_SLOT_AS_PTR( Val*, p, 0 )
#define HEAP_CHANGELOG_TAIL(p)	GET_TUPLE_SLOT_AS_VAL(       p, 1 )

//
static void   process_task_heap_changelog (Task* task,  Heap* heap);
static void   copy_all_remaining_reachable_values_in_agegroup0_to_agegroup1	                (Agegroup* agegroup1,         Task* task);			// 'task' arg is purely for debugging, can be deleted for production use.
static  Val   forward_agegroup0_chunk_to_agegroup1	(Agegroup* agegroup1,  Val v, Task* task, int caller);		// 'task' arg is only for debugging, can be dropped in production code.
static  Val   forward_special_chunk			(Agegroup* agegroup1,  Val* chunk,  Val tagword);


#ifdef VERBOSE
    extern char* sib_name__global [];			// sib_name__global	def in   src/c/heapcleaner/heapclean-n-agegroups.c
#endif

static inline void   forward_to_agegroup1_if_in_agegroup0   (Sibid* book2sibid,  Agegroup* g1,  Val *p, Task* task) {		// 'task' arg is only for debugging, can be dropped in production code.
    //               ====================================
    //
    // Forward *p if it is in agegroup0:

    Val	w =  *p;
    //
    if (IS_POINTER(w)) {
	//
	Sibid  sibid =  SIBID_FOR_POINTER( book2sibid, w );
	//
	if (sibid == AGEGROUP0_SIBID)   *p =  forward_agegroup0_chunk_to_agegroup1( g1, w, task, 0 );
    }
}


void   heapclean_agegroup0   (Task* task,  Val** roots) {
    // ===================
    //
    // Do "garbage collection" on just agegroup0.
    //
    // 'roots' is a vector of live pointers into agegroup0,
    // harvested from the live register set, global variables
    // into the heap maintained by C code, etc.
    //
    // This fun is called (only) from:
    //
    //     src/c/heapcleaner/call-heapcleaner.c
    //
    // NB: If we have multiple pthreads running,
    // each has its own agegroup0, but we process
    // all of those during this call, by virtue
    // of being passed all the roots from all the
    // running pthreads. 

    Heap*      heap =  task->heap;
    Agegroup*  age1 =  heap->agegroup[0];

													    Punt  age1_tospace_top   [ MAX_PLAIN_SIBS ];
														//
														// Heapcleaner statistics support: We use this to note the
														// current start-of-freespace in each generation-one sib buffer.
														// At the bottom of this fn, the difference between this and
														// the new start-of-freespace gives us the amount of live stuff
														// we've copied into that sib.  This is pure reportage;
														// our algorithms do not depend in any way on this information.

													    long bytes_allocated = agegroup0_usedspace_in_bytes( task );
													    //	
													    INCREASE_BIGCOUNTER( &heap->total_bytes_allocated, bytes_allocated );
														//
														// More heapcleaner statistics reportage.

													    for (int i = 0;  i < MAX_PLAIN_SIBS;  i++) {
														//
														age1_tospace_top[i]
														    =
														    (Punt)   age1->sib[ i ]->tospace.used_end;
													    }


													    #ifdef VERBOSE
														debug_say ("Agegroup 1 before cleaning agegroup0:\n");
														//
														for (int i = 0;  i < MAX_PLAIN_SIBS;  i++) {
														    //
														    debug_say ("  %s: to-space bottom = %#x, end of fromspace oldstuff = %#x, tospace.used_end = %#x\n",
															//
															sib_name__global[ i+1 ],
															//
															age1->sib[ i ]->tospace,
															age1->sib[ i ]->fromspace.seniorchunks_end,
															age1->sib[ i ]->tospace.used_end
														    );
														}
													    #endif

    // Scan the standard roots.  These are pointers
    // to live data harvested from the live registers,
    // C globals etc, so all agegroup0 records pointed
    // to by them are definitely "live" (nongarbage):
    //
    {   Sibid*  b2s = book_to_sibid__global;									// Cache global locally for speed.   book_to_sibid__global	def in    src/c/heapcleaner/heapcleaner-initialization.c
	Val*    rp;
	while ((rp = *roots++) != NULL) {
	    //
	    forward_to_agegroup1_if_in_agegroup0( b2s, age1, rp, task );
	}
    }

    // Scan the changelog -- if there are any new
    // pointers into agegroup0 from other agegroups
    // we need to know about them now:	
    //
    {    for (int i = 0;  i < MAX_PTHREADS;  i++) {									// Potentially need to process one heap storelog per pthread.
	    //
	    Pthread* pthread =  pthread_table__global[ i ];
	    //
	    Task*   task     =  pthread->task;
	    //
	    if (pthread->mode != PTHREAD_IS_VOID) {
		//
		process_task_heap_changelog( task, heap );
	    }
	}
    }

    copy_all_remaining_reachable_values_in_agegroup0_to_agegroup1( age1, task );
								    ++heap->agegroup0_heapcleanings_count;
    null_out_newly_dead_weakrefs( heap );										// null_out_newly_dead_weakrefs		def in    src/c/heapcleaner/heapcleaner-stuff.c

    //////////////////////////////////////////////////////////// 
    // At this point there is nothing left in the agegroup0
    // buffer(s) that we care about, so we're done.  Our caller
    // will reset the agegroup0 buffer(s) to empty and resume
    // allocating linearly in it/them from start to end.
    //////////////////////////////////////////////////////////// 

								    #ifdef VERBOSE
									debug_say ("Agegroup 1 after MinorGC:\n");
									for (int i = 0;  i < MAX_PLAIN_SIBS;  i++) {
									  debug_say ("  %s: base = %#x, oldTop = %#x, tospace.used_end = %#x\n",
									    sib_name__global[i+1], age1->sib[i]->tospace,
									    age1->sib[i]->oldTop, age1->sib[i]->tospace.used_end);
									}
								    #endif

								    // Cleaner statistics stuff:
								    {
									long bytes_copied = 0;

									for (int i = 0;  i < MAX_PLAIN_SIBS;  i++) {
									    //
									    int bytes = (Vunt) age1->sib[ i ]->tospace.used_end - age1_tospace_top[ i ];

									    bytes_copied += bytes;

									    INCREASE_BIGCOUNTER( &heap->total_bytes_copied_to_sib[ 0 ][ i ], bytes );
									}

									total_bytes_allocated__global  +=  bytes_allocated;				// Never used otherwise.
									total_bytes_copied__global     +=  bytes_copied;				// Never used otherwise.

									#ifdef XXX
									    debug_say ("Minor GC: %d/%d (%5.2f%%) bytes copied; %d updates\n",
									    bytes_copied, bytes_allocated,
									    (bytes_allocated ? (double)(100*bytes_copied)/(double)bytes_allocated : 0.0),
									    update_count__global - nUpdates);
									#endif
								    }


								    #ifdef CHECK_HEAP
									check_heap( heap, 1 );								// check_heap		def in    src/c/heapcleaner/check-heap.c
								    #endif

}												// fun heapclean_agegroup0


static int   get_age_of_codechunk   (Val codechunk) {
    //       ====================
    //
    Sibid* b2s =  book_to_sibid__global;							// Cache global locally for speed.   book_to_sibid__global	def in    src/c/heapcleaner/heapcleaner-initialization.c

    Sibid dst_sibid =  SIBID_FOR_POINTER(b2s, codechunk );					// Get the Sibid tag for the ram-book containing the codechunk.

    int  book;
    for (book = GET_BOOK_CONTAINING_POINTEE( codechunk );
	//
	!SIBID_ID_IS_BIGCHUNK_RECORD( dst_sibid );
	//
	dst_sibid = b2s[ --book ]
    );

    Hugechunk_Quire*  
        //
	q =  (Hugechunk_Quire*)   ADDRESS_OF_BOOK( book );

    Hugechunk* hc =  get_hugechunk_holding_pointee( q, codechunk );				// get_hugechunk_holding_pointee		def in    src/c/h/heap.h

    return     hc->age;
}


//
static void   process_task_heap_changelog   (Task* task, Heap* heap) {
    //        ===========================
    // 
    // As tasks run, they note all stores into refcells
    // and vectors in the 'heap_changelog', a lisp-style list
    // of "CONS cells" -- (val,next) pointer-pairs.
    // 
    // We need this done because such stores into the heap
    // can introduce pointers from one agegroup into a
    // younger agegroup, which we need to take into account
    // when doing partial heapcleanings ("garbage collections").
    //
    // Our job here is to promote to agegroup 1 all agegroup0
    // values referenced by a refcell/vectorslot in the heap_changelog.

    Val this_heap_changelog_cell =  task->heap_changelog; 
    if (this_heap_changelog_cell == HEAP_CHANGELOG_NIL)   return;				// Abort quickly if no work to do.

    int updates        = 0;									// Heapcleaner statistics.
    Agegroup* age1     =  heap->agegroup[ 0 ];							// Cache heap entry for speed.
    Sibid* b2s         =  book_to_sibid__global;						// Cache global locally for speed.   book_to_sibid__global	def in    src/c/heapcleaner/heapcleaner-initialization.c

    while (this_heap_changelog_cell != HEAP_CHANGELOG_NIL) {					// Over all entries in the heap_changelog.
	//
	++updates;										// Heapcleaner statistics.

	Val* pointer   	         =  HEAP_CHANGELOG_HEAD( this_heap_changelog_cell );		// Get pointer to next updated refcell/vector slot to process.
	this_heap_changelog_cell =  HEAP_CHANGELOG_TAIL( this_heap_changelog_cell );		// Step to next cell in heap_changelog list.

	Val pointee = *pointer;									// Get contents of updated refcell/vectorslot.

	if (!IS_POINTER( pointee ))   continue;							// Ignore refcells and vectorslots containing Tagged_Int values.

	Sibid src_sibid =  SIBID_FOR_POINTER(b2s, pointer );					// Get the Sibid tag for the ram-book containing the refcell/vectorslot.	Sibid  def in    src/c/h/sibid.h

	if (src_sibid == AGEGROUP0_SIBID)    continue;						// Ignore updates to agegroup0      refcells and vectorslots.
	if (BOOK_IS_UNMAPPED( src_sibid ))  continue;						// Ignore updates to runtime-global refcells and vectorslots, which are handled elsewhere.

	Sibid dst_sibid =  SIBID_FOR_POINTER(b2s, pointee );					// Get the Sibid tag for the ram-book containing the value referenced by the refcell/vectorslot.
	//
	int src_age =  GET_AGE_FROM_SIBID( src_sibid );						// agegroup of the updated refcell/vectorslot.
	int dst_age =  GET_AGE_FROM_SIBID( dst_sibid );						// agegroup of the chunk that the refcell/vectorslot points to.

	if (!SIBID_KIND_IS_CODE( dst_sibid )) {
	    //
	    if (dst_age == AGEGROUP0) {
		//
		*pointer =  forward_agegroup0_chunk_to_agegroup1( age1, pointee,task, 1);	// Promote pointee to agegroup 1.
		dst_age = 1;									// Remember pointee now has age 1, not 0.
		//
	    }

	} else {										// Refcell/vector slot is pointing to code.	

	    if (dst_age >= src_age)   continue;

            dst_age =  get_age_of_codechunk( pointee );
	}

	// Maybe update min_age value for
	// the card containing 'pointer':
	//
	if (src_age > dst_age) {
	    //
	    MAYBE_UPDATE_CARD_MIN_AGE_PER_POINTER(						// MAYBE_UPDATE_CARD_MIN_AGE_PER_POINTER	def in    src/c/h/coarse-inter-agegroup-pointers-map.h
		//
		heap->agegroup[ src_age-1 ]->coarse_inter_agegroup_pointers_map,
		pointer,
		dst_age
	    );
	}
    }

    update_count__global += updates;								// Cleaner statistics.  Apparently never used.

    task->heap_changelog =  HEAP_CHANGELOG_NIL;							// We're done with heap_changelog so clear it.

}												// fun process_task_heap_changelog

//
static Bool   sweep_agegroup1_sib_tospace   (Agegroup* ag1,  int ilk, Task* task)   {		// Called only from copy_all_remaining_reachable_values_in_agegroup0_to_agegroup1 (below).
    //        ===========================							// 'task' arg is only for debugging, can be dropped in production code.
    //
    // This is a dedicated helper fn for the below fn
    // copy_all_remaining_reachable_values_in_agegroup0_to_agegroup1
    // -- see comments there.

    Sibid* b2s =  book_to_sibid__global;							// Cache global locally for speed.   book_to_sibid__global	def in    src/c/heapcleaner/heapcleaner-initialization.c
    Sib*   sib =  ag1->sib[ ilk ];								// Find sib to scan.

    Bool   progress =  FALSE;

    Val* p = sib->tospace.swept_end;								// Pick up scanning this sib where we last left off.
    if  (p < sib->tospace.used_end) {
	//
	progress = TRUE;

        Val*q;
	do {
	    for (q = sib->tospace.used_end;  p < q;  p++) {					// Check all words in buffer.
		//
	      forward_to_agegroup1_if_in_agegroup0(b2s, ag1, p, task);				// If current agegroup 1 word points to an agegroup0 value, copy that value into agegroup 1.
	    }
	} while (q != sib->tospace.used_end);							// If the above loop has added stuff to our agegroup 1 buffer, process that stuff too.

	sib->tospace.swept_end = q;								// Remember where we left off. (Current end of buffer.)
    }

    return progress;
}

//
static void   copy_all_remaining_reachable_values_in_agegroup0_to_agegroup1   (Agegroup* ag1, Task* task)   {				// 'task' arg is only for debugging, can be removed in production use.
    //        =============================================================
    //
    // We have now copied from agegroup0 to agegroup1
    // all values directly reachable from registers,
    // global variables etc.
    //
    // The newly copied values in agegroup1 are packed
    // contiguously in memory.
    //
    // We now treat those contiguously packed values
    // as a work queue:  Any agegroup0 values referenced
    // within this work queue must also be copied to
    // agegroup1, and any agegroup0 values within them
    // and so on recursively. Thus, we consume our work
    // queue from one end while appending to it from the
    // other end.
    //
    // The great advantage of this approach is that our
    // complete recursive-copy state is encoded in two
    // pointers to the two ends of the workqueue.
    //
    // Since our to-space (agegroup1) is actually divided
    // into multiple sibs (buffers), we actually wind up
    // with one workqueue per sib, which adds a bit of
    // complexity, but not a whole lot.
    //
    //
    // Note that normally we would have to do bookkeeping to
    // keep track of refcell and vector pointers into younger
    // agegroups, but since we are only dealing with agegroup0
    // here there *are* no younger agegroups to worry about,
    // so we don't have to do anything special for the vector sib.

    Bool making_progress = TRUE;

    while (making_progress) {
	//
	making_progress = FALSE;
	//										// Since NONPTR_DATA_SIB contains no pointers we can ignore it here.
	making_progress |=  sweep_agegroup1_sib_tospace(ag1, RO_POINTERS_SIB, task );
	making_progress |=  sweep_agegroup1_sib_tospace(ag1,   RO_CONSCELL_SIB, task );
	making_progress |=  sweep_agegroup1_sib_tospace(ag1, RW_POINTERS_SIB, task );
    };
}											// fun copy_all_remaining_reachable_values_in_agegroup0_to_agegroup1.


//
static Val   forward_agegroup0_chunk_to_agegroup1   (Agegroup* ag1,  Val v, Task* task, int caller)   {		// 'task' arg is only for debugging, can be removed in production use.
    //       =====================================
    // 
    // Forward pair/record/vector/string 'v' from agegroup0 to agegroup 1.
    // This involves:
    // 
    //   o Duplicating v in the appropriate agegroup 1 to-space buffer.
    //   o Setting v's tagword to FORWARDED_CHUNK_TAGWORD.
    //   o Setting v's first slot to point to the duplicate.
    //   o Returning a pointer to the duplicate.

    Val*           new_chunk;
    Vunt  len_in_words;
    Sib*           sib;

    Val*  chunk =   PTR_CAST(Val*, v);
    Val tagword =   chunk[-1];


    switch (GET_BTAG_FROM_TAGWORD( tagword )) {
    //
    case PAIRS_AND_RECORDS_BTAG:
	//
	len_in_words =  GET_LENGTH_IN_WORDS_FROM_TAGWORD( tagword );

	#ifdef NO_PAIR_STRIP							// 'NO_PAIR_STRIP' appears nowhere else in the codebase.
	    sib = ag1->sib[RO_POINTERS_SIB];
	#else
	    if (len_in_words != 2) {
		sib = ag1->sib[ RO_POINTERS_SIB ];					// This v is not a pair, so fall through to default code.
	    } else {
		//								// This v is a pair, so we'll use special-case code.
		sib = ag1->sib[ RO_CONSCELL_SIB ];					// We'll copy it into the dedicated pairs-only sib in agegroup1.
		new_chunk = sib->tospace.used_end;			// Where to copy it in that sib.
		sib->tospace.used_end += 2;			// Allocate the space for it.
		new_chunk[0] = chunk[0];					// Copy first  word of pair.
		new_chunk[1] = chunk[1];					// Copy second word of pair.
										// Notice that we don't need to copy the tagword -- it is implicit in the fact that we're in the pairsib.
		// Set up the forward pointer in the old pair:
		//
		chunk[-1] = FORWARDED_CHUNK_TAGWORD;
		chunk[0] = (Val)(Punt)new_chunk;
		return PTR_CAST( Val, new_chunk );				// Done!
	    }
        #endif
	break;


    case RO_VECTOR_HEADER_BTAG:
    case RW_VECTOR_HEADER_BTAG:
	//
	len_in_words =  2;
	//
	sib = ag1->sib[ RO_POINTERS_SIB ];
	break;									// Fall through to generic-case code.


    case RW_VECTOR_DATA_BTAG:
	//
	len_in_words =  GET_LENGTH_IN_WORDS_FROM_TAGWORD( tagword );
	//
	sib = ag1->sib[ RW_POINTERS_SIB ];					// The RW_POINTERS_SIB allows updates, which the RO_POINTERS_SIB does not.
	break;									// Fall through to generic-case code.


    case FOUR_BYTE_ALIGNED_NONPOINTER_DATA_BTAG:
	//
	len_in_words = GET_LENGTH_IN_WORDS_FROM_TAGWORD( tagword );
	//
	sib = ag1->sib[ NONPTR_DATA_SIB ];
	break;									// Fall through to generic-case code.


    case EIGHT_BYTE_ALIGNED_NONPOINTER_DATA_BTAG:
	//
	len_in_words = GET_LENGTH_IN_WORDS_FROM_TAGWORD( tagword );
	//
	sib = ag1->sib[ NONPTR_DATA_SIB ];
	//
	#ifdef ALIGN_FLOAT64S
	#  ifdef CHECK_HEAP
		if (((Punt)sib->tospace.used_end & WORD_BYTESIZE) == 0) {
		    //
		    *sib->tospace.used_end++ = (Val) 0;
		}
	#  else
		sib->tospace.used_end = (Val*) (((Punt)sib->tospace.used_end) | WORD_BYTESIZE);
	#  endif
	#endif
	break;									// Fall through to generic-case code.

    case WEAK_POINTER_OR_SUSPENSION_BTAG:
	//
	return forward_special_chunk ( ag1, chunk, tagword );

    case FORWARDED_CHUNK_BTAG:
	//
	return PTR_CAST( Val, FOLLOW_FORWARDING_POINTER(chunk));				// We've already copied this one to agegroup1, so just return pointer to copy.

    default:
	log_if("bad chunk tag %d, chunk = %#x, tagword = %#x   -- forward_agegroup0_chunk_to_agegroup1() in src/c/heapcleaner/heapclean-agegroup0.c", GET_BTAG_FROM_TAGWORD(tagword), chunk, tagword);
	log_if("forward_agegroup0_chunk_to_agegroup1 was called by %s", caller ? "process_task_heap_changelog" : "forward_to_agegroup1_if_in_agegroup0");
	dump_task(task,"forward_agegroup0_chunk_to_agegroup1/default");
	die ("bad chunk tag %d, chunk = %#x, tagword = %#x   -- forward_agegroup0_chunk_to_agegroup1() in src/c/heapcleaner/heapclean-agegroup0.c", GET_BTAG_FROM_TAGWORD(tagword), chunk, tagword);
	exit(1);									// Cannot execute -- just to quiet gcc -Wall.
    }

    // Make an agegroup1 copy of the chunk
    // in the appropriate agegroup1 sib (buffer):
    //
    new_chunk = sib->tospace.used_end;				// Figure out where copy will be located.
    sib->tospace.used_end += (len_in_words + 1);			// Allocate space needed by the copy.
    *new_chunk++ = tagword;							// Install tagword at start of copy.  (Note that 'sib' cannot be RO_CONSCELL_SIB, we handled that case above.)
    ASSERT( sib->tospace.used_end <= sib->tospace.limit );

    COPYLOOP(chunk, new_chunk, len_in_words);					// Copy over the rest of the chunk.
										// COPYLOOP	def in   src/c/heapcleaner/copy-loop.h
    // Set up the forwarding pointer in the original
    // to the copy and return the new chunk:
    //
    chunk[-1] =  FORWARDED_CHUNK_TAGWORD;
    chunk[ 0] =  (Val) (Punt) new_chunk;

    return PTR_CAST( Val, new_chunk );
}										// fun forward_agegroup0_chunk_to_agegroup1

//
static Val   forward_special_chunk   (Agegroup* ag1,  Val* chunk,   Val tagword)   {
    //       =====================
    // 
    // Forward a special chunk (suspension or weak pointer).

    Sib*  sib =  ag1->sib[ RW_POINTERS_SIB ];						// Special chunks can be updated (modified)
											// so they have to go in RW_POINTERS_SIB.
    Val*  new_chunk = sib->tospace.used_end;

    sib->tospace.used_end += SPECIAL_CHUNK_SIZE_IN_WORDS;			// All specials are two words.

    switch (GET_LENGTH_IN_WORDS_FROM_TAGWORD( tagword )) {
        //
    case EVALUATED_LAZY_SUSPENSION_CTAG:
    case UNEVALUATED_LAZY_SUSPENSION_CTAG:
        //
	*new_chunk++ = tagword;
	*new_chunk = *chunk;
	break;

    case WEAK_POINTER_CTAG:
        {
      	    //
	    Val	v = *chunk;
									    #ifdef DEBUG_WEAKREFS
										debug_say ("MinorGC: weak [%#x ==> %#x] --> %#x", chunk, new_chunk+1, v);
									    #endif

	    if (! IS_POINTER( v )) {
										#ifdef DEBUG_WEAKREFS
										debug_say (" unboxed\n");
										#endif

	        // Weak references to unboxed chunks (i.e., immediate Int31)
		// can never be nullified, since Int31 values, being stored
		// in-pointer, take no actual heapspace and thus cannot actually
		// ever get garbage-collected.  Consequently, we can just copy
		// such weakrefs over and skip the rest of our usual processing:
                //
		new_chunk[0] = WEAKREF_TAGWORD;
		new_chunk[1] = v;

		++new_chunk;

	    } else {

		Sibid sibid =  SIBID_FOR_POINTER( book_to_sibid__global, v );
		Val*  vp    =  PTR_CAST( Val*, v );

		if (sibid != AGEGROUP0_SIBID) {

		    // Weakref points to a value in an older heap agegroup.
		    // Since we are only heapcleaning agegroup0 in
		    // this file, the referenced value cannot get
		    // garbage-collected this pass, so we can skip
		    // the usual work to check for that and if necessary
		    // null out the weakref:
		    //
										    #ifdef DEBUG_WEAKREFS
											debug_say (" old chunk\n");
										    #endif

		    new_chunk[0] =  WEAKREF_TAGWORD;
		    new_chunk[1] =  v;

		    ++new_chunk;

		} else {

		    //
		    if (vp[-1] == FORWARDED_CHUNK_TAGWORD) {
		        //
			// Reference to a chunk that has already been forwarded.
			// Note that we have to put the pointer to the non-forwarded
			// copy of the chunk (i.e, v) into the to-space copy
			// of the weak pointer, since the heapcleaner has the invariant
			// that it never sees to-space pointers during sweeping.
											#ifdef DEBUG_WEAKREFS
											    debug_say (" already forwarded to %#x\n", PTR_CAST( Val, FOLLOW_FORWARDING_POINTER(vp)));
											#endif

			new_chunk[0] =  WEAKREF_TAGWORD;
			new_chunk[1] =  v;

			++new_chunk;

		    } else {

			// This is the important case: We are copying a weakref
			// of an agegroup0 value.  That agegroup0 value might get
			// get garbage-collected this pass; if it does, we must null
			// out the weakref.
			//
			// To do this efficiently, as we copy such weakrefs from
			// agegroup0 into agegroup1 we chain them togther via
			// their tagword fields with the root pointer kept
                        // in ag1->heap->weakrefs_forwarded_during_heapcleaning.
			//
			// At the end of heapcleaning we will consume this chain of
			// weakrefs in null_out_newly_dead_weakrefs() where					// null_out_newly_dead_weakrefs	is from   src/c/heapcleaner/heapcleaner-stuff.c
			// we will null out any newly dead weakrefs and then
			// replace the chainlinks with valid tagwords -- either
			// WEAKREF_TAGWORD or NULLED_WEAKREF_TAGWORD,
			// as appropriate, thus erasing our weakref chain and
			// restoring sanity.
			//
                        // We mark the chunk reference field in the forwarded copy
			// to make it look like an Tagged_Int so that the to-space
			// sweeper does not follow the weak reference.
											#ifdef DEBUG_WEAKREFS
											    debug_say (" forward\n");
											#endif

			new_chunk[0] =  MARK_POINTER(PTR_CAST( Val, ag1->heap->weakrefs_forwarded_during_heapcleaning ));		// MARK_POINTER just sets the low bit to 1, making it look like an Int31 value
			new_chunk[1] =  MARK_POINTER( vp );										// MARK_POINTER		is from   src/c/h/heap-tags.h

			ag1->heap->weakrefs_forwarded_during_heapcleaning =  new_chunk;

			++new_chunk;
		    }
		}
	    }
	}
	break;

    case NULLED_WEAK_POINTER_CTAG:					// Shouldn't happen in agegroup0.
    default:
	die (
            "strange/unexpected special chunk @ %#x; tagword = %#x\n",
            chunk, tagword
	);
    }								// switch (GET_LENGTH_IN_WORDS_FROM_TAGWORD(tagword))

    chunk[-1] =  FORWARDED_CHUNK_TAGWORD;
    chunk[ 0] =  (Val) (Punt) new_chunk;

    return   PTR_CAST( Val, new_chunk );
}								// fun forward_special_chunk


// COPYRIGHT (c) 1993 by AT&T Bell Laboratories.
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
# outline-regexp: "[a-z]"			 		 	#
# End:									 #
##########################################################################
*/
