/**********************************************************************
 * $Id: mitab_feature.cpp,v 1.65 2006/07/25 13:22:58 dmorissette Exp $
 *
 * Name:     mitab_feature.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the feature classes specific to MapInfo files.
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2002, Daniel Morissette
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
 * $Log: mitab_feature.cpp,v $
 * Revision 1.65  2006/07/25 13:22:58  dmorissette
 * Fixed initialization of MBR of TABCollection members (bug 1520)
 *
 * Revision 1.64  2006/06/29 19:49:35  dmorissette
 * Fixed problem writing PLINE MULTIPLE to TAB format introduced in
 * MITAB 1.5.0 (bug 1466).
 *
 * Revision 1.63  2006/02/08 05:02:57  dmorissette
 * Fixed crash when attempting to write TABPolyline object with an invalid
 * geometry (GDAL bug 1059)
 *
 * Revision 1.62  2005/11/08 22:02:36  fwarmerdam
 * cast strstr() results as per email from Charles Savage for VC8.
 *
 * Revision 1.61  2005/10/12 19:13:30  jlacroix
 * bug1012 Make sure Style Units are set when seting a feature from StyleString
 *
 * Revision 1.60  2005/10/07 21:22:33  dmorissette
 * Clone region/pline/mpoint components in TABCollection::CloneTABFeature()
 *
 * Revision 1.59  2005/10/06 23:05:08  dmorissette
 * TABCollection: Added automated sync'ing of OGRFeature's geometry in
 * SetRegion/Pline/MpointDirectly() methods (bug 1126)
 *
 * Revision 1.58  2005/10/06 19:15:30  dmorissette
 * Collections: added support for reading/writing pen/brush/symbol ids and
 * for writing collection objects to .TAB/.MAP (bug 1126)
 *
 * Revision 1.57  2005/10/04 15:44:31  dmorissette
 * First round of support for Collection objects. Currently supports reading
 * from .TAB/.MAP and writing to .MIF. Still lacks symbol support and write
 * support. (Based in part on patch and docs from Jim Hope, bug 1126)
 *
 * Revision 1.56  2005/07/14 16:15:05  jlacroix
 * \n and \ are now unescaped internally.
 *
 * Revision 1.55  2005/05/19 15:26:59  jlacroix
 * Implement a method to set the StyleString of a TABFeature.
 * This is done via the ITABFeaturePen, Brush and Symbol classes.
 *
 * Revision 1.54  2004/12/01 18:25:03  dmorissette
 * Fixed potential memory leaks in error conditions (bug 881)
 *
 * Revision 1.53  2004/10/19 14:21:11  jlacroix
 * Support pen width bigger than 7 with GetStyleString. (Bug 683)
 *
 * Revision 1.52  2004/10/11 20:18:27  dmorissette
 * Do not output a BG color in style string for transparent brushes (bug 693)
 *
 * Revision 1.51  2004/06/30 20:29:03  dmorissette
 * Fixed refs to old address danmo@videotron.ca
 *
 * Revision 1.50  2003/12/19 08:30:10  fwarmerdam
 * write 3d geometries to tab in 2D
 *
 * Revision 1.49  2003/03/28 14:01:14  warmerda
 * use TRUE/FALSE in place of true/false
 *
 * Revision 1.48  2002/08/27 17:17:33  warmerda
 * handle errors better, auto-add FID column if there aren't any columns
 *
 * Revision 1.47  2002/06/17 15:00:30  julien
 * Add IsInteriorRing() function in TABRegion to validate if a ring is internal
 *
 * Revision 1.46  2002/03/26 03:17:13  daniel
 * Added Get/SetCenter() to MultiPoint
 *
 * Revision 1.45  2002/03/26 01:48:40  daniel
 * Added Multipoint object type (V650)
 *
 * Revision 1.44  2002/01/23 20:29:56  daniel
 * Fixed warning produced by CPLAssert() in non-DEBUG mode.
 *
 * Revision 1.43  2001/12/05 22:38:59  daniel
 * Fixed problems writing TAB_GEOM_LINE (bug 610, 633).
 *
 * Revision 1.42  2001/11/23 22:50:17  daniel
 * Fixed geometry type assertion in TABPolyline::WriteGeomToMAPFile (bug605)
 *
 * Revision 1.41  2001/11/17 21:54:06  daniel
 * Made several changes in order to support writing objects in 16 bits 
 * coordinate format.  New TABMAPObjHdr-derived classes are used to hold 
 * object info in mem until block is full.
 *
 * Revision 1.40  2001/09/17 21:33:44  daniel
 * TABText: do not produce an error when reading 0-length text strings.
 *
 * Revision 1.39  2001/09/05 13:33:33  daniel
 * TABPolyline::ValidateMapInfoType(): return TAB_GEOM_NONE if numpoints < 2
 *
 * Revision 1.38  2001/07/03 03:14:52  daniel
 * GetLabelStyleString(): take line spacing and num. of lines into account
 * when calculating text height.
 *
 * Revision 1.37  2001/06/25 01:04:21  daniel
 * StyleString fixes: include font name in text style string, and placed
 * brush before pen in region style strings.
 *
 * Revision 1.36  2001/05/01 18:34:12  daniel
 * TABRegion: use outside/inside ring relationship to build geometry if
 * the information is available in the source file.
 *
 * Revision 1.35  2001/02/28 07:15:08  daniel
 * Added support for text label line end point
 *
 * Revision 1.34  2001/01/22 16:03:58  warmerda
 * expanded tabs
 *
 * Revision 1.33  2000/11/23 21:11:07  daniel
 * OOpps... VC++ didn't like the way TABPenDef, etc. were initialized
 *
 * Revision 1.32  2000/11/23 20:47:45  daniel
 * Use MI defaults for Pen, Brush, Font, Symbol instead of all zeros
 *
 * Revision 1.31  2000/10/18 03:23:37  warmerda
 * ensure TABText::m_dWidth is initialized in constructor
 *
 * Revision 1.30  2000/10/03 19:29:51  daniel
 * Include OGR StyleString stuff (implemented by Stephane)
 *
 * Revision 1.29  2000/09/19 17:23:52  daniel
 * Maintain and/or compute valid region and polyline center/label point
 *
 * Revision 1.28  2000/09/07 23:32:13  daniel
 * Added RecordDeletedFlag to TABFeature with get/set methods
 *
 * Revision 1.27  2000/07/10 14:56:25  daniel
 * Handle m_nOriginQuadrant==0 as quadrant 3 (reverse x and y axis)
 *
 * Revision 1.26  2000/04/21 12:39:05  daniel
 * Added TABPolyline::GetNumParts()/GetPartRef()
 *
 * Revision 1.25  2000/02/28 16:44:10  daniel
 * Added support for indexed, unique, and for new V450 object types
 *
 * ...
 *
 * Revision 1.1  1999/07/12 04:18:24  daniel
 * Initial checkin
 *
 **********************************************************************/

#include "mitab.h"
#include "mitab_utils.h"
#include "mitab_geometry.h"

/*=====================================================================
 *                      class TABFeature
 *====================================================================*/


/**********************************************************************
 *                   TABFeature::TABFeature()
 *
 * Constructor.
 **********************************************************************/
TABFeature::TABFeature(OGRFeatureDefn *poDefnIn):
               OGRFeature(poDefnIn)
{
    m_nMapInfoType = TAB_GEOM_NONE;
    m_bDeletedFlag = FALSE;

    SetMBR(0.0, 0.0, 0.0, 0.0);
}

/**********************************************************************
 *                   TABFeature::~TABFeature()
 *
 * Destructor.
 **********************************************************************/
TABFeature::~TABFeature()
{
}


/**********************************************************************
 *                     TABFeature::CopyTABFeatureBase()
 *
 * Used by CloneTABFeature() to copy the basic (fields, geometry, etc.)
 * TABFeature members.
 *
 * The newly created feature is owned by the caller, and will have it's own
 * reference to the OGRFeatureDefn.
 *
 * It is possible to create the clone with a different OGRFeatureDefn,
 * in this case, the fields won't be copied of course.
 *
 **********************************************************************/
void TABFeature::CopyTABFeatureBase(TABFeature *poDestFeature)
{
    /*-----------------------------------------------------------------
     * Copy fields only if OGRFeatureDefn is the same
     *----------------------------------------------------------------*/
    OGRFeatureDefn *poThisDefnRef = GetDefnRef();

    if (poThisDefnRef == poDestFeature->GetDefnRef())
    {
        for( int i = 0; i < poThisDefnRef->GetFieldCount(); i++ )
        {
            poDestFeature->SetField( i, GetRawFieldRef( i ) );
        }
    }

    /*-----------------------------------------------------------------
     * Copy the geometry
     *----------------------------------------------------------------*/
    poDestFeature->SetGeometry( GetGeometryRef() );

    double dXMin, dYMin, dXMax, dYMax;
    GetMBR(dXMin, dYMin, dXMax, dYMax);
    poDestFeature->SetMBR(dXMin, dYMin, dXMax, dYMax);

    // m_nMapInfoType is not carried but it is not required anyways.
    // it will default to TAB_GEOM_NONE
}


/**********************************************************************
 *                     TABFeature::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * The newly created feature is owned by the caller, and will have it's own
 * reference to the OGRFeatureDefn.
 *
 * It is possible to create the clone with a different OGRFeatureDefn,
 * in this case, the fields won't be copied of course.
 *
 * This method calls the generic TABFeature::CopyTABFeatureBase() and 
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABFeature::CloneTABFeature(OGRFeatureDefn *poNewDefn/*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABFeature *poNew = new TABFeature(poNewDefn ? poNewDefn : GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // Nothing to do for this class

    return poNew;
}

/**********************************************************************
 *                   TABFeature::SetMBR()
 *
 * Set the values for the MBR corners for this feature.
 **********************************************************************/
void TABFeature::SetMBR(double dXMin, double dYMin, 
                        double dXMax, double dYMax)
{
    m_dXMin = MIN(dXMin, dXMax);
    m_dYMin = MIN(dYMin, dYMax);
    m_dXMax = MAX(dXMin, dXMax);
    m_dYMax = MAX(dYMin, dYMax);
}

/**********************************************************************
 *                   TABFeature::GetMBR()
 *
 * Return the values for the MBR corners for this feature.
 **********************************************************************/
void TABFeature::GetMBR(double &dXMin, double &dYMin, 
                        double &dXMax, double &dYMax)
{
    dXMin = m_dXMin;
    dYMin = m_dYMin;
    dXMax = m_dXMax;
    dYMax = m_dYMax;
}

/**********************************************************************
 *                   TABFeature::GetIntMBR()
 *
 * Return the integer coordinates values of the MBR of this feature.
 **********************************************************************/
void TABFeature::GetIntMBR(GInt32 &nXMin, GInt32 &nYMin, 
                           GInt32 &nXMax, GInt32 &nYMax)
{
    nXMin = m_nXMin;
    nYMin = m_nYMin;
    nXMax = m_nXMax;
    nYMax = m_nYMax;
}

/**********************************************************************
 *                   TABFeature::ReadRecordFromDATFile()
 *
 * Fill the fields part of the feature from the contents of the 
 * table record pointed to by poDATFile.
 *
 * It is assumed that poDATFile currently points to the beginning of
 * the table record and that this feature's OGRFeatureDefn has been 
 * properly initialized for this table.
 **********************************************************************/
int TABFeature::ReadRecordFromDATFile(TABDATFile *poDATFile)
{
    int         iField, numFields, nValue;
    double      dValue;
    const char *pszValue;

    CPLAssert(poDATFile);

    numFields = poDATFile->GetNumFields();

    for(iField=0; iField<numFields; iField++)
    {
        switch(poDATFile->GetFieldType(iField))
        {
          case TABFChar:
            pszValue = poDATFile->ReadCharField(poDATFile->
                                                GetFieldWidth(iField));
            SetField(iField, pszValue);
            break;
          case TABFDecimal:
            dValue = poDATFile->ReadDecimalField(poDATFile->
                                                 GetFieldWidth(iField));
            SetField(iField, dValue);
            break;
          case TABFInteger:
            nValue = poDATFile->ReadIntegerField(poDATFile->
                                                 GetFieldWidth(iField));
            SetField(iField, nValue);
            break;
          case TABFSmallInt:
            nValue = poDATFile->ReadSmallIntField(poDATFile->
                                                 GetFieldWidth(iField));
            SetField(iField, nValue);
            break;
          case TABFFloat:
            dValue = poDATFile->ReadFloatField(poDATFile->
                                                 GetFieldWidth(iField));
            SetField(iField, dValue);
            break;
          case TABFLogical:
            pszValue = poDATFile->ReadLogicalField(poDATFile->
                                                 GetFieldWidth(iField));
            SetField(iField, pszValue);
            break;
          case TABFDate:
            pszValue = poDATFile->ReadDateField(poDATFile->
                                                 GetFieldWidth(iField));
            SetField(iField, pszValue);
            break;
          default:
            // Other type???  Impossible!
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "Unsupported field type!");
        }

    }

    return 0;
}

/**********************************************************************
 *                   TABFeature::WriteRecordToDATFile()
 *
 * Write the attribute part of the feature to the .DAT file.
 *
 * It is assumed that poDATFile currently points to the beginning of
 * the table record and that this feature's OGRFeatureDefn has been 
 * properly initialized for this table.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFeature::WriteRecordToDATFile(TABDATFile *poDATFile,
                                     TABINDFile *poINDFile, int *panIndexNo)
{
    int         iField, numFields, nStatus=0;

    CPLAssert(poDATFile);
    CPLAssert(panIndexNo || GetDefnRef()->GetFieldCount() == 0);

    numFields = poDATFile->GetNumFields();

    for(iField=0; nStatus == 0 && iField<numFields; iField++)
    {
        // Hack for "extra" introduced field.
        if( iField >= GetDefnRef()->GetFieldCount() )
        {
            CPLAssert( poDATFile->GetFieldType(iField) == TABFInteger 
                       && iField == 0 );
            nStatus = poDATFile->WriteIntegerField( GetFID(), poINDFile, 0 );
            continue;
        }

        switch(poDATFile->GetFieldType(iField))
        {
          case TABFChar:
            nStatus = poDATFile->WriteCharField(GetFieldAsString(iField),
                                      poDATFile->GetFieldWidth(iField),
                                                poINDFile, panIndexNo[iField]);
            break;
          case TABFDecimal:
            nStatus = poDATFile->WriteDecimalField(GetFieldAsDouble(iField),
                                      poDATFile->GetFieldWidth(iField),
                                      poDATFile->GetFieldPrecision(iField),
                                             poINDFile, panIndexNo[iField]);
            break;
          case TABFInteger:
            nStatus = poDATFile->WriteIntegerField(GetFieldAsInteger(iField),
                                                poINDFile, panIndexNo[iField]);
            break;
          case TABFSmallInt:
            nStatus = poDATFile->WriteSmallIntField(GetFieldAsInteger(iField),
                                                poINDFile, panIndexNo[iField]);
            break;
          case TABFFloat:
            nStatus = poDATFile->WriteFloatField(GetFieldAsDouble(iField),
                                                poINDFile, panIndexNo[iField]);
            break;
          case TABFLogical:
            nStatus = poDATFile->WriteLogicalField(GetFieldAsString(iField),
                                                poINDFile, panIndexNo[iField]);
            break;
          case TABFDate:
            nStatus = poDATFile->WriteDateField(GetFieldAsString(iField),
                                                poINDFile, panIndexNo[iField]);
            break;
          default:
            // Other type???  Impossible!
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "Unsupported field type!");
        }

    }

    if (poDATFile->CommitRecordToFile() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABFeature::ReadGeometryFromMAPFile()
 *
 * In derived classes, this method should be reimplemented to
 * fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that before calling ReadGeometryFromMAPFile(), poMAPFile
 * currently points to the beginning of a map object.
 *
 * The current implementation does nothing since instances of TABFeature
 * objects contain no geometry (i.e. TAB_GEOM_NONE).
 * 
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFeature::ReadGeometryFromMAPFile(TABMAPFile * /*poMapFile*/,
                                        TABMAPObjHdr * /*poObjHdr*/)
{
    /*-----------------------------------------------------------------
     * Nothing to do... instances of TABFeature objects contain no geometry.
     *----------------------------------------------------------------*/

    return 0;
}


/**********************************************************************
 *                   TABFeature::ValidateCoordType()
 *
 * Checks the feature envelope to establish if the feature should be
 * written using Compressed coordinates or not and adjust m_nMapInfoType
 * accordingly. Calling this method also sets (initializes) m_nXMin, m_nYMin, 
 * m_nXMax, m_nYMax
 *
 * This function should be used only by the ValidateMapInfoType() 
 * implementations.
 *
 * Returns TRUE if coord. should be compressed, FALSE otherwise
 **********************************************************************/
GBool TABFeature::ValidateCoordType(TABMAPFile * poMapFile)
{
    GBool bCompr = FALSE;
    OGRGeometry *poGeom;

    poGeom = GetGeometryRef();

    /*-------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *------------------------------------------------------------*/
    if (poGeom && poMapFile)
    {
        OGREnvelope oEnv;
        poGeom->getEnvelope(&oEnv);
        poMapFile->Coordsys2Int(oEnv.MinX, oEnv.MinY, m_nXMin, m_nYMin);
        poMapFile->Coordsys2Int(oEnv.MaxX, oEnv.MaxY, m_nXMax, m_nYMax);
        if ((m_nXMax - m_nXMin) < 65536 && (m_nYMax-m_nYMin) < 65536)
        {
            bCompr = TRUE;
        }
        m_nComprOrgX = (m_nXMin + m_nXMax) / 2;
        m_nComprOrgY = (m_nYMin + m_nYMax) / 2;
    }


    /*-------------------------------------------------------------
     * Adjust native type
     *------------------------------------------------------------*/
    if (bCompr && ((m_nMapInfoType%3) == 2))
        m_nMapInfoType--;  // compr = 1, 4, 7, ...
    else if (!bCompr && ((m_nMapInfoType%3) == 1))
        m_nMapInfoType++;  // non-compr = 2, 5, 8, ...

    return bCompr;
}

/**********************************************************************
 *                   TABFeature::ForceCoordTypeAndOrigin()
 *
 * This function is used by TABCollection::ValidateMapInfoType() to force 
 * the coord type and compressed origin of all members of a collection 
 * to be the same. (A replacement for ValidateCoordType() for this 
 * specific case)
 **********************************************************************/
void TABFeature::ForceCoordTypeAndOrigin(int nMapInfoType, GBool bCompr,
                                         GInt32 nComprOrgX, GInt32 nComprOrgY,
                                         GInt32 nXMin, GInt32 nYMin, 
                                         GInt32 nXMax, GInt32 nYMax)
{
    /*-------------------------------------------------------------
     * Set Compressed Origin and adjust native type
     *------------------------------------------------------------*/
    m_nComprOrgX = nComprOrgX;
    m_nComprOrgY = nComprOrgY;

    m_nMapInfoType = nMapInfoType;

    if (bCompr && ((m_nMapInfoType%3) == 2))
        m_nMapInfoType--;  // compr = 1, 4, 7, ...
    else if (!bCompr && ((m_nMapInfoType%3) == 1))
        m_nMapInfoType++;  // non-compr = 2, 5, 8, ...

    m_nXMin = nXMin;
    m_nYMin = nYMin;
    m_nXMax = nXMax;
    m_nYMax = nYMax;
}

/**********************************************************************
 *                   TABFeature::WriteGeometryToMAPFile()
 *
 *
 * In derived classes, this method should be reimplemented to
 * write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that before calling WriteGeometryToMAPFile(), poMAPFile
 * currently points to a valid map object.
 *
 * The current implementation does nothing since instances of TABFeature
 * objects contain no geometry (i.e. TAB_GEOM_NONE).
 * 
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFeature::WriteGeometryToMAPFile(TABMAPFile * /* poMapFile*/,
                                       TABMAPObjHdr * /*poObjHdr*/)
{
    /*-----------------------------------------------------------------
     * Nothing to do... instances of TABFeature objects contain no geometry.
     *----------------------------------------------------------------*/

    return 0;
}

/**********************************************************************
 *                   TABFeature::DumpMID()
 *
 * Dump feature attributes in a format similar to .MID data records.
 **********************************************************************/
void TABFeature::DumpMID(FILE *fpOut /*=NULL*/)
{
    OGRFeatureDefn      *poDefn = GetDefnRef();

    if (fpOut == NULL)
        fpOut = stdout;

    for( int iField = 0; iField < GetFieldCount(); iField++ )
    {
        OGRFieldDefn    *poFDefn = poDefn->GetFieldDefn(iField);
        
        fprintf( fpOut, "  %s (%s) = %s\n",
                 poFDefn->GetNameRef(),
                 OGRFieldDefn::GetFieldTypeName(poFDefn->GetType()),
                 GetFieldAsString( iField ) );
    }

    fflush(fpOut);
}

/**********************************************************************
 *                   TABFeature::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF files.
 **********************************************************************/
void TABFeature::DumpMIF(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Generate output... not much to do, feature contains no geometry.
     *----------------------------------------------------------------*/
    fprintf(fpOut, "NONE\n" );

    fflush(fpOut);
}


/*=====================================================================
 *                      class TABPoint
 *====================================================================*/


/**********************************************************************
 *                   TABPoint::TABPoint()
 *
 * Constructor.
 **********************************************************************/
TABPoint::TABPoint(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
}

/**********************************************************************
 *                   TABPoint::~TABPoint()
 *
 * Destructor.
 **********************************************************************/
TABPoint::~TABPoint()
{
}

/**********************************************************************
 *                     TABPoint::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CloneTABFeature() and 
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABPoint::CloneTABFeature(OGRFeatureDefn *poNewDefn /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABPoint *poNew = new TABPoint(poNewDefn ? poNewDefn : GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeatureSymbol
    *(poNew->GetSymbolDefRef()) = *GetSymbolDefRef();

    return poNew;
}


/**********************************************************************
 *                   TABPoint::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
int  TABPoint::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    OGRGeometry *poGeom;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry 
     * __TODO__ For now we always write in uncompressed format (until we 
     * find that this is not correct... note that at this point the
     * decision to use compressed/uncompressed will likely be based on
     * the distance between the point and the object block center in
     * integer coordinates being > 32767 or not... remains to be verified)
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
    {
        switch(GetFeatureClass())
        {
          case TABFCFontPoint:
            m_nMapInfoType = TAB_GEOM_FONTSYMBOL;
            break;
          case TABFCCustomPoint:
            m_nMapInfoType = TAB_GEOM_CUSTOMSYMBOL;
            break;
          case TABFCPoint:
          default:
            m_nMapInfoType = TAB_GEOM_SYMBOL;
            break;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPoint: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABPoint::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABPoint::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                      TABMAPObjHdr *poObjHdr)
{
    GInt32              nX, nY;
    double              dX, dY;
    OGRGeometry         *poGeometry;
    TABMAPObjectBlock   *poObjBlock;
    GBool               bComprCoord;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();

    bComprCoord = (m_nMapInfoType == TAB_GEOM_SYMBOL_C);

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    if (m_nMapInfoType == TAB_GEOM_SYMBOL ||
        m_nMapInfoType == TAB_GEOM_SYMBOL_C )
    {
        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        m_nSymbolDefIndex = poObjBlock->ReadByte();   // Symbol index
        poMapFile->ReadSymbolDef(m_nSymbolDefIndex, &m_sSymbolDef);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/
    poMapFile->Int2Coordsys(nX, nY, dX, dY);
    poGeometry = new OGRPoint(dX, dY);
    
    SetGeometryDirectly(poGeometry);

    SetMBR(dX, dY, dX, dY);

    return 0;
}

/**********************************************************************
 *                   TABPoint::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABPoint::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                       TABMAPObjHdr *poObjHdr)
{
    GInt32              nX, nY;
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPoint: Missing or Invalid Geometry!");
        return -1;
    }

    poMapFile->Coordsys2Int(poPoint->getX(), poPoint->getY(), nX, nY);

    /*-----------------------------------------------------------------
     * Copy object information
     *----------------------------------------------------------------*/
    TABMAPObjPoint *poPointHdr = (TABMAPObjPoint *)poObjHdr;

    poPointHdr->m_nX = nX;
    poPointHdr->m_nY = nY;
    poPointHdr->SetMBR(nX, nY, nX, nY);

    m_nSymbolDefIndex = poMapFile->WriteSymbolDef(&m_sSymbolDef);
    poPointHdr->m_nSymbolId = m_nSymbolDefIndex;      // Symbol index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *                   TABPoint::GetX()
 *
 * Return this point's X coordinate.
 **********************************************************************/
double TABPoint::GetX()
{
    OGRGeometry *poGeom;
    OGRPoint    *poPoint=NULL;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPoint: Missing or Invalid Geometry!");
        return 0.0;
    }

    return poPoint->getX();
}

/**********************************************************************
 *                   TABPoint::GetY()
 *
 * Return this point's Y coordinate.
 **********************************************************************/
double TABPoint::GetY()
{
    OGRGeometry *poGeom;
    OGRPoint    *poPoint=NULL;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPoint: Missing or Invalid Geometry!");
        return 0.0;
    }

    return poPoint->getY();
}


/**********************************************************************
 *                   TABPoint::GetStyleString()
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABPoint::GetStyleString()
{
    if (m_pszStyleString == NULL)
    {
        m_pszStyleString = CPLStrdup(GetSymbolStyleString());
    }

    return m_pszStyleString;
}


/**********************************************************************
 *                   TABPoint::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF POINTs.
 **********************************************************************/
void TABPoint::DumpMIF(FILE *fpOut /*=NULL*/)
{
    OGRGeometry *poGeom;
    OGRPoint    *poPoint;

    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPoint: Missing or Invalid Geometry!");
        return;
    }

    /*-----------------------------------------------------------------
     * Generate output
     *----------------------------------------------------------------*/
    fprintf(fpOut, "POINT %.15g %.15g\n", poPoint->getX(), poPoint->getY() );

    DumpSymbolDef(fpOut);

    /*-----------------------------------------------------------------
     * Handle stuff specific to derived classes
     *----------------------------------------------------------------*/
    if (GetFeatureClass() == TABFCFontPoint)
    {
        TABFontPoint *poFeature = (TABFontPoint *)this;
        fprintf(fpOut, "  m_nFontStyle     = 0x%2.2x (%d)\n", 
                poFeature->GetFontStyleTABValue(),
                poFeature->GetFontStyleTABValue());

        poFeature->DumpFontDef(fpOut);
    }
    if (GetFeatureClass() == TABFCCustomPoint)
    {
        TABCustomPoint *poFeature = (TABCustomPoint *)this;

        fprintf(fpOut, "  m_nUnknown_      = 0x%2.2x (%d)\n", 
                poFeature->m_nUnknown_, poFeature->m_nUnknown_);
        fprintf(fpOut, "  m_nCustomStyle   = 0x%2.2x (%d)\n", 
                poFeature->GetCustomSymbolStyle(), 
                poFeature->GetCustomSymbolStyle());

        poFeature->DumpFontDef(fpOut);
    }

    fflush(fpOut);
}

/*=====================================================================
 *                      class TABFontPoint
 *====================================================================*/


/**********************************************************************
 *                   TABFontPoint::TABFontPoint()
 *
 * Constructor.
 **********************************************************************/
TABFontPoint::TABFontPoint(OGRFeatureDefn *poDefnIn):
              TABPoint(poDefnIn)
{
    m_nFontStyle = 0;
    m_dAngle = 0.0;
}

/**********************************************************************
 *                   TABFontPoint::~TABFontPoint()
 *
 * Destructor.
 **********************************************************************/
TABFontPoint::~TABFontPoint()
{
}

/**********************************************************************
 *                     TABFontPoint::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CloneTABFeature() and 
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABFontPoint::CloneTABFeature(OGRFeatureDefn *poNewDefn /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABFontPoint *poNew = new TABFontPoint(poNewDefn ? poNewDefn : 
                                                       GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeatureSymbol
    *(poNew->GetSymbolDefRef()) = *GetSymbolDefRef();

    // ITABFeatureFont
    *(poNew->GetFontDefRef()) = *GetFontDefRef();

    poNew->SetSymbolAngle( GetSymbolAngle() );
    poNew->SetFontStyleTABValue( GetFontStyleTABValue() );

    return poNew;
}

/**********************************************************************
 *                   TABFontPoint::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFontPoint::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                          TABMAPObjHdr *poObjHdr)
{
    GInt32              nX, nY;
    double              dX, dY;
    OGRGeometry         *poGeometry;
    TABMAPObjectBlock   *poObjBlock;
    GBool               bComprCoord;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();

    bComprCoord = (m_nMapInfoType == TAB_GEOM_FONTSYMBOL_C );

    /*-----------------------------------------------------------------
     * Read object information
     * NOTE: This symbol type does not contain a reference to a
     * SymbolDef block in the file, but we still use the m_sSymbolDef
     * structure to store the information inside the class so that the
     * ITABFeatureSymbol methods work properly for the class user.
     *----------------------------------------------------------------*/
    if (m_nMapInfoType == TAB_GEOM_FONTSYMBOL ||
        m_nMapInfoType == TAB_GEOM_FONTSYMBOL_C )
    {
        m_nSymbolDefIndex = -1;
        m_sSymbolDef.nRefCount = 0;

        m_sSymbolDef.nSymbolNo  = poObjBlock->ReadByte();  // shape
        m_sSymbolDef.nPointSize = poObjBlock->ReadByte();  // point size

        m_nFontStyle            = poObjBlock->ReadInt16();  // font style

        m_sSymbolDef.rgbColor   = poObjBlock->ReadByte()*256*256 +
                                  poObjBlock->ReadByte()*256 +
                                  poObjBlock->ReadByte();

        poObjBlock->ReadByte();         // ??? BG Color ???
        poObjBlock->ReadByte();         // ???
        poObjBlock->ReadByte();         // ???

        /*-------------------------------------------------------------
         * Symbol Angle, in thenths of degree.
         * Contrary to arc start/end angles, no conversion based on 
         * origin quadrant is required here
         *------------------------------------------------------------*/
        m_dAngle       = poObjBlock->ReadInt16()/10.0;

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);

        m_nFontDefIndex = poObjBlock->ReadByte();      // Font name index
        poMapFile->ReadFontDef(m_nFontDefIndex, &m_sFontDef);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/
    poMapFile->Int2Coordsys(nX, nY, dX, dY);
    poGeometry = new OGRPoint(dX, dY);
    
    SetGeometryDirectly(poGeometry);

    SetMBR(dX, dY, dX, dY);

    return 0;
}

