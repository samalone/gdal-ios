/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Utility functions for OGR classes, including some related to
 *           parsing well known text format vectors.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 *
 * $Log$
 * Revision 1.9  2002/12/09 16:10:39  warmerda
 * added DMS translation
 *
 * Revision 1.8  2002/08/07 02:46:10  warmerda
 * improved comments
 *
 * Revision 1.7  2001/11/01 17:01:28  warmerda
 * pass output buffer into OGRMakeWktCoordinate
 *
 * Revision 1.6  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.5  2001/05/29 02:24:00  warmerda
 * fixed negative support for Z coordinate
 *
 * Revision 1.4  2001/05/24 18:05:36  warmerda
 * fixed support for negative coordinte parsing
 *
 * Revision 1.3  1999/11/18 19:02:19  warmerda
 * expanded tabs
 *
 * Revision 1.2  1999/09/13 02:27:33  warmerda
 * incorporated limited 2.5d support
 *
 * Revision 1.1  1999/05/20 14:35:00  warmerda
 * New
 *
 */

#include "ogr_geometry.h"
#include "ogr_p.h"
#include <ctype.h>

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRMakeWktCoordinate()                        */
/*                                                                      */
/*      Format a well known text coordinate, trying to keep the         */
/*      ASCII representation compact, but accurate.  These rules        */
/*      will have to tighten up in the future.                          */
/*                                                                      */
/*      Currently a new point should require no more than 64            */
/*      characters barring the X or Y value being extremely large.      */
/************************************************************************/

void OGRMakeWktCoordinate( char *pszTarget, double x, double y, double z )

{
    if( z == 0 )
    {
        if( x == (int) x && y == (int) y )
            sprintf( pszTarget, "%d %d", (int) x, (int) y );
        else if( fabs(x) < 370 && fabs(y) < 370 )
            sprintf( pszTarget, "%.8f %.8f", x, y );
        else
            sprintf( pszTarget, "%.3f %.3f", x, y );
    }
    else
    {
        if( x == (int) x && y == (int) y && z == (int) z )
            sprintf( pszTarget, "%d %d %d", (int) x, (int) y, (int) z );
        else if( fabs(x) < 370 && fabs(y) < 370 )
            sprintf( pszTarget, "%.8f %.8f %.3f", x, y, z );
        else
            sprintf( pszTarget, "%.3f %.3f %.3f", x, y, z );
    }
}

/************************************************************************/
/*                          OGRWktReadToken()                           */
/*                                                                      */
/*      Read one token or delimeter and put into token buffer.  Pre     */
/*      and post white space is swallowed.                              */
/************************************************************************/

const char *OGRWktReadToken( const char * pszInput, char * pszToken )

