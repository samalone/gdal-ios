/******************************************************************************
 * $Id$
 *
 * Project:  PDF Translator
 * Purpose:  Implements OGRPDFDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_pdf.h"
#include "ogr_p.h"
#include "cpl_conv.h"

#include "pdfdataset.h"
#include "pdfcreatecopy.h"

#include "memdataset.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRPDFLayer()                             */
/************************************************************************/

OGRPDFLayer::OGRPDFLayer( OGRPDFDataSource* poDS,
                          const char * pszName,
                          OGRSpatialReference *poSRS,
                          OGRwkbGeometryType eGeomType ) :
                                OGRMemLayer(pszName, poSRS, eGeomType )
{
    this->poDS = poDS;
}

/************************************************************************/
/*                            CreateFeature()                           */
/************************************************************************/

OGRErr OGRPDFLayer::CreateFeature( OGRFeature *poFeature )
{
    poDS->SetModified();
    return OGRMemLayer::CreateFeature(poFeature);
}

/************************************************************************/
/*                              Fill()                                  */
/************************************************************************/

void OGRPDFLayer::Fill( GDALPDFArray* poArray )
{
    OGRwkbGeometryType eGeomType = wkbUnknown;
    int bGeomTypeSet = FALSE;
    int bGeomTypeMixed = FALSE;

    for(int i=0;i<poArray->GetLength();i++)
    {
        GDALPDFObject* poFeatureObj = poArray->Get(i);
        if (poFeatureObj->GetType() != PDFObjectType_Dictionary)
            continue;

        GDALPDFObject* poA = poFeatureObj->GetDictionary()->Get("A");
        if (!(poA != NULL && poA->GetType() == PDFObjectType_Dictionary))
            continue;

        GDALPDFObject* poP = poA->GetDictionary()->Get("P");
        if (!(poP != NULL && poP->GetType() == PDFObjectType_Array))
            continue;

        GDALPDFObject* poK = poFeatureObj->GetDictionary()->Get("K");
        int nK = -1;
        if (poK != NULL && poK->GetType() == PDFObjectType_Int)
            nK = poK->GetInt();

        GDALPDFArray* poPArray = poP->GetArray();
        int j;
        for(j = 0;j<poPArray->GetLength();j++)
        {
            GDALPDFObject* poKV = poPArray->Get(j);
            if (poKV->GetType() == PDFObjectType_Dictionary)
            {
                GDALPDFObject* poN = poKV->GetDictionary()->Get("N");
                GDALPDFObject* poV = poKV->GetDictionary()->Get("V");
                if (poN != NULL && poN->GetType() == PDFObjectType_String &&
                    poV != NULL)
                {
                    int nIdx = GetLayerDefn()->GetFieldIndex( poN->GetString().c_str() );
                    OGRFieldType eType = OFTString;
                    if (poV->GetType() == PDFObjectType_Int)
                        eType = OFTInteger;
                    else if (poV->GetType() == PDFObjectType_Real)
                        eType = OFTReal;
                    if (nIdx < 0)
                    {
                        OGRFieldDefn oField(poN->GetString().c_str(), eType);
                        CreateField(&oField);
                    }
                    else if (GetLayerDefn()->GetFieldDefn(nIdx)->GetType() != eType &&
                                GetLayerDefn()->GetFieldDefn(nIdx)->GetType() != OFTString)
                    {
                        OGRFieldDefn oField(poN->GetString().c_str(), OFTString);
                        AlterFieldDefn( nIdx, &oField, ALTER_TYPE_FLAG );
                    }
                }
            }
        }

        OGRFeature* poFeature = new OGRFeature(GetLayerDefn());
        for(j = 0;j<poPArray->GetLength();j++)
        {
            GDALPDFObject* poKV = poPArray->Get(j);
            if (poKV->GetType() == PDFObjectType_Dictionary)
            {
                GDALPDFObject* poN = poKV->GetDictionary()->Get("N");
                GDALPDFObject* poV = poKV->GetDictionary()->Get("V");
                if (poN != NULL && poN->GetType() == PDFObjectType_String &&
                    poV != NULL)
                {
                    if (poV->GetType() == PDFObjectType_String)
                        poFeature->SetField(poN->GetString().c_str(), poV->GetString().c_str());
                    else if (poV->GetType() == PDFObjectType_Int)
                        poFeature->SetField(poN->GetString().c_str(), poV->GetInt());
                    else if (poV->GetType() == PDFObjectType_Real)
                        poFeature->SetField(poN->GetString().c_str(), poV->GetReal());
                }
            }
        }

        if (nK >= 0)
        {
            OGRGeometry* poGeom = poDS->GetGeometryFromMCID(nK);
            if (poGeom)
            {
                if (!bGeomTypeSet)
                {
                    bGeomTypeSet = TRUE;
                    eGeomType = poGeom->getGeometryType();
                }
                else if (eGeomType != poGeom->getGeometryType())
                {
                    bGeomTypeMixed = TRUE;
                }
                poGeom->assignSpatialReference(GetSpatialRef());
                poFeature->SetGeometry(poGeom);
            }
        }

        CreateFeature(poFeature);

        delete poFeature;
    }

    if (bGeomTypeSet && !bGeomTypeMixed)
        GetLayerDefn()->SetGeomType(eGeomType);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPDFLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;
    else
        return OGRMemLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                          OGRPDFDataSource()                          */
/************************************************************************/

OGRPDFDataSource::OGRPDFDataSource()

{
    pszName = NULL;
    papszOptions = NULL;

    nLayers = 0;
    papoLayers = NULL;

    bModified = FALSE;
    bWritable = FALSE;

    poGDAL_DS = NULL;
    poPageObj = NULL;
    poCatalogObj = NULL;
    dfPageWidth = dfPageHeight = 0;

    InitMapOperators();
}

/************************************************************************/
/*                         ~OGRPDFDataSource()                          */
/************************************************************************/

OGRPDFDataSource::~OGRPDFDataSource()

{
    SyncToDisk();

    CleanupIntermediateResources();

    CPLFree( pszName );
    CSLDestroy( papszOptions );

    for(int i=0;i<nLayers;i++)
        delete papoLayers[i];
    CPLFree( papoLayers );
}

/************************************************************************/
/*                   CleanupIntermediateResources()                     */
/************************************************************************/

void OGRPDFDataSource::CleanupIntermediateResources()
{
    std::map<int,OGRGeometry*>::iterator oMapIter = oMapMCID.begin();
    for( ; oMapIter != oMapMCID.end(); ++oMapIter)
        delete oMapIter->second;
    oMapMCID.erase(oMapMCID.begin(), oMapMCID.end());

    delete poGDAL_DS;
    poGDAL_DS = NULL;

    poPageObj = NULL;
    poCatalogObj = NULL;
}

/************************************************************************/
/*                          InitMapOperators()                          */
/************************************************************************/

void OGRPDFDataSource::InitMapOperators()
{
    oMapOperators["b"] = 0;
    oMapOperators["B"] = 0;
    oMapOperators["b*"] = 0;
    oMapOperators["B*"] = 0;
    // BDC
    // BI
    // BMC
    // BT
    // BX
    oMapOperators["c"] = 6;
    oMapOperators["cm"] = 6;
    // CS
    // cs
    oMapOperators["d"] = 1; /* we have ignored the first arg */
    // d0
    // d1
    oMapOperators["Do"] = 1;
    // DP
    // EI
    // EMC
    // ET
    // EX
    oMapOperators["f"] = 0;
    oMapOperators["F"] = 0;
    oMapOperators["f*"] = 0;
    oMapOperators["G"] = 1;
    oMapOperators["g"] = 1;
    oMapOperators["gs"] = 1;
    oMapOperators["h"] = 0;
    // i
    // ID
    oMapOperators["j"] = 1;
    oMapOperators["J"] = 1;
    // K
    // k
    oMapOperators["l"] = 2;
    oMapOperators["m"] = 2;
    oMapOperators["M"] = 1;
    // MP
    oMapOperators["n"] = 0;
    oMapOperators["q"] = 0;
    oMapOperators["Q"] = 0;
    oMapOperators["re"] = 4;
    oMapOperators["RG"] = 3;
    oMapOperators["rg"] = 3;
    // ri
    oMapOperators["s"] = 0;
    oMapOperators["S"] = 0;
    // SC
    // sc
    // SCN
    // scn
    // sh
    // T*
    // Tc
    // Td
    // TD
    // Tf
    // Tj
    // TJ
    // TL
    // Tm
    // Tr
    // Ts
    // Tw
    // Tz
    oMapOperators["v"] = 4;
    oMapOperators["w"] = 1;
    oMapOperators["W"] = 0;
    oMapOperators["W*"] = 0;
    oMapOperators["y"] = 4;
    // '
    // "
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPDFDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPDFDataSource::GetLayer( int iLayer )

{
    if (iLayer < 0 || iLayer >= nLayers)
        return NULL;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                            GetLayerCount()                           */
/************************************************************************/

int OGRPDFDataSource::GetLayerCount()
{
    return nLayers;
}

/************************************************************************/
/*                            ExploreTree()                             */
/************************************************************************/

void OGRPDFDataSource::ExploreTree(GDALPDFObject* poObj)
{
    if (poObj->GetType() != PDFObjectType_Dictionary)
        return;

    GDALPDFDictionary* poDict = poObj->GetDictionary();

    GDALPDFObject* poS = poDict->Get("S");
    CPLString osS;
    if (poS != NULL && poS->GetType() == PDFObjectType_Name)
    {
        osS = poS->GetName();
    }

    GDALPDFObject* poT = poDict->Get("T");
    CPLString osT;
    if (poT != NULL && poT->GetType() == PDFObjectType_String)
    {
        osT = poT->GetString();
    }

    GDALPDFObject* poK = poDict->Get("K");
    if (poK == NULL)
        return;

    if (poK->GetType() == PDFObjectType_Array)
    {
        GDALPDFArray* poArray = poK->GetArray();
        if (poArray->GetLength() > 0 &&
            poArray->Get(0)->GetType() == PDFObjectType_Dictionary &&
            poArray->Get(0)->GetDictionary()->Get("K") != NULL &&
            poArray->Get(0)->GetDictionary()->Get("K")->GetType() == PDFObjectType_Int)
        {
            CPLString osLayerName;
            if (osT.size())
                osLayerName = osT;
            else
            {
                if (osS.size())
                    osLayerName = osS;
                else
                    osLayerName = CPLSPrintf("Layer%d", nLayers + 1);
            }

            const char* pszWKT = poGDAL_DS->GetProjectionRef();
            OGRSpatialReference* poSRS = NULL;
            if (pszWKT && pszWKT[0] != '\0')
            {
                poSRS = new OGRSpatialReference();
                poSRS->importFromWkt((char**) &pszWKT);
            }

            OGRPDFLayer* poLayer =
                new OGRPDFLayer(this, osLayerName.c_str(), poSRS, wkbUnknown);
            delete poSRS;

            poLayer->Fill(poArray);

            papoLayers = (OGRLayer**)
                CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
            papoLayers[nLayers] = poLayer;
            nLayers ++;
        }
        else
        {
            for(int i=0;i<poArray->GetLength();i++)
                ExploreTree(poArray->Get(i));
        }
    }
    else if (poK->GetType() == PDFObjectType_Dictionary)
    {
        ExploreTree(poK);
    }
}

/************************************************************************/
/*                        GetGeometryFromMCID()                         */
/************************************************************************/

OGRGeometry* OGRPDFDataSource::GetGeometryFromMCID(int nMCID)
{
    std::map<int,OGRGeometry*>::iterator oMapIter = oMapMCID.find(nMCID);
    if (oMapIter != oMapMCID.end())
        return oMapIter->second;
    else
        return NULL;
}

/************************************************************************/
/*                            GraphicState                              */
/************************************************************************/

class GraphicState
{
    public:
        double adfCM[6];

        GraphicState()
        {
            adfCM[0] = 1;
            adfCM[1] = 0;
            adfCM[2] = 0;
            adfCM[3] = 1;
            adfCM[4] = 0;
            adfCM[5] = 0;
        }

        void MultiplyBy(double adfMatrix[6])
        {
            /*
            [ a b 0 ]     [ a' b' 0]     [ aa' + bc'       ab' + bd'       0 ]
            [ c d 0 ]  *  [ c' d' 0]  =  [ ca' + dc'       cb' + dd'       0 ]
            [ e f 1 ]     [ e' f' 1]     [ ea' + fc' + e'  eb' + fd' + f'  1 ]
            */

            double a = adfCM[0];
            double b = adfCM[1];
            double c = adfCM[2];
            double d = adfCM[3];
            double e = adfCM[4];
            double f = adfCM[5];
            double ap = adfMatrix[0];
            double bp = adfMatrix[1];
            double cp = adfMatrix[2];
            double dp = adfMatrix[3];
            double ep = adfMatrix[4];
            double fp = adfMatrix[5];
            adfCM[0] = a*ap + b*cp;
            adfCM[1] = a*bp + b*dp;
            adfCM[2] = c*ap + d*cp;
            adfCM[3] = c*bp + d*dp;
            adfCM[4] = e*ap + f*cp + ep;
            adfCM[5] = e*bp + f*dp + fp;
        }

        void ApplyMatrix(double adfCoords[2])
        {
            double x = adfCoords[0];
            double y = adfCoords[1];

            adfCoords[0] = x * adfCM[0] + y * adfCM[2] + adfCM[4];
            adfCoords[1] = x * adfCM[1] + y * adfCM[3] + adfCM[5];
        }
};

/************************************************************************/
/*                         PDFCoordsToSRSCoords()                       */
/************************************************************************/

void OGRPDFDataSource::PDFCoordsToSRSCoords(double x, double y,
                                            double& X, double &Y)
{
    x = x / dfPageWidth * nXSize;
    y = (1 - y / dfPageHeight) * nYSize;

    X = adfGeoTransform[0] + x * adfGeoTransform[1] + y * adfGeoTransform[2];
    Y = adfGeoTransform[3] + x * adfGeoTransform[4] + y * adfGeoTransform[5];

    if( fabs(X - (int)floor(X + 0.5)) < 1e-8 )
        X = (int)floor(X + 0.5);
    if( fabs(Y - (int)floor(Y + 0.5)) < 1e-8 )
        Y = (int)floor(Y + 0.5);
}

/************************************************************************/
/*                            UnstackTokens()                           */
/************************************************************************/

int OGRPDFDataSource::UnstackTokens(const CPLString& osToken,
                                    std::stack<CPLString>& osTokenStack,
                                    double* adfCoords)
{
    int nArgs = oMapOperators[osToken];
    for(int i=0;i<nArgs;i++)
    {
        if (osTokenStack.empty())
        {
            CPLDebug("PDF", "not enough arguments for %s", osToken.c_str());
            return FALSE;
        }
        adfCoords[nArgs-1-i] = atof(osTokenStack.top());
        osTokenStack.pop();
    }
    return TRUE;
}

/************************************************************************/
/*                           ParseContent()                             */
/************************************************************************/

#define NEW_SUBPATH -99
#define CLOSE_SUBPATH -98
#define FILL_SUBPATH -97

void OGRPDFDataSource::ParseContent(const char* pszContent,
                                    int nMCID,
                                    GDALPDFObject* poResources)
{
    CPLString osToken;
    char ch;
    std::stack<CPLString> osTokenStack;
    int bInString = FALSE;
    int nBDCLevel = 0;
    int nParenthesisLevel = 0;
    int nArrayLevel = 0;
    int nBTLevel = 0;

    GraphicState oGS;
    std::stack<GraphicState> oGSStack;

    std::vector<double> oCoords;
    int bHasFoundFill = FALSE;
    int bHasMultiPart = FALSE;
    int bHasRe = FALSE;

    while((ch = *pszContent) != '\0')
    {
        int bPushToken = FALSE;

        if (!bInString && ch == '%')
        {
            /* Skip comments until end-of-line */
            while((ch = *pszContent) != '\0')
            {
                if (ch == '\r' || ch == '\n')
                    break;
                pszContent ++;
            }
            if (ch == 0)
                break;
        }
        else if (!bInString && (ch == ' ' || ch == '\r' || ch == '\n'))
        {
            bPushToken = TRUE;
        }

        /* Ignore arrays */
        else if (!bInString && osToken.size() == 0 && ch == '[')
        {
            nArrayLevel ++;
        }
        else if (!bInString && nArrayLevel && osToken.size() == 0 && ch == ']')
        {
            nArrayLevel --;
        }

        else if (!bInString && osToken.size() == 0 && ch == '(')
        {
            bInString = TRUE;
            nParenthesisLevel ++;
            osToken += ch;
        }
        else if (bInString && ch == '(')
        {
            nParenthesisLevel ++;
            osToken += ch;
        }
        else if (bInString && ch == ')')
        {
            nParenthesisLevel --;
            osToken += ch;
            if (nParenthesisLevel == 0)
            {
                bInString = FALSE;
                bPushToken = TRUE;
            }
        }
        else
        {
            osToken += ch;
        }

        pszContent ++;
        if (pszContent[0] == '\0')
            bPushToken = TRUE;

        if (bPushToken && osToken.size())
        {
            if (osToken == "BDC")
                nBDCLevel ++;
            else if (osToken == "EMC")
            {
                nBDCLevel --;
                if (nBDCLevel == 0)
                    break;
            }

            /* Ignore any text stuff */
            else if (osToken == "BT")
                nBTLevel ++;
            else if (osToken == "ET")
            {
                nBTLevel --;
                if (nBTLevel < 0)
                    return;
            }
            else if (!nArrayLevel && !nBTLevel)
            {
                if (osToken == "q")
                {
                    oGSStack.push(oGS);
                }
                else if (osToken == "Q")
                {
                    if (oGSStack.empty())
                    {
                        CPLDebug("PDF", "not enough arguments for %s", osToken.c_str());
                        return;
                    }

                    oGS = oGSStack.top();
                    oGSStack.pop();
                }
                else if (osToken == "cm")
                {
                    double adfMatrix[6];
                    for(int i=0;i<6;i++)
                    {
                        if (osTokenStack.empty())
                        {
                            CPLDebug("PDF", "not enough arguments for %s", osToken.c_str());
                            return;
                        }
                        adfMatrix[6-1-i] = atof(osTokenStack.top());
                        osTokenStack.pop();
                    }

                    oGS.MultiplyBy(adfMatrix);
                }
                else if (osToken == "b" ||
                         osToken == "b*")
                {
                    if (!(oCoords.size() > 0 &&
                          oCoords[oCoords.size() - 2] == CLOSE_SUBPATH &&
                          oCoords[oCoords.size() - 1] == CLOSE_SUBPATH))
                    {
                        oCoords.push_back(CLOSE_SUBPATH);
                        oCoords.push_back(CLOSE_SUBPATH);
                    }
                    oCoords.push_back(FILL_SUBPATH);
                    oCoords.push_back(FILL_SUBPATH);
                    bHasFoundFill = TRUE;
                }
                else if (osToken == "B" ||
                         osToken == "B*" ||
                         osToken == "f" ||
                         osToken == "F" ||
                         osToken == "f*")
                {
                    oCoords.push_back(FILL_SUBPATH);
                    oCoords.push_back(FILL_SUBPATH);
                    bHasFoundFill = TRUE;
                }
                else if (osToken == "h")
                {
                    oCoords.push_back(CLOSE_SUBPATH);
                    oCoords.push_back(CLOSE_SUBPATH);
                }
                else if (osToken == "n")
                {
                    oCoords.resize(0);
                }
                else if (osToken == "m" || osToken == "l")
                {
                    double adfCoords[2];
                    if (!UnstackTokens(osToken, osTokenStack, adfCoords))
                        return;

                    if (osToken == "m")
                    {
                        if (oCoords.size() != 0)
                            bHasMultiPart = TRUE;
                        oCoords.push_back(NEW_SUBPATH);
                        oCoords.push_back(NEW_SUBPATH);
                    }

                    oGS.ApplyMatrix(adfCoords);
                    oCoords.push_back(adfCoords[0]);
                    oCoords.push_back(adfCoords[1]);
                }
                else if (osToken == "c") /* Bezier curve */
                {
                    double adfCoords[6];
                    if (!UnstackTokens(osToken, osTokenStack, adfCoords))
                        return;

                    oGS.ApplyMatrix(adfCoords + 4);
                    oCoords.push_back(adfCoords[4]);
                    oCoords.push_back(adfCoords[5]);
                }
                else if (osToken == "v" || osToken == "y") /* Bezier curve */
                {
                    double adfCoords[4];
                    if (!UnstackTokens(osToken, osTokenStack, adfCoords))
                        return;

                    oGS.ApplyMatrix(adfCoords + 2);
                    oCoords.push_back(adfCoords[2]);
                    oCoords.push_back(adfCoords[3]);
                }
                else if (osToken == "re") /* Rectangle */
                {
                    double adfCoords[4];
                    if (!UnstackTokens(osToken, osTokenStack, adfCoords))
                        return;

                    adfCoords[2] += adfCoords[0];
                    adfCoords[3] += adfCoords[1];

                    oGS.ApplyMatrix(adfCoords);
                    oGS.ApplyMatrix(adfCoords + 2);

                    if (oCoords.size() != 0)
                        bHasMultiPart = TRUE;
                    oCoords.push_back(NEW_SUBPATH);
                    oCoords.push_back(NEW_SUBPATH);
                    oCoords.push_back(adfCoords[0]);
                    oCoords.push_back(adfCoords[1]);
                    oCoords.push_back(adfCoords[2]);
                    oCoords.push_back(adfCoords[1]);
                    oCoords.push_back(adfCoords[2]);
                    oCoords.push_back(adfCoords[3]);
                    oCoords.push_back(adfCoords[0]);
                    oCoords.push_back(adfCoords[3]);
                    oCoords.push_back(CLOSE_SUBPATH);
                    oCoords.push_back(CLOSE_SUBPATH);
                    bHasRe = TRUE;
                }

                else if (osToken == "Do")
                {
                    if (osTokenStack.empty())
                    {
                        CPLDebug("PDF",
                                 "not enough arguments for %s",
                                 osToken.c_str());
                        return;
                    }

                    CPLString osObjectName = osTokenStack.top();
                    osTokenStack.pop();

                    if (osObjectName[0] != '/')
                        return;

                    if (poResources == NULL)
                        return;

                    GDALPDFObject* poXObject =
                        poResources->GetDictionary()->Get("XObject");
                    if (poXObject == NULL ||
                        poXObject->GetType() != PDFObjectType_Dictionary)
                        return;

                    GDALPDFObject* poObject =
                        poXObject->GetDictionary()->Get(osObjectName.c_str() + 1);
                    if (poObject == NULL)
                        return;

                    GDALPDFStream* poStream = poObject->GetStream();
                    if (!poStream)
                        return;

                    char* pszStr = poStream->GetBytes();
                    ParseContent(pszStr, nMCID, NULL);
                    CPLFree(pszStr);
                }
                else if (oMapOperators.find(osToken) != oMapOperators.end())
                {
                    int nArgs = oMapOperators[osToken];
                    for(int i=0;i<nArgs;i++)
                    {
                        if (osTokenStack.empty())
                        {
                            CPLDebug("PDF",
                                     "not enough arguments for %s",
                                     osToken.c_str());
                            return;
                        }
                        osTokenStack.pop();
                    }
                }
                else
                {
                    //printf("%s\n", osToken.c_str());
                    osTokenStack.push(osToken);
                }
            }

            osToken = "";
        }
    }

    if (!osTokenStack.empty())
    {
        while(!osTokenStack.empty())
        {
            CPLDebug("PDF",
                     "Remaing values in stack : %s",
                     osTokenStack.top().c_str());
            osTokenStack.pop();
        }
        return;
    }

    if (!oCoords.size())
        return;


    OGRGeometry* poGeom = NULL;
    if (!bHasFoundFill)
    {
        OGRLineString* poLS = NULL;
        OGRMultiLineString* poMLS = NULL;
        if (bHasMultiPart)
        {
            poMLS = new OGRMultiLineString();
            poGeom = poMLS;
        }

        for(size_t i=0;i<oCoords.size();i+=2)
        {
            if (oCoords[i] == NEW_SUBPATH && oCoords[i+1] == NEW_SUBPATH)
            {
                poLS = new OGRLineString();
                if (poMLS)
                    poMLS->addGeometryDirectly(poLS);
                else
                    poGeom = poLS;
            }
            else if (oCoords[i] == CLOSE_SUBPATH && oCoords[i+1] == CLOSE_SUBPATH)
            {
                if (poLS && poLS->getNumPoints() >= 2 &&
                    !(poLS->getX(0) == poLS->getX(poLS->getNumPoints()-1) &&
                        poLS->getY(0) == poLS->getY(poLS->getNumPoints()-1)))
                {
                    poLS->addPoint(poLS->getX(0), poLS->getY(0));
                }
            }
            else if (oCoords[i] == FILL_SUBPATH && oCoords[i+1] == FILL_SUBPATH)
            {
                /* Should not happen */
            }
            else
            {
                if (poLS)
                {
                    double X, Y;
                    PDFCoordsToSRSCoords(oCoords[i], oCoords[i+1], X, Y);

                    poLS->addPoint(X, Y);
                }
            }
        }
    }
    else
    {
        OGRLinearRing* poLS = NULL;
        int nPolys = 0;
        OGRGeometry** papoPoly = NULL;

        for(size_t i=0;i<oCoords.size();i+=2)
        {
            if (oCoords[i] == NEW_SUBPATH && oCoords[i+1] == NEW_SUBPATH)
            {
                delete poLS;
                poLS = new OGRLinearRing();
            }
            else if ((oCoords[i] == CLOSE_SUBPATH && oCoords[i+1] == CLOSE_SUBPATH) ||
                        (oCoords[i] == FILL_SUBPATH && oCoords[i+1] == FILL_SUBPATH))
            {
                if (poLS)
                {
                    poLS->closeRings();

                    /* Recognize points as outputed by GDAL */
                    if (nPolys == 0 &&
                        poLS->getNumPoints() == 5 &&
                        poLS->getY(0) == poLS->getY(2) &&
                        poLS->getX(1) == poLS->getX(3) &&
                        fabs((poLS->getX(0) + poLS->getX(2)) / 2 - poLS->getX(1)) < 1e-5 &&
                        fabs((poLS->getY(1) + poLS->getY(3)) / 2 - poLS->getY(0)) < 1e-5)
                    {
                        poGeom = new OGRPoint((poLS->getX(0) + poLS->getX(2)) / 2,
                                                (poLS->getY(1) + poLS->getY(3)) / 2);
                        break;
                    }
                    /* ESRI points */
                    else if (bHasRe && oCoords.size() == 14 && nPolys == 0 &&
                                poLS->getNumPoints() == 5 &&
                                poLS->getY(0) == poLS->getY(1) &&
                                poLS->getX(1) == poLS->getX(2) &&
                                poLS->getY(2) == poLS->getY(3) &&
                                poLS->getX(3) == poLS->getX(0))
                    {
                        poGeom = new OGRPoint((poLS->getX(0) + poLS->getX(1)) / 2,
                                                (poLS->getY(0) + poLS->getY(2)) / 2);
                        break;
                    }
                    else
                    {
                        OGRPolygon* poPoly =  new OGRPolygon();
                        poPoly->addRingDirectly(poLS);
                        poLS = NULL;

                        papoPoly = (OGRGeometry**) CPLRealloc(papoPoly, (nPolys + 1) * sizeof(OGRGeometry*));
                        papoPoly[nPolys ++] = poPoly;
                    }
                }
            }
            else
            {
                if (poLS)
                {
                    double X, Y;
                    PDFCoordsToSRSCoords(oCoords[i], oCoords[i+1], X, Y);

                    poLS->addPoint(X, Y);
                }
            }
        }

        delete poLS;

        int bIsValidGeometry;
        if (nPolys)
            poGeom = OGRGeometryFactory::organizePolygons(
                    papoPoly, nPolys, &bIsValidGeometry, NULL);
        CPLFree(papoPoly);
    }

    /* Save geometry in map */
    oMapMCID[nMCID] = poGeom;
}

/************************************************************************/
/*                          ExploreContents()                           */
/************************************************************************/

void OGRPDFDataSource::ExploreContents(GDALPDFObject* poObj,
                                       GDALPDFObject* poResources)
{
    if (poObj->GetType() == PDFObjectType_Array)
    {
        GDALPDFArray* poArray = poObj->GetArray();
        for(int i=0;i<poArray->GetLength();i++)
            ExploreContents(poArray->Get(i), poResources);
    }

    if (poObj->GetType() != PDFObjectType_Dictionary)
        return;

    GDALPDFStream* poStream = poObj->GetStream();
    if (!poStream)
        return;

    char* pszStr = poStream->GetBytes();
    const char* pszMCID = (const char*) pszStr;
    while((pszMCID = strstr(pszMCID, "/MCID")) != NULL)
    {
        const char* pszBDC = strstr(pszMCID, "BDC");
        if (pszBDC)
        {
            int nMCID = atoi(pszMCID + 6);
            if (GetGeometryFromMCID(nMCID) == NULL)
                ParseContent(pszBDC, nMCID, poResources);
        }
        pszMCID += 5;
    }
    CPLFree(pszStr);
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

int OGRPDFDataSource::Open( const char * pszName)
{
    this->pszName = CPLStrdup(pszName);

    poGDAL_DS = GDALPDFOpen(pszName, GA_ReadOnly);
    if (poGDAL_DS == NULL)
        return FALSE;

    const char* pszPageObj = poGDAL_DS->GetMetadataItem("PDF_PAGE_OBJECT");
    if (pszPageObj)
        sscanf(pszPageObj, "%p", &poPageObj);
    if (poPageObj == NULL || poPageObj->GetType() != PDFObjectType_Dictionary)
        return FALSE;

    GDALPDFObject* poMediaBox = poPageObj->GetDictionary()->Get("MediaBox");
    if (poMediaBox == NULL || poMediaBox->GetType() != PDFObjectType_Array ||
        poMediaBox->GetArray()->GetLength() != 4)
        return FALSE;

    if (poMediaBox->GetArray()->Get(2)->GetType() == PDFObjectType_Real)
        dfPageWidth = poMediaBox->GetArray()->Get(2)->GetReal();
    else if (poMediaBox->GetArray()->Get(2)->GetType() == PDFObjectType_Int)
        dfPageWidth = poMediaBox->GetArray()->Get(2)->GetInt();
    else
        return FALSE;

    if (poMediaBox->GetArray()->Get(3)->GetType() == PDFObjectType_Real)
        dfPageHeight = poMediaBox->GetArray()->Get(3)->GetReal();
    else if (poMediaBox->GetArray()->Get(3)->GetType() == PDFObjectType_Int)
        dfPageHeight = poMediaBox->GetArray()->Get(3)->GetInt();
    else
        return FALSE;

    GDALPDFObject* poContents = poPageObj->GetDictionary()->Get("Contents");
    if (poContents == NULL)
        return FALSE;

    if (poContents->GetType() != PDFObjectType_Dictionary &&
        poContents->GetType() != PDFObjectType_Array)
        return FALSE;

    GDALPDFObject* poResources = poPageObj->GetDictionary()->Get("Resources");
    if (poResources == NULL || poResources->GetType() != PDFObjectType_Dictionary)
        return FALSE;
    
    const char* pszCatalog = poGDAL_DS->GetMetadataItem("PDF_CATALOG_OBJECT");
    if (pszCatalog)
        sscanf(pszCatalog, "%p", &poCatalogObj);
    if (poCatalogObj == NULL || poCatalogObj->GetType() != PDFObjectType_Dictionary)
        return FALSE;

    GDALPDFObject* poStructTreeRoot = poCatalogObj->GetDictionary()->Get("StructTreeRoot");
    if (poStructTreeRoot == NULL || poStructTreeRoot->GetType() != PDFObjectType_Dictionary)
        return FALSE;

    nXSize = poGDAL_DS->GetRasterXSize();
    nYSize = poGDAL_DS->GetRasterYSize();
    poGDAL_DS->GetGeoTransform(adfGeoTransform);

    ExploreContents(poContents, poResources);
    ExploreTree(poStructTreeRoot);

    CleanupIntermediateResources();

    if (nLayers == 0)
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int OGRPDFDataSource::Create( const char * pszName, char **papszOptions )
{
    this->pszName = CPLStrdup(pszName);
    this->papszOptions = CSLDuplicate(papszOptions);
    bWritable = TRUE;

    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRPDFDataSource::CreateLayer( const char * pszLayerName,
                                OGRSpatialReference *poSRS,
                                OGRwkbGeometryType eType,
                                char ** papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRLayer* poLayer = new OGRPDFLayer(this, pszLayerName, poSRS, eType);

    papoLayers = (OGRLayer**)CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
    papoLayers[nLayers] = poLayer;
    nLayers ++;

    return poLayer;
}

/************************************************************************/
/*                            SyncToDisk()                              */
/************************************************************************/

OGRErr OGRPDFDataSource::SyncToDisk()
{
    if (nLayers == 0 || !bModified || !bWritable)
        return OGRERR_NONE;

    bModified = FALSE;

    OGREnvelope sGlobalExtent;
    int bHasExtent = FALSE;
    for(int i=0;i<nLayers;i++)
    {
        OGREnvelope sExtent;
        if (papoLayers[i]->GetExtent(&sExtent) == OGRERR_NONE)
        {
            bHasExtent = TRUE;
            sGlobalExtent.Merge(sExtent);
        }
    }
    if (!bHasExtent ||
        sGlobalExtent.MinX == sGlobalExtent.MaxX ||
        sGlobalExtent.MinY == sGlobalExtent.MaxY)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot compute spatial extent of features");
        return OGRERR_FAILURE;
    }

    PDFCompressMethod eStreamCompressMethod = COMPRESS_DEFLATE;
    const char* pszStreamCompressMethod = CSLFetchNameValue(papszOptions, "STREAM_COMPRESS");
    if (pszStreamCompressMethod)
    {
        if( EQUAL(pszStreamCompressMethod, "NONE") )
            eStreamCompressMethod = COMPRESS_NONE;
        else if( EQUAL(pszStreamCompressMethod, "DEFLATE") )
            eStreamCompressMethod = COMPRESS_DEFLATE;
        else
        {
            CPLError( CE_Warning, CPLE_NotSupported,
                    "Unsupported value for STREAM_COMPRESS.");
        }
    }

    const char* pszGEO_ENCODING =
        CSLFetchNameValueDef(papszOptions, "GEO_ENCODING", "ISO32000");

    double dfDPI = atof(CSLFetchNameValueDef(papszOptions, "DPI", "72"));
    if (dfDPI < 72.0)
        dfDPI = 72.0;

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

    const char* pszExtraImages = CSLFetchNameValue(papszOptions, "EXTRA_IMAGES");
    const char* pszExtraStream = CSLFetchNameValue(papszOptions, "EXTRA_STREAM");
    const char* pszExtraLayerName = CSLFetchNameValue(papszOptions, "EXTRA_LAYER_NAME");

    const char* pszOGRDisplayField = CSLFetchNameValue(papszOptions, "OGR_DISPLAY_FIELD");
    int bWriteOGRAttributes = CSLFetchBoolean(papszOptions, "OGR_WRITE_ATTRIBUTES", TRUE);

/* -------------------------------------------------------------------- */
/*      Create file.                                                    */
/* -------------------------------------------------------------------- */
    VSILFILE* fp = VSIFOpenL(pszName, "wb");
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create PDF file %s.\n",
                  pszName );
        return OGRERR_FAILURE;
    }

    GDALPDFWriter oWriter(fp);

    double dfRatio = (sGlobalExtent.MaxY - sGlobalExtent.MinY) / (sGlobalExtent.MaxX - sGlobalExtent.MinX);

    int nWidth, nHeight;

    if (dfRatio < 1)
    {
        nWidth = 1024;
        nHeight = nWidth * dfRatio;
    }
    else
    {
        nHeight = 1024;
        nWidth = nHeight / dfRatio;
    }

    GDALDataset* poSrcDS = MEMDataset::Create( "MEM:::", nWidth, nHeight, 0, GDT_Byte, NULL );

    double adfGeoTransform[6];
    adfGeoTransform[0] = sGlobalExtent.MinX;
    adfGeoTransform[1] = (sGlobalExtent.MaxX - sGlobalExtent.MinX) / nWidth;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = sGlobalExtent.MaxY;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = - (sGlobalExtent.MaxY - sGlobalExtent.MinY) / nHeight;

    poSrcDS->SetGeoTransform(adfGeoTransform);

    OGRSpatialReference* poSRS = papoLayers[0]->GetSpatialRef();
    if (poSRS)
    {
        char* pszWKT = NULL;
        poSRS->exportToWkt(&pszWKT);
        poSrcDS->SetProjection(pszWKT);
        CPLFree(pszWKT);
    }

    oWriter.SetInfo(poSrcDS, papszOptions);

    oWriter.StartPage(poSrcDS,
                      dfDPI,
                      pszGEO_ENCODING,
                      pszNEATLINE,
                      &sMargins,
                      eStreamCompressMethod,
                      bWriteOGRAttributes);

    int iObj = 0;
    for(int i=0;i<nLayers;i++)
    {
        GDALPDFLayerDesc osVectorDesc =
            oWriter.StartOGRLayer(papoLayers[i]->GetName(),
                                  bWriteOGRAttributes);

        int iObjLayer = 0;
        for(int j=0;j<papoLayers[i]->GetFeatureCount();j++)
        {
            OGRFeature* poFeature = papoLayers[i]->GetFeature(j);

            oWriter.WriteOGRFeature(osVectorDesc,
                                    (OGRFeatureH) poFeature,
                                    pszOGRDisplayField,
                                    bWriteOGRAttributes,
                                    iObj,
                                    iObjLayer);

            delete poFeature;
        }

        oWriter.EndOGRLayer(osVectorDesc);
    }


    oWriter.EndPage(pszExtraImages,
                    pszExtraStream,
                    pszExtraLayerName);

    oWriter.Close();

    delete poSrcDS;

    return OGRERR_NONE;
}
