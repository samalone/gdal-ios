/**********************************************************************
 * $Id: mitab_feature_mif.cpp,v 1.17 2001/01/22 16:03:58 warmerda Exp $
 *
 * Name:     mitab_feature.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of R/W Fcts for (Mid/Mif) in feature classes 
 *           specific to MapInfo files.
 * Author:   Stephane Villeneuve, stephane.v@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1999, 2000, Stephane Villeneuve
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
 * $Log: mitab_feature_mif.cpp,v $
 * Revision 1.17  2001/01/22 16:03:58  warmerda
 * expanded tabs
 *
 * Revision 1.16  2000/10/03 19:29:51  daniel
 * Include OGR StyleString stuff (implemented by Stephane)
 *
 * Revision 1.15  2000/09/28 16:39:44  warmerda
 * avoid warnings for unused, and unitialized variables
 *
 * Revision 1.14  2000/09/19 17:23:53  daniel
 * Maintain and/or compute valid region and polyline center/label point
 *
 * Revision 1.13  2000/03/27 03:33:45  daniel
 * Treat SYMBOL line as optional when reading TABPoint
 *
 * Revision 1.12  2000/02/28 16:56:32  daniel
 * Support pen width in points (width values 11 to 2047)
 *
 * Revision 1.11  2000/01/15 22:30:44  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.10  2000/01/14 23:51:37  daniel
 * Fixed handling of "\n" in TABText strings... now the external interface
 * of the lib returns and expects escaped "\"+"n" as described in MIF specs
 *
 * Revision 1.9  1999/12/19 17:37:14  daniel
 * Fixed memory leaks
 *
 * Revision 1.8  1999/12/19 01:02:50  stephane
 * Add a test on the CENTER information
 *
 * Revision 1.7  1999/12/18 23:23:23  stephane
 * Change the format of the output double from %g to %.16g
 *
 * Revision 1.6  1999/12/18 08:22:57  daniel
 * Removed stray break statement in PLINE MULTIPLE write code
 *
 * Revision 1.5  1999/12/18 07:21:30  daniel
 * Fixed test on geometry type when writing OGRMultiLineStrings
 *
 * Revision 1.4  1999/12/18 07:11:57  daniel
 * Return regions as OGRMultiPolygons instead of multiple rings OGRPolygons
 *
 * Revision 1.3  1999/12/16 17:16:44  daniel
 * Use addRing/GeometryDirectly() (prevents leak), and rounded rectangles
 * always return real corner radius from file even if it is bigger than MBR
 *
 * Revision 1.2  1999/11/11 01:22:05  stephane
 * Remove DebugFeature call, Point Reading error, add IsValidFeature() to 
 * test correctly if we are on a feature
 *
 * Revision 1.1  1999/11/08 19:20:30  stephane
 * First version
 *
 * Revision 1.1  1999/11/08 04:16:07  stephane
 * First Revision
 *
 *
 **********************************************************************/

#include "mitab.h"
#include "mitab_utils.h"
#include <ctype.h>

/*=====================================================================
 *                      class TABFeature
 *====================================================================*/

/**********************************************************************
 *                   TABFeature::ReadRecordFromMIDFile()
 *
 *  This method is used to read the Record (Attributs) for all type of
 *  feature included in a mid/mif file.
 * 
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFeature::ReadRecordFromMIDFile(MIDDATAFile *fp)
{
    const char       *pszLine;
    char            **papszToken;
    int               nFields,i;

    nFields = GetFieldCount();
    
    pszLine = fp->GetLastLine();

    papszToken = CSLTokenizeStringComplex(pszLine,
                                          fp->GetDelimiter(),TRUE,TRUE); 
    if (CSLCount(papszToken) != nFields)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    for (i=0;i<nFields;i++)
    {
        SetField(i,papszToken[i]);
    }
    
    fp->GetLine();

    CSLDestroy(papszToken);

    return 0;
}

/**********************************************************************
 *                   TABFeature::WriteRecordToMIDFile()
 *
 *  This methode is used to write the Record (Attributs) for all type
 *  of feature included in a mid file.
 *
 *  Return 0 on success, -1 on error
 **********************************************************************/
int TABFeature::WriteRecordToMIDFile(MIDDATAFile *fp)
{
    int                  iField, numFields;
    OGRFieldDefn        *poFDefn = NULL;

    CPLAssert(fp);
    
    numFields = GetFieldCount();

    for(iField=0; iField<numFields; iField++)
    {
        if (iField != 0)
          fp->WriteLine(",");
        poFDefn = GetFieldDefnRef( iField );

        switch(poFDefn->GetType())
        {
          case OFTString:
            fp->WriteLine("\"%s\"",GetFieldAsString(iField));
            break;          
          default:
            fp->WriteLine("%s",GetFieldAsString(iField));
        }
    }

    fp->WriteLine("\n");

    return 0;
}

/**********************************************************************
 *                   TABFeature::ReadGeometryFromMIFFile()
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
int TABFeature::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    const char *pszLine;
    
    /* Go to the first line of the next feature */

    while (((pszLine = fp->GetLine()) != NULL) && 
           fp->IsValidFeature(pszLine) == FALSE)
      ;

    return 0;
}

