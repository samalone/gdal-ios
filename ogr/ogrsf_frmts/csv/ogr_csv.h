/******************************************************************************
 * $Id$
 *
 * Project:  CSV Translator
 * Purpose:  Definition of classes for OGR .csv driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004,  Frank Warmerdam
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
 * Revision 1.1  2004/07/20 19:18:23  warmerda
 * New
 *
 */

#ifndef _OGR_CSV_H_INCLUDED
#define _OGR_CSV_H_INLLUDED

#include "ogrsf_frmts.h"

class OGRCSVDataSource;

/************************************************************************/
/*                             OGRCSVLayer                              */
/************************************************************************/

class OGRCSVLayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;

    FILE               *fpCSV;

    int                 nNextFID;

    int                 bHasFieldNames;

    OGRFeature *        GetNextUnfilteredFeature();

  public:
                        OGRCSVLayer( const char *pszName, FILE *fp );
                        ~OGRCSVLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    OGRGeometry *       GetSpatialFilter() { return NULL; }
    void                SetSpatialFilter( OGRGeometry * ) {}

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                           OGRCSVDataSource                           */
/************************************************************************/

class OGRCSVDataSource : public OGRDataSource
{
    char                *pszName;

    OGRCSVLayer        *poLayer;

  public:
                        OGRCSVDataSource();
                        ~OGRCSVDataSource();

    int                 Open( const char * pszFilename );
    
    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return 1; }
    OGRLayer            *GetLayer( int );
    int                 TestCapability( const char * );
};

/************************************************************************/
/*                             OGRCSVDriver                             */
/************************************************************************/

class OGRCSVDriver : public OGRSFDriver
{
  public:
                ~OGRCSVDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );
    int         TestCapability( const char * );
};


#endif /* ndef _OGR_CSV_H_INCLUDED */
