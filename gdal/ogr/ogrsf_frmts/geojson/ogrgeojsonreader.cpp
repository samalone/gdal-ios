/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONReader class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
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
#include "ogrgeojsonreader.h"
#include "ogrgeojsonutils.h"
#include "ogr_geojson.h"
#include <json.h> // JSON-C

/************************************************************************/
/*                           OGRGeoJSONReader()                         */
/************************************************************************/

OGRGeoJSONReader::OGRGeoJSONReader()
    : poGJObject_( NULL ), poLayer_( NULL ),
        bGeometryPreserve_( true ),
        bAttributesSkip_( false )
{
    // Take a deep breath and get to work.
}

/************************************************************************/
/*                          ~OGRGeoJSONReader()                         */
/************************************************************************/

OGRGeoJSONReader::~OGRGeoJSONReader()
{
    if( NULL != poGJObject_ )
    {
        json_object_put(poGJObject_);
    }

    poGJObject_ = NULL;
    poLayer_ = NULL;
}

/************************************************************************/
/*                           Parse()                                    */
/************************************************************************/

OGRErr OGRGeoJSONReader::Parse( const char* pszText )
{
    if( NULL != pszText )
    {
        json_tokener* jstok = NULL;
        json_object* jsobj = NULL;

        jstok = json_tokener_new();
        jsobj = json_tokener_parse_ex(jstok, pszText, -1);
        if( jstok->err != json_tokener_success)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "GeoJSON parsing error: %s (at offset %d)",
            	      json_tokener_errors[jstok->err], jstok->char_offset);

            return OGRERR_CORRUPT_DATA;
        }
        json_tokener_free(jstok);

        /* JSON tree is shared for while lifetime of the reader object
         * and will be released in the destructor.
         */
        poGJObject_ = jsobj;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ReadLayer()                                */
/************************************************************************/

