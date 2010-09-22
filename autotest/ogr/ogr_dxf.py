#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR DXF driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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
import string

sys.path.append( '../pymod' )

import ogrtest
import gdaltest
from osgeo import gdal, ogr

###############################################################################
# Check some general things to see if they meet expectations.

def ogr_dxf_1():

    gdaltest.dxf_ds = ogr.Open( 'data/assorted.dxf' )

    if gdaltest.dxf_ds is None:
        return 'fail'

    if gdaltest.dxf_ds.GetLayerCount() != 1:
        gdaltest.post_reason( 'expected exactly one layer!' )
        return 'fail'

    gdaltest.dxf_layer = gdaltest.dxf_ds.GetLayer(0)

    if gdaltest.dxf_layer.GetName() != 'entities':
        gdaltest.post_reason( 'did not get expected layer name.' )
        return 'fail'

    defn = gdaltest.dxf_layer.GetLayerDefn()
    if defn.GetFieldCount() != 6:
        gdaltest.post_reason( 'did not get expected number of fields.' )
        return 'fail'

    fc = gdaltest.dxf_layer.GetFeatureCount()
    if fc != 14:
        gdaltest.post_reason( 'did not get expected feature count, got %d' % fc)
        return 'fail'

    return 'success'

###############################################################################
# Read the first feature, an ellipse and see if it generally meets expectations.

def ogr_dxf_2():
    
    gdaltest.dxf_layer.ResetReading()

    feat = gdaltest.dxf_layer.GetNextFeature()

    if feat.Layer != '0':
        gdaltest.post_reason( 'did not get expected layer for feature 0' )
        return 'fail'

    if feat.GetFID() != 0:
        gdaltest.post_reason( 'did not get expected fid for feature 0' )
        return 'fail'

    if feat.SubClasses != 'AcDbEntity:AcDbEllipse':
        gdaltest.post_reason( 'did not get expected SubClasses on feature 0.' )
        return 'fail'

    if feat.LineType != 'ByLayer':
        gdaltest.post_reason( 'Did not get expected LineType' )
        return 'fail'

    if feat.EntityHandle != '43':
        gdaltest.post_reason( 'did not get expected EntityHandle' )
        return 'fail'

    if feat.GetStyleString() != 'PEN(c:#000000)':
        print( '%s' % feat.GetStyleString())
        gdaltest.post_reason( 'did not get expected style string on feat 0.' )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetGeometryType() != ogr.wkbLineString25D:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 1596.12

    if area < exp_area - 0.5 or area > exp_area + 0.5:
        gdaltest.post_reason( 'envelope area not as expected, got %g.' % area )
        return 'fail'

    if abs(geom.GetX(0)-73.25) > 0.001 or abs(geom.GetY(0)-139.75) > 0.001:
        gdaltest.post_reason( 'first point (%g,%g) not expected location.' \
                              % (geom.GetX(0),geom.GetY(0)) )
        return 'fail'
        
    feat.Destroy()

    return 'success'

###############################################################################
# Second feature should be a partial ellipse.

def ogr_dxf_3():
    
    feat = gdaltest.dxf_layer.GetNextFeature()

    geom = feat.GetGeometryRef()

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 311.864

    if area < exp_area - 0.5 or area > exp_area + 0.5:
        gdaltest.post_reason( 'envelope area not as expected, got %g.' % area )
        return 'fail'

    if abs(geom.GetX(0)-61.133) > 0.01 or abs(geom.GetY(0)-103.592) > 0.01:
        gdaltest.post_reason( 'first point (%g,%g) not expected location.' \
                              % (geom.GetX(0),geom.GetY(0)) )
        return 'fail'

    feat.Destroy()
    
    return 'success'

###############################################################################
# Third feature: point.

def ogr_dxf_4():
    
    feat = gdaltest.dxf_layer.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'POINT (83.5 160.0 0)' ):
        return 'fail'

    feat.Destroy()

    return 'success'

###############################################################################
# Fourth feature: LINE

def ogr_dxf_5():
    
    feat = gdaltest.dxf_layer.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'LINESTRING (97.0 159.5 0,108.5 132.25 0)' ):
        return 'fail'

    if feat.GetGeometryRef().GetGeometryType() == ogr.wkbLineString:
        gdaltest.post_reason( 'not keeping 3D linestring as 3D' )
        return 'fail'
        
    feat.Destroy()

    return 'success'

