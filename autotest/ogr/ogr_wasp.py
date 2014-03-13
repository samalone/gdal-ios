#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id: ogr_shape.py 26794 2014-01-08 16:02:22Z rouault $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  WAsP driver testing.
# Author:   Vincent Mora <vincent dot mora at oslandia dot com>
# 
###############################################################################
# Copyright (c) 2014, Oslandia <info at oslandia dot com>
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import os
import shutil
import sys
import string
import math

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr, osr, gdal

###############################################################################
# Create wasp datasource

def ogr_wasp_create_ds():

    wasp_drv = ogr.GetDriverByName('WAsP')
    wasp_drv.DeleteDataSource( 'tmp.map' )
    
    gdaltest.wasp_ds = wasp_drv.CreateDataSource( 'tmp.map' )

    if gdaltest.wasp_ds is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Create elevation .map from linestrings z

def ogr_wasp_elevation_from_linestring_z():

    if ogr_wasp_create_ds() != 'success': return 'skip'

    ref = osr.SpatialReference()
    ref.ImportFromProj4('+proj=lcc +lat_1=46.8 +lat_0=46.8 +lon_0=0 +k_0=0.99987742 +x_0=600000 +y_0=2200000 +a=6378249.2 +b=6356514.999978254 +pm=2.337229167 +units=m +no_defs')

    layer = gdaltest.wasp_ds.CreateLayer('mylayer',
                                          ref,
                                          geom_type=ogr.wkbLineString25D )

    dfn = ogr.FeatureDefn()

    for i in range(10):
        feat = ogr.Feature( dfn )
        line = ogr.Geometry( type = ogr.wkbLineString25D )
        line.AddPoint( i, 0, i )
        line.AddPoint( i, 0.5, i )
        line.AddPoint( i, 1, i )
        feat.SetGeometry( line )
        if layer.CreateFeature( feat ) != 0:
            gdaltest.post_reason( 'unable to create feature')
            return 'fail'

    del gdaltest.wasp_ds
    del layer

    f = open('tmp.map')
    for i in range(4): f.next()
    i = 0
    j = 0
    for line in f:
        if not i%2:
            [h,n] = line.split()
            if int(n) != 3:
                gdaltest.post_reason( 'number of points sould be 3 and is %s' % n )
                return 'fail'

            if int(h) != j: 
                gdaltest.post_reason( 'altitude should be %d and is %s' %(j,h) )
                return 'fail'

            j+=1
        i+=1

    return 'success'

###############################################################################
# Create elevation .map from linestrings z with simplification

def ogr_wasp_elevation_from_linestring_z_toler():

    if ogr_wasp_create_ds() != 'success': return 'skip'

    ref = osr.SpatialReference()
    ref.ImportFromProj4('+proj=lcc +lat_1=46.8 +lat_0=46.8 +lon_0=0 +k_0=0.99987742 +x_0=600000 +y_0=2200000 +a=6378249.2 +b=6356514.999978254 +pm=2.337229167 +units=m +no_defs')

    layer = gdaltest.wasp_ds.CreateLayer('mylayer',
                                          ref,
                                          options = ['WASP_TOLERANCE=.1'],
                                          geom_type=ogr.wkbLineString25D )

    dfn = ogr.FeatureDefn()

    for i in range(10):
        feat = ogr.Feature( dfn )
        line = ogr.Geometry( type = ogr.wkbLineString25D )
        line.AddPoint( i, 0, i )
        line.AddPoint( i, 0.5, i )
        line.AddPoint( i, 1, i )
        feat.SetGeometry( line )
        if layer.CreateFeature( feat ) != 0:
            gdaltest.post_reason( 'unable to create feature')
            return 'fail'

    del gdaltest.wasp_ds
    del layer

    f = open('tmp.map')
    for i in range(4): f.next()
    i = 0
    j = 0
    for line in f:
        if not i%2:
            [h,n] = line.split()
            if int(n) != 2:
                gdaltest.post_reason( 'number of points sould be 2 and is %s' % n )
                return 'fail'

            if int(h) != j: 
                gdaltest.post_reason( 'altitude should be %d and is %s' %(j,h) )
                return 'fail'

            j+=1
        i+=1

    return 'success'


###############################################################################
# Create elevation .map from linestrings field

def ogr_wasp_elevation_from_linestring_field():

    if ogr_wasp_create_ds() != 'success': return 'skip'

    layer = gdaltest.wasp_ds.CreateLayer('mylayer',  
                                          options = ['WASP_FIELDS=elevation'],
                                          geom_type=ogr.wkbLineString )

    layer.CreateField( ogr.FieldDefn( 'elevation', ogr.OFTReal ) )

    for i in range(10):
        feat = ogr.Feature( layer.GetLayerDefn() )
        feat.SetField(0, float(i) )
        line = ogr.Geometry( type = ogr.wkbLineString )
        line.AddPoint( i, 0 )
        line.AddPoint( i, 0.5 )
        line.AddPoint( i, 1 )
        feat.SetGeometry( line )
        if layer.CreateFeature( feat ) != 0:
            gdaltest.post_reason( 'unable to create feature')
            return 'fail'

    del gdaltest.wasp_ds
    del layer

    f = open('tmp.map')
    for i in range(4): f.next()
    i = 0
    j = 0
    for line in f:
        if not i%2:
            [h,n] = line.split()
            if int(n) != 3:
                gdaltest.post_reason( 'number of points sould be 3 and is %s' % n )
                return 'fail'

            if int(h) != j: 
                gdaltest.post_reason( 'altitude should be %d and is %s' %(j,h) )
                return 'fail'

            j+=1
        i+=1

    return 'success'
    
