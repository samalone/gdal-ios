/******************************************************************************
 * $Id: ehdrdataset.cpp 10645 2007-01-18 02:22:39Z warmerdam $
 *
 * Project:  ERMapper .ers Driver
 * Purpose:  Implementation of .ers driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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

#include "rawdataset.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"
#include "ershdrnode.h"

CPL_CVSID("$Id: ehdrdataset.cpp 10645 2007-01-18 02:22:39Z warmerdam $");

/************************************************************************/
/* ==================================================================== */
/*				ERSDataset				*/
/* ==================================================================== */
/************************************************************************/

class ERSRasterBand;

class ERSDataset : public RawDataset
{
    FILE	*fpImage;	// image data file.
    GDALDataset *poDepFile;

    int         bGotTransform;
    double      adfGeoTransform[6];
    char       *pszProjection;

    CPLString   osRawFilename;

    int         bHDRDirty;
    ERSHdrNode *poHeader;

    const char *Find( const char *, const char * );

    int           nGCPCount;
    GDAL_GCP      *pasGCPList;
    char          *pszGCPProjection;

    void          ReadGCPs();
  public:
    		ERSDataset();
	       ~ERSDataset();
    
    virtual void FlushCache(void);
    virtual CPLErr GetGeoTransform( double * padfTransform );
    virtual CPLErr SetGeoTransform( double *padfTransform );
    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );
    virtual char **GetFileList(void);
    
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection );

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
};

/************************************************************************/
/* ==================================================================== */
/*				ERSDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            ERSDataset()                             */
/************************************************************************/

ERSDataset::ERSDataset()
{
    fpImage = NULL;
    poDepFile = NULL;
    pszProjection = CPLStrdup("");
    bGotTransform = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    poHeader = NULL;
    bHDRDirty = FALSE;

    nGCPCount = 0;
    pasGCPList = NULL;
    pszGCPProjection = CPLStrdup("");
}

/************************************************************************/
/*                            ~ERSDataset()                            */
/************************************************************************/

ERSDataset::~ERSDataset()

