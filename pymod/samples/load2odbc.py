#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  OGR Python samples
# Purpose:  Load ODBC table to an ODBC datastore.  Uses direct SQL 
#           since the ODBC driver is read-only for OGR.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
# 
#  $Log$
#  Revision 1.3  2005/11/02 19:42:25  fwarmerdam
#  unix text format
#
#  Revision 1.2  2005/10/24 04:37:31  fwarmerdam
#  use MEMO field for wkt geometry in access
#
#  Revision 1.1  2005/10/24 04:05:42  fwarmerdam
#  New
#
#

import osr
import ogr
import string
import sys

#############################################################################
def Usage():
    print 'Usage: load2odbc.py infile odbc_dsn layer'
    print
    sys.exit(1)

#############################################################################
# Argument processing.

infile = None
outfile = None

if len(sys.argv) != 4:
    Usage()

infile = sys.argv[1]
odbc_dsn = sys.argv[2]
layername = sys.argv[3]

#############################################################################
# Open the datasource to operate on.

in_ds = ogr.Open( infile, update = 0 )

in_layer = in_ds.GetLayerByName( layername )

if in_layer is None:
    print 'Did not find layer: ', layername
    sys.exit( 1 )

#############################################################################
#	Connect to ODBC DSN.

if len(odbc_dsn) < 6 or odbc_dsn[:5] != 'ODBC:':
    odbc_dsn = 'ODBC:' + odbc_dsn

out_ds = ogr.Open( odbc_dsn )

if out_ds is None:
    print 'Unable to connect to ' + odbc_dsn 
    sys.exit(1)

#############################################################################
#	Fetch layer definition, and defined output table on the same basis.

try:
    out_ds.ExecuteSQL( 'drop table ' + layername )
except:
    pass

defn = in_layer.GetLayerDefn()

cmd = 'CREATE TABLE ' + layername + '( OGC_FID INTEGER, WKT_GEOMETRY MEMO ' 

for iField in range(defn.GetFieldCount()):
    fielddef = defn.GetFieldDefn(iField)
    cmd = cmd + ', ' + fielddef.GetName()
    if fielddef.GetType() == ogr.OFTInteger:
	cmd = cmd + ' INTEGER' 
    elif fielddef.GetType() == ogr.OFTString:
        cmd = cmd + ' TEXT' 
    elif fielddef.GetType() == ogr.OFTReal:
        cmd = cmd + ' NUMBER'
    else:
	cmd = cmd + ' TEXT' 

cmd = cmd + ')'
print 'ExecuteSQL: ', cmd
result = out_ds.ExecuteSQL( cmd )
if result is not None:
    out_ds.ReleaseResultSet( result )

#############################################################################
# Read all features in the line layer, holding just the geometry in a hash
# for fast lookup by TLID.

in_layer.ResetReading()
feat = in_layer.GetNextFeature()
while feat is not None:
    cmd_start = 'INSERT INTO ' + layername + ' ( OGC_FID '
    cmd_end = ') VALUES (%d' % feat.GetFID()

    geom = feat.GetGeometryRef()
    if geom is not None:
	cmd_start = cmd_start + ', WKT_GEOMETRY'
	cmd_end = cmd_end + ", '" + geom.ExportToWkt() + "'"

    for iField in range(defn.GetFieldCount()):
        fielddef = defn.GetFieldDefn(iField)
        if feat.IsFieldSet( iField ) != 0:
            cmd_start = cmd_start + ', ' + fielddef.GetName()

	    if fielddef.GetType() == ogr.OFTInteger:
		cmd_end = cmd_end + ', ' + feat.GetFieldAsString(iField)
	    elif fielddef.GetType() == ogr.OFTString:
		cmd_end = cmd_end + ", '" + feat.GetFieldAsString(iField) + "'"
	    elif fielddef.GetType() == ogr.OFTReal:
		cmd_end = cmd_end + ', ' + feat.GetFieldAsString(iField)
	    else:
		cmd_end = cmd_end + ", '" + feat.GetFieldAsString(iField) + "'"

    cmd = cmd_start + cmd_end + ')'

    print 'ExecuteSQL: ', cmd
    out_ds.ExecuteSQL( cmd )

    feat.Destroy()
    feat = in_layer.GetNextFeature()

#############################################################################
# Cleanup

in_ds.Destroy()
out_ds.Destroy()
