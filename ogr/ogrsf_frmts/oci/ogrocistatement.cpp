/******************************************************************************
 * $Id$
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Implementation of OGROCIStatement, which encapsulates the 
 *           preparation, executation and fetching from an SQL statement.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.7  2003/02/06 21:16:04  warmerda
 * fixed panFieldMap memory leak
 *
 * Revision 1.6  2003/01/10 22:29:56  warmerda
 * Added separate Prepare step
 *
 * Revision 1.5  2003/01/06 17:59:26  warmerda
 * fiddle with maximum buffer widths
 *
 * Revision 1.4  2003/01/02 21:50:31  warmerda
 * various fixes
 *
 * Revision 1.3  2002/12/29 03:20:50  warmerda
 * initialize new fields
 *
 * Revision 1.2  2002/12/28 04:38:36  warmerda
 * converted to unix file conventions
 *
 * Revision 1.1  2002/12/28 04:07:27  warmerda
 * New
 *
 */

#include "ogr_oci.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGROCIStatement()                           */
/************************************************************************/

OGROCIStatement::OGROCIStatement( OGROCISession *poSessionIn )

{
    poSession = poSessionIn;
    hStatement = NULL;
    poDefn = NULL;

    nRawColumnCount = 0;
    papszCurColumn = NULL;
    papszCurImage = NULL;
    panCurColumnInd = NULL;
    panFieldMap = NULL;

    pszCommandText = NULL;
}

/************************************************************************/
/*                          ~OGROCIStatement()                          */
/************************************************************************/

OGROCIStatement::~OGROCIStatement()

{
    Clean();
}

/************************************************************************/
/*                               Clean()                                */
/************************************************************************/

void OGROCIStatement::Clean()

{
    int  i;

    CPLFree( pszCommandText );
    pszCommandText = NULL;

    if( papszCurColumn != NULL )
    {
        for( i = 0; papszCurColumn[i] != NULL; i++ )
            CPLFree( papszCurColumn[i] );
    }
    CPLFree( papszCurColumn );
    papszCurColumn = NULL;

    CPLFree( papszCurImage );
    papszCurImage = NULL;
    
    CPLFree( panCurColumnInd );
    panCurColumnInd = NULL;

    CPLFree( panFieldMap );
    panFieldMap = NULL;

    if( poDefn != NULL && poDefn->Dereference() <= 0 )
    {
        delete poDefn;
        poDefn = NULL;
    }

    if( hStatement != NULL )
    {
	OCIHandleFree((dvoid *)hStatement, (ub4)OCI_HTYPE_STMT);
        hStatement = NULL;
    }
}

/************************************************************************/
/*                              Prepare()                               */
/************************************************************************/

CPLErr OGROCIStatement::Prepare( const char *pszSQLStatement )

