#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Interlis driver testing.
# Author:   Pirmin Kalberer <pka(at)sourcepole.ch>
# 
###############################################################################
# Copyright (c) 2012, Pirmin Kalberer <pka(at)sourcepole.ch>
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

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr, osr, gdal

def cpl_debug_on():
    gdaltest.cpl_debug = gdal.GetConfigOption('CPL_DEBUG')
    gdal.SetConfigOption('CPL_DEBUG', 'ON')

def cpl_debug_reset():
    gdal.SetConfigOption('CPL_DEBUG', gdaltest.cpl_debug)

###############################################################################
# Open Driver

def ogr_interlis1_1():

    gdaltest.have_ili_reader = 0
    try:
        driver = ogr.GetDriverByName( 'Interlis 1' )
        if driver is None:
            return 'skip'
    except:
        return 'skip'

    # Check ili2c.jar
    ds = ogr.Open( 'data/ili/Beispiel.itf,data/ili/Beispiel.ili' )
    if gdal.GetLastErrorMsg().find('iom_compileIli failed.') != -1:
        gdaltest.post_reason( 'skipping test: ili2c.jar not found in PATH' )
        return 'skip'

    gdaltest.have_ili_reader = 1
    
    return 'success'

###############################################################################
# Check that Ili1 point layer is properly read.

def ogr_interlis1_2():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/Beispiel.itf,data/ili/Beispiel.ili' )
    layers = ['Bodenbedeckung__BoFlaechen',
              'Bodenbedeckung__BoFlaechen_Form',
              'Bodenbedeckung__BoFlaechen__Areas',
              'Bodenbedeckung__Strasse',
              'Bodenbedeckung__Gebaeude']
    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    lyr = ds.GetLayerByName('Bodenbedeckung__BoFlaechen')

    if lyr.GetFeatureCount() != 3:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    #Get 2nd feature
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()

    field_values = [20, 1, 168.27, 170.85, 'POINT (168.27 170.85)']

    if feat.GetFieldCount() != len(field_values)-1:
        gdaltest.post_reason( 'field count wrong.' )
        return 'fail'

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
          feat.DumpReadable()
          print(feat.GetFieldAsString(i))
          gdaltest.post_reason( 'field value wrong.' )
          return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetCoordinateDimension() != 2:
        gdaltest.post_reason( 'dimension wrong.' )
        return 'fail'

    if geom.GetGeometryName() != 'POINT':
        gdaltest.post_reason( 'Geometry of wrong type.' )
        return 'fail'

    return 'success'

###############################################################################
# Ili1 FORMAT DEFAULT test.

def ogr_interlis1_3():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/format-default.itf,data/ili/format-default.ili' )

    layers = ['FormatTests__FormatTable']
    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    lyr = ds.GetLayerByName('FormatTests__FormatTable')

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    feat = lyr.GetNextFeature()

    field_values = [0, 'aa bb', 'cc^dd', '', 1]

    if feat.GetFieldCount() != len(field_values):
        gdaltest.post_reason( 'field count wrong.' )
        return 'fail'

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
          feat.DumpReadable()
          print(feat.GetFieldAsString(i))
          gdaltest.post_reason( 'field value wrong.' )
          return 'fail'

    return 'success'

###############################################################################
# Ili1 FORMAT test.

def ogr_interlis1_4():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/format-test.itf,data/ili/format-test.ili' )

    layers = ['FormatTests__FormatTable']
    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    lyr = ds.GetLayerByName('FormatTests__FormatTable')

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    feat = lyr.GetNextFeature()

    field_values = [0, 'aa_bb', 'cc dd', '', 1]

    if feat.GetFieldCount() != len(field_values):
        gdaltest.post_reason( 'field count wrong.' )
        return 'fail'

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
          feat.DumpReadable()
          print(feat.GetFieldAsString(i))
          gdaltest.post_reason( 'field value wrong.' )
          return 'fail'

    return 'success'

###############################################################################
# Write Ili1 transfer file.