/**********************************************************************
 *                   TABFontPoint::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFontPoint::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                       TABMAPObjHdr *poObjHdr)
{
    GInt32              nX, nY;
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABFontPoint: Missing or Invalid Geometry!");
        return -1;
    }

    poMapFile->Coordsys2Int(poPoint->getX(), poPoint->getY(), nX, nY);

    /*-----------------------------------------------------------------
     * Copy object information
     * NOTE: This symbol type does not contain a reference to a
     * SymbolDef block in the file, but we still use the m_sSymbolDef
     * structure to store the information inside the class so that the
     * ITABFeatureSymbol methods work properly for the class user.
     *----------------------------------------------------------------*/
    TABMAPObjFontPoint *poPointHdr = (TABMAPObjFontPoint *)poObjHdr;

    poPointHdr->m_nX = nX;
    poPointHdr->m_nY = nY;
    poPointHdr->SetMBR(nX, nY, nX, nY);

    poPointHdr->m_nSymbolId = (GByte)m_sSymbolDef.nSymbolNo;    // shape
    poPointHdr->m_nPointSize = (GByte)m_sSymbolDef.nPointSize;  // point size
    poPointHdr->m_nFontStyle = m_nFontStyle;                    // font style

    poPointHdr->m_nR = COLOR_R(m_sSymbolDef.rgbColor);
    poPointHdr->m_nG = COLOR_G(m_sSymbolDef.rgbColor);
    poPointHdr->m_nB = COLOR_B(m_sSymbolDef.rgbColor);

    /*-------------------------------------------------------------
     * Symbol Angle, in thenths of degree.
     * Contrary to arc start/end angles, no conversion based on 
     * origin quadrant is required here
     *------------------------------------------------------------*/
    poPointHdr->m_nAngle = ROUND_INT(m_dAngle * 10.0);

    // Write Font Def
    m_nFontDefIndex = poMapFile->WriteFontDef(&m_sFontDef);
    poPointHdr->m_nFontId = m_nFontDefIndex;      // Font name index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABFontPoint::QueryFontStyle()
 *
 * Return TRUE if the specified font style attribute is turned ON,
 * or FALSE otherwise.  See enum TABFontStyle for the list of styles
 * that can be queried on.
 **********************************************************************/
GBool TABFontPoint::QueryFontStyle(TABFontStyle eStyleToQuery)
{
    return (m_nFontStyle & (int)eStyleToQuery) ? TRUE: FALSE;
}

void TABFontPoint::ToggleFontStyle(TABFontStyle eStyleToToggle, GBool bStyleOn)
{
    if (bStyleOn)
        m_nFontStyle |=  (int)eStyleToToggle;
    else
        m_nFontStyle &=  ~(int)eStyleToToggle;
}

/**********************************************************************
 *                   TABFontPoint::GetFontStyleMIFValue()
 *
 * Return the Font Style value for this object using the style values
 * that are used in a MIF FONT() clause.  See MIF specs (appendix A).
 *
 * The reason why we have to differentiate between the TAB and the MIF font
 * style values is that in TAB, TABFSBox is included in the style value
 * as code 0x100, but in MIF it is not included, instead it is implied by
 * the presence of the BG color in the FONT() clause (the BG color is 
 * present only when TABFSBox or TABFSHalo is set).
 * This also has the effect of shifting all the other style values > 0x100
 * by 1 byte.
 *
 * NOTE: Even if there is no BG color for font symbols, we inherit this
 * problem because Font Point styles use the same codes as Text Font styles.
 **********************************************************************/
int TABFontPoint::GetFontStyleMIFValue()
{
    // The conversion is simply to remove bit 0x100 from the value and shift
    // down all values past this bit.
    return (m_nFontStyle & 0xff) + (m_nFontStyle & (0xff00-0x0100))/2;
}

void TABFontPoint:: SetFontStyleMIFValue(int nStyle)
{
    m_nFontStyle = (nStyle & 0xff) + (nStyle & 0x7f00)*2;
}

/**********************************************************************
 *                   TABFontPoint::SetSymbolAngle()
 *
 * Set the symbol angle value in degrees, making sure the value is
 * always in the range [0..360]
 **********************************************************************/
void TABFontPoint::SetSymbolAngle(double dAngle)
{
    while(dAngle < 0.0)   dAngle += 360.0;
    while(dAngle > 360.0) dAngle -= 360.0;

    m_dAngle = dAngle;
}


/**********************************************************************
 *                   TABFontPoint::GetStyleString()
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABFontPoint::GetStyleString()
{
    if (m_pszStyleString == NULL)
    {
        m_pszStyleString = CPLStrdup(GetSymbolStyleString(GetSymbolAngle()));
    }

    return m_pszStyleString;
}


/*=====================================================================
 *                      class TABCustomPoint
 *====================================================================*/


/**********************************************************************
 *                   TABCustomPoint::TABCustomPoint()
 *
 * Constructor.
 **********************************************************************/
TABCustomPoint::TABCustomPoint(OGRFeatureDefn *poDefnIn):
                    TABPoint(poDefnIn)
{
    m_nUnknown_ = m_nCustomStyle = 0;
}

/**********************************************************************
 *                   TABCustomPoint::~TABCustomPoint()
 *
 * Destructor.
 **********************************************************************/
TABCustomPoint::~TABCustomPoint()
{
}

/**********************************************************************
 *                     TABCustomPoint::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CloneTABFeature() and 
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABCustomPoint::CloneTABFeature(OGRFeatureDefn *poNewDefn/*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABCustomPoint *poNew = new TABCustomPoint(poNewDefn ? poNewDefn : 
                                                           GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeatureSymbol
    *(poNew->GetSymbolDefRef()) = *GetSymbolDefRef();

    // ITABFeatureFont
    *(poNew->GetFontDefRef()) = *GetFontDefRef();

    poNew->SetCustomSymbolStyle( GetCustomSymbolStyle() );

    return poNew;
}

/**********************************************************************
 *                   TABCustomPoint::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABCustomPoint::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                            TABMAPObjHdr *poObjHdr)
{
    GInt32              nX, nY;
    double              dX, dY;
    OGRGeometry         *poGeometry;
    TABMAPObjectBlock   *poObjBlock;
    GBool               bComprCoord;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();

    bComprCoord = (m_nMapInfoType == TAB_GEOM_CUSTOMSYMBOL_C);

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    if (m_nMapInfoType == TAB_GEOM_CUSTOMSYMBOL ||
        m_nMapInfoType == TAB_GEOM_CUSTOMSYMBOL_C )
    {
        m_nUnknown_    = poObjBlock->ReadByte();  // ??? 
        m_nCustomStyle = poObjBlock->ReadByte();  // 0x01=Show BG,
                                                  // 0x02=Apply Color

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);

        m_nSymbolDefIndex = poObjBlock->ReadByte();   // Symbol index
        poMapFile->ReadSymbolDef(m_nSymbolDefIndex, &m_sSymbolDef);

        m_nFontDefIndex = poObjBlock->ReadByte();    // Font index
        poMapFile->ReadFontDef(m_nFontDefIndex, &m_sFontDef);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/
    poMapFile->Int2Coordsys(nX, nY, dX, dY);
    poGeometry = new OGRPoint(dX, dY);
    
    SetGeometryDirectly(poGeometry);

    SetMBR(dX, dY, dX, dY);

    return 0;
}

/**********************************************************************
 *                   TABCustomPoint::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABCustomPoint::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                       TABMAPObjHdr *poObjHdr)
{
    GInt32              nX, nY;
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABCustomPoint: Missing or Invalid Geometry!");
        return -1;
    }

    poMapFile->Coordsys2Int(poPoint->getX(), poPoint->getY(), nX, nY);

    /*-----------------------------------------------------------------
     * Copy object information
     *----------------------------------------------------------------*/
    TABMAPObjCustomPoint *poPointHdr = (TABMAPObjCustomPoint *)poObjHdr;

    poPointHdr->m_nX = nX;
    poPointHdr->m_nY = nY;
    poPointHdr->SetMBR(nX, nY, nX, nY);
    poPointHdr->m_nUnknown_ = m_nUnknown_;
    poPointHdr->m_nCustomStyle = m_nCustomStyle;// 0x01=Show BG,
                                               // 0x02=Apply Color

    m_nSymbolDefIndex = poMapFile->WriteSymbolDef(&m_sSymbolDef);
    poPointHdr->m_nSymbolId = m_nSymbolDefIndex;      // Symbol index

    m_nFontDefIndex = poMapFile->WriteFontDef(&m_sFontDef);
    poPointHdr->m_nFontId = m_nFontDefIndex;      // Font index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *                   TABCustomPoint::GetStyleString()
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABCustomPoint::GetStyleString()
{
    if (m_pszStyleString == NULL)
    {
        m_pszStyleString = CPLStrdup(GetSymbolStyleString());
    }

    return m_pszStyleString;
}

/*=====================================================================
 *                      class TABPolyline
 *====================================================================*/


/**********************************************************************
 *                   TABPolyline::TABPolyline()
 *
 * Constructor.
 **********************************************************************/
TABPolyline::TABPolyline(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
    m_bCenterIsSet = FALSE;
    m_bSmooth = FALSE;
}

/**********************************************************************
 *                   TABPolyline::~TABPolyline()
 *
 * Destructor.
 **********************************************************************/
TABPolyline::~TABPolyline()
{
}

/**********************************************************************
 *                     TABPolyline::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CloneTABFeature() and 
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABPolyline::CloneTABFeature(OGRFeatureDefn *poNewDefn/*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABPolyline *poNew = new TABPolyline(poNewDefn ? poNewDefn : GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeaturePen
    *(poNew->GetPenDefRef()) = *GetPenDefRef();

    poNew->m_bSmooth = m_bSmooth;
    poNew->m_bCenterIsSet = m_bCenterIsSet;
    poNew->m_dCenterX = m_dCenterX;
    poNew->m_dCenterY = m_dCenterY;

    return poNew;
}

/**********************************************************************
 *                   TABPolyline::GetNumParts()
 *
 * Return the total number of parts in this object.
 *
 * Returns 0 if the geometry contained in the object is invalid or missing.
 **********************************************************************/
int TABPolyline::GetNumParts()
{
    OGRGeometry         *poGeom;
    int                 numParts = 0;

    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
    {
        /*-------------------------------------------------------------
         * Simple polyline
         *------------------------------------------------------------*/
        numParts = 1;
    }
    else if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)
    {
        /*-------------------------------------------------------------
         * Multiple polyline
         *------------------------------------------------------------*/
        OGRMultiLineString *poMultiLine = (OGRMultiLineString*)poGeom;
        numParts = poMultiLine->getNumGeometries();
    }

    return numParts;
}

/**********************************************************************
 *                   TABPolyline::GetPartRef()
 *
 * Returns a reference to the specified OGRLineString number, hiding the
 * complexity of dealing with OGRMultiLineString vs OGRLineString cases.
 *
 * Returns NULL if the geometry contained in the object is invalid or 
 * missing or if the specified part index is invalid.
 **********************************************************************/
OGRLineString *TABPolyline::GetPartRef(int nPartIndex)
{
    OGRGeometry         *poGeom;

    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString && nPartIndex==0)
    {
        /*-------------------------------------------------------------
         * Simple polyline
         *------------------------------------------------------------*/
        return (OGRLineString *)poGeom;
    }
    else if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)
    {
        /*-------------------------------------------------------------
         * Multiple polyline
         *------------------------------------------------------------*/
        OGRMultiLineString *poMultiLine = (OGRMultiLineString*)poGeom;
        if (nPartIndex >= 0 &&
            nPartIndex < poMultiLine->getNumGeometries())
        {
            return (OGRLineString*)poMultiLine->getGeometryRef(nPartIndex);
        }
        else
            return NULL;
    }

    return NULL;
}

/**********************************************************************
 *                   TABPolyline::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
int  TABPolyline::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    OGRGeometry   *poGeom;
    OGRMultiLineString *poMultiLine = NULL;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
    {
        /*-------------------------------------------------------------
         * Simple polyline
         *------------------------------------------------------------*/
        OGRLineString *poLine = (OGRLineString*)poGeom;
        if ( poLine->getNumPoints() > TAB_300_MAX_VERTICES)
        {
            m_nMapInfoType = TAB_GEOM_V450_MULTIPLINE;
        }
        else if ( poLine->getNumPoints() > 2 )
        {
            m_nMapInfoType = TAB_GEOM_PLINE;
        }
        else if ( poLine->getNumPoints() == 2 )
        {
            m_nMapInfoType = TAB_GEOM_LINE;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "TABPolyline: Geometry must contain at least 2 points.");
            m_nMapInfoType = TAB_GEOM_NONE;
        }
    }
    else if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)
    {
        /*-------------------------------------------------------------
         * Multiple polyline... validate all components
         *------------------------------------------------------------*/
        int iLine, numLines;
        GInt32 numPointsTotal = 0;
        poMultiLine = (OGRMultiLineString*)poGeom;
        numLines = poMultiLine->getNumGeometries();

        m_nMapInfoType = TAB_GEOM_MULTIPLINE;

        for(iLine=0; iLine < numLines; iLine++)
        {
            poGeom = poMultiLine->getGeometryRef(iLine);
            if (poGeom && wkbFlatten(poGeom->getGeometryType()) != wkbLineString)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABPolyline: Object contains an invalid Geometry!");
                m_nMapInfoType = TAB_GEOM_NONE;
                numPointsTotal = 0;
                break;
            }
            OGRLineString *poLine = (OGRLineString*)poGeom;
            numPointsTotal += poLine->getNumPoints();
        }

        if (numPointsTotal > TAB_300_MAX_VERTICES)
            m_nMapInfoType = TAB_GEOM_V450_MULTIPLINE;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPolyline: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    /*-----------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *
     * __TODO__ We never write type LINE (2 points line) as compressed
     * for the moment.  If we ever do it, then the decision to write
     * a 2 point line in compressed coordinates or not should take into
     * account the location of the object block MBR, so this would be
     * better handled directly by TABMAPObjLine::WriteObject() since the
     * object block center is not known until it is written to disk.
     *----------------------------------------------------------------*/
    if (m_nMapInfoType != TAB_GEOM_LINE)
    {
        ValidateCoordType(poMapFile);
    }

    return m_nMapInfoType;
}


/**********************************************************************
 *                   TABPolyline::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABPolyline::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                         TABMAPObjHdr *poObjHdr)
{
    return ReadGeometryFromMAPFile(poMapFile, poObjHdr, NULL);
}

int TABPolyline::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                         TABMAPObjHdr *poObjHdr,
                                         TABMAPCoordBlock **ppoCoordBlock)
{
    GInt32              nX, nY;
    double              dX, dY, dXMin, dYMin, dXMax, dYMax;
    OGRGeometry         *poGeometry;
    OGRLineString       *poLine;
    GBool               bComprCoord = poObjHdr->IsCompressedType();
    TABMAPCoordBlock    *poCoordBlock = NULL;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poObjHdr->m_nType;

    if (m_nMapInfoType == TAB_GEOM_LINE ||
        m_nMapInfoType == TAB_GEOM_LINE_C )
    {
        /*=============================================================
         * LINE (2 vertices)
         *============================================================*/
        TABMAPObjLine *poLineHdr = (TABMAPObjLine *)poObjHdr;

        m_bSmooth = FALSE;

        poGeometry = poLine = new OGRLineString();
        poLine->setNumPoints(2);

        poMapFile->Int2Coordsys(poLineHdr->m_nX1, poLineHdr->m_nY1, 
                                dXMin, dYMin);
        poLine->setPoint(0, dXMin, dYMin);

        poMapFile->Int2Coordsys(poLineHdr->m_nX2, poLineHdr->m_nY2,
                                dXMax, dYMax);
        poLine->setPoint(1, dXMax, dYMax);

        m_nPenDefIndex = poLineHdr->m_nPenId;      // Pen index
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);
    }
    else if (m_nMapInfoType == TAB_GEOM_PLINE ||
             m_nMapInfoType == TAB_GEOM_PLINE_C )
    {
        /*=============================================================
         * PLINE ( > 2 vertices)
         *============================================================*/
        int     i, numPoints, nStatus;
        GUInt32 nCoordDataSize;
        GInt32  nCoordBlockPtr, nCenterX, nCenterY;

        /*-------------------------------------------------------------
         * Copy data from poObjHdr
         *------------------------------------------------------------*/
        TABMAPObjPLine *poPLineHdr = (TABMAPObjPLine *)poObjHdr;

        nCoordBlockPtr  = poPLineHdr->m_nCoordBlockPtr;
        nCoordDataSize  = poPLineHdr->m_nCoordDataSize;
        //numLineSections = poPLineHdr->m_numLineSections; // Always 1
        m_bSmooth       = poPLineHdr->m_bSmooth;

        // Centroid/label point
        poMapFile->Int2Coordsys(poPLineHdr->m_nLabelX, poPLineHdr->m_nLabelY, 
                                dX, dY);
        SetCenter(dX, dY);

        // Compressed coordinate origin (useful only in compressed case!)
        nCenterX = poPLineHdr->m_nComprOrgX;
        nCenterY = poPLineHdr->m_nComprOrgY;

        // MBR
        poMapFile->Int2Coordsys(poPLineHdr->m_nMinX, poPLineHdr->m_nMinY, 
                                dXMin, dYMin);
        poMapFile->Int2Coordsys(poPLineHdr->m_nMaxX, poPLineHdr->m_nMaxY, 
                                dXMax, dYMax);

        m_nPenDefIndex = poPLineHdr->m_nPenId;        // Pen index
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);

        /*-------------------------------------------------------------
         * Create Geometry and read coordinates
         *------------------------------------------------------------*/
        numPoints = nCoordDataSize/(bComprCoord?4:8);

        poCoordBlock = poMapFile->GetCoordBlock(nCoordBlockPtr);
        if (poCoordBlock == NULL)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Can't access coordinate block at offset %d", 
                     nCoordBlockPtr);
            return -1;
        }

        poCoordBlock->SetComprCoordOrigin(nCenterX, nCenterY);

        poGeometry = poLine = new OGRLineString();
        poLine->setNumPoints(numPoints);

        nStatus = 0;
        for(i=0; nStatus == 0 && i<numPoints; i++)
        {
            nStatus = poCoordBlock->ReadIntCoord(bComprCoord, nX, nY);
            if (nStatus != 0)
                break;
            poMapFile->Int2Coordsys(nX, nY, dX, dY);
            poLine->setPoint(i, dX, dY);
        }

        if (nStatus != 0)
        {
            // Failed ... error message has already been produced
            delete poGeometry;
            return nStatus;
        }   

    }
    else if (m_nMapInfoType == TAB_GEOM_MULTIPLINE ||
             m_nMapInfoType == TAB_GEOM_MULTIPLINE_C ||
             m_nMapInfoType == TAB_GEOM_V450_MULTIPLINE ||
             m_nMapInfoType == TAB_GEOM_V450_MULTIPLINE_C )
    {
        /*=============================================================
         * PLINE MULTIPLE
         *============================================================*/
        int     i, iSection;
        GInt32  nCoordBlockPtr, numLineSections, nCenterX, nCenterY;
        GInt32  nCoordDataSize, numPointsTotal, *panXY;
        OGRMultiLineString      *poMultiLine;
        TABMAPCoordSecHdr       *pasSecHdrs;
        GBool bV450 = (m_nMapInfoType == TAB_GEOM_V450_MULTIPLINE ||
                       m_nMapInfoType == TAB_GEOM_V450_MULTIPLINE_C);

        /*-------------------------------------------------------------
         * Copy data from poObjHdr
         *------------------------------------------------------------*/
        TABMAPObjPLine *poPLineHdr = (TABMAPObjPLine *)poObjHdr;

        nCoordBlockPtr  = poPLineHdr->m_nCoordBlockPtr;
        nCoordDataSize  = poPLineHdr->m_nCoordDataSize;
        numLineSections = poPLineHdr->m_numLineSections;
        m_bSmooth       = poPLineHdr->m_bSmooth;

        // Centroid/label point
        poMapFile->Int2Coordsys(poPLineHdr->m_nLabelX, poPLineHdr->m_nLabelY, 
                                dX, dY);
        SetCenter(dX, dY);

        // Compressed coordinate origin (useful only in compressed case!)
        nCenterX = poPLineHdr->m_nComprOrgX;
        nCenterY = poPLineHdr->m_nComprOrgY;

        // MBR
        poMapFile->Int2Coordsys(poPLineHdr->m_nMinX, poPLineHdr->m_nMinY, 
                                dXMin, dYMin);
        poMapFile->Int2Coordsys(poPLineHdr->m_nMaxX, poPLineHdr->m_nMaxY, 
                                dXMax, dYMax);

        m_nPenDefIndex = poPLineHdr->m_nPenId;        // Pen index
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);

        /*-------------------------------------------------------------
         * Read data from the coord. block
         *------------------------------------------------------------*/
        pasSecHdrs = (TABMAPCoordSecHdr*)CPLMalloc(numLineSections*
                                                   sizeof(TABMAPCoordSecHdr));

        poCoordBlock = poMapFile->GetCoordBlock(nCoordBlockPtr);
        if (poCoordBlock == NULL ||
            poCoordBlock->ReadCoordSecHdrs(bComprCoord, bV450, numLineSections,
                                           pasSecHdrs, numPointsTotal) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed reading coordinate data at offset %d", 
                     nCoordBlockPtr);
            CPLFree(pasSecHdrs);
            return -1;
        }

        poCoordBlock->SetComprCoordOrigin(nCenterX, nCenterY);

        panXY = (GInt32*)CPLMalloc(numPointsTotal*2*sizeof(GInt32));

        if (poCoordBlock->ReadIntCoords(bComprCoord,numPointsTotal,panXY) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed reading coordinate data at offset %d", 
                     nCoordBlockPtr);
            CPLFree(pasSecHdrs);
            CPLFree(panXY);
            return -1;
        }

        /*-------------------------------------------------------------
         * Create a Geometry collection with one line geometry for
         * each coordinates section
         * If object contains only one section, then return a simple LineString
         *------------------------------------------------------------*/
        if (numLineSections > 1)
            poGeometry = poMultiLine = new OGRMultiLineString();
        else
            poGeometry = poMultiLine = NULL;

        for(iSection=0; iSection<numLineSections; iSection++)
        {
            GInt32 *pnXYPtr;
            int     numSectionVertices;

            numSectionVertices = pasSecHdrs[iSection].numVertices;
            pnXYPtr = panXY + (pasSecHdrs[iSection].nVertexOffset * 2);

            poLine = new OGRLineString();
            poLine->setNumPoints(numSectionVertices);

            for(i=0; i<numSectionVertices; i++)
            {
                poMapFile->Int2Coordsys(*pnXYPtr, *(pnXYPtr+1), dX, dY);
                poLine->setPoint(i, dX, dY);
                pnXYPtr += 2;
            }

            if (poGeometry==NULL)
                poGeometry = poLine;
            else if (poMultiLine->addGeometryDirectly(poLine) != OGRERR_NONE)
            {
                CPLAssert(FALSE); // Just in case lower-level lib is modified
            }
            poLine = NULL;
        }

        CPLFree(pasSecHdrs);
        CPLFree(panXY);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    SetGeometryDirectly(poGeometry);

    SetMBR(dXMin, dYMin, dXMax, dYMax);

    /* Return a ref to coord block so that caller can continue reading
     * after the end of this object (used by TABCollection)
     */
    if (ppoCoordBlock)
        *ppoCoordBlock = poCoordBlock;

    return 0;
}

