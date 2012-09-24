#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR OSM driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import os
import sys
import ogr
import osr
import gdal

sys.path.append( '../pymod' )

import gdaltest
import ogrtest

###############################################################################
# Test .pbf

def ogr_osm_1(filename = 'data/test.pbf'):

    try:
        drv = ogr.GetDriverByName('OSM')
    except:
        drv = None
    if drv is None:
        return 'skip'

    ds = ogr.Open(filename)
    if ds is None:
        if filename == 'data/test.osm' and gdal.GetLastErrorMsg().find('OSM XML detected, but Expat parser not available') == 0:
            return 'skip'

        gdaltest.post_reason('fail')
        return 'fail'

    # Test points
    lyr = ds.GetLayer('points')
    if lyr.GetGeomType() != ogr.wkbPoint:
        gdaltest.post_reason('fail')
        return 'fail'

    sr = lyr.GetSpatialRef()
    if sr.ExportToWkt().find('GEOGCS["WGS 84",DATUM["WGS_1984",') != 0 and \
       sr.ExportToWkt().find('GEOGCS["GCS_WGS_1984",DATUM["WGS_1984"') != 0 :
        gdaltest.post_reason('fail')
        print(sr.ExportToWkt())
        return 'fail'

    if filename == 'data/test.osm':
        if lyr.GetExtent() != (2.0, 3.0, 49.0, 50.0):
            gdaltest.post_reason('fail')
            print(lyr.GetExtent())
            return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '3':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT (3.0 49.5)')) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # Test lines
    lyr = ds.GetLayer('lines')
    if lyr.GetGeomType() != ogr.wkbLineString:
        gdaltest.post_reason('fail')
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '1':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING (2 49,3 50)')) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '6':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING (2 49,3 49,3 50,2 50,2 49)')) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # Test multipolygons
    lyr = ds.GetLayer('multipolygons')
    if filename == 'tmp/ogr_osm_3':
        if lyr.GetGeomType() != ogr.wkbPolygon:
            gdaltest.post_reason('fail')
            return 'fail'
    else:
        if lyr.GetGeomType() != ogr.wkbMultiPolygon:
            gdaltest.post_reason('fail')
            return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '1':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if feat.GetFieldAsString('natural') != 'forest':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if filename == 'tmp/ogr_osm_3':
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POLYGON ((2 49,2 50,3 50,3 49,2 49),(2.1 49.1,2.2 49.1,2.2 49.2,2.1 49.2,2.1 49.1))')) != 0:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'
    else:
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('MULTIPOLYGON (((2 49,3 49,3 50,2 50,2 49),(2.1 49.1,2.2 49.1,2.2 49.2,2.1 49.2,2.1 49.1)))')) != 0:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '5':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if feat.GetFieldAsString('natural') != 'wood':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_way_id') != '8':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if feat.GetFieldAsString('name') != 'standalone_polygon':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # Test multilinestrings
    lyr = ds.GetLayer('multilinestrings')
    if filename == 'tmp/ogr_osm_3':
        if lyr.GetGeomType() != ogr.wkbLineString:
            gdaltest.post_reason('fail')
            return 'fail'
    else:
        if lyr.GetGeomType() != ogr.wkbMultiLineString:
            gdaltest.post_reason('fail')
            return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '3':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if filename == 'tmp/ogr_osm_3':
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING (2 49,3 50)')) != 0:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'
    else:
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('MULTILINESTRING ((2 49,3 50))')) != 0:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'

    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # Test other_relations
    lyr = ds.GetLayer('other_relations')
    if filename == 'tmp/ogr_osm_3':
        if lyr is not None:
            gdaltest.post_reason('fail')
            return 'fail'
    else:
        if lyr.GetGeomType() != ogr.wkbGeometryCollection:
            gdaltest.post_reason('fail')
            return 'fail'

        feat = lyr.GetNextFeature()
        if feat.GetFieldAsString('osm_id') != '4':
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'

        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (POINT (2 49),LINESTRING (2 49,3 50))')) != 0:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'

        feat = lyr.GetNextFeature()
        if feat is not None:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test .osm

def ogr_osm_2():
    return ogr_osm_1('data/test.osm')

###############################################################################
# Test ogr2ogr

def ogr_osm_3(options = None):

    try:
        drv = ogr.GetDriverByName('OSM')
    except:
        drv = None
    if drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    try:
        os.stat('tmp/ogr_osm_3')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/ogr_osm_3')
    except:
        pass

    if options is not None:
        options = ' ' + options
    else:
        options = ''
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/ogr_osm_3 data/test.pbf points lines polygons multipolygons multilinestrings -progress' + options)

    ret = ogr_osm_1('tmp/ogr_osm_3')

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/ogr_osm_3')

    return ret