def ogr_interlis1_5():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/format-default.itf,data/ili/format-default.ili' )

    lyr = ds.GetLayerByName('FormatTests__FormatTable')
    feat = lyr.GetNextFeature()

    driver = ogr.GetDriverByName( 'Interlis 1' )
    dst_ds = driver.CreateDataSource( 'tmp/interlis1_5.itf' )

    dst_lyr = dst_ds.CreateLayer( 'FormatTests__FormatTable' )

    layer_defn = lyr.GetLayerDefn()
    for i in range( layer_defn.GetFieldCount() ):
        dst_lyr.CreateField( layer_defn.GetFieldDefn( i ) )
    dst_feat = ogr.Feature( feature_def = dst_lyr.GetLayerDefn() )
    dst_feat.SetFrom( feat )
    dst_lyr.CreateFeature( dst_feat )

    return 'success'

###############################################################################
# Write Ili1 transfer file using a model.

def ogr_interlis1_6():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/format-default.itf,data/ili/format-default.ili' )
    lyr = ds.GetLayerByName('FormatTests__FormatTable')
    feat = lyr.GetNextFeature()

    driver = ogr.GetDriverByName( 'Interlis 1' )
    dst_ds = driver.CreateDataSource( 'tmp/interlis1_6.itf,data/ili/format-default.ili' )

    dst_lyr = dst_ds.CreateLayer( 'test' )

    layer_defn = lyr.GetLayerDefn()
    for i in range( layer_defn.GetFieldCount() ):
        dst_lyr.CreateField( layer_defn.GetFieldDefn( i ) )
    dst_feat = ogr.Feature( feature_def = dst_lyr.GetLayerDefn() )
    dst_feat.SetFrom( feat )
    dst_lyr.CreateFeature( dst_feat )

    return 'success'

###############################################################################
# Ili1 character encding test.

def ogr_interlis1_7():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/encoding-test.itf,data/ili/format-default.ili' )

    layers = ['FormatTests__FormatTable']
    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    lyr = ds.GetLayerByName('FormatTests__FormatTable')

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    feat = lyr.GetNextFeature()

    #Interlis 1 Encoding is ISO 8859-1 (Latin1)
    #Pyton source code is UTF-8 encoded
    field_values = [0, 'äöü', 'ÄÖÜ', '', 1]

    if feat.GetFieldCount() != len(field_values):
        gdaltest.post_reason( 'field count wrong.' )
        return 'fail'

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
          feat.DumpReadable()
          print(feat.GetFieldAsString(i))
          gdaltest.post_reason( 'field value wrong.' )
          return 'fail'

    #Write back
    driver = ogr.GetDriverByName( 'Interlis 1' )
    dst_ds = driver.CreateDataSource( 'tmp/interlis1_7.itf' )

    dst_lyr = dst_ds.CreateLayer( 'FormatTests__FormatTable' )

    layer_defn = lyr.GetLayerDefn()
    for i in range( layer_defn.GetFieldCount() ):
        dst_lyr.CreateField( layer_defn.GetFieldDefn( i ) )
    dst_feat = ogr.Feature( feature_def = dst_lyr.GetLayerDefn() )
    dst_feat.SetFrom( feat )
    dst_lyr.CreateFeature( dst_feat )

    return 'success'

###############################################################################
# Ili1 VRT rename

def ogr_interlis1_9():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/Beispiel-rename.vrt' )
    layers = ['BoGebaeude']
    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    lyr = ds.GetLayerByName('BoGebaeude')

    if lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef() != 'AssekuranzNr':
        gdaltest.post_reason( 'Wrong field name: ' +  lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef())
        return 'fail'

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    feat = lyr.GetNextFeature()

    field_values = ['958', 10, 'POINT (148.41 175.96)']

    if feat.GetFieldCount() != len(field_values)-1:
        gdaltest.post_reason( 'field count wrong.' )
        return 'fail'

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
          feat.DumpReadable()
          print(feat.GetFieldAsString(i))
          gdaltest.post_reason( 'field value wrong.' )
          return 'fail'

    return 'success'

###############################################################################
# Ili1 VRT join

