/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  GDAL GeoTIFF support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.133  2005/03/22 05:12:15  fwarmerdam
 * first crack at support 9-15 bit depths
 *
 * Revision 1.132  2005/03/07 14:23:49  fwarmerdam
 * Added PROFILE support in CreateCopy().
 *
 * Revision 1.131  2005/02/15 16:08:34  fwarmerdam
 * dont put out scale and offset if defaulted
 *
 * Revision 1.130  2005/02/10 04:31:11  fwarmerdam
 * added option to keep YCbCr in raw form
 *
 * Revision 1.129  2005/01/15 16:15:09  fwarmerdam
 * trimmed revision history.
 *
 * Revision 1.128  2005/01/15 16:13:17  fwarmerdam
 * Use GTiffRasterBand as base for 1bit and RGB band specialized types.
 * Generalized metadata support, include band metadata.
 * Added support for offset/scale as special metadata items.
 *
 * Revision 1.127  2004/11/25 14:28:55  fwarmerdam
 * Use a more accurate format for the nodata value.
 *
 * Revision 1.126  2004/11/12 17:02:08  gwalter
 * Use GDALDuplicateGCPs instead of malloc/memcpy
 * so that GCP Id and Info are duplicated
 * properly.
 *
 * Revision 1.125  2004/11/11 21:03:38  fwarmerdam
 * dont try and write tiepoint+pixelscale for southup images, bug 666
 *
 * Revision 1.124  2004/10/27 18:17:19  fwarmerdam
 * avoid variable sized arrays, not C++ standard
 *
 * Revision 1.123  2004/10/26 23:47:28  fwarmerdam
 * Added support for writing UInt16 colortables.
 *
 * Revision 1.122  2004/10/21 15:51:06  fwarmerdam
 * Still write geotiff coordinate system info in WriteGeoTIFFInfo()
 * even if we don't have a geotransform.
 *
 * Revision 1.121  2004/10/19 13:21:12  fwarmerdam
 * Be careful to avoid unused argument warnings.
 *
 * Revision 1.120  2004/10/19 13:18:17  fwarmerdam
 * Added GetInternalHandle() method to fetch hTIFF.
 *
 * Revision 1.119  2004/10/07 20:15:03  fwarmerdam
 * various improvements to alpha handling
 *
 * Revision 1.118  2004/10/06 13:11:21  fwarmerdam
 * added BigTIFF test
 *
 * Revision 1.117  2004/09/30 19:34:04  fwarmerdam
 * Fixed memory leak of codecs info.
 *
 * Revision 1.116  2004/09/25 05:21:27  fwarmerdam
 * report only available compression types if we can query
 *
 * Revision 1.115  2004/09/24 02:48:39  fwarmerdam
 * Try to avoid unnecessary calls to TIFFReadDirectory() when scanning
 * for overviews in files with only one directory.
 *
 * Revision 1.114  2004/09/08 15:33:18  warmerda
 * allow writing of projection without geotrasnform
 *
 * Revision 1.113  2004/08/25 19:10:28  warmerda
 * Try and write out ExtraSamples values if we have extra samples.
 * Apparently this is required by the TIFF spec!
 *
 * Revision 1.112  2004/08/14 00:05:33  warmerda
 * finished changes to support external overviews if file readonly
 *
 * Revision 1.111  2004/04/29 19:58:43  warmerda
 * export GTIFGetOGISDefn, and GTIFSetFromOGISDefn
 *
 * Revision 1.110  2004/04/21 14:00:18  warmerda
 * pass GTIF pointer into GTIFGetOGISDefn
 *
 * Revision 1.109  2004/04/12 09:13:18  dron
 * Use compression type of the base image to generate overviews
 * (requires libtiff >= 3.7.0).
 *
 * Revision 1.108  2004/03/01 18:36:47  warmerda
 * improve error checking for overview generation
 *
 * Revision 1.107  2004/02/19 14:31:44  dron
 * Replace COMPRESSION_DEFLATE with COMPRESSION_ADOBE_DEFLATE.
 *
 * Revision 1.106  2004/02/12 14:29:55  warmerda
 * Fiddled with rules when reading georeferencing information.  Now if there
 * are tiepoints and a transformation matrix, we will build a geotransform
 * *and* attached tiepoints.  Related to:
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=489
 *
 * Revision 1.105  2004/01/15 10:18:35  dron
 * PHOTOMETRIC option flag added. Few improvements in option parsing.
 *
 * Revision 1.104  2004/01/14 23:09:08  warmerda
 * Removed duplicate line.
 *
 * Revision 1.103  2004/01/14 23:06:09  warmerda
 * Improve calculation of 16bit color table values.
 *
 * Revision 1.102  2004/01/07 20:51:22  warmerda
 * Added Float64 to supported data types.
 *
 * Revision 1.101  2004/01/01 19:49:50  dron
 * Use uint16 insted of int16 for the count of tie points.
 */

#include "gdal_priv.h"
#define CPL_SERV_H_INCLUDED

#include "tiffio.h"
#include "xtiffio.h"
#include "geotiff.h"
#include "geo_normalize.h"
#include "tif_ovrcache.h"
#include "cpl_string.h"
#include "cpl_csv.h"
#include "cpl_minixml.h"

CPL_CVSID("$Id$");

CPL_C_START
char CPL_DLL *  GTIFGetOGISDefn( GTIF *, GTIFDefn * );
int  CPL_DLL   GTIFSetFromOGISDefn( GTIF *, const char * );
const char * GDALDefaultCSVFilename( const char *pszBasename );
CPL_C_END

static void GTiffOneTimeInit();
#define TIFFTAG_GDAL_METADATA  42112
#define TIFFTAG_GDAL_NODATA    42113

/************************************************************************/
/* ==================================================================== */
/*				GTiffDataset				*/
/* ==================================================================== */
/************************************************************************/

class GTiffRasterBand;
class GTiffRGBABand;
class GTiffBitmapBand;

class GTiffDataset : public GDALDataset
{
    friend class GTiffRasterBand;
    friend class GTiffRGBABand;
    friend class GTiffBitmapBand;
    friend class GTiffOddBitsBand;
    
    TIFF	*hTIFF;

    uint32      nDirOffset;
    int		bBase;

    uint16	nPlanarConfig;
    uint16	nSamplesPerPixel;
    uint16	nBitsPerSample;
    uint32	nRowsPerStrip;
    uint16	nPhotometric;
    uint16      nSampleFormat;
    uint16      nCompression;
    
    int		nBlocksPerBand;

    uint32      nBlockXSize;
    uint32	nBlockYSize;

    int		nLoadedBlock;		/* or tile */
    int         bLoadedBlockDirty;  
    GByte	*pabyBlockBuf;

    CPLErr      LoadBlockBuf( int );
    CPLErr      FlushBlockBuf();

    char	*pszProjection;
    double	adfGeoTransform[6];
    int		bGeoTransformValid;

    char	*pszTFWFilename;

    int		bNewDataset;            /* product of Create() */
    int         bTreatAsRGBA;
    int         bCrystalized;

    void	Crystalize();

    GDALColorTable *poColorTable;

    void	WriteGeoTIFFInfo();
    int		SetDirectory( uint32 nDirOffset = 0 );
    void        SetupTFW(const char *pszBasename);

    int		nOverviewCount;
    GTiffDataset **papoOverviewDS;

    int		nGCPCount;
    GDAL_GCP	*pasGCPList;

    int         IsBlockAvailable( int nBlockId );

    int 	bMetadataChanged;
    int         bGeoTIFFInfoChanged;
    int         bNoDataSet;
    int         bNoDataChanged;
    double      dfNoDataValue;

  public:
                 GTiffDataset();
                 ~GTiffDataset();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );
    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    CPLErr         SetGCPs( int, const GDAL_GCP *, const char * );

    virtual CPLErr IBuildOverviews( const char *, int, int *, int, int *, 
                                    GDALProgressFunc, void * );

    CPLErr	   OpenOffset( TIFF *, uint32 nDirOffset, int, GDALAccess );

    static GDALDataset *OpenDir( const char *pszFilename );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
    virtual void    FlushCache( void );

    virtual CPLErr  SetMetadata( char **, const char * = "" );
    virtual CPLErr  SetMetadataItem( const char*, const char*, 
                                     const char* = "" );
    virtual void   *GetInternalHandle( const char * );

    // only needed by createcopy and close code.
    static void	    WriteMetadata( GDALDataset *, TIFF * );
    static void	    WriteNoDataValue( TIFF *, double );
};

/************************************************************************/
/* ==================================================================== */
/*                            GTiffRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class GTiffRasterBand : public GDALRasterBand
{
    friend class GTiffDataset;

    GDALColorInterp         eBandInterp;

    int    bHaveOffsetScale;
    double dfOffset;
    double dfScale;

  public:

                   GTiffRasterBand( GTiffDataset *, int );

    // should override RasterIO eventually.
    
    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * ); 

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual CPLErr          SetColorTable( GDALColorTable * );
    virtual double	    GetNoDataValue( int * );
    virtual CPLErr	    SetNoDataValue( double );

    virtual double GetOffset( int *pbSuccess = NULL );
    virtual CPLErr SetOffset( double dfNewValue );
    virtual double GetScale( int *pbSuccess = NULL );
    virtual CPLErr SetScale( double dfNewValue );

    virtual CPLErr  SetMetadata( char **, const char * = "" );
    virtual CPLErr  SetMetadataItem( const char*, const char*, 
                                     const char* = "" );
    virtual int    GetOverviewCount();
    virtual GDALRasterBand *GetOverview( int );
};

/************************************************************************/
/*                           GTiffRasterBand()                          */
/************************************************************************/