/**********************************************************************
 *                   TABFeature::WriteGeometryToMIFFile()
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
 * objects contain no geometry.
 * 
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFeature::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    fp->WriteLine("NONE\n");
    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABPoint::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{  
    OGRGeometry         *poGeometry;
    
    char               **papszToken;
    const char *pszLine;
    double dfX,dfY;
    papszToken = CSLTokenizeString(fp->GetSavedLine());
     
    if (CSLCount(papszToken) !=3)
    {
        CSLDestroy(papszToken);
        return -1;
    }
    
    dfX = fp->GetXTrans(atof(papszToken[1]));
    dfY = fp->GetYTrans(atof(papszToken[2]));

    CSLDestroy(papszToken);
    papszToken = NULL;

    // Read optional SYMBOL line...
    pszLine = fp->GetLastLine();
    papszToken = CSLTokenizeStringComplex(pszLine," ,()",
                                          TRUE,FALSE);
    if (CSLCount(papszToken) == 4 && EQUAL(papszToken[0], "SYMBOL") )
    {
        SetSymbolNo(atoi(papszToken[1]));
        SetSymbolColor(atoi(papszToken[2]));
        SetSymbolSize(atoi(papszToken[3]));
    }

    CSLDestroy(papszToken); 
    papszToken = NULL;

    // scan until we reach 1st line of next feature
    // Since SYMBOL is optional, we have to test IsValidFeature() on that
    // line as well.
    while (pszLine && fp->IsValidFeature(pszLine) == FALSE)
    {
        pszLine = fp->GetLine();
    }
    
    poGeometry = new OGRPoint(dfX, dfY);
    
    SetGeometryDirectly(poGeometry);

    SetMBR(dfX, dfY, dfX, dfY);
    

    return 0; 
}

/**********************************************************************
 *
 **********************************************************************/
int TABPoint::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;
 
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPoint: Missing or Invalid Geometry!");
        return -1;
    }

    fp->WriteLine("Point %.16g %.16g\n",poPoint->getX(),poPoint->getY());
    fp->WriteLine("    Symbol (%d,%d,%d)\n",GetSymbolNo(),GetSymbolColor(),
                  GetSymbolSize());

    return 0; 
}

/**********************************************************************
 *
 **********************************************************************/
int TABFontPoint::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{   
    OGRGeometry         *poGeometry;
    
    char               **papszToken;
    const char *pszLine;
    double dfX,dfY;
    papszToken = CSLTokenizeString(fp->GetSavedLine());

    if (CSLCount(papszToken) !=3)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    dfX = fp->GetXTrans(atof(papszToken[1]));
    dfY = fp->GetYTrans(atof(papszToken[2]));
    
    CSLDestroy(papszToken);
    
    papszToken = CSLTokenizeStringComplex(fp->GetLastLine()," ,()",
                                          TRUE,FALSE);

    if (CSLCount(papszToken) !=7)
    {
        CSLDestroy(papszToken);
        return -1;
    }
    
    SetSymbolNo(atoi(papszToken[1]));
    SetSymbolColor(atoi(papszToken[2]));
    SetSymbolSize(atoi(papszToken[3]));
    SetFontName(papszToken[4]);
    SetFontStyleMIFValue(atoi(papszToken[5]));
    SetSymbolAngle(atof(papszToken[6]));

    CSLDestroy(papszToken);
    
    poGeometry = new OGRPoint(dfX, dfY);
    
    SetGeometryDirectly(poGeometry);

    SetMBR(dfX, dfY, dfX, dfY);

    /* Go to the first line of the next feature */

    while (((pszLine = fp->GetLine()) != NULL) && 
           fp->IsValidFeature(pszLine) == FALSE)
      ;
    return 0; 
}

/**********************************************************************
 *
 **********************************************************************/
int TABFontPoint::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;
 
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABFontPoint: Missing or Invalid Geometry!");
        return -1;
    }

    fp->WriteLine("Point %.16g %.16g\n",poPoint->getX(),poPoint->getY());
    fp->WriteLine("    Symbol (%d,%d,%d,\"%s\",%d,%.16g)\n",
                  GetSymbolNo(),GetSymbolColor(),
                  GetSymbolSize(),GetFontNameRef(),GetFontStyleMIFValue(),
                  GetSymbolAngle());

    return 0; 
}

/**********************************************************************
 *
 **********************************************************************/
int TABCustomPoint::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{   
    OGRGeometry         *poGeometry;
    
    char               **papszToken;
    const char          *pszLine;
    double               dfX,dfY;

    papszToken = CSLTokenizeString(fp->GetSavedLine());

    
    if (CSLCount(papszToken) !=3)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    dfX = fp->GetXTrans(atof(papszToken[1]));
    dfY = fp->GetYTrans(atof(papszToken[2]));

    CSLDestroy(papszToken);
    
    papszToken = CSLTokenizeStringComplex(fp->GetLastLine()," ,()",
                                          TRUE,FALSE);
    if (CSLCount(papszToken) !=5)
    {
        
        CSLDestroy(papszToken);
        return -1;
    }
    
    SetFontName(papszToken[1]);
    SetSymbolColor(atoi(papszToken[2]));
    SetSymbolSize(atoi(papszToken[3]));
    m_nCustomStyle = atoi(papszToken[4]);
    
    CSLDestroy(papszToken);
    
    poGeometry = new OGRPoint(dfX, dfY);
    
    SetGeometryDirectly(poGeometry);

    SetMBR(dfX, dfY, dfX, dfY);

    /* Go to the first line of the next feature */

    while (((pszLine = fp->GetLine()) != NULL) && 
           fp->IsValidFeature(pszLine) == FALSE)
      ;
 
    return 0; 

}

/**********************************************************************
 *
 **********************************************************************/
int TABCustomPoint::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;
 
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABCustomPoint: Missing or Invalid Geometry!");
        return -1;
    }
 

    fp->WriteLine("Point %.16g %.16g\n",poPoint->getX(),poPoint->getY());
    fp->WriteLine("    Symbol (\"%s\",%d,%d,%d)\n",GetFontNameRef(),
                  GetSymbolColor(), GetSymbolSize(),m_nCustomStyle);

    return 0; 
}

