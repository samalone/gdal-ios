/******************************************************************************
 * $Id$
 *
 * Project:  PDF driver
 * Purpose:  GDALDataset driver for PDF dataset.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault
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

/* hack for PDF driver and poppler >= 0.15.0 that defines incompatible "typedef bool GBool" */
/* in include/poppler/goo/gtypes.h with the one defined in cpl_port.h */
#define CPL_GBOOL_DEFINED

#include "pdfcreatecopy.h"

#include "cpl_vsi_virtual.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "ogr_spatialref.h"
#include "ogr_geometry.h"

#include "pdfobject.h"

CPL_CVSID("$Id$");

#define PIXEL_TO_GEO_X(x,y) adfGeoTransform[0] + x * adfGeoTransform[1] + y * adfGeoTransform[2]
#define PIXEL_TO_GEO_Y(x,y) adfGeoTransform[3] + x * adfGeoTransform[4] + y * adfGeoTransform[5]

/************************************************************************/
/*                             Init()                                   */
/************************************************************************/

void GDALPDFWriter::Init()
{
    nPageResourceId = 0;
    nCatalogId = nCatalogGen = 0;
    bInWriteObj = FALSE;
    nInfoId = nInfoGen = 0;
    nXMPId = nXMPGen = 0;

    nLastStartXRef = 0;
    nLastXRefSize = 0;
    bCanUpdate = FALSE;
}

/************************************************************************/
/*                         GDALPDFWriter()                              */
/************************************************************************/

GDALPDFWriter::GDALPDFWriter(VSILFILE* fpIn, int bAppend) : fp(fpIn)
{
    Init();

    if (!bAppend)
    {
        VSIFPrintfL(fp, "%%PDF-1.6\n");

        /* See PDF 1.7 reference, page 92. Write 4 non-ASCII bytes to indicate that the content will be binary */
        VSIFPrintfL(fp, "%%%c%c%c%c\n", 0xFF, 0xFF, 0xFF, 0xFF);

        nPageResourceId = AllocNewObject();
        nCatalogId = AllocNewObject();
    }
}

/************************************************************************/
/*                         ~GDALPDFWriter()                             */
/************************************************************************/

GDALPDFWriter::~GDALPDFWriter()
{
    Close();
}

/************************************************************************/
/*                          ParseIndirectRef()                          */
/************************************************************************/

static int ParseIndirectRef(const char* pszStr, int& nNum, int &nGen)
{
    while(*pszStr == ' ')
        pszStr ++;

    nNum = atoi(pszStr);
    while(*pszStr >= '0' && *pszStr <= '9')
        pszStr ++;
    if (*pszStr != ' ')
        return FALSE;

    while(*pszStr == ' ')
        pszStr ++;

    nGen = atoi(pszStr);
    while(*pszStr >= '0' && *pszStr <= '9')
        pszStr ++;
    if (*pszStr != ' ')
        return FALSE;

    while(*pszStr == ' ')
        pszStr ++;

    return *pszStr == 'R';
}

/************************************************************************/
/*                       ParseTrailerAndXRef()                          */
/************************************************************************/

int GDALPDFWriter::ParseTrailerAndXRef()
{
    VSIFSeekL(fp, 0, SEEK_END);
    char szBuf[1024+1];
    vsi_l_offset nOffset = VSIFTellL(fp);

    if (nOffset > 128)
        nOffset -= 128;
    else
        nOffset = 0;

    /* Find startxref section */
    VSIFSeekL(fp, nOffset, SEEK_SET);
    int nRead = VSIFReadL(szBuf, 1, 128, fp);
    szBuf[nRead] = 0;
    if (nRead < 9)
        return FALSE;

    const char* pszStartXRef = NULL;
    int i;
    for(i = nRead - 9; i>= 0; i --)
    {
        if (strncmp(szBuf + i, "startxref", 9) == 0)
        {
            pszStartXRef = szBuf + i;
            break;
        }
    }
    if (pszStartXRef == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find startxref");
        return FALSE;
    }
    pszStartXRef += 9;
    while(*pszStartXRef == '\r' || *pszStartXRef == '\n')
        pszStartXRef ++;
    if (*pszStartXRef == '\0')
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find startxref");
        return FALSE;
    }

    nLastStartXRef = atoi(pszStartXRef);

    /* Skip to beginning of xref section */
    VSIFSeekL(fp, nLastStartXRef, SEEK_SET);

    /* And skip to trailer */
    const char* pszLine;
    while( (pszLine = CPLReadLineL(fp)) != NULL)
    {
        if (strncmp(pszLine, "trailer", 7) == 0)
            break;
    }

    if( pszLine == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find trailer");
        return FALSE;
    }

    /* Read trailer content */
    nRead = VSIFReadL(szBuf, 1, 1024, fp);
    szBuf[nRead] = 0;

    /* Find XRef size */
    const char* pszSize = strstr(szBuf, "/Size");
    if (pszSize == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find trailer /Size");
        return FALSE;
    }
    pszSize += 5;
    while(*pszSize == ' ')
        pszSize ++;
    nLastXRefSize = atoi(pszSize);

    /* Find Root object */
    const char* pszRoot = strstr(szBuf, "/Root");
    if (pszRoot == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find trailer /Root");
        return FALSE;
    }
    pszRoot += 5;
    while(*pszRoot == ' ')
        pszRoot ++;

    if (!ParseIndirectRef(pszRoot, nCatalogId, nCatalogGen))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot parse trailer /Root");
        return FALSE;
    }

    /* Find Info object */
    const char* pszInfo = strstr(szBuf, "/Info");
    if (pszInfo != NULL)
    {
        pszInfo += 5;
        while(*pszInfo == ' ')
            pszInfo ++;

        if (!ParseIndirectRef(pszInfo, nInfoId, nInfoGen))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot parse trailer /Info");
            nInfoId = nInfoGen = 0;
        }
    }

    VSIFSeekL(fp, 0, SEEK_END);

    return TRUE;
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

void GDALPDFWriter::Close()
{
    if (fp)
    {
        CPLAssert(!bInWriteObj);
        if (nPageResourceId)
        {
            WritePages();
            WriteXRefTableAndTrailer();
        }
        else if (bCanUpdate)
        {
            WriteXRefTableAndTrailer();
        }
        VSIFCloseL(fp);
    }
    fp = NULL;
}

/************************************************************************/
/*                           UpdateProj()                               */
/************************************************************************/

void GDALPDFWriter::UpdateProj(GDALDataset* poSrcDS,
                               double dfDPI,
                               GDALPDFDictionaryRW* poPageDict,
                               int nPageNum, int nPageGen)
{
    bCanUpdate = TRUE;
    if ((int)asXRefEntries.size() < nLastXRefSize - 1)
        asXRefEntries.resize(nLastXRefSize - 1);

    int nViewportId = 0;
    int nLGIDictId = 0;

    CPLAssert(nPageNum != 0);
    CPLAssert(poPageDict != NULL);

    PDFMargins sMargins = {0, 0, 0, 0};

    const char* pszGEO_ENCODING = CPLGetConfigOption("GDAL_PDF_GEO_ENCODING", "ISO32000");
    if (EQUAL(pszGEO_ENCODING, "ISO32000") || EQUAL(pszGEO_ENCODING, "BOTH"))
        nViewportId = WriteSRS_ISO32000(poSrcDS, dfDPI / 72.0, NULL, &sMargins);
    if (EQUAL(pszGEO_ENCODING, "OGC_BP") || EQUAL(pszGEO_ENCODING, "BOTH"))
        nLGIDictId = WriteSRS_OGC_BP(poSrcDS, dfDPI / 72.0, NULL, &sMargins);

#ifdef invalidate_xref_entry
    GDALPDFObject* poVP = poPageDict->Get("VP");
    if (poVP)
    {
        if (poVP->GetType() == PDFObjectType_Array &&
            poVP->GetArray()->GetLength() == 1)
            poVP = poVP->GetArray()->Get(0);

        int nVPId = poVP->GetRefNum();
        if (nVPId)
        {
            asXRefEntries[nVPId - 1].bFree = TRUE;
            asXRefEntries[nVPId - 1].nGen ++;
        }
    }
#endif

    poPageDict->Remove("VP");
    poPageDict->Remove("LGIDict");

    if (nViewportId)
    {
        poPageDict->Add("VP", &((new GDALPDFArrayRW())->
                Add(nViewportId, 0)));
    }

    if (nLGIDictId)
    {
        poPageDict->Add("LGIDict", nLGIDictId, 0);
    }

    StartObj(nPageNum, nPageGen);
    VSIFPrintfL(fp, "%s\n", poPageDict->Serialize().c_str());
    EndObj();
}

/************************************************************************/
/*                           UpdateInfo()                               */
/************************************************************************/

void GDALPDFWriter::UpdateInfo(GDALDataset* poSrcDS)
{
    bCanUpdate = TRUE;
    if ((int)asXRefEntries.size() < nLastXRefSize - 1)
        asXRefEntries.resize(nLastXRefSize - 1);

    int nNewInfoId = SetInfo(poSrcDS, NULL);
    /* Write empty info, because podofo driver will find the dangling info instead */
    if (nNewInfoId == 0 && nInfoId != 0)
    {
#ifdef invalidate_xref_entry
        asXRefEntries[nInfoId - 1].bFree = TRUE;
        asXRefEntries[nInfoId - 1].nGen ++;
#else
        StartObj(nInfoId, nInfoGen);
        VSIFPrintfL(fp, "<< >>\n");
        EndObj();
#endif
    }
}

/************************************************************************/
/*                           UpdateXMP()                                */
/************************************************************************/

void GDALPDFWriter::UpdateXMP(GDALDataset* poSrcDS,
                              GDALPDFDictionaryRW* poCatalogDict)
{
    bCanUpdate = TRUE;
    if ((int)asXRefEntries.size() < nLastXRefSize - 1)
        asXRefEntries.resize(nLastXRefSize - 1);

    CPLAssert(nCatalogId != 0);
    CPLAssert(poCatalogDict != NULL);

    GDALPDFObject* poMetadata = poCatalogDict->Get("Metadata");
    if (poMetadata)
    {
        nXMPId = poMetadata->GetRefNum();
        nXMPGen = poMetadata->GetRefGen();
    }

    poCatalogDict->Remove("Metadata");
    int nNewXMPId = SetXMP(poSrcDS, NULL);

    /* Write empty metadata, because podofo driver will find the dangling info instead */
    if (nNewXMPId == 0 && nXMPId != 0)
    {
        StartObj(nXMPId, nXMPGen);
        VSIFPrintfL(fp, "<< >>\n");
        EndObj();
    }

    if (nXMPId)
        poCatalogDict->Add("Metadata", nXMPId, 0);

    StartObj(nCatalogId, nCatalogGen);
    VSIFPrintfL(fp, "%s\n", poCatalogDict->Serialize().c_str());
    EndObj();
}

/************************************************************************/
/*                           AllocNewObject()                           */
/************************************************************************/

int GDALPDFWriter::AllocNewObject()
{
    asXRefEntries.push_back(GDALXRefEntry());
    return (int)asXRefEntries.size();
}

/************************************************************************/
/*                        WriteXRefTableAndTrailer()                    */
/************************************************************************/