###############################################################################
# Test ogr2ogr with --config OSM_USE_CUSTOM_INDEXING NO

def ogr_osm_3_sqlite_nodes():
    return ogr_osm_3(options = '--config OSM_USE_CUSTOM_INDEXING NO')

###############################################################################
# Test ogr2ogr with --config OSM_COMPRESS_NODES YES

def ogr_osm_3_custom_compress_nodes():
    return ogr_osm_3(options = '--config OSM_COMPRESS_NODES YES')

###############################################################################
# Test optimization when reading only the points layer through a SQL request

def ogr_osm_4():

    try:
        drv = ogr.GetDriverByName('OSM')
    except:
        drv = None
    if drv is None:
        return 'skip'

    ds = ogr.Open( 'data/test.pbf' )
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    sql_lyr = ds.ExecuteSQL('SELECT * FROM points')

    feat = sql_lyr.GetNextFeature()
    is_none = feat is None

    ds.ReleaseResultSet(sql_lyr)

    if is_none:
        gdaltest.post_reason('fail')
        return 'fail'

    # Change layer
    sql_lyr = ds.ExecuteSQL('SELECT * FROM lines')

    feat = sql_lyr.GetNextFeature()
    is_none = feat is None

    ds.ReleaseResultSet(sql_lyr)

    if is_none:
        gdaltest.post_reason('fail')
        return 'fail'

    # Change layer
    sql_lyr = ds.ExecuteSQL('SELECT * FROM points')

    feat = sql_lyr.GetNextFeature()
    is_none = feat is None

    ds.ReleaseResultSet(sql_lyr)

    if is_none:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test optimizations for early attribute filter evaluation

def ogr_osm_5():

    try:
        drv = ogr.GetDriverByName('OSM')
    except:
        drv = None
    if drv is None:
        return 'skip'

    ds = ogr.Open( 'data/test.pbf' )
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    tests = [ [ 'points', '3', True ],
              [ 'points', 'foo', False ],
              [ 'lines', '1', True ],
              [ 'lines', 'foo', False ],
              [ 'multipolygons', '1', True ],
              [ 'multipolygons', 'foo', False ],
              [ 'multilinestrings', '3', True ],
              [ 'multilinestrings', 'foo', False ] ]

    for test in tests:
        sql_lyr = ds.ExecuteSQL("SELECT * FROM %s WHERE osm_id = '%s'" % (test[0], test[1]))
        feat = sql_lyr.GetNextFeature()
        is_none = feat is None
        feat = None
        ds.ReleaseResultSet(sql_lyr)

        if not (test[2] ^ is_none):
            gdaltest.post_reason('fail')
            print(test)
            return 'fail'

    sql_lyr = ds.ExecuteSQL("select * from multipolygons where type = 'multipolygon'")
    feat = sql_lyr.GetNextFeature()
    is_none = feat is None
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    if is_none:
        gdaltest.post_reason('fail')
        print(test)
        return 'fail'

    return 'success'

###############################################################################
# Test ogr2ogr -sql

def ogr_osm_6():

    try:
        drv = ogr.GetDriverByName('OSM')
    except:
        drv = None
    if drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    try:
        os.stat('tmp/ogr_osm_6')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/ogr_osm_6')
    except:
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/ogr_osm_6 data/test.pbf -sql "select * from multipolygons" -progress')

    ds = ogr.Open('tmp/ogr_osm_6')
    lyr = ds.GetLayer(0)
    count = lyr.GetFeatureCount()
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/ogr_osm_6')

    if count != 3:
        gdaltest.post_reason('fail')
        print(count)
        return 'fail'

    return 'success'

###############################################################################
# Test optimization when reading only the points layer through a SQL request
# with SQLite dialect (#4825)

def ogr_osm_7():

    try:
        drv = ogr.GetDriverByName('OSM')
    except:
        drv = None
    if drv is None:
        return 'skip'

    ds = ogr.Open( 'data/test.pbf' )
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    sql_lyr = ds.ExecuteSQL('SELECT * FROM points LIMIT 10', dialect = 'SQLite')
    count = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)

    if count != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

gdaltest_list = [
    ogr_osm_1,
    ogr_osm_2,
    ogr_osm_3,
    ogr_osm_3_sqlite_nodes,
    ogr_osm_3_custom_compress_nodes,
    ogr_osm_4,
    ogr_osm_5,
    ogr_osm_6,
    ogr_osm_7,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_osm' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
