// cvCreateImage.c

#include "../../mythryl-config.h"

#if HAVE_OPENCV_CV_H
#include <opencv/cv.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "runtime-base.h"
#include "runtime-values.h"
#include "make-strings-and-vectors-etc.h"
#include "cfun-proto-list.h"
#include "../raise-error.h"

/* _lib7_OpenCV_cvCreateImage : String -> Image
 *
 */
Val

_lib7_OpenCV_cvCreateImage (Task *task, Val arg)
{
#if HAVE_OPENCV_CV_H && HAVE_LIBCV
  /*
    int height =  700;
    int width  = 1000;
    cvCreateImage( cvSize(1000,700),8,3);
    return HEAP_VOID;
  */

    IplImage img;

    Val data   =  make_biwordslots_vector_sized_in_bytes__may_heapclean(  task, &img, sizeof(img), NULL);

    return  make_vector_header(task,  UNT8_RO_VECTOR_TAGWORD, data, sizeof(img));

#else

    extern char* no_opencv_support_in_runtime;
    //
    return RAISE_ERROR__MAY_HEAPCLEAN(task, no_opencv_support_in_runtime, NULL);

#endif
}



/* Notes:

   IplImage      is defined in		cxcore/include/cxtypes.h

   cvCreateImage is defined in		cxcore/src/cxarray.cpp



 */

// Code by Jeff Prothero: Copyright (c) 2010-2012,
// released under Gnu Public Licence version 3.