void GDALPDFWriter::WriteXRefTableAndTrailer()
{
    vsi_l_offset nOffsetXREF = VSIFTellL(fp);
    VSIFPrintfL(fp, "xref\n");

    char buffer[16];
    if (bCanUpdate)
    {
        VSIFPrintfL(fp, "0 1\n");
        VSIFPrintfL(fp, "0000000000 65535 f \n");
        for(size_t i=0;i<asXRefEntries.size();)
        {
            if (asXRefEntries[i].nOffset != 0 || asXRefEntries[i].bFree)
            {
                /* Find number of consecutive objects */
                size_t nCount = 1;
                while(i + nCount <asXRefEntries.size() &&
                    (asXRefEntries[i + nCount].nOffset != 0 || asXRefEntries[i + nCount].bFree))
                    nCount ++;

                VSIFPrintfL(fp, "%d %d\n", (int)i + 1, (int)nCount);
                size_t iEnd = i + nCount;
                for(; i < iEnd; i++)
                {
                    snprintf (buffer, sizeof(buffer), "%010ld", (long)asXRefEntries[i].nOffset);
                    VSIFPrintfL(fp, "%s %05d %c \n",
                                buffer, asXRefEntries[i].nGen,
                                asXRefEntries[i].bFree ? 'f' : 'n');
                }
            }
            else
            {
                i++;
            }
        }
    }
    else
    {
        VSIFPrintfL(fp, "%d %d\n",
                    0, (int)asXRefEntries.size() + 1);
        VSIFPrintfL(fp, "0000000000 65535 f \n");
        for(size_t i=0;i<asXRefEntries.size();i++)
        {
            snprintf (buffer, sizeof(buffer), "%010ld", (long)asXRefEntries[i].nOffset);
            VSIFPrintfL(fp, "%s %05d n \n", buffer, asXRefEntries[i].nGen);
        }
    }

    VSIFPrintfL(fp, "trailer\n");
    GDALPDFDictionaryRW oDict;
    oDict.Add("Size", (int)asXRefEntries.size() + 1)
         .Add("Root", nCatalogId, nCatalogGen);
    if (nInfoId)
        oDict.Add("Info", nInfoId, nInfoGen);
    if (nLastStartXRef)
        oDict.Add("Prev", nLastStartXRef);
    VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());

    VSIFPrintfL(fp,
                "startxref\n"
                "%ld\n"
                "%%%%EOF\n",
                (long)nOffsetXREF);
}

/************************************************************************/
/*                              StartObj()                              */
/************************************************************************/

void GDALPDFWriter::StartObj(int nObjectId, int nGen)
{
    CPLAssert(!bInWriteObj);
    CPLAssert(nObjectId - 1 < (int)asXRefEntries.size());
    CPLAssert(asXRefEntries[nObjectId - 1].nOffset == 0);
    asXRefEntries[nObjectId - 1].nOffset = VSIFTellL(fp);
    asXRefEntries[nObjectId - 1].nGen = nGen;
    VSIFPrintfL(fp, "%d %d obj\n", nObjectId, nGen);
    bInWriteObj = TRUE;
}

/************************************************************************/
/*                               EndObj()                               */
/************************************************************************/

void GDALPDFWriter::EndObj()
{
    CPLAssert(bInWriteObj);
    VSIFPrintfL(fp, "endobj\n");
    bInWriteObj = FALSE;
}


/************************************************************************/
/*                         GDALPDFFind4Corners()                        */
/************************************************************************/

static
void GDALPDFFind4Corners(const GDAL_GCP* pasGCPList,
                         int& iUL, int& iUR, int& iLR, int& iLL)
{
    double dfMeanX = 0, dfMeanY = 0;
    int i;

    iUL = 0;
    iUR = 0;
    iLR = 0;
    iLL = 0;

    for(i = 0; i < 4; i++ )
    {
        dfMeanX += pasGCPList[i].dfGCPPixel;
        dfMeanY += pasGCPList[i].dfGCPLine;
    }
    dfMeanX /= 4;
    dfMeanY /= 4;

    for(i = 0; i < 4; i++ )
    {
        if (pasGCPList[i].dfGCPPixel < dfMeanX &&
            pasGCPList[i].dfGCPLine  < dfMeanY )
            iUL = i;

        else if (pasGCPList[i].dfGCPPixel > dfMeanX &&
                    pasGCPList[i].dfGCPLine  < dfMeanY )
            iUR = i;

        else if (pasGCPList[i].dfGCPPixel > dfMeanX &&
                    pasGCPList[i].dfGCPLine  > dfMeanY )
            iLR = i;

        else if (pasGCPList[i].dfGCPPixel < dfMeanX &&
                    pasGCPList[i].dfGCPLine  > dfMeanY )
            iLL = i;
    }
}

/************************************************************************/
/*                         WriteSRS_ISO32000()                          */
/************************************************************************/

int  GDALPDFWriter::WriteSRS_ISO32000(GDALDataset* poSrcDS,
                                      double dfUserUnit,
                                      const char* pszNEATLINE,
                                      PDFMargins* psMargins)
{
    int  nWidth = poSrcDS->GetRasterXSize();
    int  nHeight = poSrcDS->GetRasterYSize();
    const char* pszWKT = poSrcDS->GetProjectionRef();
    double adfGeoTransform[6];

    int bHasGT = (poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None);
    const GDAL_GCP* pasGCPList = (poSrcDS->GetGCPCount() == 4) ? poSrcDS->GetGCPs() : NULL;
    if (pasGCPList != NULL)
        pszWKT = poSrcDS->GetGCPProjection();

    if( !bHasGT && pasGCPList == NULL )
        return 0;

    if( pszWKT == NULL || EQUAL(pszWKT, "") )
        return 0;

    double adfGPTS[8];

    double dfULPixel = 0;
    double dfULLine = 0;
    double dfLRPixel = nWidth;
    double dfLRLine = nHeight;

    GDAL_GCP asNeatLineGCPs[4];
    if (pszNEATLINE == NULL)
        pszNEATLINE = poSrcDS->GetMetadataItem("NEATLINE");
    if( bHasGT && pszNEATLINE != NULL && pszNEATLINE[0] != '\0' )
    {
        OGRGeometry* poGeom = NULL;
        OGRGeometryFactory::createFromWkt( (char**)&pszNEATLINE, NULL, &poGeom );
        if ( poGeom != NULL && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon )
        {
            OGRLineString* poLS = ((OGRPolygon*)poGeom)->getExteriorRing();
            double adfGeoTransformInv[6];
            if( poLS != NULL && poLS->getNumPoints() == 5 &&
                GDALInvGeoTransform(adfGeoTransform, adfGeoTransformInv) )
            {
                for(int i=0;i<4;i++)
                {
                    double X = asNeatLineGCPs[i].dfGCPX = poLS->getX(i);
                    double Y = asNeatLineGCPs[i].dfGCPY = poLS->getY(i);
                    double x = adfGeoTransformInv[0] + X * adfGeoTransformInv[1] + Y * adfGeoTransformInv[2];
                    double y = adfGeoTransformInv[3] + X * adfGeoTransformInv[4] + Y * adfGeoTransformInv[5];
                    asNeatLineGCPs[i].dfGCPPixel = x;
                    asNeatLineGCPs[i].dfGCPLine = y;
                }

                int iUL = 0, iUR = 0, iLR = 0, iLL = 0;
                GDALPDFFind4Corners(asNeatLineGCPs,
                                    iUL,iUR, iLR, iLL);

                if (fabs(asNeatLineGCPs[iUL].dfGCPPixel - asNeatLineGCPs[iLL].dfGCPPixel) > .5 ||
                    fabs(asNeatLineGCPs[iUR].dfGCPPixel - asNeatLineGCPs[iLR].dfGCPPixel) > .5 ||
                    fabs(asNeatLineGCPs[iUL].dfGCPLine - asNeatLineGCPs[iUR].dfGCPLine) > .5 ||
                    fabs(asNeatLineGCPs[iLL].dfGCPLine - asNeatLineGCPs[iLR].dfGCPLine) > .5)
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                            "Neatline coordinates should form a rectangle in pixel space. Ignoring it");
                    for(int i=0;i<4;i++)
                    {
                        CPLDebug("PDF", "pixel[%d] = %.1f, line[%d] = %.1f",
                                i, asNeatLineGCPs[i].dfGCPPixel,
                                i, asNeatLineGCPs[i].dfGCPLine);
                    }
                }
                else
                {
                    pasGCPList = asNeatLineGCPs;
                }
            }
        }
        delete poGeom;
    }

    if( pasGCPList )
    {
        int iUL = 0, iUR = 0, iLR = 0, iLL = 0;
        GDALPDFFind4Corners(pasGCPList,
                            iUL,iUR, iLR, iLL);

        if (fabs(pasGCPList[iUL].dfGCPPixel - pasGCPList[iLL].dfGCPPixel) > .5 ||
            fabs(pasGCPList[iUR].dfGCPPixel - pasGCPList[iLR].dfGCPPixel) > .5 ||
            fabs(pasGCPList[iUL].dfGCPLine - pasGCPList[iUR].dfGCPLine) > .5 ||
            fabs(pasGCPList[iLL].dfGCPLine - pasGCPList[iLR].dfGCPLine) > .5)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "GCPs should form a rectangle in pixel space");
            return 0;
        }
        
        dfULPixel = pasGCPList[iUL].dfGCPPixel;
        dfULLine = pasGCPList[iUL].dfGCPLine;
        dfLRPixel = pasGCPList[iLR].dfGCPPixel;
        dfLRLine = pasGCPList[iLR].dfGCPLine;
        
        /* Upper-left */
        adfGPTS[0] = pasGCPList[iUL].dfGCPX;
        adfGPTS[1] = pasGCPList[iUL].dfGCPY;
        
        /* Lower-left */
        adfGPTS[2] = pasGCPList[iLL].dfGCPX;
        adfGPTS[3] = pasGCPList[iLL].dfGCPY;
        
        /* Lower-right */
        adfGPTS[4] = pasGCPList[iLR].dfGCPX;
        adfGPTS[5] = pasGCPList[iLR].dfGCPY;
        
        /* Upper-right */
        adfGPTS[6] = pasGCPList[iUR].dfGCPX;
        adfGPTS[7] = pasGCPList[iUR].dfGCPY;
    }
    else
    {
        /* Upper-left */
        adfGPTS[0] = PIXEL_TO_GEO_X(0, 0);
        adfGPTS[1] = PIXEL_TO_GEO_Y(0, 0);

        /* Lower-left */
        adfGPTS[2] = PIXEL_TO_GEO_X(0, nHeight);
        adfGPTS[3] = PIXEL_TO_GEO_Y(0, nHeight);

        /* Lower-right */
        adfGPTS[4] = PIXEL_TO_GEO_X(nWidth, nHeight);
        adfGPTS[5] = PIXEL_TO_GEO_Y(nWidth, nHeight);

        /* Upper-right */
        adfGPTS[6] = PIXEL_TO_GEO_X(nWidth, 0);
        adfGPTS[7] = PIXEL_TO_GEO_Y(nWidth, 0);
    }
    
    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(pszWKT);
    if( hSRS == NULL )
        return 0;
    OGRSpatialReferenceH hSRSGeog = OSRCloneGeogCS(hSRS);
    if( hSRSGeog == NULL )
    {
        OSRDestroySpatialReference(hSRS);
        return 0;
    }
    OGRCoordinateTransformationH hCT = OCTNewCoordinateTransformation( hSRS, hSRSGeog);
    if( hCT == NULL )
    {
        OSRDestroySpatialReference(hSRS);
        OSRDestroySpatialReference(hSRSGeog);
        return 0;
    }

    int bSuccess = TRUE;
    
    bSuccess &= (OCTTransform( hCT, 1, adfGPTS + 0, adfGPTS + 1, NULL ) == 1);
    bSuccess &= (OCTTransform( hCT, 1, adfGPTS + 2, adfGPTS + 3, NULL ) == 1);
    bSuccess &= (OCTTransform( hCT, 1, adfGPTS + 4, adfGPTS + 5, NULL ) == 1);
    bSuccess &= (OCTTransform( hCT, 1, adfGPTS + 6, adfGPTS + 7, NULL ) == 1);

    if (!bSuccess)
    {
        OSRDestroySpatialReference(hSRS);
        OSRDestroySpatialReference(hSRSGeog);
        OCTDestroyCoordinateTransformation(hCT);
        return 0;
    }

    const char * pszAuthorityCode = OSRGetAuthorityCode( hSRS, NULL );
    const char * pszAuthorityName = OSRGetAuthorityName( hSRS, NULL );
    int nEPSGCode = 0;
    if( pszAuthorityName != NULL && EQUAL(pszAuthorityName, "EPSG") &&
        pszAuthorityCode != NULL )
        nEPSGCode = atoi(pszAuthorityCode);

    int bIsGeographic = OSRIsGeographic(hSRS);

    OSRMorphToESRI(hSRS);
    char* pszESRIWKT = NULL;
    OSRExportToWkt(hSRS, &pszESRIWKT);

    OSRDestroySpatialReference(hSRS);
    OSRDestroySpatialReference(hSRSGeog);
    OCTDestroyCoordinateTransformation(hCT);
    hSRS = NULL;
    hSRSGeog = NULL;
    hCT = NULL;

    if (pszESRIWKT == NULL)
        return 0;

    int nViewportId = AllocNewObject();
    int nMeasureId = AllocNewObject();
    int nGCSId = AllocNewObject();

    StartObj(nViewportId);
    GDALPDFDictionaryRW oViewPortDict;
    oViewPortDict.Add("Type", GDALPDFObjectRW::CreateName("Viewport"))
                 .Add("Name", "Layer")
                 .Add("BBox", &((new GDALPDFArrayRW())
                                ->Add(dfULPixel / dfUserUnit + psMargins->nLeft)
                                .Add((nHeight - dfLRLine) / dfUserUnit + psMargins->nBottom)
                                .Add(dfLRPixel / dfUserUnit + psMargins->nLeft)
                                .Add((nHeight - dfULLine) / dfUserUnit + psMargins->nBottom)))
                 .Add("Measure", nMeasureId, 0);
    VSIFPrintfL(fp, "%s\n", oViewPortDict.Serialize().c_str());
    EndObj();

    StartObj(nMeasureId);
    GDALPDFDictionaryRW oMeasureDict;
    oMeasureDict .Add("Type", GDALPDFObjectRW::CreateName("Measure"))
                 .Add("Subtype", GDALPDFObjectRW::CreateName("GEO"))
                 .Add("Bounds", &((new GDALPDFArrayRW())
                                ->Add(0).Add(1).
                                  Add(0).Add(0).
                                  Add(1).Add(0).
                                  Add(1).Add(1)))
                 .Add("GPTS", &((new GDALPDFArrayRW())
                                ->Add(adfGPTS[1]).Add(adfGPTS[0]).
                                  Add(adfGPTS[3]).Add(adfGPTS[2]).
                                  Add(adfGPTS[5]).Add(adfGPTS[4]).
                                  Add(adfGPTS[7]).Add(adfGPTS[6])))
                 .Add("LPTS", &((new GDALPDFArrayRW())
                                ->Add(0).Add(1).
                                  Add(0).Add(0).
                                  Add(1).Add(0).
                                  Add(1).Add(1)))
                 .Add("GCS", nGCSId, 0);
    VSIFPrintfL(fp, "%s\n", oMeasureDict.Serialize().c_str());
    EndObj();


    StartObj(nGCSId);
    GDALPDFDictionaryRW oGCSDict;
    oGCSDict.Add("Type", GDALPDFObjectRW::CreateName(bIsGeographic ? "GEOGCS" : "PROJCS"))
            .Add("WKT", pszESRIWKT);
    if (nEPSGCode)
        oGCSDict.Add("EPSG", nEPSGCode);
    VSIFPrintfL(fp, "%s\n", oGCSDict.Serialize().c_str());
    EndObj();

    CPLFree(pszESRIWKT);

    return nViewportId;
}

