/**********************************************************************
 * $Id: mitab_coordsys.cpp,v 1.16 2000/10/16 21:44:50 warmerda Exp $
 *
 * Name:     mitab_coordsys.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation translation between MIF CoordSys format, and
 *           and OGRSpatialRef format.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 **********************************************************************
 * Copyright (c) 1999, 2000, Frank Warmerdam
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************
 *
 * $Log: mitab_coordsys.cpp,v $
 * Revision 1.16  2000/10/16 21:44:50  warmerda
 * added nonearth support
 *
 * Revision 1.15  2000/10/12 22:01:01  daniel
 * Fixed test on number of bound params in MITABExtractCoordSysBounds()
 *
 * Revision 1.14  2000/09/28 16:39:44  warmerda
 * avoid warnings for unused, and unitialized variables
 *
 * Revision 1.13  2000/08/29 22:30:45  daniel
 * Made MITABCoordSys2SpatialRef() return NULL for a NonEarth coordsys since
 * returning an empty SpatialRef created confusion between LatLon and NonEarth
 *
 * Revision 1.12  2000/03/27 03:31:15  daniel
 * Fixed parsing of CoordSys NonEarth broken by previous change
 *
 * Revision 1.11  2000/03/19 23:26:27  daniel
 * Made MITABCoordSys2SpatialRef() remove leading spaces in coordsys string
 * and produce a CPLError() if it fails parsing string.
 *
 * Revision 1.10  2000/02/07 17:43:37  daniel
 * Fixed offset in parsing of custom datum string in SetSpatialRef()
 *
 * Revision 1.9  2000/01/15 22:30:43  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.8  2000/01/14 14:53:32  warmerda
 * fixed generation of MIF %d datum names
 *
 * Revision 1.7  1999/12/19 17:35:16  daniel
 * Fixed memory leaks
 *
 * Revision 1.6  1999/11/20 04:53:41  daniel
 * Fixed warning on return value in MITABExtractCoordSysBounds()
 *
 * Revision 1.5  1999/11/12 05:51:57  daniel
 * Added MITABExtractCoordSysBounds()
 *
 * Revision 1.4  1999/11/11 02:55:25  warmerda
 * fixed problems with stereographic and survey ft
 *
 * Revision 1.3  1999/11/10 20:13:12  warmerda
 * implement spheroid table
 *
 * Revision 1.2  1999/11/10 02:19:05  warmerda
 * fixed up datum support when reading MIF coord sys
 *
 * Revision 1.1  1999/11/09 22:29:38  warmerda
 * New
 *
 **********************************************************************/

#include "mitab.h"

extern MapInfoDatumInfo asDatumInfoList[200];
extern MapInfoSpheroidInfo asSpheroidInfoList[200];

/************************************************************************/
/*                             GetMIFParm()                             */
/************************************************************************/

static double GetMIFParm( char ** papszFields, int iField, double dfDefault )

{
    if( iField >= CSLCount(papszFields) )
        return dfDefault;
    else
        return atof(papszFields[iField]);
}

/************************************************************************/
/*                      MITABCoordSys2SpatialRef()                      */
/*                                                                      */
/*      Convert a MIF COORDSYS string into a new OGRSpatialReference    */
/*      object.                                                         */
/************************************************************************/

OGRSpatialReference *MITABCoordSys2SpatialRef( const char * pszCoordSys )