OGRGeoJSONLayer* OGRGeoJSONReader::ReadLayer( const char* pszName )
{
    CPLAssert( NULL == poLayer_ );
    bool bSuccess = false;

    if( NULL == poGJObject_ )
    {
        CPLDebug( "GeoJSON",
                  "Missing parset GeoJSON data. Forgot to call Parse()?" );
        return NULL;
    }
        
    poLayer_ = new OGRGeoJSONLayer( pszName, NULL,
                                   OGRGeoJSONLayer::DefaultGeometryType,
                                   NULL );

    if( !GenerateLayerDefn() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Layer schema generation failed." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Translate single geometry-only Feature object.                  */
/* -------------------------------------------------------------------- */
    GeoJSONObject::Type objType = GetType( poGJObject_ );

    if( GeoJSONObject::ePoint == objType
        || GeoJSONObject::eLineString == objType
        || GeoJSONObject::ePolygon == objType
        || GeoJSONObject::eGeometryCollection == objType )
    {
        OGRGeometry* poGeometry = NULL;
        poGeometry = ReadGeometry( poGJObject_ );
        bSuccess = AddFeature( poGeometry );
        CPLAssert( bSuccess );
    }
/* -------------------------------------------------------------------- */
/*      Translate single but complete Feature object.                   */
/* -------------------------------------------------------------------- */
    else if( GeoJSONObject::eFeature == objType )
    {
        OGRFeature* poFeature = NULL;
        poFeature = ReadFeature( poGJObject_ );
        bSuccess = AddFeature( poFeature );
        CPLAssert( bSuccess );
    }
/* -------------------------------------------------------------------- */
/*      Translate multi-feature FeatureCollection object.               */
/* -------------------------------------------------------------------- */
    else if( GeoJSONObject::eFeatureCollection == objType )
    {
        OGRGeoJSONLayer* poThisLayer = NULL;
        poThisLayer = ReadFeatureCollection( poGJObject_ );
        CPLAssert( poLayer_ == poThisLayer );
    }
    else
    {
        CPLAssert( !"SHOULD NEVER GET HERE" );
    }

/* -------------------------------------------------------------------- */
/*      Read spatial reference definition.                              */
/* -------------------------------------------------------------------- */
    // TODO: Move this code to separate function
    OGRSpatialReference* poSRS = NULL;

    json_object* poObjSrs = FindMemberByName( poGJObject_, "crs" );
    if( NULL != poObjSrs )
    {
        json_object* poObjSrsType = FindMemberByName( poObjSrs, "type" );
        const char* pszSrsType = json_object_get_string( poObjSrsType );

        // TODO: Add URL and URN types support

        if( EQUALN( pszSrsType, "EPSG", 4 ) )
        {
            json_object* poObjSrsProps = FindMemberByName( poObjSrs, "properties" );
            CPLAssert( NULL != poObjSrsProps );

            json_object* poObjCode = FindMemberByName( poObjSrsProps, "code" );
            CPLAssert( NULL != poObjCode );

            int nEPSG = json_object_get_int( poObjCode );

            poSRS = new OGRSpatialReference();
            if( OGRERR_NONE != poSRS->importFromEPSG( nEPSG ) )
            {
                delete poSRS;
                poSRS = NULL;
            }
        }
    }

    // If NULL, WGS84 is set.
    poLayer_->SetSpatialRef( poSRS );
    delete poSRS;


    // TODO: FeatureCollection

    return poLayer_;
}

/************************************************************************/
/*                  PRIVATE FUNCTIONS IMPLEMENTATION                    */
/************************************************************************/

void OGRGeoJSONReader::SetPreserveGeometryType( bool bPreserve )
{
    bGeometryPreserve_ = bPreserve;
}

void OGRGeoJSONReader::SetSkipAttributes( bool bSkip )
{
    bAttributesSkip_ = bSkip;
}

/************************************************************************/
/*                           FindByName()                               */
/************************************************************************/

json_object* OGRGeoJSONReader::FindMemberByName(json_object* poObj,
                                                const char* pszName )
{
    if( NULL == pszName || NULL == poObj)
        return NULL;

    json_object* poTmp = poObj;

    json_object_iter it;
    json_object_object_foreachC(poTmp, it)
    {
        if( EQUAL( it.key, pszName ) )
            return it.val;
    }

    return NULL;
}

/************************************************************************/
/*                           GetType()                                  */
/************************************************************************/

GeoJSONObject::Type OGRGeoJSONReader::GetType( json_object* poObj )
{
    if( NULL == poObj )
        return GeoJSONObject::eUnknown;

    json_object* poObjType = NULL;
    poObjType = FindMemberByName( poObj, "type" );
    if( NULL == poObjType )
        return GeoJSONObject::eUnknown;

    const char* name = json_object_get_string( poObjType );
    if( EQUAL( name, "Point" ) )
        return GeoJSONObject::ePoint;
    else if( EQUAL( name, "LineString" ) )
        return GeoJSONObject::eLineString;
    else if( EQUAL( name, "Polygon" ) )
        return GeoJSONObject::ePolygon;
    else if( EQUAL( name, "MultiPoint" ) )
        return GeoJSONObject::eMultiPoint;
    else if( EQUAL( name, "MultiLineString" ) )
        return GeoJSONObject::eMultiLineString;
    else if( EQUAL( name, "MultiPolygon" ) )
        return GeoJSONObject::eMultiPolygon;
    else if( EQUAL( name, "GeometryCollection" ) )
        return GeoJSONObject::eGeometryCollection;
    else if( EQUAL( name, "Feature" ) )
        return GeoJSONObject::eFeature;
    else if( EQUAL( name, "FeatureCollection" ) )
        return GeoJSONObject::eFeatureCollection;
    else
        return GeoJSONObject::eUnknown;
}

/************************************************************************/
/*                           GenerateFeatureDefn()                      */
/************************************************************************/


bool OGRGeoJSONReader::GenerateLayerDefn()
{
    CPLAssert( NULL != poGJObject_ );
    CPLAssert( NULL != poLayer_->GetLayerDefn() );
    CPLAssert( 0 == poLayer_->GetLayerDefn()->GetFieldCount() );

    bool bSuccess = true;
    OGRFeatureDefn* poDefn = NULL;

    if( bAttributesSkip_ )
        return true;

    GeoJSONObject::Type objType = GetType( poGJObject_ );
    if( GeoJSONObject::eFeature == objType )
    {
        bSuccess = GenerateFeatureDefn( poGJObject_ );
    }
    else if( GeoJSONObject::eFeatureCollection == objType )
    {
        json_object* poObjFeatures = NULL;
        poObjFeatures = FindMemberByName( poGJObject_, "features" );
        if( NULL != poObjFeatures
            && json_type_array == json_object_get_type( poObjFeatures ) )
        {
            json_object* poObjFeature = NULL;
            const int nFeatures = json_object_array_length( poObjFeatures );
            for( int i = 0; i < nFeatures; ++i )
            {
                poObjFeature = json_object_array_get_idx( poObjFeatures, i );
                if( !GenerateFeatureDefn( poObjFeature ) )
                {
                    CPLDebug( "GeoJSON", "Create feature schema failure." );
                    bSuccess = false;
                }
            }
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid FeatureCollection object. "
                      "Missing \'features\' member." );
            bSuccess = false;
        }
    }

    return bSuccess;
}
bool OGRGeoJSONReader::GenerateFeatureDefn( json_object* poObj )
{
    OGRFeatureDefn* poDefn = poLayer_->GetLayerDefn();
    CPLAssert( NULL != poDefn );

    bool bSuccess = false;

    json_object* poObjProps = NULL;
    poObjProps = FindMemberByName( poObj, "properties" );
    if( NULL != poObjProps )
    {
        json_object_iter it;
        json_object_object_foreachC( poObjProps, it )
        {
            if( -1 == poDefn->GetFieldIndex( it.key ) )
            {
                OGRFieldDefn fldDefn( it.key,
                    GeoJSONPropertyToFieldType( it.val) );
                //if( OFTString == fldDefn.GetType() ) fldDefn.SetWidth(256);
                poDefn->AddFieldDefn( &fldDefn );
            }
        }
        bSuccess = true; // SUCCESS
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid Feature object. "
                  "Missing \'properties\' member." );
    }

    return bSuccess;
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