/************************************************************************/
/*                     GDALPDFBuildOGC_BP_Datum()                       */
/************************************************************************/

static GDALPDFObject* GDALPDFBuildOGC_BP_Datum(const OGRSpatialReference* poSRS)
{
    const OGR_SRSNode* poDatumNode = poSRS->GetAttrNode("DATUM");
    const char* pszDatumDescription = NULL;
    if (poDatumNode && poDatumNode->GetChildCount() > 0)
        pszDatumDescription = poDatumNode->GetChild(0)->GetValue();

    GDALPDFObjectRW* poPDFDatum = NULL;

    if (pszDatumDescription)
    {
        double dfSemiMajor = poSRS->GetSemiMajor();
        double dfInvFlattening = poSRS->GetInvFlattening();
        int nEPSGDatum = -1;
        const char *pszAuthority = poSRS->GetAuthorityName( "DATUM" );
        if( pszAuthority != NULL && EQUAL(pszAuthority,"EPSG") )
            nEPSGDatum = atoi(poSRS->GetAuthorityCode( "DATUM" ));

        if( EQUAL(pszDatumDescription,SRS_DN_WGS84) || nEPSGDatum == 6326 )
            poPDFDatum = GDALPDFObjectRW::CreateString("WGE");
        else if( EQUAL(pszDatumDescription, SRS_DN_NAD27) || nEPSGDatum == 6267 )
            poPDFDatum = GDALPDFObjectRW::CreateString("NAS");
        else if( EQUAL(pszDatumDescription, SRS_DN_NAD83) || nEPSGDatum == 6269 )
            poPDFDatum = GDALPDFObjectRW::CreateString("NAR");
        else
        {
            CPLDebug("PDF",
                     "Unhandled datum name (%s). Write datum parameters then.",
                     pszDatumDescription);

            GDALPDFDictionaryRW* poPDFDatumDict = new GDALPDFDictionaryRW();
            poPDFDatum = GDALPDFObjectRW::CreateDictionary(poPDFDatumDict);

            const OGR_SRSNode* poSpheroidNode = poSRS->GetAttrNode("SPHEROID");
            if (poSpheroidNode && poSpheroidNode->GetChildCount() >= 3)
            {
                poPDFDatumDict->Add("Description", pszDatumDescription);

                const char* pszEllipsoidCode = NULL;
#ifdef disabled_because_terrago_toolbar_does_not_like_it
                if( ABS(dfSemiMajor-6378249.145) < 0.01
                    && ABS(dfInvFlattening-293.465) < 0.0001 )
                {
                    pszEllipsoidCode = "CD";     /* Clark 1880 */
                }
                else if( ABS(dfSemiMajor-6378245.0) < 0.01
                         && ABS(dfInvFlattening-298.3) < 0.0001 )
                {
                    pszEllipsoidCode = "KA";      /* Krassovsky */
                }
                else if( ABS(dfSemiMajor-6378388.0) < 0.01
                         && ABS(dfInvFlattening-297.0) < 0.0001 )
                {
                    pszEllipsoidCode = "IN";       /* International 1924 */
                }
                else if( ABS(dfSemiMajor-6378160.0) < 0.01
                         && ABS(dfInvFlattening-298.25) < 0.0001 )
                {
                    pszEllipsoidCode = "AN";    /* Australian */
                }
                else if( ABS(dfSemiMajor-6377397.155) < 0.01
                         && ABS(dfInvFlattening-299.1528128) < 0.0001 )
                {
                    pszEllipsoidCode = "BR";     /* Bessel 1841 */
                }
                else if( ABS(dfSemiMajor-6377483.865) < 0.01
                         && ABS(dfInvFlattening-299.1528128) < 0.0001 )
                {
                    pszEllipsoidCode = "BN";   /* Bessel 1841 (Namibia / Schwarzeck)*/
                }
#if 0
                else if( ABS(dfSemiMajor-6378160.0) < 0.01
                         && ABS(dfInvFlattening-298.247167427) < 0.0001 )
                {
                    pszEllipsoidCode = "GRS67";      /* GRS 1967 */
                }
#endif
                else if( ABS(dfSemiMajor-6378137) < 0.01
                         && ABS(dfInvFlattening-298.257222101) < 0.000001 )
                {
                    pszEllipsoidCode = "RF";      /* GRS 1980 */
                }
                else if( ABS(dfSemiMajor-6378206.4) < 0.01
                         && ABS(dfInvFlattening-294.9786982) < 0.0001 )
                {
                    pszEllipsoidCode = "CC";     /* Clarke 1866 */
                }
                else if( ABS(dfSemiMajor-6377340.189) < 0.01
                         && ABS(dfInvFlattening-299.3249646) < 0.0001 )
                {
                    pszEllipsoidCode = "AM";   /* Modified Airy */
                }
                else if( ABS(dfSemiMajor-6377563.396) < 0.01
                         && ABS(dfInvFlattening-299.3249646) < 0.0001 )
                {
                    pszEllipsoidCode = "AA";       /* Airy */
                }
                else if( ABS(dfSemiMajor-6378200) < 0.01
                         && ABS(dfInvFlattening-298.3) < 0.0001 )
                {
                    pszEllipsoidCode = "HE";    /* Helmert 1906 */
                }
                else if( ABS(dfSemiMajor-6378155) < 0.01
                         && ABS(dfInvFlattening-298.3) < 0.0001 )
                {
                    pszEllipsoidCode = "FA";   /* Modified Fischer 1960 */
                }
#if 0
                else if( ABS(dfSemiMajor-6377298.556) < 0.01
                         && ABS(dfInvFlattening-300.8017) < 0.0001 )
                {
                    pszEllipsoidCode = "evrstSS";    /* Everest (Sabah & Sarawak) */
                }
                else if( ABS(dfSemiMajor-6378165.0) < 0.01
                         && ABS(dfInvFlattening-298.3) < 0.0001 )
                {
                    pszEllipsoidCode = "WGS60";
                }
                else if( ABS(dfSemiMajor-6378145.0) < 0.01
                         && ABS(dfInvFlattening-298.25) < 0.0001 )
                {
                    pszEllipsoidCode = "WGS66";
                }
#endif
                else if( ABS(dfSemiMajor-6378135.0) < 0.01
                         && ABS(dfInvFlattening-298.26) < 0.0001 )
                {
                    pszEllipsoidCode = "WD";
                }
                else if( ABS(dfSemiMajor-6378137.0) < 0.01
                         && ABS(dfInvFlattening-298.257223563) < 0.000001 )
                {
                    pszEllipsoidCode = "WE";
                }
#endif

                if( pszEllipsoidCode != NULL )
                {
                    poPDFDatumDict->Add("Ellipsoid", pszEllipsoidCode);
                }
                else
                {
                    const char* pszEllipsoidDescription =
                        poSpheroidNode->GetChild(0)->GetValue();

                    CPLDebug("PDF",
                         "Unhandled ellipsoid name (%s). Write ellipsoid parameters then.",
                         pszEllipsoidDescription);

                    poPDFDatumDict->Add("Ellipsoid",
                        &((new GDALPDFDictionaryRW())
                        ->Add("Description", pszEllipsoidDescription)
                         .Add("SemiMajorAxis", dfSemiMajor, TRUE)
                         .Add("InvFlattening", dfInvFlattening, TRUE)));
                }

                const OGR_SRSNode *poTOWGS84 = poSRS->GetAttrNode( "TOWGS84" );
                if( poTOWGS84 != NULL
                    && poTOWGS84->GetChildCount() >= 3
                    && (poTOWGS84->GetChildCount() < 7
                    || (EQUAL(poTOWGS84->GetChild(3)->GetValue(),"")
                        && EQUAL(poTOWGS84->GetChild(4)->GetValue(),"")
                        && EQUAL(poTOWGS84->GetChild(5)->GetValue(),"")
                        && EQUAL(poTOWGS84->GetChild(6)->GetValue(),""))) )
                {
                    poPDFDatumDict->Add("ToWGS84",
                        &((new GDALPDFDictionaryRW())
                        ->Add("dx", poTOWGS84->GetChild(0)->GetValue())
                         .Add("dy", poTOWGS84->GetChild(1)->GetValue())
                         .Add("dz", poTOWGS84->GetChild(2)->GetValue())) );
                }
                else if( poTOWGS84 != NULL && poTOWGS84->GetChildCount() >= 7)
                {
                    poPDFDatumDict->Add("ToWGS84",
                        &((new GDALPDFDictionaryRW())
                        ->Add("dx", poTOWGS84->GetChild(0)->GetValue())
                         .Add("dy", poTOWGS84->GetChild(1)->GetValue())
                         .Add("dz", poTOWGS84->GetChild(2)->GetValue())
                         .Add("rx", poTOWGS84->GetChild(3)->GetValue())
                         .Add("ry", poTOWGS84->GetChild(4)->GetValue())
                         .Add("rz", poTOWGS84->GetChild(5)->GetValue())
                         .Add("sf", poTOWGS84->GetChild(6)->GetValue())) );
                }
            }
        }
    }
    else
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "No datum name. Defaulting to WGS84.");
    }

    if (poPDFDatum == NULL)
        poPDFDatum = GDALPDFObjectRW::CreateString("WGE");

    return poPDFDatum;
}

