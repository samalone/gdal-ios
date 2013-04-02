/******************************************************************************
 * $Id$
 *
 * Project:  ISO8211 library
 * Purpose:  Create a 8211 file from a XML dump file generated by "8211dump -xml"
 * Author:   Even Rouault <even dot rouault at mines-paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_minixml.h"
#include "iso8211.h"
#include <map>
#include <string>

CPL_CVSID("$Id$");

int main(int nArgc, char* papszArgv[])
{
    const char  *pszFilename = NULL, *pszOutFilename = NULL;
    DDFModule  oModule;

/* -------------------------------------------------------------------- */
/*      Check arguments.                                                */
/* -------------------------------------------------------------------- */
    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( pszFilename == NULL )
            pszFilename = papszArgv[iArg];
        else if( pszOutFilename == NULL )
            pszOutFilename = papszArgv[iArg];
        else
        {
            pszFilename = NULL;
            break;
        }
    }

    if( pszFilename == NULL )
    {
        printf( "Usage: 8211createfromxml filename.xml outfilename\n" );
        exit( 1 );
    }
    
    CPLXMLNode* poRoot = CPLParseXMLFile( pszFilename );
    if( poRoot == NULL )
    {
        fprintf(stderr, "Cannot parse XML file '%s'\n", pszFilename);
        exit( 1 );
    }
    
    CPLXMLNode* poXMLDDFModule = CPLSearchXMLNode(poRoot, "=DDFModule");
    if( poXMLDDFModule == NULL )
    {
        fprintf(stderr, "Cannot find DDFModule node in XML file '%s'\n", pszFilename);
        exit( 1 );
    }

    /* Compute the size of the DDFField tag */
    CPLXMLNode* psIter = poXMLDDFModule->psChild;
    int nSizeFieldTag = 0;
    while( psIter != NULL )
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "DDFFieldDefn") == 0 )
        {
            const char* pszTag = CPLGetXMLValue(psIter, "tag", "");
            if( nSizeFieldTag == 0 )
                nSizeFieldTag = (int)strlen(pszTag);
            else if( nSizeFieldTag != (int)strlen(pszTag) )
            {
                fprintf(stderr, "All fields have not the same tag size\n");
                exit( 1 );
            }
        }
        psIter = psIter->psNext;
    }

    char chInterchangeLevel = '3';
    char chLeaderIden = 'L';
    char chCodeExtensionIndicator = 'E';
    char chVersionNumber = '1';
    char chAppIndicator = ' ';
    const char *pszExtendedCharSet = " ! ";
    int nSizeFieldLength = 3;
    int nSizeFieldPos = 4;

    oModule.Initialize(chInterchangeLevel,
                       chLeaderIden,
                       chCodeExtensionIndicator,
                       chVersionNumber,
                       chAppIndicator,
                       pszExtendedCharSet,
                       nSizeFieldLength,
                       nSizeFieldPos,
                       nSizeFieldTag);

    int bCreated = FALSE;

    /* Create DDFFieldDefn and DDFRecord elements */
    psIter = poXMLDDFModule->psChild;
    while( psIter != NULL )
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "DDFFieldDefn") == 0 )
        {
            DDFFieldDefn* poFDefn = new DDFFieldDefn();

            DDF_data_struct_code eStructCode = dsc_elementary;
            const char* pszStructCode = CPLGetXMLValue(psIter, "dataStructCode", "");
            if( strcmp(pszStructCode, "elementary") == 0 ) eStructCode = dsc_elementary;
            else if( strcmp(pszStructCode, "vector") == 0 ) eStructCode = dsc_vector;
            else if( strcmp(pszStructCode, "array") == 0 ) eStructCode = dsc_array;
            else if( strcmp(pszStructCode, "concatenated") == 0 ) eStructCode = dsc_concatenated;

            DDF_data_type_code eTypeCode = dtc_char_string;
            const char* pszTypeCode = CPLGetXMLValue(psIter, "dataTypeCode", "");
            if( strcmp(pszTypeCode, "char_string") == 0 ) eTypeCode = dtc_char_string;
            else if( strcmp(pszTypeCode, "implicit_point") == 0 ) eTypeCode = dtc_implicit_point;
            else if( strcmp(pszTypeCode, "explicit_point") == 0 ) eTypeCode = dtc_explicit_point;
            else if( strcmp(pszTypeCode, "explicit_point_scaled") == 0 ) eTypeCode = dtc_explicit_point_scaled;
            else if( strcmp(pszTypeCode, "char_bit_string") == 0 ) eTypeCode = dtc_char_bit_string;
            else if( strcmp(pszTypeCode, "bit_string") == 0 ) eTypeCode = dtc_bit_string;
            else if( strcmp(pszTypeCode, "mixed_data_type") == 0 ) eTypeCode = dtc_mixed_data_type;

            const char* pszFormatControls = CPLGetXMLValue(psIter, "formatControls", NULL);
            if( eStructCode != dsc_elementary )
                pszFormatControls = NULL;

            const char* pszArrayDescr = CPLGetXMLValue(psIter, "arrayDescr", "");
            if( eStructCode == dsc_vector )
                pszArrayDescr = "";
            else if( eStructCode == dsc_array )
                pszArrayDescr = "*";
 
            poFDefn->Create( CPLGetXMLValue(psIter, "tag", ""),
                             CPLGetXMLValue(psIter, "fieldName", ""),
                             pszArrayDescr,
                             eStructCode, eTypeCode,
                             pszFormatControls );

            CPLXMLNode* psSubIter = psIter->psChild;
            while( psSubIter != NULL )
            {
                if( psSubIter->eType == CXT_Element &&
                    strcmp(psSubIter->pszValue, "DDFSubfieldDefn") == 0 )
                {
                    poFDefn->AddSubfield( CPLGetXMLValue(psSubIter, "name", ""),
                                          CPLGetXMLValue(psSubIter, "format", "") );
                }
                psSubIter = psSubIter->psNext;
            }

            oModule.AddField( poFDefn );
        }
        else if( psIter->eType == CXT_Element &&
                 strcmp(psIter->pszValue, "DDFRecord") == 0 )
        {
            if( !bCreated )
            {
                oModule.Create( pszOutFilename );
                bCreated = TRUE;
            }

            DDFRecord *poRec = new DDFRecord( &oModule );

            CPLXMLNode* psSubIter = psIter->psChild;
            while( psSubIter != NULL )
            {
                if( psSubIter->eType == CXT_Element &&
                    strcmp(psSubIter->pszValue, "DDFField") == 0 )
                {
                    DDFField *poField;
                    const char* pszFieldName = CPLGetXMLValue(psSubIter, "name", "");
                    DDFFieldDefn* poFieldDefn = oModule.FindFieldDefn( pszFieldName );
                    if( poFieldDefn == NULL )
                    {
                        fprintf(stderr, "Can't find field '%s'\n", pszFieldName );
                        exit(1);
                    }

                    poField = poRec->AddField( poFieldDefn );
                    const char* pszValue = CPLGetXMLValue(psSubIter, "value", NULL);
                    if( pszValue != NULL && strncmp(pszValue, "0x", 2) == 0 )
                    {
                        pszValue += 2;
                        int nDataLen = (int)strlen(pszValue)  / 2;
                        char* pabyData = (char*) malloc(nDataLen);
                        for(int i=0;i<nDataLen;i++)
                        {
                            char c;
                            int nHigh, nLow;
                            c = pszValue[2*i];
                            if( c >= 'A' && c <= 'F' )
                                nHigh = 10 + c - 'A';
                            else
                                nHigh = c - '0';
                            c = pszValue[2*i + 1];
                            if( c >= 'A' && c <= 'F' )
                                nLow = 10 + c - 'A';
                            else
                                nLow = c - '0';
                            pabyData[i] = (nHigh << 4) + nLow;
                        }
                        poRec->SetFieldRaw( poField, 0, (const char *) pabyData, nDataLen );
                        free(pabyData);
                    }
                    else
                    {
                        CPLXMLNode* psSubfieldIter = psSubIter->psChild;
                        std::map<std::string, int> oMapSubfield;
                        while( psSubfieldIter != NULL )
                        {
                            if( psSubfieldIter->eType == CXT_Element &&
                                strcmp(psSubfieldIter->pszValue, "DDFSubfield") == 0 )
                            {
                                const char* pszSubfieldName = CPLGetXMLValue(psSubfieldIter, "name", "");
                                const char* pszSubfieldType = CPLGetXMLValue(psSubfieldIter, "type", "");
                                const char* pszSubfieldValue = CPLGetXMLValue(psSubfieldIter, NULL, "");
                                int nOcc = oMapSubfield[pszSubfieldName];
                                oMapSubfield[pszSubfieldName] ++ ;
                                if( strcmp(pszSubfieldType, "float") == 0 )
                                {
                                    poRec->SetFloatSubfield( pszFieldName, 0, pszSubfieldName, nOcc,
                                                           atof(pszSubfieldValue) );
                                }
                                else if( strcmp(pszSubfieldType, "integer") == 0 )
                                {
                                    poRec->SetIntSubfield( pszFieldName, 0, pszSubfieldName, nOcc,
                                                           atoi(pszSubfieldValue) );
                                }
                                else if( strcmp(pszSubfieldType, "string") == 0 )
                                {
                                    poRec->SetStringSubfield( pszFieldName, 0, pszSubfieldName, nOcc,
                                                              pszSubfieldValue );
                                }
                                else if( strcmp(pszSubfieldType, "binary") == 0 &&
                                         strncmp(pszSubfieldValue, "0x", 2) == 0 )
                                {
                                    pszSubfieldValue += 2;
                                    int nDataLen = (int)strlen(pszSubfieldValue) / 2;
                                    char* pabyData = (char*) malloc(nDataLen);
                                    for(int i=0;i<nDataLen;i++)
                                    {
                                        char c;
                                        int nHigh, nLow;
                                        c = pszSubfieldValue[2*i];
                                        if( c >= 'A' && c <= 'F' )
                                            nHigh = 10 + c - 'A';
                                        else
                                            nHigh = c - '0';
                                        c = pszSubfieldValue[2*i + 1];
                                        if( c >= 'A' && c <= 'F' )
                                            nLow = 10 + c - 'A';
                                        else
                                            nLow = c - '0';
                                        pabyData[i] = (nHigh << 4) + nLow;
                                    }
                                    poRec->SetStringSubfield( pszFieldName, 0, pszSubfieldName, nOcc,
                                                              pabyData, nDataLen );
                                    free(pabyData);
                                }
                            }
                            psSubfieldIter = psSubfieldIter->psNext;
                        }
                    }
                }
                psSubIter = psSubIter->psNext;
            }

            poRec->Write();
            delete poRec;
        }

        psIter = psIter->psNext;
    }

    CPLDestroyXMLNode(poRoot);

    oModule.Close();

    return 0;
}
