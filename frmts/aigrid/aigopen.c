/******************************************************************************
 * $Id$
 *
 * Project:  Arc/Info Binary Grid Translator
 * Purpose:  Grid file access cover API for non-GDAL use.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.6  1999/07/23 14:04:34  warmerda
 * Removed extra printf.
 *
 * Revision 1.5  1999/06/28 01:21:39  warmerda
 * added support for selecting .adf files as well as coverage dir
 *
 * Revision 1.4  1999/05/17 01:52:35  warmerda
 * Fixed argument type problem.
 *
 * Revision 1.3  1999/04/21 16:51:30  warmerda
 * fixed up floating point support
 *
 * Revision 1.2  1999/02/04 22:15:33  warmerda
 * fleshed out implementation
 *
 * Revision 1.1  1999/02/03 14:12:56  warmerda
 * New
 */

#include "aigrid.h"

/************************************************************************/
/*                              AIGOpen()                               */
/************************************************************************/

AIGInfo_t *AIGOpen( const char * pszInputName, const char * pszAccess )

{
    AIGInfo_t	*psInfo;
    char	*pszHDRFilename;
    char        *pszCoverName;

/* -------------------------------------------------------------------- */
/*      If the pass name ends in .adf assume a file within the          */
/*      coverage has been selected, and strip that off the coverage     */
/*      name.                                                           */
/* -------------------------------------------------------------------- */
    pszCoverName = CPLStrdup( pszInputName );
    if( EQUAL(pszCoverName+strlen(pszCoverName)-4, ".adf") )
    {
        int      i;

        for( i = strlen(pszCoverName)-1; i > 0; i-- )
        {
            if( pszCoverName[i] == '\\' || pszCoverName[i] == '/' )
            {
                pszCoverName[i] = '\0';
                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Allocate info structure.                                        */
/* -------------------------------------------------------------------- */
    psInfo = (AIGInfo_t *) CPLCalloc(sizeof(AIGInfo_t),1);

/* -------------------------------------------------------------------- */
/*      Read the header file.                                           */
/* -------------------------------------------------------------------- */
    if( AIGReadHeader( pszCoverName, psInfo ) != CE_None )
    {
        CPLFree( pszCoverName );
        CPLFree( psInfo );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the file w001001.adf file itself.                          */
/* -------------------------------------------------------------------- */
    pszHDRFilename = (char *) CPLMalloc(strlen(pszCoverName)+40);
    sprintf( pszHDRFilename, "%s/w001001.adf", pszCoverName );

    psInfo->fpGrid = VSIFOpen( pszHDRFilename, "rb" );
    
    if( psInfo->fpGrid == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open grid file:\n%s\n",
                  pszHDRFilename );

        CPLFree( psInfo );
        CPLFree( pszHDRFilename );
        CPLFree( pszCoverName );
        return( NULL );
    }

    CPLFree( pszHDRFilename );
    pszHDRFilename = NULL;
    
/* -------------------------------------------------------------------- */
/*      Read the block index file.                                      */
/* -------------------------------------------------------------------- */
    if( AIGReadBlockIndex( pszCoverName, psInfo ) != CE_None )
    {
        VSIFClose( psInfo->fpGrid );
        
        CPLFree( psInfo );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the extents.                                               */
/* -------------------------------------------------------------------- */
    if( AIGReadBounds( pszCoverName, psInfo ) != CE_None )
    {
        VSIFClose( psInfo->fpGrid );
        
        CPLFree( psInfo );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the statistics.                                            */
/* -------------------------------------------------------------------- */
    if( AIGReadStatistics( pszCoverName, psInfo ) != CE_None )
    {
        VSIFClose( psInfo->fpGrid );
        
        CPLFree( psInfo );
        return NULL;
    }

    CPLFree( pszCoverName );
    pszCoverName = NULL;

/* -------------------------------------------------------------------- */
/*      Compute the number of pixels and lines.                         */
/* -------------------------------------------------------------------- */
    psInfo->nPixels = (int)
        (psInfo->dfURX - psInfo->dfLLX) / psInfo->dfCellSizeX;
    psInfo->nLines = (int)
        (psInfo->dfURY - psInfo->dfLLY) / psInfo->dfCellSizeY;
    
    return( psInfo );
}

/************************************************************************/
/*                            AIGReadTile()                             */
/************************************************************************/

CPLErr AIGReadTile( AIGInfo_t * psInfo, int nBlockXOff, int nBlockYOff,
                    GUInt32 *panData )

{
    int		nBlockID;
    CPLErr	eErr;

    nBlockID = nBlockXOff + nBlockYOff * psInfo->nBlocksPerRow;
    if( nBlockID < 0 || nBlockID >= psInfo->nBlocks )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }
    
    eErr = AIGReadBlock( psInfo->fpGrid,
                         psInfo->panBlockOffset[nBlockID],
                         psInfo->panBlockSize[nBlockID],
                         psInfo->nBlockXSize, psInfo->nBlockYSize,
                         panData, psInfo->nCellType );

    if( eErr == CE_None && psInfo->nCellType == AIG_CELLTYPE_FLOAT )
    {
        float	*pafData = (float *) panData;
        int	i, nPixels = psInfo->nBlockXSize * psInfo->nBlockYSize;

        for( i = 0; i < nPixels; i++ )
        {
            panData[i] = (int) pafData[i];
        }
    }

    return( eErr );
}

/************************************************************************/
/*                          AIGReadFloatTile()                          */
/************************************************************************/

CPLErr AIGReadFloatTile( AIGInfo_t * psInfo, int nBlockXOff, int nBlockYOff,
                         float *pafData )

{
    int		nBlockID;
    CPLErr	eErr;

    nBlockID = nBlockXOff + nBlockYOff * psInfo->nBlocksPerRow;
    if( nBlockID < 0 || nBlockID >= psInfo->nBlocks )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }
    
    eErr = AIGReadBlock( psInfo->fpGrid,
                         psInfo->panBlockOffset[nBlockID],
                         psInfo->panBlockSize[nBlockID],
                         psInfo->nBlockXSize, psInfo->nBlockYSize,
                         (GUInt32 *) pafData, psInfo->nCellType );

    if( eErr == CE_None && psInfo->nCellType == AIG_CELLTYPE_INT )
    {
        GUInt32	*panData = (GUInt32 *) pafData;
        int	i, nPixels = psInfo->nBlockXSize * psInfo->nBlockYSize;

        for( i = 0; i < nPixels; i++ )
        {
            pafData[i] = panData[i];
        }
    }

    return( eErr );
}

/************************************************************************/
/*                              AIGClose()                              */
/************************************************************************/

void AIGClose( AIGInfo_t * psInfo )

{
    VSIFClose( psInfo->fpGrid );

    CPLFree( psInfo->panBlockOffset );
    CPLFree( psInfo->panBlockSize );
    CPLFree( psInfo );
}