/**********************************************************************
 *                   TABPolyline::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABPolyline::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                       TABMAPObjHdr *poObjHdr)
{
    GInt32              nX, nY;
    TABMAPObjectBlock   *poObjBlock;
    OGRGeometry         *poGeom;
    OGRLineString       *poLine=NULL;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);
    CPLErrorReset();

    poObjBlock = poMapFile->GetCurObjBlock();

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();

    if ((m_nMapInfoType == TAB_GEOM_LINE || 
         m_nMapInfoType == TAB_GEOM_LINE_C ) &&
        poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString &&
        (poLine = (OGRLineString*)poGeom)->getNumPoints() == 2)
    {
        /*=============================================================
         * LINE (2 vertices)
         *============================================================*/
        TABMAPObjLine *poLineHdr = (TABMAPObjLine *)poObjHdr;

        poMapFile->Coordsys2Int(poLine->getX(0), poLine->getY(0), 
                                poLineHdr->m_nX1, poLineHdr->m_nY1);
        poMapFile->Coordsys2Int(poLine->getX(1), poLine->getY(1), 
                                poLineHdr->m_nX2, poLineHdr->m_nY2);
        poLineHdr->SetMBR(poLineHdr->m_nX1, poLineHdr->m_nY1,
                          poLineHdr->m_nX2, poLineHdr->m_nY2 );

        m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
        poLineHdr->m_nPenId = m_nPenDefIndex;      // Pen index

    }
    else if ((m_nMapInfoType == TAB_GEOM_PLINE ||
              m_nMapInfoType == TAB_GEOM_PLINE_C ) &&
             poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString )
    {
        /*=============================================================
         * PLINE ( > 2 vertices and less than 32767 vertices)
         *============================================================*/
        int     i, numPoints, nStatus;
        GUInt32 nCoordDataSize;
        GInt32  nCoordBlockPtr;
        TABMAPCoordBlock *poCoordBlock;
        GBool   bCompressed = poObjHdr->IsCompressedType();

        /*-------------------------------------------------------------
         * Process geometry first...
         *------------------------------------------------------------*/
        poLine = (OGRLineString*)poGeom;
        numPoints = poLine->getNumPoints();
        CPLAssert(numPoints <= TAB_300_MAX_VERTICES);

        poCoordBlock = poMapFile->GetCurCoordBlock();
        poCoordBlock->StartNewFeature();
        nCoordBlockPtr = poCoordBlock->GetCurAddress();
        poCoordBlock->SetComprCoordOrigin(m_nComprOrgX, m_nComprOrgY);

        nStatus = 0;
        for(i=0; nStatus == 0 && i<numPoints; i++)
        {
            poMapFile->Coordsys2Int(poLine->getX(i), poLine->getY(i), nX, nY);
            if ((nStatus = poCoordBlock->WriteIntCoord(nX, nY, 
                                                       bCompressed)) != 0)
            {
                // Failed ... error message has already been produced
                return nStatus;
            }   
        }

        nCoordDataSize = poCoordBlock->GetFeatureDataSize();

        /*-------------------------------------------------------------
         * Copy info to poObjHdr
         *------------------------------------------------------------*/
        TABMAPObjPLine *poPLineHdr = (TABMAPObjPLine *)poObjHdr;

        poPLineHdr->m_nCoordBlockPtr = nCoordBlockPtr;
        poPLineHdr->m_nCoordDataSize = nCoordDataSize;
        poPLineHdr->m_numLineSections = 1;

        poPLineHdr->m_bSmooth = m_bSmooth;

        // MBR
        poPLineHdr->SetMBR(m_nXMin, m_nYMin, m_nXMax, m_nYMax);

        // Polyline center/label point
        double dX, dY;
        if (GetCenter(dX, dY) != -1)
        {
            poMapFile->Coordsys2Int(dX, dY, poPLineHdr->m_nLabelX, 
                                    poPLineHdr->m_nLabelY);
        }
        else
        {
            poPLineHdr->m_nLabelX = m_nComprOrgX;
            poPLineHdr->m_nLabelY = m_nComprOrgY;
        }
        
        // Compressed coordinate origin (useful only in compressed case!)
        poPLineHdr->m_nComprOrgX = m_nComprOrgX;
        poPLineHdr->m_nComprOrgY = m_nComprOrgY;

        m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
        poPLineHdr->m_nPenId = m_nPenDefIndex;      // Pen index

    }
    else if ((m_nMapInfoType == TAB_GEOM_MULTIPLINE ||
                  m_nMapInfoType == TAB_GEOM_MULTIPLINE_C ||
                  m_nMapInfoType == TAB_GEOM_V450_MULTIPLINE ||
                  m_nMapInfoType == TAB_GEOM_V450_MULTIPLINE_C) &&
             poGeom && (wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString ||
                        wkbFlatten(poGeom->getGeometryType()) == wkbLineString) )
    {
        /*=============================================================
         * PLINE MULTIPLE (or single PLINE with more than 32767 vertices)
         *============================================================*/
        int     nStatus=0, i, iLine;
        GInt32  numPointsTotal, numPoints;
        GUInt32 nCoordDataSize;
        GInt32  nCoordBlockPtr, numLines;
        TABMAPCoordBlock        *poCoordBlock;
        OGRMultiLineString      *poMultiLine=NULL;
        TABMAPCoordSecHdr       *pasSecHdrs;
        OGREnvelope             sEnvelope;
        GBool   bCompressed = poObjHdr->IsCompressedType();

        CPLAssert(m_nMapInfoType == TAB_GEOM_MULTIPLINE ||
                  m_nMapInfoType == TAB_GEOM_MULTIPLINE_C ||
                  m_nMapInfoType == TAB_GEOM_V450_MULTIPLINE ||
                  m_nMapInfoType == TAB_GEOM_V450_MULTIPLINE_C);
        /*-------------------------------------------------------------
         * Process geometry first...
         *------------------------------------------------------------*/
        poCoordBlock = poMapFile->GetCurCoordBlock();
        poCoordBlock->StartNewFeature();
        nCoordBlockPtr = poCoordBlock->GetCurAddress();
        poCoordBlock->SetComprCoordOrigin(m_nComprOrgX, m_nComprOrgY);

        if (wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)
        {
            poMultiLine = (OGRMultiLineString*)poGeom;
            numLines = poMultiLine->getNumGeometries();
        }
        else
        {
            poMultiLine = NULL;
            numLines = 1;
        }

        /*-------------------------------------------------------------
         * Build and write array of coord sections headers
         *------------------------------------------------------------*/
        pasSecHdrs = (TABMAPCoordSecHdr*)CPLCalloc(numLines,
                                                   sizeof(TABMAPCoordSecHdr));

        /*-------------------------------------------------------------
         * In calculation of nDataOffset, we have to take into account that
         * V450 header section uses int32 instead of int16 for numVertices
         * and we add another 2 bytes to align with a 4 bytes boundary.
         *------------------------------------------------------------*/
        GBool bV450 = (m_nMapInfoType == TAB_GEOM_V450_MULTIPLINE ||
                       m_nMapInfoType == TAB_GEOM_V450_MULTIPLINE_C );
        int nTotalHdrSizeUncompressed;
        if (bV450)
            nTotalHdrSizeUncompressed = 28 * numLines;
        else
            nTotalHdrSizeUncompressed = 24 * numLines;

        numPointsTotal = 0;
        for(iLine=0; iLine < numLines; iLine++)
        {
            if (poMultiLine)
                poGeom = poMultiLine->getGeometryRef(iLine);
                
            if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
            {
                poLine = (OGRLineString*)poGeom;
                numPoints = poLine->getNumPoints();
                poLine->getEnvelope(&sEnvelope);

                pasSecHdrs[iLine].numVertices = poLine->getNumPoints();
                pasSecHdrs[iLine].numHoles = 0; // It's a line!

                poMapFile->Coordsys2Int(sEnvelope.MinX, sEnvelope.MinY,
                                        pasSecHdrs[iLine].nXMin,
                                        pasSecHdrs[iLine].nYMin);
                poMapFile->Coordsys2Int(sEnvelope.MaxX, sEnvelope.MaxY,
                                        pasSecHdrs[iLine].nXMax,
                                        pasSecHdrs[iLine].nYMax);
                pasSecHdrs[iLine].nDataOffset = nTotalHdrSizeUncompressed +
                                                numPointsTotal*4*2;
                pasSecHdrs[iLine].nVertexOffset = numPointsTotal;

                numPointsTotal += numPoints;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABPolyline: Object contains an invalid Geometry!");
                nStatus = -1;
            }

        }
         
        if (nStatus == 0)
            nStatus = poCoordBlock->WriteCoordSecHdrs(bV450, numLines, 
                                                      pasSecHdrs, bCompressed);

        CPLFree(pasSecHdrs);
        pasSecHdrs = NULL;

        if (nStatus != 0)
            return nStatus;  // Error has already been reported.

        /*-------------------------------------------------------------
         * Then write the coordinates themselves...
         *------------------------------------------------------------*/
        for(iLine=0; nStatus == 0 && iLine < numLines; iLine++)
        {
            if (poMultiLine)
                poGeom = poMultiLine->getGeometryRef(iLine);

            if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
            {
                poLine = (OGRLineString*)poGeom;
                numPoints = poLine->getNumPoints();

                for(i=0; nStatus == 0 && i<numPoints; i++)
                {
                    poMapFile->Coordsys2Int(poLine->getX(i), poLine->getY(i),
                                            nX, nY);
                    if ((nStatus=poCoordBlock->WriteIntCoord(nX, nY,
                                                           bCompressed)) != 0)
                    {
                        // Failed ... error message has already been produced
                        return nStatus;
                    }   
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABPolyline: Object contains an invalid Geometry!");
                return -1;
            }

        }

        nCoordDataSize = poCoordBlock->GetFeatureDataSize();

        /*-------------------------------------------------------------
         * ... and finally copy info to poObjHdr
         *------------------------------------------------------------*/
        TABMAPObjPLine *poPLineHdr = (TABMAPObjPLine *)poObjHdr;

        poPLineHdr->m_nCoordBlockPtr = nCoordBlockPtr;
        poPLineHdr->m_nCoordDataSize = nCoordDataSize;
        poPLineHdr->m_numLineSections = numLines;

        poPLineHdr->m_bSmooth = m_bSmooth;

        // MBR
        poPLineHdr->SetMBR(m_nXMin, m_nYMin, m_nXMax, m_nYMax);

        // Polyline center/label point
        double dX, dY;
        if (GetCenter(dX, dY) != -1)
        {
            poMapFile->Coordsys2Int(dX, dY, poPLineHdr->m_nLabelX, 
                                    poPLineHdr->m_nLabelY);
        }
        else
        {
            poPLineHdr->m_nLabelX = m_nComprOrgX;
            poPLineHdr->m_nLabelY = m_nComprOrgY;
        }
        
        // Compressed coordinate origin (useful only in compressed case!)
        poPLineHdr->m_nComprOrgX = m_nComprOrgX;
        poPLineHdr->m_nComprOrgY = m_nComprOrgY;

        m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
        poPLineHdr->m_nPenId = m_nPenDefIndex;      // Pen index

    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPolyline: Object contains an invalid Geometry!");
        return -1;
    }

    if (CPLGetLastErrorType() == CE_Failure )
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABPolyline::GetStyleString()
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABPolyline::GetStyleString()
{
    if (m_pszStyleString == NULL)
    {
        m_pszStyleString = CPLStrdup(GetPenStyleString());
    }

    return m_pszStyleString;
}


/**********************************************************************
 *                   TABPolyline::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF PLINEs.
 **********************************************************************/
void TABPolyline::DumpMIF(FILE *fpOut /*=NULL*/)
{
    OGRGeometry   *poGeom;
    OGRMultiLineString *poMultiLine = NULL;
    OGRLineString *poLine = NULL;
    int i, numPoints;

    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
    {
        /*-------------------------------------------------------------
         * Generate output for simple polyline
         *------------------------------------------------------------*/
        poLine = (OGRLineString*)poGeom;
        numPoints = poLine->getNumPoints();
        fprintf(fpOut, "PLINE %d\n", numPoints);
        for(i=0; i<numPoints; i++)
            fprintf(fpOut, "%.15g %.15g\n", poLine->getX(i), poLine->getY(i));
    }
    else if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)
    {
        /*-------------------------------------------------------------
         * Generate output for multiple polyline
         *------------------------------------------------------------*/
        int iLine, numLines;
        poMultiLine = (OGRMultiLineString*)poGeom;
        numLines = poMultiLine->getNumGeometries();
        fprintf(fpOut, "PLINE MULTIPLE %d\n", numLines);
        for(iLine=0; iLine < numLines; iLine++)
        {
            poGeom = poMultiLine->getGeometryRef(iLine);
            if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
            {
                poLine = (OGRLineString*)poGeom;
                numPoints = poLine->getNumPoints();
                fprintf(fpOut, " %d\n", numPoints);
                for(i=0; i<numPoints; i++)
                    fprintf(fpOut, "%.15g %.15g\n",poLine->getX(i),poLine->getY(i));
            }
            else
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABPolyline: Object contains an invalid Geometry!");
                return;
            }

        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPolyline: Missing or Invalid Geometry!");
        return;
    }

    if (m_bCenterIsSet)
        fprintf(fpOut, "Center %.15g %.15g\n", m_dCenterX, m_dCenterY);

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();

    fflush(fpOut);
}

/**********************************************************************
 *                   TABPolyline::GetCenter()
 *
 * Returns the center point of the line.  Compute one if it was not 
 * explicitly set:
 *
 * In MapInfo, for a simple or multiple polyline (pline), the center point 
 * in the object definition is supposed to be either the center point of 
 * the pline or the first section of a multiple pline (if an odd number of 
 * points in the pline or first section), or the midway point between the 
 * two central points (if an even number of points involved). 
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABPolyline::GetCenter(double &dX, double &dY)
{
    if (!m_bCenterIsSet)
    {
        OGRGeometry     *poGeom;
        OGRLineString   *poLine = NULL;

        poGeom = GetGeometryRef();
        if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
        {
            poLine = (OGRLineString *)poGeom;
        }
        else if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)
        {
            OGRMultiLineString *poMultiLine = (OGRMultiLineString*)poGeom;
            if (poMultiLine->getNumGeometries() > 0)
                poLine = (OGRLineString *)poMultiLine->getGeometryRef(0);
        }

        if (poLine && poLine->getNumPoints() > 0)
        {
            int i = poLine->getNumPoints()/2;
            if (poLine->getNumPoints() % 2 == 0)
            {
                // Return the midway between the 2 center points
                m_dCenterX = (poLine->getX(i-1) + poLine->getX(i))/2.0;
                m_dCenterY = (poLine->getY(i-1) + poLine->getY(i))/2.0;
            }
            else
            {
                // Return the center point
                m_dCenterX = poLine->getX(i);
                m_dCenterY = poLine->getY(i);
            }
            m_bCenterIsSet = TRUE;
        }
    }

    if (!m_bCenterIsSet)
        return -1;

    dX = m_dCenterX;
    dY = m_dCenterY;
    return 0;
}

/**********************************************************************
 *                   TABPolyline::SetCenter()
 *
 * Set the X,Y coordinates to use as center point for the line.
 **********************************************************************/
void TABPolyline::SetCenter(double dX, double dY)
{
    m_dCenterX = dX;
    m_dCenterY = dY;
    m_bCenterIsSet = TRUE;
}


/*=====================================================================
 *                      class TABRegion
 *====================================================================*/

/**********************************************************************
 *                   TABRegion::TABRegion()
 *
 * Constructor.
 **********************************************************************/
TABRegion::TABRegion(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
    m_bCenterIsSet = FALSE;
    m_bSmooth = FALSE;
}

/**********************************************************************
 *                   TABRegion::~TABRegion()
 *
 * Destructor.
 **********************************************************************/
TABRegion::~TABRegion()
{
}

/**********************************************************************
 *                     TABRegion::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CopyTABFeatureBase() and 
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABRegion::CloneTABFeature(OGRFeatureDefn *poNewDefn/*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABRegion *poNew = new TABRegion(poNewDefn ? poNewDefn : GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeaturePen
    *(poNew->GetPenDefRef()) = *GetPenDefRef();

    // ITABFeatureBrush
    *(poNew->GetBrushDefRef()) = *GetBrushDefRef();

    poNew->m_bSmooth = m_bSmooth;
    poNew->m_bCenterIsSet = m_bCenterIsSet;
    poNew->m_dCenterX = m_dCenterX;
    poNew->m_dCenterY = m_dCenterY;

    return poNew;
}

/**********************************************************************
 *                   TABRegion::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
int  TABRegion::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    OGRGeometry *poGeom;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
                   wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon))
    {
        GInt32 numPointsTotal=0, numRings=GetNumRings();
        for(int i=0; i<numRings; i++)
        {
            OGRLinearRing *poRing = GetRingRef(i);
            if (poRing)
                numPointsTotal += poRing->getNumPoints();
        }
        if (numPointsTotal > TAB_300_MAX_VERTICES)
            m_nMapInfoType = TAB_GEOM_V450_REGION;
        else
            m_nMapInfoType = TAB_GEOM_REGION;

    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRegion: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    /*-----------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *----------------------------------------------------------------*/
    ValidateCoordType(poMapFile);

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABRegion::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABRegion::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                       TABMAPObjHdr *poObjHdr)
{
    return ReadGeometryFromMAPFile(poMapFile, poObjHdr, NULL);
}

int TABRegion::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                       TABMAPObjHdr *poObjHdr,
                                       TABMAPCoordBlock **ppoCoordBlock)
{
    double              dX, dY, dXMin, dYMin, dXMax, dYMax;
    OGRGeometry         *poGeometry;
    OGRLinearRing       *poRing;
    TABMAPCoordBlock    *poCoordBlock = NULL;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poObjHdr->m_nType;

    if (m_nMapInfoType == TAB_GEOM_REGION ||
        m_nMapInfoType == TAB_GEOM_REGION_C ||
        m_nMapInfoType == TAB_GEOM_V450_REGION ||
        m_nMapInfoType == TAB_GEOM_V450_REGION_C )
    {
        /*=============================================================
         * REGION (Similar to PLINE MULTIPLE)
         *============================================================*/
        int     i, iSection;
        GInt32  nCoordBlockPtr, numLineSections, nCenterX, nCenterY;
        GInt32  nCoordDataSize, numPointsTotal, *panXY;
        OGRMultiPolygon         *poMultiPolygon = NULL;
        OGRPolygon              *poPolygon = NULL;
        TABMAPCoordSecHdr       *pasSecHdrs;
        GBool                   bComprCoord = poObjHdr->IsCompressedType();
        GBool bV450 = (m_nMapInfoType == TAB_GEOM_V450_REGION ||
                       m_nMapInfoType == TAB_GEOM_V450_REGION_C);

        /*-------------------------------------------------------------
         * Copy data from poObjHdr
         *------------------------------------------------------------*/
        TABMAPObjPLine *poPLineHdr = (TABMAPObjPLine *)poObjHdr;

        nCoordBlockPtr  = poPLineHdr->m_nCoordBlockPtr;
        nCoordDataSize  = poPLineHdr->m_nCoordDataSize;
        numLineSections = poPLineHdr->m_numLineSections;
        m_bSmooth       = poPLineHdr->m_bSmooth;

        // Centroid/label point
        poMapFile->Int2Coordsys(poPLineHdr->m_nLabelX, poPLineHdr->m_nLabelY, 
                                dX, dY);
        SetCenter(dX, dY);

        // Compressed coordinate origin (useful only in compressed case!)
        nCenterX = poPLineHdr->m_nComprOrgX;
        nCenterY = poPLineHdr->m_nComprOrgY;

        // MBR
        poMapFile->Int2Coordsys(poPLineHdr->m_nMinX, poPLineHdr->m_nMinY, 
                                dXMin, dYMin);
        poMapFile->Int2Coordsys(poPLineHdr->m_nMaxX, poPLineHdr->m_nMaxY, 
                                dXMax, dYMax);

        m_nPenDefIndex = poPLineHdr->m_nPenId;        // Pen index
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);
        m_nBrushDefIndex = poPLineHdr->m_nBrushId;    // Brush index
        poMapFile->ReadBrushDef(m_nBrushDefIndex, &m_sBrushDef);

        /*-------------------------------------------------------------
         * Read data from the coord. block
         *------------------------------------------------------------*/
        pasSecHdrs = (TABMAPCoordSecHdr*)CPLMalloc(numLineSections*
                                                   sizeof(TABMAPCoordSecHdr));

        poCoordBlock = poMapFile->GetCoordBlock(nCoordBlockPtr);
        if (poCoordBlock)
            poCoordBlock->SetComprCoordOrigin(nCenterX, nCenterY);

        if (poCoordBlock == NULL ||
            poCoordBlock->ReadCoordSecHdrs(bComprCoord, bV450, numLineSections,
                                           pasSecHdrs, numPointsTotal) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed reading coordinate data at offset %d", 
                     nCoordBlockPtr);
            CPLFree(pasSecHdrs);
            return -1;
        }

        panXY = (GInt32*)CPLMalloc(numPointsTotal*2*sizeof(GInt32));

        if (poCoordBlock->ReadIntCoords(bComprCoord,numPointsTotal,panXY) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed reading coordinate data at offset %d", 
                     nCoordBlockPtr);
            CPLFree(pasSecHdrs);
            CPLFree(panXY);
            return -1;
        }

        /*-------------------------------------------------------------
         * Decide if we should return an OGRPolygon or an OGRMultiPolygon
         * depending on the number of outer rings found in CoordSecHdr blocks.
         * The CoodSecHdr block for each outer ring in the region has a flag
         * indicating the number of inner rings that follow.
         * In older versions of the format, the count of inner rings was 
         * always zero, so in this case we would always return MultiPolygons.
         *
         * Note: The current implementation assumes that there cannot be
         * holes inside holes (i.e. multiple levels of inner rings)... if
         * that case was encountered then we would return an OGRMultiPolygon
         * in which the topological relationship between the rings would
         * be lost.
         *------------------------------------------------------------*/
        int numOuterRings = 0;
        for(iSection=0; iSection<numLineSections; iSection++)
        {
            // Count this as an outer ring.
            numOuterRings++;
            // Skip inner rings... so loop continues on an outer ring.
            iSection += pasSecHdrs[iSection].numHoles;
        }

        if (numOuterRings > 1)
            poGeometry = poMultiPolygon = new OGRMultiPolygon;
        else
            poGeometry = NULL;  // Will be set later

        /*-------------------------------------------------------------
         * OK, build the OGRGeometry object.
         *------------------------------------------------------------*/
        int numHolesToRead = 0;
        poPolygon = NULL;
        for(iSection=0; iSection<numLineSections; iSection++)
        {
            GInt32 *pnXYPtr;
            int     numSectionVertices;

            if (poPolygon == NULL)
                poPolygon = new OGRPolygon();

            if (numHolesToRead < 1)
                numHolesToRead = pasSecHdrs[iSection].numHoles;
            else
                numHolesToRead--;

            numSectionVertices = pasSecHdrs[iSection].numVertices;
            pnXYPtr = panXY + (pasSecHdrs[iSection].nVertexOffset * 2);

            poRing = new OGRLinearRing();
            poRing->setNumPoints(numSectionVertices);

            for(i=0; i<numSectionVertices; i++)
            {
                poMapFile->Int2Coordsys(*pnXYPtr, *(pnXYPtr+1), dX, dY);
                poRing->setPoint(i, dX, dY);
                pnXYPtr += 2;
            }

            poPolygon->addRingDirectly(poRing);
            poRing = NULL;

            if (numHolesToRead < 1)
            {
                if (numOuterRings > 1)
                {
                    poMultiPolygon->addGeometryDirectly(poPolygon);
                }
                else
                {
                    poGeometry = poPolygon;
                    CPLAssert(iSection == numLineSections-1);
                }

                poPolygon = NULL;  // We'll alloc a new polygon next loop.
            }

        }

        CPLFree(pasSecHdrs);
        CPLFree(panXY);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    SetGeometryDirectly(poGeometry);

    SetMBR(dXMin, dYMin, dXMax, dYMax);

    /* Return a ref to coord block so that caller can continue reading
     * after the end of this object (used by TABCollection)
     */
    if (ppoCoordBlock)
        *ppoCoordBlock = poCoordBlock;

    return 0;
}

/**********************************************************************
 *                   TABRegion::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABRegion::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                      TABMAPObjHdr *poObjHdr)
{
    GInt32              nX, nY;
    OGRGeometry         *poGeom;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();

    if ((m_nMapInfoType == TAB_GEOM_REGION ||
         m_nMapInfoType == TAB_GEOM_REGION_C ||
         m_nMapInfoType == TAB_GEOM_V450_REGION ||
         m_nMapInfoType == TAB_GEOM_V450_REGION_C ) &&
        poGeom && (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
                   wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon))
    {
        /*=============================================================
         * REGIONs are similar to PLINE MULTIPLE
         *
         * We accept both OGRPolygons (with one or multiple rings) and 
         * OGRMultiPolygons as input.
         *============================================================*/
        int     nStatus=0, i, iRing;
        int     numRingsTotal;
        GUInt32 nCoordDataSize;
        GInt32  nCoordBlockPtr;
        TABMAPCoordBlock        *poCoordBlock;
        TABMAPCoordSecHdr       *pasSecHdrs = NULL;
        GBool   bCompressed = poObjHdr->IsCompressedType();

        /*-------------------------------------------------------------
         * Process geometry first...
         *------------------------------------------------------------*/
        poCoordBlock = poMapFile->GetCurCoordBlock();
        poCoordBlock->StartNewFeature();
        nCoordBlockPtr = poCoordBlock->GetCurAddress();
        poCoordBlock->SetComprCoordOrigin(m_nComprOrgX, m_nComprOrgY);

#ifdef TABDUMP
    printf("TABRegion::WriteGeometryToMAPFile(): ComprOrgX,Y= (%d,%d)\n",
           m_nComprOrgX, m_nComprOrgY);
#endif
        /*-------------------------------------------------------------
         * Fetch total number of rings and build array of coord 
         * sections headers.
         *------------------------------------------------------------*/
        numRingsTotal = ComputeNumRings(&pasSecHdrs, poMapFile);
        if (numRingsTotal == 0)
            nStatus = -1;

        /*-------------------------------------------------------------
         * Write the Coord. Section Header
         *------------------------------------------------------------*/
        GBool bV450 = (m_nMapInfoType == TAB_GEOM_V450_REGION ||
                       m_nMapInfoType == TAB_GEOM_V450_REGION_C);
        if (nStatus == 0)
            nStatus = poCoordBlock->WriteCoordSecHdrs(bV450, numRingsTotal, 
                                                      pasSecHdrs, bCompressed);

        CPLFree(pasSecHdrs);
        pasSecHdrs = NULL;

        if (nStatus != 0)
            return nStatus;  // Error has already been reported.

        /*-------------------------------------------------------------
         * Go through all the rings in our OGRMultiPolygon or OGRPolygon
         * to write the coordinates themselves...
         *------------------------------------------------------------*/

        for(iRing=0; iRing < numRingsTotal; iRing++)
        {
            OGRLinearRing       *poRing;

            poRing = GetRingRef(iRing);
            if (poRing == NULL)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABRegion: Object Geometry contains NULL rings!");
                return -1;
            }

            int numPoints = poRing->getNumPoints();
            
            for(i=0; nStatus == 0 && i<numPoints; i++)
            {
                poMapFile->Coordsys2Int(poRing->getX(i), poRing->getY(i),
                                        nX, nY);
                if ((nStatus=poCoordBlock->WriteIntCoord(nX, nY,
                                                         bCompressed)) != 0)
                {
                    // Failed ... error message has already been produced
                    return nStatus;
                }   
            }
        }/* for iRing*/

        nCoordDataSize = poCoordBlock->GetFeatureDataSize();

        /*-------------------------------------------------------------
         * ... and finally copy info to poObjHdr
         *------------------------------------------------------------*/
        TABMAPObjPLine *poPLineHdr = (TABMAPObjPLine *)poObjHdr;

        poPLineHdr->m_nCoordBlockPtr = nCoordBlockPtr;
        poPLineHdr->m_nCoordDataSize = nCoordDataSize;
        poPLineHdr->m_numLineSections = numRingsTotal;

        poPLineHdr->m_bSmooth = m_bSmooth;

        // MBR
        poPLineHdr->SetMBR(m_nXMin, m_nYMin, m_nXMax, m_nYMax);

        // Region center/label point
        double dX, dY;
        if (GetCenter(dX, dY) != -1)
        {
            poMapFile->Coordsys2Int(dX, dY, poPLineHdr->m_nLabelX, 
                                    poPLineHdr->m_nLabelY);
        }
        else
        {
            poPLineHdr->m_nLabelX = m_nComprOrgX;
            poPLineHdr->m_nLabelY = m_nComprOrgY;
        }
        
        // Compressed coordinate origin (useful only in compressed case!)
        poPLineHdr->m_nComprOrgX = m_nComprOrgX;
        poPLineHdr->m_nComprOrgY = m_nComprOrgY;

        m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
        poPLineHdr->m_nPenId = m_nPenDefIndex;      // Pen index

        m_nBrushDefIndex = poMapFile->WriteBrushDef(&m_sBrushDef);
        poPLineHdr->m_nBrushId = m_nBrushDefIndex;  // Brush index
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRegion: Object contains an invalid Geometry!");
        return -1;
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *                   TABRegion::GetNumRings()
 *
 * Return the total number of rings in this object making it look like
 * all parts of the OGRMultiPolygon (or OGRPolygon) are a single collection
 * of rings... hides the complexity of handling OGRMultiPolygons vs 
 * OGRPolygons, etc.
 *
 * Returns 0 if the geometry contained in the object is invalid or missing.
 **********************************************************************/
int TABRegion::GetNumRings()
{
    return ComputeNumRings(NULL, NULL);
}

int TABRegion::ComputeNumRings(TABMAPCoordSecHdr **ppasSecHdrs,
                               TABMAPFile *poMapFile)
{
    OGRGeometry         *poGeom;
    int                 numRingsTotal = 0, iLastSect = 0;

    if (ppasSecHdrs)
        *ppasSecHdrs = NULL;
    poGeom = GetGeometryRef();

    if (poGeom && (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
                   wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon))
    {
        /*-------------------------------------------------------------
         * Calculate total number of rings...
         *------------------------------------------------------------*/
        OGRPolygon      *poPolygon=NULL;
        OGRMultiPolygon *poMultiPolygon = NULL;

        if (wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon)
        {
            poMultiPolygon = (OGRMultiPolygon *)poGeom;
            for(int iPoly=0; iPoly<poMultiPolygon->getNumGeometries(); iPoly++)
            {
                // We are guaranteed that all parts are OGRPolygons
                poPolygon = (OGRPolygon*)poMultiPolygon->getGeometryRef(iPoly);
                if (poPolygon  == NULL)
                    continue;

                numRingsTotal += poPolygon->getNumInteriorRings()+1;

                if (ppasSecHdrs)
                {
                    if (AppendSecHdrs(poPolygon, *ppasSecHdrs, 
                                      poMapFile, iLastSect) != 0)
                        return 0; // An error happened, return count=0
                }

            }/*for*/
        }
        else
        {
            poPolygon = (OGRPolygon*)poGeom;
            numRingsTotal = poPolygon->getNumInteriorRings()+1;

            if (ppasSecHdrs)
            {
                if (AppendSecHdrs(poPolygon, *ppasSecHdrs, 
                                  poMapFile, iLastSect) != 0)
                    return 0;  // An error happened, return count=0
            }
        }
    }

    /*-----------------------------------------------------------------
     * If we're generating section header blocks, then init the 
     * coordinate offset values.
     *
     * In calculation of nDataOffset, we have to take into account that
     * V450 header section uses int32 instead of int16 for numVertices
     * and we add another 2 bytes to align with a 4 bytes boundary.
     *------------------------------------------------------------*/
    int nTotalHdrSizeUncompressed;
    if (m_nMapInfoType == TAB_GEOM_V450_REGION ||
        m_nMapInfoType == TAB_GEOM_V450_REGION_C)
        nTotalHdrSizeUncompressed = 28 * numRingsTotal;
    else
        nTotalHdrSizeUncompressed = 24 * numRingsTotal;

    if (ppasSecHdrs)
    {
        int numPointsTotal = 0;
        CPLAssert(iLastSect == numRingsTotal);
        for (int iRing=0; iRing<numRingsTotal; iRing++)
        {
            (*ppasSecHdrs)[iRing].nDataOffset = nTotalHdrSizeUncompressed +
                                                   numPointsTotal*4*2;
            (*ppasSecHdrs)[iRing].nVertexOffset = numPointsTotal;

            numPointsTotal += (*ppasSecHdrs)[iRing].numVertices;
        }
    }

    return numRingsTotal;
}


/**********************************************************************
 *                   TABRegion::AppendSecHdrs()
 *
 * (Private method)
 *
 * Add a TABMAPCoordSecHdr for each ring in the specified polygon.
 **********************************************************************/