def ogr_interlis1_10():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/Beispiel-join.vrt' ) #broken without enums
    layers = ['BoFlaechenJoined']
    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    lyr = ds.GetLayerByName('BoFlaechenJoined')

    #TODO: Test that attribute filters are passed through to an underlying layer.
    #lyr.SetAttributeFilter( 'other = "Second"' )
    #lyr.ResetReading()

    if lyr.GetFeatureCount() != 3:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    feat = lyr.GetNextFeature()

    field_values = ['10', 0, 148.2, 183.48,'Gebaeude',  -1]

    if feat.GetFieldCount() != len(field_values)-1:
        gdaltest.post_reason( 'field count wrong.' )
        return 'fail'

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
          feat.DumpReadable()
          print(feat.GetFieldAsString(i))
          gdaltest.post_reason( 'field value wrong.' )
          return 'fail'

    return 'success'

###############################################################################
# Ili1 multi-geom test (RFC41)

def ogr_interlis1_11():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/multigeom.itf,data/ili/multigeom.ili' )

    layers = ['MultigeomTests__MultigeomTable']
    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    lyr = ds.GetLayerByName('MultigeomTests__MultigeomTable')

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    feat = lyr.GetNextFeature()

    #feat.DumpReadable()
    #        _TID (String) = 0
    #        Text1 (String) = aa bb
    #        Number (Real) = 40
    #        MULTILINESTRING ((190.26 208.0 0, ...
    #        GeomPoint_0 (Real) = 148.41
    #        GeomPoint_1 (Real) = 175.96

    if feat.GetFieldCount() != 5:
        gdaltest.post_reason( 'field count wrong.' )
        print feat.GetFieldCount()
        return 'fail'

    geom_columns = ['GeomLine', 'GeomPoint']

    if feat.GetGeomFieldCount() != len(geom_columns):
        gdaltest.post_reason( 'geom field count wrong.' )
        print feat.GetGeomFieldCount()
        return 'fail'

    for i in range(feat.GetGeomFieldCount()):
        defn = lyr.GetLayerDefn().GetGeomFieldDefn(i)
        if defn.GetName() != str(geom_columns[i]):
            print("Geom field: " + defn.GetName())
            return 'fail'

    return 'success'

###############################################################################
# Ili1 multi-geom test (RFC41)

def ogr_interlis1_12():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/multicoord.itf,data/ili/multicoord.ili' )

    layers = ['MulticoordTests__MulticoordTable']
    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    lyr = ds.GetLayerByName('MulticoordTests__MulticoordTable')

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    feat = lyr.GetNextFeature()

    if feat.GetFieldCount() != 6:
        gdaltest.post_reason( 'field count wrong.' )
        print feat.GetFieldCount()
        return 'fail'

    geom_columns = ['coordPoint1', 'coordPoint2']

    if feat.GetGeomFieldCount() != len(geom_columns):
        gdaltest.post_reason( 'geom field count wrong.' )
        print feat.GetGeomFieldCount()
        return 'fail'

    for i in range(feat.GetGeomFieldCount()):
        defn = lyr.GetLayerDefn().GetGeomFieldDefn(i)
        if defn.GetName() != str(geom_columns[i]):
            print("Geom field: " + defn.GetName())
            return 'fail'

    return 'success'

###############################################################################
# Ili1 Surface test.

def ogr_interlis1_13():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/surface.itf,data/ili/surface.ili' )

    layers = ['SURFC_TOP__SURFC_TBL', 'SURFC_TOP__SURFC_TBL_SHAPE']

    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    lyr = ds.GetLayerByName('SURFC_TOP__SURFC_TBL')

    if lyr.GetFeatureCount() != 3:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    feat = lyr.GetNextFeature()

    field_values = ['103', 1, 3, 1, 23, 25000, 20060111]

    if feat.GetFieldCount() != len(field_values):
        gdaltest.post_reason( 'field count wrong.' )
        return 'fail'

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
          feat.DumpReadable()
          print(feat.GetFieldAsString(i))
          gdaltest.post_reason( 'field value wrong.' )
          return 'fail'

    return 'success'

###############################################################################
# Reading Ili2 without model

