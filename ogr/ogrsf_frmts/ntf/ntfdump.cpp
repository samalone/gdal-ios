/******************************************************************************
 * $Id$
 *
 * Project:  NTF Translator
 * Purpose:  Simple test harnass.
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
 * Revision 1.1  1999/08/28 03:13:35  warmerda
 * New
 *
 */

#include "ntf.h"
#include "cpl_vsi.h"

static void NTFDump( const char * pszFile );
static void NTFCount( const char * pszFile );

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    const char	*pszMode = "-d";

    if( argc == 1 )
        printf( "Usage: ntfdump [-d] [-c] files\n" );
    
    for( int i = 1; i < argc; i++ )
    {
        if( argv[i][0] == '-' )
            pszMode = argv[i];
        else if( EQUAL(pszMode,"-d") )
            NTFDump( argv[i] );
        else if( EQUAL(pszMode,"-c") )
            NTFCount( argv[i] );
    }

    return 0;
}

/************************************************************************/
/*                              NTFCount()                              */
/************************************************************************/

static void NTFCount( const char * pszFile )

{
    FILE      *fp;
    NTFRecord *poRecord = NULL;
    int       anCount[100], i;

    for( i = 0; i < 100; i++ )
        anCount[i] = 0;

    fp = VSIFOpen( pszFile, "r" );
    if( fp == NULL )
        return;
    
    do {
        if( poRecord != NULL )
            delete poRecord;

        poRecord = new NTFRecord( fp );
        anCount[poRecord->GetType()]++;

    } while( poRecord->GetType() != 99 );

    VSIFClose( fp );

    printf( "\nReporting on: %s\n", pszFile );
    for( i = 0; i < 100; i++ )
    {
        if( anCount[i] > 0 )
            printf( "Found %d records of type %d\n", anCount[i], i );
    }
}

/************************************************************************/
/*                              NTFDump()                               */
/************************************************************************/

static void NTFDump( const char * pszFile )

{
    OGRFeature	       *poFeature;
    OGRNTFDataSource   oDS;

    if( !oDS.Open( pszFile ) )
        return;

    while( (poFeature = oDS.GetNextFeature()) != NULL )
    {
        printf( "-------------------------------------\n" );
        poFeature->DumpReadable( stdout );
    }
}
