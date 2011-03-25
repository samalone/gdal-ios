/******************************************************************************
 * $Id$
 *
 * Project:  GDAL WEBP Driver
 * Purpose:  Implement GDAL WEBP Support based on libwebp
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault
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
 ****************************************************************************/

#include "gdal_pam.h"
#include "cpl_string.h"

#include "webp/decode.h"
#include "webp/encode.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_WEBP(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                               WEBPDataset                            */
/* ==================================================================== */
/************************************************************************/

class WEBPRasterBand;

class WEBPDataset : public GDALPamDataset
{
    friend class WEBPRasterBand;

    VSILFILE*   fpImage;
    GByte* pabyUncompressed;
    int    bHasBeenUncompressed;
    CPLErr Uncompress();

  public:
                 WEBPDataset();
                 ~WEBPDataset();

    virtual CPLErr      IRasterIO( GDALRWFlag, int, int, int, int,
                                   void *, int, int, GDALDataType,
                                   int, int *, int, int, int );

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset* CreateCopy( const char * pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData );
};

/************************************************************************/
/* ==================================================================== */
/*                            WEBPRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class WEBPRasterBand : public GDALPamRasterBand
{
    friend class WEBPDataset;

  public:

                   WEBPRasterBand( WEBPDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
};

/************************************************************************/
/*                          WEBPRasterBand()                            */
/************************************************************************/

WEBPRasterBand::WEBPRasterBand( WEBPDataset *poDS, int nBand )

