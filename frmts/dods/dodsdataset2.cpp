/******************************************************************************
 * $Id$
 *
 * Project:  OPeNDAP Raster Driver
 * Purpose:  Implements DODSDataset and DODSRasterBand classes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2003 OPeNDAP, Inc.
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
 ******************************************************************************
 * $Log$
 * Revision 1.1  2004/10/14 20:54:23  fwarmerdam
 * New
 *
 */

#include <string>
#include <sstream>
#include <algorithm>
#include <exception>

#include <debug.h>

#include <BaseType.h>		// DODS
#include <Byte.h>
#include <Int16.h>
#include <UInt16.h>
#include <Int32.h>
#include <UInt32.h>
#include <Float32.h>
#include <Float64.h>
#include <Str.h>
#include <Url.h>
#include <Array.h>
#include <Structure.h>
#include <Sequence.h>
#include <Grid.h>

#include <AISConnect.h>		
#include <DDS.h>
#include <DAS.h>
#include <Error.h>
#include <escaping.h>

#include "gdal_priv.h"		// GDAL
#include "ogr_spatialref.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
void GDALRegister_DODS(void);
CPL_C_END

/** Attribute names used to encode geo-referencing information. Note that
    these are not C++ objects to avoid problems with static global
    constructors. 

    @see get_geo_info

    @name Attribute names */
//@{
const char *nlat = "Northernmost_Latitude"; ///<
const char *slat = "Southernmost_Latitude"; ///<
const char *wlon = "Westernmost_Longitude"; ///<
const char *elon = "Easternmost_Longitude"; ///<
const char *gcs = "GeographicCS";	    ///<
const char *pcs = "ProjectionCS";	    ///<
const char *norm_proj_param = "Norm_Proj_Param"; ///<
const char *spatial_ref = "spatial_ref";    ///<
//@}

/** Find the variable in the DDS or DataDDS, given its name. This function
    first looks for the name as given. If that can't be found, it determines
    the leaf name of a fully qualified name and looks for that (the DAP
    supports searching for leaf names as a short cut). In this case the
    driver is using that feature because of an odd problem in the responses
    returned by some servers when they are asked for a single array variable
    from a Grid. Instead of returning GRID_NAME.ARRAY_NAME, they return just
    ARRAY_NAME. That's really a bug in the spec. However, it means that if a
    CE says GRID_NAME.ARRAY_NAME and the code looks only for that, it may not
    be found because the nesting has been removed and only an array called
    ARRAY_NAME returned.

    @param dds Look in this DDS object
    @param n Names the variable to find.
    @return A BaseType pointer to the object/variable in the DDS \e dds. */

static BaseType *
get_variable(DDS &dds, const string &n)
{
    BaseType *poBT = dds.var(www2id(n));
    if (!poBT) {
	try {
	    string leaf = n.substr(n.find_last_of('.')+1);
	    poBT = dds.var(www2id(leaf));
	}
	catch (const std::exception &e) {
	    poBT = 0;
	}
    }

    return poBT;
}

/************************************************************************/
/* ==================================================================== */
/*                              DODSDataset                             */
/* ==================================================================== */
/************************************************************************/

class DODSDataset : public GDALDataset
{
private:
    AISConnect *poConnect; 	//< Virtual connection to the data source

    string oURL;		//< data source URL
    double adfGeoTransform[6];
    string oWKT;		//< Constructed WKT string

    DAS    oDAS;
    DDS    oDDS;

    AISConnect *connect_to_server() throw(Error);

    static string      SubConstraint( string raw_constraint, 
                                      string x_constraint, 
                                      string y_constraint );

    char            **CollectBandsFromDDS();
    char            **CollectBandsFromDDSVar( string, char ** );
    char            **ParseBandsFromURL( string );
    
    friend class DODSRasterBand;

public:
    // Overridden GDALDataset methods
    CPLErr GetGeoTransform(double *padfTransform);
    const char *GetProjectionRef();

    /// Open is not a method in GDALDataset; it's the driver.
    static GDALDataset *Open(GDALOpenInfo *);

    /// Return the connection object
    AISConnect *GetConnect() { return poConnect; }

