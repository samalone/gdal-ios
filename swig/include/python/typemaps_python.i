/******************************************************************************
 * $Id$
 *
 * Name:     gdal.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *

 *
 * $Log$
 * Revision 1.15  2005/02/18 16:54:35  kruland
 * Removed IGNORE_RC exception macro.
 * Removed fragments.i %include because it's not included in swig 1.3.24.
 * Defined out typemap IF_ERR_RETURN_NONE.
 * Defined out typemap THROW_OGR_ERROR.  (untested).
 *
 * Revision 1.14  2005/02/17 21:14:48  kruland
 * Use swig library's typemaps.i and fragments.i to support returning
 * multiple argument values as a tuple.  Use this in all the custom
 * argout typemaps.
 *
 * Revision 1.13  2005/02/17 17:27:13  kruland
 * Changed the handling of fixed size double arrays to make it fit more
 * naturally with GDAL/OSR usage.  Declare as typedef double * double_17;
 * If used as return argument use:  function ( ... double_17 argout ... );
 * If used as value argument use: function (... double_17 argin ... );
 *
 * Revision 1.12  2005/02/17 03:42:10  kruland
 * Added macro to define typemaps for optional arguments to functions.
 * The optional argument must be coded as a pointer in the function decl.
 * The function must properly interpret a null pointer as default.
 *
 * Revision 1.11  2005/02/16 17:49:40  kruland
 * Added in typemap for Lists of GCPs.
 * Added 'python' to all the freearg typemaps too.
 *
 * Revision 1.10  2005/02/16 17:18:03  hobu
 * put "python" name on the typemaps that are specific
 * to python
 *
 * Revision 1.9  2005/02/16 16:53:45  kruland
 * Minor comment change.
 *
 * Revision 1.8  2005/02/15 20:53:11  kruland
 * Added typemap(in) char ** from PyString which allows the pointer (to char*)
 * to change but assumes the contents do not change.
 *
 * Revision 1.7  2005/02/15 19:49:42  kruland
 * Added typemaps for arbitrary char* buffers with length.
 *
 * Revision 1.6  2005/02/15 17:05:13  kruland
 * Use CPLParseNameValue instead of strchr() in typemap(out) char **dict.
 *
 * Revision 1.5  2005/02/15 16:52:41  kruland
 * Added a swig macro for handling fixed length double array arguments.  Used
 * for Band::ComputeMinMax( double[2] ), and Dataset::?etGeoTransform() methods.
 *
 * Revision 1.4  2005/02/15 06:00:28  kruland
 * Fixed critical bug in %typemap(in) std::vector<double>.  Used incorrect python
 * parse call.
 * Removed some stray cout's.
 * Gave the "argument" to the %typemap(out) char** mapping.  This makes it easier
 * to control its application.
 *
 * Revision 1.3  2005/02/14 23:56:02  hobu
 * Added log info and C99-style comments
 *
 *
*/

/*
 * The typemaps defined here use the code fragment called:
 * t_out_helper which is defined in the pytuplehlp.swg file.
 * The *.swg library files are considered "swig internal".
 * fortunately, pytuplehlp.swg is included by typemaps.i
 * which we need anyway.
 */

/*
 * Include the typemaps from swig library for returning of
 * standard types through arguments.
 */
%include "typemaps.i"

%apply (double *OUTPUT) { double *argout };

/*
 *
 * Define a simple return code typemap
 * which checks if the return code from
 * the wrapped method is non-zero.
 * If non-zero, return None.  Otherwise,
 * return normally.  It is particularly
 * useful when the return value is through
 * arguments.
 *
 * Applied like this:
 * %apply (IF_ERR_RETURN_NONE) {CPLErr};
 * CPLErr function_to_wrap( );
 * %clear (CPLErr);
 */
%typemap(out) IF_ERR_RETURN_NONE
{
  /* %typemap(out) IF_ERR_RETURN_NONE */
  resultobj = 0;
}
%typemap(ret) IF_ERR_RETURN_NONE
{
 /* %typemap(ret) IF_ERR_RETURN_NONE */
  if (result != 0 ) {
    Py_XDECREF( resultobj );
    resultobj = Py_None;
    Py_INCREF(resultobj);
  }
}

/*
 * Another output typemap which will raise an
 * exception on error.
 *
 */