bool OGRGeoJSONReader::AddFeature( OGRGeometry* poGeometry )
{
    bool bAdded = false;

    // TODO: Should we check if geometry is of type of 
    //       wkbGeometryCollection ?

    if( NULL != poGeometry )
    {
        OGRFeature* poFeature = NULL;
        poFeature = new OGRFeature( poLayer_->GetLayerDefn() );
        poFeature->SetGeometryDirectly( poGeometry );

        bAdded = AddFeature( poFeature );
    }
    
    return bAdded;
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

bool OGRGeoJSONReader::AddFeature( OGRFeature* poFeature )
{
    bool bAdded = false;
  
    if( NULL != poFeature )
    {
        if( OGRERR_NONE == poLayer_->CreateFeature( poFeature ) )
        {
            bAdded = true;
        }
        delete poFeature;
    }

    return bAdded;
}

/************************************************************************/
/*                           ReadGeometry()                             */
/************************************************************************/

OGRGeometry* OGRGeoJSONReader::ReadGeometry( json_object* poObj )
{
    OGRGeometry* poGeometry = NULL;

    GeoJSONObject::Type objType = GetType( poObj );
    if( GeoJSONObject::ePoint == objType )
        poGeometry = ReadPoint( poObj );
    else if( GeoJSONObject::eLineString == objType )
        poGeometry = ReadLineString( poObj );
    else if( GeoJSONObject::ePolygon == objType )
        poGeometry = ReadPolygon( poObj );
    else if( GeoJSONObject::eGeometryCollection == objType )
        poGeometry = ReadGeometryCollection( poObj );

    // TODO: Add translation for Multi* geometry types

/* -------------------------------------------------------------------- */
/*      Wrap geometry with GeometryCollection as a common denominator.  */
/*      Sometimes a GeoJSON text may consist of objects of different    */
/*      geometry types. Users may request wrapping all geometries with  */
/*      OGRGeometryCollection type by using option                      */
/*      GEOMETRY_AS_COLLECTION=NO|YES (YES is default).                 */
/* -------------------------------------------------------------------- */
    if( NULL != poGeometry )
    {
        if( !bGeometryPreserve_ 
            && wkbGeometryCollection != poGeometry->getGeometryType() )
        {
            OGRGeometryCollection* poMetaGeometry = NULL;
            poMetaGeometry = new OGRGeometryCollection();
            poMetaGeometry->addGeometryDirectly( poGeometry );
            return poMetaGeometry;
        }
    }

    return poGeometry;
}

/************************************************************************/
/*                           ReadRawPoint()                             */
/************************************************************************/

bool OGRGeoJSONReader::ReadRawPoint( json_object* poObj, OGRPoint& point )
{
    if( json_type_array == json_object_get_type( poObj ) ) 
    {
        const int nSize = json_object_array_length( poObj );

        if( nSize != GeoJSONObject::eMinCoordinateDimension
            && nSize != GeoJSONObject::eMaxCoordinateDimension )
        {
            CPLDebug( "GeoJSON",
                      "Invalid coord dimension. Only 2D and 3D supported." );
            return false;
        }

        json_object* poObjCoord = NULL;

        // Read X coordinate
        poObjCoord = json_object_array_get_idx( poObj, 0 );
        
        if (json_type_double != json_object_get_type(poObjCoord) ) {
            CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid Point object. Type is not double for \'%s\'.",json_object_to_json_string(poObj) );
            return false;
        }
        point.setX(json_object_get_double( poObjCoord ));

        // Read Y coordiante
        poObjCoord = json_object_array_get_idx( poObj, 1 );
        
        if (json_type_double != json_object_get_type(poObjCoord) ) {
            CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid Point object. Type is not double for \'%s\'.",json_object_to_json_string(poObj) );
            return false;
        }
        point.setY(json_object_get_double( poObjCoord ));

        // Read Z coordinate
        if( nSize == GeoJSONObject::eMaxCoordinateDimension )
        {
            // Don't *expect* mixed-dimension geometries, although the 
            // spec doesn't explicitly forbid this.
            poObjCoord = json_object_array_get_idx( poObj, 2 );
            if (json_type_double != json_object_get_type(poObjCoord) ) {
                CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid Point object. Type is not double for \'%s\'.",json_object_to_json_string(poObj) );
                return false;
            }
            point.setZ(json_object_get_double( poObjCoord ));

        } else {
            point.flattenTo2D();
        }
        
        return true;
    }
    
    return false;
}

/************************************************************************/
/*                           ReadPoint()                                */
/************************************************************************/

OGRPoint* OGRGeoJSONReader::ReadPoint( json_object* poObj )
{
    OGRPoint* poPoint = NULL;

    json_object* poObjCoords = NULL;
    poObjCoords = FindMemberByName( poObj, "coordinates" );
    if( NULL == poObjCoords )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid Point object. Missing \'coordinates\' member." );
        return NULL;
    }

    OGRPoint pt;
    if( !ReadRawPoint( poObjCoords, pt ) )
    {
        CPLDebug( "GeoJSON", "Point: raw point parsing failure." );
        return NULL;
    }

    if (pt.getCoordinateDimension() == 2) {
        poPoint = new OGRPoint( pt.getX(), pt.getY());
    } else {
        poPoint = new OGRPoint( pt.getX(), pt.getY(), pt.getZ() );
    }
    return poPoint;
}