/**********************************************************************
 *
 **********************************************************************/
int TABPolyline::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    const char          *pszLine;
    char               **papszToken;
    OGRLineString       *poLine;
    OGRMultiLineString  *poMultiLine;
    GBool                bMultiple = FALSE;
    int                  nNumPoints,nNumSec=0,i,j;
    OGREnvelope          sEnvelope;
    

    papszToken = CSLTokenizeString(fp->GetLastLine());
    
    if (CSLCount(papszToken) < 1)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    if (EQUALN(papszToken[0],"LINE",4))
    {
        if (CSLCount(papszToken) != 5)
          return -1;

        poLine = new OGRLineString();
        poLine->setNumPoints(2);
        poLine->setPoint(0, fp->GetXTrans(atof(papszToken[1])),
                         fp->GetYTrans(atof(papszToken[2])));
        poLine->setPoint(1, fp->GetXTrans(atof(papszToken[3])),
                         fp->GetYTrans(atof(papszToken[4])));
        SetGeometryDirectly(poLine);
        poLine->getEnvelope(&sEnvelope);
        SetMBR(sEnvelope.MinX, sEnvelope.MinY,sEnvelope.MaxX,sEnvelope.MaxY);
    }
    else if (EQUALN(papszToken[0],"PLINE",5))
    {
        switch (CSLCount(papszToken))
        {
          case 1:
            bMultiple = FALSE;
            pszLine = fp->GetLine();
            nNumPoints = atoi(pszLine);
            break;
          case 2:
            bMultiple = FALSE;
            nNumPoints = atoi(papszToken[1]);
            break;
          case 3:
            if (EQUALN(papszToken[1],"MULTIPLE",8))
            {
                bMultiple = TRUE;
                nNumSec = atoi(papszToken[2]);
                pszLine = fp->GetLine();
                nNumPoints = atoi(pszLine);
                break;
            }
            else
            {
              CSLDestroy(papszToken);
              return -1;
            }
            break;
          case 4:
            if (EQUALN(papszToken[1],"MULTIPLE",8))
            {
                bMultiple = TRUE;
                nNumSec = atoi(papszToken[2]);
                nNumPoints = atoi(papszToken[3]);
                break;
            }
            else
            {
                CSLDestroy(papszToken);
                return -1;
            }
            break;
          default:
            CSLDestroy(papszToken);
            return -1;
            break;
        }

        if (bMultiple)
        {
            poMultiLine = new OGRMultiLineString();
            for (j=0;j<nNumSec;j++)
            {
                poLine = new OGRLineString();
                if (j != 0)
                    nNumPoints = atoi(fp->GetLine());
                if (nNumPoints < 2)
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "Invalid number of vertices (%d) in PLINE "
                             "MULTIPLE segment.", nNumPoints);
                    return -1;
                }
                poLine->setNumPoints(nNumPoints);
                for (i=0;i<nNumPoints;i++)
                {
                    CSLDestroy(papszToken);
                    papszToken = CSLTokenizeString(fp->GetLine());
                    poLine->setPoint(i,fp->GetXTrans(atof(papszToken[0])),
                                     fp->GetYTrans(atof(papszToken[1])));
                }
                if (poMultiLine->addGeometryDirectly(poLine) != OGRERR_NONE)
                    CPLAssert(FALSE); // Just in case OGR is modified
                
            } 
            if (SetGeometryDirectly(poMultiLine) != OGRERR_NONE)
                CPLAssert(FALSE); // Just in case OGR is modified
            poMultiLine->getEnvelope(&sEnvelope);
            SetMBR(sEnvelope.MinX, sEnvelope.MinY,
                   sEnvelope.MaxX,sEnvelope.MaxY);
        }
        else
        {
            poLine = new OGRLineString();
            poLine->setNumPoints(nNumPoints);
            for (i=0;i<nNumPoints;i++)
            {
                CSLDestroy(papszToken);
                papszToken = CSLTokenizeString(fp->GetLine());
    
                if (CSLCount(papszToken) != 2)
                  return -1;
                poLine->setPoint(i,fp->GetXTrans(atof(papszToken[0])),
                                 fp->GetYTrans(atof(papszToken[1])));
            }
            SetGeometryDirectly(poLine);
            poLine->getEnvelope(&sEnvelope);
            SetMBR(sEnvelope.MinX, sEnvelope.MinY,
                   sEnvelope.MaxX,sEnvelope.MaxY);
        }
    }    
    
    CSLDestroy(papszToken);
    papszToken = NULL;
    
    while (((pszLine = fp->GetLine()) != NULL) && 
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                              TRUE,FALSE);
        
        if (CSLCount(papszToken) >= 1)
        {
            if (EQUALN(papszToken[0],"PEN",3))
            {
                
                if (CSLCount(papszToken) == 4)
                {                   
                    SetPenWidthMIF(atoi(papszToken[1]));
                    SetPenPattern(atoi(papszToken[2]));
                    SetPenColor(atoi(papszToken[3]));
                }
                
            }
            else if (EQUALN(papszToken[0],"SMOOTH",6))
            {
                m_bSmooth = TRUE;
            }             
        }
        CSLDestroy(papszToken);
    }
    return 0; 
}

/**********************************************************************
 *
 **********************************************************************/