    /// Return the data source URL
    string GetUrl() { return oURL; }
    DAS &GetDAS() { return oDAS; }
    DDS &GetDDS() { return oDDS; }
};

/************************************************************************/
/* ==================================================================== */
/*                            DODSRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class DODSRasterBand : public GDALRasterBand 
{
private:
    string oVarName;          
    string oCE;		        // Holds the CE (with [x] and [y] still there

    friend class DODSDataset;

    int		   nOverviewCount;
    DODSRasterBand **papoOverviewBand;

    int    nOverviewFactor;     // 1 for base, or 2/4/8 for overviews.
    int    bTranspose;
    int    bFlipX;
    int    bFlipY;

public:
    DODSRasterBand( DODSDataset *poDS, string oVarName, string oCE, 
                    int nOverviewFactor );

    virtual int    GetOverviewCount();
    virtual GDALRasterBand *GetOverview( int );
    virtual CPLErr IReadBlock(int, int, void *);
};

/************************************************************************/
/* ==================================================================== */
/*                              DODSDataset                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         connect_to_server()                          */
/************************************************************************/

AISConnect *
DODSDataset::connect_to_server() throw(Error)
{
    // does the string start with 'http?'
    if (oURL.find("http://") == string::npos
	&& oURL.find("https://") == string::npos)
	throw Error(
            "The URL does not start with 'http' or 'https,' I won't try connecting.");

/* -------------------------------------------------------------------- */
/*      Do we want to override the .dodsrc file setting?  Only do       */
/*      the putenv() if there isn't already a DODS_CONF in the          */
/*      environment.                                                    */
/* -------------------------------------------------------------------- */
    if( CPLGetConfigOption( "DODS_CONF", NULL ) != NULL 
        && getenv("DODS_CONF") == NULL )
    {
        static char szDODS_CONF[1000];
            
        sprintf( szDODS_CONF, "DODS_CONF=%.980s", 
                 CPLGetConfigOption( "DODS_CONF", "" ) );
        putenv( szDODS_CONF );
    }

/* -------------------------------------------------------------------- */
/*      If we have a overridding AIS file location, apply it now.       */
/* -------------------------------------------------------------------- */
    if( CPLGetConfigOption( "DODS_AIS_FILE", NULL ) != NULL )
    {
        string oAISFile = CPLGetConfigOption( "DODS_AIS_FILE", "" );
        RCReader::instance()->set_ais_database( oAISFile );
    }

/* -------------------------------------------------------------------- */
/*      Connect, and fetch version information.                         */
/* -------------------------------------------------------------------- */
    AISConnect *poConnection = new AISConnect(oURL);
    string version = poConnection->request_version();
    if (version.empty() || version.find("/3.") == string::npos)
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "I connected to the URL but could not get a DAP 3.x version string\n"
                  "from the server.  I will continue to connect but access may fail.");
    }

    return poConnection;
}

/************************************************************************/
/*                           SubConstraint()                            */
/*                                                                      */
/*      Substitute into x and y constraint expressions in template      */
/*      constraint string for the [x] and [y] parts.                    */
/************************************************************************/

string DODSDataset::SubConstraint( string raw_constraint, 
                                   string x_constraint, 
                                   string y_constraint )

{
    string::size_type x_off, y_off;
    string final_constraint;

    x_off = raw_constraint.find( "[x]" );
    y_off = raw_constraint.find( "[y]" );

    CPLAssert( x_off != string::npos && y_off != string::npos );

    if( x_off < y_off )
        final_constraint = 
            raw_constraint.substr( 0, x_off )
            + x_constraint
            + raw_constraint.substr( x_off + 3, y_off - x_off - 3 )
            + y_constraint
            + raw_constraint.substr( y_off + 3 );
    else
        final_constraint = 
            raw_constraint.substr( 0, y_off )
            + y_constraint
            + raw_constraint.substr( y_off + 3, x_off - y_off - 3 )
            + x_constraint
            + raw_constraint.substr( x_off + 3 );

    return final_constraint;
}