/************************************************************************/
/*                           ReadLineString()                           */
/************************************************************************/

OGRLineString* OGRGeoJSONReader::ReadLineString( json_object* poObj )
{
    OGRLineString* poLine = NULL;

    json_object* poObjPoints = NULL;
    poObjPoints = FindMemberByName( poObj, "coordinates" );
    if( NULL == poObjPoints )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid LineString object. "
                  "Missing \'coordinates\' member." );
        return NULL;
    }

    if( json_type_array == json_object_get_type( poObjPoints ) )
    {
        const int nPoints = json_object_array_length( poObjPoints );

        poLine = new OGRLineString();
        poLine->setNumPoints( nPoints );

        for( int i = 0; i < nPoints; ++i)
        {
            json_object* poObjCoords = NULL;
            poObjCoords = json_object_array_get_idx( poObjPoints, i );
            
            OGRPoint pt;
            if( !ReadRawPoint( poObjCoords, pt ) )
            {
                delete poLine;
                CPLDebug( "GeoJSON",
                          "LineString: raw point parsing failure." );
                return NULL;
            }
            if (pt.getCoordinateDimension() == 2) {
                poLine->setPoint( i, pt.getX(), pt.getY());
            } else {
                poLine->setPoint( i, pt.getX(), pt.getY(), pt.getZ() );
            }
            
        }
    }

    return poLine;
}