###############################################################################
# Create roughness .map from linestrings fields

def ogr_wasp_roughness_from_linestring_fields():

    if ogr_wasp_create_ds() != 'success': return 'skip'

    layer = gdaltest.wasp_ds.CreateLayer('mylayer',  
                                          options = ['WASP_FIELDS=z_left,z_right'],
                                          geom_type=ogr.wkbPolygon )

    layer.CreateField( ogr.FieldDefn( 'dummy', ogr.OFTString ) )
    layer.CreateField( ogr.FieldDefn( 'z_left', ogr.OFTReal ) )
    layer.CreateField( ogr.FieldDefn( 'z_right', ogr.OFTReal ) )

    for i in range(10):
        feat = ogr.Feature( layer.GetLayerDefn() )
        feat.SetField(0, 'dummy_'+str(i) )
        feat.SetField(1, float(i)-1 )
        feat.SetField(2, float(i) )
        line = ogr.Geometry( type = ogr.wkbLineString )
        line.AddPoint( i, 0 )
        line.AddPoint( i, 0.5 )
        line.AddPoint( i, 1 )
        feat.SetGeometry( line )
        if layer.CreateFeature( feat ) != 0:
            gdaltest.post_reason( 'unable to create feature %d'%i)
            return 'fail'

    del gdaltest.wasp_ds
    del layer

    f = open('tmp.map')
    for i in range(4): f.next()
    i = 0
    j = 0
    for line in f:
        if not i%2:
            [l,r,n] = line.split()
            if int(n) != 3:
                gdaltest.post_reason( 'number of points sould be 3 and is %s' % n )
                return 'fail'

            if int(r) != j or int(l) != j-1: 
                gdaltest.post_reason( 'roughness should be %d and %d and is %s and %s' %(j-1,j,l,r) )
                return 'fail'

            j+=1
        i+=1

    return 'success'

###############################################################################
# Create .map from polygons z

def ogr_wasp_roughness_from_polygon_z():

    if ogr_wasp_create_ds() != 'success': return 'skip'

    layer = gdaltest.wasp_ds.CreateLayer('mylayer',  
                                          geom_type=ogr.wkbPolygon25D )

    dfn = ogr.FeatureDefn()

    for i in range(6):
        feat = ogr.Feature( dfn )
        ring = ogr.Geometry( type = ogr.wkbLinearRing )
        ring.AddPoint( 0, 0, i )
        ring.AddPoint( round(math.cos(i*math.pi/3),6),  round(math.sin(i*math.pi/3),6), i )
        ring.AddPoint( round(math.cos((i+1)*math.pi/3),6),  round(math.sin((i+1)*math.pi/3),6), i )
        ring.AddPoint( 0, 0, i )
        poly = ogr.Geometry( type = ogr.wkbPolygon25D )
        poly.AddGeometry(ring)
        feat.SetGeometry( poly )
        if layer.CreateFeature( feat ) != 0:
            gdaltest.post_reason( 'unable to create feature')
            return 'fail'

    del gdaltest.wasp_ds
    del layer

    f = open('tmp.map')
    for i in range(4): f.next()
    i = 0
    j = 0
    res = set()
    for line in f:
        if not i%2:
            [l,r,n] = [int(v) for v in line.split()]
            if n != 2:
                gdaltest.post_reason( 'number of points sould be 2 and is %d' % n )
                return 'fail'
            if r > l : res.add((l, r))
            else   : res.add((r, l))
            j+=1
        i+=1

    if j != 6: 
        gdaltest.post_reason( 'there should be 6 boundaries and there are %d' %j )
        return 'fail'

    if res != set([(0,1),(0,5),(1,2),(2,3),(3,4),(4,5)]):
        print res
        gdaltest.post_reason( 'wrong values f=in boundaries' )
        return 'fail'
    return 'success'

###############################################################################
# Create .map from polygons field 