def ogr_interlis2_1():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/RoadsExdm2ien.xml' )
    if ds is None:
        return 'fail'

    layers = ['RoadsExdm2ben.Roads.LandCover',
              'RoadsExdm2ben.Roads.Street',
              'RoadsExdm2ien.RoadsExtended.StreetAxis',
              'RoadsExdm2ben.Roads.StreetNamePosition',
              'RoadsExdm2ien.RoadsExtended.RoadSign']
    #ILI 2.2 Example
    #layers = ['RoadsExdm2ben_10.Roads.LandCover',
    #          'RoadsExdm2ben_10.Roads.Street',
    #          'RoadsExdm2ien_10.RoadsExtended.StreetAxis',
    #          'RoadsExdm2ben_10.Roads.StreetNamePosition',
    #          'RoadsExdm2ien_10.RoadsExtended.RoadSign']

    if ds.GetLayerCount() != len(layers):
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          return 'fail'

    return 'success'

###############################################################################
# Reading Ili2

def ogr_interlis2_2():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/RoadsExdm2ien.xml,data/ili/RoadsExdm2ben.ili,data/ili/RoadsExdm2ien.ili' )
    if ds is None:
        return 'fail'

    if ds.GetLayerCount() != 8:
        gdaltest.post_reason( 'layer count wrong.' )
        return 'fail'

    layers = ['RoadsExdm2ien.RoadsExtended.RoadSign',
              'RoadsExdm2ien.RoadsExtended.StreetAxis',
              'RoadsExdm2ben.Roads.RoadSign',
              'RoadsExdm2ben.Roads.StreetAxis',
              'RoadsExdm2ben.Roads.LandCover',
              'RoadsExdm2ben.Roads.StreetNamePosition',
              'RoadsExdm2ben.Roads.Street',
              'RoadsExdm2ben.Roads.LAttrs']
    for i in range(ds.GetLayerCount()):
      if not ds.GetLayer(i).GetName() in layers:
          gdaltest.post_reason( 'Did not get right layers' )
          #return 'fail'

    lyr = ds.GetLayerByName('RoadsExdm2ien.RoadsExtended.RoadSign')
    if lyr.GetFeatureCount() != 4:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    feat = lyr.GetNextFeature()

    #field_values = ['501', 'prohibition.noparking', 'POINT (168.27 170.85)']
    field_values = ['prohibition.noparking', 'POINT (168.27 170.85)']

    if feat.GetFieldCount() != len(field_values)-1:
        gdaltest.post_reason( 'field count wrong.' )
        return 'fail'

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
          feat.DumpReadable()
          print(feat.GetFieldAsString(i))
          gdaltest.post_reason( 'field value wrong.' )
          return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetCoordinateDimension() != 2:
        gdaltest.post_reason( 'dimension wrong.' )
        return 'fail'

    if geom.GetGeometryName() != 'POINT':
        gdaltest.post_reason( 'Geometry of wrong type.' )
        return 'fail'

    return 'success'


###############################################################################
# Check arc segmentation

