/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Classes for manipulating simple features that is not specific
 *           to a particular interface technology.
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
 * Revision 1.1  1999/03/29 21:21:10  warmerda
 * New
 *
 */

#ifndef _OGR_GEOMETRY_H_INCLUDED
#define _OGR_GEOMETRY_H_INLLUDED

#include <stdio.h>

#define OGRMalloc	malloc
#define OGRFree		free
#define OGRRealloc	realloc
#define OGRCalloc	calloc

typedef int OGRErr;

#define OGRERR_NONE		   0
#define OGRERR_NOT_ENOUGH_DATA	   1	/* not enough data to deserialize */
#define OGRERR_NOT_ENOUGH_MEMORY   2
#define OGRERR_UNSUPPORTED_GEOMETRY_TYPE 3

enum OGRwkbGeometryType
{
    wkbPoint = 1,
    wkbLineString = 2,
    wkbPolygon = 3,
    wkbMultiPoint = 4,
    wkbMultiLineString = 5,
    wkbMultiPolygon = 6,
    wkbGeometryCollection = 7
};

enum OGRwkbByteOrder
{
    wkbXDR = 0,
    wkbNDR = 1
};

typedef int	OGRBoolean;

/************************************************************************/
/*                             OGRGeometry                              */
/************************************************************************/
class OGRGeometry
{
  public:
    // standard
    virtual int	getDimension() = 0;
    virtual int	getCoordinateDimension() = 0;

    // IWks Interface
    virtual int	WkbSize() = 0;
    virtual OGRErr importFromWkb( unsigned char *, int=-1 )=0;
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * ) = 0;
    
    // non-standard
    virtual OGRwkbGeometryType getGeometryType() = 0;
    virtual void   dumpReadable( FILE * ) = 0;

#ifdef notdef
    
    // I presume all these should be virtual?  How many
    // should be pure?
    OGRSpatialReference	*getSpatialReference();
    OGREnvelope	*getEnvelope();

    ?		Export(); /* export to well known representation */

    OGRBoolean	IsEmpty();
    OGRBoolean	IsSimple();
    OGRGeometry *getBoundary();

    OGRBoolean	Equal( OGRGeometry * );
    OGRBoolean	Disjoint( OGRGeometry * );
    OGRBoolean	Intersect( OGRGeometry * );
    OGRBoolean	Touch( OGRGeometry * );
    OGRBoolean	Cross( OGRGeometry * );
    OGRBoolean	Within( OGRGeometry * );
    OGRBoolean	Contains( OGRGeometry * );
    OGRBoolean	Overlap( OGRGeometry * );
    OGRBoolean	Relate( OGRGeometry *, const char * );

    double	Distance( OGRGeometry * );
    OGRGeometry *Buffer( double );
    OGRGeometry *ConvexHull();
    OGRGeometry *Intersection(OGRGeometry *);
    OGRGeometry *Union( OGRGeometry * );
    OGRGeometry *Difference( OGRGeometry * );
    OGRGeometry *SymmetricDifference( OGRGeometry * );
#endif    
};

class OGRRawPoint
{
  public:
    double	x;
    double	y;
};

/************************************************************************/
/*                               OGRPoint                               */
/************************************************************************/
class OGRPoint : public OGRGeometry
{
    double	x;
    double	y;

  public:
    		OGRPoint();
                OGRPoint( double, double );
                OGRPoint( OGRRawPoint & );
                OGRPoint( OGRRawPoint * );
    virtual     ~OGRPoint();

    // IWks Interface
    virtual int	WkbSize();
    virtual OGRErr importFromWkb( unsigned char *, int=-1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * );
    
    virtual OGRwkbGeometryType getGeometryType();
    virtual void dumpReadable( FILE * );
    
    virtual int	getDimension();
    virtual int	getCoordinateDimension();

    double	getX() { return x; }
    double	getY() { return y; }

    void	setX( double xIn ) { x = xIn; }
    void	setY( double yIn ) { y = yIn; }
};

/************************************************************************/
/*                               OGRCurve                               */
/*                                                                      */
/*      This is a pure virtual class.                                   */
/************************************************************************/

class OGRCurve : public OGRGeometry
{
    int 	nPointCount;
    OGRRawPoint	*paoPoints;

  protected:
    virtual void dumpPointsReadable( FILE * );
    
  public:
    		OGRCurve();
    virtual     ~OGRCurve();

    // IWks Interface
    virtual int	WkbSize();
    virtual OGRErr importFromWkb( unsigned char *, int = -1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * );
    
    virtual int	getDimension();
    virtual int	getCoordinateDimension();

    int		getNumPoints() { return nPointCount; }

    void	getPoint( int, OGRPoint * );
    double	getX( int i ) { return paoPoints[i].x; }
    double	getY( int i ) { return paoPoints[i].y; }

    void	setNumPoints( int );
    void	setPoint( int, OGRPoint * );
    void	setPoint( int, double, double );
    void	addPoint( OGRPoint * );
    void	addPoint( double, double );
};

/************************************************************************/
/*                            OGRLineString                             */
/************************************************************************/
class OGRLineString : public OGRCurve
{
  public:
    		OGRLineString();
    virtual     ~OGRLineString();

    virtual OGRwkbGeometryType getGeometryType();
    
    virtual void dumpReadable( FILE * );
};

/************************************************************************/
/*                          OGRGeometryFactory                          */
/************************************************************************/
class OGRGeometryFactory
{
  public:
    static OGRErr createFromWkb( unsigned char *, OGRGeometry **, int = -1 );
};

#endif /* ndef _OGR_GEOMETRY_H_INCLUDED */