{
    this->poDS = poDS;

    eDataType = GDT_Byte;

    nBlockXSize = poDS->nRasterXSize;
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr WEBPRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    WEBPDataset* poGDS = (WEBPDataset*) poDS;

    poGDS->Uncompress();

    int i;
    GByte* pabyUncompressed =
        &poGDS->pabyUncompressed[nBlockYOff * nRasterXSize  * 3 + nBand - 1];
    for(i=0;i<nRasterXSize;i++)
        ((GByte*)pImage)[i] = pabyUncompressed[3 * i];

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp WEBPRasterBand::GetColorInterpretation()

{
    if ( nBand == 1 )
        return GCI_RedBand;

    else if( nBand == 2 )
        return GCI_GreenBand;

    else if ( nBand == 3 )
        return GCI_BlueBand;

    else
        return GCI_AlphaBand;
}

/************************************************************************/
/* ==================================================================== */
/*                             WEBPDataset                               */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            WEBPDataset()                              */
/************************************************************************/

WEBPDataset::WEBPDataset()

{
    fpImage = NULL;
    pabyUncompressed = NULL;
    bHasBeenUncompressed = FALSE;
}

/************************************************************************/
/*                           ~WEBPDataset()                             */
/************************************************************************/

WEBPDataset::~WEBPDataset()

{
    FlushCache();
    if (fpImage)
        VSIFCloseL(fpImage);
    VSIFree(pabyUncompressed);
}

/************************************************************************/
/*                            Uncompress()                              */
/************************************************************************/

CPLErr WEBPDataset::Uncompress()
{
    if (bHasBeenUncompressed)
        return CE_None;
    bHasBeenUncompressed = TRUE;

    VSIFSeekL(fpImage, 0, SEEK_END);
    vsi_l_offset nSize = VSIFTellL(fpImage);
    if (nSize != (vsi_l_offset)(uint32_t)nSize)
        return CE_Failure;
    VSIFSeekL(fpImage, 0, SEEK_SET);
    uint8_t* pabyCompressed = (uint8_t*)VSIMalloc(nSize);
    if (pabyCompressed == NULL)
        return CE_Failure;
    VSIFReadL(pabyCompressed, 1, nSize, fpImage);
    uint8_t* pRet = WebPDecodeRGBInto(pabyCompressed, (uint32_t)nSize,
                        (uint8_t*)pabyUncompressed, nRasterXSize * nRasterYSize * 3,
                        nRasterXSize * 3);
    VSIFree(pabyCompressed);
    if (pRet == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "WebPDecodeRGBInto() failed");
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr WEBPDataset::IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void *pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int *panBandMap,
                              int nPixelSpace, int nLineSpace, int nBandSpace )

{
    if((eRWFlag == GF_Read) &&
       (nBandCount == 3) &&
       (nBands == 3) &&
       (nXOff == 0) && (nXOff == 0) &&
       (nXSize == nBufXSize) && (nXSize == nRasterXSize) &&
       (nYSize == nBufYSize) && (nYSize == nRasterYSize) &&
       (eBufType == GDT_Byte) && 
       (nPixelSpace == 3) &&
       (nLineSpace == (nPixelSpace*nXSize)) &&
       (nBandSpace == 1) &&
       (pData != NULL) &&
       (panBandMap != NULL) &&
       (panBandMap[0] == 1) && (panBandMap[1] == 2) && (panBandMap[2] == 3))
    {
        Uncompress();
        memcpy(pData, pabyUncompressed, 3 * nXSize * nYSize);
        return CE_None;
    }

    return GDALPamDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nBandCount, panBandMap,
                                     nPixelSpace, nLineSpace, nBandSpace);
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int WEBPDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    GByte  *pabyHeader = NULL;
    int    nHeaderBytes = poOpenInfo->nHeaderBytes;

    pabyHeader = poOpenInfo->pabyHeader;

    if( nHeaderBytes < 20 )
        return FALSE;

    return memcmp(pabyHeader, "RIFF", 4) == 0 &&
           memcmp(pabyHeader + 8, "WEBP", 4) == 0 &&
           memcmp(pabyHeader + 12, "VP8 ", 4) == 0;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *WEBPDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) )
        return NULL;

    int nWidth, nHeight;
    if (!WebPGetInfo((const uint8_t*)poOpenInfo->pabyHeader, (uint32_t)poOpenInfo->nHeaderBytes,
                     &nWidth, &nHeight))
        return NULL;

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The WEBP driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the file using the large file api.                         */
/* -------------------------------------------------------------------- */
    VSILFILE* fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb" );

    if( fpImage == NULL )
        return NULL;

    GByte* pabyUncompressed = (GByte*)VSIMalloc3(nWidth, nHeight, 3);
    if (pabyUncompressed == NULL)
    {
        VSIFCloseL(fpImage);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    WEBPDataset  *poDS;

    poDS = new WEBPDataset();
    poDS->nRasterXSize = nWidth;
    poDS->nRasterYSize = nHeight;
    poDS->fpImage = fpImage;
    poDS->pabyUncompressed = pabyUncompressed;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < 3; iBand++ )
        poDS->SetBand( iBand+1, new WEBPRasterBand( poDS, iBand+1 ) );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );

    poDS->TryLoadXML( poOpenInfo->papszSiblingFiles );

/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename, poOpenInfo->papszSiblingFiles );

    return poDS;
}

/************************************************************************/
/*                         WEBPDatasetWriter()                          */
/************************************************************************/

int WEBPDatasetWriter(const uint8_t* data, size_t data_size,
                      const WebPPicture* const picture)
{
    return VSIFWriteL(data, 1, data_size, (VSILFILE*)picture->custom_ptr)
            == data_size;
}

/************************************************************************/
/*                              CreateCopy()                            */
/************************************************************************/

GDALDataset *
WEBPDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                        int bStrict, char ** papszOptions,
                        GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();
    int  nQuality = 80;