/************************************************************************/
/*                        CollectBandsFromDDS()                         */
/*                                                                      */
/*      If no constraint/variable list is provided we will scan the     */
/*      DDS output for arrays or grids that look like bands and         */
/*      return the list of them with "guessed" [y][x] constraint        */
/*      strings.                                                        */
/*                                                                      */
/*      We pick arrays or grids with at least two dimensions as         */
/*      candidates.  After the first we only accept additional          */
/*      objects as bands if they match the size of the original.        */
/*                                                                      */
/*      Auto-recognision rules will presumably evolve over time to      */
/*      recognise different common configurations and to support        */
/*      more variations.                                                */
/************************************************************************/

char **DODSDataset::CollectBandsFromDDS()

{
    DDS::Vars_iter v_i;
    char **papszResultList = NULL;

    for( v_i = oDDS.var_begin(); v_i != oDDS.var_end(); v_i++ )
    {
        BaseType *poVar = *v_i;
        papszResultList = CollectBandsFromDDSVar( poVar->name(), 
                                                  papszResultList );
    }

    return papszResultList;
}

/************************************************************************/
/*                       CollectBandsFromDDSVar()                       */
/*                                                                      */
/*      Collect zero or more band definitions (varname + CE) for the    */
/*      passed variable.  If it is inappropriate then nothing is        */
/*      added to the list.  This method is shared by                    */
/*      CollectBandsFromDDS(), and by ParseBandsFromURL() when it       */
/*      needs a default constraint expression generated.                */
/************************************************************************/

char **DODSDataset::CollectBandsFromDDSVar( string oVarName, 
                                            char **papszResultList )

{
    Array *poArray;
    Grid *poGrid = NULL;
    
/* -------------------------------------------------------------------- */
/*      Is this a grid or array?                                        */
/* -------------------------------------------------------------------- */
    BaseType *poVar = get_variable( GetDDS(), oVarName );

    if( poVar->type() == dods_array_c )
    {
        poGrid = NULL;
        poArray = dynamic_cast<Array *>( poVar );
    }
    else if( poVar->type() == dods_grid_c )
    {
        poGrid = dynamic_cast<Grid *>( poVar );
        poArray = dynamic_cast<Array *>( poGrid->array_var() );
    }
    else
        return papszResultList;

/* -------------------------------------------------------------------- */
/*      Eventually we will want to support arrays with more than two    */
/*      dimensions ... but not quite yet.                               */
/* -------------------------------------------------------------------- */
    if( poArray->dimensions() != 2 )
        return papszResultList;

/* -------------------------------------------------------------------- */
/*      Get the dimension information for this variable.                */
/* -------------------------------------------------------------------- */
    Array::Dim_iter dim1 = poArray->dim_begin() + 0;
    Array::Dim_iter dim2 = poArray->dim_begin() + 1;
    
    int nDim1Size = poArray->dimension_size( dim1 );
    int nDim2Size = poArray->dimension_size( dim2 );
    
    if( nDim1Size == 1 || nDim2Size == 1 )
        return papszResultList;

/* -------------------------------------------------------------------- */
/*      Try to guess which is x and y.                                  */
/* -------------------------------------------------------------------- */
    string dim1_name = poArray->dimension_name( dim1 );
    string dim2_name = poArray->dimension_name( dim2 );
    int iXDim=-1, iYDim=-1;

    if( dim1_name == "easting" && dim2_name == "northing" )
    {
        iXDim = 0;
        iYDim = 1;
    }
    else if( dim1_name == "easting" && dim2_name == "northing" )
    {
        iXDim = 1;
        iYDim = 0;
    }
    else if( EQUALN(dim1_name.c_str(),"lat",3) 
             && EQUALN(dim2_name.c_str(),"lon",3) )
    {
        iXDim = 0;
        iYDim = 1;
    }
    else if( EQUALN(dim1_name.c_str(),"lon",3) 
             && EQUALN(dim2_name.c_str(),"lat",3) )
    {
        iXDim = 1;
        iYDim = 0;
    }
    else 
    {
        iYDim = 0;
        iXDim = 1;
    }

/* -------------------------------------------------------------------- */
/*      Does this match the established dimension?                      */
/* -------------------------------------------------------------------- */
    if( nRasterXSize == 0 && nRasterYSize == 0 )
    {
        nRasterXSize = poArray->dimension_size( 
            poArray->dim_begin() + iXDim );
        nRasterYSize = poArray->dimension_size( 
            poArray->dim_begin() + iYDim );
    }

    if( nRasterXSize != poArray->dimension_size( 
            poArray->dim_begin() + iXDim ) 
        || nRasterYSize != poArray->dimension_size( 
            poArray->dim_begin() + iYDim ) )
        return papszResultList;

/* -------------------------------------------------------------------- */
/*      OK, we have an acceptable candidate!                            */
/* -------------------------------------------------------------------- */
    string oConstraint;

    if( iXDim == 0 && iYDim == 1 )
        oConstraint = "[x][y]";
    else if( iXDim == 1 && iYDim == 0 )
        oConstraint = "[y][x]";
    else
        return papszResultList;

    papszResultList = CSLAddString( papszResultList, 
                                    poVar->name().c_str() );
    papszResultList = CSLAddString( papszResultList, 
                                    oConstraint.c_str() );

    return papszResultList;
}