{
    Clean();

    CPLDebug( "OCI", "Prepare(%s)", pszSQLStatement );

    pszCommandText = CPLStrdup(pszSQLStatement);

    if( hStatement != NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Statement already executed once on this OGROCIStatement." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate a statement handle.                                    */
/* -------------------------------------------------------------------- */
    if( poSession->Failed( 
        OCIHandleAlloc( poSession->hEnv, (dvoid **) &hStatement, 
                        (ub4)OCI_HTYPE_STMT,(size_t)0, (dvoid **)0 ), 
        "OCIHandleAlloc(Statement)" ) )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Prepare the statement.                                          */
/* -------------------------------------------------------------------- */
    if( poSession->Failed(
        OCIStmtPrepare( hStatement, poSession->hError, 
                        (text *) pszSQLStatement, strlen(pszSQLStatement),
                        (ub4)OCI_NTV_SYNTAX, (ub4)OCI_DEFAULT ),
        "OCIStmtPrepare" ) )
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                              Execute()                               */
/************************************************************************/

CPLErr OGROCIStatement::Execute( const char *pszSQLStatement,
                                 int nMode )

{
/* -------------------------------------------------------------------- */
/*      Prepare the statement if it is being passed in.                 */
/* -------------------------------------------------------------------- */
    if( pszSQLStatement != NULL )
    {
        CPLErr eErr = Prepare( pszSQLStatement );
        if( eErr != CE_None )
            return eErr;
    }

    if( hStatement == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "No prepared statement in call to OGROCIStatement::Execute(NULL)" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Work out some details about execution mode.                     */
/* -------------------------------------------------------------------- */
    int  bSelect = pszCommandText != NULL 
        && EQUALN(pszCommandText,"SELECT",5);

    if( nMode == -1 )
    {
        if( bSelect )
            nMode = OCI_DEFAULT;
        else
            nMode = OCI_COMMIT_ON_SUCCESS;
    }

/* -------------------------------------------------------------------- */
/*      Execute the statement.                                          */
/* -------------------------------------------------------------------- */
    if( poSession->Failed( 
        OCIStmtExecute( poSession->hSvcCtx, hStatement, 
                        poSession->hError, (ub4)bSelect ? 0 : 1, (ub4)0, 
                        (OCISnapshot *)NULL, (OCISnapshot *)NULL, 
                        (ub4) (bSelect ? OCI_DEFAULT : OCI_COMMIT_ON_SUCCESS)),
        pszCommandText ) )
        return CE_Failure;

    if( !bSelect )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Count the columns.                                              */
/* -------------------------------------------------------------------- */
    for( nRawColumnCount = 0; TRUE; nRawColumnCount++ )
    {								
        OCIParam     *hParmDesc;

        if( OCIParamGet( hStatement, OCI_HTYPE_STMT, poSession->hError, 
                         (dvoid**)&hParmDesc, 
                         (ub4) nRawColumnCount+1 ) != OCI_SUCCESS )
            break;
    }
    
    panFieldMap = (int *) CPLCalloc(sizeof(int),nRawColumnCount);
    
    papszCurColumn = (char **) CPLCalloc(sizeof(char*),nRawColumnCount+1);
    panCurColumnInd = (sb2 *) CPLCalloc(sizeof(sb2),nRawColumnCount+1);
        
/* ==================================================================== */
/*      Establish result column definitions, and setup parameter        */
/*      defines.                                                        */
/* ==================================================================== */
    poDefn = new OGRFeatureDefn( pszCommandText );
    poDefn->Reference();

    for( int iParm = 0; iParm < nRawColumnCount; iParm++ )
    {								
        OGRFieldDefn oField( "", OFTString );
        OCIParam     *hParmDesc;
        ub2          nOCIType;
        ub4          nOCILen;

/* -------------------------------------------------------------------- */
/*      Get parameter definition.                                       */
/* -------------------------------------------------------------------- */
        if( poSession->Failed( 
            OCIParamGet( hStatement, OCI_HTYPE_STMT, poSession->hError, 
                         (dvoid**)&hParmDesc, (ub4) iParm+1 ),
            "OCIParamGet") )
            return CE_Failure;

        if( poSession->GetParmInfo( hParmDesc, &oField, &nOCIType, &nOCILen )
            != CE_None )
            return CE_Failure;

        if( oField.GetType() == OFTBinary )
        {
            panFieldMap[iParm] = -1;
            continue;			
        }

        poDefn->AddFieldDefn( &oField );
        panFieldMap[iParm] = poDefn->GetFieldCount() - 1;

/* -------------------------------------------------------------------- */
/*      Prepare a binding.                                              */
/* -------------------------------------------------------------------- */
        int nBufWidth = 256, nOGRField = panFieldMap[iParm];
        OCIDefine *hDefn = NULL;

        if( oField.GetWidth() > 0 )
            nBufWidth = oField.GetWidth() + 2;
        else if( oField.GetType() == OFTInteger )
            nBufWidth = 22;
        else if( oField.GetType() == OFTReal )
            nBufWidth = 36;

        papszCurColumn[nOGRField] = (char *) CPLMalloc(nBufWidth+2);
        CPLAssert( ((long) papszCurColumn[nOGRField]) % 2 == 0 );

        if( poSession->Failed(
            OCIDefineByPos( hStatement, &hDefn, poSession->hError,
                            iParm+1, 
                            (ub1 *) papszCurColumn[nOGRField], nBufWidth,
                            SQLT_STR, panCurColumnInd + nOGRField, 
                            NULL, NULL, OCI_DEFAULT ),
            "OCIDefineByPos" ) )
            return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                           SimpleFetchRow()                           */
/************************************************************************/

char **OGROCIStatement::SimpleFetchRow()

{
    int nStatus, i;
    
    if( papszCurImage == NULL )
    {
        papszCurImage = (char **) 
            CPLCalloc(sizeof(char *), nRawColumnCount+1 );
    }

    nStatus = OCIStmtFetch( hStatement, poSession->hError, 1, 
                            OCI_FETCH_NEXT, OCI_DEFAULT );

    if( nStatus == OCI_NO_DATA )
        return NULL;
    else if( poSession->Failed( nStatus, "OCIStmtFetch" ) )
        return NULL;

    for( i = 0; papszCurColumn[i] != NULL; i++ )
    {
        if( panCurColumnInd[i] == OCI_IND_NULL )
            papszCurImage[i] = NULL;
        else
            papszCurImage[i] = papszCurColumn[i];
    }

    return papszCurImage;
}