/************************************************************************/
/*                   GDALPDFBuildOGC_BP_Projection()                    */
/************************************************************************/

static GDALPDFDictionaryRW* GDALPDFBuildOGC_BP_Projection(const OGRSpatialReference* poSRS)
{

    const char* pszProjectionOGCBP = "GEOGRAPHIC";
    const char *pszProjection = poSRS->GetAttrValue("PROJECTION");

    GDALPDFDictionaryRW* poProjectionDict = new GDALPDFDictionaryRW();
    poProjectionDict->Add("Type", GDALPDFObjectRW::CreateName("Projection"));
    poProjectionDict->Add("Datum", GDALPDFBuildOGC_BP_Datum(poSRS));

    if( pszProjection == NULL )
    {
        if( poSRS->IsGeographic() )
            pszProjectionOGCBP = "GEOGRAPHIC";
        else if( poSRS->IsLocal() )
            pszProjectionOGCBP = "LOCAL CARTESIAN";
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported, "Unsupported SRS type");
            delete poProjectionDict;
            return NULL;
        }
    }
    else if( EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR) )
    {
        int bNorth;
        int nZone = poSRS->GetUTMZone( &bNorth );

        if( nZone != 0 )
        {
            pszProjectionOGCBP = "UT";
            poProjectionDict->Add("Hemisphere", (bNorth) ? "N" : "S");
            poProjectionDict->Add("Zone", nZone);
        }
        else
        {
            double dfCenterLat = poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,90.L);
            double dfCenterLong = poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
            double dfScale = poSRS->GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0);
            double dfFalseEasting = poSRS->GetNormProjParm(SRS_PP_FALSE_EASTING,0.0);
            double dfFalseNorthing = poSRS->GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0);

            /* OGC_BP supports representing numbers as strings for better precision */
            /* so use it */

            pszProjectionOGCBP = "TC";
            poProjectionDict->Add("OriginLatitude", dfCenterLat, TRUE);
            poProjectionDict->Add("CentralMeridian", dfCenterLong, TRUE);
            poProjectionDict->Add("ScaleFactor", dfScale, TRUE);
            poProjectionDict->Add("FalseEasting", dfFalseEasting, TRUE);
            poProjectionDict->Add("FalseNorthing", dfFalseNorthing, TRUE);
        }
    }
    else if( EQUAL(pszProjection,SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        double dfCenterLat = poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        double dfCenterLong = poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        double dfScale = poSRS->GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0);
        double dfFalseEasting = poSRS->GetNormProjParm(SRS_PP_FALSE_EASTING,0.0);
        double dfFalseNorthing = poSRS->GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0);

        if( fabs(dfCenterLat) == 90.0 && dfCenterLong == 0.0 &&
            dfScale == 0.994 && dfFalseEasting == 200000.0 && dfFalseNorthing == 200000.0)
        {
            pszProjectionOGCBP = "UP";
            poProjectionDict->Add("Hemisphere", (dfCenterLat > 0) ? "N" : "S");
        }
        else
        {
            pszProjectionOGCBP = "PG";
            poProjectionDict->Add("LatitudeTrueScale", dfCenterLat, TRUE);
            poProjectionDict->Add("LongitudeDownFromPole", dfCenterLong, TRUE);
            poProjectionDict->Add("ScaleFactor", dfScale, TRUE);
            poProjectionDict->Add("FalseEasting", dfFalseEasting, TRUE);
            poProjectionDict->Add("FalseNorthing", dfFalseNorthing, TRUE);
        }
    }

    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP))
    {
        double dfStdP1 = poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        double dfStdP2 = poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        double dfCenterLat = poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        double dfCenterLong = poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        double dfFalseEasting = poSRS->GetNormProjParm(SRS_PP_FALSE_EASTING,0.0);
        double dfFalseNorthing = poSRS->GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0);

        pszProjectionOGCBP = "LE";
        poProjectionDict->Add("StandardParallelOne", dfStdP1, TRUE);
        poProjectionDict->Add("StandardParallelTwo", dfStdP2, TRUE);
        poProjectionDict->Add("OriginLatitude", dfCenterLat, TRUE);
        poProjectionDict->Add("CentralMeridian", dfCenterLong, TRUE);
        poProjectionDict->Add("FalseEasting", dfFalseEasting, TRUE);
        poProjectionDict->Add("FalseNorthing", dfFalseNorthing, TRUE);
    }

    else if( EQUAL(pszProjection,SRS_PT_MERCATOR_1SP) )
    {
        double dfCenterLong = poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        double dfCenterLat = poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        double dfScale = poSRS->GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0);
        double dfFalseEasting = poSRS->GetNormProjParm(SRS_PP_FALSE_EASTING,0.0);
        double dfFalseNorthing = poSRS->GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0);

        pszProjectionOGCBP = "MC";
        poProjectionDict->Add("CentralMeridian", dfCenterLong, TRUE);
        poProjectionDict->Add("OriginLatitude", dfCenterLat, TRUE);
        poProjectionDict->Add("ScaleFactor", dfScale, TRUE);
        poProjectionDict->Add("FalseEasting", dfFalseEasting, TRUE);
        poProjectionDict->Add("FalseNorthing", dfFalseNorthing, TRUE);
    }

#ifdef not_supported
    else if( EQUAL(pszProjection,SRS_PT_MERCATOR_2SP) )
    {
        double dfStdP1 = poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        double dfCenterLong = poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        double dfFalseEasting = poSRS->GetNormProjParm(SRS_PP_FALSE_EASTING,0.0);
        double dfFalseNorthing = poSRS->GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0);

        pszProjectionOGCBP = "MC";
        poProjectionDict->Add("StandardParallelOne", dfStdP1, TRUE);
        poProjectionDict->Add("CentralMeridian", dfCenterLong, TRUE);
        poProjectionDict->Add("FalseEasting", dfFalseEasting, TRUE);
        poProjectionDict->Add("FalseNorthing", dfFalseNorthing, TRUE);
    }
#endif

    else
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Unhandled projection type (%s) for now", pszProjection);
    }

    poProjectionDict->Add("ProjectionType", pszProjectionOGCBP);

    if( poSRS->IsProjected() )
    {
        char* pszUnitName = NULL;
        double dfLinearUnits = poSRS->GetLinearUnits(&pszUnitName);
        if (dfLinearUnits == 1.0)
            poProjectionDict->Add("Units", "M");
        else if (dfLinearUnits == 0.3048)
            poProjectionDict->Add("Units", "FT");
    }

    return poProjectionDict;
}

/************************************************************************/
/*                           WriteSRS_OGC_BP()                          */
/************************************************************************/

