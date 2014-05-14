/******************************************************************************
 * $Id$
 *
 * Project:  GeoPackage Translator
 * Purpose:  Definition of classes for OGR GeoPackage driver.
 * Author:   Paul Ramsey, pramsey@boundlessgeo.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
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

#ifndef _OGR_GEOPACKAGE_H_INCLUDED
#define _OGR_GEOPACKAGE_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_sqlite.h"

#define UNDEFINED_SRID 0

/************************************************************************/
/*                           OGRGeoPackageDriver                        */
/************************************************************************/

class OGRGeoPackageDriver : public OGRSFDriver
{
    public:
                            ~OGRGeoPackageDriver();
        const char*         GetName();
        OGRDataSource*      Open( const char *, int );
        OGRDataSource*      CreateDataSource( const char * pszFilename, char **papszOptions );
        OGRErr              DeleteDataSource( const char * pszFilename );
        int                 TestCapability( const char * );
};


/************************************************************************/
/*                           OGRGeoPackageDataSource                    */
/************************************************************************/

class OGRGeoPackageDataSource : public OGRSQLiteBaseDataSource
{
    OGRLayer**          m_papoLayers;
    int                 m_nLayers;
    int                 m_bUtf8;

    public:
                            OGRGeoPackageDataSource();
                            ~OGRGeoPackageDataSource();

        virtual int         GetLayerCount() { return m_nLayers; }
        int                 Open( const char * pszFilename, int bUpdate );
        int                 Create( const char * pszFilename, char **papszOptions );
        OGRLayer*           GetLayer( int iLayer );
        int                 DeleteLayer( int iLayer );
        OGRLayer*           CreateLayer( const char * pszLayerName,
                                         OGRSpatialReference * poSpatialRef,
                                         OGRwkbGeometryType eGType,
                                         char **papszOptions );
        int                 TestCapability( const char * );

        virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect );
        virtual void        ReleaseResultSet( OGRLayer * poLayer );

        int                 GetSrsId( const OGRSpatialReference * poSRS );
        const char*         GetSrsName( const OGRSpatialReference * poSRS );
        OGRSpatialReference* GetSpatialRef( int iSrsId );
        virtual int         GetUTF8() { return m_bUtf8; }
        OGRErr              AddColumn( const char * pszTableName, 
                                       const char * pszColumnName, 
                                       const char * pszColumnType );

    private:
    
        OGRErr              PragmaCheck(const char * pszPragma, const char * pszExpected, int nRowsExpected);
        bool                CheckApplicationId(const char * pszFileName);
        OGRErr              SetApplicationId();
    
};


/************************************************************************/
/*                           OGRGeoPackageLayer                         */
/************************************************************************/

class OGRGeoPackageLayer : public OGRLayer
{
  protected:
    OGRGeoPackageDataSource *m_poDS;

    OGRFeatureDefn*      m_poFeatureDefn;
    int                  iNextShapeId;

    sqlite3_stmt        *m_poQueryStatement;
    int                  bDoStep;

    char                *m_pszFidColumn;

    int                 iFIDCol;
    int                 iGeomCol;
    int                *panFieldOrdinals;

    void                ClearStatement();
    virtual OGRErr      ResetStatement() = 0;
    
    void                BuildFeatureDefn( const char *pszLayerName,
                                           sqlite3_stmt *hStmt );

    OGRFeature*         TranslateFeature(sqlite3_stmt* hStmt);

  public:

                        OGRGeoPackageLayer(OGRGeoPackageDataSource* poDS);
                        ~OGRGeoPackageLayer();
    /************************************************************************/
    /* OGR API methods */

    OGRFeature*         GetNextFeature();
    const char*         GetFIDColumn(); 
    void                ResetReading();
    int                 TestCapability( const char * );
    OGRFeatureDefn*     GetLayerDefn() { return m_poFeatureDefn; }
};

/************************************************************************/
/*                        OGRGeoPackageTableLayer                       */
/************************************************************************/

class OGRGeoPackageTableLayer : public OGRGeoPackageLayer
{
    char*                       m_pszTableName;
    int                         m_iSrs;
    OGREnvelope*                m_poExtent;
    CPLString                   m_soColumns;
    CPLString                   m_soFilter;
    OGRBoolean                  m_bExtentChanged;
    sqlite3_stmt*               m_poUpdateStatement;
    sqlite3_stmt*               m_poInsertStatement;

    virtual OGRErr      ResetStatement();
    
    public:
    
                        OGRGeoPackageTableLayer( OGRGeoPackageDataSource *poDS,
                                            const char * pszTableName );
                        ~OGRGeoPackageTableLayer();

    /************************************************************************/
    /* OGR API methods */
                        
    int                 TestCapability( const char * );
    OGRErr              CreateField( OGRFieldDefn *poField, int bApproxOK = TRUE );
    void                ResetReading();
	OGRErr				CreateFeature( OGRFeature *poFeater );
    OGRErr              SetFeature( OGRFeature *poFeature );
    OGRErr              DeleteFeature(long nFID);
    OGRErr              SetAttributeFilter( const char *pszQuery );
    OGRErr              SyncToDisk();
    OGRFeature*         GetFeature(long nFID);
    OGRErr              StartTransaction();
    OGRErr              CommitTransaction();
    OGRErr              RollbackTransaction();
    int                 GetFeatureCount( int );
    OGRErr              GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
    
    // void                SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn );

    OGRErr              ReadTableDefinition(int bIsSpatial);

    /************************************************************************/
    /* GPKG methods */
    
    private:
    
    OGRErr              UpdateExtent( const OGREnvelope *poExtent );
    OGRErr              SaveExtent();
    OGRErr              BuildColumns();
    OGRBoolean          IsGeomFieldSet( OGRFeature *poFeature );
    CPLString           FeatureGenerateUpdateSQL( OGRFeature *poFeature );
    CPLString           FeatureGenerateInsertSQL( OGRFeature *poFeature );
    OGRErr              FeatureBindUpdateParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt );
    OGRErr              FeatureBindInsertParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt );
    OGRErr              FeatureBindParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt, int *pnColCount );

};

/************************************************************************/
/*                         OGRGeoPackageSelectLayer                     */
/************************************************************************/

class OGRGeoPackageSelectLayer : public OGRGeoPackageLayer
{
    CPLString           osSQLBase;

    int                 bEmptyLayer;

    virtual OGRErr      ResetStatement();

  public:
                        OGRGeoPackageSelectLayer( OGRGeoPackageDataSource *, 
                                              CPLString osSQL,
                                              sqlite3_stmt *,
                                              int bUseStatementForGetNextFeature,
                                              int bEmptyLayer );

    virtual void        ResetReading();

    virtual OGRFeature *GetNextFeature();
    virtual int         GetFeatureCount( int );
};


#endif /* _OGR_GEOPACKAGE_H_INCLUDED */