int TABRegion::AppendSecHdrs(OGRPolygon *poPolygon,
                             TABMAPCoordSecHdr * &pasSecHdrs,
                             TABMAPFile *poMapFile,
                             int &iLastRing)
{
    int iRing, numRingsInPolygon;
    /*-------------------------------------------------------------
     * Add a pasSecHdrs[] entry for each ring in this polygon.
     * Note that the structs won't be fully initialized.
     *------------------------------------------------------------*/
    numRingsInPolygon = poPolygon->getNumInteriorRings()+1;

    pasSecHdrs = (TABMAPCoordSecHdr*)CPLRealloc(pasSecHdrs,
                                                (iLastRing+numRingsInPolygon)*
                                                sizeof(TABMAPCoordSecHdr));

    for(iRing=0; iRing < numRingsInPolygon; iRing++)
    {
        OGRLinearRing   *poRing;
        OGREnvelope     sEnvelope;

        if (iRing == 0)
            poRing = poPolygon->getExteriorRing();
        else 
            poRing = poPolygon->getInteriorRing(iRing-1);

        if (poRing == NULL)
        {
            CPLError(CE_Failure, CPLE_AssertionFailed, 
                     "Assertion Failed: Encountered NULL ring in OGRPolygon");
            return -1;
        }

        poRing->getEnvelope(&sEnvelope);
            
        pasSecHdrs[iLastRing].numVertices = poRing->getNumPoints();

        if (iRing == 0)
            pasSecHdrs[iLastRing].numHoles = numRingsInPolygon-1;
        else 
            pasSecHdrs[iLastRing].numHoles = 0;

        poMapFile->Coordsys2Int(sEnvelope.MinX, sEnvelope.MinY,
                                pasSecHdrs[iLastRing].nXMin,
                                pasSecHdrs[iLastRing].nYMin);
        poMapFile->Coordsys2Int(sEnvelope.MaxX, sEnvelope.MaxY,
                                pasSecHdrs[iLastRing].nXMax,
                                pasSecHdrs[iLastRing].nYMax);

        iLastRing++;
    }/* for iRing*/

    return 0;
}

/**********************************************************************
 *                   TABRegion::GetRingRef()
 *
 * Returns a reference to the specified ring number making it look like
 * all parts of the OGRMultiPolygon (or OGRPolygon) are a single collection
 * of rings... hides the complexity of handling OGRMultiPolygons vs 
 * OGRPolygons, etc.
 *
 * Returns NULL if the geometry contained in the object is invalid or 
 * missing or if the specified ring index is invalid.
 **********************************************************************/
OGRLinearRing *TABRegion::GetRingRef(int nRequestedRingIndex)
{
    OGRGeometry     *poGeom;
    OGRLinearRing   *poRing = NULL;

    poGeom = GetGeometryRef();

    if (poGeom && (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
                   wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon))
    {
        /*-------------------------------------------------------------
         * Establish number of polygons based on geometry type
         *------------------------------------------------------------*/
        OGRPolygon      *poPolygon=NULL;
        OGRMultiPolygon *poMultiPolygon = NULL;
        int             iCurRing = 0;
        int             numOGRPolygons = 0;

        if (wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon)
        {
            poMultiPolygon = (OGRMultiPolygon *)poGeom;
            numOGRPolygons = poMultiPolygon->getNumGeometries();
        }
        else
        {
            poPolygon = (OGRPolygon*)poGeom;
            numOGRPolygons = 1;
        }

        /*-------------------------------------------------------------
         * Loop through polygons until we find the requested ring.
         *------------------------------------------------------------*/
        iCurRing = 0;
        for(int iPoly=0; poRing == NULL && iPoly < numOGRPolygons; iPoly++)
        {
            if (poMultiPolygon)
                poPolygon = (OGRPolygon*)poMultiPolygon->getGeometryRef(iPoly);
            else
                poPolygon = (OGRPolygon*)poGeom;

            int numIntRings = poPolygon->getNumInteriorRings();

            if (iCurRing == nRequestedRingIndex)
            {
                poRing = poPolygon->getExteriorRing();
            }
            else if (nRequestedRingIndex > iCurRing &&
                     nRequestedRingIndex-(iCurRing+1) < numIntRings)
           {
                poRing = poPolygon->getInteriorRing(nRequestedRingIndex-
                                                                (iCurRing+1) );
            }
            iCurRing += numIntRings+1;
        }
    }

    return poRing;
}

/**********************************************************************
 *                   TABRegion::RingIsHole()
 *
 * Return false if the requested ring index is the first of a polygon
 **********************************************************************/
GBool TABRegion::IsInteriorRing(int nRequestedRingIndex)
{
    OGRGeometry     *poGeom;
    OGRLinearRing   *poRing = NULL;

    poGeom = GetGeometryRef();

    if (poGeom && (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
                   wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon))
    {
        /*-------------------------------------------------------------
         * Establish number of polygons based on geometry type
         *------------------------------------------------------------*/
        OGRPolygon      *poPolygon=NULL;
        OGRMultiPolygon *poMultiPolygon = NULL;
        int             iCurRing = 0;
        int             numOGRPolygons = 0;

        if (wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon)
        {
            poMultiPolygon = (OGRMultiPolygon *)poGeom;
            numOGRPolygons = poMultiPolygon->getNumGeometries();
        }
        else
        {
            poPolygon = (OGRPolygon*)poGeom;
            numOGRPolygons = 1;
        }

        /*-------------------------------------------------------------
         * Loop through polygons until we find the requested ring.
         *------------------------------------------------------------*/
        iCurRing = 0;
        for(int iPoly=0; poRing == NULL && iPoly < numOGRPolygons; iPoly++)
        {
            if (poMultiPolygon)
                poPolygon = (OGRPolygon*)poMultiPolygon->getGeometryRef(iPoly);
            else
                poPolygon = (OGRPolygon*)poGeom;

            int numIntRings = poPolygon->getNumInteriorRings();

            if (iCurRing == nRequestedRingIndex)
            {
                return FALSE;
            }
            else if (nRequestedRingIndex > iCurRing &&
                     nRequestedRingIndex-(iCurRing+1) < numIntRings)
           {
                return TRUE;
            }
            iCurRing += numIntRings+1;
        }
    }

    return FALSE;
}

/**********************************************************************
 *                   TABRegion::GetStyleString()
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABRegion::GetStyleString()
{
    if (m_pszStyleString == NULL)
    {
        // Since GetPen/BrushStyleString() use CPLSPrintf(), we need 
        // to use temporary buffers
        char *pszPen = CPLStrdup(GetPenStyleString());
        char *pszBrush = CPLStrdup(GetBrushStyleString());

        m_pszStyleString = CPLStrdup(CPLSPrintf("%s;%s", pszBrush, pszPen));

        CPLFree(pszPen);
        CPLFree(pszBrush);
    }

    return m_pszStyleString;
}



/**********************************************************************
 *                   TABRegion::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF REGIONs.
 **********************************************************************/
void TABRegion::DumpMIF(FILE *fpOut /*=NULL*/)
{
    OGRGeometry   *poGeom;
    int i, numPoints;

    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
                   wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon))
    {
        /*-------------------------------------------------------------
         * Generate output for region
         *
         * Note that we want to handle both OGRPolygons and OGRMultiPolygons
         * that's why we use the GetNumRings()/GetRingRef() interface.
         *------------------------------------------------------------*/
        int iRing, numRingsTotal = GetNumRings();

        fprintf(fpOut, "REGION %d\n", numRingsTotal);

        for(iRing=0; iRing < numRingsTotal; iRing++)
        {
            OGRLinearRing       *poRing;

            poRing = GetRingRef(iRing);

            if (poRing == NULL)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABRegion: Object Geometry contains NULL rings!");
                return;
            }

            numPoints = poRing->getNumPoints();
            fprintf(fpOut, " %d\n", numPoints);
            for(i=0; i<numPoints; i++)
                fprintf(fpOut, "%.15g %.15g\n",poRing->getX(i),poRing->getY(i));
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRegion: Missing or Invalid Geometry!");
        return;
    }

    if (m_bCenterIsSet)
        fprintf(fpOut, "Center %.15g %.15g\n", m_dCenterX, m_dCenterY);

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();
    DumpBrushDef();

    fflush(fpOut);
}

/**********************************************************************
 *                   TABRegion::GetCenter()
 *
 * Returns the center/label point of the region.  
 * Compute one using OGRPolygonLabelPoint() if it was not explicitly set 
 * before.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABRegion::GetCenter(double &dX, double &dY)
{
    if (!m_bCenterIsSet)
    {
        /*-------------------------------------------------------------
         * Calculate label point.  If we have a multipolygon then we use 
         * the first OGRPolygon in the feature to calculate the point.
         *------------------------------------------------------------*/
        OGRPoint        oLabelPoint;
        OGRPolygon      *poPolygon=NULL;
        OGRGeometry     *poGeom;

        poGeom = GetGeometryRef();
        if (poGeom == NULL)
            return -1;

        if (wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon)
        {
            OGRMultiPolygon *poMultiPolygon = (OGRMultiPolygon *)poGeom;
            if (poMultiPolygon->getNumGeometries() > 0)
                poPolygon = (OGRPolygon*)poMultiPolygon->getGeometryRef(0);
        }
        else if (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon)
        {
            poPolygon = (OGRPolygon*)poGeom;
        }

        if (poPolygon != NULL &&
            OGRPolygonLabelPoint(poPolygon, &oLabelPoint) == OGRERR_NONE)
        {
            m_dCenterX = oLabelPoint.getX();
            m_dCenterY = oLabelPoint.getY();
        }
        else
        {
            OGREnvelope oEnv;
            poGeom->getEnvelope(&oEnv);
            m_dCenterX = (oEnv.MaxX + oEnv.MinX)/2.0;
            m_dCenterY = (oEnv.MaxY + oEnv.MinY)/2.0;
        }

        m_bCenterIsSet = TRUE;
    }

    if (!m_bCenterIsSet)
        return -1;

    dX = m_dCenterX;
    dY = m_dCenterY;
    return 0;
}

/**********************************************************************
 *                   TABRegion::SetCenter()
 *
 * Set the X,Y coordinates to use as center/label point for the region.
 **********************************************************************/
void TABRegion::SetCenter(double dX, double dY)
{
    m_dCenterX = dX;
    m_dCenterY = dY;
    m_bCenterIsSet = TRUE;
}


/*=====================================================================
 *                      class TABRectangle
 *====================================================================*/

/**********************************************************************
 *                   TABRectangle::TABRectangle()
 *
 * Constructor.
 **********************************************************************/
TABRectangle::TABRectangle(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
    m_bRoundCorners = FALSE;
    m_dRoundXRadius = m_dRoundYRadius = 0.0;
}

/**********************************************************************
 *                   TABRectangle::~TABRectangle()
 *
 * Destructor.
 **********************************************************************/
TABRectangle::~TABRectangle()
{
}

/**********************************************************************
 *                     TABRectangle::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CopyTABFeatureBase() and 
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABRectangle::CloneTABFeature(OGRFeatureDefn *poNewDefn/*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABRectangle *poNew = new TABRectangle(poNewDefn ? poNewDefn : 
                                                       GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeaturePen
    *(poNew->GetPenDefRef()) = *GetPenDefRef();

    // ITABFeatureBrush
    *(poNew->GetBrushDefRef()) = *GetBrushDefRef();

    poNew->m_bRoundCorners = m_bRoundCorners;
    poNew->m_dRoundXRadius = m_dRoundXRadius;
    poNew->m_dRoundYRadius = m_dRoundYRadius;

    return poNew;
}

/**********************************************************************
 *                   TABRectangle::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
int  TABRectangle::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    OGRGeometry *poGeom;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon)
    {
        if (m_bRoundCorners && m_dRoundXRadius!=0.0 && m_dRoundYRadius!=0.0)
            m_nMapInfoType = TAB_GEOM_ROUNDRECT;
        else
            m_nMapInfoType = TAB_GEOM_RECT;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRectangle: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    /*-----------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *----------------------------------------------------------------*/
    // __TODO__ For now we always write uncompressed for this class...
    // ValidateCoordType(poMapFile);

    return m_nMapInfoType;
}


/**********************************************************************
 *                   TABRectangle::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABRectangle::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                          TABMAPObjHdr *poObjHdr)
{
    GInt32              nX, nY;
    double              dXMin, dYMin, dXMax, dYMax;
    OGRPolygon          *poPolygon;
    OGRLinearRing       *poRing;
    TABMAPObjectBlock   *poObjBlock;
    GBool               bComprCoord;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();

    bComprCoord = (m_nMapInfoType == TAB_GEOM_RECT_C ||
                   m_nMapInfoType == TAB_GEOM_ROUNDRECT_C );

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    if (m_nMapInfoType == TAB_GEOM_RECT ||
        m_nMapInfoType == TAB_GEOM_RECT_C ||
        m_nMapInfoType == TAB_GEOM_ROUNDRECT ||
        m_nMapInfoType == TAB_GEOM_ROUNDRECT_C)
    {

        // Read the corners radius

        if (m_nMapInfoType == TAB_GEOM_ROUNDRECT ||
            m_nMapInfoType == TAB_GEOM_ROUNDRECT_C)
        {
            // Read the corner's diameters
            nX = bComprCoord? poObjBlock->ReadInt16():poObjBlock->ReadInt32();
            nY = bComprCoord? poObjBlock->ReadInt16():poObjBlock->ReadInt32();
            poMapFile->Int2CoordsysDist(nX, nY, 
                                        m_dRoundXRadius, m_dRoundYRadius);
            // Divide by 2 since we store the corner's radius
            m_dRoundXRadius /= 2.0;
            m_dRoundYRadius /= 2.0;

            m_bRoundCorners = TRUE;
        }
        else
        {
            m_bRoundCorners = FALSE;
            m_dRoundXRadius = m_dRoundYRadius = 0.0;
        }

        // A rectangle is defined by its MBR

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMin, dYMin);
        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMax, dYMax);

        m_nPenDefIndex = poObjBlock->ReadByte();      // Pen index
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);
        m_nBrushDefIndex = poObjBlock->ReadByte();    // Brush index
        poMapFile->ReadBrushDef(m_nBrushDefIndex, &m_sBrushDef);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Call SetMBR() and GetMBR() now to make sure that min values are
     * really smaller than max values.
     *----------------------------------------------------------------*/
    SetMBR(dXMin, dYMin, dXMax, dYMax);
    GetMBR(dXMin, dYMin, dXMax, dYMax);

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/
    poPolygon = new OGRPolygon;
    poRing = new OGRLinearRing();
    if (m_bRoundCorners && m_dRoundXRadius != 0.0 && m_dRoundYRadius != 0.0)
    {
        /*-------------------------------------------------------------
         * For rounded rectangles, we generate arcs with 45 line
         * segments for each corner.  We start with lower-left corner 
         * and proceed counterclockwise
         * We also have to make sure that rounding radius is not too
         * large for the MBR in the generated polygon... however, we 
         * always return the true X/Y radius (not adjusted) since this
         * is the way MapInfo seems to do it when a radius bigger than
         * the MBR is passed from TBA to MIF.
         *------------------------------------------------------------*/
        double dXRadius = MIN(m_dRoundXRadius, (dXMax-dXMin)/2.0);
        double dYRadius = MIN(m_dRoundYRadius, (dYMax-dYMin)/2.0);
        TABGenerateArc(poRing, 45, 
                       dXMin + dXRadius, dYMin + dYRadius, dXRadius, dYRadius,
                       PI, 3.0*PI/2.0);
        TABGenerateArc(poRing, 45, 
                       dXMax - dXRadius, dYMin + dYRadius, dXRadius, dYRadius,
                       3.0*PI/2.0, 2.0*PI);
        TABGenerateArc(poRing, 45, 
                       dXMax - dXRadius, dYMax - dYRadius, dXRadius, dYRadius,
                       0.0, PI/2.0);
        TABGenerateArc(poRing, 45, 
                       dXMin + dXRadius, dYMax - dYRadius, dXRadius, dYRadius,
                       PI/2.0, PI);
                       
        TABCloseRing(poRing);
    }
    else
    {
        poRing->addPoint(dXMin, dYMin);
        poRing->addPoint(dXMax, dYMin);
        poRing->addPoint(dXMax, dYMax);
        poRing->addPoint(dXMin, dYMax);
        poRing->addPoint(dXMin, dYMin);
    }

    poPolygon->addRingDirectly(poRing);
    SetGeometryDirectly(poPolygon);

    return 0;
}

/**********************************************************************
 *                   TABRectangle::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABRectangle::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                       TABMAPObjHdr *poObjHdr)
{
    OGRGeometry         *poGeom;
    OGRPolygon          *poPolygon;
    OGREnvelope         sEnvelope;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon)
        poPolygon = (OGRPolygon*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRectangle: Missing or Invalid Geometry!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Note that we will simply use the rectangle's MBR and don't really 
     * read the polygon geometry... this should be OK unless the 
     * polygon geometry was not really a rectangle.
     *----------------------------------------------------------------*/
    poPolygon->getEnvelope(&sEnvelope);

    /*-----------------------------------------------------------------
     * Copy object information
     *----------------------------------------------------------------*/
    TABMAPObjRectEllipse *poRectHdr = (TABMAPObjRectEllipse *)poObjHdr;

    if (m_nMapInfoType == TAB_GEOM_ROUNDRECT || 
        m_nMapInfoType == TAB_GEOM_ROUNDRECT_C)
    {
        poMapFile->Coordsys2IntDist(m_dRoundXRadius*2.0, m_dRoundYRadius*2.0,
                                    poRectHdr->m_nCornerWidth,
                                    poRectHdr->m_nCornerHeight);
    }
    else
    {
        poRectHdr->m_nCornerWidth = poRectHdr->m_nCornerHeight = 0;
    }

    // A rectangle is defined by its MBR
    poMapFile->Coordsys2Int(sEnvelope.MinX, sEnvelope.MinY, 
                            poRectHdr->m_nMinX, poRectHdr->m_nMinY);
    poMapFile->Coordsys2Int(sEnvelope.MaxX, sEnvelope.MaxY, 
                            poRectHdr->m_nMaxX, poRectHdr->m_nMaxY);

    m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
    poRectHdr->m_nPenId = m_nPenDefIndex;      // Pen index

    m_nBrushDefIndex = poMapFile->WriteBrushDef(&m_sBrushDef);
    poRectHdr->m_nBrushId = m_nBrushDefIndex;      // Brush index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABRectangle::GetStyleString()
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABRectangle::GetStyleString()
{
    if (m_pszStyleString == NULL)
    {
        // Since GetPen/BrushStyleString() use CPLSPrintf(), we need 
        // to use temporary buffers
        char *pszPen = CPLStrdup(GetPenStyleString());
        char *pszBrush = CPLStrdup(GetBrushStyleString());

        m_pszStyleString = CPLStrdup(CPLSPrintf("%s;%s", pszBrush, pszPen));

        CPLFree(pszPen);
        CPLFree(pszBrush);
    }

    return m_pszStyleString;
}

/**********************************************************************
 *                   TABRectangle::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF REGIONs.
 **********************************************************************/
void TABRectangle::DumpMIF(FILE *fpOut /*=NULL*/)
{
    OGRGeometry   *poGeom;
    OGRPolygon    *poPolygon = NULL;
    int i, numPoints;

    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Output RECT or ROUNDRECT parameters
     *----------------------------------------------------------------*/
    double dXMin, dYMin, dXMax, dYMax;
    GetMBR(dXMin, dYMin, dXMax, dYMax);
    if (m_bRoundCorners)
        fprintf(fpOut, "(ROUNDRECT %.15g %.15g %.15g %.15g    %.15g %.15g)\n", 
                dXMin, dYMin, dXMax, dYMax, 
                m_dRoundXRadius, m_dRoundYRadius);
    else
        fprintf(fpOut, "(RECT %.15g %.15g %.15g %.15g)\n", dXMin, dYMin, dXMax, dYMax);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon)
    {
        /*-------------------------------------------------------------
         * Generate rectangle output as a region
         * We could also output as a RECT or ROUNDRECT in a real MIF generator
         *------------------------------------------------------------*/
        int iRing, numIntRings;
        poPolygon = (OGRPolygon*)poGeom;
        numIntRings = poPolygon->getNumInteriorRings();
        fprintf(fpOut, "REGION %d\n", numIntRings+1);
        // In this loop, iRing=-1 for the outer ring.
        for(iRing=-1; iRing < numIntRings; iRing++)
        {
            OGRLinearRing       *poRing;

            if (iRing == -1)
                poRing = poPolygon->getExteriorRing();
            else
                poRing = poPolygon->getInteriorRing(iRing);

            if (poRing == NULL)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABRectangle: Object Geometry contains NULL rings!");
                return;
            }

            numPoints = poRing->getNumPoints();
            fprintf(fpOut, " %d\n", numPoints);
            for(i=0; i<numPoints; i++)
                fprintf(fpOut, "%.15g %.15g\n",poRing->getX(i),poRing->getY(i));
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRectangle: Missing or Invalid Geometry!");
        return;
    }

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();
    DumpBrushDef();

    fflush(fpOut);
}


/*=====================================================================
 *                      class TABEllipse
 *====================================================================*/

/**********************************************************************
 *                   TABEllipse::TABEllipse()
 *
 * Constructor.
 **********************************************************************/
TABEllipse::TABEllipse(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
}

/**********************************************************************
 *                   TABEllipse::~TABEllipse()
 *
 * Destructor.
 **********************************************************************/
TABEllipse::~TABEllipse()
{
}

/**********************************************************************
 *                     TABEllipse::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CopyTABFeatureBase() and 
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABEllipse::CloneTABFeature(OGRFeatureDefn *poNewDefn/*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABEllipse *poNew = new TABEllipse(poNewDefn ? poNewDefn : 
                                                   GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeaturePen
    *(poNew->GetPenDefRef()) = *GetPenDefRef();

    // ITABFeatureBrush
    *(poNew->GetBrushDefRef()) = *GetBrushDefRef();

    poNew->m_dCenterX = m_dCenterX;
    poNew->m_dCenterY = m_dCenterY;
    poNew->m_dXRadius = m_dXRadius;
    poNew->m_dYRadius = m_dYRadius;

    return poNew;
}

/**********************************************************************
 *                   TABEllipse::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
int  TABEllipse::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    OGRGeometry *poGeom;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if ( (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ) ||
         (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint ) )
    {
        m_nMapInfoType = TAB_GEOM_ELLIPSE;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABEllipse: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    /*-----------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *----------------------------------------------------------------*/
    // __TODO__ For now we always write uncompressed for this class...
    // ValidateCoordType(poMapFile);

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABEllipse::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABEllipse::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                        TABMAPObjHdr *poObjHdr)
{
    GInt32              nX, nY;
    double              dXMin, dYMin, dXMax, dYMax;
    OGRPolygon          *poPolygon;
    OGRLinearRing       *poRing;
    TABMAPObjectBlock   *poObjBlock;
    GBool               bComprCoord;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();

    bComprCoord = (m_nMapInfoType == TAB_GEOM_ELLIPSE_C );

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    if (m_nMapInfoType == TAB_GEOM_ELLIPSE ||
        m_nMapInfoType == TAB_GEOM_ELLIPSE_C )
    {

        // An ellipse is defined by its MBR

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMin, dYMin);
        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMax, dYMax);

        m_nPenDefIndex = poObjBlock->ReadByte();      // Pen index
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);
        m_nBrushDefIndex = poObjBlock->ReadByte();    // Brush index
        poMapFile->ReadBrushDef(m_nBrushDefIndex, &m_sBrushDef);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Save info about the ellipse def. inside class members
     *----------------------------------------------------------------*/
    m_dCenterX = (dXMin + dXMax) / 2.0;
    m_dCenterY = (dYMin + dYMax) / 2.0;
    m_dXRadius = ABS( (dXMax - dXMin) / 2.0 );
    m_dYRadius = ABS( (dYMax - dYMin) / 2.0 );

    SetMBR(dXMin, dYMin, dXMax, dYMax);

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/
    poPolygon = new OGRPolygon;
    poRing = new OGRLinearRing();


    /*-----------------------------------------------------------------
     * For the OGR geometry, we generate an ellipse with 2 degrees line
     * segments.
     *----------------------------------------------------------------*/
    TABGenerateArc(poRing, 180, 
                   m_dCenterX, m_dCenterY,
                   m_dXRadius, m_dYRadius,
                   0.0, 2.0*PI);
    TABCloseRing(poRing);

    poPolygon->addRingDirectly(poRing);
    SetGeometryDirectly(poPolygon);

    return 0;
}

/**********************************************************************
 *                   TABEllipse::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABEllipse::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                       TABMAPObjHdr *poObjHdr)
{
    TABMAPObjectBlock   *poObjBlock;
    OGRGeometry         *poGeom;
    OGREnvelope         sEnvelope;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    poObjBlock = poMapFile->GetCurObjBlock();

    /*-----------------------------------------------------------------
     * Fetch and validate geometry... Polygon and point are accepted.
     * Note that we will simply use the ellipse's MBR and don't really 
     * read the polygon geometry... this should be OK unless the 
     * polygon geometry was not really an ellipse.
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if ( (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ) ||
         (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint )  )
        poGeom->getEnvelope(&sEnvelope);
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABEllipse: Missing or Invalid Geometry!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Copy object information
     *
     * We use the center of the MBR as the ellipse center, and the 
     * X/Y radius to define the MBR size.  If X/Y radius are null then
     * we'll try to use the MBR to recompute them.
     *----------------------------------------------------------------*/
    TABMAPObjRectEllipse *poRectHdr = (TABMAPObjRectEllipse *)poObjHdr;

    // Reset RoundRect Corner members... just in case (unused for ellipse)
    poRectHdr->m_nCornerWidth = poRectHdr->m_nCornerHeight = 0;

    // An ellipse is defined by its MBR
    double      dXCenter, dYCenter;
    dXCenter = (sEnvelope.MaxX + sEnvelope.MinX)/2.0;
    dYCenter = (sEnvelope.MaxY + sEnvelope.MinY)/2.0;
    if (m_dXRadius == 0.0 && m_dYRadius == 0.0)
    {
        m_dXRadius = ABS(sEnvelope.MaxX - sEnvelope.MinX) / 2.0;
        m_dYRadius = ABS(sEnvelope.MaxY - sEnvelope.MinY);
    }

    poMapFile->Coordsys2Int(dXCenter - m_dXRadius, dYCenter - m_dYRadius,
                            poRectHdr->m_nMinX, poRectHdr->m_nMinY);
    poMapFile->Coordsys2Int(dXCenter + m_dXRadius, dYCenter + m_dYRadius,
                            poRectHdr->m_nMaxX, poRectHdr->m_nMaxY);

    m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
    poRectHdr->m_nPenId = m_nPenDefIndex;      // Pen index

    m_nBrushDefIndex = poMapFile->WriteBrushDef(&m_sBrushDef);
    poRectHdr->m_nBrushId = m_nBrushDefIndex;      // Brush index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABEllipse::GetStyleString()
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABEllipse::GetStyleString()
{
    if (m_pszStyleString == NULL)
    {
        // Since GetPen/BrushStyleString() use CPLSPrintf(), we need 
        // to use temporary buffers
        char *pszPen = CPLStrdup(GetPenStyleString());
        char *pszBrush = CPLStrdup(GetBrushStyleString());

        m_pszStyleString = CPLStrdup(CPLSPrintf("%s;%s", pszBrush, pszPen));

        CPLFree(pszPen);
        CPLFree(pszBrush);
    }

    return m_pszStyleString;
}


/**********************************************************************
 *                   TABEllipse::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF REGIONs.
 **********************************************************************/
void TABEllipse::DumpMIF(FILE *fpOut /*=NULL*/)
{
    OGRGeometry   *poGeom;
    OGRPolygon    *poPolygon = NULL;
    int i, numPoints;

    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Output ELLIPSE parameters
     *----------------------------------------------------------------*/
    double dXMin, dYMin, dXMax, dYMax;
    GetMBR(dXMin, dYMin, dXMax, dYMax);
    fprintf(fpOut, "(ELLIPSE %.15g %.15g %.15g %.15g)\n", dXMin, dYMin, dXMax, dYMax);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon)
    {
        /*-------------------------------------------------------------
         * Generate ellipse output as a region
         * We could also output as an ELLIPSE in a real MIF generator
         *------------------------------------------------------------*/
        int iRing, numIntRings;
        poPolygon = (OGRPolygon*)poGeom;
        numIntRings = poPolygon->getNumInteriorRings();
        fprintf(fpOut, "REGION %d\n", numIntRings+1);
        // In this loop, iRing=-1 for the outer ring.
        for(iRing=-1; iRing < numIntRings; iRing++)
        {
            OGRLinearRing       *poRing;

            if (iRing == -1)
                poRing = poPolygon->getExteriorRing();
            else
                poRing = poPolygon->getInteriorRing(iRing);

            if (poRing == NULL)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABEllipse: Object Geometry contains NULL rings!");
                return;
            }

            numPoints = poRing->getNumPoints();
            fprintf(fpOut, " %d\n", numPoints);
            for(i=0; i<numPoints; i++)
                fprintf(fpOut, "%.15g %.15g\n",poRing->getX(i),poRing->getY(i));
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABEllipse: Missing or Invalid Geometry!");
        return;
    }

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();
    DumpBrushDef();

    fflush(fpOut);
}