int GDALPDFWriter::WriteSRS_OGC_BP(GDALDataset* poSrcDS,
                                   double dfUserUnit,
                                   const char* pszNEATLINE,
                                   PDFMargins* psMargins)
{
    int  nWidth = poSrcDS->GetRasterXSize();
    int  nHeight = poSrcDS->GetRasterYSize();
    const char* pszWKT = poSrcDS->GetProjectionRef();
    double adfGeoTransform[6];

    int bHasGT = (poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None);
    int nGCPCount = poSrcDS->GetGCPCount();
    const GDAL_GCP* pasGCPList = (nGCPCount >= 4) ? poSrcDS->GetGCPs() : NULL;
    if (pasGCPList != NULL)
        pszWKT = poSrcDS->GetGCPProjection();

    if( !bHasGT && pasGCPList == NULL )
        return 0;

    if( pszWKT == NULL || EQUAL(pszWKT, "") )
        return 0;
    
    if( !bHasGT )
    {
        if (!GDALGCPsToGeoTransform( nGCPCount, pasGCPList,
                                     adfGeoTransform, FALSE ))
        {
            CPLDebug("PDF", "Could not compute GT with exact match. Writing Registration then");
        }
        else
        {
            bHasGT = TRUE;
        }
    }

    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(pszWKT);
    if( hSRS == NULL )
        return 0;

    const OGRSpatialReference* poSRS = (const OGRSpatialReference*)hSRS;
    GDALPDFDictionaryRW* poProjectionDict = GDALPDFBuildOGC_BP_Projection(poSRS);
    if (poProjectionDict == NULL)
    {
        OSRDestroySpatialReference(hSRS);
        return 0;
    }

    GDALPDFArrayRW* poNeatLineArray = NULL;

    if (pszNEATLINE == NULL)
        pszNEATLINE = poSrcDS->GetMetadataItem("NEATLINE");
    if( bHasGT && pszNEATLINE != NULL && !EQUAL(pszNEATLINE, "NO") && pszNEATLINE[0] != '\0' )
    {
        OGRGeometry* poGeom = NULL;
        OGRGeometryFactory::createFromWkt( (char**)&pszNEATLINE, NULL, &poGeom );
        if ( poGeom != NULL && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon )
        {
            OGRLineString* poLS = ((OGRPolygon*)poGeom)->getExteriorRing();
            double adfGeoTransformInv[6];
            if( poLS != NULL && poLS->getNumPoints() >= 5 &&
                GDALInvGeoTransform(adfGeoTransform, adfGeoTransformInv) )
            {
                poNeatLineArray = new GDALPDFArrayRW();

                 // FIXME : ensure that they are in clockwise order ?
                for(int i=0;i<poLS->getNumPoints() - 1;i++)
                {
                    double X = poLS->getX(i);
                    double Y = poLS->getY(i);
                    double x = adfGeoTransformInv[0] + X * adfGeoTransformInv[1] + Y * adfGeoTransformInv[2];
                    double y = adfGeoTransformInv[3] + X * adfGeoTransformInv[4] + Y * adfGeoTransformInv[5];
                    poNeatLineArray->Add(x / dfUserUnit + psMargins->nLeft, TRUE);
                    poNeatLineArray->Add((nHeight - y) / dfUserUnit + psMargins->nBottom, TRUE);
                }
            }
        }
        delete poGeom;
    }

    if( pszNEATLINE != NULL && EQUAL(pszNEATLINE, "NO") )
    {
        // Do nothing
    }
    else if( pasGCPList && poNeatLineArray == NULL)
    {
        if (nGCPCount == 4)
        {
            int iUL = 0, iUR = 0, iLR = 0, iLL = 0;
            GDALPDFFind4Corners(pasGCPList,
                                iUL,iUR, iLR, iLL);

            double adfNL[8];
            adfNL[0] = pasGCPList[iUL].dfGCPPixel / dfUserUnit + psMargins->nLeft;
            adfNL[1] = (nHeight - pasGCPList[iUL].dfGCPLine) / dfUserUnit + psMargins->nBottom;
            adfNL[2] = pasGCPList[iLL].dfGCPPixel / dfUserUnit + psMargins->nLeft;
            adfNL[3] = (nHeight - pasGCPList[iLL].dfGCPLine) / dfUserUnit + psMargins->nBottom;
            adfNL[4] = pasGCPList[iLR].dfGCPPixel / dfUserUnit + psMargins->nLeft;
            adfNL[5] = (nHeight - pasGCPList[iLR].dfGCPLine) / dfUserUnit + psMargins->nBottom;
            adfNL[6] = pasGCPList[iUR].dfGCPPixel / dfUserUnit + psMargins->nLeft;
            adfNL[7] = (nHeight - pasGCPList[iUR].dfGCPLine) / dfUserUnit + psMargins->nBottom;

            poNeatLineArray = new GDALPDFArrayRW();
            poNeatLineArray->Add(adfNL, 8, TRUE);
        }
        else
        {
            poNeatLineArray = new GDALPDFArrayRW();

            // FIXME : ensure that they are in clockwise order ?
            int i;
            for(i = 0; i < nGCPCount; i++)
            {
                poNeatLineArray->Add(pasGCPList[i].dfGCPPixel / dfUserUnit + psMargins->nLeft, TRUE);
                poNeatLineArray->Add((nHeight - pasGCPList[i].dfGCPLine) / dfUserUnit + psMargins->nBottom, TRUE);
            }
        }
    }
    else if (poNeatLineArray == NULL)
    {
        poNeatLineArray = new GDALPDFArrayRW();

        poNeatLineArray->Add(0 / dfUserUnit + psMargins->nLeft, TRUE);
        poNeatLineArray->Add((nHeight - 0) / dfUserUnit + psMargins->nBottom, TRUE);

        poNeatLineArray->Add(0 / dfUserUnit + psMargins->nLeft, TRUE);
        poNeatLineArray->Add((nHeight -nHeight) / dfUserUnit + psMargins->nBottom, TRUE);

        poNeatLineArray->Add(nWidth / dfUserUnit + psMargins->nLeft, TRUE);
        poNeatLineArray->Add((nHeight -nHeight) / dfUserUnit + psMargins->nBottom, TRUE);

        poNeatLineArray->Add(nWidth / dfUserUnit + psMargins->nLeft, TRUE);
        poNeatLineArray->Add((nHeight - 0) / dfUserUnit + psMargins->nBottom, TRUE);
    }

    int nLGIDictId = AllocNewObject();
    StartObj(nLGIDictId);
    GDALPDFDictionaryRW oLGIDict;
    oLGIDict.Add("Type", GDALPDFObjectRW::CreateName("LGIDict"))
            .Add("Version", "2.1");
    if( bHasGT )
    {
        double adfCTM[6];
        double dfX1 = psMargins->nLeft;
        double dfY2 = nHeight / dfUserUnit + psMargins->nBottom ;

        adfCTM[0] = adfGeoTransform[1] * dfUserUnit;
        adfCTM[1] = adfGeoTransform[2] * dfUserUnit;
        adfCTM[2] = - adfGeoTransform[4] * dfUserUnit;
        adfCTM[3] = - adfGeoTransform[5] * dfUserUnit;
        adfCTM[4] = adfGeoTransform[0] - (adfCTM[0] * dfX1 + adfCTM[2] * dfY2);
        adfCTM[5] = adfGeoTransform[3] - (adfCTM[1] * dfX1 + adfCTM[3] * dfY2);

        oLGIDict.Add("CTM", &((new GDALPDFArrayRW())->Add(adfCTM, 6, TRUE)));
    }
    else
    {
        GDALPDFArrayRW* poRegistrationArray = new GDALPDFArrayRW();
        int i;
        for(i = 0; i < nGCPCount; i++)
        {
            GDALPDFArrayRW* poPTArray = new GDALPDFArrayRW();
            poPTArray->Add(pasGCPList[i].dfGCPPixel / dfUserUnit + psMargins->nLeft, TRUE);
            poPTArray->Add((nHeight - pasGCPList[i].dfGCPLine) / dfUserUnit + psMargins->nBottom, TRUE);
            poPTArray->Add(pasGCPList[i].dfGCPX, TRUE);
            poPTArray->Add(pasGCPList[i].dfGCPY, TRUE);
            poRegistrationArray->Add(poPTArray);
        }
        oLGIDict.Add("Registration", poRegistrationArray);
    }
    if( poNeatLineArray )
    {
        oLGIDict.Add("Neatline", poNeatLineArray);
    }

    const OGR_SRSNode* poNode = poSRS->GetRoot();
    if( poNode != NULL )
        poNode = poNode->GetChild(0);
    const char* pszDescription = NULL;
    if( poNode != NULL )
        pszDescription = poNode->GetValue();
    if( pszDescription )
    {
        oLGIDict.Add("Description", pszDescription);
    }

    oLGIDict.Add("Projection", poProjectionDict);

    /* GDAL extension */
    if( CSLTestBoolean( CPLGetConfigOption("GDAL_PDF_OGC_BP_WRITE_WKT", "TRUE") ) )
        poProjectionDict->Add("WKT", pszWKT);

    VSIFPrintfL(fp, "%s\n", oLGIDict.Serialize().c_str());
    EndObj();

    OSRDestroySpatialReference(hSRS);
    
    return nLGIDictId;
}

/************************************************************************/
/*                     GDALPDFGetValueFromDSOrOption()                  */
/************************************************************************/

static const char* GDALPDFGetValueFromDSOrOption(GDALDataset* poSrcDS,
                                                 char** papszOptions,
                                                 const char* pszKey)
{
    const char* pszValue = CSLFetchNameValue(papszOptions, pszKey);
    if (pszValue == NULL)
        pszValue = poSrcDS->GetMetadataItem(pszKey);
    if (pszValue != NULL && pszValue[0] == '\0')
        return NULL;
    else
        return pszValue;
}

/************************************************************************/
/*                             SetInfo()                                */
/************************************************************************/

int GDALPDFWriter::SetInfo(GDALDataset* poSrcDS,
                           char** papszOptions)
{
    const char* pszAUTHOR = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "AUTHOR");
    const char* pszPRODUCER = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "PRODUCER");
    const char* pszCREATOR = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "CREATOR");
    const char* pszCREATION_DATE = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "CREATION_DATE");
    const char* pszSUBJECT = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "SUBJECT");
    const char* pszTITLE = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "TITLE");
    const char* pszKEYWORDS = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "KEYWORDS");

    if (pszAUTHOR == NULL && pszPRODUCER == NULL && pszCREATOR == NULL && pszCREATION_DATE == NULL &&
        pszSUBJECT == NULL && pszTITLE == NULL && pszKEYWORDS == NULL)
        return 0;

    if (nInfoId == 0)
        nInfoId = AllocNewObject();
    StartObj(nInfoId, nInfoGen);
    GDALPDFDictionaryRW oDict;
    if (pszAUTHOR != NULL)
        oDict.Add("Author", pszAUTHOR);
    if (pszPRODUCER != NULL)
        oDict.Add("Producer", pszPRODUCER);
    if (pszCREATOR != NULL)
        oDict.Add("Creator", pszCREATOR);
    if (pszCREATION_DATE != NULL)
        oDict.Add("CreationDate", pszCREATION_DATE);
    if (pszSUBJECT != NULL)
        oDict.Add("Subject", pszSUBJECT);
    if (pszTITLE != NULL)
        oDict.Add("Title", pszTITLE);
    if (pszKEYWORDS != NULL)
        oDict.Add("Keywords", pszKEYWORDS);
    VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    EndObj();

    return nInfoId;
}

/************************************************************************/
/*                             SetXMP()                                 */
/************************************************************************/

int  GDALPDFWriter::SetXMP(GDALDataset* poSrcDS,
                           const char* pszXMP)
{
    if (pszXMP != NULL && EQUALN(pszXMP, "NO", 2))
        return 0;
    if (pszXMP != NULL && pszXMP[0] == '\0')
        return 0;

    char** papszXMP = poSrcDS->GetMetadata("xml:XMP");
    if (pszXMP == NULL && papszXMP != NULL && papszXMP[0] != NULL)
        pszXMP = papszXMP[0];

    if (pszXMP == NULL)
        return 0;

    CPLXMLNode* psNode = CPLParseXMLString(pszXMP);
    if (psNode == NULL)
        return 0;
    CPLDestroyXMLNode(psNode);

    if(nXMPId == 0)
        nXMPId = AllocNewObject();
    StartObj(nXMPId, nXMPGen);
    GDALPDFDictionaryRW oDict;
    oDict.Add("Type", GDALPDFObjectRW::CreateName("Metadata"))
         .Add("Subtype", GDALPDFObjectRW::CreateName("XML"))
         .Add("Length", (int)strlen(pszXMP));
    VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    VSIFPrintfL(fp, "stream\n");
    VSIFPrintfL(fp, "%s\n", pszXMP);
    VSIFPrintfL(fp, "endstream\n");
    EndObj();
    return nXMPId;
}

/************************************************************************/
/*                              WriteOCG()                              */
/************************************************************************/

int GDALPDFWriter::WriteOCG(const char* pszLayerName)
{
    if (pszLayerName == NULL || pszLayerName[0] == '\0')
        return 0;

    int nLayerId = AllocNewObject();

    asLayersId.push_back(nLayerId);

    StartObj(nLayerId);
    {
        GDALPDFDictionaryRW oDict;
        oDict.Add("Type", GDALPDFObjectRW::CreateName("OCG"));
        oDict.Add("Name", pszLayerName);
        VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    }
    EndObj();

    return nLayerId;
}

/************************************************************************/
/*                              StartPage()                             */
/************************************************************************/

int GDALPDFWriter::StartPage(GDALDataset* poSrcDS,
                             double dfDPI,
                             const char* pszGEO_ENCODING,
                             const char* pszNEATLINE,
                             PDFMargins* psMargins)
{
    int  nWidth = poSrcDS->GetRasterXSize();
    int  nHeight = poSrcDS->GetRasterYSize();
    int  nBands = poSrcDS->GetRasterCount();

    double dfUserUnit = dfDPI / 72.0;
    double dfWidthInUserUnit = nWidth / dfUserUnit + psMargins->nLeft + psMargins->nRight;
    double dfHeightInUserUnit = nHeight / dfUserUnit + psMargins->nBottom + psMargins->nTop;

    int nPageId = AllocNewObject();
    asPageId.push_back(nPageId);

    int nContentId = AllocNewObject();
    int nResourcesId = AllocNewObject();

    int bISO32000 = EQUAL(pszGEO_ENCODING, "ISO32000") ||
                    EQUAL(pszGEO_ENCODING, "BOTH");
    int bOGC_BP   = EQUAL(pszGEO_ENCODING, "OGC_BP") ||
                    EQUAL(pszGEO_ENCODING, "BOTH");

    int nViewportId = 0;
    if( bISO32000 )
        nViewportId = WriteSRS_ISO32000(poSrcDS, dfUserUnit, pszNEATLINE, psMargins);
        
    int nLGIDictId = 0;
    if( bOGC_BP )
        nLGIDictId = WriteSRS_OGC_BP(poSrcDS, dfUserUnit, pszNEATLINE, psMargins);

    StartObj(nPageId);
    GDALPDFDictionaryRW oDictPage;
    oDictPage.Add("Type", GDALPDFObjectRW::CreateName("Page"))
             .Add("Parent", nPageResourceId, 0)
             .Add("MediaBox", &((new GDALPDFArrayRW())
                               ->Add(0).Add(0).Add(dfWidthInUserUnit).Add(dfHeightInUserUnit)))
             .Add("UserUnit", dfUserUnit)
             .Add("Contents", nContentId, 0)
             .Add("Resources", nResourcesId, 0);

    if (nBands == 4)
    {
        oDictPage.Add("Group",
                      &((new GDALPDFDictionaryRW())
                        ->Add("Type", GDALPDFObjectRW::CreateName("Group"))
                         .Add("S", GDALPDFObjectRW::CreateName("Transparency"))
                         .Add("CS", GDALPDFObjectRW::CreateName("DeviceRGB"))));
    }
    if (nViewportId)
    {
        oDictPage.Add("VP", &((new GDALPDFArrayRW())
                               ->Add(nViewportId, 0)));
    }
    if (nLGIDictId)
    {
        oDictPage.Add("LGIDict", nLGIDictId, 0);
    }
    VSIFPrintfL(fp, "%s\n", oDictPage.Serialize().c_str());
    EndObj();

    oPageContext.nContentId = nContentId;
    oPageContext.nResourcesId = nResourcesId;

    return TRUE;
}