int TABPolyline::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    OGRGeometry   *poGeom;
    OGRMultiLineString *poMultiLine = NULL;
    OGRLineString *poLine = NULL;
    int nNumPoints,i;

  
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbLineString)
    {
        /*-------------------------------------------------------------
         * Simple polyline
         *------------------------------------------------------------*/
        poLine = (OGRLineString*)poGeom;
        nNumPoints = poLine->getNumPoints();
        if (nNumPoints == 2)
        {
            fp->WriteLine("Line %.16g %.16g %.16g %.16g\n",poLine->getX(0),poLine->getY(0),
                          poLine->getX(1),poLine->getY(1));
        }
        else
        {
            
            fp->WriteLine("Pline %d\n",nNumPoints);
            for (i=0;i<nNumPoints;i++)
            {
                fp->WriteLine("%.16g %.16g\n",poLine->getX(i),poLine->getY(i));
            }
        }
    }
    else if (poGeom && poGeom->getGeometryType() == wkbMultiLineString)
    {
        /*-------------------------------------------------------------
         * Multiple polyline... validate all components
         *------------------------------------------------------------*/
        int iLine, numLines;
        poMultiLine = (OGRMultiLineString*)poGeom;
        numLines = poMultiLine->getNumGeometries();

        fp->WriteLine("PLINE MULTIPLE %d\n", numLines);

        for(iLine=0; iLine < numLines; iLine++)
        {
            poGeom = poMultiLine->getGeometryRef(iLine);
            if (poGeom && poGeom->getGeometryType() == wkbLineString)
            { 
                poLine = (OGRLineString*)poGeom;
                nNumPoints = poLine->getNumPoints();
        
                fp->WriteLine("  %d\n",nNumPoints);
                for (i=0;i<nNumPoints;i++)
                {
                    fp->WriteLine("%.16g %.16g\n",poLine->getX(i),poLine->getY(i));
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABPolyline: Object contains an invalid Geometry!");
            }
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPolyline: Missing or Invalid Geometry!");
    }
    
    if (GetPenPattern())
      fp->WriteLine("    Pen (%d,%d,%d)\n",GetPenWidthMIF(),GetPenPattern(),
                    GetPenColor());
    if (m_bSmooth)
      fp->WriteLine("    Smooth\n");

    return 0; 

}

/**********************************************************************
 *                   TABRegion::ReadGeometryFromMIFFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MIF file
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABRegion::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    double               dX, dY;
    OGRLinearRing       *poRing;
    OGRGeometry         *poGeometry = NULL;
    OGRPolygon          *poPolygon = NULL;
    OGRMultiPolygon     *poMultiPolygon = NULL;
    int                  i,iSection, numLineSections=0;
    char               **papszToken;
    const char          *pszLine;
    OGREnvelope          sEnvelope;

    m_bSmooth = FALSE;
    /*=============================================================
     * REGION (Similar to PLINE MULTIPLE)
     *============================================================*/
    papszToken = CSLTokenizeString(fp->GetLastLine());
    
    if (CSLCount(papszToken) ==2)
      numLineSections = atoi(papszToken[1]);
    CSLDestroy(papszToken);
    papszToken = NULL;

    /*-------------------------------------------------------------
     * For 1-ring regions, we return an OGRPolygon with one single
     * OGRLinearRing geometry. 
     *
     * REGIONs with multiple rings are returned as OGRMultiPolygon
     * instead of as OGRPolygons since OGRPolygons require that the
     * first ring be the outer ring, and the other all be inner 
     * rings, but this is not guaranteed inside MapInfo files.  
     *------------------------------------------------------------*/
    if (numLineSections > 1)
        poGeometry = poMultiPolygon = new OGRMultiPolygon;
    else
        poGeometry = NULL;  // Will be set later

    for(iSection=0; iSection<numLineSections; iSection++)
    {
        int     numSectionVertices = 0;

        poPolygon = new OGRPolygon();

        if ((pszLine = fp->GetLine()) != NULL)
        {
            numSectionVertices = atoi(pszLine);
        }

        poRing = new OGRLinearRing();
        poRing->setNumPoints(numSectionVertices);


        for(i=0; i<numSectionVertices; i++)
        {
            pszLine = fp->GetLine();
            if (pszLine)
            {
                papszToken = CSLTokenizeStringComplex(pszLine," ,",
                                                      TRUE,FALSE);
                if (CSLCount(papszToken) == 2)
                {              
                    dX = fp->GetXTrans(atof(papszToken[0]));
                    dY = fp->GetYTrans(atof(papszToken[1]));
                    poRing->setPoint(i, dX, dY);
                }
                CSLDestroy(papszToken);
                papszToken = NULL;
            }   
        }
        poPolygon->addRingDirectly(poRing);
        poRing = NULL;

        if (numLineSections > 1)
            poMultiPolygon->addGeometryDirectly(poPolygon);
        else
            poGeometry = poPolygon;

        poPolygon = NULL;
    }
  
  
    SetGeometryDirectly(poGeometry);
    poGeometry->getEnvelope(&sEnvelope);
    
    SetMBR(sEnvelope.MinX, sEnvelope.MinY, sEnvelope.MaxX, sEnvelope.MaxY);

    while (((pszLine = fp->GetLine()) != NULL) && 
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                              TRUE,FALSE);
        
        if (CSLCount(papszToken) > 1)
        {
            if (EQUALN(papszToken[0],"PEN",3))
            {
                
                if (CSLCount(papszToken) == 4)
                {           
                    SetPenWidthMIF(atoi(papszToken[1]));
                    SetPenPattern(atoi(papszToken[2]));
                    SetPenColor(atoi(papszToken[3]));
                }
                
            }
            else if (EQUALN(papszToken[0],"BRUSH", 5))
            {
                if (CSLCount(papszToken) >= 3)
                {
                    SetBrushFGColor(atoi(papszToken[2]));
                    SetBrushPattern(atoi(papszToken[1]));
                    
                    if (CSLCount(papszToken) == 4)
                       SetBrushBGColor(atoi(papszToken[3]));
                    else
                      SetBrushTransparent(TRUE);
                }
                
            }
            else if (EQUALN(papszToken[0],"CENTER",6))
            {
                if (CSLCount(papszToken) == 3)
                {
                    SetCenter(fp->GetXTrans(atof(papszToken[1])),
                              fp->GetYTrans(atof(papszToken[2])) );
                }
            }
        }
        CSLDestroy(papszToken);
        papszToken = NULL;
    }
    
    
    return 0; 
}
    