/*=====================================================================
 *                      class TABArc
 *====================================================================*/

/**********************************************************************
 *                   TABArc::TABArc()
 *
 * Constructor.
 **********************************************************************/
TABArc::TABArc(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
    m_dStartAngle = m_dEndAngle = 0.0;
    m_dCenterX = m_dCenterY = m_dXRadius = m_dYRadius = 0.0;

}

/**********************************************************************
 *                   TABArc::~TABArc()
 *
 * Destructor.
 **********************************************************************/
TABArc::~TABArc()
{
}

/**********************************************************************
 *                     TABArc::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CopyTABFeatureBase() and 
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABArc::CloneTABFeature(OGRFeatureDefn *poNewDefn/*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABArc *poNew = new TABArc(poNewDefn ? poNewDefn : GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeaturePen
    *(poNew->GetPenDefRef()) = *GetPenDefRef();

    poNew->SetStartAngle( GetStartAngle() );
    poNew->SetEndAngle( GetEndAngle() );

    poNew->m_dCenterX = m_dCenterX;
    poNew->m_dCenterY = m_dCenterY;
    poNew->m_dXRadius = m_dXRadius;
    poNew->m_dYRadius = m_dYRadius;

    return poNew;
}

/**********************************************************************
 *                   TABArc::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
int  TABArc::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    OGRGeometry *poGeom;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if ( (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString ) ||
         (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint ) )
    {
        m_nMapInfoType = TAB_GEOM_ARC;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABArc: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    /*-----------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *----------------------------------------------------------------*/
    // __TODO__ For now we always write uncompressed for this class...
    // ValidateCoordType(poMapFile);

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABArc::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABArc::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                    TABMAPObjHdr *poObjHdr)
{
    GInt32              nX, nY;
    double              dXMin, dYMin, dXMax, dYMax;
    OGRLineString       *poLine;
    TABMAPObjectBlock   *poObjBlock;
    GBool               bComprCoord;
    int                 numPts;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();

    bComprCoord = (m_nMapInfoType == TAB_GEOM_ARC_C );

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    if (m_nMapInfoType == TAB_GEOM_ARC ||
        m_nMapInfoType == TAB_GEOM_ARC_C )
    {
        /*-------------------------------------------------------------
         * Start/End angles
         * Since the angles are specified for integer coordinates, and
         * that these coordinates can have the X axis reversed, we have to
         * adjust the angle values for the change in the X axis
         * direction.
         *
         * This should be necessary only when X axis is flipped.
         * __TODO__ Why is order of start/end values reversed as well???
         *------------------------------------------------------------*/

        /*-------------------------------------------------------------
         * OK, Arc angles again!!!!!!!!!!!!
         * After some tests in 1999-11, it appeared that the angle values
         * ALWAYS had to be flipped (read order= end angle followed by 
         * start angle), no matter which quadrant the file is in.
         * This does not make any sense, so I suspect that there is something
         * that we are missing here!
         *
         * 2000-01-14.... Again!!!  Based on some sample data files:
         *  File         Ver Quadr  ReflXAxis  Read_Order   Adjust_Angle
         * test_symb.tab 300    2        1      end,start    X=yes Y=no
         * alltypes.tab: 300    1        0      start,end    X=no  Y=no
         * arcs.tab:     300    2        0      end,start    X=yes Y=no
         *
         * Until we prove it wrong, the rule would be:
         *  -> Quadrant 1 and 3, angles order = start, end
         *  -> Quadrant 2 and 4, angles order = end, start
         * + Always adjust angles for x and y axis based on quadrant.
         *
         * This was confirmed using some more files in which the quadrant was 
         * manually changed, but whether these are valid results is 
         * discutable.
         *
         * The ReflectXAxis flag seems to have no effect here...
         *------------------------------------------------------------*/

        /*-------------------------------------------------------------
         * In version 100 .tab files (version 400 .map), it is possible
         * to have a quadrant value of 0 and it should be treated the 
         * same way as quadrant 3
         *------------------------------------------------------------*/
        if ( poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==1 ||
             poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==3 ||
             poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==0  )
        {
            // Quadrants 1 and 3 ... read order = start, end
            m_dStartAngle = poObjBlock->ReadInt16()/10.0;
            m_dEndAngle = poObjBlock->ReadInt16()/10.0;
        }
        else
        {
            // Quadrants 2 and 4 ... read order = end, start
            m_dEndAngle = poObjBlock->ReadInt16()/10.0;
            m_dStartAngle = poObjBlock->ReadInt16()/10.0;
        }

        if ( poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==2 ||
             poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==3 ||
             poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==0 )
        {
            // X axis direction is flipped... adjust angle
            m_dStartAngle = (m_dStartAngle<=180.0) ? (180.0-m_dStartAngle):
                                                 (540.0-m_dStartAngle);
            m_dEndAngle   = (m_dEndAngle<=180.0) ? (180.0-m_dEndAngle):
                                               (540.0-m_dEndAngle);
        }

        if (poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==3 ||
            poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==4 ||
            poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==0 )
        {
            // Y axis direction is flipped... this reverses angle direction
            // Unfortunately we never found any file that contains this case,
            // but this should be the behavior to expect!!!
            //
            // 2000-01-14: some files in which quadrant was set to 3 and 4
            // manually seemed to confirm that this is the right thing to do.
            m_dStartAngle = 360.0 - m_dStartAngle;
            m_dEndAngle = 360.0 - m_dEndAngle;
        }

        // An arc is defined by its defining ellipse's MBR:

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMin, dYMin);
        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMax, dYMax);

        m_dCenterX = (dXMin + dXMax) / 2.0;
        m_dCenterY = (dYMin + dYMax) / 2.0;
        m_dXRadius = ABS( (dXMax - dXMin) / 2.0 );
        m_dYRadius = ABS( (dYMax - dYMin) / 2.0 );

        // Read the Arc's MBR and use that as this feature's MBR

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMin, dYMin);
        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMax, dYMax);

        SetMBR(dXMin, dYMin, dXMax, dYMax);

        m_nPenDefIndex = poObjBlock->ReadByte();      // Pen index
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);


    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     * For the OGR geometry, we generate an arc with 2 degrees line
     * segments.
     *----------------------------------------------------------------*/
    poLine = new OGRLineString;

    if (m_dEndAngle < m_dStartAngle)
        numPts = (int) ABS( ((m_dEndAngle+360.0)-m_dStartAngle)/2.0 ) + 1;
    else
        numPts = (int) ABS( (m_dEndAngle-m_dStartAngle)/2.0 ) + 1;
    numPts = MAX(2, numPts);

    TABGenerateArc(poLine, numPts,
                   m_dCenterX, m_dCenterY,
                   m_dXRadius, m_dYRadius,
                   m_dStartAngle*PI/180.0, m_dEndAngle*PI/180.0);

    SetGeometryDirectly(poLine);

    return 0;
}

/**********************************************************************
 *                   TABArc::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABArc::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                       TABMAPObjHdr *poObjHdr)
{
    OGRGeometry         *poGeom;
    OGREnvelope         sEnvelope;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if ( (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString ) )
    {
        /*-------------------------------------------------------------
         * POLYGON geometry:
         * Note that we will simply use the ellipse's MBR and don't really 
         * read the polygon geometry... this should be OK unless the 
         * polygon geometry was not really an ellipse.
         * In the case of a polygon geometry. the m_dCenterX/Y values MUST
         * have been set by the caller.
         *------------------------------------------------------------*/
        poGeom->getEnvelope(&sEnvelope);
    }
    else if ( (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint ) ) 
    {
        /*-------------------------------------------------------------
         * In the case of a POINT GEOMETRY, we will make sure the the 
         * feature's m_dCenterX/Y are in sync with the point's X,Y coords.
         *
         * In this case we have to reconstruct the arc inside a temporary
         * geometry object in order to find its real MBR.
         *------------------------------------------------------------*/
        OGRPoint *poPoint = (OGRPoint *)poGeom;
        m_dCenterX = poPoint->getX();
        m_dCenterY = poPoint->getY();

        OGRLineString oTmpLine;
        int numPts=0;
        if (m_dEndAngle < m_dStartAngle)
            numPts = (int) ABS( ((m_dEndAngle+360)-m_dStartAngle)/2 ) + 1;
        else
            numPts = (int) ABS( (m_dEndAngle-m_dStartAngle)/2 ) + 1;
        numPts = MAX(2, numPts);

        TABGenerateArc(&oTmpLine, numPts,
                       m_dCenterX, m_dCenterY,
                       m_dXRadius, m_dYRadius,
                       m_dStartAngle*PI/180.0, m_dEndAngle*PI/180.0);

        oTmpLine.getEnvelope(&sEnvelope);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABArc: Missing or Invalid Geometry!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Copy object information
     *----------------------------------------------------------------*/
    TABMAPObjArc *poArcHdr = (TABMAPObjArc *)poObjHdr;

    /*-------------------------------------------------------------
     * Start/End angles
     * Since we ALWAYS produce files in quadrant 1 then we can
     * ignore the special angle conversion required by flipped axis.
     *
     * See the notes about Arc angles in TABArc::ReadGeometryFromMAPFile()
     *------------------------------------------------------------*/
    CPLAssert(poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant == 1);

    poArcHdr->m_nStartAngle = ROUND_INT(m_dStartAngle*10.0);
    poArcHdr->m_nEndAngle = ROUND_INT(m_dEndAngle*10.0);
    
    // An arc is defined by its defining ellipse's MBR:
    poMapFile->Coordsys2Int(m_dCenterX-m_dXRadius, m_dCenterY-m_dYRadius,
                            poArcHdr->m_nArcEllipseMinX, 
                            poArcHdr->m_nArcEllipseMinY);
    poMapFile->Coordsys2Int(m_dCenterX+m_dXRadius, m_dCenterY+m_dYRadius,
                            poArcHdr->m_nArcEllipseMaxX, 
                            poArcHdr->m_nArcEllipseMaxY);

    // Write the Arc's actual MBR
    poMapFile->Coordsys2Int(sEnvelope.MinX, sEnvelope.MinY, 
                            poArcHdr->m_nMinX, poArcHdr->m_nMinY);
    poMapFile->Coordsys2Int(sEnvelope.MaxX, sEnvelope.MaxY, 
                            poArcHdr->m_nMaxX, poArcHdr->m_nMaxY);

    m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
    poArcHdr->m_nPenId = m_nPenDefIndex;      // Pen index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABArc::SetStart/EndAngle()
 *
 * Set the start/end angle values in degrees, making sure the values are
 * always in the range [0..360]
 **********************************************************************/
void TABArc::SetStartAngle(double dAngle)
{
    while(dAngle < 0.0)   dAngle += 360.0;
    while(dAngle > 360.0) dAngle -= 360.0;

    m_dStartAngle = dAngle;
}

void TABArc::SetEndAngle(double dAngle)
{
    while(dAngle < 0.0)   dAngle += 360.0;
    while(dAngle > 360.0) dAngle -= 360.0;

    m_dEndAngle = dAngle;
}


/**********************************************************************
 *                   TABArc::GetStyleString()
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABArc::GetStyleString()
{
    if (m_pszStyleString == NULL)
    {
        m_pszStyleString = CPLStrdup(GetPenStyleString());
    }

    return m_pszStyleString;
}

/**********************************************************************
 *                   TABArc::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF REGIONs.
 **********************************************************************/
void TABArc::DumpMIF(FILE *fpOut /*=NULL*/)
{
    OGRGeometry   *poGeom;
    OGRLineString *poLine = NULL;
    int i, numPoints;

    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Output ARC parameters
     *----------------------------------------------------------------*/
    fprintf(fpOut, "(ARC %.15g %.15g %.15g %.15g   %d %d)\n",
            m_dCenterX - m_dXRadius, m_dCenterY - m_dYRadius,
            m_dCenterX + m_dXRadius, m_dCenterY + m_dYRadius,
            (int)m_dStartAngle, (int)m_dEndAngle);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
    {
        /*-------------------------------------------------------------
         * Generate arc output as a simple polyline
         * We could also output as an ELLIPSE in a real MIF generator
         *------------------------------------------------------------*/
        poLine = (OGRLineString*)poGeom;
        numPoints = poLine->getNumPoints();
        fprintf(fpOut, "PLINE %d\n", numPoints);
        for(i=0; i<numPoints; i++)
            fprintf(fpOut, "%.15g %.15g\n", poLine->getX(i), poLine->getY(i));
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABArc: Missing or Invalid Geometry!");
        return;
    }

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();

    fflush(fpOut);
}



/*=====================================================================
 *                      class TABText
 *====================================================================*/

/**********************************************************************
 *                   TABText::TABText()
 *
 * Constructor.
 **********************************************************************/
TABText::TABText(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
    m_pszString = NULL;

    m_dAngle = m_dHeight = 0.0;
    m_dfLineEndX = m_dfLineEndY = 0.0;
    m_bLineEndSet = FALSE;

    m_rgbForeground = 0x000000;
    m_rgbBackground = 0xffffff;

    m_nTextAlignment = 0;
    m_nFontStyle = 0;
    m_dWidth = 0;
}

/**********************************************************************
 *                   TABText::~TABText()
 *
 * Destructor.
 **********************************************************************/
TABText::~TABText()
{
    CPLFree(m_pszString);
}

/**********************************************************************
 *                     TABText::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CopyTABFeatureBase() and 
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABText::CloneTABFeature(OGRFeatureDefn *poNewDefn/*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABText *poNew = new TABText(poNewDefn ? poNewDefn : GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeaturePen
    *(poNew->GetPenDefRef()) = *GetPenDefRef();

    // ITABFeatureFont
    *(poNew->GetFontDefRef()) = *GetFontDefRef();


    poNew->SetTextString( GetTextString() );
    poNew->SetTextAngle( GetTextAngle() );
    poNew->SetTextBoxHeight( GetTextBoxHeight() );
    poNew->SetTextBoxWidth( GetTextBoxWidth() );
    poNew->SetFontStyleTABValue( GetFontStyleTABValue() );
    poNew->SetFontBGColor( GetFontBGColor() );
    poNew->SetFontFGColor( GetFontFGColor() );

    poNew->SetTextJustification( GetTextJustification() );
    poNew->SetTextSpacing( GetTextSpacing() );
    // Note: Text arrow/line coordinates are not transported... but 
    //       we ignore them most of the time anyways.
    poNew->SetTextLineType( TABTLNoLine );

    return poNew;
}

/**********************************************************************
 *                   TABText::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
int  TABText::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    OGRGeometry *poGeom;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
    {
        m_nMapInfoType = TAB_GEOM_TEXT;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABText: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    /*-----------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *----------------------------------------------------------------*/
    // __TODO__ For now we always write uncompressed for this class...
    // ValidateCoordType(poMapFile);

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABText::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABText::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                     TABMAPObjHdr *poObjHdr)
{
    double              dXMin, dYMin, dXMax, dYMax;
    OGRGeometry         *poGeometry;
    TABMAPObjectBlock   *poObjBlock;
    GBool               bComprCoord;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();

    bComprCoord = (m_nMapInfoType == TAB_GEOM_TEXT_C);

    if (m_nMapInfoType == TAB_GEOM_TEXT ||
        m_nMapInfoType == TAB_GEOM_TEXT_C )
    {
        /*=============================================================
         * TEXT
         *============================================================*/
        int     nStringLen;
        GInt32  nCoordBlockPtr;
        GInt32  nX, nY;
        double  dJunk;
        TABMAPCoordBlock        *poCoordBlock;

        /*-------------------------------------------------------------
         * Read data from poObjBlock
         *------------------------------------------------------------*/
        nCoordBlockPtr = poObjBlock->ReadInt32();  // String position
        nStringLen     = poObjBlock->ReadInt16();  // String length
        m_nTextAlignment = poObjBlock->ReadInt16();  // just./spacing/arrow

        /*-------------------------------------------------------------
         * Text Angle, in thenths of degree.
         * Contrary to arc start/end angles, no conversion based on 
         * origin quadrant is required here
         *------------------------------------------------------------*/
        m_dAngle       = poObjBlock->ReadInt16()/10.0;

        m_nFontStyle = poObjBlock->ReadInt16();          // Font style

        m_rgbForeground = poObjBlock->ReadByte()*256*256 +
                          poObjBlock->ReadByte()*256 +
                          poObjBlock->ReadByte();
        m_rgbBackground = poObjBlock->ReadByte()*256*256 +
                          poObjBlock->ReadByte()*256 +
                          poObjBlock->ReadByte();

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);    // arrow endpoint
        poMapFile->Int2Coordsys(nX, nY, m_dfLineEndX, m_dfLineEndY);
        m_bLineEndSet = TRUE;

        // Text Height
        nY = bComprCoord? poObjBlock->ReadInt16():poObjBlock->ReadInt32();
        poMapFile->Int2CoordsysDist(0, nY, dJunk, m_dHeight);

        m_nFontDefIndex = poObjBlock->ReadByte();      // Font name index
        poMapFile->ReadFontDef(m_nFontDefIndex, &m_sFontDef);

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);    // Read MBR
        poMapFile->Int2Coordsys(nX, nY, dXMin, dYMin);
        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMax, dYMax);

        m_nPenDefIndex = poObjBlock->ReadByte();      // Pen index for line
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);

        /*-------------------------------------------------------------
         * Read text string from the coord. block
         * Note that the string may contain binary '\n' and '\\' chars
         * that we keep to an unescaped form internally. This is to
         * be like OGR drivers. See bug 1107 for details.
         *------------------------------------------------------------*/
        char *pszTmpString = (char*)CPLMalloc((nStringLen+1)*sizeof(char));

        if (nStringLen > 0)
        {
            CPLAssert(nCoordBlockPtr > 0);
            poCoordBlock = poMapFile->GetCoordBlock(nCoordBlockPtr);
            if (poCoordBlock == NULL ||
                poCoordBlock->ReadBytes(nStringLen,(GByte*)pszTmpString) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Failed reading text string at offset %d", 
                         nCoordBlockPtr);
                CPLFree(pszTmpString);                
                return -1;
            }
        }

        pszTmpString[nStringLen] = '\0';

        CPLFree(m_pszString);
        m_pszString = pszTmpString; // This string was Escaped before 20050714
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }
    
    /* Set/retrieve the MBR to make sure Mins are smaller than Maxs
     */
    SetMBR(dXMin, dYMin, dXMax, dYMax);
    GetMBR(dXMin, dYMin, dXMax, dYMax);

    /*-----------------------------------------------------------------
     * Create an OGRPoint Geometry... 
     * The point X,Y values will be the coords of the lower-left corner before
     * rotation is applied.  (Note that the rotation in MapInfo is done around
     * the upper-left corner)
     * We need to calculate the true lower left corner of the text based
     * on the MBR after rotation, the text height and the rotation angle.
     *----------------------------------------------------------------*/
    double dCos, dSin, dX, dY;
    dSin = sin(m_dAngle*PI/180.0);
    dCos = cos(m_dAngle*PI/180.0);
    if (dSin > 0.0  && dCos > 0.0)
    {
        dX = dXMin + m_dHeight * dSin;
        dY = dYMin;
    }
    else if (dSin > 0.0  && dCos < 0.0)
    {
        dX = dXMax;
        dY = dYMin - m_dHeight * dCos;
    }
    else if (dSin < 0.0  && dCos < 0.0)
    {
        dX = dXMax + m_dHeight * dSin;
        dY = dYMax;
    }
    else  // dSin < 0 && dCos > 0
    {   
        dX = dXMin;
        dY = dYMax - m_dHeight * dCos;
    }

    poGeometry = new OGRPoint(dX, dY);

    SetGeometryDirectly(poGeometry);

    /*-----------------------------------------------------------------
     * Compute Text Width: the width of the Text MBR before rotation 
     * in ground units... unfortunately this value is not stored in the
     * file, so we have to compute it with the MBR after rotation and 
     * the height of the MBR before rotation:
     * With  W = Width of MBR before rotation
     *       H = Height of MBR before rotation
     *       dX = Width of MBR after rotation
     *       dY = Height of MBR after rotation
     *       teta = rotation angle
     *
     *  For [-PI/4..teta..+PI/4] or [3*PI/4..teta..5*PI/4], we'll use:
     *   W = H * (dX - H * sin(teta)) / (H * cos(teta))
     *
     * and for other teta values, use:
     *   W = H * (dY - H * cos(teta)) / (H * sin(teta))
     *----------------------------------------------------------------*/
    dSin = ABS(dSin);
    dCos = ABS(dCos);
    if (m_dHeight == 0.0)
        m_dWidth = 0.0;
    else if ( dCos > dSin )
        m_dWidth = m_dHeight * ((dXMax-dXMin) - m_dHeight*dSin) / 
                                                        (m_dHeight*dCos);
    else
        m_dWidth = m_dHeight * ((dYMax-dYMin) - m_dHeight*dCos) /
                                                        (m_dHeight*dSin);
    m_dWidth = ABS(m_dWidth);

    return 0;
}

/**********************************************************************
 *                   TABText::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABText::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                       TABMAPObjHdr *poObjHdr)
{
    GInt32              nX, nY, nXMin, nYMin, nXMax, nYMax;
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;
    GInt32              nCoordBlockPtr;
    TABMAPCoordBlock    *poCoordBlock;
    int                 nStringLen;
  
    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABText: Missing or Invalid Geometry!");
        return -1;
    }

    poMapFile->Coordsys2Int(poPoint->getX(), poPoint->getY(), nX, nY);

    /*-----------------------------------------------------------------
     * Write string to a coord block first...
     * Note that the string may contain unescaped '\n' and '\\'
     * that we have to keep like that for the MAP file.
     * See MapTools bug 1107 for more details. 
     *----------------------------------------------------------------*/
    poCoordBlock = poMapFile->GetCurCoordBlock();
    poCoordBlock->StartNewFeature();
    nCoordBlockPtr = poCoordBlock->GetCurAddress();

    // This string was escaped before 20050714
    char *pszTmpString = m_pszString;

    nStringLen = strlen(pszTmpString);

    if (nStringLen > 0)
    {
        poCoordBlock->WriteBytes(nStringLen, (GByte *)pszTmpString);
    }
    else
    {
        nCoordBlockPtr = 0;
    }

    pszTmpString = NULL;

    /*-----------------------------------------------------------------
     * Copy object information
     *----------------------------------------------------------------*/
    TABMAPObjText *poTextHdr = (TABMAPObjText *)poObjHdr;

    poTextHdr->m_nCoordBlockPtr = nCoordBlockPtr;     // String position
    poTextHdr->m_nCoordDataSize = nStringLen;         // String length
    poTextHdr->m_nTextAlignment = m_nTextAlignment;   // just./spacing/arrow

    /*-----------------------------------------------------------------
     * Text Angle, (written in thenths of degrees)
     * Contrary to arc start/end angles, no conversion based on 
     * origin quadrant is required here
     *----------------------------------------------------------------*/
    poTextHdr->m_nAngle = ROUND_INT(m_dAngle*10.0);

    poTextHdr->m_nFontStyle = m_nFontStyle;          // Font style/effect

    poTextHdr->m_nFGColorR = COLOR_R(m_rgbForeground);
    poTextHdr->m_nFGColorG = COLOR_G(m_rgbForeground);
    poTextHdr->m_nFGColorB = COLOR_B(m_rgbForeground);

    poTextHdr->m_nBGColorR = COLOR_R(m_rgbBackground);
    poTextHdr->m_nBGColorG = COLOR_G(m_rgbBackground);
    poTextHdr->m_nBGColorB = COLOR_B(m_rgbBackground);

    /*-----------------------------------------------------------------
     * The OGRPoint's X,Y values were the coords of the lower-left corner
     * before rotation was applied.  (Note that the rotation in MapInfo is
     * done around the upper-left corner)
     * The Feature's MBR is the MBR of the text after rotation... that's
     * what MapInfo uses to define the text location.
     *----------------------------------------------------------------*/
    double dXMin, dYMin, dXMax, dYMax;
    // Make sure Feature MBR is in sync with other params
 
    UpdateTextMBR();
    GetMBR(dXMin, dYMin, dXMax, dYMax);

    poMapFile->Coordsys2Int(dXMin, dYMin, nXMin, nYMin);
    poMapFile->Coordsys2Int(dXMax, dYMax, nXMax, nYMax);

    // Label line end point
    double dX, dY;
    GetTextLineEndPoint(dX, dY); // Make sure a default line end point is set
    poMapFile->Coordsys2Int(m_dfLineEndX, m_dfLineEndY, 
                           poTextHdr->m_nLineEndX, poTextHdr->m_nLineEndY);

    // Text Height
    poMapFile->Coordsys2IntDist(0.0, m_dHeight, nX, nY);
    poTextHdr->m_nHeight = nY;

    // Font name
    m_nFontDefIndex = poMapFile->WriteFontDef(&m_sFontDef);
    poTextHdr->m_nFontId = m_nFontDefIndex;      // Font name index

    // MBR after rotation
    poTextHdr->SetMBR(nXMin, nYMin, nXMax, nYMax);

    m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
    poTextHdr->m_nPenId = m_nPenDefIndex;      // Pen index for line/arrow


    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *                   TABText::GetTextString()
 *
 * Return ref to text string value.
 *
 * Returned string is a reference to the internal string buffer and should
 * not be modified or freed by the caller.
 **********************************************************************/
const char *TABText::GetTextString()
{
    if (m_pszString == NULL)
        return "";

    return m_pszString;
}

/**********************************************************************
 *                   TABText::SetTextString()
 *
 * Set new text string value.
 *
 * Note: The text string may contain "\n" chars or "\\" chars
 * and we expect to receive them in a 2 chars escaped form as 
 * described in the MIF format specs.
 **********************************************************************/
void TABText::SetTextString(const char *pszNewStr)
{
    CPLFree(m_pszString);
    m_pszString = CPLStrdup(pszNewStr);
}

/**********************************************************************
 *                   TABText::GetTextAngle()
 *
 * Return text angle in degrees.
 **********************************************************************/
double TABText::GetTextAngle()
{
    return m_dAngle;
}

void TABText::SetTextAngle(double dAngle)
{
    // Make sure angle is in the range [0..360]
    while(dAngle < 0.0)   dAngle += 360.0;
    while(dAngle > 360.0) dAngle -= 360.0;
    m_dAngle = dAngle;
    UpdateTextMBR();
}

/**********************************************************************
 *                   TABText::GetTextBoxHeight()
 *
 * Return text height in Y axis coord. units of the text box before rotation.
 **********************************************************************/
double TABText::GetTextBoxHeight()
{
    return m_dHeight;
}

void TABText::SetTextBoxHeight(double dHeight)
{
    m_dHeight = dHeight;
    UpdateTextMBR();
}

/**********************************************************************
 *                   TABText::GetTextBoxWidth()
 *
 * Return text width in X axis coord. units. of the text box before rotation.
 *
 * If value has not been set, then we force a default value that assumes
 * that one char's box width is 60% of its height... and we ignore
 * the multiline case.  This should not matter when the user PROPERLY sets
 * the value.
 **********************************************************************/
double TABText::GetTextBoxWidth()
{
    if (m_dWidth == 0.0 && m_pszString)
    {
        m_dWidth = 0.6 * m_dHeight * strlen(m_pszString);
    }
    return m_dWidth;
}

void TABText::SetTextBoxWidth(double dWidth)
{
    m_dWidth = dWidth;
    UpdateTextMBR();
}

/**********************************************************************
 *                   TABText::GetTextLineEndPoint()
 *
 * Return X,Y coordinates of the text label line end point.
 * Default is the center of the text MBR.
 **********************************************************************/
void TABText::GetTextLineEndPoint(double &dX, double &dY)
{
    if (!m_bLineEndSet)
    {
        // Set default location at center of text MBR
        double dXMin, dYMin, dXMax, dYMax;
        UpdateTextMBR();
        GetMBR(dXMin, dYMin, dXMax, dYMax);
        m_dfLineEndX = (dXMin + dXMax) /2.0;
        m_dfLineEndY = (dYMin + dYMax) /2.0;
        m_bLineEndSet = TRUE;
    }

    // Return values
    dX = m_dfLineEndX;
    dY = m_dfLineEndY;
}

void TABText::SetTextLineEndPoint(double dX, double dY)
{
    m_dfLineEndX = dX;
    m_dfLineEndY = dY;
    m_bLineEndSet = TRUE;
}

/**********************************************************************
 *                   TABText::UpdateTextMBR()
 *
 * Update the feature MBR using the text origin (OGRPoint geometry), the
 * rotation angle, and the Width/height before rotation.
 *
 * This function cannot perform properly unless all the above have been set.
 **********************************************************************/