/************************************************************************/
/*                             WriteImagery()                           */
/************************************************************************/

int GDALPDFWriter::WriteImagery(GDALDataset* poSrcDS,
                                double dfDPI,
                                PDFMargins* psMargins,
                                PDFCompressMethod eCompressMethod,
                                int nPredictor,
                                int nJPEGQuality,
                                const char* pszJPEG2000_DRIVER,
                                int nBlockXSize, int nBlockYSize,
                                GDALProgressFunc pfnProgress,
                                void * pProgressData)
{
    int  nWidth = poSrcDS->GetRasterXSize();
    int  nHeight = poSrcDS->GetRasterYSize();
    double dfUserUnit = dfDPI / 72.0;

    /* Does the source image has a color table ? */
    GDALColorTable* poCT = poSrcDS->GetRasterBand(1)->GetColorTable();
    int nColorTableId = 0;
    if (poCT != NULL && poCT->GetColorEntryCount() <= 256)
    {
        int nColors = poCT->GetColorEntryCount();
        nColorTableId = AllocNewObject();

        int nLookupTableId = AllocNewObject();

        /* Index object */
        StartObj(nColorTableId);
        {
            GDALPDFArrayRW oArray;
            oArray.Add(GDALPDFObjectRW::CreateName("Indexed"))
                  .Add(&((new GDALPDFArrayRW())->Add(GDALPDFObjectRW::CreateName("DeviceRGB"))))
                  .Add(nColors-1)
                  .Add(nLookupTableId, 0);
            VSIFPrintfL(fp, "%s\n", oArray.Serialize().c_str());
        }
        EndObj();

        /* Lookup table object */
        StartObj(nLookupTableId);
        {
            GDALPDFDictionaryRW oDict;
            oDict.Add("Length", nColors * 3);
            VSIFPrintfL(fp, "%s %% Lookup table\n", oDict.Serialize().c_str());
        }
        VSIFPrintfL(fp, "stream\n");
        GByte pabyLookup[768];
        for(int i=0;i<nColors;i++)
        {
            const GDALColorEntry* poEntry = poCT->GetColorEntry(i);
            pabyLookup[3 * i + 0] = (GByte)poEntry->c1;
            pabyLookup[3 * i + 1] = (GByte)poEntry->c2;
            pabyLookup[3 * i + 2] = (GByte)poEntry->c3;
        }
        VSIFWriteL(pabyLookup, 3 * nColors, 1, fp);
        VSIFPrintfL(fp, "\n");
        VSIFPrintfL(fp, "endstream\n");
        EndObj();
    }
    
    int nXBlocks = (nWidth + nBlockXSize - 1) / nBlockXSize;
    int nYBlocks = (nHeight + nBlockYSize - 1) / nBlockYSize;
    int nBlocks = nXBlocks * nYBlocks;
    int nBlockXOff, nBlockYOff;
    for(nBlockYOff = 0; nBlockYOff < nYBlocks; nBlockYOff ++)
    {
        for(nBlockXOff = 0; nBlockXOff < nXBlocks; nBlockXOff ++)
        {
            int nReqWidth = MIN(nBlockXSize, nWidth - nBlockXOff * nBlockXSize);
            int nReqHeight = MIN(nBlockYSize, nHeight - nBlockYOff * nBlockYSize);
            int iImage = nBlockYOff * nXBlocks + nBlockXOff;

            void* pScaledData = GDALCreateScaledProgress( iImage / (double)nBlocks,
                                                          (iImage + 1) / (double)nBlocks,
                                                          pfnProgress, pProgressData);

            int nImageId = WriteBlock(poSrcDS,
                                      nBlockXOff * nBlockXSize,
                                      nBlockYOff * nBlockYSize,
                                      nReqWidth, nReqHeight,
                                      nColorTableId,
                                      eCompressMethod,
                                      nPredictor,
                                      nJPEGQuality,
                                      pszJPEG2000_DRIVER,
                                      GDALScaledProgress,
                                      pScaledData);

            GDALDestroyScaledProgress(pScaledData);

            if (nImageId == 0)
                return FALSE;

            GDALPDFImageDesc oImageDesc;
            oImageDesc.nImageId = nImageId;
            oImageDesc.dfXOff = (nBlockXOff * nBlockXSize) / dfUserUnit + psMargins->nLeft;
            oImageDesc.dfYOff = (nHeight - nBlockYOff * nBlockYSize - nReqHeight) / dfUserUnit + psMargins->nBottom;
            oImageDesc.dfXSize = nReqWidth / dfUserUnit;
            oImageDesc.dfYSize = nReqHeight / dfUserUnit;

            oPageContext.asImageDesc.push_back(oImageDesc);
        }
    }

    return TRUE;
}

/************************************************************************/
/*                               EndPage()                              */
/************************************************************************/

int GDALPDFWriter::EndPage(const char* pszLayerName,
                           const char* pszExtraContentStream,
                           const char* pszExtraContentLayerName)
{
    int nLayerExtraContentId = WriteOCG(pszExtraContentLayerName);
    int nLayerRasterId = WriteOCG(pszLayerName);

    int bHasTimesRoman = pszExtraContentStream && strstr(pszExtraContentStream, "/FTimesRoman");
    int bHasTimesBold = pszExtraContentStream && strstr(pszExtraContentStream, "/FTimesBold");

    int nContentLengthId = AllocNewObject();

    StartObj(oPageContext.nContentId);
    {
        GDALPDFDictionaryRW oDict;
        oDict.Add("Length", nContentLengthId, 0);
        VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    }

    VSIFPrintfL(fp, "stream\n");
    vsi_l_offset nStreamStart = VSIFTellL(fp);

    if (nLayerRasterId)
        VSIFPrintfL(fp, "/OC /Lyr%d BDC\n", nLayerRasterId);

    for(size_t iImage = 0; iImage < oPageContext.asImageDesc.size(); iImage ++)
    {
        VSIFPrintfL(fp, "q\n");
        GDALPDFObjectRW* poXSize = GDALPDFObjectRW::CreateReal(oPageContext.asImageDesc[iImage].dfXSize);
        GDALPDFObjectRW* poYSize = GDALPDFObjectRW::CreateReal(oPageContext.asImageDesc[iImage].dfYSize);
        GDALPDFObjectRW* poXOff = GDALPDFObjectRW::CreateReal(oPageContext.asImageDesc[iImage].dfXOff);
        GDALPDFObjectRW* poYOff = GDALPDFObjectRW::CreateReal(oPageContext.asImageDesc[iImage].dfYOff);
        VSIFPrintfL(fp, "%s 0 0 %s %s %s cm\n",
                    poXSize->Serialize().c_str(),
                    poYSize->Serialize().c_str(),
                    poXOff->Serialize().c_str(),
                    poYOff->Serialize().c_str());
        delete poXSize;
        delete poYSize;
        delete poXOff;
        delete poYOff;
        VSIFPrintfL(fp, "/Image%d Do\n",
                    oPageContext.asImageDesc[iImage].nImageId);
        VSIFPrintfL(fp, "Q\n");
    }

    if (nLayerRasterId)
        VSIFPrintfL(fp, "EMC\n");

    if (pszExtraContentStream)
    {
        if (nLayerExtraContentId)
            VSIFPrintfL(fp, "/OC /Lyr%d BDC\n", nLayerExtraContentId);
        VSIFPrintfL(fp, "%s\n", pszExtraContentStream);
        if (nLayerExtraContentId)
            VSIFPrintfL(fp, "EMC\n");
    }

    vsi_l_offset nStreamEnd = VSIFTellL(fp);
    VSIFPrintfL(fp, "endstream\n");
    EndObj();

    StartObj(nContentLengthId);
    VSIFPrintfL(fp,
                "   %ld\n",
                (long)(nStreamEnd - nStreamStart));
    EndObj();

    StartObj(oPageContext.nResourcesId);
    {
        GDALPDFDictionaryRW oDict;
        GDALPDFDictionaryRW* poDictXObject = new GDALPDFDictionaryRW();
        oDict.Add("XObject", poDictXObject);
        for(size_t iImage = 0; iImage < oPageContext.asImageDesc.size(); iImage ++)
        {
            poDictXObject->Add(CPLSPrintf("Image%d", oPageContext.asImageDesc[iImage].nImageId),
                               oPageContext.asImageDesc[iImage].nImageId, 0);
        }

        GDALPDFDictionaryRW* poDictFTimesRoman = NULL;
        if (bHasTimesRoman)
        {
            poDictFTimesRoman = new GDALPDFDictionaryRW();
            poDictFTimesRoman->Add("Type", GDALPDFObjectRW::CreateName("Font"));
            poDictFTimesRoman->Add("BaseFont", GDALPDFObjectRW::CreateName("Times-Roman"));
            poDictFTimesRoman->Add("Encoding", GDALPDFObjectRW::CreateName("WinAnsiEncoding"));
            poDictFTimesRoman->Add("Subtype", GDALPDFObjectRW::CreateName("Type1"));
        }

        GDALPDFDictionaryRW* poDictFTimesBold = NULL;
        if (bHasTimesBold)
        {
            poDictFTimesBold = new GDALPDFDictionaryRW();
            poDictFTimesBold->Add("Type", GDALPDFObjectRW::CreateName("Font"));
            poDictFTimesBold->Add("BaseFont", GDALPDFObjectRW::CreateName("Times-Bold"));
            poDictFTimesBold->Add("Encoding", GDALPDFObjectRW::CreateName("WinAnsiEncoding"));
            poDictFTimesBold->Add("Subtype", GDALPDFObjectRW::CreateName("Type1"));
        }

        if (poDictFTimesRoman != NULL || poDictFTimesBold != NULL)
        {
            GDALPDFDictionaryRW* poDictFont = new GDALPDFDictionaryRW();
            if (poDictFTimesRoman)
                poDictFont->Add("FTimesRoman", poDictFTimesRoman);
            if (poDictFTimesBold)
                poDictFont->Add("FTimesBold", poDictFTimesBold);
            oDict.Add("Font", poDictFont);
        }

        if (asLayersId.size())
        {
            GDALPDFDictionaryRW* poDictProperties = new GDALPDFDictionaryRW();
            for(size_t i=0; i<asLayersId.size(); i++)
                poDictProperties->Add(CPLSPrintf("Lyr%d", asLayersId[i]),
                                      asLayersId[i], 0);
            oDict.Add("Properties", poDictProperties);
        }

        VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    }
    EndObj();

    return TRUE;
}

/************************************************************************/
/*                             WriteMask()                              */
/************************************************************************/