{
    FlushCache();

    if( fpImage != NULL )
    {
        VSIFCloseL( fpImage );
    }

    if( poDepFile != NULL )
    {
        int iBand;

        for( iBand = 0; iBand < nBands; iBand++ )
            papoBands[iBand] = NULL;

        GDALClose( (GDALDatasetH) poDepFile );
    }

    CPLFree( pszProjection );

    CPLFree( pszGCPProjection );
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    if( poHeader != NULL )
        delete poHeader;
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void ERSDataset::FlushCache()

{
    if( bHDRDirty )
    {
        FILE * fpERS = VSIFOpenL( GetDescription(), "w" );
        if( fpERS == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Unable to rewrite %s header.",
                      GetDescription() );
        }
        else
        {
            VSIFPrintfL( fpERS, "DatasetHeader Begin\n" );
            poHeader->WriteSelf( fpERS, 1 );
            VSIFPrintfL( fpERS, "DatasetHeader End\n" );
            VSIFCloseL( fpERS );
        }
    }

    RawDataset::FlushCache();
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int ERSDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *ERSDataset::GetGCPProjection()

{
    return pszGCPProjection;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *ERSDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                              SetGCPs()                               */
/************************************************************************/

CPLErr ERSDataset::SetGCPs( int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                            const char *pszGCPProjectionIn )

{
/* -------------------------------------------------------------------- */
/*      Clean old gcps.                                                 */
/* -------------------------------------------------------------------- */
    CPLFree( pszGCPProjection );
    pszGCPProjection = NULL;

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );

        pasGCPList = NULL;
        nGCPCount = 0;
    }

/* -------------------------------------------------------------------- */
/*      Copy new ones.                                                  */
/* -------------------------------------------------------------------- */
    nGCPCount = nGCPCountIn;
    pasGCPList = GDALDuplicateGCPs( nGCPCount, pasGCPListIn );
    pszGCPProjection = CPLStrdup( pszGCPProjectionIn );

/* -------------------------------------------------------------------- */
/*      Setup the header contents corresponding to these GCPs.          */
/* -------------------------------------------------------------------- */
    bHDRDirty = TRUE;

    poHeader->Set( "RasterInfo.WarpControl.WarpType", "Polynomial" );
    if( nGCPCount > 6 )
        poHeader->Set( "RasterInfo.WarpControl.WarpOrder", "2" );
    else
        poHeader->Set( "RasterInfo.WarpControl.WarpOrder", "1" );
    poHeader->Set( "RasterInfo.WarpControl.WarpSampling", "Nearest" );

/* -------------------------------------------------------------------- */
/*      Translate the projection.                                       */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS( pszGCPProjection );
    char szERSProj[32], szERSDatum[32], szERSUnits[32];

    oSRS.exportToERM( szERSProj, szERSDatum, szERSUnits );
    
    poHeader->Set( "RasterInfo.WarpControl.CoordinateSpace.Datum", 
                   CPLString().Printf( "\"%s\"", szERSDatum ) );
    poHeader->Set( "RasterInfo.WarpControl.CoordinateSpace.Projection", 
                   CPLString().Printf( "\"%s\"", szERSProj ) );
    poHeader->Set( "RasterInfo.WarpControl.CoordinateSpace.CoordinateType", 
                   CPLString().Printf( "EN" ) );
    poHeader->Set( "RasterInfo.WarpControl.CoordinateSpace.Units", 
                   CPLString().Printf( "\"%s\"", szERSUnits ) );
    poHeader->Set( "RasterInfo.WarpControl.CoordinateSpace.Rotation", 
                   "0:0:0.0" );

/* -------------------------------------------------------------------- */
/*      Translate the GCPs.                                             */
/* -------------------------------------------------------------------- */
    CPLString osControlPoints = "{\n";
    int iGCP;
    
    for( iGCP = 0; iGCP < nGCPCount; iGCP++ )
    {
        CPLString osLine;

        CPLString osId = pasGCPList[iGCP].pszId;
        if( strlen(osId) == 0 )
            osId.Printf( "%d", iGCP + 1 );

        osLine.Printf( "\t\t\t\t\"%s\"\tYes\tYes\t%.6f\t%.6f\t%.15g\t%.15g\t%.15g\n",
                       osId.c_str(),
                       pasGCPList[iGCP].dfGCPPixel,
                       pasGCPList[iGCP].dfGCPLine,
                       pasGCPList[iGCP].dfGCPX,
                       pasGCPList[iGCP].dfGCPY,
                       pasGCPList[iGCP].dfGCPZ );
        osControlPoints += osLine;
    }
    osControlPoints += "\t\t}";
    
    poHeader->Set( "RasterInfo.WarpControl.ControlPoints", osControlPoints );

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *ERSDataset::GetProjectionRef()

{
    // try xml first
    const char* pszPrj = GDALPamDataset::GetProjectionRef();
    if(pszPrj && strlen(pszPrj) > 0)
        return pszPrj;

    return pszProjection;
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr ERSDataset::SetProjection( const char *pszSRS )

{
    if( pszProjection && EQUAL(pszSRS,pszProjection) )
        return CE_None;

    if( pszSRS == NULL )
        pszSRS = "";

    CPLFree( pszProjection );
    pszProjection = CPLStrdup(pszSRS);

    OGRSpatialReference oSRS( pszSRS );
    char szERSProj[32], szERSDatum[32], szERSUnits[32];

    oSRS.exportToERM( szERSProj, szERSDatum, szERSUnits );
    
    bHDRDirty = TRUE;
    poHeader->Set( "CoordinateSpace.Datum", 
                   CPLString().Printf( "\"%s\"", szERSDatum ) );
    poHeader->Set( "CoordinateSpace.Projection", 
                   CPLString().Printf( "\"%s\"", szERSProj ) );
    poHeader->Set( "CoordinateSpace.CoordinateType", 
                   CPLString().Printf( "EN" ) );
    poHeader->Set( "CoordinateSpace.Units", 
                   CPLString().Printf( "\"%s\"", szERSUnits ) );
    poHeader->Set( "CoordinateSpace.Rotation", 
                   "0:0:0.0" );

/* -------------------------------------------------------------------- */
/*      It seems that CoordinateSpace needs to come before              */
/*      RasterInfo.  Try moving it up manually.                         */
/* -------------------------------------------------------------------- */
    int iRasterInfo = -1;
    int iCoordSpace = -1;
    int i;

    for( i = 0; i < poHeader->nItemCount; i++ )
    {
        if( EQUAL(poHeader->papszItemName[i],"RasterInfo") )
            iRasterInfo = i;

        if( EQUAL(poHeader->papszItemName[i],"CoordinateSpace") )
        {
            iCoordSpace = i;
            break;
        }
    }

    if( iCoordSpace > iRasterInfo && iRasterInfo != -1 )
    {
        for( i = iCoordSpace; i > 0 && i != iRasterInfo; i-- )
        {
            char *pszTemp;

            ERSHdrNode *poTemp = poHeader->papoItemChild[i];
            poHeader->papoItemChild[i] = poHeader->papoItemChild[i-1];
            poHeader->papoItemChild[i-1] = poTemp;

            pszTemp = poHeader->papszItemName[i];
            poHeader->papszItemName[i] = poHeader->papszItemName[i-1];
            poHeader->papszItemName[i-1] = pszTemp;

            pszTemp = poHeader->papszItemValue[i];
            poHeader->papszItemValue[i] = poHeader->papszItemValue[i-1];
            poHeader->papszItemValue[i-1] = pszTemp;
        }
    }
    
    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ERSDataset::GetGeoTransform( double * padfTransform )

{
    if( bGotTransform )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
        return CE_None;
    }
    else
    {
        return GDALPamDataset::GetGeoTransform( padfTransform );
    }
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr ERSDataset::SetGeoTransform( double *padfTransform )

{
    if( memcmp( padfTransform, adfGeoTransform, sizeof(double)*6 ) == 0 )
        return CE_None;

    if( adfGeoTransform[2] != 0 || adfGeoTransform[4] != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Rotated and skewed geotransforms not currently supported for ERS driver." );
        return CE_Failure;
    }

    bGotTransform = TRUE;
    memcpy( adfGeoTransform, padfTransform, sizeof(double) * 6 );

    bHDRDirty = TRUE;

    poHeader->Set( "RasterInfo.CellInfo.Xdimension", 
                   CPLString().Printf( "%.15g", fabs(adfGeoTransform[1]) ) );
    poHeader->Set( "RasterInfo.CellInfo.Ydimension", 
                   CPLString().Printf( "%.15g", fabs(adfGeoTransform[5]) ) );
    poHeader->Set( "RasterInfo.RegistrationCoord.Eastings", 
                   CPLString().Printf( "%.15g", adfGeoTransform[0] ) );
    poHeader->Set( "RasterInfo.RegistrationCoord.Northings", 
                   CPLString().Printf( "%.15g", adfGeoTransform[3] ) );
    
    return CE_None;
}

/************************************************************************/
/*                             ERSDMS2Dec()                             */
/*                                                                      */
/*      Convert ERS DMS format to decimal degrees.   Input is like      */
/*      "-180:00:00".                                                   */
/************************************************************************/

static double ERSDMS2Dec( const char *pszDMS )

{
    char **papszTokens = CSLTokenizeStringComplex( pszDMS, ":", FALSE, FALSE );

    if( CSLCount(papszTokens) != 3 )
    {
        return CPLAtof( pszDMS );
    }
    else
    {
        double dfResult = fabs(CPLAtof(papszTokens[0]))
            + CPLAtof(papszTokens[1]) / 60.0
            + CPLAtof(papszTokens[2]) / 3600.0;

        if( CPLAtof(papszTokens[0]) < 0 )
            dfResult *= -1;

        CSLDestroy( papszTokens );
        return dfResult;
    }
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **ERSDataset::GetFileList()

{
    char **papszFileList = NULL;

    // Main data file, etc. 
    papszFileList = GDALPamDataset::GetFileList();

    // Add raw data file if we have one. 
    if( strlen(osRawFilename) > 0 )
        papszFileList = CSLAddString( papszFileList, osRawFilename );

    // If we have a dependent file, merge it's list of files in. 
    if( poDepFile )
    {
        char **papszDepFiles = poDepFile->GetFileList();
        papszFileList = 
            CSLInsertStrings( papszFileList, -1, papszDepFiles );
        CSLDestroy( papszDepFiles );
    }

    return papszFileList;
}

/************************************************************************/
/*                              ReadGCPs()                              */
/*                                                                      */
/*      Read the GCPs from the header.                                  */
/************************************************************************/

void ERSDataset::ReadGCPs()

{
    const char *pszCP = 
        poHeader->Find( "RasterInfo.WarpControl.ControlPoints", NULL );

    if( pszCP == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Parse the control points.  They will look something like:       */
/*                                                                      */
/*   "1035" Yes No 2344.650885 3546.419458 483270.73 3620906.21 3.105   */
/* -------------------------------------------------------------------- */
    char **papszTokens = CSLTokenizeStringComplex( pszCP, "{ \t}", TRUE,FALSE);
    int nItemsPerLine;
    int nItemCount = CSLCount(papszTokens);

/* -------------------------------------------------------------------- */
/*      Work out if we have elevation values or not.                    */
/* -------------------------------------------------------------------- */
    if( nItemCount == 7 )
        nItemsPerLine = 7;
    else if( nItemCount == 8 )
        nItemsPerLine = 8;
    else if( nItemCount < 14 )
    {
        CPLAssert( FALSE );
        return;
    }
    else if( EQUAL(papszTokens[8],"Yes") || EQUAL(papszTokens[8],"No") )
        nItemsPerLine = 7;
    else if( EQUAL(papszTokens[9],"Yes") || EQUAL(papszTokens[9],"No") )
        nItemsPerLine = 8;
    else
    {
        CPLAssert( FALSE );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Setup GCPs.                                                     */
/* -------------------------------------------------------------------- */
    int iGCP;

    CPLAssert( nGCPCount == 0 );

    nGCPCount = nItemCount / nItemsPerLine;
    pasGCPList = (GDAL_GCP *) CPLCalloc(nGCPCount,sizeof(GDAL_GCP));
    GDALInitGCPs( nGCPCount, pasGCPList );

    for( iGCP = 0; iGCP < nGCPCount; iGCP++ )
    {
        GDAL_GCP *psGCP = pasGCPList + iGCP;

        CPLFree( psGCP->pszId );
        psGCP->pszId = CPLStrdup(papszTokens[iGCP*nItemsPerLine+0]);
        psGCP->dfGCPPixel = atof(papszTokens[iGCP*nItemsPerLine+3]);
        psGCP->dfGCPLine  = atof(papszTokens[iGCP*nItemsPerLine+4]);
        psGCP->dfGCPX     = atof(papszTokens[iGCP*nItemsPerLine+5]);
        psGCP->dfGCPY     = atof(papszTokens[iGCP*nItemsPerLine+6]);
        if( nItemsPerLine == 8 )
            psGCP->dfGCPZ = atof(papszTokens[iGCP*nItemsPerLine+7]);
    }
    
    CSLDestroy( papszTokens );
    
/* -------------------------------------------------------------------- */
/*      Parse the GCP projection.                                       */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;

    CPLString osProjection = poHeader->Find( 
        "RasterInfo.WarpControl.CoordinateSpace.Projection", "RAW" );
    CPLString osDatum = poHeader->Find( 
        "RasterInfo.WarpControl.CoordinateSpace.Datum", "WGS84" );
    CPLString osUnits = poHeader->Find( 
        "RasterInfo.WarpControl.CoordinateSpace.Units", "METERS" );

    oSRS.importFromERM( osProjection, osDatum, osUnits );

    CPLFree( pszGCPProjection );
    oSRS.exportToWkt( &pszGCPProjection );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ERSDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      We assume the user selects the .ers file.                       */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes > 15
        && EQUALN((const char *) poOpenInfo->pabyHeader,"Algorithm Begin",15) )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "%s appears to be an algorithm ERS file, which is not currently supported.", 
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      We assume the user selects the .ers file.                       */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 15 
        || !EQUALN((const char *) poOpenInfo->pabyHeader,"DatasetHeader ",14) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Open the .ers file, and read the first line.                    */
/* -------------------------------------------------------------------- */
    FILE *fpERS = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    
    if( fpERS == NULL )
        return NULL;

    CPLReadLineL( fpERS );

/* -------------------------------------------------------------------- */
/*      Now ingest the rest of the file as a tree of header nodes.      */
/* -------------------------------------------------------------------- */
    ERSHdrNode *poHeader = new ERSHdrNode();

    if( !poHeader->ParseChildren( fpERS ) )
    {
        delete poHeader;
        return NULL;
    }

    VSIFCloseL( fpERS );

/* -------------------------------------------------------------------- */
/*      Do we have the minimum required information from this header?   */
/* -------------------------------------------------------------------- */
    if( poHeader->Find( "RasterInfo.NrOfLines" ) == NULL 
        || poHeader->Find( "RasterInfo.NrOfCellsPerLine" ) == NULL 
        || poHeader->Find( "RasterInfo.NrOfBands" ) == NULL )
    {
        if( poHeader->FindNode( "Algorithm" ) != NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "%s appears to be an algorithm ERS file, which is not currently supported.", 
                      poOpenInfo->pszFilename );
        }
        delete poHeader;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ERSDataset     *poDS;

    poDS = new ERSDataset();
    poDS->poHeader = poHeader;
    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    int nBands = atoi(poHeader->Find( "RasterInfo.NrOfBands" ));
    poDS->nRasterXSize = atoi(poHeader->Find( "RasterInfo.NrOfCellsPerLine" ));
    poDS->nRasterYSize = atoi(poHeader->Find( "RasterInfo.NrOfLines" ));

/* -------------------------------------------------------------------- */
/*     Get the HeaderOffset if it exists in the header                  */
/* -------------------------------------------------------------------- */
    GIntBig nHeaderOffset = 0;
    if( poHeader->Find( "HeaderOffset" ) != NULL )
    {
        nHeaderOffset = atoi(poHeader->Find( "HeaderOffset" ));
    }

/* -------------------------------------------------------------------- */
/*      Establish the data type.                                        */
/* -------------------------------------------------------------------- */
    GDALDataType eType;
    CPLString osCellType = poHeader->Find( "RasterInfo.CellType", 
                                           "Unsigned8BitInteger" );
    if( EQUAL(osCellType,"Unsigned8BitInteger") )
        eType = GDT_Byte;
    else if( EQUAL(osCellType,"Signed8BitInteger") )
        eType = GDT_Byte;
    else if( EQUAL(osCellType,"Unsigned16BitInteger") )
        eType = GDT_UInt16;
    else if( EQUAL(osCellType,"Signed16BitInteger") )
        eType = GDT_Int16;
    else if( EQUAL(osCellType,"Unsigned32BitInteger") )
        eType = GDT_UInt32;
    else if( EQUAL(osCellType,"Signed32BitInteger") )
        eType = GDT_Int32;
    else if( EQUAL(osCellType,"IEEE4ByteReal") )
        eType = GDT_Float32;
    else if( EQUAL(osCellType,"IEEE8ByteReal") )
        eType = GDT_Float64;
    else
    {
        CPLDebug( "ERS", "Unknown CellType '%s'", osCellType.c_str() );
        eType = GDT_Byte;
    }

/* -------------------------------------------------------------------- */
/*      Pick up the word order.                                         */
/* -------------------------------------------------------------------- */
    int bNative;

#ifdef CPL_LSB
    bNative = EQUAL(poHeader->Find( "ByteOrder", "LSBFirst" ),
                    "LSBFirst");
#else
    bNative = EQUAL(poHeader->Find( "ByteOrder", "MSBFirst" ),
                    "MSBFirst");
#endif

/* -------------------------------------------------------------------- */
/*      Figure out the name of the target file.                         */
/* -------------------------------------------------------------------- */
    CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
    CPLString osDataFile = poHeader->Find( "DataFile", "" );
    CPLString osDataFilePath;

    if( osDataFile.length() == 0 ) // just strip off extension.
    {
        osDataFile = CPLGetFilename( poOpenInfo->pszFilename );
        osDataFile = osDataFile.substr( 0, osDataFile.find_last_of('.') );
    }
        
    osDataFilePath = CPLFormFilename( osPath, osDataFile, NULL );

/* -------------------------------------------------------------------- */
/*      DataSetType = Translated files are links to things like ecw     */
/*      files.                                                          */
/* -------------------------------------------------------------------- */
    if( EQUAL(poHeader->Find("DataSetType",""),"Translated") )
    {
        poDS->poDepFile = (GDALDataset *) 
            GDALOpenShared( osDataFile, poOpenInfo->eAccess );

        if( poDS->poDepFile != NULL 
            && poDS->poDepFile->GetRasterCount() >= nBands )
        {
            int iBand;

            for( iBand = 0; iBand < nBands; iBand++ )
            {
                // Assume pixel interleaved.
                poDS->SetBand( iBand+1, 
                               poDS->poDepFile->GetRasterBand( iBand+1 ) );
            }
        }
    }

/* ==================================================================== */
/*      While ERStorage indicates a raw file.                           */
/* ==================================================================== */
    else if( EQUAL(poHeader->Find("DataSetType",""),"ERStorage") )
    {
        // Open data file.
        if( poOpenInfo->eAccess == GA_Update )
            poDS->fpImage = VSIFOpenL( osDataFilePath, "r+" );
        else
            poDS->fpImage = VSIFOpenL( osDataFilePath, "r" );

        poDS->osRawFilename = osDataFilePath;

        if( poDS->fpImage != NULL )
        {
            int iWordSize = GDALGetDataTypeSize(eType) / 8;
            int iBand;

            for( iBand = 0; iBand < nBands; iBand++ )
            {
                // Assume pixel interleaved.
                poDS->SetBand( 
                    iBand+1, 
                    new RawRasterBand( poDS, iBand+1, poDS->fpImage,
                                       nHeaderOffset 
                                       + iWordSize * iBand * poDS->nRasterXSize,
                                       iWordSize,
                                       iWordSize * nBands * poDS->nRasterXSize,
                                       eType, bNative, TRUE ));
                if( EQUAL(osCellType,"Signed8BitInteger") )
                    poDS->GetRasterBand(iBand+1)->
                        SetMetadataItem( "PIXELTYPE", "SIGNEDBYTE", 
                                         "IMAGE_STRUCTURE" );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we have an error!                                     */
/* -------------------------------------------------------------------- */
    if( poDS->nBands == 0 )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Look for band descriptions.                                     */
/* -------------------------------------------------------------------- */
    int iChild, iBand = 0;
    ERSHdrNode *poRI = poHeader->FindNode( "RasterInfo" );

    for( iChild = 0; 
         poRI != NULL && iChild < poRI->nItemCount && iBand < poDS->nBands; 
         iChild++ )
    {
        if( poRI->papoItemChild[iChild] != NULL
            && EQUAL(poRI->papszItemName[iChild],"BandId") )
        {
            const char *pszValue = 
                poRI->papoItemChild[iChild]->Find( "Value", NULL );

            iBand++;
            if( pszValue )
            {
                CPLPushErrorHandler( CPLQuietErrorHandler );
                poDS->GetRasterBand( iBand )->SetDescription( pszValue );
                CPLPopErrorHandler();
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Look for projection.                                            */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;

    CPLString osProjection = poHeader->Find( "CoordinateSpace.Projection", 
                                             "RAW" );
    CPLString osDatum = poHeader->Find( "CoordinateSpace.Datum", "WGS84" );
    CPLString osUnits = poHeader->Find( "CoordinateSpace.Units", "METERS" );

    oSRS.importFromERM( osProjection, osDatum, osUnits );

    CPLFree( poDS->pszProjection );
    oSRS.exportToWkt( &(poDS->pszProjection) );

/* -------------------------------------------------------------------- */
/*      Look for the geotransform.                                      */
/* -------------------------------------------------------------------- */
    if( poHeader->Find( "RasterInfo.RegistrationCoord.Eastings", NULL )
        && poHeader->Find( "RasterInfo.CellInfo.Xdimension", NULL ) )
    {
        poDS->bGotTransform = TRUE;
        poDS->adfGeoTransform[0] = CPLAtof( 
            poHeader->Find( "RasterInfo.RegistrationCoord.Eastings", "" ));
        poDS->adfGeoTransform[1] = CPLAtof( 
            poHeader->Find( "RasterInfo.CellInfo.Xdimension", "" ));
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = CPLAtof( 
            poHeader->Find( "RasterInfo.RegistrationCoord.Northings", "" ));
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -CPLAtof( 
            poHeader->Find( "RasterInfo.CellInfo.Ydimension", "" ));
    }
    else if( poHeader->Find( "RasterInfo.RegistrationCoord.Latitude", NULL )
             && poHeader->Find( "RasterInfo.CellInfo.Xdimension", NULL ) )
    {
        poDS->bGotTransform = TRUE;
        poDS->adfGeoTransform[0] = ERSDMS2Dec( 
            poHeader->Find( "RasterInfo.RegistrationCoord.Longitude", "" ));
        poDS->adfGeoTransform[1] = CPLAtof( 
            poHeader->Find( "RasterInfo.CellInfo.Xdimension", "" ));
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = ERSDMS2Dec( 
            poHeader->Find( "RasterInfo.RegistrationCoord.Latitude", "" ));
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -CPLAtof( 
            poHeader->Find( "RasterInfo.CellInfo.Ydimension", "" ));
    }

/* -------------------------------------------------------------------- */
/*      Adjust if we have a registration cell.                          */
/* -------------------------------------------------------------------- */
    int iCellX = atoi(poHeader->Find("RasterInfo.RegistrationCellX", "1"));
    int iCellY = atoi(poHeader->Find("RasterInfo.RegistrationCellY", "1"));

    if( poDS->bGotTransform )
    {
        poDS->adfGeoTransform[0] -=
            (iCellX-1) * poDS->adfGeoTransform[1]
            + (iCellY-1) * poDS->adfGeoTransform[2];
        poDS->adfGeoTransform[3] -= 
            (iCellX-1) * poDS->adfGeoTransform[4]
            + (iCellY-1) * poDS->adfGeoTransform[5];
    }

/* -------------------------------------------------------------------- */
/*      Check for null values.                                          */
/* -------------------------------------------------------------------- */
    if( poHeader->Find( "RasterInfo.NullCellValue", NULL ) )
    {
        CPLPushErrorHandler( CPLQuietErrorHandler );

        for( iBand = 1; iBand <= poDS->nBands; iBand++ )
            poDS->GetRasterBand(iBand)->SetNoDataValue(
                CPLAtofM(poHeader->Find( "RasterInfo.NullCellValue" )) );
        
        CPLPopErrorHandler();
    }

/* -------------------------------------------------------------------- */
/*      Do we have an "All" region?                                     */
/* -------------------------------------------------------------------- */
    ERSHdrNode *poAll = NULL;

    for( iChild = 0; 
         poRI != NULL && iChild < poRI->nItemCount; 
         iChild++ )
    {
        if( poRI->papoItemChild[iChild] != NULL
            && EQUAL(poRI->papszItemName[iChild],"RegionInfo") )
        {
            if( EQUAL(poRI->papoItemChild[iChild]->Find("RegionName",""), 
                      "All") )
                poAll = poRI->papoItemChild[iChild];
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have statistics?                                          */
/* -------------------------------------------------------------------- */
    if( poAll && poAll->FindNode( "Stats" ) )
    {
        CPLPushErrorHandler( CPLQuietErrorHandler );

        for( iBand = 1; iBand <= poDS->nBands; iBand++ )
        {
            const char *pszValue = 
                poAll->FindElem( "Stats.MinimumValue", iBand-1 );

            if( pszValue )
                poDS->GetRasterBand(iBand)->SetMetadataItem(
                    "STATISTICS_MINIMUM", pszValue );

            pszValue = poAll->FindElem( "Stats.MaximumValue", iBand-1 );

            if( pszValue )
                poDS->GetRasterBand(iBand)->SetMetadataItem(
                    "STATISTICS_MAXIMUM", pszValue );

            pszValue = poAll->FindElem( "Stats.MeanValue", iBand-1 );

            if( pszValue )
                poDS->GetRasterBand(iBand)->SetMetadataItem(
                    "STATISTICS_MEAN", pszValue );

            pszValue = poAll->FindElem( "Stats.MedianValue", iBand-1 );

            if( pszValue )
                poDS->GetRasterBand(iBand)->SetMetadataItem(
                    "STATISTICS_MEDIAN", pszValue );
        }
        
        CPLPopErrorHandler();
        
    }

/* -------------------------------------------------------------------- */
/*      Do we have GCPs.                                                */
/* -------------------------------------------------------------------- */
    if( poHeader->FindNode( "RasterInfo.WarpControl" ) )
        poDS->ReadGCPs();

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();
    
    // if no SR in xml, try aux
    const char* pszPrj = poDS->GDALPamDataset::GetProjectionRef();
    if( !pszPrj || strlen(pszPrj) == 0 )
    {
        // try aux
        GDALDataset* poAuxDS = GDALFindAssociatedAuxFile( poOpenInfo->pszFilename, GA_ReadOnly, poDS );
        if( poAuxDS )
        {
            pszPrj = poAuxDS->GetProjectionRef();
            if( pszPrj && strlen(pszPrj) > 0 )
            {
                CPLFree( poDS->pszProjection );
                poDS->pszProjection = CPLStrdup(pszPrj);
            }

            GDALClose( poAuxDS );
        }
    }
/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *ERSDataset::Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char ** papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Verify settings.                                                */
/* -------------------------------------------------------------------- */
    if (nBands <= 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "ERS driver does not support %d bands.\n", nBands);
        return NULL;
    }

    if( eType != GDT_Byte && eType != GDT_Int16 && eType != GDT_UInt16
        && eType != GDT_Int32 && eType != GDT_UInt32
        && eType != GDT_Float32 && eType != GDT_Float64 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The ERS driver does not supporting creating files of types %s.", 
                  GDALGetDataTypeName( eType ) );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Work out the name we want to use for the .ers and binary        */
/*      data files.                                                     */
/* -------------------------------------------------------------------- */
    CPLString osBinFile, osErsFile;

    if( EQUAL(CPLGetExtension( pszFilename ), "ers") )
    {
        osErsFile = pszFilename;
        osBinFile = osErsFile.substr(0,osErsFile.length()-4);
    }
    else
    {
        osBinFile = pszFilename;
        osErsFile = osBinFile + ".ers";
    }

/* -------------------------------------------------------------------- */
/*      Work out some values we will write.                             */
/* -------------------------------------------------------------------- */
    const char *pszCellType = "Unsigned8BitInteger";

    if( eType == GDT_Byte )
        pszCellType = "Unsigned8BitInteger";
    else if( eType == GDT_Int16 )
        pszCellType = "Signed16BitInteger";
    else if( eType == GDT_UInt16 )
        pszCellType = "Unsigned16BitInteger";
    else if( eType == GDT_Int32 )
        pszCellType = "Signed32BitInteger";
    else if( eType == GDT_UInt32 )
        pszCellType = "Unsigned32BitInteger";
    else if( eType == GDT_Float32 )
        pszCellType = "IEEE4ByteReal";
    else if( eType == GDT_Float64 )
        pszCellType = "IEEE8ByteReal";
    else
    {
        CPLAssert( FALSE );
    }

/* -------------------------------------------------------------------- */
/*      Handling for signed eight bit data.                             */
/* -------------------------------------------------------------------- */
    const char *pszPixelType = CSLFetchNameValue( papszOptions, "PIXELTYPE" );
    if( pszPixelType 
        && EQUAL(pszPixelType,"SIGNEDBYTE") 
        && eType == GDT_Byte )
        pszCellType = "Signed8BitInteger";

/* -------------------------------------------------------------------- */
/*      Write binary file.                                              */
/* -------------------------------------------------------------------- */
    GUIntBig nSize;
    GByte byZero = 0;

    FILE *fpBin = VSIFOpenL( osBinFile, "w" );

    if( fpBin == NULL )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Failed to create %s:\n%s", 
                  osBinFile.c_str(), VSIStrerror( errno ) );
        return NULL;
    }

    nSize = nXSize * (GUIntBig) nYSize 
        * nBands * (GDALGetDataTypeSize(eType) / 8);
    if( VSIFSeekL( fpBin, nSize-1, SEEK_SET ) != 0
        || VSIFWriteL( &byZero, 1, 1, fpBin ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Failed to write %s:\n%s", 
                  osBinFile.c_str(), VSIStrerror( errno ) );
        VSIFCloseL( fpBin );
        return NULL;
    }
    VSIFCloseL( fpBin );


/* -------------------------------------------------------------------- */
/*      Try writing header file.                                        */
/* -------------------------------------------------------------------- */
    FILE *fpERS = VSIFOpenL( osErsFile, "w" );
    
    if( fpERS == NULL )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Failed to create %s:\n%s", 
                  osErsFile.c_str(), VSIStrerror( errno ) );
        return NULL;
    }

    VSIFPrintfL( fpERS, "DatasetHeader Begin\n" );
    VSIFPrintfL( fpERS, "\tVersion\t\t = \"6.0\"\n" );
    VSIFPrintfL( fpERS, "\tName\t\t= \"%s\"\n", CPLGetFilename(osErsFile) );

// Last updated requires timezone info which we don't necessarily get
// get from VSICTime() so perhaps it is better to omit this.
//    VSIFPrintfL( fpERS, "\tLastUpdated\t= %s", 
//                 VSICTime( VSITime( NULL ) ) );

    VSIFPrintfL( fpERS, "\tDataSetType\t= ERStorage\n" );
    VSIFPrintfL( fpERS, "\tDataType\t= Raster\n" );
    VSIFPrintfL( fpERS, "\tByteOrder\t= LSBFirst\n" );
    VSIFPrintfL( fpERS, "\tRasterInfo Begin\n" );
    VSIFPrintfL( fpERS, "\t\tCellType\t= %s\n", pszCellType );
    VSIFPrintfL( fpERS, "\t\tNrOfLines\t= %d\n", nYSize );
    VSIFPrintfL( fpERS, "\t\tNrOfCellsPerLine\t= %d\n", nXSize );
    VSIFPrintfL( fpERS, "\t\tNrOfBands\t= %d\n", nBands );
    VSIFPrintfL( fpERS, "\tRasterInfo End\n" );
    if( VSIFPrintfL( fpERS, "DatasetHeader End\n" ) < 17 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Failed to write %s:\n%s", 
                  osErsFile.c_str(), VSIStrerror( errno ) );
        return NULL;
    }

    VSIFCloseL( fpERS );

/* -------------------------------------------------------------------- */
/*      Reopen.                                                         */
/* -------------------------------------------------------------------- */
    return (GDALDataset *) GDALOpen( osErsFile, GA_Update );
}

/************************************************************************/
/*                         GDALRegister_ERS()                           */
/************************************************************************/

void GDALRegister_ERS()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "ERS" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "ERS" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "ERMapper .ers Labelled" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_ers.html" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Int32 UInt32 Float32 Float64" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>"
"   <Option name='PIXELTYPE' type='string' description='By setting this to SIGNEDBYTE, a new Byte file can be forced to be written as signed byte'/>"
"</CreationOptionList>" );

        poDriver->pfnOpen = ERSDataset::Open;
        poDriver->pfnCreate = ERSDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