void TABText::UpdateTextMBR()
{
    OGRGeometry *poGeom;
    OGRPoint *poPoint=NULL;

    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
    {
        double dSin, dCos, dX0, dY0, dX1, dY1;
        double dX[4], dY[4];
        poPoint = (OGRPoint *)poGeom;

        dX0 = poPoint->getX();
        dY0 = poPoint->getY();

        dSin = sin(m_dAngle*PI/180.0);
        dCos = cos(m_dAngle*PI/180.0);

        GetTextBoxWidth();  // Force default width value if necessary.
        
        dX[0] = dX0;
        dY[0] = dY0;
        dX[1] = dX0 + m_dWidth;
        dY[1] = dY0;
        dX[2] = dX0 + m_dWidth;
        dY[2] = dY0 + m_dHeight;
        dX[3] = dX0;
        dY[3] = dY0 + m_dHeight;

        SetMBR(dX0, dY0, dX0, dY0);
        for(int i=0; i<4; i++)
        {
            // Rotate one of the box corners
            dX1 = dX0 + (dX[i]-dX0)*dCos - (dY[i]-dY0)*dSin;
            dY1 = dY0 + (dX[i]-dX0)*dSin + (dY[i]-dY0)*dCos;

            // And update feature MBR with rotated coordinate
            if (dX1 < m_dXMin) m_dXMin = dX1;
            if (dX1 > m_dXMax) m_dXMax = dX1;
            if (dY1 < m_dYMin) m_dYMin = dY1;
            if (dY1 > m_dYMax) m_dYMax = dY1;
        }
    }
}

/**********************************************************************
 *                   TABText::GetFontBGColor()
 *
 * Return background color.
 **********************************************************************/
GInt32 TABText::GetFontBGColor()
{
    return m_rgbBackground;
}

void TABText::SetFontBGColor(GInt32 rgbColor)
{
    m_rgbBackground = rgbColor;
}

/**********************************************************************
 *                   TABText::GetFontFGColor()
 *
 * Return foreground color.
 **********************************************************************/
GInt32 TABText::GetFontFGColor()
{
    return m_rgbForeground;
}

void TABText::SetFontFGColor(GInt32 rgbColor)
{
    m_rgbForeground = rgbColor;
}

/**********************************************************************
 *                   TABText::GetTextJustification()
 *
 * Return text justification.  Default is TABTJLeft
 **********************************************************************/
TABTextJust TABText::GetTextJustification()
{
    TABTextJust eJust = TABTJLeft;

    if (m_nTextAlignment & 0x0200)
        eJust = TABTJCenter;
    else if (m_nTextAlignment & 0x0400)
        eJust = TABTJRight;

    return eJust;
}

void TABText::SetTextJustification(TABTextJust eJustification)
{
    // Flush current value... default is TABTJLeft
    m_nTextAlignment &= ~ 0x0600;
    // ... and set new one.
    if (eJustification == TABTJCenter)
        m_nTextAlignment |= 0x0200;
    else if (eJustification == TABTJRight)
        m_nTextAlignment |= 0x0400;
}

/**********************************************************************
 *                   TABText::GetTextSpacing()
 *
 * Return text vertical spacing factor.  Default is TABTSSingle
 **********************************************************************/
TABTextSpacing TABText::GetTextSpacing()
{
    TABTextSpacing eSpacing = TABTSSingle;

    if (m_nTextAlignment & 0x0800)
        eSpacing = TABTS1_5;
    else if (m_nTextAlignment & 0x1000)
        eSpacing = TABTSDouble;

    return eSpacing;
}

void TABText::SetTextSpacing(TABTextSpacing eSpacing)
{
    // Flush current value... default is TABTSSingle
    m_nTextAlignment &= ~ 0x1800;
    // ... and set new one.
    if (eSpacing == TABTS1_5)
        m_nTextAlignment |= 0x0800;
    else if (eSpacing == TABTSDouble)
        m_nTextAlignment |= 0x1000;
}

/**********************************************************************
 *                   TABText::GetTextLineType()
 *
 * Return text line (arrow) type.  Default is TABTLNoLine
 **********************************************************************/
TABTextLineType TABText::GetTextLineType()
{
    TABTextLineType eLine = TABTLNoLine;

    if (m_nTextAlignment & 0x2000)
        eLine = TABTLSimple;
    else if (m_nTextAlignment & 0x4000)
        eLine = TABTLArrow;

    return eLine;
}

void TABText::SetTextLineType(TABTextLineType eLineType)
{
    // Flush current value... default is TABTLNoLine
    m_nTextAlignment &= ~ 0x6000;
    // ... and set new one.
    if (eLineType == TABTLSimple)
        m_nTextAlignment |= 0x2000;
    else if (eLineType == TABTLArrow)
        m_nTextAlignment |= 0x4000;
}

/**********************************************************************
 *                   TABText::QueryFontStyle()
 *
 * Return TRUE if the specified font style attribute is turned ON,
 * or FALSE otherwise.  See enum TABFontStyle for the list of styles
 * that can be queried on.
 **********************************************************************/
GBool TABText::QueryFontStyle(TABFontStyle eStyleToQuery)
{
    return (m_nFontStyle & (int)eStyleToQuery) ? TRUE: FALSE;
}

void TABText::ToggleFontStyle(TABFontStyle eStyleToToggle, GBool bStyleOn)
{
    if (bStyleOn)
        m_nFontStyle |=  (int)eStyleToToggle;
    else
        m_nFontStyle &=  ~ (int)eStyleToToggle;
}


/**********************************************************************
 *                   TABText::GetFontStyleMIFValue()
 *
 * Return the Font Style value for this object using the style values
 * that are used in a MIF FONT() clause.  See MIF specs (appendix A).
 *
 * The reason why we have to differentiate between the TAB and the MIF font
 * style values is that in TAB, TABFSBox is included in the style value
 * as code 0x100, but in MIF it is not included, instead it is implied by
 * the presence of the BG color in the FONT() clause (the BG color is 
 * present only when TABFSBox or TABFSHalo is set).
 * This also has the effect of shifting all the other style values > 0x100
 * by 1 byte.
 **********************************************************************/
int TABText::GetFontStyleMIFValue()
{
    // The conversion is simply to remove bit 0x100 from the value and shift
    // down all values past this bit.
    return (m_nFontStyle & 0xff) + (m_nFontStyle & (0xff00-0x0100))/2;
}

void TABText:: SetFontStyleMIFValue(int nStyle, GBool bBGColorSet)
{
    m_nFontStyle = (nStyle & 0xff) + (nStyle & 0x7f00)*2;
    // When BG color is set, then either BOX or HALO should be set.
    if (bBGColorSet && !QueryFontStyle(TABFSHalo))
        ToggleFontStyle(TABFSBox, TRUE);
}

int TABText::IsFontBGColorUsed()
{
    // Font BG color is used only when BOX or HALO are set.
    return (QueryFontStyle(TABFSBox) || QueryFontStyle(TABFSHalo));
}

/**********************************************************************
 *                   TABText::GetLabelStyleString()
 *
 * This is not the correct location, it should be in ITABFeatureFont,
 * but it's really more easy to put it here.  This fct return a complete
 * string for the representation with the string to display
 **********************************************************************/
const char *TABText::GetLabelStyleString()
{
    const char *pszStyle = NULL;
    char szPattern[20];
    int nJustification = 1;
    
    szPattern[0] = '\0';
    

    switch(GetTextJustification())
    {
      case TABTJCenter:
        nJustification = 2;
        break;
      case TABTJRight:
        nJustification = 1;
        break;
      case TABTJLeft:
      default:
        nJustification =1;
        break;
    }

    // Compute real font size, taking number of lines ("\\n") and line
    // spacing into account.
    int numLines = 1;
    const char *pszNewline = GetTextString();
    while((pszNewline = strstr(pszNewline, "\\n")) != NULL)
    {
        numLines++;
        pszNewline +=2;
    }
    
    double dHeight = GetTextBoxHeight()/numLines;

    // In all cases, take out 20% of font height to account for line spacing
    switch(GetTextSpacing())
    {
      case TABTS1_5:
        dHeight *= (0.67 * 0.8);
        break;
      case TABTSDouble:
        dHeight *= (0.5 * 0.8);
        break;
      default:
        dHeight *= 0.8;
    }

    if (IsFontBGColorUsed())
        pszStyle=CPLSPrintf("LABEL(t:\"%s\",a:%f,s:%fg,c:#%6.6x,b:#%6.6x,p:%d,f:\"%s\")",
                            GetTextString(),GetTextAngle(), dHeight,
                            GetFontFGColor(),GetFontBGColor(),nJustification,
                            GetFontNameRef());
    else
        pszStyle=CPLSPrintf("LABEL(t:\"%s\",a:%f,s:%fg,c:#%6.6x,p:%d,f:\"%s\")",
                            GetTextString(),GetTextAngle(), dHeight,
                            GetFontFGColor(),nJustification,
                            GetFontNameRef());
     
    return pszStyle;
    
}  

/**********************************************************************
 *                   TABText::GetStyleString()
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABText::GetStyleString()
{
    if (m_pszStyleString == NULL)
    {
        m_pszStyleString = CPLStrdup(GetLabelStyleString());
    }

    return m_pszStyleString;
}



/**********************************************************************
 *                   TABText::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF REGIONs.
 **********************************************************************/
void TABText::DumpMIF(FILE *fpOut /*=NULL*/)
{
    OGRGeometry   *poGeom;
    OGRPoint      *poPoint = NULL;

    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
    {
        /*-------------------------------------------------------------
         * Generate output for text object
         *------------------------------------------------------------*/
        poPoint = (OGRPoint*)poGeom;

        fprintf(fpOut, "TEXT \"%s\" %.15g %.15g\n", m_pszString?m_pszString:"",
                poPoint->getX(), poPoint->getY());

        fprintf(fpOut, "  m_pszString = '%s'\n", m_pszString);
        fprintf(fpOut, "  m_dAngle    = %.15g\n",   m_dAngle);
        fprintf(fpOut, "  m_dHeight   = %.15g\n",   m_dHeight);
        fprintf(fpOut, "  m_rgbForeground  = 0x%6.6x (%d)\n", 
                                             m_rgbForeground, m_rgbForeground);
        fprintf(fpOut, "  m_rgbBackground  = 0x%6.6x (%d)\n", 
                                             m_rgbBackground, m_rgbBackground);
        fprintf(fpOut, "  m_nTextAlignment = 0x%4.4x\n",  m_nTextAlignment);
        fprintf(fpOut, "  m_nFontStyle     = 0x%4.4x\n",  m_nFontStyle);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABText: Missing or Invalid Geometry!");
        return;
    }

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();
    DumpFontDef();

    fflush(fpOut);
}

/*=====================================================================
 *                      class TABMultiPoint
 *====================================================================*/

/**********************************************************************
 *                   TABMultiPoint::TABMultiPoint()
 *
 * Constructor.
 **********************************************************************/
TABMultiPoint::TABMultiPoint(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
    m_bCenterIsSet = FALSE;
}

/**********************************************************************
 *                   TABMultiPoint::~TABMultiPoint()
 *
 * Destructor.
 **********************************************************************/
TABMultiPoint::~TABMultiPoint()
{
}

/**********************************************************************
 *                     TABMultiPoint::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CloneTABFeature() and 
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABMultiPoint::CloneTABFeature(OGRFeatureDefn *poNewDefn /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABMultiPoint *poNew = new TABMultiPoint(poNewDefn?poNewDefn:GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeatureSymbol
    *(poNew->GetSymbolDefRef()) = *GetSymbolDefRef();

    poNew->m_bCenterIsSet = m_bCenterIsSet;
    poNew->m_dCenterX = m_dCenterX;
    poNew->m_dCenterY = m_dCenterY;

    return poNew;
}


/**********************************************************************
 *                   TABMultiPoint::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
int  TABMultiPoint::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    OGRGeometry *poGeom;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry 
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint)
    {
        m_nMapInfoType = TAB_GEOM_MULTIPOINT;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABMultiPoint: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    /*-----------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *----------------------------------------------------------------*/
    ValidateCoordType(poMapFile);

    return m_nMapInfoType;
}



/**********************************************************************
 *                   TABMultiPoint::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABMultiPoint::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                           TABMAPObjHdr *poObjHdr)
{
    return ReadGeometryFromMAPFile(poMapFile, poObjHdr, NULL);
}

int TABMultiPoint::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                           TABMAPObjHdr *poObjHdr,
                                           TABMAPCoordBlock **ppoCoordBlock)
{
    GInt32              nX, nY;
    double              dX, dY, dXMin, dYMin, dXMax, dYMax;
    OGRGeometry         *poGeometry=NULL;
    GBool               bComprCoord = poObjHdr->IsCompressedType();
    TABMAPCoordBlock    *poCoordBlock = NULL;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poObjHdr->m_nType;

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    if (m_nMapInfoType == TAB_GEOM_MULTIPOINT ||
        m_nMapInfoType == TAB_GEOM_MULTIPOINT_C )
    {
        /*-------------------------------------------------------------
         * Copy data from poObjHdr
         *------------------------------------------------------------*/
        TABMAPObjMultiPoint *poMPointHdr = (TABMAPObjMultiPoint *)poObjHdr;

        // MBR
        poMapFile->Int2Coordsys(poMPointHdr->m_nMinX, poMPointHdr->m_nMinY, 
                                dXMin, dYMin);
        poMapFile->Int2Coordsys(poMPointHdr->m_nMaxX, poMPointHdr->m_nMaxY, 
                                dXMax, dYMax);

        m_nSymbolDefIndex = poMPointHdr->m_nSymbolId;   // Symbol index
        poMapFile->ReadSymbolDef(m_nSymbolDefIndex, &m_sSymbolDef);

        // Centroid/label point
        poMapFile->Int2Coordsys(poMPointHdr->m_nLabelX, poMPointHdr->m_nLabelY,
                                dX, dY);
        SetCenter(dX, dY);

        /*-------------------------------------------------------------
         * Read Point Coordinates
         *------------------------------------------------------------*/
        OGRMultiPoint   *poMultiPoint;
        poGeometry = poMultiPoint = new OGRMultiPoint();

        poCoordBlock = poMapFile->GetCoordBlock(poMPointHdr->m_nCoordBlockPtr);
        poCoordBlock->SetComprCoordOrigin(poMPointHdr->m_nComprOrgX, 
                                          poMPointHdr->m_nComprOrgY);

        for(int iPoint=0; iPoint<poMPointHdr->m_nNumPoints; iPoint++)
        {
            if (poCoordBlock->ReadIntCoord(bComprCoord, nX, nY) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Failed reading coordinate data at offset %d", 
                         poMPointHdr->m_nCoordBlockPtr);
                return -1;
            }

            poMapFile->Int2Coordsys(nX, nY, dX, dY);
            OGRPoint *poPoint = new OGRPoint(dX, dY);
    
            if (poMultiPoint->addGeometryDirectly(poPoint) != OGRERR_NONE)
            {
                CPLAssert(FALSE); // Just in case lower-level lib is modified
            }
        }

    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    SetGeometryDirectly(poGeometry);

    SetMBR(dXMin, dYMin, dXMax, dYMax);

    /* Return a ref to coord block so that caller can continue reading
     * after the end of this object (used by TABCollection)
     */
    if (ppoCoordBlock)
        *ppoCoordBlock = poCoordBlock;

    return 0;
}

/**********************************************************************
 *                   TABMultiPoint::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABMultiPoint::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                          TABMAPObjHdr *poObjHdr)
{
    GInt32              nX, nY;
    OGRGeometry         *poGeom;
    OGRMultiPoint       *poMPoint;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    TABMAPObjMultiPoint *poMPointHdr = (TABMAPObjMultiPoint *)poObjHdr;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint)
        poMPoint = (OGRMultiPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABMultiPoint: Missing or Invalid Geometry!");
        return -1;
    }

    poMPointHdr->m_nNumPoints = poMPoint->getNumGeometries();

    /*-----------------------------------------------------------------
     * Write data to coordinate block
     *----------------------------------------------------------------*/
    int iPoint, nStatus;
    TABMAPCoordBlock *poCoordBlock;
    GBool   bCompressed = poObjHdr->IsCompressedType();

    poCoordBlock = poMapFile->GetCurCoordBlock();
    poCoordBlock->StartNewFeature();
    poMPointHdr->m_nCoordBlockPtr = poCoordBlock->GetCurAddress();
    poCoordBlock->SetComprCoordOrigin(m_nComprOrgX, m_nComprOrgY);


    for(iPoint=0, nStatus=0; 
        nStatus == 0 && iPoint < poMPointHdr->m_nNumPoints; iPoint++)
    {
        poGeom = poMPoint->getGeometryRef(iPoint);

        if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        {
            OGRPoint *poPoint = (OGRPoint*)poGeom;

            poMapFile->Coordsys2Int(poPoint->getX(), poPoint->getY(), nX, nY);
            if (iPoint == 0)
            {
                // Default to the first point, we may use explicit value below
                poMPointHdr->m_nLabelX = nX;
                poMPointHdr->m_nLabelY = nY;
            }

            if ((nStatus = poCoordBlock->WriteIntCoord(nX, nY, 
                                                       bCompressed)) != 0)
            {
                // Failed ... error message has already been produced
                return nStatus;
            }   

        }
        else
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "TABMultiPoint: Invalid Geometry, expecting OGRPoint!");
            return -1;
        }
    }

    /*-----------------------------------------------------------------
     * Copy object information
     *----------------------------------------------------------------*/

    // Compressed coordinate origin (useful only in compressed case!)
    poMPointHdr->m_nComprOrgX = m_nComprOrgX;
    poMPointHdr->m_nComprOrgY = m_nComprOrgY;

    poMPointHdr->m_nCoordDataSize = poCoordBlock->GetFeatureDataSize();
    poMPointHdr->SetMBR(m_nXMin, m_nYMin, m_nXMax, m_nYMax);

    // Center/label point (default value already set above)
    double dX, dY;
    if (GetCenter(dX, dY) != -1)
    {
        poMapFile->Coordsys2Int(dX, dY, poMPointHdr->m_nLabelX, 
                                poMPointHdr->m_nLabelY);
    }

    m_nSymbolDefIndex = poMapFile->WriteSymbolDef(&m_sSymbolDef);
    poMPointHdr->m_nSymbolId = m_nSymbolDefIndex;      // Symbol index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *                   TABMultiPoint::GetXY()
 *
 * Return this point's X,Y coordinates.
 **********************************************************************/
int TABMultiPoint::GetXY(int i, double &dX, double &dY)
{
    OGRGeometry *poGeom;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint)
    {
        OGRMultiPoint *poMPoint = (OGRMultiPoint*)poGeom;

        if (i >= 0 && i < poMPoint->getNumGeometries() &&
            (poGeom = poMPoint->getGeometryRef(i)) != NULL &&
            wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
        {
            OGRPoint *poPoint = (OGRPoint*)poGeom;

            dX = poPoint->getX();
            dY = poPoint->getY();
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABMultiPoint: Missing or Invalid Geometry!");
        dX = dY = 0.0;
        return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABMultiPoint::GetNumPoints()
 *
 * Return the number of points in this multipoint object
 **********************************************************************/
int TABMultiPoint::GetNumPoints()
{
    OGRGeometry *poGeom;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint)
    {
        OGRMultiPoint *poMPoint = (OGRMultiPoint*)poGeom;

        return poMPoint->getNumGeometries();
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABMultiPoint: Missing or Invalid Geometry!");
        return 0;
    }

    return 0;
}


/**********************************************************************
 *                   TABMultiPoint::GetStyleString()
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABMultiPoint::GetStyleString()
{
    if (m_pszStyleString == NULL)
    {
        m_pszStyleString = CPLStrdup(GetSymbolStyleString());
    }

    return m_pszStyleString;
}

/**********************************************************************
 *                   TABMultiPoint::GetCenter()
 *
 * Returns the center point (or label point?) of the object.  Compute one 
 * if it was not explicitly set:
 *
 * The default seems to be to use the first point in the collection as
 * the center.. so we'll use that.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMultiPoint::GetCenter(double &dX, double &dY)
{
    if (!m_bCenterIsSet && GetNumPoints() > 0)
    {
        // The default seems to be to use the first point in the collection
        // as the center... so we'll use that.
        if (GetXY(0, m_dCenterX, m_dCenterY) == 0)
            m_bCenterIsSet = TRUE;
    }

    if (!m_bCenterIsSet)
        return -1;

    dX = m_dCenterX;
    dY = m_dCenterY;
    return 0;
}

/**********************************************************************
 *                   TABMultiPoint::SetCenter()
 *
 * Set the X,Y coordinates to use as center point (or label point?)
 **********************************************************************/
void TABMultiPoint::SetCenter(double dX, double dY)
{
    m_dCenterX = dX;
    m_dCenterY = dY;
    m_bCenterIsSet = TRUE;
}


/**********************************************************************
 *                   TABMultiPoint::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF POINTs.
 **********************************************************************/
void TABMultiPoint::DumpMIF(FILE *fpOut /*=NULL*/)
{
    OGRGeometry *poGeom;
    OGRMultiPoint *poMPoint;

    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint)
        poMPoint = (OGRMultiPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABMultiPoint: Missing or Invalid Geometry!");
        return;
    }

    /*-----------------------------------------------------------------
     * Generate output
     *----------------------------------------------------------------*/
    fprintf(fpOut, "MULTIPOINT %d\n", poMPoint->getNumGeometries());

    for (int iPoint=0; iPoint < poMPoint->getNumGeometries(); iPoint++)
    {
        poGeom = poMPoint->getGeometryRef(iPoint);

        if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        {
            OGRPoint *poPoint = (OGRPoint*)poGeom;
            fprintf(fpOut, "  %.15g %.15g\n", poPoint->getX(), poPoint->getY() );
        }
        else
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "TABMultiPoint: Invalid Geometry, expecting OGRPoint!");
            return;
        }
    }

    DumpSymbolDef(fpOut);

    if (m_bCenterIsSet)
        fprintf(fpOut, "Center %.15g %.15g\n", m_dCenterX, m_dCenterY);

    fflush(fpOut);
}

/*=====================================================================
 *                      class TABCollection
 *====================================================================*/

/**********************************************************************
 *                   TABCollection::TABCollection()
 *
 * Constructor.
 **********************************************************************/
TABCollection::TABCollection(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
    m_poRegion = NULL;
    m_poPline = NULL;
    m_poMpoint = NULL;
}

/**********************************************************************
 *                   TABCollection::~TABCollection()
 *
 * Destructor.
 **********************************************************************/
TABCollection::~TABCollection()
{
    EmptyCollection();
}

/**********************************************************************
 *                   TABCollection::EmptyCollection()
 *
 * Delete/free all collection components.
 **********************************************************************/
void TABCollection::EmptyCollection()
{

    if (m_poRegion)
    {
        delete m_poRegion;
        m_poRegion = NULL;
    }

    if (m_poPline)
    {
        delete m_poPline;
        m_poPline = NULL;
    }

    if (m_poMpoint)
    {
        delete m_poMpoint;
        m_poMpoint = NULL;
    }

    // Empty OGR Geometry Collection as well
    SyncOGRGeometryCollection(TRUE, TRUE, TRUE);

}

/**********************************************************************
 *                     TABCollection::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CloneTABFeature() and 
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABCollection::CloneTABFeature(OGRFeatureDefn *poNewDefn /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABCollection *poNew = new TABCollection(poNewDefn?poNewDefn:GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/

    if (m_poRegion)
        poNew->SetRegionDirectly((TABRegion*)m_poRegion->CloneTABFeature());

    if (m_poPline)
        poNew->SetPolylineDirectly((TABPolyline*)m_poPline->CloneTABFeature());

    if (m_poMpoint)
        poNew->SetMultiPointDirectly((TABMultiPoint*)m_poMpoint->CloneTABFeature());

    return poNew;
}


/**********************************************************************
 *                   TABCollection::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
int  TABCollection::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    OGRGeometry *poGeom;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry 
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbGeometryCollection)
    {
        m_nMapInfoType = TAB_GEOM_COLLECTION;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABCollection: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    /*-----------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *----------------------------------------------------------------*/
    GBool bComprCoord = ValidateCoordType(poMapFile);

    /*-----------------------------------------------------------------
     * Since all members of the collection share the same compressed coord
     * origin, we should force the compressed origin in all components
     * to be the same. 
     * This also implies that ValidateMapInfoType() should *NOT* be called
     * again until the collection components are written by WriteGeom...()
     *----------------------------------------------------------------*/
    if (m_poRegion)
    {
        m_poRegion->ValidateCoordType(poMapFile);
        if (m_poRegion->ValidateMapInfoType(poMapFile) !=  TAB_GEOM_NONE)
        {
            GInt32 nXMin=0, nYMin=0, nXMax=0, nYMax=0;
            m_poRegion->GetIntMBR(nXMin, nYMin, nXMax, nYMax);
            m_poRegion->ForceCoordTypeAndOrigin(TAB_GEOM_V450_REGION,
                                                bComprCoord,
                                                m_nComprOrgX, m_nComprOrgY,
                                                nXMin, nYMin, nXMax, nYMax);
        }
    }

    if (m_poPline)
    {
        m_poPline->ValidateCoordType(poMapFile);
        if (m_poPline->ValidateMapInfoType(poMapFile) !=  TAB_GEOM_NONE)
        {
            GInt32 nXMin, nYMin, nXMax, nYMax;
            m_poPline->GetIntMBR(nXMin, nYMin, nXMax, nYMax);
            m_poPline->ForceCoordTypeAndOrigin(TAB_GEOM_V450_MULTIPLINE,
                                               bComprCoord,
                                               m_nComprOrgX, m_nComprOrgY,
                                               nXMin, nYMin, nXMax, nYMax);
        }
    }

    if (m_poMpoint)
    {
        m_poMpoint->ValidateCoordType(poMapFile);
        if (m_poMpoint->ValidateMapInfoType(poMapFile) !=  TAB_GEOM_NONE)
        {
            GInt32 nXMin, nYMin, nXMax, nYMax;
            m_poMpoint->GetIntMBR(nXMin, nYMin, nXMax, nYMax);
            m_poMpoint->ForceCoordTypeAndOrigin(TAB_GEOM_MULTIPOINT,
                                                bComprCoord,
                                                m_nComprOrgX, m_nComprOrgY,
                                                nXMin, nYMin, nXMax, nYMax);
        }
    }

    return m_nMapInfoType;
}


/**********************************************************************
 *                   TABCollection::ReadLabelAndMBR()
 *
 * Reads the label and MBR elements of the header of a collection component
 *
 * Returns 0 on success, -1 on failure.
 **********************************************************************/
int  TABCollection::ReadLabelAndMBR(TABMAPCoordBlock *poCoordBlock,
                                    GBool bComprCoord,
                                    GInt32 nComprOrgX, GInt32 nComprOrgY,
                                    GInt32 &pnMinX, GInt32 &pnMinY,
                                    GInt32 &pnMaxX, GInt32 &pnMaxY,
                                    GInt32 &pnLabelX, GInt32 &pnLabelY )
{
    //
    // The sections in the collection's coord blocks start with center/label
    // point + MBR that are normally found in the object data blocks 
    // of regular region/pline/mulitpoint objects.
    //
    if (bComprCoord)
    {
        // Region center/label point, relative to compr. coord. origin
        // No it's not relative to the Object block center
        pnLabelX = poCoordBlock->ReadInt16();
        pnLabelY = poCoordBlock->ReadInt16();

        pnLabelX += nComprOrgX;
        pnLabelY += nComprOrgY;

        pnMinX = nComprOrgX + poCoordBlock->ReadInt16(); // Read MBR
        pnMinY = nComprOrgY + poCoordBlock->ReadInt16();
        pnMaxX = nComprOrgX + poCoordBlock->ReadInt16();
        pnMaxY = nComprOrgY + poCoordBlock->ReadInt16();
    }
    else
    {
        // Region center/label point, relative to compr. coord. origin
        // No it's not relative to the Object block center
        pnLabelX = poCoordBlock->ReadInt32();
        pnLabelY = poCoordBlock->ReadInt32();

        pnMinX = poCoordBlock->ReadInt32();    // Read MBR
        pnMinY = poCoordBlock->ReadInt32();
        pnMaxX = poCoordBlock->ReadInt32();
        pnMaxY = poCoordBlock->ReadInt32();
    }

    return 0;
}

/**********************************************************************
 *                   TABCollection::WriteLabelAndMBR()
 *
 * Writes the label and MBR elements of the header of a collection component
 *
 * Returns 0 on success, -1 on failure.
 **********************************************************************/
int  TABCollection::WriteLabelAndMBR(TABMAPCoordBlock *poCoordBlock,
                                     GBool bComprCoord,
                                     GInt32 nMinX, GInt32 nMinY,
                                     GInt32 nMaxX, GInt32 nMaxY,
                                     GInt32 nLabelX, GInt32 nLabelY )
{
    int nStatus;

    //
    // The sections in the collection's coord blocks start with center/label
    // point + MBR that are normally found in the object data blocks 
    // of regular region/pline/mulitpoint objects.
    //

    if ((nStatus = poCoordBlock->WriteIntCoord(nLabelX, nLabelY, 
                                               bComprCoord)) != 0 ||
        (nStatus = poCoordBlock->WriteIntCoord(nMinX, nMinY, 
                                               bComprCoord)) != 0 ||
        (nStatus = poCoordBlock->WriteIntCoord(nMaxX, nMaxY, 
                                               bComprCoord)) != 0   )
    {
        // Failed ... error message has already been produced
        return nStatus;
    }   

    return 0;
}