/**********************************************************************
 *                   TABRegion::WriteGeometryToMIFFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MIF file
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABRegion::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    OGRGeometry         *poGeom;

    poGeom = GetGeometryRef();

    if (poGeom && (poGeom->getGeometryType() == wkbPolygon ||
                   poGeom->getGeometryType() == wkbMultiPolygon ) )
    {
        /*=============================================================
         * REGIONs are similar to PLINE MULTIPLE
         *
         * We accept both OGRPolygons (with one or multiple rings) and 
         * OGRMultiPolygons as input.
         *============================================================*/
        int     i, iRing, numRingsTotal, numPoints;

        numRingsTotal = GetNumRings();
        
        fp->WriteLine("Region %d\n",numRingsTotal);
        
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

            numPoints = poRing->getNumPoints();

            fp->WriteLine("  %d\n",numPoints);
            for(i=0; i<numPoints; i++)
            {
                fp->WriteLine("%.16g %.16g\n",poRing->getX(i), poRing->getY(i));
            }
        }
        
        if (GetPenPattern())
          fp->WriteLine("    Pen (%d,%d,%d)\n",
                          GetPenWidthMIF(),GetPenPattern(),
                          GetPenColor());
        

        if (GetBrushPattern())
        {
            if (GetBrushTransparent() == 0)
              fp->WriteLine("    Brush (%d,%d,%d)\n",GetBrushPattern(),
                            GetBrushFGColor(),GetBrushBGColor());
            else
              fp->WriteLine("    Brush (%d,%d)\n",GetBrushPattern(),
                            GetBrushFGColor());
        }

        if (m_bCenterIsSet)
        {
            fp->WriteLine("    Center %.16g %.16g\n", m_dCenterX, m_dCenterY);
        }


    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRegion: Object contains an invalid Geometry!");
        return -1;
    }

    return 0; 
}

/**********************************************************************
 *
 **********************************************************************/
int TABRectangle::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    const char          *pszLine;
    char               **papszToken;
    double               dXMin, dYMin, dXMax, dYMax;
    OGRPolygon          *poPolygon;
    OGRLinearRing       *poRing;

    papszToken = CSLTokenizeString(fp->GetLastLine());

    if (CSLCount(papszToken) <  5)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    dXMin = fp->GetXTrans(atof(papszToken[1]));
    dXMax = fp->GetXTrans(atof(papszToken[3]));
    dYMin = fp->GetYTrans(atof(papszToken[2]));
    dYMax = fp->GetYTrans(atof(papszToken[4]));
    
    /*-----------------------------------------------------------------
     * Call SetMBR() and GetMBR() now to make sure that min values are
     * really smaller than max values.
     *----------------------------------------------------------------*/
    SetMBR(dXMin, dYMin, dXMax, dYMax);
    GetMBR(dXMin, dYMin, dXMax, dYMax);
    
    m_bRoundCorners = FALSE;
    m_dRoundXRadius  = 0.0;
    m_dRoundYRadius  = 0.0;
    
    if (EQUALN(papszToken[0],"ROUNDRECT",9))
    {
        m_bRoundCorners = TRUE;
        if (CSLCount(papszToken) == 6)
          m_dRoundXRadius = m_dRoundYRadius = atof(papszToken[5])/2.0;
        else
        {
            CSLDestroy(papszToken);
            papszToken = CSLTokenizeString(fp->GetLine());
            if (CSLCount(papszToken) !=1 )
              m_dRoundXRadius = m_dRoundYRadius = atof(papszToken[1])/2.0;
        }
    }
    CSLDestroy(papszToken);
    papszToken = NULL;

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
         * large for the MBR however, we 
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


   while (((pszLine = fp->GetLine()) != NULL) && 
          fp->IsValidFeature(pszLine) == FALSE)
   {
       papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                             TRUE,FALSE);

       if (CSLCount(papszToken) > 1)
       {
           if (EQUALN(papszToken[0],"PEN",3))
           {       
               if (CSLCount(papszToken) == 4)
               {   
                   SetPenWidthMIF(atoi(papszToken[1]));
                   SetPenPattern(atoi(papszToken[2]));
                   SetPenColor(atoi(papszToken[3]));
               }
              
           }
           else if (EQUALN(papszToken[0],"BRUSH", 5))
           {
               if (CSLCount(papszToken) >=3)
               {
                   SetBrushFGColor(atoi(papszToken[2]));
                   SetBrushPattern(atoi(papszToken[1]));

                   if (CSLCount(papszToken) == 4)
                       SetBrushBGColor(atoi(papszToken[3]));
                   else
                      SetBrushTransparent(TRUE);
               }
              
           }
       }
       CSLDestroy(papszToken);
       papszToken = NULL;
   }
 
   return 0; 

}    


/**********************************************************************
 *
 **********************************************************************/
