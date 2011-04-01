/******************************************************************************
 * $Id$
 *
 * Project:  GFT Translator
 * Purpose:  Implements OGRGFTResultLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_gft.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRGFTResultLayer()                           */
/************************************************************************/

OGRGFTResultLayer::OGRGFTResultLayer(OGRGFTDataSource* poDS,
                                     const char* pszSQL) : OGRGFTLayer(poDS)

{
    osSQL = PatchSQL(pszSQL);

    bGotAllRows = FALSE;

    poFeatureDefn = new OGRFeatureDefn( "result" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbUnknown );
}

/************************************************************************/
/*                       ~OGRGFTResultLayer()                           */
/************************************************************************/

OGRGFTResultLayer::~OGRGFTResultLayer()

{
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGFTResultLayer::ResetReading()

{
    nNextInSeq = 0;
    nOffset = 0;
    if (!bGotAllRows)
    {
        aosRows.resize(0);
        bEOF = FALSE;
    }
}

/************************************************************************/
/*                           FetchNextRows()                            */
/************************************************************************/

int OGRGFTResultLayer::FetchNextRows()
{
    if (!((EQUALN(osSQL.c_str(), "SELECT", 6) ||
           EQUALN(osSQL.c_str(), "select", 6))))
        return FALSE;

    aosRows.resize(0);

    CPLString osChangedSQL(osSQL);
    if (strstr(osSQL.c_str(), " OFFSET ") == NULL &&
        strstr(osSQL.c_str(), " offset ") == NULL &&
        strstr(osSQL.c_str(), " LIMIT ") == NULL &&
        strstr(osSQL.c_str(), " limit ") == NULL )
    {
        osChangedSQL += CPLSPrintf(" OFFSET %d LIMIT %d",
                                   nOffset, GetFeaturesToFetch());
    }

    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLHTTPResult * psResult = poDS->RunSQL(osChangedSQL);
    CPLPopErrorHandler();

    if (psResult == NULL)
    {
        bEOF = TRUE;
        return FALSE;
    }

    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL ||
        psResult->pszErrBuf != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "RunSQL() failed");
        CPLHTTPDestroyResult(psResult);
        bEOF = TRUE;
        return FALSE;
    }

    pszLine = OGRGFTGotoNextLine(pszLine);
    if (pszLine == NULL)
    {
        CPLHTTPDestroyResult(psResult);
        bEOF = TRUE;
        return FALSE;
    }

    ParseCSVResponse(pszLine, aosRows);

    CPLHTTPDestroyResult(psResult);

    bEOF = (int)aosRows.size() < GetFeaturesToFetch();

    return TRUE;
}

/************************************************************************/
/*                         OGRGFTExtractTableID()                        */
/************************************************************************/

static CPLString OGRGFTExtractTableID(const char* pszTableID,
                                      CPLString& osReminder)
{
    CPLString osTableId(pszTableID);
    if (osTableId.size() > 1 &&
        (osTableId[0] == '"' || osTableId[0] == '\''))
    {
        char chFirstChar = osTableId[0];
        osTableId.erase(0, 1);
        for(int i=0;i<(int)osTableId.size();i++)
        {
            if (osTableId[i] == chFirstChar)
            {
                osReminder = osTableId.substr(i+1);
                osTableId.resize(i);
                break;
            }
        }
    }
    else
    {
        for(int i=0;i<(int)osTableId.size();i++)
        {
            if (osTableId[i] == ' ')
            {
                osReminder = osTableId.substr(i);
                osTableId.resize(i);
                break;
            }
        }
    }
    return osTableId;
}

/************************************************************************/
/*                               RunSQL()                               */
/************************************************************************/