/************************************************************************/
/*                         ParseBandsFromURL()                          */
/************************************************************************/

char **DODSDataset::ParseBandsFromURL( string oVarList )

{
    char **papszResultList = NULL;
    char **papszVars = CSLTokenizeString2( oVarList.c_str(), ",", 0 );
    int i;

    for( i = 0; papszVars != NULL && papszVars[i] != NULL; i++ )
    {
        string oVarName;
        string oCE;

/* -------------------------------------------------------------------- */
/*      Split into a varname and constraint equation.                   */
/* -------------------------------------------------------------------- */
        char *pszCEStart = strstr(papszVars[i],"[");

        // If we have no constraints we will have to try to guess
        // reasonable values from the DDS.  In fact, we might end up
        // deriving multiple bands from one variable in this case.
        if( pszCEStart == NULL )
        {
            oVarName = papszVars[i];

            papszResultList = 
                CollectBandsFromDDSVar( oVarName, papszResultList );
        }
        else
        {
            oCE = pszCEStart;
            *pszCEStart = '\0';
            oVarName = papszVars[i];

            // Eventually we should consider supporting a [band] keyword
            // to select a constraint variable that should be used to
            // identify a band dimension ... but not for now. 

            papszResultList = CSLAddString( papszResultList,
                                            oVarName.c_str() );
            papszResultList = CSLAddString( papszResultList, 
                                            oCE.c_str() );
        }
    }

    return papszResultList;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *
DODSDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if( !EQUALN(poOpenInfo->pszFilename,"http://",7) 
        && !EQUALN(poOpenInfo->pszFilename,"https://",8) )
        return NULL;

    DODSDataset *poDS = new DODSDataset();

    poDS->nRasterXSize = 0;
    poDS->nRasterYSize = 0;

    try {
/* -------------------------------------------------------------------- */
/*      Split the URL from the projection/CE portion of the name.       */
/* -------------------------------------------------------------------- */
        string oWholeName( poOpenInfo->pszFilename );
        string oVarList;
        string::size_type t_char;

        t_char = oWholeName.find('?');
        if( t_char == string::npos )
        {
            oVarList = "";
            poDS->oURL = oWholeName;
        }
        else
        {
            poDS->oURL = oWholeName.substr(0,t_char);
            oVarList = oWholeName.substr(t_char+1,oWholeName.length());
        }

/* -------------------------------------------------------------------- */
/*      Get the AISConnect instance and the DAS and DDS for this        */
/*      server.                                                         */
/* -------------------------------------------------------------------- */
	poDS->poConnect = poDS->connect_to_server();
	poDS->poConnect->request_das(poDS->oDAS);
	poDS->poConnect->request_dds(poDS->oDDS);

/* -------------------------------------------------------------------- */
/*      If we are given a constraint/projection list, then parse it     */
/*      into a list of varname/constraint pairs.  Otherwise walk the    */
/*      DDS and try to identify grids or arrays that are good           */
/*      targets and return them in the same format.                     */
/* -------------------------------------------------------------------- */
        char **papszVarConstraintList = NULL;

        if( oVarList.length() == 0 )
            papszVarConstraintList = poDS->CollectBandsFromDDS();
        else
            papszVarConstraintList = poDS->ParseBandsFromURL( oVarList );

/* -------------------------------------------------------------------- */
/*      Did we get any target variables?                                */
/* -------------------------------------------------------------------- */
        if( CSLCount(papszVarConstraintList) == 0 )
            throw Error( "No apparent raster grids or arrays found in DDS.");

/* -------------------------------------------------------------------- */
/*      For now we support only a single band.                          */
/* -------------------------------------------------------------------- */
        DODSRasterBand *poBaseBand = 
            new DODSRasterBand(poDS, 
                               string(papszVarConstraintList[0]),
                               string(papszVarConstraintList[1]), 
                               1 );

        poDS->nRasterXSize = poBaseBand->GetXSize();
        poDS->nRasterYSize = poBaseBand->GetYSize();
            
	poDS->SetBand(1, poBaseBand );

        for( int iBand = 1; papszVarConstraintList[iBand*2] != NULL; iBand++ )
        {
            poDS->SetBand( iBand+1, 
                           new DODSRasterBand(poDS, 
                                   string(papszVarConstraintList[iBand*2+0]),
                                   string(papszVarConstraintList[iBand*2+1]), 
                                              1 ) );
        }

/* -------------------------------------------------------------------- */
/*      Set the georeferencing.                                         */
/* -------------------------------------------------------------------- */
        poDS->oWKT = "";
        poDS->adfGeoTransform[0] = 0.0;
        poDS->adfGeoTransform[1] = 1.0;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = 0.0;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = 1.0;
    }

    catch (Error &e) {
	string msg =
"An error occurred while creating a virtual connection to the DAP server:\n";
	msg += e.get_error_message();
        CPLError(CE_Failure, CPLE_AppDefined, msg.c_str());
	return 0;
    }

    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr 
DODSDataset::GetGeoTransform( double * padfTransform )
{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );

    return CE_None;
}