{
    if( pszInput == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Swallow pre-white space.                                        */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' || *pszInput == '\t' )
        pszInput++;

/* -------------------------------------------------------------------- */
/*      If this is a delimeter, read just one character.                */
/* -------------------------------------------------------------------- */
    if( *pszInput == '(' || *pszInput == ')' || *pszInput == ',' )
    {
        pszToken[0] = *pszInput;
        pszToken[1] = '\0';
        
        pszInput++;
    }

/* -------------------------------------------------------------------- */
/*      Or if it alpha numeric read till we reach non-alpha numeric     */
/*      text.                                                           */
/* -------------------------------------------------------------------- */
    else
    {
        int             iChar = 0;
        
        while( iChar < OGR_WKT_TOKEN_MAX-1
               && ((*pszInput >= 'a' && *pszInput <= 'z')
                   || (*pszInput >= 'A' && *pszInput <= 'Z')
                   || (*pszInput >= '0' && *pszInput <= '9')
                   || *pszInput == '.' 
                   || *pszInput == '+' 
                   || *pszInput == '-') )
        {
            pszToken[iChar++] = *(pszInput++);
        }

        pszToken[iChar++] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Eat any trailing white space.                                   */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' || *pszInput == '\t' )
        pszInput++;

    return( pszInput );
}

/************************************************************************/
/*                          OGRWktReadPoints()                          */
/*                                                                      */
/*      Read a point string.  The point list must be contained in       */
/*      brackets and each point pair separated by a comma.              */
/************************************************************************/

const char * OGRWktReadPoints( const char * pszInput,
                               OGRRawPoint ** ppaoPoints, double **ppadfZ,
                               int * pnMaxPoints,
                               int * pnPointsRead )

{
    *pnPointsRead = 0;

    if( pszInput == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Eat any leading white space.                                    */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' || *pszInput == '\t' )
        pszInput++;

/* -------------------------------------------------------------------- */
/*      If this isn't an opening bracket then we have a problem!        */
/* -------------------------------------------------------------------- */
    if( *pszInput != '(' )
    {
        CPLDebug( "OGR",
                  "Expected '(', but got %s in OGRWktReadPoints().\n",
                  pszInput );
                  
        return pszInput;
    }

    pszInput++;

/* ==================================================================== */
/*      This loop reads a single point.  It will continue till we       */
/*      run out of well formed points, or a closing bracket is          */
/*      encountered.                                                    */
/* ==================================================================== */
    char        szDelim[OGR_WKT_TOKEN_MAX];
    
    do {
/* -------------------------------------------------------------------- */
/*      Read the X and Y values, verify they are numeric.               */
/* -------------------------------------------------------------------- */
        char    szTokenX[OGR_WKT_TOKEN_MAX];
        char    szTokenY[OGR_WKT_TOKEN_MAX];

        pszInput = OGRWktReadToken( pszInput, szTokenX );
        pszInput = OGRWktReadToken( pszInput, szTokenY );

        if( (!isdigit(szTokenX[0]) && szTokenX[0] != '-')
            || (!isdigit(szTokenY[0]) && szTokenY[0] != '-') )
            return NULL;

/* -------------------------------------------------------------------- */
/*      Do we need to grow the point list to hold this point?           */
/* -------------------------------------------------------------------- */
        if( *pnPointsRead == *pnMaxPoints )
        {
            *pnMaxPoints = *pnMaxPoints * 2 + 10;
            *ppaoPoints = (OGRRawPoint *)
                CPLRealloc(*ppaoPoints, sizeof(OGRRawPoint) * *pnMaxPoints);

            if( *ppadfZ != NULL )
            {
                *ppadfZ = (double *)
                    CPLRealloc(*ppadfZ, sizeof(double) * *pnMaxPoints);
            }
        }

/* -------------------------------------------------------------------- */
/*      Add point to list.                                              */
/* -------------------------------------------------------------------- */
        (*ppaoPoints)[*pnPointsRead].x = atof(szTokenX);
        (*ppaoPoints)[*pnPointsRead].y = atof(szTokenY);

/* -------------------------------------------------------------------- */
/*      Do we have a Z coordinate?                                      */
/* -------------------------------------------------------------------- */
        pszInput = OGRWktReadToken( pszInput, szDelim );

        if( isdigit(szDelim[0]) || szDelim[0] == '-' )
        {
            if( *ppadfZ == NULL )
            {
                *ppadfZ = (double *) CPLCalloc(sizeof(double),*pnMaxPoints);
            }

            (*ppadfZ)[*pnPointsRead] = atof(szDelim);
            
            pszInput = OGRWktReadToken( pszInput, szDelim );
        }
        
        (*pnPointsRead)++;

/* -------------------------------------------------------------------- */
/*      Read next delimeter ... it should be a comma if there are       */
/*      more points.                                                    */
/* -------------------------------------------------------------------- */
        if( szDelim[0] != ')' && szDelim[0] != ',' )
        {
            CPLDebug( "OGR",
                      "Corrupt input in OGRWktReadPoints()\n"
                      "Got `%s' when expecting `,' or `)'.\n",
                      szDelim );
            return NULL;
        }
        
    } while( szDelim[0] == ',' );

    return pszInput;
}

/************************************************************************/
/*                             OGRMalloc()                              */
/*                                                                      */
/*      Cover for CPLMalloc()                                           */
/************************************************************************/

void *OGRMalloc( size_t size )

{
    return CPLMalloc( size );
}

/************************************************************************/
/*                             OGRCalloc()                              */
/*                                                                      */
/*      Cover for CPLCalloc()                                           */
/************************************************************************/

void * OGRCalloc( size_t count, size_t size )

{
    return CPLCalloc( count, size );
}

/************************************************************************/
/*                             OGRRealloc()                             */
/*                                                                      */
/*      Cover for CPLRealloc()                                          */
/************************************************************************/

void *OGRRealloc( void * pOld, size_t size )

{
    return CPLRealloc( pOld, size );
}

/************************************************************************/
/*                              OGRFree()                               */
/*                                                                      */
/*      Cover for CPLFree().                                            */
/************************************************************************/

void OGRFree( void * pMemory )

{
    CPLFree( pMemory );
}

/************************************************************************/
/*                            proj_strtod()                             */
/************************************************************************/
static double
proj_strtod(char *nptr, char **endptr) 

{
    char c, *cp = nptr;
    double result;

    /*
     * Scan for characters which cause problems with VC++ strtod()
     */
    while ((c = *cp) != '\0') {
        if (c == 'd' || c == 'D') {

            /*
             * Found one, so NUL it out, call strtod(),
             * then restore it and return
             */
            *cp = '\0';
            result = strtod(nptr, endptr);
            *cp = c;
            return result;
        }
        ++cp;
    }

    /* no offending characters, just handle normally */

    return strtod(nptr, endptr);
}
/************************************************************************/
/*                            OSRDMSToDec()                             */
/************************************************************************/

static const char*sym = "NnEeSsWw";
static const double vm[] = { 1.0, 0.0166666666667, 0.00027777778 };

double OSRDMSToDec( const char *is )

{
    int sign, n, nl;
    char *p, *s, work[64];
    double v, tv;

    /* copy sting into work space */
    while (isspace(sign = *is)) ++is;
    for (n = sizeof(work), s = work, p = (char *)is; isgraph(*p) && --n ; )
        *s++ = *p++;
    *s = '\0';
    /* it is possible that a really odd input (like lots of leading
       zeros) could be truncated in copying into work.  But ... */
    sign = *(s = work);
    if (sign == '+' || sign == '-') s++;
    else sign = '+';
    for (v = 0., nl = 0 ; nl < 3 ; nl = n + 1 ) {
        if (!(isdigit(*s) || *s == '.')) break;
        if ((tv = proj_strtod(s, &s)) == HUGE_VAL)
            return tv;
        switch (*s) {
          case 'D': case 'd':
            n = 0; break;
          case '\'':
            n = 1; break;
          case '"':
            n = 2; break;
          case 'r': case 'R':
            if (nl) {
                return 0.0;
            }
            ++s;
            v = tv;
            goto skip;
          default:
            v += tv * vm[nl];
          skip:	n = 4;
            continue;
        }
        if (n < nl) {
            return 0.0;
        }
        v += tv * vm[n];
        ++s;
    }
    /* postfix sign */
    if (*s && (p = strchr(sym, *s))) {
        sign = (p - sym) >= 4 ? '-' : '+';
        ++s;
    }
    if (sign == '-')
        v = -v;

    return v;
}

/************************************************************************/
/*                            OSRDecToDMS()                             */
/************************************************************************/
CPL_C_START
const char CPL_DLL *GDALDecToDMS( double, const char *, int );
CPL_C_END

const char *OSRDecToDMS( double dfAngle, const char *pszAxis, int nPrecision )

{
    return GDALDecToDMS( dfAngle, pszAxis, nPrecision );
}