/************************************************************************/
/*                           ReadLineRing()                             */
/************************************************************************/

OGRLinearRing* OGRGeoJSONReader::ReadLinearRing( json_object* poObj )
{
    OGRLinearRing* poRing = NULL;

    if( json_type_array == json_object_get_type( poObj ) )
    {
        const int nPoints = json_object_array_length( poObj );

        poRing= new OGRLinearRing();
        poRing->setNumPoints( nPoints );

        for( int i = 0; i < nPoints; ++i)
        {
            json_object* poObjCoords = NULL;
            poObjCoords = json_object_array_get_idx( poObj, i );
            
            OGRPoint pt;
            if( !ReadRawPoint( poObjCoords, pt ) )
            {
                delete poRing;
                CPLDebug( "GeoJSON",
                          "LinearRing: raw point parsing failure." );
                return NULL;
            }
            
            if (pt.getCoordinateDimension() == 2) {
                poRing->setPoint( i, pt.getX(), pt.getY());
            } else {
                poRing->setPoint( i, pt.getX(), pt.getY(), pt.getZ() );
            }
            
            
        }
    }

    return poRing;
}

/************************************************************************/
/*                           ReadPolygon()                              */
/************************************************************************/

OGRPolygon* OGRGeoJSONReader::ReadPolygon( json_object* poObj )
{
    OGRPolygon* poPolygon = NULL;

    json_object* poObjRings = NULL;
    poObjRings = FindMemberByName( poObj, "coordinates" );
    if( NULL == poObjRings )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid Polygon object. "
                  "Missing \'geometries\' member." );
        return NULL;
    }

    if( json_type_array == json_object_get_type( poObjRings ) )
    {
        const int nRings = json_object_array_length( poObjRings );
        if( nRings > 0 )
        {
            json_object* poObjPoints = NULL;
            poObjPoints = json_object_array_get_idx( poObjRings, 0 );

            OGRLinearRing* poRing = ReadLinearRing( poObjPoints );
            if( NULL != poRing )
            {
                poPolygon = new OGRPolygon();
                poPolygon->addRingDirectly( poRing );
            }

            for( int i = 1; i < nRings && NULL != poPolygon; ++i )
            {
                poObjPoints = json_object_array_get_idx( poObjRings, i );
                OGRLinearRing* poRing = ReadLinearRing( poObjPoints );
                if( NULL != poRing )
                {
                    poPolygon->addRingDirectly( poRing );
                }
            }
        }
    }

    return poPolygon;
}

/************************************************************************/
/*                           ReadGeometryCollection()                   */
/************************************************************************/

OGRGeometryCollection*
OGRGeoJSONReader::ReadGeometryCollection( json_object* poObj )
{
    OGRGeometry* poGeometry = NULL;
    OGRGeometryCollection* poCollection = NULL;

    json_object* poObjGeoms = NULL;
    poObjGeoms = FindMemberByName( poObj, "geometries" );
    if( NULL == poObjGeoms )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid GeometryCollection object. "
                  "Missing \'geometries\' member." );
        return NULL;
    }

    if( json_type_array == json_object_get_type( poObjGeoms ) )
    {
        GeoJSONObject::Type objType = GeoJSONObject::eUnknown;
        const int nGeoms = json_object_array_length( poObjGeoms );
        if( nGeoms > 0 )
        {
            poCollection = new OGRGeometryCollection();
        }

        json_object* poObjGeom = NULL;
        for( int i = 0; i < nGeoms; ++i )
        {
            poObjGeom = json_object_array_get_idx( poObjGeoms, i );

            objType = GetType( poObjGeom );
            if( GeoJSONObject::ePoint == objType )
                poGeometry = ReadPoint( poObjGeom );
            else if( GeoJSONObject::eLineString == objType )
                poGeometry = ReadLineString( poObjGeom );
            else if( GeoJSONObject::ePolygon == objType )
                poGeometry = ReadPolygon( poObjGeom );

            if( NULL != poGeometry )
            {
                poCollection->addGeometryDirectly( poGeometry );
            }
        }
    }

    return poCollection;
}