/* -------------------------------------------------------------------- */
/*      WEBP library initialization                                     */
/* -------------------------------------------------------------------- */

    WebPPicture sPicture;
    if (!WebPPictureInit(&sPicture))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WebPPictureInit() failed");
        return NULL;
    }

    WebPConfig sConfig;
    if (!WebPConfigInit(&sConfig))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WebPConfigInit() failed");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    if( nBands != 3 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "WEBP driver doesn't support %d bands. Must be 3 (RGB) bands.\n",
                  nBands );

        return NULL;
    }

    GDALDataType eDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();

    if( eDT != GDT_Byte )
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "WEBP driver doesn't support data type %s. "
                  "Only eight bit byte bands supported.\n",
                  GDALGetDataTypeName(
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        if (bStrict)
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      What options has the user selected?                             */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszOptions,"QUALITY") != NULL )
    {
        nQuality = atoi(CSLFetchNameValue(papszOptions,"QUALITY"));
        if( nQuality < 1 || nQuality > 100 )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "QUALITY=%s is not a legal value in the range 1-100.",
                      CSLFetchNameValue(papszOptions,"QUALITY") );
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Allocate memory                                                 */
/* -------------------------------------------------------------------- */
    GByte   *pabyBuffer;

    pabyBuffer = (GByte *) VSIMalloc( 3 * nXSize * nYSize );
    if (pabyBuffer == NULL)
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    VSILFILE    *fpImage;

    fpImage = VSIFOpenL( pszFilename, "wb" );
    if( fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create WEBP file %s.\n",
                  pszFilename );
        VSIFree(pabyBuffer);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      WEBP library settings                                           */
/* -------------------------------------------------------------------- */

    sPicture.colorspace = 0;
    sPicture.width = nXSize;
    sPicture.height = nYSize;
    sPicture.writer = WEBPDatasetWriter;
    sPicture.custom_ptr = fpImage;
    if (!WebPPictureAlloc(&sPicture))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WebPPictureAlloc() failed");
        VSIFree(pabyBuffer);
        VSIFCloseL( fpImage );
        return NULL;
    }

    sConfig.quality = nQuality;

/* -------------------------------------------------------------------- */
/*      Acquire source imagery.                                         */
/* -------------------------------------------------------------------- */
    CPLErr      eErr = CE_None;

    eErr = poSrcDS->RasterIO( GF_Read, 0, 0, nXSize, nYSize,
                              pabyBuffer, nXSize, nYSize, GDT_Byte,
                              3, NULL,
                              3, 3 * nXSize, 1 );

/* -------------------------------------------------------------------- */
/*      Import and write to file                                        */
/* -------------------------------------------------------------------- */
    if (eErr == CE_None &&
        !WebPPictureImportRGB(&sPicture, pabyBuffer, 3 * nXSize))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WebPPictureImportRGB() failed");
        eErr = CE_Failure;
    }

    if (eErr == CE_None && !WebPEncode(&sConfig, &sPicture))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WebPEncode() failed");
        eErr = CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and close.                                              */
/* -------------------------------------------------------------------- */
    CPLFree( pabyBuffer );

    WebPPictureFree(&sPicture);

    VSIFCloseL( fpImage );

    if( eErr != CE_None )
    {
        VSIUnlink( pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */
    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);

    /* If outputing to stdout, we can't reopen it, so we'll return */
    /* a fake dataset to make the caller happy */
    CPLPushErrorHandler(CPLQuietErrorHandler);
    WEBPDataset *poDS = (WEBPDataset*) WEBPDataset::Open( &oOpenInfo );
    CPLPopErrorHandler();
    if( poDS )
    {
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );
        return poDS;
    }

    return NULL;
}

/************************************************************************/
/*                         GDALRegister_WEBP()                          */
/************************************************************************/

void GDALRegister_WEBP()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "WEBP" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "WEBP" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "WEBP" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_webp.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "webp" );
        poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/webp" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                                   "Byte" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>\n"
"   <Option name='QUALITY' type='int' description='good=100, bad=0, default=80'/>\n"
"</CreationOptionList>\n" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnIdentify = WEBPDataset::Identify;
        poDriver->pfnOpen = WEBPDataset::Open;
        poDriver->pfnCreateCopy = WEBPDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