int TABRectangle::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    OGRGeometry         *poGeom;
    OGRPolygon          *poPolygon;
    OGREnvelope         sEnvelope;
    
     /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPolygon)
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

    if (m_bRoundCorners == TRUE)
    {
        fp->WriteLine("Roundrect %.16g %.16g %.16g %.16g %.16g\n", 
                      sEnvelope.MinX, sEnvelope.MinY,
                      sEnvelope.MaxX, sEnvelope.MaxY, m_dRoundXRadius*2.0);
    }
    else
    {
        fp->WriteLine("Rect %.16g %.16g %.16g %.16g\n", 
                      sEnvelope.MinX, sEnvelope.MinY,
                      sEnvelope.MaxX, sEnvelope.MaxY);
    }
    
    if (GetPenPattern())
      fp->WriteLine("    Pen (%d,%d,%d)\n",GetPenWidthMIF(),GetPenPattern(),
                    GetPenColor());

    if (GetBrushPattern())
    {
        if (GetBrushTransparent() == 0)
          fp->WriteLine("    Brush (%d,%d,%d)\n",GetBrushPattern(),
                        GetBrushFGColor(),GetBrushBGColor());
        else
          fp->WriteLine("    Brush (%d,%d)\n",GetBrushPattern(),
                        GetBrushFGColor());
    }
    return 0; 
}

/**********************************************************************
 *
 **********************************************************************/
int TABEllipse::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{   
    const char *pszLine;
    char **papszToken;
    double              dXMin, dYMin, dXMax, dYMax;
    OGRPolygon          *poPolygon;
    OGRLinearRing       *poRing;

    papszToken = CSLTokenizeString(fp->GetLastLine());

    if (CSLCount(papszToken) != 5)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    dXMin = fp->GetXTrans(atof(papszToken[1]));
    dXMax = fp->GetXTrans(atof(papszToken[3]));
    dYMin = fp->GetYTrans(atof(papszToken[2]));
    dYMax = fp->GetYTrans(atof(papszToken[4]));

    CSLDestroy(papszToken);
    papszToken = NULL;

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

    while (((pszLine = fp->GetLine()) != NULL) && 
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                              TRUE,FALSE);
        
        if (CSLCount(papszToken) > 1)
        {
            if (EQUALN(papszToken[0],"PEN",3))
            {       
                if (CSLCount(papszToken) == 4)
                {   
                    SetPenWidthMIF(atoi(papszToken[1]));
                    SetPenPattern(atoi(papszToken[2]));
                   SetPenColor(atoi(papszToken[3]));
                }
                
            }
            else if (EQUALN(papszToken[0],"BRUSH", 5))
            {
                if (CSLCount(papszToken) >= 3)
                {
                    SetBrushFGColor(atoi(papszToken[2]));
                    SetBrushPattern(atoi(papszToken[1]));
                    
                    if (CSLCount(papszToken) == 4)
                      SetBrushBGColor(atoi(papszToken[3]));
                    else
                      SetBrushTransparent(TRUE);
                    
                }
                
            }
        }
        CSLDestroy(papszToken);
        papszToken = NULL;
    }
    return 0; 
}

/**********************************************************************
 *
 **********************************************************************/
int TABEllipse::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    OGRGeometry         *poGeom;
    OGREnvelope         sEnvelope;
 
    poGeom = GetGeometryRef();
    if ( (poGeom && poGeom->getGeometryType() == wkbPolygon ) ||
         (poGeom && poGeom->getGeometryType() == wkbPoint )  )
        poGeom->getEnvelope(&sEnvelope);
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABEllipse: Missing or Invalid Geometry!");
        return -1;
    }
      
    fp->WriteLine("Ellipse %.16g %.16g %.16g %.16g\n",sEnvelope.MinX, sEnvelope.MinY,
                  sEnvelope.MaxX,sEnvelope.MaxY);
    
    if (GetPenPattern())
      fp->WriteLine("    Pen (%d,%d,%d)\n",GetPenWidthMIF(),GetPenPattern(),
                    GetPenColor());
    
    if (GetBrushPattern())
    {       
        if (GetBrushTransparent() == 0)
          fp->WriteLine("    Brush (%d,%d,%d)\n",GetBrushPattern(),
                        GetBrushFGColor(),GetBrushBGColor());
        else
          fp->WriteLine("    Brush (%d,%d)\n",GetBrushPattern(),
                        GetBrushFGColor());
    }
    return 0; 
}

/**********************************************************************
 *
 **********************************************************************/