/**********************************************************************
 *                   TABCollection::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABCollection::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                           TABMAPObjHdr *poObjHdr)
{
    double              dXMin, dYMin, dXMax, dYMax;
    GBool               bComprCoord = poObjHdr->IsCompressedType();
    TABMAPCoordBlock*   poCoordBlock = NULL;
    int                 nCurCoordBlockPtr;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poObjHdr->m_nType;

    if (m_nMapInfoType != TAB_GEOM_COLLECTION &&
        m_nMapInfoType != TAB_GEOM_COLLECTION_C )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    // Make sure collection is empty
    EmptyCollection();

    /*-------------------------------------------------------------
     * Copy data from poObjHdr
     *------------------------------------------------------------*/
    TABMAPObjCollection *poCollHdr = (TABMAPObjCollection *)poObjHdr;

    // MBR
    poMapFile->Int2Coordsys(poCollHdr->m_nMinX, poCollHdr->m_nMinY, 
                            dXMin, dYMin);
    poMapFile->Int2Coordsys(poCollHdr->m_nMaxX, poCollHdr->m_nMaxY, 
                            dXMax, dYMax);

    SetMBR(dXMin, dYMin, dXMax, dYMax);

    nCurCoordBlockPtr = poCollHdr->m_nCoordBlockPtr;
    poCoordBlock = poMapFile->GetCoordBlock(nCurCoordBlockPtr);

    /*-----------------------------------------------------------------
     * Region Component
     *----------------------------------------------------------------*/
    if(poCollHdr->m_nNumRegSections > 0)
    {
        //
        // Build fake coord section header to pass to TABRegion::ReadGeom...()
        //
        TABMAPObjPLine      oRegionHdr;

        oRegionHdr.m_nComprOrgX = poCollHdr->m_nComprOrgX;
        oRegionHdr.m_nComprOrgY = poCollHdr->m_nComprOrgY;

        //
        // The region section in the coord block starts with center/label
        // point + MBR that are normally found in the object data blocks 
        // of regular region objects.
        //
        ReadLabelAndMBR(poCoordBlock, bComprCoord,
                        oRegionHdr.m_nComprOrgX, oRegionHdr.m_nComprOrgY,
                        oRegionHdr.m_nMinX, oRegionHdr.m_nMinY,
                        oRegionHdr.m_nMaxX, oRegionHdr.m_nMaxY,
                        oRegionHdr.m_nLabelX, oRegionHdr.m_nLabelY);

        // Set CoordBlockPtr so that TABRegion continues reading here
        oRegionHdr.m_nCoordBlockPtr = poCoordBlock->GetCurAddress();

        if (bComprCoord)
            oRegionHdr.m_nType = TAB_GEOM_V450_REGION_C;
        else
            oRegionHdr.m_nType = TAB_GEOM_V450_REGION;
        oRegionHdr.m_numLineSections = poCollHdr->m_nNumRegSections;
        oRegionHdr.m_nPenId = poCollHdr->m_nRegionPenId;
        oRegionHdr.m_nBrushId = poCollHdr->m_nRegionBrushId;
        oRegionHdr.m_bSmooth = 0;       // TODO

        //
        // Use a TABRegion to read/store the Region coord data
        //
        m_poRegion = new TABRegion(GetDefnRef());
        if (m_poRegion->ReadGeometryFromMAPFile(poMapFile, &oRegionHdr, 
                                                &poCoordBlock) != 0)
            return -1;

        // Set new coord block ptr for next object
        if (poCoordBlock)
            nCurCoordBlockPtr = poCoordBlock->GetCurAddress();
    }


    /*-----------------------------------------------------------------
     * PLine Component
     *----------------------------------------------------------------*/
    if(poCollHdr->m_nNumPLineSections > 0)
    {
        //
        // Build fake coord section header to pass to TABPolyline::ReadGeom..()
        //
        TABMAPObjPLine      oPLineHdr;

        oPLineHdr.m_nComprOrgX = poCollHdr->m_nComprOrgX;
        oPLineHdr.m_nComprOrgY = poCollHdr->m_nComprOrgY;

        //
        // The pline section in the coord block starts with center/label
        // point + MBR that are normally found in the object data blocks 
        // of regular pline objects.
        //
        ReadLabelAndMBR(poCoordBlock, bComprCoord,
                        oPLineHdr.m_nComprOrgX, oPLineHdr.m_nComprOrgY,
                        oPLineHdr.m_nMinX, oPLineHdr.m_nMinY,
                        oPLineHdr.m_nMaxX, oPLineHdr.m_nMaxY,
                        oPLineHdr.m_nLabelX, oPLineHdr.m_nLabelY);

        // Set CoordBlockPtr so that TABRegion continues reading here
        oPLineHdr.m_nCoordBlockPtr = poCoordBlock->GetCurAddress();

        if (bComprCoord)
            oPLineHdr.m_nType = TAB_GEOM_V450_MULTIPLINE_C;
        else
            oPLineHdr.m_nType = TAB_GEOM_V450_MULTIPLINE;
        oPLineHdr.m_numLineSections = poCollHdr->m_nNumPLineSections;
        oPLineHdr.m_nPenId = poCollHdr->m_nPolylinePenId;
        oPLineHdr.m_bSmooth = 0;        // TODO

        //
        // Use a TABPolyline to read/store the Polyline coord data
        //
        m_poPline = new TABPolyline(GetDefnRef());
        if (m_poPline->ReadGeometryFromMAPFile(poMapFile, &oPLineHdr, 
                                               &poCoordBlock) != 0)
            return -1;

        // Set new coord block ptr for next object
        if (poCoordBlock)
            nCurCoordBlockPtr = poCoordBlock->GetCurAddress();
    }

    /*-----------------------------------------------------------------
     * MultiPoint Component
     *----------------------------------------------------------------*/
    if(poCollHdr->m_nNumMultiPoints > 0)
    {
        //
        // Build fake coord section header to pass to TABMultiPoint::ReadGeom()
        //
        TABMAPObjMultiPoint oMPointHdr;

        oMPointHdr.m_nComprOrgX = poCollHdr->m_nComprOrgX;
        oMPointHdr.m_nComprOrgY = poCollHdr->m_nComprOrgY;

        //
        // The pline section in the coord block starts with center/label
        // point + MBR that are normally found in the object data blocks 
        // of regular pline objects.
        //
        ReadLabelAndMBR(poCoordBlock, bComprCoord,
                        oMPointHdr.m_nComprOrgX, oMPointHdr.m_nComprOrgY,
                        oMPointHdr.m_nMinX, oMPointHdr.m_nMinY,
                        oMPointHdr.m_nMaxX, oMPointHdr.m_nMaxY,
                        oMPointHdr.m_nLabelX, oMPointHdr.m_nLabelY);

        // Set CoordBlockPtr so that TABRegion continues reading here
        oMPointHdr.m_nCoordBlockPtr = poCoordBlock->GetCurAddress();

        if (bComprCoord)
            oMPointHdr.m_nType = TAB_GEOM_MULTIPOINT_C;
        else
            oMPointHdr.m_nType = TAB_GEOM_MULTIPOINT;
        oMPointHdr.m_nNumPoints = poCollHdr->m_nNumMultiPoints;
        oMPointHdr.m_nSymbolId = poCollHdr->m_nMultiPointSymbolId;

        //
        // Use a TABMultiPoint to read/store the coord data
        //
        m_poMpoint = new TABMultiPoint(GetDefnRef());
        if (m_poMpoint->ReadGeometryFromMAPFile(poMapFile, &oMPointHdr) != 0)
            return -1;

        // Set new coord block ptr for next object (not really useful here)
        if (poCoordBlock)
            nCurCoordBlockPtr = poCoordBlock->GetCurAddress();
    }
    
    /*-----------------------------------------------------------------
     * Set the main OGRFeature Geometry 
     * (this is actually duplicating geometries from each member)
     *----------------------------------------------------------------*/
    if (SyncOGRGeometryCollection(TRUE, TRUE, TRUE) != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABCollection::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABCollection::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                          TABMAPObjHdr *poObjHdr)
{
    /*-----------------------------------------------------------------
     * Note that the current implementation does not allow setting the
     * Geometry via OGRFeature::SetGeometry(). The geometries must be set
     * via the SetRegion/Pline/MpointDirectly() methods which will take 
     * care of keeping the OGRFeature's geometry in sync.
     * 
     * TODO: If we ever want to support sync'ing changes from the OGRFeature's
     * geometry to the m_poRegion/Pline/Mpoint then a call should be added
     * here, or perhaps in ValidateMapInfoType(), or even better in 
     * custom TABCollection::SetGeometry*()... but then this last option
     * won't work unless OGRFeature::SetGeometry*() are made virtual in OGR.
     *----------------------------------------------------------------*/


    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    TABMAPObjCollection *poCollHdr = (TABMAPObjCollection *)poObjHdr;

    /*-----------------------------------------------------------------
     * Write data to coordinate block for each component...
     *
     * Note that at this point, the caller (TABFile) has called 
     * TABCollection::ValidateMapInfoType() which in turn has called
     * each component's respective ValidateMapInfoType() and
     * ForceCoordTypeAndCoordOrigin() so the objects are ready to have
     * their respective WriteGeometryToMapFile() called.
     *----------------------------------------------------------------*/
    TABMAPCoordBlock *poCoordBlock;
    GBool   bCompressed = poObjHdr->IsCompressedType();
    // TODO: ??? Do we need to track overall collection coord data size???
    int     nTotalFeatureDataSize = 0;

    poCoordBlock = poMapFile->GetCurCoordBlock();
    poCoordBlock->StartNewFeature();
    poCollHdr->m_nCoordBlockPtr = poCoordBlock->GetCurAddress();
    poCoordBlock->SetComprCoordOrigin(m_nComprOrgX, m_nComprOrgY);

    /*-----------------------------------------------------------------
     * Region component
     *----------------------------------------------------------------*/
    if (m_poRegion && m_poRegion->GetMapInfoType() != TAB_GEOM_NONE)
    {
        CPLAssert(m_poRegion->GetMapInfoType() == TAB_GEOM_V450_REGION ||
                  m_poRegion->GetMapInfoType() == TAB_GEOM_V450_REGION_C );

        TABMAPObjPLine *poRegionHdr = (TABMAPObjPLine *)
            TABMAPObjHdr::NewObj(m_poRegion->GetMapInfoType(), -1);

        // Update count of objects by type in header
        poMapFile->UpdateMapHeaderInfo(m_poRegion->GetMapInfoType());

        // Write a placeholder for centroid/label point and MBR mini-header
        // and we'll come back later to write the real values.
        //
        // Note that the call to WriteGeometryToMAPFile() below will call 
        // StartNewFeature() as well, so we need to track the current 
        // value before calling it

        poCoordBlock = poMapFile->GetCurCoordBlock();
        poCoordBlock->StartNewFeature();
        int nMiniHeaderPtr = poCoordBlock->GetCurAddress();

        WriteLabelAndMBR(poCoordBlock, bCompressed,
                         0, 0, 0, 0, 0, 0);
        nTotalFeatureDataSize += poCoordBlock->GetFeatureDataSize();

        if (m_poRegion->WriteGeometryToMAPFile(poMapFile, poRegionHdr) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed writing Region part in collection.");
            delete poRegionHdr;
            return -1;
        }

        nTotalFeatureDataSize += poRegionHdr->m_nCoordDataSize;

        // Come back to write the real values in the mini-header
        poCoordBlock = poMapFile->GetCurCoordBlock();
        int nEndOfObjectPtr = poCoordBlock->GetCurAddress();
        poCoordBlock->StartNewFeature();

        if (poCoordBlock->GotoByteInFile(nMiniHeaderPtr, TRUE) != 0)
        {
            delete poRegionHdr;
            return -1;
        }

        WriteLabelAndMBR(poCoordBlock, bCompressed,
                         poRegionHdr->m_nMinX, poRegionHdr->m_nMinY, 
                         poRegionHdr->m_nMaxX, poRegionHdr->m_nMaxY,
                         poRegionHdr->m_nLabelX, poRegionHdr->m_nLabelY);

        // And finally move the pointer back to the end of this component
        if (poCoordBlock->GotoByteInFile(nEndOfObjectPtr, TRUE) != 0)
        {
            delete poRegionHdr;
            return -1;
        }

        // Copy other header members to the main collection header
        // TODO: Does m_nRegionDataSize need to include the centroid+mbr
        //       mini-header???
        poCollHdr->m_nRegionDataSize = poRegionHdr->m_nCoordDataSize;
        poCollHdr->m_nNumRegSections = poRegionHdr->m_numLineSections;
        poCollHdr->m_nRegionPenId    = poRegionHdr->m_nPenId;
        poCollHdr->m_nRegionBrushId  = poRegionHdr->m_nBrushId;
        // TODO: Smooth flag         = poRegionHdr->m_bSmooth;
        
        delete poRegionHdr;
    }
    else
    {
        // No Region component. Set corresponding header fields to 0

        poCollHdr->m_nRegionDataSize = 0;
        poCollHdr->m_nNumRegSections = 0;
        poCollHdr->m_nTotalRegDataSize = 0;
        poCollHdr->m_nRegionPenId = 0;
        poCollHdr->m_nRegionBrushId = 0;
    }

    /*-----------------------------------------------------------------
     * PLine component
     *----------------------------------------------------------------*/
    if (m_poPline && m_poPline->GetMapInfoType() != TAB_GEOM_NONE)
    {
        CPLAssert(m_poPline->GetMapInfoType() == TAB_GEOM_V450_MULTIPLINE ||
                  m_poPline->GetMapInfoType() == TAB_GEOM_V450_MULTIPLINE_C );

        TABMAPObjPLine *poPlineHdr = (TABMAPObjPLine *)
            TABMAPObjHdr::NewObj(m_poPline->GetMapInfoType(), -1);

        // Update count of objects by type in header
        poMapFile->UpdateMapHeaderInfo(m_poPline->GetMapInfoType());

        // Write a placeholder for centroid/label point and MBR mini-header
        // and we'll come back later to write the real values.
        //
        // Note that the call to WriteGeometryToMAPFile() below will call 
        // StartNewFeature() as well, so we need to track the current 
        // value before calling it

        poCoordBlock = poMapFile->GetCurCoordBlock();
        poCoordBlock->StartNewFeature();
        int nMiniHeaderPtr = poCoordBlock->GetCurAddress();

        WriteLabelAndMBR(poCoordBlock, bCompressed,
                         0, 0, 0, 0, 0, 0);
        nTotalFeatureDataSize += poCoordBlock->GetFeatureDataSize();

        if (m_poPline->WriteGeometryToMAPFile(poMapFile, poPlineHdr) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed writing Region part in collection.");
            delete poPlineHdr;
            return -1;
        }

        nTotalFeatureDataSize += poPlineHdr->m_nCoordDataSize;

        // Come back to write the real values in the mini-header
        poCoordBlock = poMapFile->GetCurCoordBlock();
        int nEndOfObjectPtr = poCoordBlock->GetCurAddress();
        poCoordBlock->StartNewFeature();

        if (poCoordBlock->GotoByteInFile(nMiniHeaderPtr, TRUE) != 0)
        {
            delete poPlineHdr;
            return -1;
        }

        WriteLabelAndMBR(poCoordBlock, bCompressed,
                         poPlineHdr->m_nMinX, poPlineHdr->m_nMinY, 
                         poPlineHdr->m_nMaxX, poPlineHdr->m_nMaxY,
                         poPlineHdr->m_nLabelX, poPlineHdr->m_nLabelY);

        // And finally move the pointer back to the end of this component
        if (poCoordBlock->GotoByteInFile(nEndOfObjectPtr, TRUE) != 0)
        {
            delete poPlineHdr;
            return -1;
        }

        // Copy other header members to the main collection header
        // TODO: Does m_nRegionDataSize need to include the centroid+mbr
        //       mini-header???
        poCollHdr->m_nPolylineDataSize = poPlineHdr->m_nCoordDataSize;
        poCollHdr->m_nNumPLineSections = poPlineHdr->m_numLineSections;
        poCollHdr->m_nPolylinePenId    = poPlineHdr->m_nPenId;
        // TODO: Smooth flag           = poPlineHdr->m_bSmooth;

        delete poPlineHdr;        
    }
    else
    {
        // No Region component. Set corresponding header fields to 0

        poCollHdr->m_nPolylineDataSize = 0;
        poCollHdr->m_nNumPLineSections = 0;
        poCollHdr->m_nTotalPolyDataSize = 0;
        poCollHdr->m_nPolylinePenId = 0;
    }


    /*-----------------------------------------------------------------
     * MultiPoint component
     *----------------------------------------------------------------*/
    if (m_poMpoint && m_poMpoint->GetMapInfoType() != TAB_GEOM_NONE)
    {
        CPLAssert(m_poMpoint->GetMapInfoType() == TAB_GEOM_MULTIPOINT ||
                  m_poMpoint->GetMapInfoType() == TAB_GEOM_MULTIPOINT_C );

        TABMAPObjMultiPoint *poMpointHdr = (TABMAPObjMultiPoint *)
            TABMAPObjHdr::NewObj(m_poMpoint->GetMapInfoType(), -1);

        // Update count of objects by type in header
        poMapFile->UpdateMapHeaderInfo(m_poMpoint->GetMapInfoType());

        // Write a placeholder for centroid/label point and MBR mini-header
        // and we'll come back later to write the real values.
        //
        // Note that the call to WriteGeometryToMAPFile() below will call 
        // StartNewFeature() as well, so we need to track the current 
        // value before calling it

        poCoordBlock = poMapFile->GetCurCoordBlock();
        poCoordBlock->StartNewFeature();
        int nMiniHeaderPtr = poCoordBlock->GetCurAddress();

        WriteLabelAndMBR(poCoordBlock, bCompressed,
                         0, 0, 0, 0, 0, 0);
        nTotalFeatureDataSize += poCoordBlock->GetFeatureDataSize();

        if (m_poMpoint->WriteGeometryToMAPFile(poMapFile, poMpointHdr) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed writing Region part in collection.");
            delete poMpointHdr;
            return -1;
        }

        nTotalFeatureDataSize += poMpointHdr->m_nCoordDataSize;

        // Come back to write the real values in the mini-header
        poCoordBlock = poMapFile->GetCurCoordBlock();
        int nEndOfObjectPtr = poCoordBlock->GetCurAddress();
        poCoordBlock->StartNewFeature();

        if (poCoordBlock->GotoByteInFile(nMiniHeaderPtr, TRUE) != 0)
        {
            delete poMpointHdr;
            return -1;
        }

        WriteLabelAndMBR(poCoordBlock, bCompressed,
                         poMpointHdr->m_nMinX, poMpointHdr->m_nMinY, 
                         poMpointHdr->m_nMaxX, poMpointHdr->m_nMaxY,
                         poMpointHdr->m_nLabelX, poMpointHdr->m_nLabelY);

        // And finally move the pointer back to the end of this component
        if (poCoordBlock->GotoByteInFile(nEndOfObjectPtr, TRUE) != 0)
        {
            delete poMpointHdr;
            return -1;
        }

        // Copy other header members to the main collection header
        // TODO: Does m_nRegionDataSize need to include the centroid+mbr
        //       mini-header???
        poCollHdr->m_nMPointDataSize     = poMpointHdr->m_nCoordDataSize;
        poCollHdr->m_nNumMultiPoints     = poMpointHdr->m_nNumPoints;
        poCollHdr->m_nMultiPointSymbolId = poMpointHdr->m_nSymbolId;

        delete poMpointHdr;        
    }
    else
    {
        // No Region component. Set corresponding header fields to 0

        poCollHdr->m_nMPointDataSize = 0;
        poCollHdr->m_nNumMultiPoints = 0;
        poCollHdr->m_nMultiPointSymbolId = 0;
    }


    /*-----------------------------------------------------------------
     * Copy object information
     *----------------------------------------------------------------*/

    // Compressed coordinate origin (useful only in compressed case!)
    poCollHdr->m_nComprOrgX = m_nComprOrgX;
    poCollHdr->m_nComprOrgY = m_nComprOrgY;

    poCollHdr->m_nCoordDataSize = nTotalFeatureDataSize;

    poCollHdr->SetMBR(m_nXMin, m_nYMin, m_nXMax, m_nYMax);


    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *                   TABCollection::SyncOGRGeometryCollection()
 *
 * Copy the region/pline/multipoint's geometries to the OGRFeature's
 * geometry.
 **********************************************************************/
int    TABCollection::SyncOGRGeometryCollection(GBool bSyncRegion,
                                                GBool bSyncPline,
                                                GBool bSyncMpoint)
{
    OGRGeometry         *poThisGeom = GetGeometryRef();
    OGRGeometryCollection *poGeomColl;

    // poGeometry is defined in the OGRFeature class
    if (poThisGeom == NULL)
    {
        poThisGeom = poGeomColl = new OGRGeometryCollection();
        SetGeometryDirectly(poGeomColl);
    }
    else if (wkbFlatten(poThisGeom->getGeometryType())==wkbGeometryCollection)
    {
         poGeomColl = (OGRGeometryCollection *)poThisGeom;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABCollection: Invalid Geometry. Type must be OGRCollection.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Start by removing geometries that need to be replaced
     * In theory there should be a single geometry of each type, but 
     * just in case, we'll loop over the whole collection and delete all
     * instances of each type if there are some.
     *----------------------------------------------------------------*/
    int numGeometries = poGeomColl->getNumGeometries();
    for (int i=0; i<numGeometries; i++)
    {
        OGRGeometry *poGeom = poGeomColl->getGeometryRef(i);
        if (!poGeom)
            continue;

        if ( (bSyncRegion && 
              (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
               wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon) ) ||
             (bSyncPline && 
              (wkbFlatten(poGeom->getGeometryType()) == wkbLineString ||
               wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)) ||
             (bSyncMpoint && 
              (wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint) ) )
        {
            // Remove this geometry
            poGeomColl->removeGeometry(i);

            // Unless this was the last geometry, we need to restart
            // scanning the collection since we modified it
            if (i != numGeometries-1)
            {
                i=0;
                numGeometries = poGeomColl->getNumGeometries();
            }
        }
    }

    /*-----------------------------------------------------------------
     * Copy TAB Feature geometries to OGRGeometryCollection
     *----------------------------------------------------------------*/
    if(bSyncRegion && m_poRegion && m_poRegion->GetGeometryRef() != NULL)
        poGeomColl->addGeometry(m_poRegion->GetGeometryRef());
    
    if(bSyncPline && m_poPline && m_poPline->GetGeometryRef() != NULL)
        poGeomColl->addGeometry(m_poPline->GetGeometryRef());

    if(bSyncMpoint && m_poMpoint && m_poMpoint->GetGeometryRef() != NULL)
        poGeomColl->addGeometry(m_poMpoint->GetGeometryRef());

    return 0;
}


/**********************************************************************
 *                   TABCollection::SetRegionDirectly()
 *
 * Set the region component of the collection, deleting the current
 * region component if there is one. The object is then owned by the
 * TABCollection object. Passing NULL just deletes it.
 *
 * Note that an intentional side-effect is that calling this method
 * with the same poRegion pointer that is already owned by this object
 * will force resync'ing the OGR Geometry member.
 **********************************************************************/
int    TABCollection::SetRegionDirectly(TABRegion *poRegion)
{
    if (m_poRegion && m_poRegion != poRegion)
        delete m_poRegion;
    m_poRegion = poRegion;

    // Update OGRGeometryCollection component as well
    return SyncOGRGeometryCollection(TRUE, FALSE, FALSE);
}

/**********************************************************************
 *                   TABCollection::SetPolylineDirectly()
 *
 * Set the polyline component of the collection, deleting the current
 * polyline component if there is one. The object is then owned by the
 * TABCollection object. Passing NULL just deletes it.
 *
 * Note that an intentional side-effect is that calling this method
 * with the same poPline pointer that is already owned by this object
 * will force resync'ing the OGR Geometry member.
 **********************************************************************/
int    TABCollection::SetPolylineDirectly(TABPolyline *poPline)
{
    if (m_poPline && m_poPline != poPline)
        delete m_poPline;
    m_poPline = poPline;

    // Update OGRGeometryCollection component as well
    return SyncOGRGeometryCollection(FALSE, TRUE, FALSE);
}

/**********************************************************************
 *                   TABCollection::SetMultiPointDirectly()
 *
 * Set the multipoint component of the collection, deleting the current
 * multipoint component if there is one. The object is then owned by the
 * TABCollection object. Passing NULL just deletes it.
 *
 * Note that an intentional side-effect is that calling this method
 * with the same poMpoint pointer that is already owned by this object
 * will force resync'ing the OGR Geometry member.
 **********************************************************************/
int    TABCollection::SetMultiPointDirectly(TABMultiPoint *poMpoint)
{
    if (m_poMpoint && m_poMpoint != poMpoint)
        delete m_poMpoint;
    m_poMpoint = poMpoint;

    // Update OGRGeometryCollection component as well
    return SyncOGRGeometryCollection(FALSE, FALSE, TRUE);
}


/**********************************************************************
 *                   TABCollection::GetStyleString()
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABCollection::GetStyleString()
{
    if (m_pszStyleString == NULL)
    {
        m_pszStyleString = CPLStrdup(GetSymbolStyleString());
    }

    return m_pszStyleString;
}


/**********************************************************************
 *                   TABCollection::DumpMIF()
 *
 * Dump feature geometry 
 **********************************************************************/
void TABCollection::DumpMIF(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Generate output
     *----------------------------------------------------------------*/
    int numParts = 0;
    if (m_poRegion)     numParts++;
    if (m_poPline)      numParts++;
    if (m_poMpoint)     numParts++;

    fprintf(fpOut, "COLLECTION %d\n", numParts);

    if (m_poRegion)
        m_poRegion->DumpMIF(fpOut);

    if (m_poPline)
        m_poPline->DumpMIF(fpOut);

    if (m_poMpoint)
        m_poMpoint->DumpMIF(fpOut);


    DumpSymbolDef(fpOut);

    fflush(fpOut);
}

/*=====================================================================
 *                      class TABDebugFeature
 *====================================================================*/

/**********************************************************************
 *                   TABDebugFeature::TABDebugFeature()
 *
 * Constructor.
 **********************************************************************/
TABDebugFeature::TABDebugFeature(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
}

/**********************************************************************
 *                   TABDebugFeature::~TABDebugFeature()
 *
 * Destructor.
 **********************************************************************/
TABDebugFeature::~TABDebugFeature()
{
}

/**********************************************************************
 *                   TABDebugFeature::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABDebugFeature::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                             TABMAPObjHdr *poObjHdr)
{
    TABMAPObjectBlock   *poObjBlock;
    TABMAPHeaderBlock   *poHeader;

    /*-----------------------------------------------------------------
     * Fetch geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();
    poHeader = poMapFile->GetHeaderBlock();

    /*-----------------------------------------------------------------
     * If object type has coords in a type 3 block, then its position 
     * follows
     *----------------------------------------------------------------*/
    if (poHeader->MapObjectUsesCoordBlock(m_nMapInfoType))
    {
        m_nCoordDataPtr = poObjBlock->ReadInt32();
        m_nCoordDataSize = poObjBlock->ReadInt32();
    }
    else
    {
        m_nCoordDataPtr = -1;
        m_nCoordDataSize = 0;
    }

    m_nSize = poHeader->GetMapObjectSize(m_nMapInfoType);
    if (m_nSize > 0)
    {
        poObjBlock->GotoByteRel(-5);    // Go back to beginning of header
        poObjBlock->ReadBytes(m_nSize, m_abyBuf);
    }

    return 0;
}

/**********************************************************************
 *                   TABDebugFeature::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABDebugFeature::WriteGeometryToMAPFile(TABMAPFile * /*poMapFile*/,
                                            TABMAPObjHdr * /*poObjHdr*/)
{
    // Nothing to do here!

    CPLError(CE_Failure, CPLE_NotSupported,
             "TABDebugFeature::WriteGeometryToMAPFile() not implemented.\n");

    return -1;
}

/**********************************************************************
 *                   TABDebugFeature::DumpMIF()
 *
 * Dump feature contents... available only in DEBUG mode.
 **********************************************************************/
void TABDebugFeature::DumpMIF(FILE *fpOut /*=NULL*/)
{
    int i;

    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "----- TABDebugFeature (type = 0x%2.2x) -----\n",
            GetMapInfoType());
    fprintf(fpOut, "  Object size: %d bytes\n", m_nSize);
    fprintf(fpOut, "  m_nCoordDataPtr  = %d\n", m_nCoordDataPtr);
    fprintf(fpOut, "  m_nCoordDataSize = %d\n", m_nCoordDataSize);
    fprintf(fpOut, "  ");

    for(i=0; i<m_nSize; i++)
        fprintf(fpOut, " %2.2x", m_abyBuf[i]);

    fprintf(fpOut, "  \n");


    fflush(fpOut);
}


/*=====================================================================
 *                      class ITABFeaturePen
 *====================================================================*/

/**********************************************************************
 *                   ITABFeaturePen::ITABFeaturePen()
 **********************************************************************/