int OGRGFTResultLayer::RunSQL()
{
    CPLString osChangedSQL(osSQL);
    int bHasSetLimit = FALSE;
    OGRGFTTableLayer* poTableLayer = NULL;
    OGRFeatureDefn* poTableDefn = NULL;
    CPLString osTableId;
    if (EQUALN(osSQL.c_str(), "SELECT", 6))
    {
        const char* pszFROM = strstr(osSQL.c_str(), " FROM ");
        if (pszFROM == NULL)
            pszFROM = strstr(osSQL.c_str(), " from ");
        if (pszFROM == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "RunSQL() failed. Missing FROM in SELECT");
            return FALSE;
        }
        CPLString osReminder;
        int nPosFROM = pszFROM - osSQL.c_str() + 6;
        osTableId = OGRGFTExtractTableID(pszFROM + 6, osReminder);
        pszFROM = NULL;

        poTableLayer = (OGRGFTTableLayer*) poDS->GetLayerByName(osTableId);
        if (poTableLayer != NULL)
            poTableDefn = poTableLayer->GetLayerDefn();

        if (poTableLayer != NULL &&
            poTableLayer->GetTableId().size() &&
            !EQUAL(osTableId, poTableLayer->GetTableId()))
        {
            osChangedSQL = osSQL;
            osChangedSQL.resize(nPosFROM);
            osChangedSQL += poTableLayer->GetTableId();
            osChangedSQL += osReminder;
            osSQL = osChangedSQL;
            CPLDebug("GFT", "Patching table name (%s) to table id (%s)",
                     osTableId.c_str(), poTableLayer->GetTableId().c_str());
        }

        int nFeaturesToFetch = GetFeaturesToFetch();
        if (strstr(osSQL.c_str(), " OFFSET ") == NULL &&
            strstr(osSQL.c_str(), " offset ") == NULL &&
            strstr(osSQL.c_str(), " LIMIT ") == NULL &&
            strstr(osSQL.c_str(), " limit ") == NULL &&
            nFeaturesToFetch > 0)
        {
            osChangedSQL += CPLSPrintf(" LIMIT %d", nFeaturesToFetch);
            bHasSetLimit = TRUE;
        }
    }
    else
    {
        bGotAllRows = bEOF = TRUE;
        poFeatureDefn->SetGeomType( wkbNone );
    }

    CPLHTTPResult * psResult = poDS->RunSQL(osChangedSQL);

    if (psResult == NULL)
        return FALSE;

    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL ||
        psResult->pszErrBuf != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "RunSQL() failed");
        CPLHTTPDestroyResult(psResult);
        return FALSE;
    }

    if (EQUALN(osSQL.c_str(), "SELECT", 6) ||
        EQUAL(osSQL.c_str(), "SHOW TABLES") ||
        EQUALN(osSQL.c_str(), "DESCRIBE", 8))
    {
        ParseCSVResponse(pszLine, aosRows);
        if (aosRows.size() > 0)
        {
            char** papszTokens = OGRGFTCSVSplitLine(aosRows[0], ',');
            for(int i=0;papszTokens && papszTokens[i];i++)
            {
                const char* pszFieldName = papszTokens[i];
                int iIndex = (poTableDefn) ? poTableDefn->GetFieldIndex(pszFieldName) : -1;
                if (iIndex >= 0)
                {
                    poFeatureDefn->AddFieldDefn(poTableDefn->GetFieldDefn(iIndex));
                    if (iIndex == poTableLayer->GetGeometryFieldIndex())
                        iGeometryField = i;
                    if (iIndex == poTableLayer->GetLatitudeFieldIndex())
                        iLatitudeField = i;
                    if (iIndex == poTableLayer->GetLongitudeFieldIndex())
                        iLongitudeField = i;
                }
                else
                {
                    OGRFieldType eType = OFTString;
                    if (EQUAL(pszFieldName, "COUNT()"))
                        eType = OFTInteger;
                    OGRFieldDefn oFieldDefn(pszFieldName, eType);
                    poFeatureDefn->AddFieldDefn(&oFieldDefn);
                }
            }
            CSLDestroy(papszTokens);

            aosRows.erase(aosRows.begin());
        }

        if (iLatitudeField >= 0 && iLongitudeField >= 0)
        {
            iGeometryField = iLatitudeField;
            poFeatureDefn->SetGeomType( wkbPoint );
        }

        if (bHasSetLimit)
            bGotAllRows = bEOF = (int)aosRows.size() < GetFeaturesToFetch();
        else
            bGotAllRows = bEOF = TRUE;
    }

    CPLHTTPDestroyResult(psResult);

    return TRUE;
}