int GDALPDFWriter::WriteMask(GDALDataset* poSrcDS,
                             int nXOff, int nYOff, int nReqXSize, int nReqYSize,
                             PDFCompressMethod eCompressMethod)
{
    int nMaskSize = nReqXSize * nReqYSize;
    GByte* pabyMask = (GByte*)VSIMalloc(nMaskSize);
    if (pabyMask == NULL)
        return 0;

    CPLErr eErr;
    eErr = poSrcDS->GetRasterBand(4)->RasterIO(
            GF_Read,
            nXOff, nYOff,
            nReqXSize, nReqYSize,
            pabyMask, nReqXSize, nReqYSize, GDT_Byte,
            0, 0);
    if (eErr != CE_None)
    {
        VSIFree(pabyMask);
        return 0;
    }

    int bOnly0or255 = TRUE;
    int bOnly255 = TRUE;
    int bOnly0 = TRUE;
    int i;
    for(i=0;i<nReqXSize * nReqYSize;i++)
    {
        if (pabyMask[i] == 0)
            bOnly255 = FALSE;
        else if (pabyMask[i] == 255)
            bOnly0 = FALSE;
        else
        {
            bOnly0or255 = FALSE;
            break;
        }
    }

    if (bOnly255)
    {
        CPLFree(pabyMask);
        return 0;
    }

    if (bOnly0or255)
    {
        /* Translate to 1 bit */
        int nReqXSize1 = (nReqXSize + 7) / 8;
        GByte* pabyMask1 = (GByte*)VSICalloc(nReqXSize1, nReqYSize);
        if (pabyMask1 == NULL)
        {
            CPLFree(pabyMask);
            return 0;
        }
        for(int y=0;y<nReqYSize;y++)
        {
            for(int x=0;x<nReqXSize;x++)
            {
                if (pabyMask[y * nReqXSize + x])
                    pabyMask1[y * nReqXSize1 + x / 8] |= 1 << (7 - (x % 8));
            }
        }
        VSIFree(pabyMask);
        pabyMask = pabyMask1;
        nMaskSize = nReqXSize1 * nReqYSize;
    }

    int nMaskId = AllocNewObject();
    int nMaskLengthId = AllocNewObject();

    StartObj(nMaskId);
    GDALPDFDictionaryRW oDict;
    oDict.Add("Length", nMaskLengthId, 0)
         .Add("Type", GDALPDFObjectRW::CreateName("XObject"));
    if( eCompressMethod != COMPRESS_NONE )
    {
        oDict.Add("Filter", GDALPDFObjectRW::CreateName("FlateDecode"));
    }
    oDict.Add("Subtype", GDALPDFObjectRW::CreateName("Image"))
         .Add("Width", nReqXSize)
         .Add("Height", nReqYSize)
         .Add("ColorSpace", GDALPDFObjectRW::CreateName("DeviceGray"))
         .Add("BitsPerComponent", (bOnly0or255) ? 1 : 8);
    VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    VSIFPrintfL(fp, "stream\n");
    vsi_l_offset nStreamStart = VSIFTellL(fp);

    VSILFILE* fpGZip = NULL;
    VSILFILE* fpBack = fp;
    if( eCompressMethod != COMPRESS_NONE )
    {
        fpGZip = (VSILFILE* )VSICreateGZipWritable( (VSIVirtualHandle*) fp, TRUE, FALSE );
        fp = fpGZip;
    }

    VSIFWriteL(pabyMask, nMaskSize, 1, fp);
    CPLFree(pabyMask);

    if (fpGZip)
        VSIFCloseL(fpGZip);

    fp = fpBack;

    vsi_l_offset nStreamEnd = VSIFTellL(fp);
    VSIFPrintfL(fp,
                "\n"
                "endstream\n");
    EndObj();

    StartObj(nMaskLengthId);
    VSIFPrintfL(fp,
                "   %ld\n",
                (long)(nStreamEnd - nStreamStart));
    EndObj();

    return nMaskId;
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

int GDALPDFWriter::WriteBlock(GDALDataset* poSrcDS,
                             int nXOff, int nYOff, int nReqXSize, int nReqYSize,
                             int nColorTableId,
                             PDFCompressMethod eCompressMethod,
                             int nPredictor,
                             int nJPEGQuality,
                             const char* pszJPEG2000_DRIVER,
                             GDALProgressFunc pfnProgress,
                             void * pProgressData)
{
    int  nBands = poSrcDS->GetRasterCount();

    CPLErr eErr = CE_None;
    GDALDataset* poBlockSrcDS = NULL;
    GDALDatasetH hMemDS = NULL;
    GByte* pabyMEMDSBuffer = NULL;

    int nMaskId = 0;
    if (nBands == 4)
    {
        nMaskId = WriteMask(poSrcDS,
                            nXOff, nYOff, nReqXSize, nReqYSize,
                            eCompressMethod);
    }

    if( nReqXSize == poSrcDS->GetRasterXSize() &&
        nReqYSize == poSrcDS->GetRasterYSize() &&
        nBands != 4)
    {
        poBlockSrcDS = poSrcDS;
    }
    else
    {
        if (nBands == 4)
            nBands = 3;

        GDALDriverH hMemDriver = GDALGetDriverByName("MEM");
        if( hMemDriver == NULL )
            return 0;

        hMemDS = GDALCreate(hMemDriver, "MEM:::",
                            nReqXSize, nReqYSize, 0,
                            GDT_Byte, NULL);
        if (hMemDS == NULL)
            return 0;

        pabyMEMDSBuffer =
            (GByte*)VSIMalloc3(nReqXSize, nReqYSize, nBands);
        if (pabyMEMDSBuffer == NULL)
        {
            GDALClose(hMemDS);
            return 0;
        }

        eErr = poSrcDS->RasterIO(GF_Read,
                                nXOff, nYOff,
                                nReqXSize, nReqYSize,
                                pabyMEMDSBuffer, nReqXSize, nReqYSize,
                                GDT_Byte, nBands, NULL,
                                0, 0, 0);

        if( eErr != CE_None )
        {
            CPLFree(pabyMEMDSBuffer);
            GDALClose(hMemDS);
            return 0;
        }

        int iBand;
        for(iBand = 0; iBand < nBands; iBand ++)
        {
            char** papszMEMDSOptions = NULL;
            char szTmp[64];
            memset(szTmp, 0, sizeof(szTmp));
            CPLPrintPointer(szTmp,
                            pabyMEMDSBuffer + iBand * nReqXSize * nReqYSize, sizeof(szTmp));
            papszMEMDSOptions = CSLSetNameValue(papszMEMDSOptions, "DATAPOINTER", szTmp);
            GDALAddBand(hMemDS, GDT_Byte, papszMEMDSOptions);
            CSLDestroy(papszMEMDSOptions);
        }

        poBlockSrcDS = (GDALDataset*) hMemDS;
    }

    int nImageId = AllocNewObject();
    int nImageLengthId = AllocNewObject();

    StartObj(nImageId);

    GDALPDFDictionaryRW oDict;
    oDict.Add("Length", nImageLengthId, 0)
         .Add("Type", GDALPDFObjectRW::CreateName("XObject"));

    if( eCompressMethod == COMPRESS_DEFLATE )
    {
        oDict.Add("Filter", GDALPDFObjectRW::CreateName("FlateDecode"));
        if( nPredictor == 2 )
            oDict.Add("Filter", &((new GDALPDFDictionaryRW())
                                  ->Add("Predictor", 2)
                                   .Add("Colors", nBands)
                                   .Add("Columns", nReqXSize)));
    }
    else if( eCompressMethod == COMPRESS_JPEG )
    {
        oDict.Add("Filter", GDALPDFObjectRW::CreateName("DCTDecode"));
    }
    else if( eCompressMethod == COMPRESS_JPEG2000 )
    {
        oDict.Add("Filter", GDALPDFObjectRW::CreateName("JPXDecode"));
    }

    oDict.Add("Subtype", GDALPDFObjectRW::CreateName("Image"))
         .Add("Width", nReqXSize)
         .Add("Height", nReqYSize)
         .Add("ColorSpace",
              (nColorTableId != 0) ? GDALPDFObjectRW::CreateIndirect(nColorTableId, 0) :
              (nBands == 1) ?        GDALPDFObjectRW::CreateName("DeviceGray") :
                                     GDALPDFObjectRW::CreateName("DeviceRGB"))
         .Add("BitsPerComponent", 8);
    if( nMaskId )
    {
        oDict.Add("SMask", nMaskId, 0);
    }
    VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    VSIFPrintfL(fp, "stream\n");

    vsi_l_offset nStreamStart = VSIFTellL(fp);

    if( eCompressMethod == COMPRESS_JPEG ||
        eCompressMethod == COMPRESS_JPEG2000 )
    {
        GDALDriver* poJPEGDriver = NULL;
        char szTmp[64];
        char** papszOptions = NULL;

        if( eCompressMethod == COMPRESS_JPEG )
        {
            poJPEGDriver = (GDALDriver*) GDALGetDriverByName("JPEG");
            if (poJPEGDriver != NULL && nJPEGQuality > 0)
                papszOptions = CSLAddString(papszOptions, CPLSPrintf("QUALITY=%d", nJPEGQuality));
            sprintf(szTmp, "/vsimem/pdftemp/%p.jpg", this);
        }
        else
        {
            if (pszJPEG2000_DRIVER == NULL || EQUAL(pszJPEG2000_DRIVER, "JP2KAK"))
                poJPEGDriver = (GDALDriver*) GDALGetDriverByName("JP2KAK");
            if (poJPEGDriver == NULL)
            {
                if (pszJPEG2000_DRIVER == NULL || EQUAL(pszJPEG2000_DRIVER, "JP2ECW"))
                    poJPEGDriver = (GDALDriver*) GDALGetDriverByName("JP2ECW");
                if (poJPEGDriver)
                {
                    papszOptions = CSLAddString(papszOptions, "PROFILE=NPJE");
                    papszOptions = CSLAddString(papszOptions, "LAYERS=1");
                    papszOptions = CSLAddString(papszOptions, "GeoJP2=OFF");
                    papszOptions = CSLAddString(papszOptions, "GMLJP2=OFF");
                }
            }
            if (poJPEGDriver == NULL)
            {
                if (pszJPEG2000_DRIVER == NULL || EQUAL(pszJPEG2000_DRIVER, "JP2OpenJPEG"))
                    poJPEGDriver = (GDALDriver*) GDALGetDriverByName("JP2OpenJPEG");
            }
            if (poJPEGDriver == NULL)
            {
                if (pszJPEG2000_DRIVER == NULL || EQUAL(pszJPEG2000_DRIVER, "JPEG2000"))
                    poJPEGDriver = (GDALDriver*) GDALGetDriverByName("JPEG2000");
            }
            sprintf(szTmp, "/vsimem/pdftemp/%p.jp2", this);
        }

        if( poJPEGDriver == NULL )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "No %s driver found",
                     ( eCompressMethod == COMPRESS_JPEG ) ? "JPEG" : "JPEG2000");
            eErr = CE_Failure;
            goto end;
        }

        GDALDataset* poJPEGDS = NULL;

        poJPEGDS = poJPEGDriver->CreateCopy(szTmp, poBlockSrcDS,
                                            FALSE, papszOptions,
                                            pfnProgress, pProgressData);

        CSLDestroy(papszOptions);
        if( poJPEGDS == NULL )
        {
            eErr = CE_Failure;
            goto end;
        }

        GDALClose(poJPEGDS);

        vsi_l_offset nJPEGDataSize = 0;
        GByte* pabyJPEGData = VSIGetMemFileBuffer(szTmp, &nJPEGDataSize, TRUE);
        VSIFWriteL(pabyJPEGData, nJPEGDataSize, 1, fp);
        CPLFree(pabyJPEGData);
    }
    else
    {
        VSILFILE* fpGZip = NULL;
        VSILFILE* fpBack = fp;
        if( eCompressMethod == COMPRESS_DEFLATE )
        {
            fpGZip = (VSILFILE* )VSICreateGZipWritable( (VSIVirtualHandle*) fp, TRUE, FALSE );
            fp = fpGZip;
        }

        GByte* pabyLine = (GByte*)CPLMalloc(nReqXSize * nBands);
        for(int iLine = 0; iLine < nReqYSize; iLine ++)
        {
            /* Get pixel interleaved data */
            eErr = poBlockSrcDS->RasterIO(GF_Read,
                                          0, iLine, nReqXSize, 1,
                                          pabyLine, nReqXSize, 1, GDT_Byte,
                                          nBands, NULL, nBands, 0, 1);
            if( eErr != CE_None )
                break;

            /* Apply predictor if needed */
            if( nPredictor == 2 )
            {
                if( nBands == 1 )
                {
                    int nPrevValue = pabyLine[0];
                    for(int iPixel = 1; iPixel < nReqXSize; iPixel ++)
                    {
                        int nCurValue = pabyLine[iPixel];
                        pabyLine[iPixel] = (GByte) (nCurValue - nPrevValue);
                        nPrevValue = nCurValue;
                    }
                }
                else if( nBands == 3 )
                {
                    int nPrevValueR = pabyLine[0];
                    int nPrevValueG = pabyLine[1];
                    int nPrevValueB = pabyLine[2];
                    for(int iPixel = 1; iPixel < nReqXSize; iPixel ++)
                    {
                        int nCurValueR = pabyLine[3 * iPixel + 0];
                        int nCurValueG = pabyLine[3 * iPixel + 1];
                        int nCurValueB = pabyLine[3 * iPixel + 2];
                        pabyLine[3 * iPixel + 0] = (GByte) (nCurValueR - nPrevValueR);
                        pabyLine[3 * iPixel + 1] = (GByte) (nCurValueG - nPrevValueG);
                        pabyLine[3 * iPixel + 2] = (GByte) (nCurValueB - nPrevValueB);
                        nPrevValueR = nCurValueR;
                        nPrevValueG = nCurValueG;
                        nPrevValueB = nCurValueB;
                    }
                }
            }

            if( VSIFWriteL(pabyLine, nReqXSize * nBands, 1, fp) != 1 )
            {
                eErr = CE_Failure;
                break;
            }

            if( eErr == CE_None
                && !pfnProgress( (iLine+1) / (double)nReqYSize,
                                NULL, pProgressData ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt,
                        "User terminated CreateCopy()" );
                eErr = CE_Failure;
                break;
            }
        }

        CPLFree(pabyLine);

        if (fpGZip)
            VSIFCloseL(fpGZip);

        fp = fpBack;
    }

