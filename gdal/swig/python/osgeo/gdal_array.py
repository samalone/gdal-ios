# This file was automatically generated by SWIG (http://www.swig.org).
# Version 1.3.39
#
# Do not make changes to this file unless you know what you are doing--modify
# the SWIG interface file instead.
# This file is compatible with both classic and new-style classes.

from sys import version_info
if version_info >= (2,6,0):
    def swig_import_helper():
        from os.path import dirname
        import imp
        fp = None
        try:
            fp, pathname, description = imp.find_module('_gdal_array', [dirname(__file__)])
        except ImportError:
            import _gdal_array
            return _gdal_array
        if fp is not None:
            try:
                _mod = imp.load_module('_gdal_array', fp, pathname, description)
            finally:
                fp.close()
                return _mod
    _gdal_array = swig_import_helper()
    del swig_import_helper
else:
    import _gdal_array
del version_info
try:
    _swig_property = property
except NameError:
    pass # Python < 2.2 doesn't have 'property'.
def _swig_setattr_nondynamic(self,class_type,name,value,static=1):
    if (name == "thisown"): return self.this.own(value)
    if (name == "this"):
        if type(value).__name__ == 'SwigPyObject':
            self.__dict__[name] = value
            return
    method = class_type.__swig_setmethods__.get(name,None)
    if method: return method(self,value)
    if (not static) or hasattr(self,name):
        self.__dict__[name] = value
    else:
        raise AttributeError("You cannot add attributes to %s" % self)

def _swig_setattr(self,class_type,name,value):
    return _swig_setattr_nondynamic(self,class_type,name,value,0)

def _swig_getattr(self,class_type,name):
    if (name == "thisown"): return self.this.own()
    method = class_type.__swig_getmethods__.get(name,None)
    if method: return method(self)
    raise AttributeError(name)

def _swig_repr(self):
    try: strthis = "proxy of " + self.this.__repr__()
    except: strthis = ""
    return "<%s.%s; %s >" % (self.__class__.__module__, self.__class__.__name__, strthis,)

try:
    _object = object
    _newclass = 1
except AttributeError:
    class _object : pass
    _newclass = 0



def GetArrayFilename(*args):
  """GetArrayFilename(PyArrayObject psArray) -> retStringAndCPLFree"""
  return _gdal_array.GetArrayFilename(*args)

def BandRasterIONumPy(*args, **kwargs):
  """
    BandRasterIONumPy(Band band, int bWrite, int xoff, int yoff, int xsize, 
        int ysize, PyArrayObject psArray, int buf_xsize = None, 
        int buf_ysize = None, int buf_type = None, 
        int buf_pixel_space = None, int buf_line_space = None) -> CPLErr
    """
  return _gdal_array.BandRasterIONumPy(*args, **kwargs)
import numpy
import _gdal_array

import gdalconst
import gdal
gdal.AllRegister()

codes = {   gdalconst.GDT_Byte      :   numpy.uint8,
            gdalconst.GDT_UInt16    :   numpy.uint16,
            gdalconst.GDT_Int16     :   numpy.int16,
            gdalconst.GDT_UInt32    :   numpy.uint32,
            gdalconst.GDT_Int32     :   numpy.int32,
            gdalconst.GDT_Float32   :   numpy.float32,
            gdalconst.GDT_Float64   :   numpy.float64,
            gdalconst.GDT_CInt16    :   numpy.complex64,
            gdalconst.GDT_CInt32    :   numpy.complex64,
            gdalconst.GDT_CFloat32  :   numpy.complex64,
            gdalconst.GDT_CFloat64  :   numpy.complex128
        }

def OpenArray( array, prototype_ds = None ):

    ds = gdal.Open( GetArrayFilename(array) )

    if ds is not None and prototype_ds is not None:
        if type(prototype_ds).__name__ == 'str':
            prototype_ds = gdal.Open( prototype_ds )
        if prototype_ds is not None:
            CopyDatasetInfo( prototype_ds, ds )
            
    return ds
    
    
