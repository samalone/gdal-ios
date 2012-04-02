#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ARG Testing.
# Author:   David Zwarg <dzwarg@azavea.com>
# 
###############################################################################
# Copyright (c) 2012, David Zwarg <dzwarg@azavea.com>
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
import struct
from copy import copy
try:
    from osgeo import gdal
except ImportError:
    import gdal

sys.path.append( '../pymod' )

import gdaltest

# given fmt and nodata, encodes a value as bytes
def pack(fmt, nodata, value):
    if value is None: value = nodata
    return struct.pack(fmt, value)

# packs the given values together as bytes
def encode(fmt, nodata, values):
    chunks = [pack(fmt, nodata, v) for v in values]
    return ''.join(chunks)

###############################################################################
# 
def arg_init():
    try:
        gdaltest.argDriver = gdal.GetDriverByName('ARG')
    except Exception, ex:
        gdaltest.argDriver = None

    if gdaltest.argDriver is None:
        return 'skip'
        
    gdaltest.argJsontpl = """{
    "layer": "%(fmt)s",
    "type": "arg",
    "datatype": "%(dt)s",
    "xmin": %(xmin)f,
    "ymin": %(ymin)f,
    "xmax": %(xmax)f,
    "ymax": %(ymax)f,
    "cellwidth": %(width)f,
    "cellheight": %(height)f,
    "rows": %(rows)d,
    "cols": %(cols)d
}"""
    gdaltest.argDefaults = {
        'xmin':0.0,
        'ymin':0.0,
        'xmax':2.0,
        'ymax':2.0,
        'width':1.0,
        'height':1.0,
        'rows':2,
        'cols':2
    }

    # None means "no data"
    gdaltest.argTests = [
        {'formats': [('int8', '>b', -(1<<7)),
                     ('int16', '>h', -(1<<15)),
                     ('int32', '>i', -(1<<31)),
                     ('int64', '>q', -(1<<63))],
         'data': [None, 2, -3, -4]},
        {'formats': [('uint8', '>B', (1<<8)-1),
                     ('uint16', '>H', (1<<16)-1),
                     ('uint32', '>I', (1<<32)-1),
                     ('uint64', '>Q', (1<<64)-1)],
         'data': [None, 2, 3, 4]},
        {'formats': [('float32', '>f', float('nan')),
                     ('float64', '>d', float('nan'))],
         'data': [None, 1.1, -20.02, 300.003]},
    ]

    for d in gdaltest.argTests:
        for (name, fmt, nodata) in d['formats']:
            bytes = encode(fmt, nodata, d['data'])
            arg = open('data/arg-'+name+'.arg', 'wb')
            arg.write(bytes)
            arg.close()

            meta = copy(gdaltest.argDefaults)
            meta.update(fmt='arg-'+name,dt=name)
            json = open('data/arg-'+name+'.json', 'w')
            json.write( gdaltest.argJsontpl % meta )
            json.close()

    try:
        ds = gdal.Open('data/arg-'+gdaltest.argTests[0]['formats'][1][0]+'.arg')
    except Exception, ex:
        gdaltest.argDriver = None

    if ds is None:
        gdaltest.argDriver = None

    if gdaltest.argDriver is None:
        return 'skip'

    return 'success'

def arg_unsupported():
    if gdaltest.argDriver is None:
        return 'skip'

    # int8 is unsupported
    for d in gdaltest.argTests:
        for (name, fmt, nodata) in d['formats']:
            if name == 'int64' or name == 'uint64':
                ds = gdal.Open('data/arg-'+name+'.arg')
                if not ds is None:
                    return 'fail'
            else:
                ds = gdal.Open('data/arg-'+name+'.arg')
                if ds is None:
                    return 'fail'

    return 'success'

def arg_getrastercount():
    if gdaltest.argDriver is None:
        return 'skip'

    for d in gdaltest.argTests:
        for (name, fmt, nodata) in d['formats']:
            ds = gdal.Open('data/arg-'+name+'.arg')
            if ds is None:
                continue

            if ds.RasterCount != 1:
                return 'fail'

    return 'success'

def arg_getgeotransform():
    if gdaltest.argDriver is None:
        return 'skip'

    for d in gdaltest.argTests:
        for (name, fmt, nodata) in d['formats']:
            ds = gdal.Open('data/arg-'+name+'.arg')
            if ds is None:
                continue

            gt = ds.GetGeoTransform()

            if gt[0] != 0 or \
                gt[1] != 1 or \
                gt[2] != 0 or \
                gt[3] != 2 or \
                gt[4] != 0 or \
                gt[5] != -1:
                return 'fail'

    return 'success'

def arg_destroy():
    if gdaltest.argDriver is None:
        return 'skip'

    for d in gdaltest.argTests:
        for (name, fmt, nodata) in d['formats']:
            os.remove('data/arg-'+name+'.arg')
            os.remove('data/arg-'+name+'.json')

    return 'success'
    

gdaltest_list = [
    arg_init,
    arg_unsupported,
    arg_getrastercount,
    arg_getgeotransform,
    arg_destroy]

if __name__ == '__main__':

    gdaltest.setup_run( 'ARG' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