{
    char	**papszFields;
    OGRSpatialReference *poSR;

    if( pszCoordSys == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Parse the passed string into words.                             */
/* -------------------------------------------------------------------- */
    while(*pszCoordSys == ' ') pszCoordSys++;  // Eat leading spaces
    if( EQUALN(pszCoordSys,"CoordSys",8) )
        pszCoordSys += 9;
    
    papszFields = CSLTokenizeStringComplex( pszCoordSys, " ,", TRUE, FALSE );

/* -------------------------------------------------------------------- */
/*      Clip off Bounds information.                                    */
/* -------------------------------------------------------------------- */
    int		iBounds = CSLFindString( papszFields, "Bounds" );

    while( iBounds != -1 && papszFields[iBounds] != NULL )
    {
        CPLFree( papszFields[iBounds] );
        papszFields[iBounds] = NULL;
        iBounds++;
    }

/* -------------------------------------------------------------------- */
/*      Create a spatialreference object to operate on.                 */
/* -------------------------------------------------------------------- */
    poSR = new OGRSpatialReference;

/* -------------------------------------------------------------------- */
/*	Fetch the projection.						*/
/* -------------------------------------------------------------------- */
    char	**papszNextField;
    int	nProjection = 0;

    if( CSLCount( papszFields ) >= 3
        && EQUAL(papszFields[0],"Earth")
        && EQUAL(papszFields[1],"Projection") )
    {
        nProjection = atoi(papszFields[2]);
        papszNextField = papszFields + 3;
    }
    else if (CSLCount( papszFields ) >= 2
             && EQUAL(papszFields[0],"NonEarth") )
    {
        // NonEarth Units "..." Bounds (x, y) (x, y)
        nProjection = 0;
        papszNextField = papszFields + 2;

        if( papszNextField[0] != NULL && EQUAL(papszNextField[0],"Units") )
            papszNextField++;
    }
    else
    {
        // Invalid projection string ???
        if (CSLCount(papszFields) > 0)
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "Failed parsing CoordSys: '%s'", pszCoordSys);
        CSLDestroy(papszFields);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the datum information.                                    */
/* -------------------------------------------------------------------- */
    int		nDatum = 0;
    double	adfDatumParm[8];
    int		nEllipsoid=0;

    if( nProjection != 0 && CSLCount(papszNextField) > 0 )
    {
        nDatum = atoi(papszNextField[0]);
        papszNextField++;
    }

    if( (nDatum == 999 || nDatum == 9999)
        && CSLCount(papszNextField) >= 4 )
    {
        nEllipsoid = atoi(papszFields[0]);
        adfDatumParm[0] = atof(papszNextField[1]);
        adfDatumParm[1] = atof(papszNextField[2]);
        adfDatumParm[2] = atof(papszNextField[3]);
        papszNextField += 4;
    }

    if( nDatum == 9999
        && CSLCount(papszNextField) >= 5 )
    {
        adfDatumParm[3] = atof(papszNextField[0]);
        adfDatumParm[4] = atof(papszNextField[1]);
        adfDatumParm[5] = atof(papszNextField[2]);
        adfDatumParm[6] = atof(papszNextField[3]);
        adfDatumParm[7] = atof(papszNextField[4]);
        papszNextField += 5;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the units string.                                         */
/* -------------------------------------------------------------------- */
    const char	*pszMIFUnits = NULL;
    
    if( CSLCount(papszNextField) > 0 )
    {
        pszMIFUnits = papszNextField[0];
        papszNextField++;
    }

/* -------------------------------------------------------------------- */
/*      Handle the PROJCS style projections, but add the datum later.   */
/* -------------------------------------------------------------------- */
    switch( nProjection )
    {
        /*--------------------------------------------------------------
         * NonEarth ... we return with an empty SpatialRef.  Eventually
         * we might want to include the units, but not for now.
         *
         * __TODO__ Changed to return NULL because returning an empty
         * SpatialRef caused confusion between Latlon and NonEarth since
         * empty SpatialRefs do have a GEOGCS set and makes them look like
         * Lat/Lon SpatialRefs.
         *
         * Ideally we would like to return a SpatialRef whith no GEGOCS
         *-------------------------------------------------------------*/
      case 0:
        poSR->SetLocalCS( "Nonearth" );
        break;

        /*--------------------------------------------------------------
         * lat/long .. just add the GEOGCS later.
         *-------------------------------------------------------------*/
      case 1:
        break;

        /*--------------------------------------------------------------
         * Cylindrical Equal Area
         *-------------------------------------------------------------*/
      case 2:
        poSR->SetCEA( GetMIFParm( papszNextField, 1, 0.0 ),
                      GetMIFParm( papszNextField, 0, 0.0 ),
                      GetMIFParm( papszNextField, 2, 0.0 ),
                      GetMIFParm( papszNextField, 3, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Lambert Conic Conformal
         *-------------------------------------------------------------*/
      case 3:
        poSR->SetLCC( GetMIFParm( papszNextField, 2, 0.0 ),
                      GetMIFParm( papszNextField, 3, 0.0 ),
                      GetMIFParm( papszNextField, 1, 0.0 ),
                      GetMIFParm( papszNextField, 0, 0.0 ),
                      GetMIFParm( papszNextField, 4, 0.0 ),
                      GetMIFParm( papszNextField, 5, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Lambert Azimuthal Equal Area
         *-------------------------------------------------------------*/
      case 4:
        poSR->SetLAEA( GetMIFParm( papszNextField, 1, 0.0 ),
                       GetMIFParm( papszNextField, 0, 0.0 ),
                       0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Azimuthal Equidistant (Polar aspect only)
         *-------------------------------------------------------------*/
      case 5:
        poSR->SetAE( GetMIFParm( papszNextField, 1, 0.0 ),
                     GetMIFParm( papszNextField, 0, 0.0 ),
                     0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Equidistant Conic
         *-------------------------------------------------------------*/
      case 6:
        poSR->SetEC( GetMIFParm( papszNextField, 2, 0.0 ),
                     GetMIFParm( papszNextField, 3, 0.0 ),
                     GetMIFParm( papszNextField, 1, 0.0 ),
                     GetMIFParm( papszNextField, 0, 0.0 ),
                     GetMIFParm( papszNextField, 4, 0.0 ),
                     GetMIFParm( papszNextField, 5, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Hotine Oblique Mercator
         *-------------------------------------------------------------*/
      case 7:
        poSR->SetHOM( GetMIFParm( papszNextField, 1, 0.0 ),
                      GetMIFParm( papszNextField, 0, 0.0 ),
                      GetMIFParm( papszNextField, 2, 0.0 ),
                      90.0,
                      GetMIFParm( papszNextField, 3, 1.0 ),
                      GetMIFParm( papszNextField, 4, 0.0 ),
                      GetMIFParm( papszNextField, 5, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Albers Conic Equal Area
         *-------------------------------------------------------------*/
      case 9:
        poSR->SetACEA( GetMIFParm( papszNextField, 2, 0.0 ),
                       GetMIFParm( papszNextField, 3, 0.0 ),
                       GetMIFParm( papszNextField, 1, 0.0 ),
                       GetMIFParm( papszNextField, 0, 0.0 ),
                       GetMIFParm( papszNextField, 4, 0.0 ),
                       GetMIFParm( papszNextField, 5, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Mercator
         *-------------------------------------------------------------*/
      case 10:
        poSR->SetMercator( 0.0, GetMIFParm( papszNextField, 0, 0.0 ),
                           1.0, 0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Miller Cylindrical
         *-------------------------------------------------------------*/
      case 11:
        poSR->SetMC( 0.0, GetMIFParm( papszNextField, 0, 0.0 ),
                     0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Robinson
         *-------------------------------------------------------------*/
      case 12:
        poSR->SetRobinson( GetMIFParm( papszNextField, 0, 0.0 ),
                           0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Mollweide
         *-------------------------------------------------------------*/
      case 13:
        poSR->SetMollweide( GetMIFParm( papszNextField, 0, 0.0 ),
                            0.0, 0.0 );

        /*--------------------------------------------------------------
         * Eckert IV
         *-------------------------------------------------------------*/
      case 14:
        poSR->SetEckertIV( GetMIFParm( papszNextField, 0, 0.0 ),
                           0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Eckert VI
         *-------------------------------------------------------------*/
      case 15:
        poSR->SetEckertVI( GetMIFParm( papszNextField, 0, 0.0 ),
                           0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Sinusoidal
         *-------------------------------------------------------------*/
      case 16:
        poSR->SetSinusoidal( GetMIFParm( papszNextField, 0, 0.0 ),
                             0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Transverse Mercator
         *-------------------------------------------------------------*/
      case 8:
      case 21:
      case 22:
      case 23:
      case 24:
        poSR->SetTM( GetMIFParm( papszNextField, 1, 0.0 ),
                     GetMIFParm( papszNextField, 0, 0.0 ),
                     GetMIFParm( papszNextField, 2, 1.0 ),
                     GetMIFParm( papszNextField, 3, 0.0 ),
                     GetMIFParm( papszNextField, 4, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Gall
         *-------------------------------------------------------------*/
      case 17:
        poSR->SetGS( GetMIFParm( papszNextField, 0, 0.0 ),
                     0.0, 0.0 );
        break;
        
        /*--------------------------------------------------------------
         * New Zealand Map Grid
         *-------------------------------------------------------------*/
      case 18:
        poSR->SetNZMG( GetMIFParm( papszNextField, 1, 0.0 ),
                       GetMIFParm( papszNextField, 0, 0.0 ),
                       GetMIFParm( papszNextField, 2, 0.0 ),
                       GetMIFParm( papszNextField, 3, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Lambert Conic Conformal (Belgium)
         *-------------------------------------------------------------*/
      case 19:
        poSR->SetLCCB( GetMIFParm( papszNextField, 2, 0.0 ),
                       GetMIFParm( papszNextField, 3, 0.0 ),
                       GetMIFParm( papszNextField, 1, 0.0 ),
                       GetMIFParm( papszNextField, 0, 0.0 ),
                       GetMIFParm( papszNextField, 4, 0.0 ),
                       GetMIFParm( papszNextField, 5, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Stereographic
         *-------------------------------------------------------------*/
      case 20:
        poSR->SetStereographic( GetMIFParm( papszNextField, 1, 0.0 ),
                                GetMIFParm( papszNextField, 0, 0.0 ),
                                GetMIFParm( papszNextField, 2, 1.0 ),
                                GetMIFParm( papszNextField, 3, 0.0 ),
                                GetMIFParm( papszNextField, 4, 0.0 ) );
        break;

      default:
        break;
    }

/* -------------------------------------------------------------------- */
/*      Set linear units.                                               */
/* -------------------------------------------------------------------- */
    if( nProjection == 1 || pszMIFUnits == NULL )
        /* do nothing */;
    else if( EQUAL(pszMIFUnits,"km") )
        poSR->SetLinearUnits( "Kilometer", 1000.0 );
    
    else if( EQUAL(pszMIFUnits, "in" ) )
        poSR->SetLinearUnits( "IINCH", 0.0254 );
    
    else if( EQUAL(pszMIFUnits, "ft" ) )
        poSR->SetLinearUnits( SRS_UL_FOOT, atof(SRS_UL_FOOT_CONV));
    
    else if( EQUAL(pszMIFUnits, "yd" ) )
        poSR->SetLinearUnits( "IYARD", 0.9144 );
    
    else if( EQUAL(pszMIFUnits, "mm" ) )
        poSR->SetLinearUnits( "Millimeter", 0.001);
    
    else if( EQUAL(pszMIFUnits, "cm" ) )
        poSR->SetLinearUnits( "Centimeter", 0.01);

    else if( EQUAL(pszMIFUnits, "m" ) )
        poSR->SetLinearUnits( SRS_UL_METER, 1.0 );
        
    else if( EQUAL(pszMIFUnits, "survey foot" )
             || EQUAL(pszMIFUnits, "survey ft" ) )
        poSR->SetLinearUnits( SRS_UL_US_FOOT, atof(SRS_UL_US_FOOT_CONV) );
        
    else if( EQUAL(pszMIFUnits, "nmi" ) )
        poSR->SetLinearUnits( SRS_UL_NAUTICAL_MILE,
                              atof(SRS_UL_NAUTICAL_MILE_CONV) );
        
    else if( EQUAL(pszMIFUnits, "li" ) )
        poSR->SetLinearUnits( SRS_UL_LINK, atof(SRS_UL_LINK_CONV) );
        
    else if( EQUAL(pszMIFUnits, "ch" ) )
        poSR->SetLinearUnits( SRS_UL_CHAIN, atof(SRS_UL_CHAIN_CONV));
        
    else if( EQUAL(pszMIFUnits, "rd" ) )
        poSR->SetLinearUnits( SRS_UL_ROD, atof(SRS_UL_ROD));
        
    else if( EQUAL(pszMIFUnits, "mi" ) )
        poSR->SetLinearUnits( "Mile", 1609.344 );

/* -------------------------------------------------------------------- */
/*      For Non-Earth projection, we're done at this point.             */
/* -------------------------------------------------------------------- */
    if (nProjection == 0)
        return poSR;

/* ==================================================================== */
/*      Establish the GeogCS                                            */
/* ==================================================================== */
    const char *pszGeogName = "unnamed";
    const char *pszSpheroidName = "GRS_1980";
    double	dfSemiMajor = 6378137.0;
    double	dfInvFlattening = 298.257222101;
    const char *pszPrimeM = "Greenwich";
    double	dfPMLongToGreenwich = 0.0;

/* -------------------------------------------------------------------- */
/*      Find the datum, and collect it's parameters if possible.        */
/* -------------------------------------------------------------------- */
    int		iDatum;
    MapInfoDatumInfo *psDatumInfo = NULL;
    
    for( iDatum = 0; asDatumInfoList[iDatum].nMapInfoDatumID != -1; iDatum++ )
    {
        if( asDatumInfoList[iDatum].nMapInfoDatumID == nDatum )
        {
            psDatumInfo = asDatumInfoList + iDatum;
            break;
        }
    }

    if( asDatumInfoList[iDatum].nMapInfoDatumID == -1
        && nDatum != 999 && nDatum != 9999 )
    {
        /* use WGS84 */
        psDatumInfo = asDatumInfoList + 0;
    }

    if( psDatumInfo != NULL )
    {
        nEllipsoid = psDatumInfo->nEllipsoid;
        adfDatumParm[0] =  psDatumInfo->dfShiftX;
        adfDatumParm[1] = psDatumInfo->dfShiftY;
        adfDatumParm[2] = psDatumInfo->dfShiftZ;
        adfDatumParm[3] = psDatumInfo->dfDatumParm0;
        adfDatumParm[4] = psDatumInfo->dfDatumParm1;
        adfDatumParm[5] = psDatumInfo->dfDatumParm2;
        adfDatumParm[6] = psDatumInfo->dfDatumParm3;
        adfDatumParm[7] = psDatumInfo->dfDatumParm4;
    }
    
/* -------------------------------------------------------------------- */
/*	Set the spheroid if it is known from the table.			*/
/* -------------------------------------------------------------------- */
    for( int i = 0; asSpheroidInfoList[i].nMapInfoId != -1; i++ )
    {
        if( asSpheroidInfoList[i].nMapInfoId == nEllipsoid )
        {
            dfSemiMajor = asSpheroidInfoList[i].dfA;
            dfInvFlattening = asSpheroidInfoList[i].dfInvFlattening;
            pszSpheroidName = asSpheroidInfoList[i].pszMapinfoName;
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      apply datum parameters.                                         */
/* -------------------------------------------------------------------- */
    char	szDatumName[128];

    if( nDatum == 999 )
    {
        sprintf( szDatumName,
                 "MIF 9999,%d,%.15g,%.15g,%.15g",
                 nEllipsoid,
                 adfDatumParm[0],
                 adfDatumParm[1],
                 adfDatumParm[2] );
    }
    else if( nDatum == 9999 )
    {
        sprintf( szDatumName,
                 "MIF 9999,%d,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g",
                 nEllipsoid,
                 adfDatumParm[0],
                 adfDatumParm[1],
                 adfDatumParm[2],
                 adfDatumParm[3],
                 adfDatumParm[4],
                 adfDatumParm[5],
                 adfDatumParm[6],
                 adfDatumParm[7] );
    }
    else if( psDatumInfo->pszOGCDatumName != NULL
             && strlen(psDatumInfo->pszOGCDatumName) > 0 )
    {
        strncpy( szDatumName, psDatumInfo->pszOGCDatumName,
                 sizeof(szDatumName) );
    }
    else
    {
        sprintf( szDatumName, "MIF %d", nDatum );
    }

/* -------------------------------------------------------------------- */
/*      Set prime meridian for 9999 datums.                             */
/* -------------------------------------------------------------------- */
    if( nDatum == 9999 )
    {
        pszPrimeM = "non-Greenwich";
        dfPMLongToGreenwich = adfDatumParm[7];
    }

/* -------------------------------------------------------------------- */
/*      Set the GeogCS.                                                 */
/* -------------------------------------------------------------------- */
    poSR->SetGeogCS( pszGeogName, szDatumName, pszSpheroidName,
                     dfSemiMajor, dfInvFlattening,
                     pszPrimeM, dfPMLongToGreenwich,
                     SRS_UA_DEGREE,
                     atof(SRS_UA_DEGREE_CONV) );


/* -------------------------------------------------------------------- */
/*      Report on translation.                                          */
/* -------------------------------------------------------------------- */
    char	*pszWKT;

    poSR->exportToWkt( &pszWKT );
    if( pszWKT != NULL )
    {
        CPLDebug( "MITAB",
                  "This CoordSys value:\n%s\nwas translated to:\n%s\n",
                  pszCoordSys, pszWKT );
        CPLFree( pszWKT );
    }

    CSLDestroy(papszFields);

    return poSR;
}

/************************************************************************/
/*                      MITABSpatialRef2CoordSys()                      */
/*                                                                      */
/*      Converts a OGRSpatialReference object into a MIF COORDSYS       */
/*      string.                                                         */
/*                                                                      */
/*      The function returns a newly allocated string that should be    */
/*      CPLFree()'d by the caller.                                      */
/************************************************************************/

char *MITABSpatialRef2CoordSys( OGRSpatialReference * poSR )

{
    if( poSR == NULL )
        return NULL;
    
    /*-----------------------------------------------------------------
     * Transform the projection and projection parameters.
     *----------------------------------------------------------------*/
    const char *pszProjection = poSR->GetAttrValue("PROJECTION");
    double	parms[10];
    int		nProjection = 0;
    int		nParmCount = 0;

    if( pszProjection == NULL )
    {
        /*--------------------------------------------------------------
         * NULL projection.  
         * We have 2 possibilities: CoordSys NonEarth or Lat/Lon 
         * NonEarth ... is an empty SpatialRef.  
         * Lat/Lon has no "PROJECTION" but GEOGCS is set
         *-------------------------------------------------------------*/
         if ( poSR->GetAttrValue("GEOGCS") == NULL)
            nProjection = 0; // Non-Earth
        else
            nProjection = 1; // Lat/Lon
    }
    else if( EQUAL(pszProjection,SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        nProjection = 9;
        parms[0] = poSR->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        parms[3] = poSR->GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        parms[4] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[5] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 6;
    }

    else if( EQUAL(pszProjection,SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        nProjection = 5;
        parms[0] = poSR->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0);
        parms[2] = 90.0;
        nParmCount = 3;
    }

    else if( EQUAL(pszProjection,SRS_PT_CYLINDRICAL_EQUAL_AREA) )
    {
        nProjection = 2;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        nParmCount = 2;
    }

    else if( EQUAL(pszProjection,SRS_PT_ECKERT_IV) )
    {
        nProjection = 14;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        nParmCount = 1;
    }

    else if( EQUAL(pszProjection,SRS_PT_ECKERT_VI) )
    {
        nProjection = 15;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        nParmCount = 1;
    }

    else if( EQUAL(pszProjection,SRS_PT_EQUIDISTANT_CONIC) )
    {
        nProjection = 6;
        parms[0] = poSR->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        parms[3] = poSR->GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        parms[4] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[5] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 6;
    }

    else if( EQUAL(pszProjection,SRS_PT_GALL_STEREOGRAPHIC) )
    {
        nProjection = 17;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        nParmCount = 1;
    }

    else if( EQUAL(pszProjection,SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
    {
        nProjection = 7;
        parms[0] = poSR->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_AZIMUTH,0.0);
        parms[3] = poSR->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        parms[4] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[5] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 6;
    }

    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        nProjection = 4;
        parms[0] = poSR->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0);
        parms[2] = 90.0;
        nParmCount = 3;
    }

    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
        nProjection = 3;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        parms[3] = poSR->GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        parms[4] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[5] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 6;
    }

    else if( EQUAL(pszProjection,SRS_PT_MERCATOR_1SP) )
    {
        nProjection = 10;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        nParmCount = 1;
    }

    else if( EQUAL(pszProjection,SRS_PT_MILLER_CYLINDRICAL) )
    {
        nProjection = 11;
        parms[0] = poSR->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        nParmCount = 1;
    }

    else if( EQUAL(pszProjection,SRS_PT_MOLLWEIDE) )
    {
        nProjection = 13;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        nParmCount = 1;
    }

    else if( EQUAL(pszProjection,SRS_PT_NEW_ZEALAND_MAP_GRID) )
    {
        nProjection = 18;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[3] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 4;
    }

    else if( EQUAL(pszProjection,SRS_PT_ROBINSON) )
    {
        nProjection = 12;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        nParmCount = 1;
    }

    else if( EQUAL(pszProjection,SRS_PT_SINUSOIDAL) )
    {
        nProjection = 16;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        nParmCount = 1;
    }

    else if( EQUAL(pszProjection,SRS_PT_STEREOGRAPHIC) )
    {
        nProjection = 20;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        parms[3] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[4] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 5;
    }

    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR) )
    {
        nProjection = 8;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        parms[3] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[4] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 5;
    }
    
    /* ==============================================================
     * Translate Datum and Ellipsoid
     * ============================================================== */
    int		nDatum = 0;
    double	adfDatumParm[8];
    int		nEllipsoid=0;
    
    const char *pszWKTDatum = poSR->GetAttrValue("DATUM");

    if( pszWKTDatum == NULL )
    {
        nDatum = 0;
        if( nProjection == 1 )
            nProjection = 0;
    }
    
    /*-----------------------------------------------------------------
     * We know the MIF datum number, and need to look it up to
     * translate into datum parameters.
     *----------------------------------------------------------------*/
    else if( EQUALN(pszWKTDatum,"MIF ",4)
             && atoi(pszWKTDatum+4) != 999
             && atoi(pszWKTDatum+4) != 9999 )
    {
        nDatum = atoi(pszWKTDatum+4);
    }

    /*-----------------------------------------------------------------
     * We have the MIF datum parameters, and apply those directly.
     *----------------------------------------------------------------*/
    else if( EQUALN(pszWKTDatum,"MIF ",4)
             && (atoi(pszWKTDatum+4) == 999 || atoi(pszWKTDatum+4) == 9999) )
    {
        char	**papszFields;
        
        nDatum = atoi(pszWKTDatum+4);

        papszFields =
            CSLTokenizeStringComplex( pszWKTDatum+4, ",", FALSE, TRUE);

        if( CSLCount(papszFields) >= 5 )
        {
            nEllipsoid = atoi(papszFields[1]);
            adfDatumParm[0] = atof(papszFields[2]);
            adfDatumParm[1] = atof(papszFields[3]);
            adfDatumParm[2] = atof(papszFields[4]);
        }

        if( CSLCount(papszFields) >= 10 )
        {
            adfDatumParm[3] = atof(papszFields[5]);
            adfDatumParm[4] = atof(papszFields[6]);
            adfDatumParm[5] = atof(papszFields[7]);
            adfDatumParm[6] = atof(papszFields[8]);
            adfDatumParm[7] = atof(papszFields[9]);
        }

        if( CSLCount(papszFields) < 5 )
            nDatum = 104; /* WGS84 */

        CSLDestroy( papszFields );
    }
    
    /*-----------------------------------------------------------------
     * We have a "real" datum name.  Try to look it up and get the
     * parameters.  If we don't find it just use WGS84.
     *----------------------------------------------------------------*/
    else 
    {
        int	i;

        for( i = 0; asDatumInfoList[i].nMapInfoDatumID != -1; i++ )
        {
            if( EQUAL(pszWKTDatum,asDatumInfoList[i].pszOGCDatumName) )
            {
                nDatum = asDatumInfoList[i].nMapInfoDatumID;
                break;
            }
        }

        if( nDatum == 0 )
            nDatum = 104; /* WGS84 */
    }

    /*-----------------------------------------------------------------
     * Translate the units
     *----------------------------------------------------------------*/
    const char	*pszMIFUnits = "m";
    double      dfLinearConv;
    char	*pszLinearUnits;

    dfLinearConv = poSR->GetLinearUnits( &pszLinearUnits );

    if( nProjection == 1 )
        pszMIFUnits = NULL;
    else if( pszLinearUnits == NULL )
        pszMIFUnits = "m";
    else if( dfLinearConv == 1000.0 )
        pszMIFUnits = "km";
    else if( dfLinearConv == 0.0254 )
        pszMIFUnits = "in";
    else if( EQUAL(pszLinearUnits,SRS_UL_FOOT) )
        pszMIFUnits = "ft";
    else if( EQUAL(pszLinearUnits,"IYARD") || dfLinearConv == 0.9144 )
        pszMIFUnits = "yd";
    else if( dfLinearConv == 0.001 )
        pszMIFUnits = "mm";
    else if( dfLinearConv == 0.01 )
        pszMIFUnits = "cm";
    else if( dfLinearConv == 1.0 )
        pszMIFUnits = "m";
    else if( EQUAL(pszLinearUnits,SRS_UL_US_FOOT) )
        pszMIFUnits = "survey ft";
    else if( EQUAL(pszLinearUnits,SRS_UL_NAUTICAL_MILE) )
        pszMIFUnits = "nmi";
    else if( EQUAL(pszLinearUnits,SRS_UL_LINK) )
        pszMIFUnits = "li";
    else if( EQUAL(pszLinearUnits,SRS_UL_CHAIN) )
        pszMIFUnits = "ch";
    else if( EQUAL(pszLinearUnits,SRS_UL_ROD) )
        pszMIFUnits = "rd";
    else if( EQUAL(pszLinearUnits,"Mile") 
             || EQUAL(pszLinearUnits,"IMILE") )
        pszMIFUnits = "mi";
    
/* -------------------------------------------------------------------- */
/*      Build coordinate system definition.                             */
/* -------------------------------------------------------------------- */
    char	szCoordSys[256];

    if( nProjection != 0 )
    {
        sprintf( szCoordSys,
                 "Earth Projection %d",
                 nProjection );

    }
    else
        sprintf( szCoordSys,
                 "NonEarth Units" );

/* -------------------------------------------------------------------- */
/*      Append Datum                                                    */
/* -------------------------------------------------------------------- */
    if( nProjection != 0 )
    {
        sprintf( szCoordSys + strlen(szCoordSys),
                 ", %d",
                 nDatum );

        if( nDatum == 999 || nDatum == 9999 )
        {
            sprintf( szCoordSys + strlen(szCoordSys),
                     ", %d, %.15g, %.15g, %.15g",
                     nEllipsoid,
                     adfDatumParm[0], adfDatumParm[1], adfDatumParm[2] );
        }
        
        if( nDatum == 9999 )
        {
            sprintf( szCoordSys + strlen(szCoordSys),
                     ", %.15g, %.15g, %.15g, %.15g, %.15g",
                     adfDatumParm[3], adfDatumParm[4], adfDatumParm[5],
                     adfDatumParm[6], adfDatumParm[7] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Append units.                                                   */
/* -------------------------------------------------------------------- */
    if( nProjection != 1 && pszMIFUnits != NULL )
    {
        if( nProjection != 0 )
            strcat( szCoordSys, "," );
        
        sprintf( szCoordSys + strlen(szCoordSys),
                 " \"%s\"",
                 pszMIFUnits );
    }

/* -------------------------------------------------------------------- */
/*      Append Projection Parms.                                        */
/* -------------------------------------------------------------------- */
    for( int iParm = 0; iParm < nParmCount; iParm++ )
        sprintf( szCoordSys + strlen(szCoordSys),
                 ", %.15g",
                 parms[iParm] );

/* -------------------------------------------------------------------- */
/*      Report on translation                                           */
/* -------------------------------------------------------------------- */
    char	*pszWKT = NULL;

    poSR->exportToWkt( &pszWKT );
    if( pszWKT != NULL )
    {
        CPLDebug( "MITAB",
                  "This WKT Projection:\n%s\n\ntranslates to:\n%s\n",
                  pszWKT, szCoordSys );
        CPLFree( pszWKT );
    }

    return( CPLStrdup( szCoordSys ) );
}


/************************************************************************/
/*                      MITABExtractCoordSysBounds                      */
/*                                                                      */
/* Return TRUE if MIF coordsys string contains a BOUNDS parameter and   */
/* Set x/y min/max values.                                              */
/************************************************************************/

GBool MITABExtractCoordSysBounds( const char * pszCoordSys,
                                  double &dXMin, double &dYMin,
                                  double &dXMax, double &dYMax )

{
    char	**papszFields;

    if( pszCoordSys == NULL )
        return FALSE;
    
    papszFields = CSLTokenizeStringComplex( pszCoordSys, " ,()", TRUE, FALSE );

    int iBounds = CSLFindString( papszFields, "Bounds" );

    if (iBounds >= 0 && iBounds + 4 < CSLCount(papszFields))
    {
        dXMin = atof(papszFields[++iBounds]);
        dYMin = atof(papszFields[++iBounds]);
        dXMax = atof(papszFields[++iBounds]);
        dYMax = atof(papszFields[++iBounds]);
        return TRUE;
    }

    return FALSE;
}