def flip_code(code):
    if isinstance(code, type):
        # since several things map to complex64 we must carefully select
        # the opposite that is an exact match (ticket 1518)
        if code == numpy.int8:
            return gdalconst.GDT_Byte
        if code == numpy.complex64:
            return gdalconst.GDT_CFloat32
        
        for key, value in codes.items():
            if value == code:
                return key
        return None
    else:
        try:
            return codes[code]
        except KeyError:
            return None

def NumericTypeCodeToGDALTypeCode(numeric_type):
    if not isinstance(numeric_type, type):
        raise TypeError("Input must be a type")
    return flip_code(numeric_type)

def GDALTypeCodeToNumericTypeCode(gdal_code):
    return flip_code(gdal_code)
    
def LoadFile( filename, xoff=0, yoff=0, xsize=None, ysize=None ):
    ds = gdal.Open( filename )
    if ds is None:
        raise ValueError("Can't open "+filename+"\n\n"+gdal.GetLastErrorMsg())

    return DatasetReadAsArray( ds, xoff, yoff, xsize, ysize )

def SaveArray( src_array, filename, format = "GTiff", prototype = None ):
    driver = gdal.GetDriverByName( format )
    if driver is None:
        raise ValueError("Can't find driver "+format)

    return driver.CreateCopy( filename, OpenArray(src_array,prototype) )

def DatasetReadAsArray( ds, xoff=0, yoff=0, xsize=None, ysize=None, buf_obj=None ):

    if xsize is None:
        xsize = ds.RasterXSize
    if ysize is None:
        ysize = ds.RasterYSize

    if ds.RasterCount == 1:
        return BandReadAsArray( ds.GetRasterBand(1), xoff, yoff, xsize, ysize, buf_obj = buf_obj)

    datatype = ds.GetRasterBand(1).DataType
    for band_index in range(2,ds.RasterCount+1):
        if datatype != ds.GetRasterBand(band_index).DataType:
            datatype = gdalconst.GDT_Float32
    
    typecode = GDALTypeCodeToNumericTypeCode( datatype )
    if typecode == None:
        datatype = gdalconst.GDT_Float32
        typecode = numpy.float32

    if buf_obj is not None:
        for band_index in range(1,ds.RasterCount+1):
            BandReadAsArray( ds.GetRasterBand(band_index),
                             xoff, yoff, xsize, ysize, buf_obj = buf_obj[band_index-1])
        return buf_obj
    
    array_list = []
    for band_index in range(1,ds.RasterCount+1):
        band_array = BandReadAsArray( ds.GetRasterBand(band_index),
                                      xoff, yoff, xsize, ysize)
        array_list.append( numpy.reshape( band_array, [1,ysize,xsize] ) )

    return numpy.concatenate( array_list )
            
def BandReadAsArray( band, xoff = 0, yoff = 0, win_xsize = None, win_ysize = None,
                     buf_xsize=None, buf_ysize=None, buf_obj=None ):
    """Pure python implementation of reading a chunk of a GDAL file
    into a numpy array.  Used by the gdal.Band.ReadAsArray method."""

    if win_xsize is None:
        win_xsize = band.XSize
    if win_ysize is None:
        win_ysize = band.YSize

    if buf_obj is None:
        if buf_xsize is None:
            buf_xsize = win_xsize
        if buf_ysize is None:
            buf_ysize = win_ysize
    else:
        if len(buf_obj.shape) == 2:
            if buf_xsize is None:
                buf_xsize = buf_obj.shape[1]
            if buf_ysize is None:
                buf_ysize = buf_obj.shape[0]
        else:
            if buf_xsize is None:
                buf_xsize = buf_obj.shape[2]
            if buf_ysize is None:
                buf_ysize = buf_obj.shape[1]
                
    datatype = band.DataType
    typecode = GDALTypeCodeToNumericTypeCode( datatype )
    if typecode == None:
        datatype = gdalconst.GDT_Float32
        typecode = numpy.float32
    else:
        datatype = NumericTypeCodeToGDALTypeCode( typecode )

    if buf_obj is None:
        if datatype == gdalconst.GDT_Byte and band.GetMetadataItem('PIXELTYPE', 'IMAGE_STRUCTURE') == 'SIGNEDBYTE':
            typecode = numpy.int8
        ar = numpy.empty([buf_ysize,buf_xsize], dtype = typecode)
        if BandRasterIONumPy( band, 0, xoff, yoff, win_xsize, win_ysize,
                                ar, buf_xsize, buf_ysize, datatype ) != 0:
            return None

        return ar
    else:
            
        datatype = NumericTypeCodeToGDALTypeCode( buf_obj.dtype.type )
            
        if BandRasterIONumPy( band, 0, xoff, yoff, win_xsize, win_ysize,
                                buf_obj, buf_xsize, buf_ysize, datatype ) != 0:
            return None

        return buf_obj