def ogr_interlis_arc1():

    if not gdaltest.have_ili_reader:
        return 'skip'

    ds = ogr.Open( 'data/ili/Beispiel.itf,data/ili/Beispiel.ili' )

    length_0_1_deg = 72.7181992353 # Line length with 0.1 degree segments

    #Read Area lines
    lyr = ds.GetLayerByName('Bodenbedeckung__BoFlaechen_Form')
    if lyr.GetFeatureCount() != 4:
        gdaltest.post_reason( 'feature count wrong.' )
        return 'fail'

    #Get 3rd feature
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()

    geom = feat.GetGeometryRef()
    length = geom.Length()
    if abs(length-length_0_1_deg) > 0.001: #72.7177335946
        gdaltest.post_reason( 'Length() result wrong, got %g.' % length )
        return 'fail'
    if ogrtest.check_feature_geometry(geom, 'MULTILINESTRING ((186.38 206.82 0,186.460486424448106 206.327529546107257 0,186.532365795904212 205.833729416057821 0,186.595616219215486 205.338750026154884 0,186.650218427690277 204.84274215191553 0,186.696155788966905 204.345856882143238 0,186.733414310080178 203.848245572904574 0,186.761982641723563 203.350059801424834 0,186.781852081706546 202.851451319916038 0,186.793016577605215 202.352572009351746 0,186.795472728605944 201.853573833202773 0,186.789219786541395 201.354608791147427 0,186.774259656118232 200.855828872771127 0,186.75059689433715 200.357386011268773 0,186.718238709104583 199.859432037164538 0,186.677194957037244 199.362118632062675 0,186.627478140459601 198.865597282444014 0,186.569103403595591 198.370019233521532 0,186.502088527955578 197.875535443169724 0,186.426453926919834 197.382296535941265 0,186.342222639520543 196.890452757185443 0,186.249420323423806 196.400153927281849 0,186.148075247114093 195.911549396003693 0,186.038218281283434 195.424787997024453 0,185.919882889427811 194.940018002581581 0,185.793105117653909 194.457387078311513 0,185.65792358369913 193.977042238269064 0,185.514379465168304 193.499129800145766 0,185.362516486990415 193.023795340699877 0,185.202380908099769 192.551183651412458 0,185.034021507344988 192.081438694382314 0,184.857489568630456 191.614703558473906 0,184.672838865294779 191.151120415730986 0,184.480125643730986 190.690830478069671 0,184.279408606253213 190.233973954263888 0,184.070748893215466 189.780690007236473 0,183.854210064387644 189.331116711668756 0,183.629858079594698 188.885391011941635 0,183.397761278624529 188.44364868042112 0,183.26 188.19 0,183.157990360411077 188.006024276100675 0,182.910618361498763 187.572651103613168 0,182.655720633794772 187.143661172625173 0,182.393374821616248 186.719185157625333 0,182.123660838038973 186.299352358119819 0,181.846660840555074 185.884290659246403 0,181.562459206047208 185.474126492819352 0,181.271142505086345 185.06898479881707 0,180.972799475561658 184.668988987324269 0,180.667520995650023 184.274260900939993 0,180.355400056133732 183.884920777663154 0,180.036531732074565 183.501087214266903 0,179.711013153852946 183.122877130172981 0,179.378943477581203 182.750405731836764 0,179.040423854899558 182.383786477654411 0,178.695557402164354 182.023131043402287 0,178.344449169037915 181.668549288219367 0,177.987206106489339 181.320149221143225 0,177.623937034216141 180.978036968209295 0,177.254752607496783 180.642316740123988 0,176.879765283484062 180.313090800520911 0,176.499089286949413 179.990459434810589 0,176.112840575489088 179.674520919632386 0,175.721136804202303 179.365371492918626 0,175.324097289852261 179.063105324579453 0,174.921842974521269 178.767814487817873 0,174.514496388770482 178.479588931083327 0,174.102181614316009 178.198516450672486 0,173.685024246232274 177.924682663985692 0,173.26315135469477 177.658170983447064 0,172.836691446272965 177.399062591096254 0,172.405774424786216 177.147436413859566 0,171.970531551733643 176.903369099508211 0,171.531095406310641 176.666934993310434 0,171.087599845024073 176.438206115385384 0,170.64017996091809 176.217252138765019 0,170.188972042423671 176.004140368171022 0,170.18 176.0 0,170.18 176.0 0,140.69 156.63 0))') != 0:
        gdaltest.post_reason( 'Ili curve not correctly read' )
        print(geom.ExportToWkt())
        return 'fail'
    line = geom.GetGeometryRef(0)
    points = line.GetPoints()
    if len(points) != 80:
        gdaltest.post_reason( 'line point count wrong.' )
        return 'fail'

    #Get 4th feature
    feat = lyr.GetNextFeature()

    geom = feat.GetGeometryRef()
    length = geom.Length()
    if abs(length-98.0243498288) > 0.0000000001:
        gdaltest.post_reason( 'Length() result wrong, got %g.' % length )
        return 'fail'
    if ogrtest.check_feature_geometry(geom, 'MULTILINESTRING ((186.38 206.82 0,194.26 208.19 0,194.363742877465398 207.560666258887778 0,194.456486566153643 206.929617805490125 0,194.538202815438581 206.297046863278979 0,194.608866733759072 205.663146119491216 0,194.66845679620107 205.028108666434122 0,194.716954851054254 204.392127942667628 0,194.754346125341328 203.755397674081081 0,194.780619229317949 203.118111814882468 0,194.795766159942076 202.48046448851801 0,194.799782303311986 201.842649928540311 0,194.792666436071414 201.204862419443032 0,194.774420725782534 200.567296237479837 0,194.745050730265405 199.930145591485967 0,194.704565395905178 199.293604563720407 0,194.652977054926907 198.657867050746546 0,194.590301421638998 198.023126704369332 0,194.516557587646503 197.389576872647183 0,194.431768016035619 196.757410540996148 0,194.335958534531215 196.126820273404775 0,194.229158327629534 195.497998153777246 0,194.111399927708163 194.87113572742274 0,193.982719205116467 194.246423942708901 0,193.843155357249145 193.624053092897014 0,193.692750896606185 193.004212758177033 0,193.531551637843307 192.387091747919413 0,193.359606683816367 191.772878043162052 0,193.176968410623942 191.161758739349466 0,192.983692451653383 190.553919989341608 0,192.779837680634046 189.949546946710058 0,192.565466193703969 189.348823709338234 0,192.340643290494768 188.751933263343574 0,192.105437454240644 188.159057427338212 0,191.859920330917817 187.570376797045014 0,191.604166707420404 186.986070690286709 0,191.338254488779711 186.406317092363707 0,191.062264674433465 185.831292601838129 0,190.776281333552674 185.261172376740149 0,190.75 185.21 0,190.480391579433388 184.696130081213255 0,190.174685542961015 184.136337832614316 0,189.859256345155728 183.581966149085105 0,189.534200068806825 183.033183897610741 0,189.199615729204936 182.490158242581202 0,188.855605243981131 181.953054594871418 0,188.502273402061718 181.422036561455485 0,188.139727831748502 180.897265895570513 0,187.768078967934287 180.378902447444887 0,187.387440018463224 179.867104115606367 0,186.997926929646695 179.362026798784797 0,186.599658350944793 178.863824348423634 0,186.192755598824732 178.372648521815478 0,185.777342619806575 177.88864893587521 0,185.35354595270789 177.411973021565359 0,184.921494690098939 176.94276597898704 0,184.481320438979651 176.481170733150748 0,184.033157280690972 176.027327890439949 0,183.577141730072384 175.58137569578102 0,183.113412693878161 175.143449990532446 0,182.642111428464915 174.713684171106365 0,182.163381496763719 174.292209148334592 0,181.677368724549325 173.879153307591992 0,181.184221156020271 173.47464246968903 0,180.684089008703126 173.078799852545501 0,180.177124627694894 172.691746033657182 0,179.663482439257081 172.313598913366803 0,179.143318903776049 171.944473678950402 0,178.616792468103654 171.584482769530155 0,178.084063517292776 171.233735841824398 0,177.545294325742475 170.8923397367451 0,177.000649007767692 170.560398446852986 0,176.450293467608361 170.238013084680574 0,175.894395348893454 169.925281851932226 0,175.333123983575007 169.622300009570893 0,174.766650340348065 169.329159848800828 0,174.195146972571933 169.045950662954681 0,174.1 169.0 0,174.1 169.0 0,145.08 149.94 0,140.69 156.63 0))') != 0:
        gdaltest.post_reason( 'Ili curve not correctly read' )
        print(geom.ExportToWkt())
        return 'fail'
    line = geom.GetGeometryRef(0)
    points = line.GetPoints()
    if len(points) != 81:
        gdaltest.post_reason( 'line point count wrong.' )
        return 'fail'

    #0.1 deg instead of default (1 deg) (max deg for ili sementizer is about 1.3 degrees)
    os.environ['ARC_DEGREES'] = '0.1'
    #GML: gdal.SetConfigOption('OGR_ARC_STEPSIZE','0.1')
    ds = ogr.Open( 'data/ili/Beispiel.itf,data/ili/Beispiel.ili' )

    #Read Area lines
    lyr = ds.GetLayerByName('Bodenbedeckung__BoFlaechen_Form')
    #Get 3rd feature
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()

    geom = feat.GetGeometryRef()
    length = geom.Length()
    if abs(length-length_0_1_deg) > 0.0000000001:
        gdaltest.post_reason( 'Length() result wrong, got %g.' % length )
        return 'fail'
    line = geom.GetGeometryRef(0)
    points = line.GetPoints()
    if len(points) != 755: #80 for 1 deg
        gdaltest.post_reason( 'line point count wrong.' )
        return 'fail'

    #Compare with GML segmentation
    gml = """<gml:Curve xmlns:gml="http://www.opengis.net/gml" srsName="foo">
             <gml:segments>
              <gml:Arc interpolation="circularArc3Points" numArc="1">
               <gml:pos>186.38 206.82</gml:pos>
               <gml:pos>183.26 188.19</gml:pos>
               <gml:pos>170.18 176.00</gml:pos>
              </gml:Arc>
              <gml:LineStringSegment interpolation="linear">
               <gml:pos>170.18 176.00</gml:pos>
               <gml:pos>140.69 156.63</gml:pos>
              </gml:LineStringSegment>
             </gml:segments>
            </gml:Curve>"""
    gdal.SetConfigOption('OGR_ARC_STEPSIZE','6') #Default: 4 [The largest step in degrees along the arc]
    geom = ogr.CreateGeometryFromGML( gml )
    gdal.SetConfigOption('OGR_ARC_STEPSIZE',None)
    length = geom.Length()
    if abs(length-length_0_1_deg) > 0.7: #72.7016803283
        gdaltest.post_reason( 'Length() result wrong, got %g.' % length )
        return 'fail'
    if ogrtest.check_feature_geometry(geom, 'LINESTRING (186.38 206.82,186.768623686387684 203.205889815121054,186.742131691546092 200.213311625041456,186.402975223206795 197.239896258915451,185.754870150610031 194.318221077721432,184.804917248555853 191.480296566144432,183.26 188.19,181.620123736225366 185.556121443670179,179.776932597909394 183.198395319379188,177.697389164927273 181.046251002686745,175.404277350303147 179.123267837091674,172.922720966942563 177.45051442856635,170.18 176.0,140.69 156.63)') != 0:
        gdaltest.post_reason( '<gml:Curve> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    gdal.SetConfigOption('OGR_ARC_STEPSIZE','10') #Default: 4
    geom = ogr.CreateGeometryFromGML( gml )
    gdal.SetConfigOption('OGR_ARC_STEPSIZE',None)

    length = geom.Length()
    if abs(length-length_0_1_deg) > 0.7: #72.6745269621
        gdaltest.post_reason( 'Length() result wrong, got %g.' % length )
        return 'fail'
    if ogrtest.check_feature_geometry(geom, 'LINESTRING (186.38 206.82,186.707260350859286 199.715527332323774,185.884082527359169 194.800205905462519,183.26 188.19,179.446170855918467 182.824761966327173,175.800254672220944 179.426924177391925,170.18 176.0,140.69 156.63)') != 0:
        gdaltest.post_reason( '<gml:Curve> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# 

def ogr_interlis_cleanup():

    if not gdaltest.have_ili_reader:
        return 'skip'

    return 'success'

gdaltest_list = [ 
    ogr_interlis1_1,
    ogr_interlis1_11,
    ogr_interlis1_12,
    ogr_interlis1_13,
    ogr_interlis1_2,
    ogr_interlis1_3,
    ogr_interlis1_4,
    ogr_interlis1_5,
    ogr_interlis1_6,
    ogr_interlis1_7,
    ogr_interlis1_9,
    #ogr_interlis1_10,
    ogr_interlis2_1,
    ogr_interlis2_2,
    ogr_interlis_arc1,
    ogr_interlis_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_ili' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