###############################################################################
# Fourth feature: MTEXT

def ogr_dxf_6():
    
    feat = gdaltest.dxf_layer.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'POINT (84 126)' ):
        return 'fail'

    if feat.GetGeometryRef().GetGeometryType() == ogr.wkbPoint25D:
        gdaltest.post_reason( 'not keeping 2D text as 2D' )
        return 'fail'
        
    if feat.GetStyleString() != 'LABEL(f:"Arial",t:"Test",a:30,s:5g,p:7,c:#000000)':
        print(feat.GetStyleString())
        gdaltest.post_reason( 'got wrong style string' )
        return 'fail'

    feat.Destroy()

    return 'success'

###############################################################################
# Partial CIRCLE

def ogr_dxf_7():
    
    feat = gdaltest.dxf_layer.GetNextFeature()

    geom = feat.GetGeometryRef()

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 445.748

    if area < exp_area - 0.5 or area > exp_area + 0.5:
        print(envelope)
        gdaltest.post_reason( 'envelope area not as expected, got %g.' % area )
        return 'fail'

    if abs(geom.GetX(0)-115.258) > 0.01 or abs(geom.GetY(0)-107.791) > 0.01:
        gdaltest.post_reason( 'first point (%g,%g) not expected location.' \
                              % (geom.GetX(0),geom.GetY(0)) )
        return 'fail'

    feat.Destroy()
    
    return 'success'

###############################################################################
# Dimension 

def ogr_dxf_8():

    # Skip boring line.
    feat = gdaltest.dxf_layer.GetNextFeature()
    feat.Destroy()

    # Dimension lines
    feat = gdaltest.dxf_layer.GetNextFeature()
    geom = feat.GetGeometryRef()

    if geom.GetGeometryType() != ogr.wkbMultiLineString:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'MULTILINESTRING ((63.862871944482457 149.209935992088333,24.341960668550669 111.934531038652722),(72.754404848874373 139.782768575383642,62.744609795879391 150.395563330366286),(33.233493572942614 102.507363621948002,23.2236985199476 113.120158376930675),(63.862871944482457 149.209935992088333,59.187727781045531 147.04077688455709),(63.862871944482457 149.209935992088333,61.424252078251662 144.669522208001183),(24.341960668550669 111.934531038652722,26.78058053478146 116.474944822739886),(24.341960668550669 111.934531038652722,29.017104831987599 114.103690146183979))' ):
        return 'fail'

    feat.Destroy()

    # Dimension text
    feat = gdaltest.dxf_layer.GetNextFeature()
    
    geom = feat.GetGeometryRef()

    if ogrtest.check_feature_geometry( feat, 'POINT (42.815907752635709 131.936242584545397)' ):
        return 'fail'

    expected_style = 'LABEL(f:"Arial",t:"54.3264",p:5,a:43.3,s:2.5g)'
    if feat.GetStyleString() != expected_style:
        gdaltest.post_reason( 'Got unexpected style string:\n%s\ninstead of:\n%s.' % (feat.GetStyleString(),expected_style) )
        return 'fail'

    feat.Destroy()
    
    return 'success'

###############################################################################
# BLOCK (inlined)

def ogr_dxf_9():

    # Skip two dimensions each with a line and text.
    for x in range(4):
        feat = gdaltest.dxf_layer.GetNextFeature()
        feat.Destroy()

    # block 
    feat = gdaltest.dxf_layer.GetNextFeature()
    geom = feat.GetGeometryRef()

    if geom.GetGeometryType() != ogr.wkbGeometryCollection25D:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'GEOMETRYCOLLECTION (LINESTRING (79.069506278985116 121.003652476272777 0,79.716898725419625 118.892590150942851 0),LINESTRING (79.716898725419625 118.892590150942851 0,78.140638855839953 120.440702522851453 0),LINESTRING (78.140638855839953 120.440702522851453 0,80.139111190485622 120.328112532167196 0),LINESTRING (80.139111190485622 120.328112532167196 0,78.619146316248077 118.920737648613908 0),LINESTRING (78.619146316248077 118.920737648613908 0,79.041358781314059 120.975504978601705 0))' ):
        return 'fail'

    feat.Destroy()

    return 'success'