GTiffRasterBand::GTiffRasterBand( GTiffDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;

    bHaveOffsetScale = FALSE;
    dfOffset = 0.0;
    dfScale = 1.0;

/* -------------------------------------------------------------------- */
/*      Get the GDAL data type.                                         */
/* -------------------------------------------------------------------- */
    uint16		nSampleFormat = poDS->nSampleFormat;

    eDataType = GDT_Unknown;

    if( poDS->nBitsPerSample <= 8 )
        eDataType = GDT_Byte;
    else if( poDS->nBitsPerSample <= 16 )
    {
        if( nSampleFormat == SAMPLEFORMAT_INT )
            eDataType = GDT_Int16;
        else
            eDataType = GDT_UInt16;
    }
    else if( poDS->nBitsPerSample == 32 )
    {
        if( nSampleFormat == SAMPLEFORMAT_COMPLEXINT )
            eDataType = GDT_CInt16;
        else if( nSampleFormat == SAMPLEFORMAT_IEEEFP )
            eDataType = GDT_Float32;
        else if( nSampleFormat == SAMPLEFORMAT_INT )
            eDataType = GDT_Int32;
        else
            eDataType = GDT_UInt32;
    }
    else if( poDS->nBitsPerSample == 64 )
    {
        if( nSampleFormat == SAMPLEFORMAT_IEEEFP )
            eDataType = GDT_Float64;
        else if( nSampleFormat == SAMPLEFORMAT_COMPLEXIEEEFP )
            eDataType = GDT_CFloat32;
        else if( nSampleFormat == SAMPLEFORMAT_COMPLEXINT )
            eDataType = GDT_CInt32;
    }
    else if( poDS->nBitsPerSample == 128 )
    {
        if( nSampleFormat == SAMPLEFORMAT_COMPLEXIEEEFP )
            eDataType = GDT_CFloat64;
    }

/* -------------------------------------------------------------------- */
/*      Try to work out band color interpretation.                      */
/* -------------------------------------------------------------------- */
    if( poDS->nPhotometric == PHOTOMETRIC_RGB )
    {
        if( nBand == 1 )
            eBandInterp = GCI_RedBand;
        else if( nBand == 2 )
            eBandInterp = GCI_GreenBand;
        else if( nBand == 3 )
            eBandInterp = GCI_BlueBand;
        else
        {
            uint16 *v;
            uint16 count = 0;

            if( TIFFGetField( poDS->hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v) )
            {
                if( nBand - 3 <= count && v[nBand-4] == EXTRASAMPLE_ASSOCALPHA )
                    eBandInterp = GCI_AlphaBand;
                else
                    eBandInterp = GCI_Undefined;
            }
            else if( nBand == 4 )
                eBandInterp = GCI_AlphaBand;
            else
                eBandInterp = GCI_Undefined;
        }
    }
    else if( poDS->nPhotometric == PHOTOMETRIC_YCBCR )
    {
        if( nBand == 1 )
            eBandInterp = GCI_YCbCr_YBand;
        else if( nBand == 2 )
            eBandInterp = GCI_YCbCr_CbBand;
        else if( nBand == 3 )
            eBandInterp = GCI_YCbCr_CrBand;
        else
        {
            uint16 *v;
            uint16 count = 0;

            if( TIFFGetField( poDS->hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v) )
            {
                if( nBand - 3 <= count && v[nBand-4] == EXTRASAMPLE_ASSOCALPHA )
                    eBandInterp = GCI_AlphaBand;
                else
                    eBandInterp = GCI_Undefined;
            }
            else if( nBand == 4 )
                eBandInterp = GCI_AlphaBand;
            else
                eBandInterp = GCI_Undefined;
        }
    }
    else if( poDS->nPhotometric == PHOTOMETRIC_MINISBLACK && nBand == 1 )
        eBandInterp = GCI_GrayIndex;
    else if( poDS->nPhotometric == PHOTOMETRIC_PALETTE && nBand == 1 ) 
        eBandInterp = GCI_PaletteIndex;
    else
    {
        uint16 *v;
        uint16 count = 0;

        if( TIFFGetField( poDS->hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v ) )
        {
            int nBaseSamples;
            nBaseSamples = poDS->nSamplesPerPixel - count;

            if( nBand > nBaseSamples 
                && v[nBand-nBaseSamples-1] == EXTRASAMPLE_ASSOCALPHA )
                eBandInterp = GCI_AlphaBand;
            else
                eBandInterp = GCI_Undefined;
        }
        else
            eBandInterp = GCI_Undefined;
    }

/* -------------------------------------------------------------------- */
/*	Establish block size for strip or tiles.			*/
/* -------------------------------------------------------------------- */
    nBlockXSize = poDS->nBlockXSize;
    nBlockYSize = poDS->nBlockYSize;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;
    int			nBlockBufSize, nBlockId, nBlockIdBand0;
    CPLErr		eErr = CE_None;

    poGDS->SetDirectory();

    if( TIFFIsTiled(poGDS->hTIFF) )
        nBlockBufSize = TIFFTileSize( poGDS->hTIFF );
    else
    {
        CPLAssert( nBlockXOff == 0 );
        nBlockBufSize = TIFFStripSize( poGDS->hTIFF );
    }

    nBlockIdBand0 = nBlockXOff + nBlockYOff * nBlocksPerRow;
    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlockId = nBlockIdBand0 + (nBand-1) * poGDS->nBlocksPerBand;
    else
        nBlockId = nBlockIdBand0;
        
/* -------------------------------------------------------------------- */
/*	Handle the case of a strip in a writable file that doesn't	*/
/*	exist yet, but that we want to read.  Just set to zeros and	*/
/*	return.								*/
/* -------------------------------------------------------------------- */
    if( poGDS->eAccess == GA_Update && !poGDS->IsBlockAvailable(nBlockId) )
    {
        memset( pImage, 0,
                nBlockXSize * nBlockYSize
                * GDALGetDataTypeSize(eDataType) / 8 );
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Handle simple case (separate, onesampleperpixel)		*/
/* -------------------------------------------------------------------- */
    if( poGDS->nBands == 1
        || poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
    {
        if( TIFFIsTiled( poGDS->hTIFF ) )
        {
            if( TIFFReadEncodedTile( poGDS->hTIFF, nBlockId, pImage,
                                     nBlockBufSize ) == -1 )
            {
                memset( pImage, 0, nBlockBufSize );
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadEncodedTile() failed.\n" );
                
                eErr = CE_Failure;
            }
        }
        else
        {
            if( TIFFReadEncodedStrip( poGDS->hTIFF, nBlockId, pImage,
                                      nBlockBufSize ) == -1 )
            {
                memset( pImage, 0, nBlockBufSize );
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadEncodedStrip() failed.\n" );
                
                eErr = CE_Failure;
            }
        }

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Load desired block                                              */
/* -------------------------------------------------------------------- */
    eErr = poGDS->LoadBlockBuf( nBlockId );
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Handle simple case of eight bit data, and pixel interleaving.   */
/* -------------------------------------------------------------------- */
    if( poGDS->nBitsPerSample == 8 )
    {
        int	i, nBlockPixels;
        GByte	*pabyImage;
        
        pabyImage = poGDS->pabyBlockBuf + nBand - 1;

        nBlockPixels = nBlockXSize * nBlockYSize;
        for( i = 0; i < nBlockPixels; i++ )
        {
            ((GByte *) pImage)[i] = *pabyImage;
            pabyImage += poGDS->nBands;
        }
    }
    else
    {
        int	i, nBlockPixels, nWordBytes;
        GByte	*pabyImage;

        nWordBytes = poGDS->nBitsPerSample / 8;
        pabyImage = poGDS->pabyBlockBuf + (nBand - 1) * nWordBytes;

        nBlockPixels = nBlockXSize * nBlockYSize;
        for( i = 0; i < nBlockPixels; i++ )
        {
            for( int j = 0; j < nWordBytes; j++ )
            {
                ((GByte *) pImage)[i*nWordBytes + j] = pabyImage[j];
            }
            pabyImage += poGDS->nBands * nWordBytes;
        }
    }

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/*                                                                      */
/*      This is still limited to writing stripped datasets.             */
/************************************************************************/

CPLErr GTiffRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                     void * pImage )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;
    int		nBlockId, nBlockBufSize;
    CPLErr      eErr = CE_None;

    poGDS->Crystalize();
    poGDS->SetDirectory();

    CPLAssert( poGDS != NULL
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != NULL );

/* -------------------------------------------------------------------- */
/*      Handle case of "separate" images                                */
/* -------------------------------------------------------------------- */
    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE
        || poGDS->nBands == 1 )
    {
        nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow
            + (nBand-1) * poGDS->nBlocksPerBand;
        
        if( TIFFIsTiled(poGDS->hTIFF) )
        {
            nBlockBufSize = TIFFTileSize( poGDS->hTIFF );
            if( TIFFWriteEncodedTile( poGDS->hTIFF, nBlockId, pImage, 
                                      nBlockBufSize ) == -1 )
                eErr = CE_Failure;
        }
        else
        {
            nBlockBufSize = TIFFStripSize( poGDS->hTIFF );
            if( TIFFWriteEncodedStrip( poGDS->hTIFF, nBlockId, pImage, 
                                       nBlockBufSize ) == -1 )
                eErr = CE_Failure;
        }

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Handle case of pixel interleaved (PLANARCONFIG_CONTIG) images.  */
/* -------------------------------------------------------------------- */

    nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;
        
    eErr = poGDS->LoadBlockBuf( nBlockId );
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Handle simple case of eight bit data, and pixel interleaving.   */
/* -------------------------------------------------------------------- */
    int	i, nBlockPixels, nWordBytes;
    GByte	*pabyImage;

    nWordBytes = poGDS->nBitsPerSample / 8;
    pabyImage = poGDS->pabyBlockBuf + (nBand - 1) * nWordBytes;
    
    nBlockPixels = nBlockXSize * nBlockYSize;
    for( i = 0; i < nBlockPixels; i++ )
    {
        for( int j = 0; j < nWordBytes; j++ )
        {
            pabyImage[j] = ((GByte *) pImage)[i*nWordBytes + j];
        }
        pabyImage += poGDS->nBands * nWordBytes;
    }

    poGDS->bLoadedBlockDirty = TRUE;

    return CE_None;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double GTiffRasterBand::GetOffset( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bHaveOffsetScale;
    return dfOffset;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr GTiffRasterBand::SetOffset( double dfNewValue )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

    poGDS->bMetadataChanged = TRUE;

    bHaveOffsetScale = TRUE;
    dfOffset = dfNewValue;
    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double GTiffRasterBand::GetScale( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bHaveOffsetScale;
    return dfScale;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr GTiffRasterBand::SetScale( double dfNewValue )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

    poGDS->bMetadataChanged = TRUE;

    bHaveOffsetScale = TRUE;
    dfScale = dfNewValue;
    return CE_None;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/
CPLErr GTiffRasterBand::SetMetadata( char ** papszMD, const char *pszDomain )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

    poGDS->bMetadataChanged = TRUE;
    return GDALRasterBand::SetMetadata( papszMD, pszDomain );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GTiffRasterBand::SetMetadataItem( const char *pszName, 
                                         const char *pszValue,
                                         const char *pszDomain )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

    poGDS->bMetadataChanged = TRUE;
    return GDALRasterBand::SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GTiffRasterBand::GetColorInterpretation()

{
    return eBandInterp;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GTiffRasterBand::GetColorTable()

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

    if( nBand == 1 )
        return poGDS->poColorTable;
    else
        return NULL;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr GTiffRasterBand::SetColorTable( GDALColorTable * poCT )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

/* -------------------------------------------------------------------- */
/*      Check if this is even a candidate for applying a PCT.           */
/* -------------------------------------------------------------------- */
    if( poGDS->bCrystalized )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "SetColorTable() not supported for existing TIFF files." );
        return CE_Failure;
    }

    if( poGDS->nSamplesPerPixel != 1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "SetColorTable() not supported for multi-sample TIFF files." );
        return CE_Failure;
    }
        
    if( eDataType != GDT_Byte && eDataType != GDT_UInt16 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "SetColorTable() only supported for Byte or UInt16 bands in TIFF format." );
        return CE_Failure;
    }
        
/* -------------------------------------------------------------------- */
/*      Write out the colortable, and update the configuration.         */
/* -------------------------------------------------------------------- */
    int nColors;

    if( eDataType == GDT_Byte )
        nColors = 256;
    else
        nColors = 65536;

    unsigned short *panTRed, *panTGreen, *panTBlue;

    panTRed = (unsigned short *) CPLMalloc(sizeof(unsigned short)*nColors);
    panTGreen = (unsigned short *) CPLMalloc(sizeof(unsigned short)*nColors);
    panTBlue = (unsigned short *) CPLMalloc(sizeof(unsigned short)*nColors);

    for( int iColor = 0; iColor < nColors; iColor++ )
    {
        if( iColor < poCT->GetColorEntryCount() )
        {
            GDALColorEntry  sRGB;
            
            poCT->GetColorEntryAsRGB( iColor, &sRGB );
            
            panTRed[iColor] = (unsigned short) (257 * sRGB.c1);
            panTGreen[iColor] = (unsigned short) (257 * sRGB.c2);
            panTBlue[iColor] = (unsigned short) (257 * sRGB.c3);
        }
        else
        {
            panTRed[iColor] = panTGreen[iColor] = panTBlue[iColor] = 0;
        }
    }

    TIFFSetField( poGDS->hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
    TIFFSetField( poGDS->hTIFF, TIFFTAG_COLORMAP, panTRed, panTGreen, panTBlue );

    CPLFree( panTRed );
    CPLFree( panTGreen );
    CPLFree( panTBlue );

    if( poGDS->poColorTable )
        delete poGDS->poColorTable;

    poGDS->poColorTable = poCT->Clone();

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GTiffRasterBand::GetNoDataValue( int * pbSuccess )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

    if( pbSuccess )
        *pbSuccess = poGDS->bNoDataSet;

    return poGDS->dfNoDataValue;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr GTiffRasterBand::SetNoDataValue( double dfNoData )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

    poGDS->bNoDataSet = TRUE;
    poGDS->bNoDataChanged = TRUE;
    poGDS->dfNoDataValue = dfNoData;

    return CE_None;
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int GTiffRasterBand::GetOverviewCount()

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

    if( poGDS->nOverviewCount > 0 )
        return poGDS->nOverviewCount;
    else
        return GDALRasterBand::GetOverviewCount();
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *GTiffRasterBand::GetOverview( int i )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

    if( poGDS->nOverviewCount > 0 )
    {
        if( i < 0 || i >= poGDS->nOverviewCount )
            return NULL;
        else
            return poGDS->papoOverviewDS[i]->GetRasterBand(nBand);
    }
    else
        return GDALRasterBand::GetOverview( i );
}

/************************************************************************/
/* ==================================================================== */
/*                             GTiffRGBABand                            */
/* ==================================================================== */
/************************************************************************/

class GTiffRGBABand : public GTiffRasterBand
{
    friend class GTiffDataset;

  public:

                   GTiffRGBABand( GTiffDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual GDALColorInterp GetColorInterpretation();
};


/************************************************************************/
/*                           GTiffRGBABand()                            */
/************************************************************************/

GTiffRGBABand::GTiffRGBABand( GTiffDataset *poDS, int nBand )
        : GTiffRasterBand( poDS, nBand )

{
    eDataType = GDT_Byte;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffRGBABand::IWriteBlock( int, int, void * )

{
    CPLError( CE_Failure, CPLE_AppDefined, 
              "RGBA interpreted raster bands are read-only." );
    return CE_Failure;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffRGBABand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;
    int			nBlockBufSize, nBlockId;
    CPLErr		eErr = CE_None;

    poGDS->SetDirectory();

    nBlockBufSize = 4 * nBlockXSize * nBlockYSize;
    nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

/* -------------------------------------------------------------------- */
/*      Allocate a temporary buffer for this strip.                     */
/* -------------------------------------------------------------------- */
    if( poGDS->pabyBlockBuf == NULL )
    {
        poGDS->pabyBlockBuf = (GByte *) VSICalloc( 1, nBlockBufSize );
        if( poGDS->pabyBlockBuf == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                   "Unable to allocate %d bytes for a temporary strip buffer\n"
                      "in GeoTIFF driver.",
                      nBlockBufSize );
            
            return( CE_Failure );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Read the strip                                                  */
/* -------------------------------------------------------------------- */
    if( poGDS->nLoadedBlock != nBlockId )
    {
        if( TIFFIsTiled( poGDS->hTIFF ) )
        {
            if( TIFFReadRGBATile(poGDS->hTIFF, 
                                 nBlockXOff * nBlockXSize, 
                                 nBlockYOff * nBlockYSize,
                                 (uint32 *) poGDS->pabyBlockBuf) == -1 )
            {
                /* Once TIFFError() is properly hooked, this can go away */
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadRGBATile() failed." );
                
                memset( poGDS->pabyBlockBuf, 0, nBlockBufSize );
                
                eErr = CE_Failure;
            }
        }
        else
        {
            if( TIFFReadRGBAStrip(poGDS->hTIFF, 
                                  nBlockId * nBlockYSize,
                                  (uint32 *) poGDS->pabyBlockBuf) == -1 )
            {
                /* Once TIFFError() is properly hooked, this can go away */
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadRGBAStrip() failed." );
                
                memset( poGDS->pabyBlockBuf, 0, nBlockBufSize );
                
                eErr = CE_Failure;
            }
        }
    }

    poGDS->nLoadedBlock = nBlockId;
                              
/* -------------------------------------------------------------------- */
/*      Handle simple case of eight bit data, and pixel interleaving.   */
/* -------------------------------------------------------------------- */
    int   iDestLine, nBO;
    int   nThisBlockYSize;

    if( (nBlockYOff+1) * nBlockYSize > GetYSize()
        && !TIFFIsTiled( poGDS->hTIFF ) )
        nThisBlockYSize = GetYSize() - nBlockYOff * nBlockYSize;
    else
        nThisBlockYSize = nBlockYSize;

#ifdef CPL_LSB
    nBO = nBand - 1;
#else
    nBO = 4 - nBand;
#endif

    for( iDestLine = 0; iDestLine < nThisBlockYSize; iDestLine++ )
    {
        int	nSrcOffset;

        nSrcOffset = (nThisBlockYSize - iDestLine - 1) * nBlockXSize * 4;

        GDALCopyWords( poGDS->pabyBlockBuf + nBO + nSrcOffset, GDT_Byte, 4,
                       ((GByte *) pImage)+iDestLine*nBlockXSize, GDT_Byte, 1, 
                       nBlockXSize );
    }

    return eErr;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GTiffRGBABand::GetColorInterpretation()

{
    if( nBand == 1 )
        return GCI_RedBand;
    else if( nBand == 2 )
        return GCI_GreenBand;
    else if( nBand == 3 )
        return GCI_BlueBand;
    else
        return GCI_AlphaBand;
}

/************************************************************************/
/* ==================================================================== */
/*                             GTiffBitmapBand                          */
/* ==================================================================== */
/************************************************************************/

class GTiffBitmapBand : public GTiffRasterBand
{
    friend class GTiffDataset;

    GDALColorTable *poColorTable;

  public:

                   GTiffBitmapBand( GTiffDataset *, int );
    virtual       ~GTiffBitmapBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
};


/************************************************************************/
/*                           GTiffBitmapBand()                          */
/************************************************************************/

GTiffBitmapBand::GTiffBitmapBand( GTiffDataset *poDS, int nBand )
        : GTiffRasterBand( poDS, nBand )

{

    if( nBand != 1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "One bit deep TIFF files only supported with one sample per pixel (band)." );
    }

    eDataType = GDT_Byte;

    if( poDS->poColorTable != NULL )
        poColorTable = poDS->poColorTable->Clone();
    else
    {
        GDALColorEntry	oWhite, oBlack;

        oWhite.c1 = 255;
        oWhite.c2 = 255;
        oWhite.c3 = 255;
        oWhite.c4 = 255;

        oBlack.c1 = 0;
        oBlack.c2 = 0;
        oBlack.c3 = 0;
        oBlack.c4 = 255;

        poColorTable = new GDALColorTable();
        
        if( poDS->nPhotometric == PHOTOMETRIC_MINISWHITE )
        {
            poColorTable->SetColorEntry( 0, &oWhite );
            poColorTable->SetColorEntry( 1, &oBlack );
        }
        else
        {
            poColorTable->SetColorEntry( 0, &oBlack );
            poColorTable->SetColorEntry( 1, &oWhite );
        }
    }
}

/************************************************************************/
/*                          ~GTiffBitmapBand()                          */
/************************************************************************/

GTiffBitmapBand::~GTiffBitmapBand()

{
    delete poColorTable;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffBitmapBand::IWriteBlock( int, int, void * )

{
    CPLError( CE_Failure, CPLE_AppDefined, 
              "One bit raster bands are read-only." );
    return CE_Failure;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffBitmapBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;
    int			nBlockBufSize, nBlockId;
    CPLErr		eErr = CE_None;

    poGDS->SetDirectory();

    if( TIFFIsTiled(poGDS->hTIFF) )
        nBlockBufSize = TIFFTileSize( poGDS->hTIFF );
    else
    {
        CPLAssert( nBlockXOff == 0 );
        nBlockBufSize = TIFFStripSize( poGDS->hTIFF );
    }

    nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

/* -------------------------------------------------------------------- */
/*      Load the block buffer.                                          */
/* -------------------------------------------------------------------- */
    eErr = poGDS->LoadBlockBuf( nBlockId );
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Translate 1bit data to eight bit.                               */
/* -------------------------------------------------------------------- */
    int	  iDstOffset=0, iLine;
    register GByte *pabyBlockBuf = poGDS->pabyBlockBuf;

    for( iLine = 0; iLine < nBlockYSize; iLine++ )
    {
        int iSrcOffset, iPixel;

        iSrcOffset = ((nBlockXSize+7) >> 3) * 8 * iLine;

        for( iPixel = 0; iPixel < nBlockXSize; iPixel++, iSrcOffset++ )
        {
            if( pabyBlockBuf[iSrcOffset >>3] & (0x80 >> (iSrcOffset & 0x7)) )
                ((GByte *) pImage)[iDstOffset++] = 1;
            else
                ((GByte *) pImage)[iDstOffset++] = 0;
        }
    }
    
    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GTiffBitmapBand::GetColorInterpretation()

{
    return GCI_PaletteIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GTiffBitmapBand::GetColorTable()

{
    return poColorTable;
}

/************************************************************************/
/* ==================================================================== */
/*                             GTiffOddBitsBand                         */
/* ==================================================================== */
/************************************************************************/

class GTiffOddBitsBand : public GTiffRasterBand
{
    friend class GTiffDataset;
  public:

                   GTiffOddBitsBand( GTiffDataset *, int );
    virtual       ~GTiffOddBitsBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );
};


/************************************************************************/
/*                           GTiffOddBitsBand()                          */
/************************************************************************/

GTiffOddBitsBand::GTiffOddBitsBand( GTiffDataset *poGDS, int nBand )
        : GTiffRasterBand( poGDS, nBand )

{

    if( nBand != 1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "One bit deep TIFF files only supported with one sample per pixel (band)." );
    }

    eDataType = GDT_Byte;
    if( poGDS->nBitsPerSample > 8 && poGDS->nBitsPerSample < 16 )
        eDataType = GDT_UInt16;
}

/************************************************************************/
/*                          ~GTiffOddBitsBand()                          */
/************************************************************************/

GTiffOddBitsBand::~GTiffOddBitsBand()

{
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffOddBitsBand::IWriteBlock( int, int, void * )

{
    CPLError( CE_Failure, CPLE_AppDefined, 
              "Odd bits raster bands are read-only." );
    return CE_Failure;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffOddBitsBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;
    int			nBlockBufSize, nBlockId;
    CPLErr		eErr = CE_None;

    poGDS->SetDirectory();

    if( TIFFIsTiled(poGDS->hTIFF) )
        nBlockBufSize = TIFFTileSize( poGDS->hTIFF );
    else
    {
        CPLAssert( nBlockXOff == 0 );
        nBlockBufSize = TIFFStripSize( poGDS->hTIFF );
    }

    nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlockId += (nBand-1) * poGDS->nBlocksPerBand;

/* -------------------------------------------------------------------- */
/*	Handle the case of a strip in a writable file that doesn't	*/
/*	exist yet, but that we want to read.  Just set to zeros and	*/
/*	return.								*/
/* -------------------------------------------------------------------- */
    if( poGDS->eAccess == GA_Update && !poGDS->IsBlockAvailable(nBlockId) )
    {
        memset( pImage, 0,
                nBlockXSize * nBlockYSize
                * GDALGetDataTypeSize(eDataType) / 8 );
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Load the block buffer.                                          */
/* -------------------------------------------------------------------- */
    eErr = poGDS->LoadBlockBuf( nBlockId );
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Handle 9-15 bits to 16 bits.                                    */
/* -------------------------------------------------------------------- */
    if( eDataType == GDT_UInt16 )
    {
        int	iBit, iPixel, iBitOffset = 0, nBlockPixels;
        int     iPixelBitSkip, iBandBitOffset;

        if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
        {
            iPixelBitSkip = poGDS->nBands * poGDS->nBitsPerSample;
            iBandBitOffset = (nBand-1) * poGDS->nBitsPerSample;
        }
        else
        {
            iPixelBitSkip = poGDS->nBitsPerSample;
            iBandBitOffset = 0;
        }

        nBlockPixels = nBlockXSize * nBlockYSize;
        for( iPixel = 0; iPixel < nBlockPixels; iPixel++ )
        {
            iBitOffset = iBandBitOffset + iPixel * iPixelBitSkip;
            GUInt16   nOutWord = 0;

            for( iBit = 0; iBit < poGDS->nBitsPerSample; iBit++ )
            {
                if( poGDS->pabyBlockBuf[iBitOffset>>3] 
                    & (0x80 >>(iBitOffset & 7)) )
                    nOutWord |= (1 << (poGDS->nBitsPerSample - 1 - iBit));
                iBitOffset++;
            } 
            
            ((GUInt16 *) pImage)[iPixel] = nOutWord;
        }
    }
    
    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                            GTiffDataset                              */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            GTiffDataset()                            */
/************************************************************************/

GTiffDataset::GTiffDataset()

{
    nLoadedBlock = -1;
    bLoadedBlockDirty = FALSE;
    pabyBlockBuf = NULL;
    hTIFF = NULL;
    bNewDataset = FALSE;
    bCrystalized = TRUE;
    poColorTable = NULL;
    bNoDataSet = FALSE;
    bNoDataChanged = FALSE;
    dfNoDataValue = -9999.0;
    pszProjection = CPLStrdup("");
    bBase = TRUE;
    bTreatAsRGBA = FALSE;
    nOverviewCount = 0;
    papoOverviewDS = NULL;
    nDirOffset = 0;

    pszTFWFilename = NULL;

    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    nGCPCount = 0;
    pasGCPList = NULL;
}

/************************************************************************/
/*                           ~GTiffDataset()                            */
/************************************************************************/

GTiffDataset::~GTiffDataset()

{
    Crystalize();

    FlushCache();

    if( bBase )
    {
        for( int i = 0; i < nOverviewCount; i++ )
        {
            delete papoOverviewDS[i];
        }
        CPLFree( papoOverviewDS );
    }

    SetDirectory();

    if( poColorTable != NULL )
        delete poColorTable;

    if( GetAccess() == GA_Update && bBase )
    {
        if( bNewDataset || bMetadataChanged )
            WriteMetadata( this, hTIFF );
        
        if( bNewDataset || bGeoTIFFInfoChanged )
            WriteGeoTIFFInfo();

	if( bNoDataChanged )
            WriteNoDataValue( hTIFF, dfNoDataValue );
        
        if( bNewDataset || bMetadataChanged || bGeoTIFFInfoChanged || bNoDataChanged )
        {
#if defined(TIFFLIB_VERSION)
#if  TIFFLIB_VERSION > 20010925 && TIFFLIB_VERSION != 20011807
            TIFFRewriteDirectory( hTIFF );
#endif
#endif
        }
    }

    if( bBase )
    {
        XTIFFClose( hTIFF );
    }

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    if( pszTFWFilename != NULL )
        CPLFree( pszTFWFilename );
    CPLFree( pszProjection );
}

/************************************************************************/
/*                           FlushBlockBuf()                            */
/************************************************************************/

CPLErr GTiffDataset::FlushBlockBuf()

{
    int		nBlockBufSize;
    CPLErr      eErr = CE_None;

    if( nLoadedBlock < 0 || !bLoadedBlockDirty )
        return CE_None;

    if( TIFFIsTiled(hTIFF) )
        nBlockBufSize = TIFFTileSize( hTIFF );
    else
        nBlockBufSize = TIFFStripSize( hTIFF );

    bLoadedBlockDirty = FALSE;

    if( TIFFIsTiled( hTIFF ) )
    {
        if( TIFFWriteEncodedTile(hTIFF, nLoadedBlock, pabyBlockBuf,
                                 nBlockBufSize) == -1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "TIFFWriteEncodedTile() failed." );
            eErr = CE_Failure;
        }
    }
    else
    {
        if( TIFFWriteEncodedStrip(hTIFF, nLoadedBlock, pabyBlockBuf,
                                 nBlockBufSize) == -1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "TIFFWriteEncodedStrip() failed." );
            eErr = CE_Failure;
        }
    }

    return eErr;;
}

/************************************************************************/
/*                            LoadBlockBuf()                            */
/*                                                                      */
/*      Load working block buffer with request block (tile/strip).      */
/************************************************************************/

CPLErr GTiffDataset::LoadBlockBuf( int nBlockId )

{
    int	nBlockBufSize;
    CPLErr	eErr = CE_None;

    if( nLoadedBlock == nBlockId )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      If we have a dirty loaded block, flush it out first.            */
/* -------------------------------------------------------------------- */
    if( nLoadedBlock != -1 && bLoadedBlockDirty )
    {
        eErr = FlushBlockBuf();
        if( eErr != CE_None )
            return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Get block size.                                                 */
/* -------------------------------------------------------------------- */
    if( TIFFIsTiled(hTIFF) )
        nBlockBufSize = TIFFTileSize( hTIFF );
    else
        nBlockBufSize = TIFFStripSize( hTIFF );

/* -------------------------------------------------------------------- */
/*      Allocate a temporary buffer for this strip.                     */
/* -------------------------------------------------------------------- */
    if( pabyBlockBuf == NULL )
    {
        pabyBlockBuf = (GByte *) VSICalloc( 1, nBlockBufSize );
        if( pabyBlockBuf == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "Unable to allocate %d bytes for a temporary strip buffer\n"
                      "in GeoTIFF driver.",
                      nBlockBufSize );
            
            return( CE_Failure );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      If we don't have this block already loaded, and we know it      */
/*      doesn't yet exist on disk, just zero the memory buffer and      */
/*      pretend we loaded it.                                           */
/* -------------------------------------------------------------------- */
    if( eAccess == GA_Update && !IsBlockAvailable( nBlockId ) )
    {
        memset( pabyBlockBuf, 0, nBlockBufSize );
        nLoadedBlock = nBlockId;
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Load the block, if it isn't our current block.                  */
/* -------------------------------------------------------------------- */
    if( TIFFIsTiled( hTIFF ) )
    {
        if( TIFFReadEncodedTile(hTIFF, nBlockId, pabyBlockBuf,
                                nBlockBufSize) == -1 )
        {
            /* Once TIFFError() is properly hooked, this can go away */
            CPLError( CE_Failure, CPLE_AppDefined,
                      "TIFFReadEncodedTile() failed." );
                
            memset( pabyBlockBuf, 0, nBlockBufSize );
                
            eErr = CE_Failure;
        }
    }
    else
    {
        if( TIFFReadEncodedStrip(hTIFF, nBlockId, pabyBlockBuf,
                                 nBlockBufSize) == -1 )
        {
            /* Once TIFFError() is properly hooked, this can go away */
            CPLError( CE_Failure, CPLE_AppDefined,
                      "TIFFReadEncodedStrip() failed." );
                
            memset( pabyBlockBuf, 0, nBlockBufSize );
                
            eErr = CE_Failure;
        }
    }

    nLoadedBlock = nBlockId;
    bLoadedBlockDirty = FALSE;

    return eErr;
}


/************************************************************************/
/*                             Crystalize()                             */
/*                                                                      */
/*      Make sure that the directory information is written out for     */
/*      a new file, require before writing any imagery data.            */
/************************************************************************/

void GTiffDataset::Crystalize()

{
    if( !bCrystalized )
    {
        bCrystalized = TRUE;

        TIFFWriteCheck( hTIFF, TIFFIsTiled(hTIFF), "GTiffDataset::Crystalize");
        TIFFWriteDirectory( hTIFF );

        TIFFSetDirectory( hTIFF, 0 );
        nDirOffset = TIFFCurrentDirOffset( hTIFF );
    }
}


/************************************************************************/
/*                          IsBlockAvailable()                          */
/*                                                                      */
/*      Return TRUE if the indicated strip/tile is available.  We       */
/*      establish this by testing if the stripbytecount is zero.  If    */
/*      zero then the block has never been committed to disk.           */
/************************************************************************/

int GTiffDataset::IsBlockAvailable( int nBlockId )

{
    uint32 *panByteCounts = NULL;

    if( ( TIFFIsTiled( hTIFF ) 
          && TIFFGetField( hTIFF, TIFFTAG_TILEBYTECOUNTS, &panByteCounts ) )
        || ( !TIFFIsTiled( hTIFF ) 
          && TIFFGetField( hTIFF, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts ) ) )
    {
        if( panByteCounts == NULL )
            return FALSE;
        else
            return panByteCounts[nBlockId] != 0;
    }
    else
        return FALSE;
}

/************************************************************************/
/*                             FlushCache()                             */
/*                                                                      */
/*      We override this so we can also flush out local tiff strip      */
/*      cache if need be.                                               */
/************************************************************************/

void GTiffDataset::FlushCache()

{
    GDALDataset::FlushCache();

    if( bLoadedBlockDirty && nLoadedBlock != -1 )
        FlushBlockBuf();

    CPLFree( pabyBlockBuf );
    pabyBlockBuf = NULL;
    nLoadedBlock = -1;
    bLoadedBlockDirty = FALSE;
}

/************************************************************************/
/*                         TIFF_OvLevelAdjust()                         */
/*                                                                      */
/*      Some overview levels cannot be achieved closely enough to be    */
/*      recognised as the desired overview level.  This function        */
/*      will adjust an overview level to one that is achievable on      */
/*      the given raster size.                                          */
/*                                                                      */
/*      For instance a 1200x1200 image on which a 256 level overview    */
/*      is request will end up generating a 5x5 overview.  However,     */
/*      this will appear to the system be a level 240 overview.         */
/*      This function will adjust 256 to 240 based on knowledge of      */
/*      the image size.                                                 */
/*                                                                      */
/*      This is a copy of the GDALOvLevelAdjust() function in           */
/*      gdaldefaultoverviews.cpp.                                       */
/************************************************************************/

static int TIFF_OvLevelAdjust( int nOvLevel, int nXSize )

{
    int	nOXSize = (nXSize + nOvLevel - 1) / nOvLevel;
    
    return (int) (0.5 + nXSize / (double) nOXSize);
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr GTiffDataset::IBuildOverviews( 
    const char * pszResampling, 
    int nOverviews, int * panOverviewList,
    int nBands, int * panBandList,
    GDALProgressFunc pfnProgress, void * pProgressData )

{
    CPLErr       eErr = CE_None;
    int          i;
    GTiffDataset *poODS;

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

    Crystalize();

    TIFFFlush( hTIFF );

/* -------------------------------------------------------------------- */
/*      If we don't have read access, then create the overviews externally.*/
/* -------------------------------------------------------------------- */
    if( GetAccess() != GA_Update )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "File open for read-only accessing, "
                  "creating overviews externally." );

        return GDALDataset::IBuildOverviews( 
            pszResampling, nOverviews, panOverviewList, 
            nBands, panBandList, pfnProgress, pProgressData );
    }

/* -------------------------------------------------------------------- */
/*      Our TIFF overview support currently only works safely if all    */
/*      bands are handled at the same time.                             */
/* -------------------------------------------------------------------- */
    if( nBands != GetRasterCount() )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Generation of overviews in TIFF currently only"
                  " supported when operating on all bands.\n" 
                  "Operation failed.\n" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a palette?  If so, create a TIFF compatible version. */
/* -------------------------------------------------------------------- */
    unsigned short	anTRed[65536], anTGreen[65536], anTBlue[65536];
    unsigned short      *panRed=NULL, *panGreen=NULL, *panBlue=NULL;

    if( nPhotometric == PHOTOMETRIC_PALETTE && poColorTable != NULL )
    {
        int nColors;

        if( nBitsPerSample == 8 )
            nColors = 256;
        else
            nColors = 65536;
        
        for( int iColor = 0; iColor < nColors; iColor++ )
        {
            if( iColor < poColorTable->GetColorEntryCount() )
            {
                GDALColorEntry  sRGB;

                poColorTable->GetColorEntryAsRGB( iColor, &sRGB );

                anTRed[iColor] = (unsigned short) (256 * sRGB.c1);
                anTGreen[iColor] = (unsigned short) (256 * sRGB.c2);
                anTBlue[iColor] = (unsigned short) (256 * sRGB.c3);
            }
            else
            {
                anTRed[iColor] = anTGreen[iColor] = anTBlue[iColor] = 0;
            }
        }

        panRed = anTRed;
        panGreen = anTGreen;
        panBlue = anTBlue;
    }
        
/* -------------------------------------------------------------------- */
/*      Establish which of the overview levels we already have, and     */
/*      which are new.  We assume that band 1 of the file is            */
/*      representative.                                                 */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nOverviews && eErr == CE_None; i++ )
    {
        int   j;

        for( j = 0; j < nOverviewCount; j++ )
        {
            int    nOvFactor;

            poODS = papoOverviewDS[j];

            nOvFactor = (int) 
                (0.5 + GetRasterXSize() / (double) poODS->GetRasterXSize());

            if( nOvFactor == panOverviewList[i] )
                panOverviewList[i] *= -1;
        }

        if( panOverviewList[i] > 0 )
        {
            uint32	nOverviewOffset;
            int         nOXSize, nOYSize;

            nOXSize = (GetRasterXSize() + panOverviewList[i] - 1) 
                / panOverviewList[i];
            nOYSize = (GetRasterYSize() + panOverviewList[i] - 1)
                / panOverviewList[i];

            nOverviewOffset = 
                TIFF_WriteOverview( hTIFF, nOXSize, nOYSize, 
                                    nBitsPerSample, nSamplesPerPixel, 
                                    128, 128, TRUE, nCompression, 
                                    nPhotometric, nSampleFormat, 
                                    panRed, panGreen, panBlue, FALSE );

            if( nOverviewOffset == 0 )
            {
                eErr = CE_Failure;
                continue;
            }

            poODS = new GTiffDataset();
            if( poODS->OpenOffset( hTIFF, nOverviewOffset, FALSE, 
                                   GA_Update ) != CE_None )
            {
                delete poODS;
                eErr = CE_Failure;
            }
            else
            {
                nOverviewCount++;
                papoOverviewDS = (GTiffDataset **)
                    CPLRealloc(papoOverviewDS, 
                               nOverviewCount * (sizeof(void*)));
                papoOverviewDS[nOverviewCount-1] = poODS;
            }
        }
        else
            panOverviewList[i] *= -1;
    }

/* -------------------------------------------------------------------- */
/*      Refresh old overviews that were listed.                         */
/* -------------------------------------------------------------------- */
    GDALRasterBand **papoOverviewBands;

    papoOverviewBands = (GDALRasterBand **) 
        CPLCalloc(sizeof(void*),nOverviews);

    for( int iBand = 0; iBand < nBands && eErr == CE_None; iBand++ )
    {
        GDALRasterBand *poBand;
        int            nNewOverviews;

        poBand = GetRasterBand( panBandList[iBand] );

        nNewOverviews = 0;
        for( i = 0; i < nOverviews && poBand != NULL; i++ )
        {
            int   j;
            
            for( j = 0; j < poBand->GetOverviewCount(); j++ )
            {
                int    nOvFactor;
                GDALRasterBand * poOverview = poBand->GetOverview( j );

                nOvFactor = (int) 
                  (0.5 + poBand->GetXSize() / (double) poOverview->GetXSize());

                if( nOvFactor == panOverviewList[i] 
                    || nOvFactor == TIFF_OvLevelAdjust( panOverviewList[i],
                                                        poBand->GetXSize() ) )
                {
                    papoOverviewBands[nNewOverviews++] = poOverview;
                }
            }
        }

        void         *pScaledProgressData;

        pScaledProgressData = 
            GDALCreateScaledProgress( iBand / (double) nBands, 
                                      (iBand+1) / (double) nBands,
                                      pfnProgress, pProgressData );

        eErr = GDALRegenerateOverviews( poBand,
                                        nNewOverviews, papoOverviewBands,
                                        pszResampling, 
                                        GDALScaledProgress, 
                                        pScaledProgressData);

        GDALDestroyScaledProgress( pScaledProgressData );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( papoOverviewBands );

    pfnProgress( 1.0, NULL, pProgressData );

    return eErr;
}


/************************************************************************/
/*                          WriteGeoTIFFInfo()                          */
/************************************************************************/

void GTiffDataset::WriteGeoTIFFInfo()

{
/* -------------------------------------------------------------------- */
/*      If the geotransform is the default, don't bother writing it.    */
/* -------------------------------------------------------------------- */
    if( adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0
        || adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0
        || adfGeoTransform[4] != 0.0 || ABS(adfGeoTransform[5]) != 1.0 )
    {

/* -------------------------------------------------------------------- */
/*      Write the transform.  If we have a normal north-up image we     */
/*      use the tiepoint plus pixelscale otherwise we use a matrix.     */
/* -------------------------------------------------------------------- */
	if( adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0 
            && adfGeoTransform[5] < 0.0 )
	{
	    double	adfPixelScale[3], adfTiePoints[6];

	    adfPixelScale[0] = adfGeoTransform[1];
	    adfPixelScale[1] = fabs(adfGeoTransform[5]);
	    adfPixelScale[2] = 0.0;
	    
	    TIFFSetField( hTIFF, TIFFTAG_GEOPIXELSCALE, 3, adfPixelScale );
	    
	    adfTiePoints[0] = 0.0;
	    adfTiePoints[1] = 0.0;
	    adfTiePoints[2] = 0.0;
	    adfTiePoints[3] = adfGeoTransform[0];
	    adfTiePoints[4] = adfGeoTransform[3];
	    adfTiePoints[5] = 0.0;
	    
	    TIFFSetField( hTIFF, TIFFTAG_GEOTIEPOINTS, 6, adfTiePoints );
	}
	else
	{
	    double	adfMatrix[16];
	    
	    memset(adfMatrix,0,sizeof(double) * 16);
	    
	    adfMatrix[0] = adfGeoTransform[1];
	    adfMatrix[1] = adfGeoTransform[2];
	    adfMatrix[3] = adfGeoTransform[0];
	    adfMatrix[4] = adfGeoTransform[4];
	    adfMatrix[5] = adfGeoTransform[5];
	    adfMatrix[7] = adfGeoTransform[3];
	    adfMatrix[15] = 1.0;
	    
	    TIFFSetField( hTIFF, TIFFTAG_GEOTRANSMATRIX, 16, adfMatrix );
	}
/* -------------------------------------------------------------------- */
/*      Are we maintaining a .tfw file?                                 */
/* -------------------------------------------------------------------- */
	if( pszTFWFilename != NULL )
	{
	    FILE	*fp;

	    fp = VSIFOpen( pszTFWFilename, "wt" );
	    
	    fprintf( fp, "%.10f\n", adfGeoTransform[1] );
	    fprintf( fp, "%.10f\n", adfGeoTransform[4] );
	    fprintf( fp, "%.10f\n", adfGeoTransform[2] );
	    fprintf( fp, "%.10f\n", adfGeoTransform[5] );
	    fprintf( fp, "%.10f\n", adfGeoTransform[0] 
		     + 0.5 * adfGeoTransform[1]
		     + 0.5 * adfGeoTransform[2] );
	    fprintf( fp, "%.10f\n", adfGeoTransform[3]
		     + 0.5 * adfGeoTransform[4]
		     + 0.5 * adfGeoTransform[5] );
	    VSIFClose( fp );
	}
    }
    else if( GetGCPCount() > 0 )
    {
	double	*padfTiePoints;
	int		iGCP;
	
	padfTiePoints = (double *) 
	    CPLMalloc( 6 * sizeof(double) * GetGCPCount() );

	for( iGCP = 0; iGCP < GetGCPCount(); iGCP++ )
	{

	    padfTiePoints[iGCP*6+0] = pasGCPList[iGCP].dfGCPPixel;
	    padfTiePoints[iGCP*6+1] = pasGCPList[iGCP].dfGCPLine;
	    padfTiePoints[iGCP*6+2] = 0;
	    padfTiePoints[iGCP*6+3] = pasGCPList[iGCP].dfGCPX;
	    padfTiePoints[iGCP*6+4] = pasGCPList[iGCP].dfGCPY;
	    padfTiePoints[iGCP*6+5] = pasGCPList[iGCP].dfGCPZ;
	}

	TIFFSetField( hTIFF, TIFFTAG_GEOTIEPOINTS, 
		      6 * GetGCPCount(), padfTiePoints );
	CPLFree( padfTiePoints );
	
	pszProjection = CPLStrdup( GetGCPProjection() );
    }

/* -------------------------------------------------------------------- */
/*	Write out projection definition.				*/
/* -------------------------------------------------------------------- */
    if( pszProjection != NULL && !EQUAL( pszProjection, "" ) )
    {
        GTIF	*psGTIF;

        psGTIF = GTIFNew( hTIFF );
        GTIFSetFromOGISDefn( psGTIF, pszProjection );
        GTIFWriteKeys( psGTIF );
        GTIFFree( psGTIF );
    }
}

/************************************************************************/
/*                         AppendMetadataItem()                         */
/************************************************************************/

static CPLXMLNode *AppendMetadataItem( CPLXMLNode *psRoot, 
                                       const char *pszKey, 
                                       const char *pszValue,
                                       int nBand, const char *pszRole )

{
    char szBandId[32];
    CPLXMLNode *psItem;

    if( psRoot == NULL )
        psRoot = CPLCreateXMLNode( NULL, CXT_Element, "GDALMetadata" );
        
    psItem = CPLCreateXMLNode( psRoot, CXT_Element, "Item" );
    CPLCreateXMLNode( CPLCreateXMLNode( psItem, CXT_Attribute, "name"),
                      CXT_Text, pszKey );

    if( nBand > 0 )
    {
        sprintf( szBandId, "%d", nBand - 1 );
        CPLCreateXMLNode( CPLCreateXMLNode( psItem,CXT_Attribute,"sample"),
                          CXT_Text, szBandId );
    }

    if( pszRole != NULL )
        CPLCreateXMLNode( CPLCreateXMLNode( psItem,CXT_Attribute,"role"),
                          CXT_Text, pszRole );

    char *pszEscapedItemValue = CPLEscapeString(pszValue,-1,CPLES_XML);
    CPLCreateXMLNode( psItem, CXT_Text, pszEscapedItemValue );
    CPLFree( pszEscapedItemValue );

    return psRoot;
}

/************************************************************************/
/*                           WriteMetadata()                            */
/************************************************************************/

void GTiffDataset::WriteMetadata( GDALDataset *poSrcDS, TIFF *hTIFF )

{
/* -------------------------------------------------------------------- */
/*      Convert all the remaining metadata into a simple XML            */
/*      format.                                                         */
/* -------------------------------------------------------------------- */
    char **papszMD = poSrcDS->GetMetadata();
    int  iItem, nItemCount = CSLCount(papszMD);
    CPLXMLNode *psRoot = NULL;
    
    for( iItem = 0; iItem < nItemCount; iItem++ )
    {
        const char *pszItemValue;
        char *pszItemName = NULL;

        pszItemValue = CPLParseNameValue( papszMD[iItem], &pszItemName );

        if( EQUAL(pszItemName,"TIFFTAG_DOCUMENTNAME") )
            TIFFSetField( hTIFF, TIFFTAG_DOCUMENTNAME, pszItemValue );
        else if( EQUAL(pszItemName,"TIFFTAG_IMAGEDESCRIPTION") )
            TIFFSetField( hTIFF, TIFFTAG_IMAGEDESCRIPTION, pszItemValue );
        else if( EQUAL(pszItemName,"TIFFTAG_SOFTWARE") )
            TIFFSetField( hTIFF, TIFFTAG_SOFTWARE, pszItemValue );
        else if( EQUAL(pszItemName,"TIFFTAG_DATETIME") )
            TIFFSetField( hTIFF, TIFFTAG_DATETIME, pszItemValue );
        else if( EQUAL(pszItemName,"TIFFTAG_XRESOLUTION") )
            TIFFSetField( hTIFF, TIFFTAG_XRESOLUTION, atof(pszItemValue) );
        else if( EQUAL(pszItemName,"TIFFTAG_YRESOLUTION") )
            TIFFSetField( hTIFF, TIFFTAG_YRESOLUTION, atof(pszItemValue) );
        else if( EQUAL(pszItemName,"TIFFTAG_RESOLUTIONUNIT") )
            TIFFSetField( hTIFF, TIFFTAG_RESOLUTIONUNIT, atoi(pszItemValue) );
        else
        {
            psRoot = AppendMetadataItem( psRoot, pszItemName, pszItemValue, 
                                         0, NULL );
        }

        CPLFree( pszItemName );
    }

/* -------------------------------------------------------------------- */
/*      We also need to address band specific metadata.                 */
/* -------------------------------------------------------------------- */
    int nBand;
    for( nBand = 1; nBand <= poSrcDS->GetRasterCount(); nBand++ )
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand( nBand );

        
        papszMD = poBand->GetMetadata();
        nItemCount = CSLCount(papszMD);

        for( iItem = 0; iItem < nItemCount; iItem++ )
        {
            const char *pszItemValue;
            char *pszItemName = NULL;

            pszItemValue = CPLParseNameValue( papszMD[iItem], &pszItemName );

            psRoot = AppendMetadataItem( psRoot, pszItemName, pszItemValue, 
                                         nBand, NULL );
            CPLFree( pszItemName );
        }

        int bSuccess;
        double dfOffset = poBand->GetOffset( &bSuccess );
        double dfScale = poBand->GetScale();

        if( bSuccess && (dfOffset != 0.0 || dfScale != 1.0) )
        {
            char szValue[128];

            sprintf( szValue, "%.16g", dfOffset );
            psRoot = AppendMetadataItem( psRoot, "OFFSET", szValue, nBand, 
                                         "offset" );
            sprintf( szValue, "%.16g", dfScale );
            psRoot = AppendMetadataItem( psRoot, "SCALE", szValue, nBand, 
                                         "scale" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write out the generic XML metadata if there is any.             */
/* -------------------------------------------------------------------- */
    if( psRoot != NULL )
    {
        char *pszXML_MD = CPLSerializeXMLTree( psRoot );
        if( strlen(pszXML_MD) > 32000 )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Lost metadata writing to GeoTIFF ... too large to fit in tag." );
        }
        else
        {
            TIFFSetField( hTIFF, TIFFTAG_GDAL_METADATA, pszXML_MD );
        }
        CPLFree( pszXML_MD );
        CPLDestroyXMLNode( psRoot );
    }
}

/************************************************************************/
/*                         WriteNoDataValue()                           */
/************************************************************************/

void GTiffDataset::WriteNoDataValue( TIFF *hTIFF, double dfNoData )

{
    const char *pszText;
    
    pszText = CPLSPrintf( "%.16g", dfNoData );
    TIFFSetField( hTIFF, TIFFTAG_GDAL_NODATA, pszText );
}

/************************************************************************/
/*                            SetDirectory()                            */
/************************************************************************/

int GTiffDataset::SetDirectory( uint32 nNewOffset )

{
    Crystalize();

    if( nNewOffset == 0 )
        nNewOffset = nDirOffset;

    if( nNewOffset == 0)
        return TRUE;

    if( TIFFCurrentDirOffset(hTIFF) == nNewOffset )
        return TRUE;

    if( GetAccess() == GA_Update )
        TIFFFlush( hTIFF );
    
    return TIFFSetSubDirectory( hTIFF, nNewOffset );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GTiffDataset::Open( GDALOpenInfo * poOpenInfo )

{
    TIFF	*hTIFF;

/* -------------------------------------------------------------------- */
/*      We have a special hook for handling opening a specific          */
/*      directory of a TIFF file.                                       */
/* -------------------------------------------------------------------- */
    if( EQUALN(poOpenInfo->pszFilename,"GTIFF_DIR:",10) )
        return OpenDir( poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 2 )
        return NULL;

    if( (poOpenInfo->pabyHeader[0] != 'I' || poOpenInfo->pabyHeader[1] != 'I')
     && (poOpenInfo->pabyHeader[0] != 'M' || poOpenInfo->pabyHeader[1] != 'M'))
        return NULL;

    if( poOpenInfo->pabyHeader[2] == 43 && poOpenInfo->pabyHeader[3] == 0 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "This is a BigTIFF file.  BigTIFF is not supported by this\n"
                  "version of GDAL and libtiff." );
        return NULL;
    }

    if( (poOpenInfo->pabyHeader[2] != 0x2A || poOpenInfo->pabyHeader[3] != 0)
        && (poOpenInfo->pabyHeader[3] != 0x2A || poOpenInfo->pabyHeader[2] != 0) )
        return NULL;

    GTiffOneTimeInit();

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_ReadOnly )
	hTIFF = XTIFFOpen( poOpenInfo->pszFilename, "r" );
    else
        hTIFF = XTIFFOpen( poOpenInfo->pszFilename, "r+" );
    
    if( hTIFF == NULL )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GTiffDataset 	*poDS;

    poDS = new GTiffDataset();
    poDS->SetDescription( poOpenInfo->pszFilename );

    if( poDS->OpenOffset( hTIFF,TIFFCurrentDirOffset(hTIFF), TRUE,
                          poOpenInfo->eAccess ) != CE_None )
    {
        delete poDS;
        return NULL;
    }

    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );
    
    return poDS;
}

/************************************************************************/
/*                              OpenDir()                               */
/*                                                                      */
/*      Open a specific directory as encoded into a filename.           */
/************************************************************************/

GDALDataset *GTiffDataset::OpenDir( const char *pszCompositeName )

{
    if( !EQUALN(pszCompositeName,"GTIFF_DIR:",10) )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Split out filename, and dir#/offset.                            */
/* -------------------------------------------------------------------- */
    const char *pszFilename = pszCompositeName + 10;
    int        bAbsolute = FALSE;
    uint32     nOffset;
    
    if( EQUALN(pszFilename,"off:",4) )
    {
        bAbsolute = TRUE;
        pszFilename += 4;
    }

    nOffset = atol(pszFilename);
    pszFilename += 1;

    while( *pszFilename != '\0' && pszFilename[-1] != ':' )
        pszFilename++;

    if( *pszFilename == '\0' || nOffset == 0 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to extract offset or filename, should take the form\n"
                  "GTIFF_DIR:<dir>:filename or GTIFF_DIR:off:<dir_offset>:filename" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    TIFF	*hTIFF;

    GTiffOneTimeInit();

    hTIFF = XTIFFOpen( pszFilename, "r" );
    if( hTIFF == NULL )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      If a directory was requested by index, advance to it now.       */
/* -------------------------------------------------------------------- */
    if( !bAbsolute )
    {
        while( nOffset > 1 )
        {
            if( TIFFReadDirectory( hTIFF ) == 0 )
            {
                XTIFFClose( hTIFF );
                CPLError( CE_Failure, CPLE_OpenFailed, 
                          "Requested directory %d not found." );
                return NULL;
            }
            nOffset--;
        }

        nOffset = TIFFCurrentDirOffset( hTIFF );
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GTiffDataset 	*poDS;

    poDS = new GTiffDataset();
    poDS->SetDescription( pszFilename );

    if( poDS->OpenOffset( hTIFF, nOffset, FALSE, GA_ReadOnly ) != CE_None )
    {
        delete poDS;
        return NULL;
    }
    else
    {
        return poDS;
    }
}

/************************************************************************/
/*                             OpenOffset()                             */
/*                                                                      */
/*      Initialize the GTiffDataset based on a passed in file           */
/*      handle, and directory offset to utilize.  This is called for    */
/*      full res, and overview pages.                                   */
/************************************************************************/

CPLErr GTiffDataset::OpenOffset( TIFF *hTIFFIn, uint32 nDirOffsetIn, 
				 int bBaseIn, GDALAccess eAccess )

{
    uint32	nXSize, nYSize;
    int		bTreatAsBitmap = FALSE;

    hTIFF = hTIFFIn;

    nDirOffset = nDirOffsetIn;

    SetDirectory( nDirOffsetIn );

    bBase = bBaseIn;

    this->eAccess = eAccess;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    TIFFGetField( hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );
    TIFFGetField( hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;

    if( !TIFFGetField(hTIFF, TIFFTAG_SAMPLESPERPIXEL, &nSamplesPerPixel ) )
        nBands = 1;
    else
        nBands = nSamplesPerPixel;
    
    if( !TIFFGetField(hTIFF, TIFFTAG_BITSPERSAMPLE, &(nBitsPerSample)) )
        nBitsPerSample = 1;
    
    if( !TIFFGetField( hTIFF, TIFFTAG_PLANARCONFIG, &(nPlanarConfig) ) )
        nPlanarConfig = PLANARCONFIG_CONTIG;
    
    if( !TIFFGetField( hTIFF, TIFFTAG_PHOTOMETRIC, &(nPhotometric) ) )
        nPhotometric = PHOTOMETRIC_MINISBLACK;
    
    if( !TIFFGetField( hTIFF, TIFFTAG_SAMPLEFORMAT, &(nSampleFormat) ) )
        nSampleFormat = SAMPLEFORMAT_UINT;
    
    if( !TIFFGetField( hTIFF, TIFFTAG_COMPRESSION, &(nCompression) ) )
        nCompression = COMPRESSION_NONE;

/* -------------------------------------------------------------------- */
/*      Get strip/tile layout.                                          */
/* -------------------------------------------------------------------- */
    if( TIFFIsTiled(hTIFF) )
    {
        TIFFGetField( hTIFF, TIFFTAG_TILEWIDTH, &(nBlockXSize) );
        TIFFGetField( hTIFF, TIFFTAG_TILELENGTH, &(nBlockYSize) );
    }
    else
    {
        if( !TIFFGetField( hTIFF, TIFFTAG_ROWSPERSTRIP,
                           &(nRowsPerStrip) ) )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "RowsPerStrip not defined ... assuming all one strip." );
            nRowsPerStrip = nYSize; /* dummy value */
        }

        nBlockXSize = nRasterXSize;
        nBlockYSize = MIN(nRowsPerStrip,nYSize);
    }
        
    nBlocksPerBand =
        ((nYSize + nBlockYSize - 1) / nBlockYSize)
        * ((nXSize + nBlockXSize  - 1) / nBlockXSize);

/* -------------------------------------------------------------------- */
/*      Should we handle this using the GTiffBitmapBand?                */
/* -------------------------------------------------------------------- */
    if( nBitsPerSample == 1 && nBands == 1 )
        bTreatAsBitmap = TRUE;

/* -------------------------------------------------------------------- */
/*      Should we treat this via the RGBA interface?                    */
/* -------------------------------------------------------------------- */
    if( !bTreatAsBitmap
        && (nPhotometric == PHOTOMETRIC_CIELAB ||
            nPhotometric == PHOTOMETRIC_LOGL ||
            nPhotometric == PHOTOMETRIC_LOGLUV ||
            ( nPhotometric == PHOTOMETRIC_YCBCR 
            && CSLTestBoolean( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB",
                                                  "YES") )) ||
            nBitsPerSample < 8 ) )
    {
        char	szMessage[1024];

        if( TIFFRGBAImageOK( hTIFF, szMessage ) == 1 )
        {
            bTreatAsRGBA = TRUE;
            nBands = 4;
        }
        else
        {
            CPLDebug( "GTiff", "TIFFRGBAImageOK says:\n%s", szMessage );
        }
    }
        
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        if( bTreatAsRGBA )
            SetBand( iBand+1, new GTiffRGBABand( this, iBand+1 ) );
        else if( bTreatAsBitmap )
            SetBand( iBand+1, new GTiffBitmapBand( this, iBand+1 ) );
        else if( nBitsPerSample > 8 && nBitsPerSample < 16 )
            SetBand( iBand+1, new GTiffOddBitsBand( this, iBand+1 ) );
        else
            SetBand( iBand+1, new GTiffRasterBand( this, iBand+1 ) );
    }

/* -------------------------------------------------------------------- */
/*      Capture the color table if there is one.                        */
/* -------------------------------------------------------------------- */
    unsigned short	*panRed, *panGreen, *panBlue;

    if( nPhotometric != PHOTOMETRIC_PALETTE 
        || bTreatAsRGBA 
        || TIFFGetField( hTIFF, TIFFTAG_COLORMAP, 
                         &panRed, &panGreen, &panBlue) == 0 )
    {
	// Build inverted palette if we have inverted photometric.
	// Pixel values remains unchanged.
	if( nPhotometric == PHOTOMETRIC_MINISWHITE )
	{
	    GDALColorEntry  oEntry;
	    int		    iColor, nColorCount;
	    
	    poColorTable = new GDALColorTable();
	    nColorCount = 1 << nBitsPerSample;

	    for ( iColor = 0; iColor < nColorCount; iColor++ )
	    {
		oEntry.c1 = oEntry.c2 = oEntry.c3 = 
                    (short) (nColorCount - 1 - iColor);
		oEntry.c4 = 255;
		poColorTable->SetColorEntry( iColor, &oEntry );
	    }

	    nPhotometric = PHOTOMETRIC_PALETTE;
	}
	else
	    poColorTable = NULL;
    }
    else
    {
        int	nColorCount;
        GDALColorEntry oEntry;

        poColorTable = new GDALColorTable();

        nColorCount = 1 << nBitsPerSample;

        for( int iColor = nColorCount - 1; iColor >= 0; iColor-- )
        {
            oEntry.c1 = panRed[iColor] / 256;
            oEntry.c2 = panGreen[iColor] / 256;
            oEntry.c3 = panBlue[iColor] / 256;
            oEntry.c4 = 255;

            poColorTable->SetColorEntry( iColor, &oEntry );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Get the transform or gcps from the GeoTIFF file.                */
/* -------------------------------------------------------------------- */
    if( bBaseIn )
    {
        char    *pszTabWKT = NULL;
        double	*padfTiePoints, *padfScale, *padfMatrix;
        uint16	nCount;

        adfGeoTransform[0] = 0.0;
        adfGeoTransform[1] = 1.0;
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = 0.0;
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = 1.0;
    
        if( TIFFGetField(hTIFF,TIFFTAG_GEOPIXELSCALE,&nCount,&padfScale )
            && nCount >= 2 
            && padfScale[0] != 0.0 && padfScale[1] != 0.0 )
        {
            adfGeoTransform[1] = padfScale[0];
            adfGeoTransform[5] = - ABS(padfScale[1]);

            if( TIFFGetField(hTIFF,TIFFTAG_GEOTIEPOINTS,&nCount,&padfTiePoints )
                && nCount >= 6 )
            {
                adfGeoTransform[0] =
                    padfTiePoints[3] - padfTiePoints[0] * adfGeoTransform[1];
                adfGeoTransform[3] =
                    padfTiePoints[4] - padfTiePoints[1] * adfGeoTransform[5];

                bGeoTransformValid = TRUE;
            }
        }

        else if( TIFFGetField(hTIFF,TIFFTAG_GEOTRANSMATRIX,&nCount,&padfMatrix ) 
                 && nCount == 16 )
        {
            adfGeoTransform[0] = padfMatrix[3];
            adfGeoTransform[1] = padfMatrix[0];
            adfGeoTransform[2] = padfMatrix[1];
            adfGeoTransform[3] = padfMatrix[7];
            adfGeoTransform[4] = padfMatrix[4];
            adfGeoTransform[5] = padfMatrix[5];
            bGeoTransformValid = TRUE;
        }

/* -------------------------------------------------------------------- */
/*      Otherwise try looking for a .tfw, .tifw or .wld file.           */
/* -------------------------------------------------------------------- */
        else
        {
            bGeoTransformValid = 
                GDALReadWorldFile( GetDescription(), "tfw", adfGeoTransform );

            if( !bGeoTransformValid )
            {
                bGeoTransformValid = 
                    GDALReadWorldFile( GetDescription(), "tifw", adfGeoTransform );
            }
            if( !bGeoTransformValid )
            {
                bGeoTransformValid = 
                    GDALReadWorldFile( GetDescription(), "wld", adfGeoTransform );
            }
            if( !bGeoTransformValid )
            {
                int bTabFileOK = 
                    GDALReadTabFile( GetDescription(), adfGeoTransform, 
                                     &pszTabWKT, &nGCPCount, &pasGCPList );

                if( bTabFileOK && nGCPCount == 0 )
                    bGeoTransformValid = TRUE;
            }
        }

/* -------------------------------------------------------------------- */
/*      Check for GCPs.  Note, we will allow there to be GCPs and a     */
/*      transform in some circumstances.                                */
/* -------------------------------------------------------------------- */
        if( TIFFGetField(hTIFF,TIFFTAG_GEOTIEPOINTS,&nCount,&padfTiePoints )
                 && nCount > 6 )
        {
            nGCPCount = nCount / 6;
            pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),nGCPCount);
        
            for( int iGCP = 0; iGCP < nGCPCount; iGCP++ )
            {
                char	szID[32];

                sprintf( szID, "%d", iGCP+1 );
                pasGCPList[iGCP].pszId = CPLStrdup( szID );
                pasGCPList[iGCP].pszInfo = CPLStrdup("");
                pasGCPList[iGCP].dfGCPPixel = padfTiePoints[iGCP*6+0];
                pasGCPList[iGCP].dfGCPLine = padfTiePoints[iGCP*6+1];
                pasGCPList[iGCP].dfGCPX = padfTiePoints[iGCP*6+3];
                pasGCPList[iGCP].dfGCPY = padfTiePoints[iGCP*6+4];
                pasGCPList[iGCP].dfGCPZ = padfTiePoints[iGCP*6+5];
            }
        }

/* -------------------------------------------------------------------- */
/*      Capture the GeoTIFF projection, if available.                   */
/* -------------------------------------------------------------------- */
        GTIF 	*hGTIF;
        GTIFDefn	sGTIFDefn;

        CPLFree( pszProjection );
        pszProjection = NULL;
    
        hGTIF = GTIFNew(hTIFF);

        if ( !hGTIF )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "GeoTIFF tags apparently corrupt, they are being ignored." );
        }
        else
        {
            if( GTIFGetDefn( hGTIF, &sGTIFDefn ) )
            {
                pszProjection = GTIFGetOGISDefn( hGTIF, &sGTIFDefn );
            }

            GTIFFree( hGTIF );
        }
        
/* -------------------------------------------------------------------- */
/*      If we didn't get a geotiff projection definition, but we did    */
/*      get one from the .tag file, use that instead.                   */
/* -------------------------------------------------------------------- */
        if( pszTabWKT != NULL 
            && (pszProjection == NULL || pszProjection[0] == '\0') )
        {
            CPLFree( pszProjection );
            pszProjection = pszTabWKT;
            pszTabWKT = NULL;
        }
        
        if( pszProjection == NULL )
        {
            pszProjection = CPLStrdup( "" );
        }
        
        if( pszTabWKT != NULL )
            CPLFree( pszTabWKT );

        bGeoTIFFInfoChanged = FALSE;

/* -------------------------------------------------------------------- */
/*      Capture some other potentially interesting information.         */
/* -------------------------------------------------------------------- */
        char	*pszText, szWorkMDI[200];
        float   fResolution;
        uint16  nResUnits;

        if( TIFFGetField( hTIFF, TIFFTAG_DOCUMENTNAME, &pszText ) )
            SetMetadataItem( "TIFFTAG_DOCUMENTNAME",  pszText );

        if( TIFFGetField( hTIFF, TIFFTAG_IMAGEDESCRIPTION, &pszText ) )
            SetMetadataItem( "TIFFTAG_IMAGEDESCRIPTION", pszText );

        if( TIFFGetField( hTIFF, TIFFTAG_SOFTWARE, &pszText ) )
            SetMetadataItem( "TIFFTAG_SOFTWARE", pszText );

        if( TIFFGetField( hTIFF, TIFFTAG_DATETIME, &pszText ) )
            SetMetadataItem( "TIFFTAG_DATETIME", pszText );

        if( TIFFGetField( hTIFF, TIFFTAG_XRESOLUTION, &fResolution ) )
        {
            sprintf( szWorkMDI, "%.8g", fResolution );
            SetMetadataItem( "TIFFTAG_XRESOLUTION", szWorkMDI );
        }

        if( TIFFGetField( hTIFF, TIFFTAG_YRESOLUTION, &fResolution ) )
        {
            sprintf( szWorkMDI, "%.8g", fResolution );
            SetMetadataItem( "TIFFTAG_YRESOLUTION", szWorkMDI );
        }

        if( TIFFGetField( hTIFF, TIFFTAG_RESOLUTIONUNIT, &nResUnits ) )
        {
            if( nResUnits == RESUNIT_NONE )
                sprintf( szWorkMDI, "%d (unitless)", nResUnits );
            else if( nResUnits == RESUNIT_INCH )
                sprintf( szWorkMDI, "%d (pixels/inch)", nResUnits );
            else if( nResUnits == RESUNIT_CENTIMETER )
                sprintf( szWorkMDI, "%d (pixels/cm)", nResUnits );
            else
                sprintf( szWorkMDI, "%d", nResUnits );
            SetMetadataItem( "TIFFTAG_RESOLUTIONUNIT", szWorkMDI );
        }

        if( TIFFGetField( hTIFF, TIFFTAG_GDAL_METADATA, &pszText ) )
        {
            CPLXMLNode *psRoot = CPLParseXMLString( pszText );
            CPLXMLNode *psItem = NULL;

            if( psRoot != NULL && psRoot->eType == CXT_Element
                && EQUAL(psRoot->pszValue,"GDALMetadata") )
                psItem = psRoot->psChild;

            for( ; psItem != NULL; psItem = psItem->psNext )
            {
                const char *pszKey, *pszValue, *pszRole; 
                char *pszUnescapedValue;
                int nBand;

                if( psItem->eType != CXT_Element
                    || !EQUAL(psItem->pszValue,"Item") )
                    continue;

                pszKey = CPLGetXMLValue( psItem, "name", NULL );
                pszValue = CPLGetXMLValue( psItem, NULL, NULL );
                nBand = atoi(CPLGetXMLValue( psItem, "sample", "-1" )) + 1;
                pszRole = CPLGetXMLValue( psItem, "role", "" );
                
                if( pszKey == NULL || pszValue == NULL )
                    continue;

                pszUnescapedValue = CPLUnescapeString( pszValue, NULL, 
                                                       CPLES_XML );
                if( nBand == 0 )
                    SetMetadataItem( pszKey, pszUnescapedValue );
                else
                {
                    GDALRasterBand *poBand = GetRasterBand(nBand);
                    if( poBand != NULL )
                    {
                        if( EQUAL(pszRole,"scale") )
                            poBand->SetScale( atof(pszUnescapedValue) );
                        else if( EQUAL(pszRole,"offset") )
                            poBand->SetOffset( atof(pszUnescapedValue) );
                        else
                            poBand->SetMetadataItem(pszKey,pszUnescapedValue);
                    }
                }
                CPLFree( pszUnescapedValue );
            }

            CPLDestroyXMLNode( psRoot );
        }

        bMetadataChanged = FALSE;
	
        if( TIFFGetField( hTIFF, TIFFTAG_GDAL_NODATA, &pszText ) )
        {
	    bNoDataSet = TRUE;
	    dfNoDataValue = atof( pszText );
	}

	bNoDataChanged = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If this is a "base" raster, we should scan for any              */
/*      associated overviews.                                           */
/* -------------------------------------------------------------------- */
    if( bBase )
    {
        while( !TIFFLastDirectory( hTIFF ) 
               && TIFFReadDirectory( hTIFF ) != 0 )
        {
            uint32	nThisDir = TIFFCurrentDirOffset(hTIFF);
            uint32	nSubType;

            if( TIFFGetField(hTIFF, TIFFTAG_SUBFILETYPE, &nSubType)
                && (nSubType & FILETYPE_REDUCEDIMAGE) )
            {
                GTiffDataset	*poODS;

                poODS = new GTiffDataset();
                if( poODS->OpenOffset( hTIFF, nThisDir, FALSE, 
                                       eAccess ) != CE_None 
                    || poODS->GetRasterCount() != GetRasterCount() )
                {
                    delete poODS;
                }
                else
                {
                    CPLDebug( "GTiff", "Opened %dx%d overview.\n", 
                              poODS->GetRasterXSize(), poODS->GetRasterYSize());
                    nOverviewCount++;
                    papoOverviewDS = (GTiffDataset **)
                        CPLRealloc(papoOverviewDS, 
                                   nOverviewCount * (sizeof(void*)));
                    papoOverviewDS[nOverviewCount-1] = poODS;
                }
            }

            SetDirectory( nThisDir );
        }
    }

    return( CE_None );
}

/************************************************************************/
/*                              SetupTFW()                              */
/************************************************************************/

void GTiffDataset::SetupTFW( const char *pszTIFFilename )

{
    char	*pszPath;
    char	*pszBasename;
    
    pszPath = CPLStrdup( CPLGetPath(pszTIFFilename) );
    pszBasename = CPLStrdup( CPLGetBasename(pszTIFFilename) );

    pszTFWFilename = CPLStrdup( CPLFormFilename(pszPath,pszBasename,"tfw") );
    
    CPLFree( pszPath );
    CPLFree( pszBasename );
}

/************************************************************************/
/*                            GTiffCreate()                             */
/*                                                                      */
/*      Shared functionality between GTiffDataset::Create() and         */
/*      GTiffCreateCopy() for creating TIFF file based on a set of      */
/*      options and a configuration.                                    */
/************************************************************************/

TIFF *GTiffCreate( const char * pszFilename,
                   int nXSize, int nYSize, int nBands,
                   GDALDataType eType,
                   char **papszParmList )

{
    TIFF		*hTIFF;
    int                 nBlockXSize = 0, nBlockYSize = 0;
    int                 bTiled = FALSE;
    int                 nCompression = COMPRESSION_NONE;
    uint16              nSampleFormat;
    int			nPlanar;
    const char          *pszValue;

    GTiffOneTimeInit();

/* -------------------------------------------------------------------- */
/*	Setup values based on options.					*/
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszParmList,"TILED") != NULL )
        bTiled = TRUE;

    pszValue = CSLFetchNameValue(papszParmList,"BLOCKXSIZE");
    if( pszValue != NULL )
        nBlockXSize = atoi( pszValue );

    pszValue = CSLFetchNameValue(papszParmList,"BLOCKYSIZE");
    if( pszValue != NULL )
        nBlockYSize = atoi( pszValue );

    pszValue = CSLFetchNameValue(papszParmList,"INTERLEAVE");
    if( pszValue != NULL )
    {
        pszValue = CSLFetchNameValue(papszParmList,"INTERLEAVE");
        if( EQUAL( pszValue, "PIXEL" ) )
            nPlanar = PLANARCONFIG_CONTIG;
        else if( EQUAL( pszValue, "BAND" ) )
            nPlanar = PLANARCONFIG_SEPARATE;
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "INTERLEAVE=%s unsupported, value must be PIXEL or BAND.",
                      pszValue );
            return NULL;
        }
    }
    else 
    {
        if( nBands == 1 )
            nPlanar = PLANARCONFIG_CONTIG;
        else
            nPlanar = PLANARCONFIG_SEPARATE;
    }

    pszValue = CSLFetchNameValue(papszParmList,"COMPRESS");
    if( pszValue  != NULL )
    {
        if( EQUAL( pszValue, "JPEG" ) )
            nCompression = COMPRESSION_JPEG;
        else if( EQUAL( pszValue, "LZW" ) )
            nCompression = COMPRESSION_LZW;
        else if( EQUAL( pszValue, "PACKBITS" ))
            nCompression = COMPRESSION_PACKBITS;
        else if( EQUAL( pszValue, "DEFLATE" ) || EQUAL( pszValue, "ZIP" ))
            nCompression = COMPRESSION_ADOBE_DEFLATE;
        else
            CPLError( CE_Warning, CPLE_IllegalArg, 
                      "COMPRESS=%s value not recognised, ignoring.",
                      pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    hTIFF = XTIFFOpen( pszFilename, "w+" );
    if( hTIFF == NULL )
    {
        if( CPLGetLastErrorNo() == 0 )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Attempt to create new tiff file `%s'\n"
                      "failed in XTIFFOpen().\n",
                      pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Setup some standard flags.                                      */
/* -------------------------------------------------------------------- */
    TIFFSetField( hTIFF, TIFFTAG_COMPRESSION, nCompression );
    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, nXSize );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, nYSize );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE, GDALGetDataTypeSize(eType) );

    if( eType == GDT_Int16 || eType == GDT_Int32 )
        nSampleFormat = SAMPLEFORMAT_INT;
    else if( eType == GDT_CInt16 || eType == GDT_CInt32 )
        nSampleFormat = SAMPLEFORMAT_COMPLEXINT;
    else if( eType == GDT_Float32 || eType == GDT_Float64 )
        nSampleFormat = SAMPLEFORMAT_IEEEFP;
    else if( eType == GDT_CFloat32 || eType == GDT_CFloat64 )
        nSampleFormat = SAMPLEFORMAT_COMPLEXIEEEFP;
    else
        nSampleFormat = SAMPLEFORMAT_UINT;

    TIFFSetField( hTIFF, TIFFTAG_SAMPLEFORMAT, nSampleFormat );
    TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, nBands );
    TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, nPlanar );

/* -------------------------------------------------------------------- */
/*      Setup Photometric Interpretation. Take this value from the user */
/*      passed option or guess correct value otherwise.                 */
/* -------------------------------------------------------------------- */
    int nSamplesAccountedFor = 1;

    pszValue = CSLFetchNameValue(papszParmList,"PHOTOMETRIC");
    if( pszValue != NULL )
    {
        if( EQUAL( pszValue, "MINISBLACK" ) )
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
        else if( EQUAL( pszValue, "MINISWHITE" ) )
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE );
        else if( EQUAL( pszValue, "RGB" ))
        {
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
            nSamplesAccountedFor = 3;
        }
        else if( EQUAL( pszValue, "CMYK" ))
        {
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED );
            nSamplesAccountedFor = 4;
        }
        else if( EQUAL( pszValue, "YCBCR" ))
        {
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_YCBCR );
            nSamplesAccountedFor = 3;
        }
        else if( EQUAL( pszValue, "CIELAB" ))
        {
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_CIELAB );
            nSamplesAccountedFor = 3;
        }
        else if( EQUAL( pszValue, "ICCLAB" ))
        {
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_ICCLAB );
            nSamplesAccountedFor = 3;
        }
        else if( EQUAL( pszValue, "ITULAB" ))
        {
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_ITULAB );
            nSamplesAccountedFor = 3;
        }
        else
        {
            CPLError( CE_Warning, CPLE_IllegalArg, 
                      "PHOTOMETRIC=%s value not recognised, ignoring.\n"
                      "Set the Photometric Interpretation as MINISBLACK.", 
                      pszValue );
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
        }
    }
    else
    {
        /* 
         * If image contains 3 or 4 bands and datatype is Byte then we will
         * assume it is RGB. In all other cases assume it is MINISBLACK.
         */
        if( nBands == 3 && eType == GDT_Byte )
        {
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
            nSamplesAccountedFor = 3;
        }
        else if( nBands == 4 && eType == GDT_Byte )
        {
            uint16 v[1];

            v[0] = EXTRASAMPLE_ASSOCALPHA;
            TIFFSetField(hTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
            nSamplesAccountedFor = 4;
        }
        else
        {
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
            nSamplesAccountedFor = 1;
        }
    }

/* -------------------------------------------------------------------- */
/*      If there are extra samples, we need to mark them with an        */
/*      appropriate extrasamples definition here.                       */
/* -------------------------------------------------------------------- */
    if( nBands > nSamplesAccountedFor )
    {
        uint16 *v;
        int i;
        int nExtraSamples = nBands - nSamplesAccountedFor;

        v = (uint16 *) CPLMalloc( sizeof(uint16) * nExtraSamples );

        if( CSLFetchBoolean(papszParmList,"ALPHA",FALSE) )
            v[0] = EXTRASAMPLE_ASSOCALPHA;
        else
            v[0] = EXTRASAMPLE_UNSPECIFIED;
            
        for( i = 1; i < nExtraSamples; i++ )
            v[i] = EXTRASAMPLE_UNSPECIFIED;

        TIFFSetField(hTIFF, TIFFTAG_EXTRASAMPLES, nExtraSamples, v );
    }
    
/* -------------------------------------------------------------------- */
/*      Setup tiling/stripping flags.                                   */
/* -------------------------------------------------------------------- */
    if( bTiled )
    {
        if( nBlockXSize == 0 )
            nBlockXSize = 256;
        
        if( nBlockYSize == 0 )
            nBlockYSize = 256;

        TIFFSetField( hTIFF, TIFFTAG_TILEWIDTH, nBlockXSize );
        TIFFSetField( hTIFF, TIFFTAG_TILELENGTH, nBlockYSize );
    }
    else
    {
        uint32 nRowsPerStrip;

        if( nBlockYSize == 0 )
            nRowsPerStrip = MIN(nYSize, (int)TIFFDefaultStripSize(hTIFF,0));
        else
            nRowsPerStrip = nBlockYSize;
        
        TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP, nRowsPerStrip );
    }
    
    return( hTIFF );
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new GeoTIFF or TIFF file.                              */
/************************************************************************/

GDALDataset *GTiffDataset::Create( const char * pszFilename,
                                   int nXSize, int nYSize, int nBands,
                                   GDALDataType eType,
                                   char **papszParmList )

{
    GTiffDataset *	poDS;
    TIFF		*hTIFF;

/* -------------------------------------------------------------------- */
/*      Create the underlying TIFF file.                                */
/* -------------------------------------------------------------------- */
    hTIFF = GTiffCreate( pszFilename, nXSize, nYSize, nBands, 
                         eType, papszParmList );

    if( hTIFF == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the new GTiffDataset object.                             */
/* -------------------------------------------------------------------- */
    poDS = new GTiffDataset();
    poDS->hTIFF = hTIFF;

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->bNewDataset = TRUE;
    poDS->bCrystalized = FALSE;
    poDS->pszProjection = CPLStrdup("");
    poDS->nSamplesPerPixel = (uint16) nBands;

    TIFFGetField( hTIFF, TIFFTAG_SAMPLEFORMAT, &(poDS->nSampleFormat) );
    TIFFGetField( hTIFF, TIFFTAG_PLANARCONFIG, &(poDS->nPlanarConfig) );
    TIFFGetField( hTIFF, TIFFTAG_PHOTOMETRIC, &(poDS->nPhotometric) );
    TIFFGetField( hTIFF, TIFFTAG_BITSPERSAMPLE, &(poDS->nBitsPerSample) );
    TIFFGetField( hTIFF, TIFFTAG_COMPRESSION, &(poDS->nCompression) );

    if( TIFFIsTiled(hTIFF) )
    {
        TIFFGetField( hTIFF, TIFFTAG_TILEWIDTH, &(poDS->nBlockXSize) );
        TIFFGetField( hTIFF, TIFFTAG_TILELENGTH, &(poDS->nBlockYSize) );
    }
    else
    {
        if( !TIFFGetField( hTIFF, TIFFTAG_ROWSPERSTRIP,
                           &(poDS->nRowsPerStrip) ) )
            poDS->nRowsPerStrip = 1; /* dummy value */

        poDS->nBlockXSize = nXSize;
        poDS->nBlockYSize = MIN((int)poDS->nRowsPerStrip,nYSize);
    }

    poDS->nBlocksPerBand =
        ((nYSize + poDS->nBlockYSize - 1) / poDS->nBlockYSize)
        * ((nXSize + poDS->nBlockXSize - 1) / poDS->nBlockXSize);

/* -------------------------------------------------------------------- */
/*      Do we need a TFW file?                                          */
/* -------------------------------------------------------------------- */
    if( CSLFetchBoolean( papszParmList, "TFW", FALSE ) 
        || CSLFetchBoolean( papszParmList, "WORLDFILE", FALSE ) )
        poDS->SetupTFW( pszFilename );

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int		iBand;

    for( iBand = 0; iBand < nBands; iBand++ )
    {
        poDS->SetBand( iBand+1, new GTiffRasterBand( poDS, iBand+1 ) );
    }

    return( poDS );
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

static GDALDataset *
GTiffCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                 int bStrict, char ** papszOptions, 
                 GDALProgressFunc pfnProgress, void * pProgressData )

{
    TIFF	*hTIFF;
    int		nXSize = poSrcDS->GetRasterXSize();
    int		nYSize = poSrcDS->GetRasterYSize();
    int		nBands = poSrcDS->GetRasterCount();
    int         iBand;
    GDALDataType eType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    CPLErr      eErr = CE_None;
    uint16	nPlanarConfig;
        
/* -------------------------------------------------------------------- */
/*      Check, whether all bands in input dataset has the same type.    */
/* -------------------------------------------------------------------- */
    for ( iBand = 2; iBand <= nBands; iBand++ )
    {
        if ( eType != poSrcDS->GetRasterBand(iBand)->GetRasterDataType() )
        {
            if ( bStrict )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Unable to export GeoTIFF file with different datatypes per\n"
                          "different bands. All bands should have the same types in TIFF." );
                return NULL;
            }
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Unable to export GeoTIFF file with different datatypes per\n"
                          "different bands. All bands should have the same types in TIFF." );
            }
        }
    }

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Capture the profile.                                            */
/* -------------------------------------------------------------------- */
    const char          *pszProfile;
    int                 bGeoTIFF;

    pszProfile = CSLFetchNameValue(papszOptions,"PROFILE");
    if( pszProfile == NULL )
        pszProfile = "GDALGeoTIFF";

    if( !EQUAL(pszProfile,"BASELINE") 
        && !EQUAL(pszProfile,"GeoTIFF") 
        && !EQUAL(pszProfile,"GDALGeoTIFF") )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "PROFILE=%s not supported in GTIFF driver.", 
                  pszProfile );
        return NULL;
    }
    
    if( EQUAL(pszProfile,"BASELINE") )
        bGeoTIFF = FALSE;
    else
        bGeoTIFF = TRUE;

/* -------------------------------------------------------------------- */
/*      Create the file.                                                */
/* -------------------------------------------------------------------- */
    hTIFF = GTiffCreate( pszFilename, nXSize, nYSize, nBands, 
                         eType, papszOptions );

    if( hTIFF == NULL )
        return NULL;

    TIFFGetField( hTIFF, TIFFTAG_PLANARCONFIG, &nPlanarConfig );

/* -------------------------------------------------------------------- */
/*      Are we really producing an RGBA image?  If so, set the          */
/*      associated alpha information.                                   */
/* -------------------------------------------------------------------- */
    if( nBands == 4 
        && poSrcDS->GetRasterBand(4)->GetColorInterpretation()==GCI_AlphaBand)
    {
        uint16 v[1];

        v[0] = EXTRASAMPLE_ASSOCALPHA;
	TIFFSetField(hTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
    }

/* -------------------------------------------------------------------- */
/*      Does the source image consist of one band, with a palette?      */
/*      If so, copy over.                                               */
/* -------------------------------------------------------------------- */
    if( nBands == 1 && poSrcDS->GetRasterBand(1)->GetColorTable() != NULL 
        && eType == GDT_Byte )
    {
        unsigned short	anTRed[256], anTGreen[256], anTBlue[256];
        GDALColorTable *poCT;

        poCT = poSrcDS->GetRasterBand(1)->GetColorTable();
        
        for( int iColor = 0; iColor < 256; iColor++ )
        {
            if( iColor < poCT->GetColorEntryCount() )
            {
                GDALColorEntry  sRGB;

                poCT->GetColorEntryAsRGB( iColor, &sRGB );

                anTRed[iColor] = (unsigned short) (256 * sRGB.c1);
                anTGreen[iColor] = (unsigned short) (256 * sRGB.c2);
                anTBlue[iColor] = (unsigned short) (256 * sRGB.c3);
            }
            else
            {
                anTRed[iColor] = anTGreen[iColor] = anTBlue[iColor] = 0;
            }
        }

        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
        TIFFSetField( hTIFF, TIFFTAG_COLORMAP, anTRed, anTGreen, anTBlue );
    }
    else if( nBands == 1 
             && poSrcDS->GetRasterBand(1)->GetColorTable() != NULL 
             && eType == GDT_UInt16 )
    {
        unsigned short	anTRed[65536], anTGreen[65536], anTBlue[65536];
        GDALColorTable *poCT;

        poCT = poSrcDS->GetRasterBand(1)->GetColorTable();
        
        for( int iColor = 0; iColor < 65536; iColor++ )
        {
            if( iColor < poCT->GetColorEntryCount() )
            {
                GDALColorEntry  sRGB;

                poCT->GetColorEntryAsRGB( iColor, &sRGB );

                anTRed[iColor] = (unsigned short) (256 * sRGB.c1);
                anTGreen[iColor] = (unsigned short) (256 * sRGB.c2);
                anTBlue[iColor] = (unsigned short) (256 * sRGB.c3);
            }
            else
            {
                anTRed[iColor] = anTGreen[iColor] = anTBlue[iColor] = 0;
            }
        }

        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
        TIFFSetField( hTIFF, TIFFTAG_COLORMAP, anTRed, anTGreen, anTBlue );
    }
    else if( poSrcDS->GetRasterBand(1)->GetColorTable() != NULL )
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unable to export color table to GeoTIFF file.  Color tables\n"
                  "can only be written to 1 band Byte or UInt16 GeoTIFF files." );

/* -------------------------------------------------------------------- */
/* 	Transfer some TIFF specific metadata, if available.             */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszProfile,"GDALGeoTIFF") )
        GTiffDataset::WriteMetadata( poSrcDS, hTIFF );

/* -------------------------------------------------------------------- */
/* 	Write NoData value, if exist.                                   */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszProfile,"GDALGeoTIFF") )
    {
        int		bSuccess;
        double	dfNoData;
    
        dfNoData = poSrcDS->GetRasterBand(1)->GetNoDataValue( &bSuccess );
        if ( bSuccess )
            GTiffDataset::WriteNoDataValue( hTIFF, dfNoData );
    }

/* -------------------------------------------------------------------- */
/*      Write affine transform if it is meaningful.                     */
/* -------------------------------------------------------------------- */
    const char *pszProjection = NULL;
    double      adfGeoTransform[6];
    
    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None
        && (adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0
            || adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0
            || adfGeoTransform[4] != 0.0 || ABS(adfGeoTransform[5]) != 1.0 ))
    {
        if( bGeoTIFF )
        {
            if( adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0 
                && adfGeoTransform[5] < 0.0 )
            {

                double	adfPixelScale[3], adfTiePoints[6];

                adfPixelScale[0] = adfGeoTransform[1];
                adfPixelScale[1] = fabs(adfGeoTransform[5]);
                adfPixelScale[2] = 0.0;

                TIFFSetField( hTIFF, TIFFTAG_GEOPIXELSCALE, 3, adfPixelScale );
            
                adfTiePoints[0] = 0.0;
                adfTiePoints[1] = 0.0;
                adfTiePoints[2] = 0.0;
                adfTiePoints[3] = adfGeoTransform[0];
                adfTiePoints[4] = adfGeoTransform[3];
                adfTiePoints[5] = 0.0;
        
                TIFFSetField( hTIFF, TIFFTAG_GEOTIEPOINTS, 6, adfTiePoints );
            }
            else
            {
                double	adfMatrix[16];

                memset(adfMatrix,0,sizeof(double) * 16);

                adfMatrix[0] = adfGeoTransform[1];
                adfMatrix[1] = adfGeoTransform[2];
                adfMatrix[3] = adfGeoTransform[0];
                adfMatrix[4] = adfGeoTransform[4];
                adfMatrix[5] = adfGeoTransform[5];
                adfMatrix[7] = adfGeoTransform[3];
                adfMatrix[15] = 1.0;

                TIFFSetField( hTIFF, TIFFTAG_GEOTRANSMATRIX, 16, adfMatrix );
            }
            
            pszProjection = poSrcDS->GetProjectionRef();
        }

/* -------------------------------------------------------------------- */
/*      Do we need a TFW file?                                          */
/* -------------------------------------------------------------------- */
        if( CSLFetchBoolean( papszOptions, "TFW", FALSE ) )
            GDALWriteWorldFile( pszFilename, "tfw", adfGeoTransform );
        else if( CSLFetchBoolean( papszOptions, "WORLDFILE", FALSE ) )
            GDALWriteWorldFile( pszFilename, "wld", adfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise write tiepoints if they are available.                */
/* -------------------------------------------------------------------- */
    else if( poSrcDS->GetGCPCount() > 0 && bGeoTIFF )
    {
        const GDAL_GCP *pasGCPs = poSrcDS->GetGCPs();
        double	*padfTiePoints;

        padfTiePoints = (double *) 
            CPLMalloc(6*sizeof(double)*poSrcDS->GetGCPCount());

        for( int iGCP = 0; iGCP < poSrcDS->GetGCPCount(); iGCP++ )
        {

            padfTiePoints[iGCP*6+0] = pasGCPs[iGCP].dfGCPPixel;
            padfTiePoints[iGCP*6+1] = pasGCPs[iGCP].dfGCPLine;
            padfTiePoints[iGCP*6+2] = 0;
            padfTiePoints[iGCP*6+3] = pasGCPs[iGCP].dfGCPX;
            padfTiePoints[iGCP*6+4] = pasGCPs[iGCP].dfGCPY;
            padfTiePoints[iGCP*6+5] = pasGCPs[iGCP].dfGCPZ;
        }

        TIFFSetField( hTIFF, TIFFTAG_GEOTIEPOINTS, 
                      6*poSrcDS->GetGCPCount(), padfTiePoints );
        CPLFree( padfTiePoints );
        
        pszProjection = poSrcDS->GetGCPProjection();
    }

    else
        pszProjection = poSrcDS->GetProjectionRef();

/* -------------------------------------------------------------------- */
/*      Write the projection information, if possible.                  */
/* -------------------------------------------------------------------- */
    if( pszProjection != NULL && strlen(pszProjection) > 0 && bGeoTIFF )
    {
        GTIF	*psGTIF;

        psGTIF = GTIFNew( hTIFF );
        GTIFSetFromOGISDefn( psGTIF, pszProjection );
        GTIFWriteKeys( psGTIF );
        GTIFFree( psGTIF );
    }

/* -------------------------------------------------------------------- */
/*      Copy image data ... tiled.                                      */
/* -------------------------------------------------------------------- */
    if( TIFFIsTiled( hTIFF ) && nPlanarConfig == PLANARCONFIG_SEPARATE )
    {
        uint32  nBlockXSize;
        uint32	nBlockYSize;
        int     nTileSize, nTilesAcross, nTilesDown, iTileX, iTileY;
        GByte   *pabyTile;
        int     nTilesDone = 0, nPixelSize;

        TIFFGetField( hTIFF, TIFFTAG_TILEWIDTH, &nBlockXSize );
        TIFFGetField( hTIFF, TIFFTAG_TILELENGTH, &nBlockYSize );
        
        nTilesAcross = (nXSize+nBlockXSize-1) / nBlockXSize;
        nTilesDown = (nYSize+nBlockYSize-1) / nBlockYSize;

        nPixelSize = GDALGetDataTypeSize(eType) / 8;
        nTileSize =  nPixelSize * nBlockXSize * nBlockYSize;
        pabyTile = (GByte *) CPLMalloc(nTileSize);

        for(iBand = 0; eErr == CE_None && iBand < nBands; iBand++ )
        {
            GDALRasterBand *poBand = poSrcDS->GetRasterBand(iBand+1);

            for( iTileY = 0; 
                 eErr == CE_None && iTileY < nTilesDown; 
                 iTileY++ )
            {
                for( iTileX = 0; 
                     eErr == CE_None && iTileX < nTilesAcross; 
                     iTileX++ )
                {
                    int   nThisBlockXSize = nBlockXSize;
                    int   nThisBlockYSize = nBlockYSize;

                    if( (int) ((iTileX+1) * nBlockXSize) > nXSize )
                    {
                        nThisBlockXSize = nXSize - iTileX*nBlockXSize;
                        memset( pabyTile, 0, nTileSize );
                    }

                    if( (int) ((iTileY+1) * nBlockYSize) > nYSize )
                    {
                        nThisBlockYSize = nYSize - iTileY*nBlockYSize;
                        memset( pabyTile, 0, nTileSize );
                    }

                    eErr = poBand->RasterIO( GF_Read, 
                                             iTileX * nBlockXSize, 
                                             iTileY * nBlockYSize, 
                                             nThisBlockXSize, 
                                             nThisBlockYSize,
                                             pabyTile,
                                             nThisBlockXSize, 
                                             nThisBlockYSize, eType,
                                             nPixelSize, 
                                             nBlockXSize * nPixelSize );

                    TIFFWriteEncodedTile( hTIFF, nTilesDone, pabyTile, 
                                          nTileSize );

                    nTilesDone++;

                    if( eErr == CE_None 
                        && !pfnProgress( nTilesDone / 
                                         ((double) nTilesAcross * nTilesDown * nBands),
                                         NULL, pProgressData ) )
                    {
                        eErr = CE_Failure;
                        CPLError( CE_Failure, CPLE_UserInterrupt, 
                                  "User terminated CreateCopy()" );
                    }
                }
                
            }
        }

        CPLFree( pabyTile );
    }
/* -------------------------------------------------------------------- */
/*      Copy image data, one scanline at a time.                        */
/* -------------------------------------------------------------------- */
    else if( !TIFFIsTiled(hTIFF) && nPlanarConfig == PLANARCONFIG_SEPARATE )
    {
        int     nLinesDone = 0, nPixelSize, nLineSize;
        GByte   *pabyLine;

        nPixelSize = GDALGetDataTypeSize(eType) / 8;
        nLineSize =  nPixelSize * nXSize;
        pabyLine = (GByte *) CPLMalloc(nLineSize);

        for( int iBand = 0; 
             eErr == CE_None && iBand < nBands; 
             iBand++ )
        {
            GDALRasterBand *poBand = poSrcDS->GetRasterBand(iBand+1);

            for( int iLine = 0; 
                 eErr == CE_None && iLine < nYSize; 
                 iLine++ )
            {
                eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                         pabyLine, nXSize, 1, eType, 
                                         0, 0 );
                if( eErr == CE_None 
                    && TIFFWriteScanline( hTIFF, pabyLine, iLine, 
                                          (tsample_t) iBand ) == -1 )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "TIFFWriteScanline failed." );
                    eErr = CE_Failure;
                }

                nLinesDone++;
                if( eErr == CE_None 
                    && !pfnProgress( nLinesDone / 
                                     ((double) nYSize * nBands), 
                                     NULL, pProgressData ) )
                {
                    eErr = CE_Failure;
                    CPLError( CE_Failure, CPLE_UserInterrupt, 
                              "User terminated CreateCopy()" );
                }
            }
        }

        CPLFree( pabyLine );
    }

/* -------------------------------------------------------------------- */
/*      Copy image data ... tiled.                                      */
/* -------------------------------------------------------------------- */
    else if( TIFFIsTiled( hTIFF ) && nPlanarConfig == PLANARCONFIG_CONTIG )
    {
        uint32  nBlockXSize;
        uint32	nBlockYSize;
        int     nTileSize, nTilesAcross, nTilesDown, iTileX, iTileY, nElemSize;
        GByte   *pabyTile;
        int     nTilesDone = 0, nPixelSize;

        TIFFGetField( hTIFF, TIFFTAG_TILEWIDTH, &nBlockXSize );
        TIFFGetField( hTIFF, TIFFTAG_TILELENGTH, &nBlockYSize );
        
        nTilesAcross = (nXSize+nBlockXSize-1) / nBlockXSize;
        nTilesDown = (nYSize+nBlockYSize-1) / nBlockYSize;

        nElemSize = GDALGetDataTypeSize(eType) / 8;
        nPixelSize = nElemSize * nBands;
        nTileSize =  nPixelSize * nBlockXSize * nBlockYSize;
        pabyTile = (GByte *) CPLMalloc(nTileSize);

        for( iTileY = 0; 
             eErr == CE_None && iTileY < nTilesDown; 
             iTileY++ )
        {
            for( iTileX = 0; 
                 eErr == CE_None && iTileX < nTilesAcross; 
                 iTileX++ )
            {
                int   nThisBlockXSize = nBlockXSize;
                int   nThisBlockYSize = nBlockYSize;
                
                if( (int) ((iTileX+1) * nBlockXSize) > nXSize )
                {
                    nThisBlockXSize = nXSize - iTileX*nBlockXSize;
                    memset( pabyTile, 0, nTileSize );
                }

                if( (int) ((iTileY+1) * nBlockYSize) > nYSize )
                {
                    nThisBlockYSize = nYSize - iTileY*nBlockYSize;
                    memset( pabyTile, 0, nTileSize );
                }

                for( int iBand = 0; 
                     eErr == CE_None && iBand < nBands; 
                     iBand++ )
                {
                    GDALRasterBand *poBand = poSrcDS->GetRasterBand(iBand+1);

                    eErr = poBand->RasterIO( GF_Read, 
                                             iTileX * nBlockXSize, 
                                             iTileY * nBlockYSize, 
                                             nThisBlockXSize, 
                                             nThisBlockYSize,
                                             pabyTile + iBand * nElemSize,
                                             nThisBlockXSize, 
                                             nThisBlockYSize, eType,
                                             nPixelSize, 
                                             nBlockXSize * nPixelSize );
                }

                TIFFWriteEncodedTile( hTIFF, nTilesDone, pabyTile, 
                                      nTileSize );

                nTilesDone++;

                if( eErr == CE_None 
                    && !pfnProgress( nTilesDone / 
                                     ((double) nTilesAcross * nTilesDown),
                                     NULL, pProgressData ) )
                {
                    eErr = CE_Failure;
                    CPLError( CE_Failure, CPLE_UserInterrupt, 
                              "User terminated CreateCopy()" );
                }
            }
        }

        CPLFree( pabyTile );
    }
/* -------------------------------------------------------------------- */
/*      Copy image data, one scanline at a time.                        */
/* -------------------------------------------------------------------- */
    else if( !TIFFIsTiled(hTIFF) && nPlanarConfig == PLANARCONFIG_CONTIG )
    {
        int     nLinesDone = 0, nPixelSize, nLineSize, nElemSize;
        GByte   *pabyLine;

        nElemSize = GDALGetDataTypeSize(eType) / 8;
        nPixelSize = nElemSize * nBands;
        nLineSize =  nPixelSize * nXSize;
        pabyLine = (GByte *) CPLMalloc(nLineSize);

        for( int iLine = 0; 
             eErr == CE_None && iLine < nYSize; 
             iLine++ )
        {
            eErr = 
                poSrcDS->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                   pabyLine, nXSize, 1, eType, nBands, NULL, 
                                   nPixelSize, nLineSize, nElemSize );

            if( eErr == CE_None 
                && TIFFWriteScanline( hTIFF, pabyLine, iLine, 0 ) == -1 )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFWriteScanline failed." );
                eErr = CE_Failure;
            }

            nLinesDone++;
            if( eErr == CE_None 
                && !pfnProgress( nLinesDone / ((double) nYSize), 
                                 NULL, pProgressData ) )
            {
                eErr = CE_Failure;
                CPLError( CE_Failure, CPLE_UserInterrupt, 
                          "User terminated CreateCopy()" );
            }
        }

        CPLFree( pabyLine );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    TIFFFlush( hTIFF );
    XTIFFClose( hTIFF );

    if( eErr != CE_None )
    {
        VSIUnlink( pszFilename );
        return NULL;
    }

    GDALDataset *poDS;

    poDS = (GDALDataset *) GDALOpen( pszFilename, GA_Update );
    if( poDS == NULL )
        poDS = (GDALDataset *) GDALOpen( pszFilename, GA_ReadOnly );
    
    return poDS;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *GTiffDataset::GetProjectionRef()

{
    if( nGCPCount == 0 )
        return( pszProjection );
    else
        return "";
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr GTiffDataset::SetProjection( const char * pszNewProjection )

{
    if( !EQUALN(pszNewProjection,"GEOGCS",6)
        && !EQUALN(pszNewProjection,"PROJCS",6)
        && !EQUAL(pszNewProjection,"") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Only OGC WKT Projections supported for writing to GeoTIFF.\n"
                "%s not supported.",
                  pszNewProjection );
        
        return CE_Failure;
    }
    
    CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszNewProjection );

    bGeoTIFFInfoChanged = TRUE;

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GTiffDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );

    if( bGeoTransformValid )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GTiffDataset::SetGeoTransform( double * padfTransform )

{
    if( GetAccess() == GA_Update )
    {
        memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );
        bGeoTransformValid = TRUE;
        bGeoTIFFInfoChanged = TRUE;

        return( CE_None );
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
      "SetGeoTransform() is only supported on newly created GeoTIFF files." );
        return CE_Failure;
    }
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int GTiffDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *GTiffDataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
        return pszProjection;
    else
        return "";
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *GTiffDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                               SetGCPs()                              */
/************************************************************************/

CPLErr GTiffDataset::SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                              const char *pszGCPProjection )
{
    if( GetAccess() == GA_Update )
    {
        if( this->nGCPCount > 0 )
        {
            GDALDeinitGCPs( this->nGCPCount, this->pasGCPList );
            CPLFree( this->pasGCPList );
        }

	this->nGCPCount = nGCPCount;
	this->pasGCPList = GDALDuplicateGCPs(nGCPCount, pasGCPList);

        CPLFree( this->pszProjection );
	this->pszProjection = CPLStrdup( pszGCPProjection );
        bGeoTIFFInfoChanged = TRUE;

	return CE_None;
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
            "SetGCPs() is only supported on newly created GeoTIFF files." );
        return CE_Failure;
    }
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/
CPLErr GTiffDataset::SetMetadata( char ** papszMD, const char *pszDomain )

{
    bMetadataChanged = TRUE;
    return GDALDataset::SetMetadata( papszMD, pszDomain );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GTiffDataset::SetMetadataItem( const char *pszName, 
                                      const char *pszValue,
                                      const char *pszDomain )

{
    bMetadataChanged = TRUE;
    return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                         GetInternalHandle()                          */
/************************************************************************/

void *GTiffDataset::GetInternalHandle( const char * /* pszHandleName */ )

{
    return hTIFF;
}

/************************************************************************/
/*                       PrepareTIFFErrorFormat()                       */
/*                                                                      */
/*      sometimes the "module" has stuff in it that has special         */
/*      meaning in a printf() style format, so we try to escape it.     */
/*      For now we hope the only thing we have to escape is %'s.        */
/************************************************************************/

static char *PrepareTIFFErrorFormat( const char *module, const char *fmt )

{
    char      *pszModFmt;
    int       iIn, iOut;

    pszModFmt = (char *) CPLMalloc( strlen(module)*2 + strlen(fmt) + 2 );
    for( iOut = 0, iIn = 0; module[iIn] != '\0'; iIn++ )
    {
        if( module[iIn] == '%' )
        {
            pszModFmt[iOut++] = '%';
            pszModFmt[iOut++] = '%';
        }
        else
            pszModFmt[iOut++] = module[iIn];
    }
    pszModFmt[iOut] = '\0';
    strcat( pszModFmt, ":" );
    strcat( pszModFmt, fmt );

    return pszModFmt;
}

/************************************************************************/
/*                        GTiffWarningHandler()                         */
/************************************************************************/
void
GTiffWarningHandler(const char* module, const char* fmt, va_list ap )
{
    char	*pszModFmt;

    if( strstr(fmt,"unknown field") != NULL )
        return;

    pszModFmt = PrepareTIFFErrorFormat( module, fmt );
    CPLErrorV( CE_Warning, CPLE_AppDefined, pszModFmt, ap );
    CPLFree( pszModFmt );
}

/************************************************************************/
/*                        GTiffWarningHandler()                         */
/************************************************************************/
void
GTiffErrorHandler(const char* module, const char* fmt, va_list ap )
{
    char *pszModFmt;

    pszModFmt = PrepareTIFFErrorFormat( module, fmt );
    CPLErrorV( CE_Failure, CPLE_AppDefined, pszModFmt, ap );
    CPLFree( pszModFmt );
}

/************************************************************************/
/*                          GTiffTagExtender()                          */
/*                                                                      */
/*      Install tags specially known to GDAL.                           */
/************************************************************************/

static TIFFExtendProc _ParentExtender = NULL;

static void GTiffTagExtender(TIFF *tif)

{
    static const TIFFFieldInfo xtiffFieldInfo[] = {
        { TIFFTAG_GDAL_METADATA,    -1,-1, TIFF_ASCII,	FIELD_CUSTOM,
          TRUE,	FALSE,	"GDALMetadata" },
        { TIFFTAG_GDAL_NODATA,	    -1,-1, TIFF_ASCII,	FIELD_CUSTOM,
          TRUE,	FALSE,	"GDALNoDataValue" }
    };

    if (_ParentExtender) 
        (*_ParentExtender)(tif);

    TIFFMergeFieldInfo( tif, xtiffFieldInfo,
		        sizeof(xtiffFieldInfo) / sizeof(xtiffFieldInfo[0]) );
}

/************************************************************************/
/*                          GTiffOneTimeInit()                          */
/*                                                                      */
/*      This is stuff that is initialized for the TIFF library just     */
/*      once.  We deliberately defer the initialization till the        */
/*      first time we are likely to call into libtiff to avoid          */
/*      unnecessary paging in of the library for GDAL apps that         */
/*      don't use it.                                                   */
/************************************************************************/

static void GTiffOneTimeInit()

{
    static int bOneTimeInitDone = FALSE;
    
    if( bOneTimeInitDone )
        return;

    bOneTimeInitDone = TRUE;

    _ParentExtender = TIFFSetTagExtender(GTiffTagExtender);

    TIFFSetWarningHandler( GTiffWarningHandler );
    TIFFSetErrorHandler( GTiffErrorHandler );

    // This only really needed if we are linked to an external libgeotiff
    // with its own (lame) file searching logic. 
    SetCSVFilenameHook( GDALDefaultCSVFilename );
}

/************************************************************************/
/*                        GDALDeregister_GTiff()                        */
/************************************************************************/

void GDALDeregister_GTiff( GDALDriver * )

{
    CPLDebug( "GDAL", "GDALDeregister_GTiff() called." );
    CSVDeaccess( NULL );

#if defined(LIBGEOTIFF_VERSION) && LIBGEOTIFF_VERSION > 1150
    GTIFDeaccessCSV();
#endif
}

/************************************************************************/
/*                          GDALRegister_GTiff()                        */
/************************************************************************/

void GDALRegister_GTiff()

{
    if( GDALGetDriverByName( "GTiff" ) == NULL )
    {
        GDALDriver	*poDriver;
        char szCreateOptions[2048];
        char szOptionalCompressItems[500];

        poDriver = new GDALDriver();
        
/* -------------------------------------------------------------------- */
/*      Determine which compression codecs are available that we        */
/*      want to advertise.  If we are using an old libtiff we won't     */
/*      be able to find out so we just assume all are available.        */
/* -------------------------------------------------------------------- */
        strcpy( szOptionalCompressItems, 
                "       <Value>NONE</Value>" );

#if TIFFLIB_VERSION <= 20040919
        strcat( szOptionalCompressItems, 
                "       <Value>PACKBITS</Value>"
                "       <Value>JPEG</Value>"
                "       <Value>LZW</Value>"
                "       <Value>DEFLATE</Value>" );
#else
        TIFFCodec	*c, *codecs = TIFFGetConfiguredCODECs();

        for( c = codecs; c->name; c++ )
        {
            if( c->scheme == COMPRESSION_PACKBITS )
                strcat( szOptionalCompressItems,
                        "       <Value>PACKBITS</Value>" );
            else if( c->scheme == COMPRESSION_JPEG )
                strcat( szOptionalCompressItems,
                        "       <Value>JPEG</Value>" );
            else if( c->scheme == COMPRESSION_LZW )
                strcat( szOptionalCompressItems,
                        "       <Value>LZW</Value>" );
            else if( c->scheme == COMPRESSION_ADOBE_DEFLATE )
                strcat( szOptionalCompressItems,
                        "       <Value>DEFLATE</Value>" );
        }
        _TIFFfree( codecs );
#endif        

/* -------------------------------------------------------------------- */
/*      Build full creation option list.                                */
/* -------------------------------------------------------------------- */
        sprintf( szCreateOptions, "%s%s%s", 
"<CreationOptionList>"
"   <Option name='COMPRESS' type='string-select'>",
                 szOptionalCompressItems,
"   </Option>"
"   <Option name='INTERLEAVE' type='string-select'>"
"       <Value>BAND</Value>"
"       <Value>PIXEL</Value>"
"   </Option>"
"   <Option name='TILED' type='boolean' description='Switch to tiled format'/>"
"   <Option name='TFW' type='boolean' description='Write out world file'/>"
"   <Option name='BLOCKXSIZE' type='int' description='Tile Width'/>"
"   <Option name='BLOCKYSIZE' type='int' description='Tile/Strip Height'/>"
"   <Option name='PHOTOMETRIC' type='string-select'>"
"       <Value>MINISBLACK</Value>"
"       <Value>MINISWHITE</value>"
"       <Value>RGB</Value>"
"       <Value>CMYK</Value>"
"       <Value>YCBCR</Value>"
"       <Value>CIELAB</Value>"
"       <Value>ICCLAB</Value>"
"       <Value>ITULAB</Value>"
"   </Option>"
"   <Option name='PROFILE' type='string-select'>"
"       <Value>GDALGeoTIFF</Value>"
"       <Value>GeoTIFF</value>"
"       <Value>BASELINE</Value>"
"   </Option>"
"</CreationOptionList>" );
                 
/* -------------------------------------------------------------------- */
/*      Set the driver details.                                         */
/* -------------------------------------------------------------------- */
        poDriver->SetDescription( "GTiff" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "GeoTIFF" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_gtiff.html" );
        poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/tiff" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "tif" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte UInt16 Int16 UInt32 Int32 Float32 Float64 CInt16 CInt32 CFloat32 CFloat64" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
                                   szCreateOptions );

        poDriver->pfnOpen = GTiffDataset::Open;
        poDriver->pfnCreate = GTiffDataset::Create;
        poDriver->pfnCreateCopy = GTiffCreateCopy;
        poDriver->pfnUnloadDriver = GDALDeregister_GTiff;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
