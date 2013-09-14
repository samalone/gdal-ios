/******************************************************************************
 * $Id$
 *
 * Project:  SOSI Translator
 * Purpose:  Implements OGRSOSIDriver.
 * Author:   Thomas Hirsch, <thomas.hirsch statkart no>
 *
 ******************************************************************************
 * Copyright (c) 2010, Thomas Hirsch
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

#ifndef _OGR_SOSI_H_INCLUDED
#define _OGR_SOSI_H_INCLUDED

#include "ogrsf_frmts.h"
#include "fyba.h"
#include <map>

typedef std::map<CPLString, CPLString> S2S;
typedef std::map<CPLString, unsigned int> S2I;

void RegisterOGRSOSI();

class ORGSOSILayer;      /* defined below */
class OGRSOSIDataSource; /* defined below */

/************************************************************************
 *                           OGRSOSIDriver                              *
 * OGRSOSIDriver is the main driver class. It does not much, except     *
 * reporting on its capabilities and opening a single data source       *
 * currently                                                            *
 ************************************************************************/

class OGRSOSIDriver : public OGRSFDriver {
public:

    OGRSOSIDriver();
    ~OGRSOSIDriver();

    const char    *GetName();
    OGRDataSource *Open( const char *, int );
    int            TestCapability( const char * );
    OGRDataSource *CreateDataSource( const char *pszName, char **papszOptions = NULL);
};

/************************************************************************
 *                           OGRSOSILayer                               *
 * OGRSOSILayer reports all the OGR Features generated by the data      *
 * source, in an orderly fashion.                                       *
 ************************************************************************/

class OGRSOSILayer : public OGRLayer {
    int                 nNextFID;

    OGRSOSIDataSource  *poParent;   /* used to call methods from data source */
    LC_FILADM          *poFileadm;  /* ResetReading needs to refer to the file struct */
    OGRFeatureDefn     *poFeatureDefn;  /* the common definition of all features returned by this layer  */
    S2I                *poHeaderDefn;

    LC_SNR_ADM          oSnradm;
    LC_BGR              oNextSerial;  /* used by FYBA to iterate through features */
    LC_BGR             *poNextSerial;

public:
    OGRSOSILayer( OGRSOSIDataSource *poPar, OGRFeatureDefn *poFeatDefn, LC_FILADM *poFil, S2I *poHeadDefn);
    ~OGRSOSILayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();
    OGRFeatureDefn *    GetLayerDefn();
    OGRErr              CreateField(OGRFieldDefn *poField, int bApproxOK=TRUE);
    OGRErr              CreateFeature(OGRFeature *poFeature);
    int                 TestCapability( const char * );
};

/************************************************************************
 *                           OGRSOSIDataSource                          *
 * OGRSOSIDataSource reads a SOSI file, prebuilds the features, and     *
 * creates one OGRSOSILayer per geometry type                           *
 ************************************************************************/
class OGRSOSIDataSource : public OGRDataSource {
    char                *pszName;
    OGRSOSILayer        **papoLayers;
    int                 nLayers;
    
#define MODE_READING 0
#define MODE_WRITING 1    
    int                 nMode;

    void                buildOGRPoint(long nSerial);
    void                buildOGRLineString(int nNumCoo, long nSerial);
    void                buildOGRMultiPoint(int nNumCoo, long nSerial);

public:

    OGRSpatialReference *poSRS;
    const char          *pszEncoding;
    unsigned int        nNumFeatures;
    OGRGeometry         **papoBuiltGeometries;  /* OGRSOSIDataSource prebuilds some features upon opening, te be used
                                                * by the more complex geometries later. */
    //FYBA specific
    LC_BASEADM          *poBaseadm;
    LC_FILADM           *poFileadm;

    S2I                 *poPolyHeaders;   /* Contain the header definitions of the four feature layers */
    S2I                 *poPointHeaders;
    S2I                 *poCurveHeaders;
    S2I                 *poTextHeaders;

    OGRSOSIDataSource();
    ~OGRSOSIDataSource();

    int                 Open  ( const char * pszFilename, int bUpdate);
    int                 Create( const char * pszFilename );
    const char          *GetName() {
        return pszName;
    }
    int                 GetLayerCount() {
        return nLayers;
    }
    OGRLayer            *GetLayer( int );
    OGRLayer            *CreateLayer( const char *pszName, OGRSpatialReference  *poSpatialRef=NULL, OGRwkbGeometryType eGType=wkbUnknown, char **papszOptions=NULL);
    int                 TestCapability( const char * );
};

#endif /* _OGR_SOSI_H_INCLUDED */