###############################################################################
# LWPOLYLINE in an Object Coordinate System.

def ogr_dxf_10():

    ocs_ds = ogr.Open('data/LWPOLYLINE-OCS.dxf')
    ocs_lyr = ocs_ds.GetLayer(0)
    
    # Skip boring line.
    feat = ocs_lyr.GetNextFeature()
    feat.Destroy()

    # LWPOLYLINE in OCS
    feat = ocs_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()

    if geom.GetGeometryType() != ogr.wkbLineString25D:
        print(geom.GetGeometryType())
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'LINESTRING (600325.567999998573214 3153021.253000000491738 562.760000000052969,600255.215999998385087 3151973.98600000096485 536.950000000069849,597873.927999997511506 3152247.628000000491738 602.705000000089058)' ):
        return 'fail'

    feat.Destroy()

    ocs_lyr = None
    ocs_ds.Destroy()
    ocs_ds = None

    return 'success'

###############################################################################
# Test reading from an entities-only dxf file (#3412)

def ogr_dxf_11():

    eo_ds = ogr.Open('data/entities_only.dxf')
    eo_lyr = eo_ds.GetLayer(0)
    
    # Check first point.
    feat = eo_lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat,
                                       'POINT (672500.0 242000.0 539.986)' ):
        return 'fail'

    feat.Destroy()

    # Check second point.
    feat = eo_lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat,
                                       'POINT (672750.0 242000.0 558.974)' ):
        return 'fail'

    feat.Destroy()

    eo_lyr = None
    eo_ds.Destroy()
    eo_ds = None

    return 'success'

###############################################################################
# Write a simple file with a polygon and a line, and read back.

def ogr_dxf_12():

    ds = ogr.GetDriverByName('DXF').CreateDataSource('tmp/dxf_11.dxf' )

    lyr = ds.CreateLayer( 'entities' )

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'LINESTRING(10 12, 60 65)' ) )
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()
                                  
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POLYGON((0 0,100 0,100 100,0 0))' ) )
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    lyr = None
    ds = None

    # Read back.
    ds = ogr.Open('tmp/dxf_11.dxf')
    lyr = ds.GetLayer(0)
    
    # Check first feature
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat,
                                       'LINESTRING(10 12, 60 65)' ):
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    if feat.GetGeometryRef().GetGeometryType() == ogr.wkbLineString25D:
        gdaltest.post_reason( 'not linestring 2D' )
        return 'fail'
        
    feat.Destroy()

    # Check second point.
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat,
                                       'POLYGON((0 0,100 0,100 100,0 0))' ):
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    if feat.GetGeometryRef().GetGeometryType() == ogr.wkbPolygon25D:
        gdaltest.post_reason( 'not keeping polygon 2D' )
        return 'fail'
        
    feat.Destroy()

    lyr = None
    ds.Destroy()
    ds = None
    
    os.unlink( 'tmp/dxf_11.dxf' )
        
    return 'success'
    

###############################################################################
# Check smoothed polyline.

def ogr_dxf_13():

    ds = ogr.Open( 'data/polyline_smooth.dxf' )
    
    layer = ds.GetLayer(0)

    feat = layer.GetNextFeature()

    if feat.Layer != '1':
        gdaltest.post_reason( 'did not get expected layer for feature 0' )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetGeometryType() != ogr.wkbPolygon25D:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 1350.43

    if area < exp_area - 0.5 or area > exp_area + 0.5:
        gdaltest.post_reason( 'envelope area not as expected, got %g.' % area )
        return 'fail'

    # Check for specific number of points from tesselated arc(s).
    # Note that this number depends on the tesselation algorithm and
    # possibly the default global arc_stepsize variable; therefore it is
    # not guaranteed to remain constant even if the input DXF file is constant.
    # If you retain this test, you may need to update the point count if
    # changes are made to the aforementioned items. Ideally, one would test 
    # only that more points are returned than in the original polyline, and 
    # that the points lie along (or reasonably close to) said path.

    rgeom = geom.GetGeometryRef(0)
    if rgeom.GetPointCount() != 146:
        gdaltest.post_reason( 'did not get expected number of points, got %d' % (rgeom.GetPointCount()) )
        return 'fail'
    
    if abs(rgeom.GetX(0)-251297.8179) > 0.001 \
       or abs(rgeom.GetY(0)-412226.8286) > 0.001:
        gdaltest.post_reason( 'first point (%g,%g) not expected location.' \
                              % (rgeom.GetX(0),rgeom.GetY(0)) )
        return 'fail'

    # Other possible tests:
    # Polylines with no explicit Z coordinates (e.g., no attribute 38 for
    # LWPOLYLINE and no attribute 30 for POLYLINE) should always return
    # geometry type ogr.wkbPolygon. Otherwise, ogr.wkbPolygon25D should be 
    # returned even if the Z coordinate values are zero.
    # If the arc_stepsize global is used, one could test that returned adjacent
    # points do not slope-diverge greater than that value.
        
    feat.Destroy()

    ds = None

    return 'success'