int TABArc::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    const char          *pszLine;
    OGRLineString       *poLine;
    char               **papszToken;
    double               dXMin,dXMax, dYMin,dYMax;
    int                  numPts;
    
    papszToken = CSLTokenizeString(fp->GetLastLine());

    if (CSLCount(papszToken) == 5)
    {
        dXMin = fp->GetXTrans(atof(papszToken[1]));
        dXMax = fp->GetXTrans(atof(papszToken[3]));
        dYMin = fp->GetYTrans(atof(papszToken[2]));
        dYMax = fp->GetYTrans(atof(papszToken[4]));

        CSLDestroy(papszToken);
        papszToken = CSLTokenizeString(fp->GetLine());
        if (CSLCount(papszToken) != 2)
        {
            CSLDestroy(papszToken);
            return -1;
        }

        m_dStartAngle = atof(papszToken[0]);
        m_dEndAngle = atof(papszToken[1]);
    }
    else if (CSLCount(papszToken) == 7)
    {
        dXMin = fp->GetXTrans(atof(papszToken[1]));
        dXMax = fp->GetXTrans(atof(papszToken[3]));
        dYMin = fp->GetYTrans(atof(papszToken[2]));
        dYMax = fp->GetYTrans(atof(papszToken[4]));
        m_dStartAngle = atof(papszToken[5]);
        m_dEndAngle = atof(papszToken[6]);
    }
    else
    {
        CSLDestroy(papszToken);
        return -1;
    }

    CSLDestroy(papszToken);
    papszToken = NULL;

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

    if (fp->GetXMultiplier() <= 0.0)
    {
        m_dStartAngle = 360.0 - m_dStartAngle;
        m_dEndAngle = 360.0 - m_dEndAngle;
    }
    
    m_dCenterX = (dXMin + dXMax) / 2.0;
    m_dCenterY = (dYMin + dYMax) / 2.0;
    m_dXRadius = ABS( (dXMax - dXMin) / 2.0 );
    m_dYRadius = ABS( (dYMax - dYMin) / 2.0 );

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

    SetMBR(dXMin, dYMin, dXMax, dYMax);
    SetGeometryDirectly(poLine);

    while (((pszLine = fp->GetLine()) != NULL) && 
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                              TRUE,FALSE);
        
        if (CSLCount(papszToken) > 1)
        {
            if (EQUALN(papszToken[0],"PEN",3))
            {
                
                if (CSLCount(papszToken) == 4)
                {    
                    SetPenWidthMIF(atoi(papszToken[1]));
                    SetPenPattern(atoi(papszToken[2]));
                    SetPenColor(atoi(papszToken[3]));
                }
                
            }
        }
        CSLDestroy(papszToken);
        papszToken = NULL;
   }
   return 0; 
}

/**********************************************************************
 *
 **********************************************************************/
int TABArc::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    /*-------------------------------------------------------------
     * Start/End angles
     * Since we ALWAYS produce files in quadrant 1 then we can
     * ignore the special angle conversion required by flipped axis.
     *------------------------------------------------------------*/

     
    // Write the Arc's actual MBR
     fp->WriteLine("Arc %.16g %.16g %.16g %.16g\n", m_dCenterX-m_dXRadius, 
                   m_dCenterY-m_dYRadius, m_dCenterX+m_dXRadius, 
                   m_dCenterY+m_dYRadius);

     fp->WriteLine("  %.16g %.16g\n",m_dStartAngle,m_dEndAngle); 
     
     if (GetPenPattern())
       fp->WriteLine("    Pen (%d,%d,%d)\n",GetPenWidthMIF(),GetPenPattern(),
                     GetPenColor());
     
   
    return 0; 

}

/**********************************************************************
 *
 **********************************************************************/
int TABText::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{ 
    double               dXMin, dYMin, dXMax, dYMax;
    OGRGeometry         *poGeometry;
    const char          *pszLine;
    char               **papszToken;
    const char          *pszString;
  
    papszToken = CSLTokenizeString(fp->GetLastLine());

    if (CSLCount(papszToken) == 1)
    {
        CSLDestroy(papszToken);
        papszToken = CSLTokenizeString(fp->GetLine());
        if (CSLCount(papszToken) != 1)
        {
            CSLDestroy(papszToken);
            return -1;
        }
        else
          pszString = papszToken[0];
    }
    else if (CSLCount(papszToken) == 2)
    {
        pszString = papszToken[1];
    }
    else
     {
        CSLDestroy(papszToken);
        return -1;
    }

    /*-------------------------------------------------------------
     * Note: The text string may contain escaped "\n" chars, and we
     * return them in their escaped form.
     *------------------------------------------------------------*/
    m_pszString = CPLStrdup(pszString);

    CSLDestroy(papszToken);
    papszToken = CSLTokenizeString(fp->GetLine());
    if (CSLCount(papszToken) != 4)
    {
        CSLDestroy(papszToken);
        return -1;
    }
    else
    {
        dXMin = fp->GetXTrans(atof(papszToken[0]));
        dXMax = fp->GetXTrans(atof(papszToken[2]));
        dYMin = fp->GetYTrans(atof(papszToken[1]));
        dYMax = fp->GetYTrans(atof(papszToken[3]));

        m_dHeight = dYMax - dYMin;  //SetTextBoxHeight(dYMax - dYMin);
        m_dWidth  = dXMax - dXMin;  //SetTextBoxWidth(dXMax - dXMin);
        
        if (m_dHeight <0.0)
          m_dHeight*=-1.0;
        if (m_dWidth <0.0)
          m_dWidth*=-1.0;
    }

    CSLDestroy(papszToken);
    papszToken = NULL;

    /* Set/retrieve the MBR to make sure Mins are smaller than Maxs
     */

    SetMBR(dXMin, dYMin, dXMax, dYMax);
    GetMBR(dXMin, dYMin, dXMax, dYMax);
    
    while (((pszLine = fp->GetLine()) != NULL) && 
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                              TRUE,FALSE);
        
        if (CSLCount(papszToken) > 1)
        {
            if (EQUALN(papszToken[0],"FONT",4))
            {
                if (CSLCount(papszToken) >= 5)
                {    
                    SetFontName(papszToken[1]);
                    SetFontFGColor(atoi(papszToken[4]));
                    if (CSLCount(papszToken) ==6)
                    {
                        SetFontBGColor(atoi(papszToken[5]));
                        SetFontStyleMIFValue(atoi(papszToken[2]),TRUE);
                    }
                    else
                      SetFontStyleMIFValue(atoi(papszToken[2]));

                    // papsztoken[3] = Size ???
                }
                
            }
            else if (EQUALN(papszToken[0],"SPACING",7))
            {
                if (CSLCount(papszToken) >= 2)
                {   
                    if (EQUALN(papszToken[1],"2",1))
                    {
                        SetTextSpacing(TABTSDouble);
                    }
                    else if (EQUALN(papszToken[1],"1.5",3))
                    {
                        SetTextSpacing(TABTS1_5);
                    }
                }
                
                if (CSLCount(papszToken) == 7)
                {
                    if (EQUALN(papszToken[2],"LAbel",5))
                    {
                        if (EQUALN(papszToken[4],"simple",6))
                        {
                            SetTextLineType(TABTLSimple);
                            m_dfLineX = fp->GetXTrans(atof(papszToken[5]));
                            m_dfLineY = fp->GetYTrans(atof(papszToken[6]));
                        }
                        else if (EQUALN(papszToken[4],"arrow", 5))
                        {
                            SetTextLineType(TABTLArrow);
                            m_dfLineX = fp->GetXTrans(atof(papszToken[5]));
                            m_dfLineY = fp->GetYTrans(atof(papszToken[6]));
                        }
                    }
                }               
            }
            else if (EQUALN(papszToken[0],"Justify",7))
            {
                if (CSLCount(papszToken) == 2)
                {
                    if (EQUALN( papszToken[1],"Center",6))
                    {
                        SetTextJustification(TABTJCenter);
                    }
                    else  if (EQUALN( papszToken[1],"Right",5))
                    {
                        SetTextJustification(TABTJRight);
                    }
                    
                }
                
            }
            else if (EQUALN(papszToken[0],"Angle",5))
            {
                if (CSLCount(papszToken) == 2)
                {    
                    m_dAngle = atof(papszToken[1]);

                    //SetTextAngle(atof(papszToken[1]));
                }
                
            }
            else if (EQUALN(papszToken[0],"LAbel",5))
            {
                if (CSLCount(papszToken) == 5)
                {    
                    if (EQUALN(papszToken[2],"simple",6))
                    {
                        SetTextLineType(TABTLSimple);
                        m_dfLineX = fp->GetXTrans(atof(papszToken[3]));
                        m_dfLineY = fp->GetYTrans(atof(papszToken[4]));
                    }
                    else if (EQUALN(papszToken[2],"arrow", 5))
                    {
                        SetTextLineType(TABTLArrow);
                        m_dfLineX = fp->GetXTrans(atof(papszToken[3]));
                        m_dfLineY = fp->GetYTrans(atof(papszToken[4]));
                    }
                }
                

                // What I do with the XY coordonate
            }
        }
        CSLDestroy(papszToken);
        papszToken = NULL;
    }
    /*-----------------------------------------------------------------
     * Create an OGRPoint Geometry... 
     * The point X,Y values will be the coords of the lower-left corner before
     * rotation is applied.  (Note that the rotation in MapInfo is done around
     * the upper-left corner)
     * We need to calculate the true lower left corner of the text based
     * on the MBR after rotation, the text height and the rotation angle.
     *---------------------------------------------------------------- */
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
     *---------------------------------------------------------------- */
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
 *
 **********************************************************************/