%typemap(out) THROW_OGR_ERROR
{
  /* %typemap(out) THROW_OGR_ERROR */
  resultobj = 0;
  if ( result != 0) {
    char *errMsg = "OGR Error %02d";
    sprintf(errMsg,result);
    PyErr_SetString( PyExc_RuntimeError, errMsg );
    SWIG_fail;
  }
}

/*
 * SWIG macro to define fixed length array typemaps
 *
 * defines the following:
 *
 * typemap(in,numinputs=0) double_size *argout
 * typemap(out) double_size *argout
 *
 * which matches decls like:  Dataset::GetGeoTransform( double_6 *c_transform )
 * where c_transform is a returned argument.
 *
 * typemap(in) double_size argin
 *
 * which matches decls like: Dataset::SetGeoTransform( double_6 c_transform )
 * where c_transform is an input variable.
 *
 * The actual typedef for these new types needs to be in gdal.i in the %{..%} block
 * like this:
 *
 * %{
 *   ....
 *   typedef double double_6[6];
 * %}
 */

%define ARRAY_TYPEMAP(size)
%typemap(python,in,numinputs=0) ( double_ ## size argout) (double argout[size])
{
  /* %typemap(in,numinputs=0) (double_ ## size argout) */
  $1 = argout;
}
%typemap(python,argout,fragment="t_output_helper") ( double_ ## size argout)
{
  /* %typemap(argout) (double_ ## size *argout) */
  PyObject *out = PyTuple_New( size );
  for( unsigned int i=0; i<size; i++ ) {
    PyObject *val = PyFloat_FromDouble( $1[i] );
    PyTuple_SetItem( out, i, val );
  }
  $result = t_output_helper($result,out);
}
%typemap(python,in) (double_ ## size argin) (double argin[size])
{
  /* %typemap(python,in) (double_ ## size argin) */
  $1 = argin;
  if (! PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }
  int seq_size = PySequence_Size($input);
  if ( seq_size != size ) {
    PyErr_SetString(PyExc_TypeError, "sequence must have length ##size");
    SWIG_fail;
  }
  for (unsigned int i=0; i<size; i++) {
    PyObject *o = PySequence_GetItem($input,i);
    double val;
    PyArg_Parse(o, "d", &val );
    $1[i] =  val;
  }
}
%enddef

/*
 * Typemap for double c_minmax[2]. 
 * Used in Band::ComputeMinMax()
 */
ARRAY_TYPEMAP(2);

/*
 * Typemap for double c_transform[6]
 * Used in Dataset::GetGeoTransform(), Dataset::SetGeoTransform().
 */
ARRAY_TYPEMAP(6);

// Used by SpatialReference
ARRAY_TYPEMAP(7);
ARRAY_TYPEMAP(15);
ARRAY_TYPEMAP(17);

// Used by CoordinateTransformation::TransformPoint()
ARRAY_TYPEMAP(3);

/*
 *  Typemap for counted arrays of ints <- PySequence
 */
%typemap(python,in,numargs=1) (int nList, int* pList)
{
  /* %typemap(in,numargs=1) (int nList, int* pList)*/
  /* check if is List */
  if ( !PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }
  $1 = PySequence_Size($input);
  $2 = (int*) malloc($1*sizeof(int));
  for( int i = 0; i<$1; i++ ) {
    PyObject *o = PySequence_GetItem($input,i);
    if ( !PyArg_Parse(o,"i",&$2[i]) ) {
      SWIG_fail;
    }
  }
}
%typemap(python,freearg) (int nList, int* pList)
{
  /* %typemap(python,freearg) (int nList, int* pList) */
  if ($2) {
    free((void*) $2);
  }
}

/*
 * Typemap for buffers with length <-> PyStrings
 * Used in Band::ReadRaster() and Band::WriteRaster()
 *
 * This typemap has a typecheck also since the WriteRaster()
 * methods are overloaded.
 */
%typemap(python,in,numinputs=0) (int *nLen, char **pBuf ) ( int nLen, char *pBuf )
{
  /* %typemap(in,numinputs=0) (int *nLen, char **pBuf ) */
  $1 = &nLen;
  $2 = &pBuf;
}
%typemap(python,argout) (int *nLen, char **pBuf )
{
  /* %typemap(argout) (int *nLen, char **pBuf ) */
  Py_DECREF($result);
  $result = PyString_FromStringAndSize( *$2, *$1 );
}
%typemap(python,freearg) (int *nLen, char **pBuf )
{
  /* %typemap(python,freearg) (int *nLen, char **pBuf ) */
  if( $1 ) {
    free( *$2 );
  }
}
%typemap(python,in,numinputs=1) (int nLen, char *pBuf )
{
  /* %typemap(in,numinputs=1) (int nLen, char *pBuf ) */
  PyString_AsStringAndSize($input, &$2, &$1 );
}
%typemap(python,typecheck,precedence=SWIG_TYPECHECK_POINTER)
        (int nLen, char *pBuf)
{
  /* %typecheck(SWIG_TYPECHECK_POINTER) (int nLen, char *pBuf) */
  $1 = (PyString_Check($input)) ? 1 : 0;
}

/*
 * Typemap argout of GDAL_GCP* used in Dataset::GetGCPs( )
 */
%typemap(python,in,numinputs=0) (int *nGCPs, GDAL_GCP const **pGCPs ) (int nGCPs, GDAL_GCP *pGCPs )
{
  /* %typemap(in,numinputs=0) (int *nGCPs, GDAL_GCP const **pGCPs ) */
  $1 = &nGCPs;
  $2 = &pGCPs;
}
%typemap(python,argout) (int *nGCPs, GDAL_GCP const **pGCPs )
{
  /* %typemap(argout) (int *nGCPs, GDAL_GCP const **pGCPs ) */
  PyObject *dict = PyTuple_New( *$1 );
  for( int i = 0; i < *$1; i++ ) {
    PyTuple_SetItem(dict, i, 
      Py_BuildValue("(ssddddd)", 
                    (*$2)[i].pszId,
                    (*$2)[i].pszInfo,
                    (*$2)[i].dfGCPPixel,
                    (*$2)[i].dfGCPLine,
                    (*$2)[i].dfGCPX,
                    (*$2)[i].dfGCPY,
                    (*$2)[i].dfGCPZ ) );
  }
  Py_DECREF($result);
  $result = dict;
}
%typemap(python,in,numinputs=1) (int nGCPs, GDAL_GCP const *pGCPs ) ( GDAL_GCP *tmpGCPList )
{
  /* %typemap(python,in,numinputs=1) (int nGCPs, GDAL_GCP const *pGCPs ) */
  /* check if is List */
  if ( !PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }
  $1 = PySequence_Size($input);
  tmpGCPList = (GDAL_GCP*) malloc($1*sizeof(GDAL_GCP));
  $2 = tmpGCPList;
  for( int i = 0; i<$1; i++ ) {
    PyObject *o = PySequence_GetItem($input,i);
    if ( !PyArg_ParseTuple(o,"ssddddd:Parse GCP List",
                      &tmpGCPList->pszId,
                      &tmpGCPList->pszInfo,
                      &tmpGCPList->dfGCPPixel,
                      &tmpGCPList->dfGCPLine,
                      &tmpGCPList->dfGCPX,
                      &tmpGCPList->dfGCPY,
                      &tmpGCPList->dfGCPZ) ) {
      SWIG_fail;
    }
    ++tmpGCPList;
  }
}
%typemap(python,freearg) (int nGCPs, GDAL_GCP const *pGCPs )
{
  /* %typemap(python,freearg) (int nGCPs, GDAL_GCP const *pGCPs ) */
  if ($2) {
    free( (void*) $2 );
  }
}

/*
 * Typemap for GDALColorEntry* <-> tuple
 */
%typemap(python,out) GDALColorEntry*
{
  /*  %typemap(out) GDALColorEntry* */
   $result = Py_BuildValue( "(hhhh)", (*$1).c1,(*$1).c2,(*$1).c3,(*$1).c4);
}

%typemap(python,in) GDALColorEntry*
{
  /* %typemap(in) GDALColorEntry* */
   
   GDALColorEntry ce = {255,255,255,255};
   int size = PySequence_Size($input);
   if ( size > 4 ) {
     PyErr_SetString(PyExc_TypeError, "sequence too long");
     SWIG_fail;
   }
   PyArg_ParseTuple( $input,"hhh|h", &ce.c1, &ce.c2, &ce.c3, &ce.c4 );
   $1 = &ce;
}

/*
 * Typemap char ** -> dict
 */
%typemap(python,out) char **dict
{
  /* %typemap(out) char ** -> to hash */
  char **stringarray = $1;
  $result = PyDict_New();
  if ( stringarray != NULL ) {
    while (*stringarray != NULL ) {
      char const *valptr;
      char *keyptr;
      valptr = CPLParseNameValue( *stringarray, &keyptr );
      if ( valptr != 0 ) {
        PyObject *nm = PyString_FromString( keyptr );
        PyObject *val = PyString_FromString( valptr );
        PyDict_SetItem($result, nm, val );
        CPLFree( keyptr );
      }
      stringarray++;
    }
  }
}

/*
 * Typemap char **<- dict
 */
%typemap(python,in) char **dict
{
  /* %typemap(in) char **dict */
  if ( ! PyMapping_Check( $input ) ) {
    PyErr_SetString(PyExc_TypeError,"not supports mapping (dict) protocol");
    SWIG_fail;
  }
  $1 = NULL;
  int size = PyMapping_Length( $input );
  if ( size > 0 ) {
    PyObject *item_list = PyMapping_Items( $input );
    for( int i=0; i<size; i++ ) {
      PyObject *it = PySequence_GetItem( item_list, i );
      char *nm;
      char *val;
      PyArg_ParseTuple( it, "ss", &nm, &val );
      $1 = CSLAddNameValue( $1, nm, val );
    }
  }
}
%typemap(python,freearg) char **dict
{
  /* %typemap(python,freearg) char **dict */
  CSLDestroy( $1 );
}

/*
 * Typemap maps char** arguments from Python Sequence Object
 */
%typemap(python,in) char **options
{
  /* %typemap(in) char **options */
  /* Check if is a list */
  if ( ! PySequence_Check($input)) {
    PyErr_SetString(PyExc_TypeError,"not a sequence");
    SWIG_fail;
  }

  int size = PySequence_Size($input);
  for (int i = 0; i < size; i++) {
    char *pszItem = NULL;
    if ( ! PyArg_Parse( PySequence_GetItem($input,i), "s", &pszItem ) ) {
      PyErr_SetString(PyExc_TypeError,"sequence must contain strings");
      SWIG_fail;
    }
    $1 = CSLAddString( $1, pszItem );
  }
}
%typemap(python,freearg) char **options
{
  /* %typemap(python,freearg) char **options */
  CSLDestroy( $1 );
}

/*
 * Typemaps map mutable char ** arguments from PyStrings.  Does not
 * return the modified argument
 */
%typemap(python,in) (char **ignorechange) ( char *val )
{
  /* %typemap(in) (char **ignorechange) */
  PyArg_Parse( $input, "s", &val );
  $1 = &val;
}

/*
 * Typemap for char **argout.
 */
%typemap(python,in,numinputs=0) (char **argout) ( char *argout=0 )
{
  /* %typemap(python,in,numinputs=0) (char **argout) */
  $1 = &argout;
}
%typemap(python,argout,fragment="t_output_helper") (char **argout)
{
  /* %typemap(python,argout) (char **argout) */
  PyObject *o = PyString_FromString( *$1 );
  $result = t_output_helper($result, o);
}
%typemap(python,freearg) (char **argout)
{
  /* %typemap(python,freearg) (char **argout) */
  if ( *$1 )
    CPLFree( *$1 );
}

/*
 * Typemap for an optional POD argument.
 * Declare function to take POD *.  If the parameter
 * is NULL then the function needs to define a default
 * value.
 */
%define OPTIONAL_POD(type,argstring)
%typemap(python,in) (type *optional_##type) ( type val )
{
  /* %typemap(python,in) (type *optional_##type) */
  if ( $input == Py_None ) {
    $1 = 0;
  }
  else if ( PyArg_Parse( $input, #argstring ,&val ) ) {
    $1 = &val;
  }
  else {
    PyErr_SetString( PyExc_TypeError, "Invalid Parameter" );
    SWIG_fail;
  }
}
%typemap(python,typecheck,precedence=0) (type *optional_##type)
{
  /* %typemap(python,typecheck,precedence=0) (type *optionalInt) */
  $1 = (($input==Py_None) || my_PyCheck_##type($input)) ? 1 : 0;
}
%enddef

OPTIONAL_POD(int,i);