ITABFeaturePen::ITABFeaturePen()
{
    static const TABPenDef csDefaultPen = MITAB_PEN_DEFAULT;

    m_nPenDefIndex=-1;

    /* MI default is PEN(1,2,0) */
    m_sPenDef = csDefaultPen;
}


/**********************************************************************
 *                   ITABFeaturePen::GetPenWidthPixel()
 *                   ITABFeaturePen::SetPenWidthPixel()
 *                   ITABFeaturePen::GetPenWidthPoint()
 *                   ITABFeaturePen::SetPenWidthPoint()
 *
 * Pen width can be expressed in pixels (value from 1 to 7 pixels) or 
 * in points (value from 0.1 to 203.7 points). The default pen width 
 * in MapInfo is 1 pixel.  Pen width in points exist only in file version 450.
 *
 * The following methods hide the way the pen width is stored in the files.
 *
 * In order to establish if a given pen def had its width specified in 
 * pixels or in points, one should first call GetPenWidthPoint(), and if
 * it returns 0 then the Pixel width should be used instead:
 *    if (GetPenWidthPoint() == 0)
 *       ... use pen width in points ...
 *    else
 *       ... use Pixel width from GetPenWidthPixel()
 *
 * Note that the reverse is not true: the default pixel width is always 1, 
 * even when the pen width was actually set in points.
 **********************************************************************/

GByte ITABFeaturePen::GetPenWidthPixel()
{
    return m_sPenDef.nPixelWidth;
}

void  ITABFeaturePen::SetPenWidthPixel(GByte val) 
{
    m_sPenDef.nPixelWidth = MIN(MAX(val, 1), 7);
    m_sPenDef.nPointWidth = 0;
}

double ITABFeaturePen::GetPenWidthPoint()
{
    // We store point width internally as tenths of points
    return m_sPenDef.nPointWidth/10.0;
}

void  ITABFeaturePen::SetPenWidthPoint(double val) 
{
    m_sPenDef.nPointWidth = MIN(MAX(((int)(val*10)), 1), 2037);
    m_sPenDef.nPixelWidth = 1;
}

/**********************************************************************
 *                   ITABFeaturePen::GetPenWidthMIF()
 *                   ITABFeaturePen::SetPenWidthMIF()
 *
 * The MIF representation for pen width is either a value from 1 to 7
 * for a pen width in pixels, or a value from 11 to 2047 for a pen
 * width in points = 10 + (point_width*10)
 **********************************************************************/
int     ITABFeaturePen::GetPenWidthMIF()
{
    return ( m_sPenDef.nPointWidth > 0?
             (m_sPenDef.nPointWidth+10): m_sPenDef.nPixelWidth );
}

void  ITABFeaturePen::SetPenWidthMIF(int val) 
{
    if (val > 10)
    {
        m_sPenDef.nPointWidth = MIN((val-10), 2037);
        m_sPenDef.nPixelWidth = 0;
    }
    else
    {
        m_sPenDef.nPixelWidth = (GByte)MIN(MAX(val, 1), 7);
        m_sPenDef.nPointWidth = 0;
    }
}

/**********************************************************************
 *                   ITABFeaturePen::GetPenStyleString()
 *
 *  Return a PEN() string. All representations info for the pen are here.
 **********************************************************************/
const char *ITABFeaturePen::GetPenStyleString()
{
    const char *pszStyle = NULL;
    int    nOGRStyle  = 0;
    char szPattern[20];
    
    szPattern[0] = '\0';

    // For now, I only add the 25 first styles 
    switch (GetPenPattern())
    {
      case 1:
        nOGRStyle =1; 
        break;
      case 2:
        nOGRStyle = 0;
        break;
      case 3:
        nOGRStyle = 3; 
        strcpy(szPattern,"1 1");
        break;
      case 4:
        nOGRStyle = 3;
        strcpy(szPattern,"2 1");
        break;
      case 5:
        nOGRStyle = 3;
        strcpy(szPattern,"3 1");
        break;
      case 6:
        nOGRStyle = 3;
        strcpy(szPattern,"6 1");
        break;
      case 7:
        nOGRStyle = 4;
        strcpy(szPattern,"12 2");
        break;
      case 8:
        nOGRStyle = 4;
        strcpy(szPattern,"24 4");
        break;
      case 9:
        nOGRStyle = 3;
        strcpy(szPattern,"4 3");
        break;
      case 10:
        nOGRStyle = 5;
        strcpy(szPattern,"1 4");
        break;
      case 11:
        nOGRStyle = 3;
        strcpy(szPattern,"4 6");
        break;
      case 12:
        nOGRStyle = 3;
        strcpy(szPattern,"6 4");
        break;
      case 13:
        nOGRStyle = 4;
        strcpy(szPattern,"12 12");
        break;
      case 14:
        nOGRStyle = 6;
        strcpy(szPattern,"8 2 1 2");
        break;
      case 15:
        nOGRStyle = 6;
        strcpy(szPattern,"12 1 1 1");
        break;
      case 16:
        nOGRStyle = 6;
        strcpy(szPattern,"12 1 3 1");
        break;
      case 17:
        nOGRStyle = 6;
        strcpy(szPattern,"24 6 4 6");
        break;
      case 18:
        nOGRStyle = 7;
        strcpy(szPattern,"24 3 3 3 3 3");
        break;
      case 19:
        nOGRStyle = 7;
        strcpy(szPattern,"24 3 3 3 3 3 3 3");
        break;
      case 20:
        nOGRStyle = 7;
        strcpy(szPattern,"6 3 1 3 1 3");
        break;
      case 21:
        nOGRStyle = 7;
        strcpy(szPattern,"12 2 1 2 1 2");
        break;
      case 22:
        nOGRStyle = 7;
        strcpy(szPattern,"12 2 1 2 1 2 1 2");
        break;
      case 23:
        nOGRStyle = 6;
        strcpy(szPattern,"4 1 1 1");
        break;
      case 24:
        nOGRStyle = 7;
        strcpy(szPattern,"4 1 1 1 1");
        break;
      case 25:
        nOGRStyle = 6;
        strcpy(szPattern,"4 1 1 1 2 1 1 1");
        break;

        default:
        nOGRStyle = 0;
        break;
    }

    if (strlen(szPattern) != 0)
    {
      if(m_sPenDef.nPointWidth > 0)
        pszStyle =CPLSPrintf("PEN(w:%dpt,c:#%6.6x,id:\"mapinfo-pen-%d."
                             "ogr-pen-%d\",p:\"%spx\")",
                             ((int)GetPenWidthPoint()),
                             m_sPenDef.rgbColor,GetPenPattern(),nOGRStyle,
                             szPattern);
      else
        pszStyle =CPLSPrintf("PEN(w:%dpx,c:#%6.6x,id:\"mapinfo-pen-%d."
                             "ogr-pen-%d\",p:\"%spx\")",
                             GetPenWidthPixel(),
                             m_sPenDef.rgbColor,GetPenPattern(),nOGRStyle,
                             szPattern);
    }
    else
    {
      if(m_sPenDef.nPointWidth > 0)
        pszStyle =CPLSPrintf("PEN(w:%dpt,c:#%6.6x,id:\""
                             "mapinfo-pen-%d.ogr-pen-%d\")",
                             ((int)GetPenWidthPoint()),
                             m_sPenDef.rgbColor,GetPenPattern(),nOGRStyle);
      else
        pszStyle =CPLSPrintf("PEN(w:%dpx,c:#%6.6x,id:\""
                             "mapinfo-pen-%d.ogr-pen-%d\")",
                             GetPenWidthPixel(),
                             m_sPenDef.rgbColor,GetPenPattern(),nOGRStyle);
    }

    return pszStyle;
}

/**********************************************************************
 *                   ITABFeaturePen::SetPenFromStyleString()
 *
 *  Init the Pen properties from a style string.
 **********************************************************************/
void  ITABFeaturePen::SetPenFromStyleString(const char *pszStyleString)
{
    int numParts, i;
    GBool bIsNull;

    const char *pszPenName, *pszPenPattern;

    double nPenWidth;

    GInt32 nPenColor;
    const char *pszPenColor;

    int   nPenId;
    char* pszPenId;

    // Use the Style Manager to retreive all the information we need.
    OGRStyleMgr *poStyleMgr = new OGRStyleMgr(NULL);
    OGRStyleTool *poStylePart;

    // Init the StyleMgr with the StyleString.
    poStyleMgr->InitStyleString(pszStyleString);

    // Retreive the Pen info.
    numParts = poStyleMgr->GetPartCount();
    for(i=0; i<numParts; i++)
    {
        poStylePart = poStyleMgr->GetPart(i);

        if(poStylePart->GetType() == OGRSTCPen)
        {
            break;
        }
    }

    // If the no Pen found, do nothing.
    if(i >= numParts)
        return;

    OGRStylePen *poPenStyle = (OGRStylePen*)poStylePart;

    // With Pen, we always want to output points or pixels (which are the same,
    // so just use points).
    //
    // It's very important to set the output unit of the feature.
    // The default value is meter. If we don't do it all numerical values 
    // will be assumed to be converted from the input unit to meter when we 
    // will get them via GetParam...() functions.
    // See OGRStyleTool::Parse() for more details.
    poPenStyle->SetUnit(OGRSTUPoints, 1);

    // Get the Pen Id or pattern
    pszPenName = poPenStyle->Id(bIsNull);
    if (bIsNull) pszPenName = NULL;

    // Set the width
    if(poPenStyle->Width(bIsNull))
    {
        nPenWidth = poPenStyle->Width(bIsNull);
        // Width < 10 is a pixel
        if(nPenWidth > 10)
            SetPenWidthPoint(nPenWidth);
        else
            SetPenWidthPixel((GByte)nPenWidth);
    }

    //Set the color
    pszPenColor = poPenStyle->Color(bIsNull);
    if(pszPenColor != NULL)
    {
        if(pszPenColor[0] == '#')
            pszPenColor++;
        // The Pen color is an Hexa string that need to be convert in a int
        nPenColor = strtol(pszPenColor, NULL, 16);
        SetPenColor(nPenColor);
    }

    // Set the Id of the Pen, use Pattern if necessary.
    if(pszPenName && strstr(pszPenName, "mapinfo-pen-") ||
       strstr(pszPenName, "ogr-pen-"))
    {
        if((pszPenId = (char *) strstr(pszPenName, "mapinfo-pen-")))
        {
            nPenId = atoi(pszPenId+12);
            SetPenPattern(nPenId);
        }
        else if((pszPenId = (char *) strstr(pszPenName, "ogr-pen-")))
        {
            nPenId = atoi(pszPenId+8);
            if(nPenId == 0)
                nPenId = 2;
            SetPenPattern(nPenId);
        }
    }
    else
    {
        // If no Pen Id, use the Pen Pattern to retreive the Id.
        pszPenPattern = poPenStyle->Pattern(bIsNull);
        if(strcmp(pszPenPattern, "1 1") == 0)
            SetPenPattern(3);
        else if(strcmp(pszPenPattern, "2 1") == 0)
            SetPenPattern(4);
        else if(strcmp(pszPenPattern, "3 1") == 0)
            SetPenPattern(5);
        else if(strcmp(pszPenPattern, "6 1") == 0)
            SetPenPattern(6);
        else if(strcmp(pszPenPattern, "12 2") == 0)
            SetPenPattern(7);
        else if(strcmp(pszPenPattern, "24 4") == 0)
            SetPenPattern(8);
        else if(strcmp(pszPenPattern, "4 3") == 0)
            SetPenPattern(9);
        else if(strcmp(pszPenPattern, "1 4") == 0)
            SetPenPattern(10);
        else if(strcmp(pszPenPattern, "4 6") == 0)
            SetPenPattern(11);
        else if(strcmp(pszPenPattern, "6 4") == 0)
            SetPenPattern(12);
        else if(strcmp(pszPenPattern, "12 12") == 0)
            SetPenPattern(13);
        else if(strcmp(pszPenPattern, "8 2 1 2") == 0)
            SetPenPattern(14);
        else if(strcmp(pszPenPattern, "12 1 1 1") == 0)
            SetPenPattern(15);
        else if(strcmp(pszPenPattern, "12 1 3 1") == 0)
            SetPenPattern(16);
        else if(strcmp(pszPenPattern, "24 6 4 6") == 0)
            SetPenPattern(17);
        else if(strcmp(pszPenPattern, "24 3 3 3 3 3") == 0)
            SetPenPattern(18);
        else if(strcmp(pszPenPattern, "24 3 3 3 3 3 3 3") == 0)
            SetPenPattern(19);
        else if(strcmp(pszPenPattern, "6 3 1 3 1 3") == 0)
            SetPenPattern(20);
        else if(strcmp(pszPenPattern, "12 2 1 2 1 2") == 0)
            SetPenPattern(21);
        else if(strcmp(pszPenPattern, "12 2 1 2 1 2 1 2") == 0)
            SetPenPattern(22);
        else if(strcmp(pszPenPattern, "4 1 1 1") == 0)
            SetPenPattern(23);
        else if(strcmp(pszPenPattern, "4 1 1 1 1") == 0)
            SetPenPattern(24);
        else if(strcmp(pszPenPattern, "4 1 1 1 2 1 1 1") == 0)
            SetPenPattern(25);
    }

    return;
}

/**********************************************************************
 *                   ITABFeaturePen::DumpPenDef()
 *
 * Dump pen definition information.
 **********************************************************************/
void ITABFeaturePen::DumpPenDef(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "  m_nPenDefIndex         = %d\n", m_nPenDefIndex);
    fprintf(fpOut, "  m_sPenDef.nRefCount    = %d\n", m_sPenDef.nRefCount);
    fprintf(fpOut, "  m_sPenDef.nPixelWidth  = %d\n", m_sPenDef.nPixelWidth);
    fprintf(fpOut, "  m_sPenDef.nLinePattern = %d\n", m_sPenDef.nLinePattern);
    fprintf(fpOut, "  m_sPenDef.nPointWidth  = %d\n", m_sPenDef.nPointWidth);
    fprintf(fpOut, "  m_sPenDef.rgbColor     = 0x%6.6x (%d)\n",
                                     m_sPenDef.rgbColor, m_sPenDef.rgbColor);

    fflush(fpOut);
}

/*=====================================================================
 *                      class ITABFeatureBrush
 *====================================================================*/

/**********************************************************************
 *                   ITABFeatureBrush::ITABFeatureBrush()
 **********************************************************************/

ITABFeatureBrush::ITABFeatureBrush()
{
    static const TABBrushDef csDefaultBrush = MITAB_BRUSH_DEFAULT;

    m_nBrushDefIndex=-1;

    /* MI default is BRUSH(2,16777215,16777215) */
    m_sBrushDef = csDefaultBrush;
}


/**********************************************************************
 *                   ITABFeatureBrush::GetBrushStyleString()
 *
 *  Return a Brush() string. All representations info for the Brush are here.
 **********************************************************************/
const char *ITABFeatureBrush::GetBrushStyleString()
{
    const char *pszStyle = NULL;
    int    nOGRStyle  = 0;
    char szPattern[20];
    
    szPattern[0] = '\0';
    
    if (m_sBrushDef.nFillPattern == 1)
      nOGRStyle = 1;
    else if (m_sBrushDef.nFillPattern == 3)
      nOGRStyle = 2;
    else if (m_sBrushDef.nFillPattern == 4)
      nOGRStyle = 3;
    else if (m_sBrushDef.nFillPattern == 5)
      nOGRStyle = 5;
    else if (m_sBrushDef.nFillPattern == 6)
      nOGRStyle = 4;
    else if (m_sBrushDef.nFillPattern == 7)
      nOGRStyle = 6;
    else if (m_sBrushDef.nFillPattern == 8)
      nOGRStyle = 7;
          

    if (GetBrushTransparent())
    {
        /* Omit BG Color for transparent brushes */
        pszStyle =CPLSPrintf("BRUSH(fc:#%6.6x,id:\"mapinfo-brush-%d.ogr-brush-%d\")",
                             m_sBrushDef.rgbFGColor,
                             m_sBrushDef.nFillPattern,nOGRStyle);
    }
    else
    {
        pszStyle =CPLSPrintf("BRUSH(fc:#%6.6x,bc:#%6.6x,id:\"mapinfo-brush-%d.ogr-brush-%d\")",
                             m_sBrushDef.rgbFGColor,
                             m_sBrushDef.rgbBGColor,
                             m_sBrushDef.nFillPattern,nOGRStyle);
    }

     return pszStyle;
    
}  


/**********************************************************************
 *                   ITABFeatureBrush::SetBrushFromStyleString()
 *
 *  Set all Brush elements from a StyleString.
 *  Use StyleMgr to do so.
 **********************************************************************/
void  ITABFeatureBrush::SetBrushFromStyleString(const char *pszStyleString)
{
    int numParts, i;
    GBool bIsNull;

    const char *pszBrushId;
    int nBrushId;

    const char *pszBrushColor;
    int nBrushColor;

    // Use the Style Manager to retreive all the information we need.
    OGRStyleMgr *poStyleMgr = new OGRStyleMgr(NULL);
    OGRStyleTool *poStylePart;

    // Init the StyleMgr with the StyleString.
    poStyleMgr->InitStyleString(pszStyleString);

    // Retreive the Brush info.
    numParts = poStyleMgr->GetPartCount();
    for(i=0; i<numParts; i++)
    {
        poStylePart = poStyleMgr->GetPart(i);

        if(poStylePart->GetType() == OGRSTCBrush)
        {
            break;
        }
    }

    // If the no Brush found, do nothing.
    if(i >= numParts)
        return;

    OGRStyleBrush *poBrushStyle = (OGRStyleBrush*)poStylePart;

    // Set the Brush Id (FillPattern)
    pszBrushId = poBrushStyle->Id(bIsNull);
    if(bIsNull) pszBrushId = NULL;

    if(pszBrushId && strstr(pszBrushId, "mapinfo-brush-") || 
       strstr(pszBrushId, "ogr-brush-"))
    {
        if(strstr(pszBrushId, "mapinfo-brush-"))
        {
            nBrushId = atoi(pszBrushId+14);
            SetBrushPattern((GByte)nBrushId);
        }
        else if(strstr(pszBrushId, "ogr-brush-"))
        {
            nBrushId = atoi(pszBrushId+10);
            if(nBrushId > 1)
                nBrushId++;
            SetBrushPattern((GByte)nBrushId);
        }
    }

    // Set the BackColor, if not set, then it's transparent
    pszBrushColor = poBrushStyle->BackColor(bIsNull);
    if(pszBrushColor)
    {
        if(pszBrushColor[0] == '#')
            pszBrushColor++;
        nBrushColor = strtol(pszBrushColor, NULL, 16);
        SetBrushBGColor((GInt32)nBrushColor);
    }
    else
    {
        SetBrushTransparent(1);
    }

    // Set the ForeColor
    pszBrushColor = poBrushStyle->ForeColor(bIsNull);
    if(pszBrushColor)
    {
        if(pszBrushColor[0] == '#')
            pszBrushColor++;
        nBrushColor = strtol(pszBrushColor, NULL, 16);
        SetBrushFGColor((GInt32)nBrushColor);
    }

     return;
    
}  

/**********************************************************************
 *                   ITABFeatureBrush::DumpBrushDef()
 *
 * Dump Brush definition information.
 **********************************************************************/
void ITABFeatureBrush::DumpBrushDef(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "  m_nBrushDefIndex         = %d\n", m_nBrushDefIndex);
    fprintf(fpOut, "  m_sBrushDef.nRefCount    = %d\n", m_sBrushDef.nRefCount);
    fprintf(fpOut, "  m_sBrushDef.nFillPattern = %d\n", 
                                                (int)m_sBrushDef.nFillPattern);
    fprintf(fpOut, "  m_sBrushDef.bTransparentFill = %d\n", 
                                            (int)m_sBrushDef.bTransparentFill);
    fprintf(fpOut, "  m_sBrushDef.rgbFGColor   = 0x%6.6x (%d)\n",
                               m_sBrushDef.rgbFGColor, m_sBrushDef.rgbFGColor);
    fprintf(fpOut, "  m_sBrushDef.rgbBGColor   = 0x%6.6x (%d)\n",
                               m_sBrushDef.rgbBGColor, m_sBrushDef.rgbBGColor);

    fflush(fpOut);
}

/*=====================================================================
 *                      class ITABFeatureFont
 *====================================================================*/

/**********************************************************************
 *                   ITABFeatureFont::ITABFeatureFont()
 **********************************************************************/

ITABFeatureFont::ITABFeatureFont()
{
    static const TABFontDef csDefaultFont = MITAB_FONT_DEFAULT;

    m_nFontDefIndex=-1;

    /* MI default is Font("Arial",0,0,0) */
    m_sFontDef = csDefaultFont;
}

/**********************************************************************
 *                   ITABFeatureFont::DumpFontDef()
 *
 * Dump Font definition information.
 **********************************************************************/
void ITABFeatureFont::DumpFontDef(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "  m_nFontDefIndex       = %d\n", m_nFontDefIndex);
    fprintf(fpOut, "  m_sFontDef.nRefCount  = %d\n", m_sFontDef.nRefCount);
    fprintf(fpOut, "  m_sFontDef.szFontName = '%s'\n", m_sFontDef.szFontName);

    fflush(fpOut);
}


/*=====================================================================
 *                      class ITABFeatureSymbol
 *====================================================================*/

/**********************************************************************
 *                   ITABFeatureSymbol::ITABFeatureSymbol()
 **********************************************************************/

ITABFeatureSymbol::ITABFeatureSymbol()
{
    static const TABSymbolDef csDefaultSymbol = MITAB_SYMBOL_DEFAULT;

    m_nSymbolDefIndex=-1;

    /* MI default is Symbol(35,0,12) */
    m_sSymbolDef = csDefaultSymbol;
}

/**********************************************************************
 *                   ITABFeatureSymbol::GetSymbolStyleString()
 *
 *  Return a Symbol() string. All representations info for the Symbol are here.
 **********************************************************************/
const char *ITABFeatureSymbol::GetSymbolStyleString(double dfAngle)
{
    const char *pszStyle = NULL;
    int    nOGRStyle  = 1;
    char szPattern[20];
    int nAngle = 0;
    
    szPattern[0] = '\0';
    
    if (m_sSymbolDef.nSymbolNo == 31)
      nOGRStyle = 0;
    else if (m_sSymbolDef.nSymbolNo == 32)
      nOGRStyle = 6;
    else if (m_sSymbolDef.nSymbolNo == 33)
    {
        nAngle = 45;
        nOGRStyle = 6;
    }
    else if (m_sSymbolDef.nSymbolNo == 34)
      nOGRStyle = 4;
    else if (m_sSymbolDef.nSymbolNo == 35)
      nOGRStyle = 10;
    else if (m_sSymbolDef.nSymbolNo == 36)
      nOGRStyle = 8;
    else if (m_sSymbolDef.nSymbolNo == 37)
    {
        nAngle = 180;
        nOGRStyle = 8;
    }
    else if (m_sSymbolDef.nSymbolNo == 38)
      nOGRStyle = 5;
    else if (m_sSymbolDef.nSymbolNo == 39)
    {
        nAngle = 45;
        nOGRStyle = 5;
    }
    else if (m_sSymbolDef.nSymbolNo == 40)
      nOGRStyle = 3;
    else if (m_sSymbolDef.nSymbolNo == 41)
      nOGRStyle = 9;
    else if (m_sSymbolDef.nSymbolNo == 42)
      nOGRStyle = 7;
    else if (m_sSymbolDef.nSymbolNo == 43)
    {
        nAngle = 180;
        nOGRStyle = 7;
    }
    else if (m_sSymbolDef.nSymbolNo == 44)
      nOGRStyle = 6;
    else if (m_sSymbolDef.nSymbolNo == 45)
      nOGRStyle = 8;
    else if (m_sSymbolDef.nSymbolNo == 46)
      nOGRStyle = 4;
    else if (m_sSymbolDef.nSymbolNo == 49)
      nOGRStyle = 1;
    else if (m_sSymbolDef.nSymbolNo == 50)
      nOGRStyle = 2;

    nAngle += (int)dfAngle;
    
    pszStyle=CPLSPrintf("SYMBOL(a:%d,c:#%6.6x,s:%dpt,id:\"mapinfo-sym-%d.ogr-sym-%d\")",
                        nAngle,
                        m_sSymbolDef.rgbColor,
                        m_sSymbolDef.nPointSize,
                        m_sSymbolDef.nSymbolNo,
                        nOGRStyle);
     
    return pszStyle;
    
}  

/**********************************************************************
 *                   ITABFeatureSymbol::SetSymbolFromStyleString()
 *
 *  Set all Symbol var from a StyleString. Use StyleMgr to do so.
 **********************************************************************/
void ITABFeatureSymbol::SetSymbolFromStyleString(const char *pszStyleString)
{
    int numParts, i;
    GBool bIsNull;

    const char *pszSymbolId;
    int nSymbolId;

    const char *pszSymbolColor;
    int nSymbolColor;

    double dSymbolSize;

    // Use the Style Manager to retreive all the information we need.
    OGRStyleMgr *poStyleMgr = new OGRStyleMgr(NULL);
    OGRStyleTool *poStylePart;

    // Init the StyleMgr with the StyleString.
    poStyleMgr->InitStyleString(pszStyleString);

    // Retreive the Symbol info.
    numParts = poStyleMgr->GetPartCount();
    for(i=0; i<numParts; i++)
    {
        poStylePart = poStyleMgr->GetPart(i);

        if(poStylePart->GetType() == OGRSTCSymbol)
        {
            break;
        }
    }

    // If the no Symbol found, do nothing.
    if(i >= numParts)
        return;

    OGRStyleSymbol *poSymbolStyle = (OGRStyleSymbol*)poStylePart;

    // With Symbol, we always want to output points
    //
    // It's very important to set the output unit of the feature.
    // The default value is meter. If we don't do it all numerical values 
    // will be assumed to be converted from the input unit to meter when we 
    // will get them via GetParam...() functions.
    // See OGRStyleTool::Parse() for more details.
    poSymbolStyle->SetUnit(OGRSTUPoints, (72.0 * 39.37));

    // Set the Symbol Id (SymbolNo)
    pszSymbolId = poSymbolStyle->Id(bIsNull);
    if(bIsNull) pszSymbolId = NULL;

    if(pszSymbolId && strstr(pszSymbolId, "mapinfo-sym-") || 
       strstr(pszSymbolId, "ogr-sym-"))
    {
        if(strstr(pszSymbolId, "mapinfo-sym-"))
        {
            nSymbolId = atoi(pszSymbolId+12);
            SetSymbolNo((GByte)nSymbolId);
        }
        else if(strstr(pszSymbolId, "ogr-sym-"))
        {
            nSymbolId = atoi(pszSymbolId+8);

            // The OGR symbol is not the MapInfo one
            // Here's some mapping
            switch (nSymbolId)
            {
              case 0:
                SetSymbolNo(31);
                break;
              case 1:
                SetSymbolNo(49);
                break;
              case 2:
                SetSymbolNo(50);
                break;
              case 3:
                SetSymbolNo(40);
                break;
              case 4:
                SetSymbolNo(34);
                break;
              case 5:
                SetSymbolNo(38);
                break;
              case 6:
                SetSymbolNo(32);
                break;
              case 7:
                SetSymbolNo(42);
                break;
              case 8:
                SetSymbolNo(36);
                break;
              case 9:
                SetSymbolNo(41);
                break;
              case 10:
                SetSymbolNo(35);
                break;
            }
        }
    }

    // Set SymbolSize
    dSymbolSize = poSymbolStyle->Size(bIsNull);
    if(dSymbolSize)
    {
        SetSymbolSize((GInt32)dSymbolSize);
    }

    // Set Symbol Color
    pszSymbolColor = poSymbolStyle->Color(bIsNull);
    if(pszSymbolColor)
    {
        if(pszSymbolColor[0] == '#')
            pszSymbolColor++;
        nSymbolColor = strtol(pszSymbolColor, NULL, 16);
        SetSymbolColor((GInt32)nSymbolColor);
    }
    
    return;
    
}  

/**********************************************************************
 *                   ITABFeatureSymbol::DumpSymbolDef()
 *
 * Dump Symbol definition information.
 **********************************************************************/
void ITABFeatureSymbol::DumpSymbolDef(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "  m_nSymbolDefIndex       = %d\n", m_nSymbolDefIndex);
    fprintf(fpOut, "  m_sSymbolDef.nRefCount  = %d\n", m_sSymbolDef.nRefCount);
    fprintf(fpOut, "  m_sSymbolDef.nSymbolNo  = %d\n", m_sSymbolDef.nSymbolNo);
    fprintf(fpOut, "  m_sSymbolDef.nPointSize = %d\n",m_sSymbolDef.nPointSize);
    fprintf(fpOut, "  m_sSymbolDef._unknown_  = %d\n", 
                                            (int)m_sSymbolDef._nUnknownValue_);
    fprintf(fpOut, "  m_sSymbolDef.rgbColor   = 0x%6.6x (%d)\n",
                                m_sSymbolDef.rgbColor, m_sSymbolDef.rgbColor);

    fflush(fpOut);
}