/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *
DODSDataset::GetProjectionRef()
{
    return oWKT.c_str();
}

/************************************************************************/
/* ==================================================================== */
/*                            DODSRasterBand                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           DODSRasterBand()                           */
/************************************************************************/

DODSRasterBand::DODSRasterBand(DODSDataset *poDSIn, string oVarNameIn, 
                               string oCEIn, int nOverviewFactorIn )
{
    poDS = poDSIn;

    bTranspose = FALSE;
    bFlipX = FALSE;
    bFlipY = FALSE;

    oVarName = oVarNameIn;
    oCE = oCEIn;
    nOverviewFactor = nOverviewFactorIn;

    nOverviewCount = 0;
    papoOverviewBand = NULL;

/* -------------------------------------------------------------------- */
/*      Fetch the DDS definition, and isolate the Array.                */
/* -------------------------------------------------------------------- */
    BaseType *poDDSDef = get_variable( poDSIn->GetDDS(), oVarNameIn );
    if( poDDSDef == NULL )
    {
        throw InternalErr(
            CPLSPrintf( "Could not find DDS definition for variable %s.", 
                        oVarNameIn.c_str() ) );
        return;
    }

    Array *poArray = NULL;
    Grid  *poGrid = NULL;

    if( poDDSDef->type() == dods_grid_c )
    {
        poGrid = dynamic_cast<Grid *>( poDDSDef );
        poArray = dynamic_cast<Array *>( poGrid->array_var() );
    }
    else if( poDDSDef->type() == dods_array_c )
    {
        poArray = dynamic_cast<Array *>( poDDSDef );
    }
    else
    {
        throw InternalErr(
            CPLSPrintf( "Variable %s is not a grid or an array.",
                        oVarNameIn.c_str() ) );
    }

/* -------------------------------------------------------------------- */
/*      Determine the datatype.                                         */
/* -------------------------------------------------------------------- */
    
    // Now grab the data type of the variable.
    switch (poArray->var()->type()) {
      case dods_byte_c: eDataType = GDT_Byte; break;
      case dods_int16_c: eDataType = GDT_Int16; break;
      case dods_uint16_c: eDataType = GDT_UInt16; break;
      case dods_int32_c: eDataType = GDT_Int32; break;
      case dods_uint32_c: eDataType = GDT_UInt32; break;
      case dods_float32_c: eDataType = GDT_Float32; break;
      case dods_float64_c: eDataType = GDT_Float64; break;
      default:
	throw Error("The DODS GDAL driver supports only numeric data types.");
    }

/* -------------------------------------------------------------------- */
/*      For now we hard code to assume that the two dimensions are      */
/*      ysize and xsize.                                                */
/* -------------------------------------------------------------------- */
    if( poArray->dimensions() != 2 )
    {
        throw Error("Variable does not have 2 dimensions.  For now this is required." );
    }

    // TODO: This should clearly be based on the constraint!

    Array::Dim_iter x_dim = poArray->dim_begin() + 1;
    Array::Dim_iter y_dim = poArray->dim_begin() + 0;

    nRasterXSize = poArray->dimension_size( x_dim ) / nOverviewFactor;
    nRasterYSize = poArray->dimension_size( y_dim ) / nOverviewFactor;

/* -------------------------------------------------------------------- */
/*      Decide on a block size.  We aim for a block size of roughly     */
/*      256K.  This should be a big enough chunk to justify a           */
/*      roundtrip to get the data, but small enough to avoid reading    */
/*      too much data.                                                  */
/* -------------------------------------------------------------------- */
    int nBytesPerPixel = GDALGetDataTypeSize( eDataType ) / 8;

    if( nBytesPerPixel == 1 )
    {
        nBlockXSize = 1024;
        nBlockYSize= 256;
    }
    else if( nBytesPerPixel == 2 )
    {
        nBlockXSize = 512;
        nBlockYSize= 256;
    }
    else if( nBytesPerPixel == 4 )
    {
        nBlockXSize = 512;
        nBlockYSize= 128;
    }
    else
    {
        nBlockXSize = 256;
        nBlockYSize= 128;
    }

    if( nRasterXSize < nBlockXSize * 2 )
        nBlockXSize = nRasterXSize;
    
    if( nRasterYSize < nBlockYSize * 2 )
        nBlockYSize = nRasterYSize;

/* -------------------------------------------------------------------- */
/*      Create overview band objects.                                   */
/* -------------------------------------------------------------------- */
    if( nOverviewFactorIn == 1 )
    {
        int iOverview;

        nOverviewCount = 0;
        papoOverviewBand = (DODSRasterBand **) 
            CPLCalloc( sizeof(void*), 8 );

        for( iOverview = 1; iOverview < 8; iOverview++ )
        {
            int nThisFactor = 1 << iOverview;
            
            if( nRasterXSize / nThisFactor < 128
                && nRasterYSize / nThisFactor < 128 )
                break;

            papoOverviewBand[nOverviewCount++] = 
                new DODSRasterBand( poDSIn, oVarNameIn, oCEIn, 
                                    nThisFactor );
        }
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr 
DODSRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage)
{
    DODSDataset *poDODS = dynamic_cast<DODSDataset *>(poDS);

/* -------------------------------------------------------------------- */
/*      What is the actual rectangle we want to read?  We can't read    */
/*      full blocks that go off the edge of the original data.          */
/* -------------------------------------------------------------------- */
    int nXOff, nXSize, nYOff, nYSize;

    nXOff = nBlockXOff * nBlockXSize;
    nYOff = nBlockYOff * nBlockYSize;
    nXSize = nBlockXSize;
    nYSize = nBlockYSize;

    if( nXOff + nXSize > nRasterXSize )
        nXSize = nRasterXSize - nXOff;
    if( nYOff + nYSize > nRasterYSize )
        nYSize = nRasterYSize - nYOff;

/* -------------------------------------------------------------------- */
/*      Prepare constraint expression for this request.                 */
/* -------------------------------------------------------------------- */
    string x_constraint, y_constraint, raw_constraint, final_constraint;

    x_constraint = 
        CPLSPrintf( "[%d:%d:%d]", 
                    nXOff * nOverviewFactor, 
                    nOverviewFactor, 
                    (nXOff + nXSize - 1) * nOverviewFactor );
    y_constraint = 
        CPLSPrintf( "[%d:%d:%d]", 
                    nYOff * nOverviewFactor,
                    nOverviewFactor,
                    (nYOff + nYSize - 1) * nOverviewFactor );

    raw_constraint = oVarName + oCE;

    final_constraint = poDODS->SubConstraint( raw_constraint, 
                                              x_constraint, 
                                              y_constraint );

    CPLDebug( "DODS", "constraint = %s", final_constraint.c_str() );

/* -------------------------------------------------------------------- */
/*      Request data from server.                                       */
/* -------------------------------------------------------------------- */
    try {
        DataDDS data;

        poDODS->GetConnect()->request_data(data, final_constraint );

/* -------------------------------------------------------------------- */
/*      Get the DataDDS Array object from the response.                 */
/* -------------------------------------------------------------------- */
        BaseType *poBt = get_variable(data, oVarName );
        if (!poBt)
            throw Error(string("I could not read the variable '")
		    + oVarName
		    + string("' from the data source at:\n")
		    + poDODS->GetUrl() );

        Array *poA;
        switch (poBt->type()) {
          case dods_grid_c:
            poA = dynamic_cast<Array*>(dynamic_cast<Grid*>(poBt)->array_var());
            break;
            
          case dods_array_c:
            poA = dynamic_cast<Array *>(poBt);
            break;
            
          default:
            throw InternalErr("Expected an Array or Grid variable!");
        }

/* -------------------------------------------------------------------- */
/*      Pre-initialize the output buffer to zero.                       */
/* -------------------------------------------------------------------- */
        if( nXSize < nBlockXSize || nYSize < nBlockYSize )
            memset( pImage, 0, 
                    nBlockXSize * nBlockYSize 
                    * GDALGetDataTypeSize(eDataType) / 8 );

/* -------------------------------------------------------------------- */
/*      Dump the contents of the Array data into our output image buffer.*/
/*                                                                      */
/* -------------------------------------------------------------------- */
        poA->buf2val(&pImage);	// !Suck the data out of the Array!

/* -------------------------------------------------------------------- */
/*      If we only read a partial block we need to re-organize the      */
/*      data.                                                           */
/* -------------------------------------------------------------------- */
        if( nXSize < nBlockXSize || nYSize < nBlockYSize )
        {
            int iLine;
            int nBytesPerPixel = GDALGetDataTypeSize(eDataType) / 8;

            for( iLine = nYSize-1; iLine >= 0; iLine-- )
            {
                memmove( ((GByte *) pImage) + iLine * nBlockXSize,
                         ((GByte *) pImage) + iLine * nXSize,
                         nBytesPerPixel * nXSize );
                memset( ((GByte *) pImage) + iLine * nBlockXSize
                        + nBytesPerPixel * nXSize, 
                        0, nBytesPerPixel * (nBlockXSize - nXSize) );
            }
        }

/* -------------------------------------------------------------------- */
/*      Eventually we need to add flipping and transposition support    */
/*      here.                                                           */
/* -------------------------------------------------------------------- */

    }

/* -------------------------------------------------------------------- */
/*      Catch exceptions                                                */
/* -------------------------------------------------------------------- */
    catch (Error &e) {
        CPLError(CE_Failure, CPLE_AppDefined, e.get_error_message().c_str());
        return CE_Failure;
    }
    
    return CE_None;
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int DODSRasterBand::GetOverviewCount()

{
    return nOverviewCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *DODSRasterBand::GetOverview( int iOverview )

{
    if( iOverview < 0 || iOverview >= nOverviewCount )
        return NULL;
    else
        return papoOverviewBand[iOverview];
}

/************************************************************************/
/*                         GDALRegister_DODS()                          */
/************************************************************************/

void 
GDALRegister_DODS()
{
    GDALDriver *poDriver;

    if( GDALGetDriverByName( "DODS" ) == NULL ) {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "DODS" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "DAP 3.x servers" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#DODS" );

        poDriver->pfnOpen = DODSDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