###############################################################################
# Check smooth LWPOLYLINE entity.

def ogr_dxf_14():

    # This test is identical to the previous one except the
    # newer lwpolyline entity is used. See the comments in the 
    # previous test regarding caveats, etc.

    ds = ogr.Open( 'data/lwpolyline_smooth.dxf' )
    
    layer = ds.GetLayer(0)

    feat = layer.GetNextFeature()

    if feat.Layer != '1':
        gdaltest.post_reason( 'did not get expected layer for feature 0' )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetGeometryType() != ogr.wkbPolygon:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 1350.43

    if area < exp_area - 0.5 or area > exp_area + 0.5:
        gdaltest.post_reason( 'envelope area not as expected, got %g.' % area )
        return 'fail'

    rgeom = geom.GetGeometryRef(0)
    if rgeom.GetPointCount() != 146:
        gdaltest.post_reason( 'did not get expected number of points, got %d' % (rgeom.GetPointCount()) )
        return 'fail'
    
    if abs(rgeom.GetX(0)-251297.8179) > 0.001 \
       or abs(rgeom.GetY(0)-412226.8286) > 0.001:
        gdaltest.post_reason( 'first point (%g,%g) not expected location.' \
                              % (rgeom.GetX(0),rgeom.GetY(0)) )
        return 'fail'
        
    feat.Destroy()

    ds = None

    return 'success'

###############################################################################
# Write a file with dynamic layer creation and confirm that the
# dynamically created layer 'abc' matches the definition of the default
# layer '0'.

def ogr_dxf_15():

    ds = ogr.GetDriverByName('DXF').CreateDataSource('tmp/dxf_14.dxf' )

    lyr = ds.CreateLayer( 'entities' )

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'LINESTRING(10 12, 60 65)' ) )
    dst_feat.SetField( 'Layer', 'abc' )
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()
                                  
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POLYGON((0 0,100 0,100 100,0 0))' ) )
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    lyr = None
    ds = None

    # Read back.
    ds = ogr.Open('tmp/dxf_14.dxf')
    lyr = ds.GetLayer(0)
    
    # Check first feature
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat,
                                       'LINESTRING(10 12, 60 65)' ):
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    if feat.GetGeometryRef().GetGeometryType() == ogr.wkbLineString25D:
        gdaltest.post_reason( 'not linestring 2D' )
        return 'fail'

    if feat.GetField('Layer') != 'abc':
        gdaltest.post_reason( 'Did not get expected layer, abc.' )
        return 'fail'
        
    feat.Destroy()

    # Check second point.
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat,
                                       'POLYGON((0 0,100 0,100 100,0 0))' ):
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    if feat.GetGeometryRef().GetGeometryType() == ogr.wkbPolygon25D:
        gdaltest.post_reason( 'not keeping polygon 2D' )
        return 'fail'
        
    if feat.GetField('Layer') != '0':
        print feat.GetField('Layer')
        gdaltest.post_reason( 'Did not get expected layer, 0.' )
        return 'fail'
        
    feat.Destroy()

    lyr = None
    ds.Destroy()
    ds = None

    # Check the DXF file itself to try and ensure that the layer
    # is defined essentially as we expect.  We assume the only thing
    # that will be different is the layer name is 'abc' instead of '0'.

    outdxf = open('tmp/dxf_14.dxf').read()
    start_1 = outdxf.find('  0\nLAYER')
    start_2 = outdxf.find('  0\nLAYER',start_1+10)

    txt_1 = outdxf[start_1:start_2]
    txt_2 = outdxf[start_2:start_2+len(txt_1)+2]

    abc_off = txt_2.find('abc\n')

    if txt_2[:abc_off] + '0' + txt_2[abc_off+3:] != txt_1:
        print txt_2[:abc_off] + '0' + txt_2[abc_off+3:]
        gdaltest.post_reason( 'Layer abc does not seem to match layer 0.' )
        return 'fail'
    
    os.unlink( 'tmp/dxf_14.dxf' )
        
    return 'success'
    

