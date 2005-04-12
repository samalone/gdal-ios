/******************************************************************************
 * $Id$
 *
 * Project:  GDAL 
 * Purpose:  ECW Driver: virtualized io stream declaration.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************
 *
 * $Log$
 * Revision 1.10  2005/04/12 03:58:34  fwarmerdam
 * turn ownership of fpVSIL over to vsiiostream
 *
 * Revision 1.9  2005/04/11 13:52:50  fwarmerdam
 * added proper handling of iostream cleanup from David Carter
 *
 * Revision 1.8  2005/04/04 14:13:54  fwarmerdam
 * Return true for 0 byte reads.
 *
 * Revision 1.7  2005/04/02 21:17:01  fwarmerdam
 * add extra include files
 *
 * Revision 1.6  2005/03/10 14:48:42  fwarmerdam
 * Return false on read requests of zero bytes, at David's suggestion.
 *
 * Revision 1.5  2005/02/24 15:11:42  fwarmerdam
 * fixed debug arguments in Read()
 *
 * Revision 1.4  2005/02/07 22:53:54  fwarmerdam
 * added preliminary Create support for JP2ECW driver
 *
 * Revision 1.3  2004/12/21 16:11:11  fwarmerdam
 * hacked Read() method error reporting to avoid NPJE issue
 *
 * Revision 1.2  2004/12/20 22:17:57  fwarmerdam
 * adjusted for two create copy entry points
 *
 * Revision 1.1  2004/12/10 19:15:34  fwarmerdam
 * New
 *
 */

#ifndef VSIIOSTREAM_H_INCLUDED
#define VSIIOSTREAM_H_INCLUDED

#include "cpl_vsi.h"
#include "gdal_priv.h"
#include "gdal_frmts.h"

#ifdef FRMT_ecw

/* -------------------------------------------------------------------- */
/*      These definitions aren't really specific to the VSIIOStream,    */
/*      but are shared amoung the ECW driver modules.                   */
/* -------------------------------------------------------------------- */
#include <NCSECWClient.h>
#include <NCSECWCompressClient.h>
#include <NCSErrors.h>
#include <NCSFile.h>
#include <NCSJP2File.h>
#include <NCSJP2FileView.h>

/* As of July 2002 only uncompress support is available on Unix */
#define HAVE_COMPRESS

#ifdef HAVE_COMPRESS
GDALDataset *
ECWCreateCopyECW( const char * pszFilename, GDALDataset *poSrcDS, 
                 int bStrict, char ** papszOptions, 
                 GDALProgressFunc pfnProgress, void * pProgressData );
GDALDataset *
ECWCreateCopyJPEG2000( const char * pszFilename, GDALDataset *poSrcDS, 
                 int bStrict, char ** papszOptions, 
                 GDALProgressFunc pfnProgress, void * pProgressData );

GDALDataset *
ECWCreateECW( const char * pszFilename, int nXSize, int nYSize, int nBands, 
              GDALDataType eType, char **papszOptions );
GDALDataset *
ECWCreateJPEG2000(const char *pszFilename, int nXSize, int nYSize, int nBands, 
                  GDALDataType eType, char **papszOptions );
#endif

CPL_C_START
char **ECWGetCSList(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                             VSIIOStream                              */
/* ==================================================================== */
/************************************************************************/

class VSIIOStream : public CNCSJPCIOStream

{
  public:
    
    INT64    startOfJPData;
    INT64    lengthOfJPData;
    FILE    *fpVSIL;
    int      bWritable;
	int      nFileViewCount;
    char     *pszFilename;

    VSIIOStream() {
        nFileViewCount = 0;
        startOfJPData = 0;
        lengthOfJPData = -1;
        fpVSIL = NULL;
    }
    virtual ~VSIIOStream() {
        Close();
        if( fpVSIL != NULL )
        {
            VSIFCloseL( fpVSIL );
            fpVSIL = NULL;
        }
    }

    virtual CNCSError Access( FILE *fpVSILIn, BOOLEAN bWrite,
                              const char *pszFilename, 
                              INT64 start, INT64 size = -1) {

        fpVSIL = fpVSILIn;
        startOfJPData = start;
        lengthOfJPData = size;
        bWritable = bWrite;
        VSIFSeekL(fpVSIL, startOfJPData, SEEK_SET);

        return(CNCSJPCIOStream::Open((char *)pszFilename, (bool) bWrite));
    }

    virtual bool NCS_FASTCALL Seek() {
        return(true);
    }
    
    virtual bool NCS_FASTCALL Seek(INT64 offset, Origin origin = CURRENT) {
        switch(origin) {
            case START:
                return(0 == VSIFSeekL(fpVSIL, offset+startOfJPData, SEEK_SET));

            case CURRENT:
                return(0 == VSIFSeekL(fpVSIL, offset, SEEK_CUR));
                
            case END:
                return(0 == VSIFSeekL(fpVSIL, offset, SEEK_END));
        }
        
        return(false);
    }

    virtual INT64 NCS_FASTCALL Tell() {
        return VSIFTellL( fpVSIL ) - startOfJPData;
    }

    virtual INT64 NCS_FASTCALL Size() {
        if( lengthOfJPData != -1 )
            return lengthOfJPData;
        else
        {
            INT64 curPos = Tell(), size;

            Seek( 0, END );
            size = Tell();
            Seek( curPos, START );

            return size;
        }
    }

    virtual bool NCS_FASTCALL Read(void* buffer, UINT32 count) {
        if( count == 0 )
            return true;

//        return(1 == VSIFReadL( buffer, count, 1, fpVSIL ) );

        // The following is a hack 
        if( VSIFReadL( buffer, count, 1, fpVSIL ) != 1 )
        {
            CPLDebug( "VSIIOSTREAM",
                      "Read(%d) failed @ %d, ignoring failure.",
                      count, (int) (VSIFTellL( fpVSIL ) - startOfJPData) );
        }
        
        return true;
    }

    virtual bool NCS_FASTCALL Write(void* buffer, UINT32 count) {
        if( count == 0 )
            return true;
        return(1 == VSIFWriteL(buffer, count, 1, fpVSIL));
    }
};

#endif /* def FRMT_ecw */

#endif /* ndef VSIIOSTREAM_H_INCLUDED */