def ogr_wasp_roughness_from_polygon_field():

    if ogr_wasp_create_ds() != 'success': return 'skip'

    layer = gdaltest.wasp_ds.CreateLayer('mylayer',  
                                          options = ['WASP_FIELDS=roughness'],
                                          geom_type=ogr.wkbPolygon )

    layer.CreateField( ogr.FieldDefn( 'roughness', ogr.OFTReal ) )
    layer.CreateField( ogr.FieldDefn( 'dummy', ogr.OFTString ) )

    for i in range(6):
        feat = ogr.Feature( layer.GetLayerDefn() )
        feat.SetField( 0, float(i) )
        ring = ogr.Geometry( type = ogr.wkbLinearRing )
        ring.AddPoint( 0, 0 )
        ring.AddPoint( round(math.cos(i*math.pi/3),6),  round(math.sin(i*math.pi/3),6) )
        ring.AddPoint( round(math.cos((i+1)*math.pi/3),6),  round(math.sin((i+1)*math.pi/3),6) )
        ring.AddPoint( 0, 0 )
        poly = ogr.Geometry( type = ogr.wkbPolygon )
        poly.AddGeometry(ring)
        feat.SetGeometry( poly )
        if layer.CreateFeature( feat ) != 0:
            gdaltest.post_reason( 'unable to create feature')
            return 'fail'

    del gdaltest.wasp_ds
    del layer

    f = open('tmp.map')
    for i in range(4): f.next()
    i = 0
    j = 0
    res = set()
    for line in f:
        if not i%2:
            [l,r,n] = [int(v) for v in line.split()]
            if n != 2:
                gdaltest.post_reason( 'number of points sould be 2 and is %d' % n )
                return 'fail'
            if r > l : res.add((l, r))
            else   : res.add((r, l))
            j+=1
        i+=1

    if j != 6: 
        gdaltest.post_reason( 'there should be 6 boundaries and there are %d' %j )
        return 'fail'

    if res != set([(0,1),(0,5),(1,2),(2,3),(3,4),(4,5)]):
        print res
        gdaltest.post_reason( 'wrong values f=in boundaries' )
        return 'fail'
    return 'success'

###############################################################################
# Test merging of linestrings
# especially the unwanted merging of a corner point that could be merged with
# a continuing line (pichart map)

def ogr_wasp_merge():

    if ogr_wasp_create_ds() != 'success': return 'skip'

    layer = gdaltest.wasp_ds.CreateLayer('mylayer',  
                                          geom_type=ogr.wkbPolygon25D )

    dfn = ogr.FeatureDefn()

    for i in range(6):
        feat = ogr.Feature( dfn )
        ring = ogr.Geometry( type = ogr.wkbLinearRing )
        h = i%2
        ring.AddPoint( 0, 0, h )
        ring.AddPoint( round(math.cos(i*math.pi/3),6),  round(math.sin(i*math.pi/3),6), h )
        ring.AddPoint( round(math.cos((i+1)*math.pi/3),6),  round(math.sin((i+1)*math.pi/3),6), h )
        ring.AddPoint( 0, 0, h )
        poly = ogr.Geometry( type = ogr.wkbPolygon25D )
        poly.AddGeometry(ring)
        feat.SetGeometry( poly )
        if layer.CreateFeature( feat ) != 0:
            gdaltest.post_reason( 'unable to create feature')
            return 'fail'

    del gdaltest.wasp_ds
    del layer

    f = open('tmp.map')
    for i in range(4): f.next()
    i = 0
    j = 0
    res = [] 
    for line in f:
        if not i%2:
            [l,r,n] = [int(v) for v in line.split()]
            if n != 2:
                gdaltest.post_reason( 'number of points sould be 2 and is %d (unwanted merge ?)' % n )
                return 'fail'
            if r > l : res.append((l, r))
            else   : res.append((r, l))
            j+=1
        i+=1

    if j != 6: 
        gdaltest.post_reason( 'there should be 6 boundaries and there are %d' %j )
        return 'fail'

    if res != [(0,1) for i in range(6)]:
        print res
        gdaltest.post_reason( 'wrong values f=in boundaries' )
        return 'fail'
    return 'success'
###############################################################################
# Read map file

def ogr_wasp_reading():
    if ogr_wasp_elevation_from_linestring_z() != 'success': return 'skip'

    gdaltest.wasp_ds = None

    ds = ogr.Open( 'tmp.map' )

    if ds == None or  ds.GetLayerCount() != 1:
        return 'fail'

    layer = ds.GetLayer(0)
    feat = layer.GetNextFeature()

    i = 0
    while feat:
        feat = layer.GetNextFeature()
        i+=1

    if i != 10:
        return 'fail'
    return 'success'
###############################################################################
# Cleanup

def ogr_wasp_cleanup():

    wasp_drv = ogr.GetDriverByName('WAsP')
    wasp_drv.DeleteDataSource( 'tmp.map' )
    return 'success'

gdaltest_list = [ 
    ogr_wasp_create_ds,
    ogr_wasp_elevation_from_linestring_z,
    ogr_wasp_elevation_from_linestring_z_toler,
    ogr_wasp_elevation_from_linestring_field,
    ogr_wasp_roughness_from_linestring_fields,
    ogr_wasp_roughness_from_polygon_z,
    ogr_wasp_roughness_from_polygon_field,
    ogr_wasp_merge,
    ogr_wasp_reading,
    ogr_wasp_cleanup
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_wasp' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