###############################################################################
# Test reading without DXF blocks inlined.

def ogr_dxf_16():

    gdal.SetConfigOption( 'DXF_INLINE_BLOCKS', 'FALSE' )
    
    dxf_ds = ogr.Open( 'data/assorted.dxf' )

    if dxf_ds is None:
        return 'fail'

    if dxf_ds.GetLayerCount() != 2:
        gdaltest.post_reason( 'expected exactly two layers!' )
        return 'fail'

    dxf_layer = dxf_ds.GetLayer(1)

    if dxf_layer.GetName() != 'entities':
        gdaltest.post_reason( 'did not get expected layer name.' )
        return 'fail'

    # read through till we encounter the block reference.
    feat = dxf_layer.GetNextFeature()
    while feat.GetField('EntityHandle') != '55':
        feat = dxf_layer.GetNextFeature()

    # check contents.
    if feat.GetField('BlockName') != 'STAR':
        gdaltest.post_reason( 'Did not get blockname!' )
        return 'fail'

    if feat.GetField('BlockAngle') != 0.0:
        gdaltest.post_reason( 'Did not get expected angle.' )
        return 'fail'

    if feat.GetField('BlockScale') != '(3:1,1,1)':
        print feat.GetField('BlockScale')
        gdaltest.post_reason( 'Did not get expected BlockScale' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POINT (79.097653776656188 119.962195062443342 0)' ):
        return 'fail'

    feat = None

    # Now we need to check the blocks layer and ensure it is as expected.

    dxf_layer = dxf_ds.GetLayer(0)
    
    if dxf_layer.GetName() != 'blocks':
        gdaltest.post_reason( 'did not get expected layer name.' )
        return 'fail'

    feat = dxf_layer.GetNextFeature()

    if feat.GetField('BlockName') != 'STAR':
        gdaltest.post_reason( 'Did not get expected block name.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'GEOMETRYCOLLECTION (LINESTRING (-0.028147497671066 1.041457413829428 0,0.619244948763444 -1.069604911500494 0),LINESTRING (0.619244948763444 -1.069604911500494 0,-0.957014920816232 0.478507460408116 0),LINESTRING (-0.957014920816232 0.478507460408116 0,1.041457413829428 0.365917469723853 0),LINESTRING (1.041457413829428 0.365917469723853 0,-0.478507460408116 -1.041457413829428 0),LINESTRING (-0.478507460408116 -1.041457413829428 0,-0.056294995342131 1.013309916158363 0))' ):
        return 'fail'

    feat = None

    # cleanup
    
    gdal.SetConfigOption( 'DXF_INLINE_BLOCKS', 'TRUE' )
    
    return 'success'

###############################################################################
# cleanup

def ogr_dxf_cleanup():
    gdaltest.dxf_layer = None
    gdaltest.dxf_ds.Destroy()
    gdaltest.dxf_ds = None

    return 'success'
    
###############################################################################
# 

gdaltest_list = [ 
    ogr_dxf_1,
    ogr_dxf_2,
    ogr_dxf_3,
    ogr_dxf_4,
    ogr_dxf_5,
    ogr_dxf_6,
    ogr_dxf_7,
    ogr_dxf_8,
    ogr_dxf_9,
    ogr_dxf_10,
    ogr_dxf_11,
    ogr_dxf_12,
    ogr_dxf_13,
    ogr_dxf_14,
    ogr_dxf_15,
    ogr_dxf_16,
    ogr_dxf_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_dxf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