/************************************************************************/
/*                           ReadFeature()                              */
/************************************************************************/

OGRFeature* OGRGeoJSONReader::ReadFeature( json_object* poObj )
{
    CPLAssert( NULL != poObj );
    CPLAssert( NULL != poLayer_ );

    OGRFeature* poFeature = NULL;
    poFeature = new OGRFeature( poLayer_->GetLayerDefn() );

/* -------------------------------------------------------------------- */
/*      Translate properties values to feature attributes.              */
/* -------------------------------------------------------------------- */
    CPLAssert( NULL != poFeature );

    json_object* poObjProps = NULL;
    poObjProps = FindMemberByName( poObj, "properties" );
    if( !bAttributesSkip_ && NULL != poObjProps )
    {
        int nField = -1;
        OGRFieldDefn* poFieldDefn = NULL;
        json_object_iter it;
        json_object_object_foreachC( poObjProps, it )
        {
            nField = poFeature->GetFieldIndex(it.key);
            poFieldDefn = poFeature->GetFieldDefnRef(nField);
            CPLAssert( NULL != poFieldDefn );

            //poFeature->UnsetField( nField );
            if( OFTInteger == poFieldDefn->GetType() )
                poFeature->SetField( nField, json_object_get_int(it.val) );
            else if( OFTReal == poFieldDefn->GetType() )
                poFeature->SetField( nField, json_object_get_double(it.val) );
            else
                poFeature->SetField( nField, json_object_get_string(it.val) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate geometry sub-object of GeoJSON Feature.               */
/* -------------------------------------------------------------------- */
    json_object* poObjGeom = NULL;
    poObjGeom = FindMemberByName( poObj, "geometry" );
    if( NULL != poObjGeom )
    {
        OGRGeometry* poGeometry = ReadGeometry( poObjGeom );
        if( NULL != poGeometry )
        {
            poFeature->SetGeometryDirectly( poGeometry );
        }
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid Feature object. "
                  "Missing \'geometry\' member." );
        return NULL;
    }

    return poFeature;
}

/************************************************************************/
/*                           ReadFeatureCollection()                    */
/************************************************************************/

OGRGeoJSONLayer*
OGRGeoJSONReader::ReadFeatureCollection( json_object* poObj )
{
    CPLAssert( NULL != poLayer_ );

    json_object* poObjFeatures = NULL;
    poObjFeatures = FindMemberByName( poObj, "features" );
    if( NULL == poObjFeatures )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid FeatureCollection object. "
                  "Missing \'features\' member." );
        return NULL;
    }

    if( json_type_array == json_object_get_type( poObjFeatures ) )
    {
        bool bAdded = false;
        OGRFeature* poFeature = NULL;
        json_object* poObjFeature = NULL;

        const int nFeatures = json_object_array_length( poObjFeatures );
        for( int i = 0; i < nFeatures; ++i )
        {
            poObjFeature = json_object_array_get_idx( poObjFeatures, i );
            poFeature = OGRGeoJSONReader::ReadFeature( poObjFeature );
            bAdded = AddFeature( poFeature );
            CPLAssert( bAdded );
        }
        CPLAssert( nFeatures == poLayer_->GetFeatureCount() );
    }

    // We're returning class member to follow the same pattern of
    // Read* functions call convention.
    CPLAssert( NULL != poLayer_ );
    return poLayer_;
}

