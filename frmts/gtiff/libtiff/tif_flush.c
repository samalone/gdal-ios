/* $Header: /cvsroot/osrs/libtiff/libtiff/tif_flush.c,v 1.1.1.1 1999/07/27 21:50:27 mike Exp $ */

/*
 * Copyright (c) 1988-1997 Sam Leffler
 * Copyright (c) 1991-1997 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 */

/*
 * TIFF Library.
 */
#include "tiffiop.h"

int
TIFFFlush(TIFF* tif)
{

	if (tif->tif_mode != O_RDONLY) {
		if (!TIFFFlushData(tif))
			return (0);
		if ((tif->tif_flags & TIFF_DIRTYDIRECT) &&
		    !TIFFWriteDirectory(tif))
			return (0);
	}
	return (1);
}

/*
 * Flush buffered data to the file.
 */
int
TIFFFlushData(TIFF* tif)
{
	if ((tif->tif_flags & TIFF_BEENWRITING) == 0)
		return (0);
	if (tif->tif_flags & TIFF_POSTENCODE) {
		tif->tif_flags &= ~TIFF_POSTENCODE;
		if (!(*tif->tif_postencode)(tif))
			return (0);
	}
	return (TIFFFlushData1(tif));
}