def BandWriteArray( band, array, xoff=0, yoff=0 ):
    """Pure python implementation of writing a chunk of a GDAL file
    from a numpy array.  Used by the gdal.Band.WriteArray method."""

    if array is None or len(array.shape) != 2:
        raise ValueError("expected array of dim 2")

    xsize = array.shape[1]
    ysize = array.shape[0]

    if xsize + xoff > band.XSize or ysize + yoff > band.YSize:
        raise ValueError("array larger than output file, or offset off edge")

    datatype = NumericTypeCodeToGDALTypeCode( array.dtype.type )

    # if we receive some odd type, like int64, try casting to a very
    # generic type we do support (#2285)
    if not datatype:
        gdal.Debug( 'gdal_array', 'force array to float64' )
        array = array.astype( numpy.float64 )
        datatype = NumericTypeCodeToGDALTypeCode( array.dtype.type )
        
    if not datatype:
        raise ValueError("array does not have corresponding GDAL data type")

    return BandRasterIONumPy( band, 1, xoff, yoff, xsize, ysize,
                                array, xsize, ysize, datatype )

    
def CopyDatasetInfo( src, dst, xoff=0, yoff=0 ):
    """
    Copy georeferencing information and metadata from one dataset to another.
    src: input dataset
    dst: output dataset - It can be a ROI - 
    xoff, yoff:  dst's offset with respect to src in pixel/line.  
    
    Notes: Destination dataset must have update access.  Certain formats
           do not support creation of geotransforms and/or gcps.

    """

    dst.SetMetadata( src.GetMetadata() )
                    


    #Check for geo transform
    gt = src.GetGeoTransform()
    if gt != (0,1,0,0,0,1):
        dst.SetProjection( src.GetProjectionRef() )
        
        if (xoff == 0) and (yoff == 0):
            dst.SetGeoTransform( gt  )
        else:
            ngt = [gt[0],gt[1],gt[2],gt[3],gt[4],gt[5]]
            ngt[0] = gt[0] + xoff*gt[1] + yoff*gt[2];
            ngt[3] = gt[3] + xoff*gt[4] + yoff*gt[5];
            dst.SetGeoTransform( ( ngt[0], ngt[1], ngt[2], ngt[3], ngt[4], ngt[5] ) )
            
    #Check for GCPs
    elif src.GetGCPCount() > 0:
        
        if (xoff == 0) and (yoff == 0):
            dst.SetGCPs( src.GetGCPs(), src.GetGCPProjection() )
        else:
            gcps = src.GetGCPs()
            #Shift gcps
            new_gcps = []
            for gcp in gcps:
                ngcp = gdal.GCP()
                ngcp.GCPX = gcp.GCPX 
                ngcp.GCPY = gcp.GCPY
                ngcp.GCPZ = gcp.GCPZ
                ngcp.GCPPixel = gcp.GCPPixel - xoff
                ngcp.GCPLine = gcp.GCPLine - yoff
                ngcp.Info = gcp.Info
                ngcp.Id = gcp.Id
                new_gcps.append(ngcp)

            try:
                dst.SetGCPs( new_gcps , src.GetGCPProjection() )
            except:
                print ("Failed to set GCPs")
                return

    return