end:
    CPLFree(pabyMEMDSBuffer);
    pabyMEMDSBuffer = NULL;
    if( hMemDS != NULL )
    {
        GDALClose(hMemDS);
        hMemDS = NULL;
    }

    vsi_l_offset nStreamEnd = VSIFTellL(fp);
    VSIFPrintfL(fp,
                "\n"
                "endstream\n");
    EndObj();

    StartObj(nImageLengthId);
    VSIFPrintfL(fp,
                "   %ld\n",
                (long)(nStreamEnd - nStreamStart));
    EndObj();

    return eErr == CE_None ? nImageId : 0;
}

/************************************************************************/
/*                              WritePages()                            */
/************************************************************************/

void GDALPDFWriter::WritePages()
{
    StartObj(nPageResourceId);
    {
        GDALPDFDictionaryRW oDict;
        GDALPDFArrayRW* poKids = new GDALPDFArrayRW();
        oDict.Add("Type", GDALPDFObjectRW::CreateName("Pages"))
             .Add("Count", (int)asPageId.size())
             .Add("Kids", poKids);

        for(size_t i=0;i<asPageId.size();i++)
            poKids->Add(asPageId[i], 0);

        VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    }
    EndObj();

    StartObj(nCatalogId);
    {
        GDALPDFDictionaryRW oDict;
        oDict.Add("Type", GDALPDFObjectRW::CreateName("Catalog"))
             .Add("Pages", nPageResourceId, 0);
        if (nXMPId)
            oDict.Add("Metadata", nXMPId, 0);
        if (asLayersId.size())
        {
            GDALPDFDictionaryRW* poDictOCProperties = new GDALPDFDictionaryRW();
            oDict.Add("OCProperties", poDictOCProperties);

            GDALPDFDictionaryRW* poDictD = new GDALPDFDictionaryRW();
            poDictOCProperties->Add("D", poDictD);

            GDALPDFArrayRW* poArrayOrder = new GDALPDFArrayRW();
            size_t i;
            for(i=0;i<asLayersId.size();i++)
                poArrayOrder->Add(asLayersId[i], 0);
            poDictD->Add("Order", poArrayOrder);


            GDALPDFArrayRW* poArrayOGCs = new GDALPDFArrayRW();
            for(i=0;i<asLayersId.size();i++)
                poArrayOGCs->Add(asLayersId[i], 0);
            poDictOCProperties->Add("OCGs", poArrayOGCs);
        }
        VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    }
    EndObj();
}

/************************************************************************/
/*                        GDALPDFGetJPEGQuality()                       */
/************************************************************************/

static int GDALPDFGetJPEGQuality(char** papszOptions)
{
    int nJpegQuality = -1;
    const char* pszValue = CSLFetchNameValue( papszOptions, "JPEG_QUALITY" );
    if( pszValue  != NULL )
    {
        nJpegQuality = atoi( pszValue );
        if (!(nJpegQuality >= 1 && nJpegQuality <= 100))
        {
            CPLError( CE_Warning, CPLE_IllegalArg,
                    "JPEG_QUALITY=%s value not recognised, ignoring.",
                    pszValue );
            nJpegQuality = -1;
        }
    }
    return nJpegQuality;
}

/************************************************************************/
/*                          GDALPDFCreateCopy()                         */
/************************************************************************/

GDALDataset *GDALPDFCreateCopy( const char * pszFilename,
                                GDALDataset *poSrcDS,
                                int bStrict,
                                char **papszOptions,
                                GDALProgressFunc pfnProgress,
                                void * pProgressData )
{
    int  nBands = poSrcDS->GetRasterCount();
    int  nWidth = poSrcDS->GetRasterXSize();
    int  nHeight = poSrcDS->GetRasterYSize();

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    if( nBands != 1 && nBands != 3 && nBands != 4 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "PDF driver doesn't support %d bands.  Must be 1 (grey or with color table), "
                  "3 (RGB) or 4 bands.\n", nBands );

        return NULL;
    }

    GDALDataType eDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    if( eDT != GDT_Byte )
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "PDF driver doesn't support data type %s. "
                  "Only eight bit byte bands supported.\n",
                  GDALGetDataTypeName(
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        if (bStrict)
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*     Read options.                                                    */
/* -------------------------------------------------------------------- */
    PDFCompressMethod eCompressMethod = COMPRESS_DEFLATE;
    const char* pszCompressMethod = CSLFetchNameValue(papszOptions, "COMPRESS");
    if (pszCompressMethod)
    {
        if( EQUAL(pszCompressMethod, "NONE") )
            eCompressMethod = COMPRESS_NONE;
        else if( EQUAL(pszCompressMethod, "DEFLATE") )
            eCompressMethod = COMPRESS_DEFLATE;
        else if( EQUAL(pszCompressMethod, "JPEG") )
            eCompressMethod = COMPRESS_JPEG;
        else if( EQUAL(pszCompressMethod, "JPEG2000") )
            eCompressMethod = COMPRESS_JPEG2000;
        else
        {
            CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                    "Unsupported value for COMPRESS.");

            if (bStrict)
                return NULL;
        }
    }

    if (nBands == 1 &&
        poSrcDS->GetRasterBand(1)->GetColorTable() != NULL &&
        (eCompressMethod == COMPRESS_JPEG || eCompressMethod == COMPRESS_JPEG2000))
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "The source raster band has a color table, which is not appropriate with JPEG or JPEG2000 compression.\n"
                  "You should rather consider using color table expansion (-expand option in gdal_translate)");
    }


    int nBlockXSize = nWidth;
    int nBlockYSize = nHeight;
    const char* pszValue;

    int bTiled = CSLFetchBoolean( papszOptions, "TILED", FALSE );
    if( bTiled )
        nBlockXSize = nBlockYSize = 256;

    pszValue = CSLFetchNameValue(papszOptions, "BLOCKXSIZE");
    if( pszValue != NULL )
    {
        nBlockXSize = atoi( pszValue );
        if (nBlockXSize < 0 || nBlockXSize >= nWidth)
            nBlockXSize = nWidth;
    }

    pszValue = CSLFetchNameValue(papszOptions, "BLOCKYSIZE");
    if( pszValue != NULL )
    {
        nBlockYSize = atoi( pszValue );
        if (nBlockYSize < 0 || nBlockYSize >= nHeight)
            nBlockYSize = nHeight;
    }

    int nJPEGQuality = GDALPDFGetJPEGQuality(papszOptions);

    const char* pszJPEG2000_DRIVER = CSLFetchNameValue(papszOptions, "JPEG2000_DRIVER");
    
    const char* pszGEO_ENCODING =
        CSLFetchNameValueDef(papszOptions, "GEO_ENCODING", "ISO32000");

    const char* pszXMP = CSLFetchNameValue(papszOptions, "XMP");

    double dfDPI = atof(CSLFetchNameValueDef(papszOptions, "DPI", "72"));
    if (dfDPI < 72.0)
        dfDPI = 72.0;

    const char* pszPredictor = CSLFetchNameValue(papszOptions, "PREDICTOR");
    int nPredictor = 1;
    if (pszPredictor)
    {
        if (eCompressMethod != COMPRESS_DEFLATE)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "PREDICTOR option is only taken into account for DEFLATE compression");
        }
        else
        {
            nPredictor = atoi(pszPredictor);
            if (nPredictor != 1 && nPredictor != 2)
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                                    "Supported PREDICTOR values are 1 or 2");
                nPredictor = 1;
            }
        }
    }

    const char* pszNEATLINE = CSLFetchNameValue(papszOptions, "NEATLINE");

    int nMargin = atoi(CSLFetchNameValueDef(papszOptions, "MARGIN", "0"));

    PDFMargins sMargins;
    sMargins.nLeft = nMargin;
    sMargins.nRight = nMargin;
    sMargins.nTop = nMargin;
    sMargins.nBottom = nMargin;

    const char* pszLeftMargin = CSLFetchNameValue(papszOptions, "LEFT_MARGIN");
    if (pszLeftMargin) sMargins.nLeft = atoi(pszLeftMargin);

    const char* pszRightMargin = CSLFetchNameValue(papszOptions, "RIGHT_MARGIN");
    if (pszRightMargin) sMargins.nRight = atoi(pszRightMargin);

    const char* pszTopMargin = CSLFetchNameValue(papszOptions, "TOP_MARGIN");
    if (pszTopMargin) sMargins.nTop = atoi(pszTopMargin);

    const char* pszBottomMargin = CSLFetchNameValue(papszOptions, "BOTTOM_MARGIN");
    if (pszBottomMargin) sMargins.nBottom = atoi(pszBottomMargin);

    const char* pszExtraContentStream = CSLFetchNameValue(papszOptions, "EXTRA_CONTENT_STREAM");

    const char* pszLayerName = CSLFetchNameValue(papszOptions, "LAYER_NAME");
    const char* pszExtraContentLayerName = CSLFetchNameValue(papszOptions, "EXTRA_CONTENT_LAYER_NAME");

/* -------------------------------------------------------------------- */
/*      Create file.                                                    */
/* -------------------------------------------------------------------- */
    VSILFILE* fp = VSIFOpenL(pszFilename, "wb");
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create PDF file %s.\n",
                  pszFilename );
        return NULL;
    }


    GDALPDFWriter oWriter(fp);

    if( CSLFetchBoolean(papszOptions, "WRITE_INFO", TRUE) )
        oWriter.SetInfo(poSrcDS, papszOptions);
    oWriter.SetXMP(poSrcDS, pszXMP);

    oWriter.StartPage(poSrcDS,
                      dfDPI,
                      pszGEO_ENCODING,
                      pszNEATLINE,
                      &sMargins);

    int bRet = oWriter.WriteImagery(poSrcDS,
                                    dfDPI,
                                    &sMargins,
                                    eCompressMethod,
                                    nPredictor,
                                    nJPEGQuality,
                                    pszJPEG2000_DRIVER,
                                    nBlockXSize, nBlockYSize,
                                    pfnProgress, pProgressData);

    if (bRet)
        oWriter.EndPage(pszLayerName,
                        pszExtraContentStream,
                        pszExtraContentLayerName);
    oWriter.Close();

    if (!bRet)
    {
        VSIUnlink(pszFilename);
        return NULL;
    }
    else
    {
        return (GDALDataset*) GDALOpen(pszFilename, GA_ReadOnly);
    }
}