int TABText::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    double dXMin,dYMin,dXMax,dYMax;

    /*-------------------------------------------------------------
     * Note: The text string may contain "\n" chars or "\\" chars
     * and we expect to receive them in an escaped form.
     *------------------------------------------------------------*/
    fp->WriteLine("Text \"%s\"\n", GetTextString() );

    //    UpdateTextMBR();
    GetMBR(dXMin, dYMin, dXMax, dYMax);
    fp->WriteLine("    %.16g %.16g %.16g %.16g\n",dXMin, dYMin,dXMax, dYMax); 
 
    if (IsFontBGColorUsed())
      fp->WriteLine("    Font (\"%s\",%d,%d,%d,%d)\n", GetFontNameRef(), 
                    GetFontStyleMIFValue(),0,GetFontFGColor(),
                    GetFontBGColor());
    else
      fp->WriteLine("    Font (\"%s\",%d,%d,%d)\n", GetFontNameRef(), 
                    GetFontStyleMIFValue(),0,GetFontFGColor());

    switch (GetTextSpacing())
    {
      case   TABTS1_5:
        fp->WriteLine("    Spacing 1.5\n");
        break;
      case TABTSDouble:
        fp->WriteLine("    Spacing 2.0\n");
        break;    
      case TABTSSingle:
      default:
        break;
    }

    switch (GetTextJustification())
    {
      case TABTJCenter:
        fp->WriteLine("    Justify Center\n");
        break;
      case TABTJRight:
        fp->WriteLine("    Justify Right\n");
        break;
      case TABTJLeft:
      default:
        break;
    }

    if ((GetTextAngle() - 0.000001) >0.0)
        fp->WriteLine("    Angle %.16g\n",GetTextAngle());

    switch (GetTextLineType())
    {
      case TABTLSimple:
        fp->WriteLine("    Label Line Simple %.16g %.16g \n",
                      m_dfLineX,m_dfLineY );
        break;
      case TABTLArrow:
        fp->WriteLine("    Label Line Arrow %.16g %.16g \n",
                      m_dfLineX,m_dfLineY );
        break;
      case TABTLNoLine:
      default:
        break;
    }
    return 0; 

}

/**********************************************************************
 *
 **********************************************************************/
int TABDebugFeature::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{ 
   const char *pszLine;
  
   
  /* Go to the first line of the next feature */
   printf("%s\n", fp->GetLastLine());

   while (((pszLine = fp->GetLine()) != NULL) && 
          fp->IsValidFeature(pszLine) == FALSE)
     ;
  
   return 0; 
}


/**********************************************************************
 *
 **********************************************************************/
int TABDebugFeature::WriteGeometryToMIFFile(MIDDATAFile *fp){ return -1; }




