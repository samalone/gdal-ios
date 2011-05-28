/******************************************************************************
 * $Id: ogrdwglayer.cpp 22008 2011-03-22 19:45:20Z warmerdam $
 *
 * Project:  DWG Translator
 * Purpose:  Implements OGRDWGLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Frank Warmerdam <warmerdam@pobox.com>
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
 ****************************************************************************/

#include "ogr_dwg.h"
#include "cpl_conv.h"

#include "DbPolyline.h"
#include "Db2dPolyline.h"
#include "DbLine.h"
#include "DbPoint.h"
#include "DbEllipse.h"
#include "DbArc.h"
#include "DbMText.h"
#include "DbText.h"
#include "DbCircle.h"
#include "DbSpline.h"
#include "DbBlockReference.h"
#include "Ge/GeScale3d.h"

CPL_CVSID("$Id: ogrdwglayer.cpp 22008 2011-03-22 19:45:20Z warmerdam $");

#ifndef PI
#define PI  3.14159265358979323846
#endif 

/************************************************************************/
/*                            OGRDWGLayer()                             */
/************************************************************************/

OGRDWGLayer::OGRDWGLayer( OGRDWGDataSource *poDS )

{
    this->poDS = poDS;

    iNextFID = 0;

    poFeatureDefn = new OGRFeatureDefn( "entities" );
    poFeatureDefn->Reference();

    poDS->AddStandardFields( poFeatureDefn );

    if( !poDS->InlineBlocks() )
    {
        OGRFieldDefn  oScaleField( "BlockScale", OFTRealList );
        poFeatureDefn->AddFieldDefn( &oScaleField );

        OGRFieldDefn  oBlockAngleField( "BlockAngle", OFTReal );
        poFeatureDefn->AddFieldDefn( &oBlockAngleField );
    }

/* -------------------------------------------------------------------- */
/*      Find the *Paper_Space block, which seems to contain all the     */
/*      regular entities.                                               */
/* -------------------------------------------------------------------- */
    OdDbBlockTablePtr pTable = poDS->GetDB()->getBlockTableId().safeOpenObject();
    OdDbSymbolTableIteratorPtr pBlkIter = pTable->newIterator();
    
    for (pBlkIter->start(); ! pBlkIter->done(); pBlkIter->step())
    {
        poBlock = pBlkIter->getRecordId().safeOpenObject();

        if( EQUAL(poBlock->getName(),"*Model_Space") )
            break;
        else
            poBlock = NULL;
    }

    ResetReading();
}

/************************************************************************/
/*                           ~OGRDWGLayer()                           */
/************************************************************************/

OGRDWGLayer::~OGRDWGLayer()

{
    ClearPendingFeatures();
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "DWG", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    if( poFeatureDefn )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                           SetBlockTable()                            */
/*                                                                      */
/*      Set what block table to read features from.  This layer         */
/*      object is used to read blocks features as well as generic       */
/*      entities.                                                       */
/************************************************************************/

void OGRDWGLayer::SetBlockTable( OdDbBlockTableRecordPtr poNewBlock )

{
    poBlock = poNewBlock;

    ResetReading();
}


/************************************************************************/
/*                        ClearPendingFeatures()                        */
/************************************************************************/

void OGRDWGLayer::ClearPendingFeatures()

{
    while( !apoPendingFeatures.empty() )
    {
        delete apoPendingFeatures.front();
        apoPendingFeatures.pop();
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRDWGLayer::ResetReading()

{
    iNextFID = 0;
    ClearPendingFeatures();

    if( !poBlock.isNull() )
        poEntIter = poBlock->newIterator();
}

/************************************************************************/
/*                     TranslateGenericProperties()                     */
/*                                                                      */
/*      Try and convert entity properties handled similarly for most    */
/*      or all entity types                                             */
/************************************************************************/

void OGRDWGLayer::TranslateGenericProperties( OGRFeature *poFeature, 
                                              OdDbEntityPtr poEntity )

{
    poFeature->SetField( "Layer", (const char *) poEntity->layer() );
    poFeature->SetField( "Linetype", (const char *) poEntity->linetype() );

    CPLString osValue;
    osValue.Printf( "%d", (int) poEntity->lineWeight() );
    oStyleProperties["LineWeight"] = osValue;

    OdDbHandle oHandle = poEntity->getDbHandle();
    poFeature->SetField( "EntityHandle", (const char *) oHandle.ascii() );

/* -------------------------------------------------------------------- */
/*      Collect the subclasses.                                         */
/* -------------------------------------------------------------------- */
    CPLString osSubClasses;
    OdRxClass *poClass = poEntity->isA();

    while( poClass != NULL )
    {
        if( osSubClasses.size() > 0 )
            osSubClasses += ":";
        
        osSubClasses += (const char *) poClass->name();
        if( EQUAL(poClass->name(),"AcDbEntity") )
            break;

        poClass = poClass->myParent();
    }
    
    poFeature->SetField( "SubClasses", osSubClasses.c_str() );

#ifdef notdef
      case 62:
        oStyleProperties["Color"] = pszValue;
        break;

        // Extended entity data
      case 1000:
      case 1002:
      case 1004:
      case 1005:
      case 1040:
      case 1041:
      case 1070:
      case 1071:
      {
          CPLString osAggregate = poFeature->GetFieldAsString("ExtendedEntity");

          if( osAggregate.size() > 0 )
              osAggregate += " ";
          osAggregate += pszValue;
            
          poFeature->SetField( "ExtendedEntity", osAggregate );
      }
      break;

      // OCS vector.
      case 210:
        oStyleProperties["210_N.dX"] = pszValue;
        break;
        
      case 220:
        oStyleProperties["220_N.dY"] = pszValue;
        break;
        
      case 230:
        oStyleProperties["230_N.dZ"] = pszValue;
        break;


      default:
        break;
    }
#endif
}

/************************************************************************/
/*                          PrepareLineStyle()                          */
/************************************************************************/

void OGRDWGLayer::PrepareLineStyle( OGRFeature *poFeature )

{
#ifdef notdef
    CPLString osLayer = poFeature->GetFieldAsString("Layer");

/* -------------------------------------------------------------------- */
/*      Work out the color for this feature.                            */
/* -------------------------------------------------------------------- */
    int nColor = 256;

    if( oStyleProperties.count("Color") > 0 )
        nColor = atoi(oStyleProperties["Color"]);

    // Use layer color? 
    if( nColor < 1 || nColor > 255 )
    {
        const char *pszValue = poDS->LookupLayerProperty( osLayer, "Color" );
        if( pszValue != NULL )
            nColor = atoi(pszValue);
    }
        
    if( nColor < 1 || nColor > 255 )
        return;

/* -------------------------------------------------------------------- */
/*      Get line weight if available.                                   */
/* -------------------------------------------------------------------- */
    double dfWeight = 0.0;

    if( oStyleProperties.count("LineWeight") > 0 )
    {
        CPLString osWeight = oStyleProperties["LineWeight"];

        if( osWeight == "-1" )
            osWeight = poDS->LookupLayerProperty(osLayer,"LineWeight");

        dfWeight = CPLAtof(osWeight) / 100.0;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a dash/dot line style?                               */
/* -------------------------------------------------------------------- */
    const char *pszPattern = poDS->LookupLineType(
        poFeature->GetFieldAsString("Linetype") );

/* -------------------------------------------------------------------- */
/*      Format the style string.                                        */
/* -------------------------------------------------------------------- */
    CPLString osStyle;
    const unsigned char *pabyDWGColors = OGRDWGDriver::GetDWGColorTable();

    osStyle.Printf( "PEN(c:#%02x%02x%02x", 
                    pabyDWGColors[nColor*3+0],
                    pabyDWGColors[nColor*3+1],
                    pabyDWGColors[nColor*3+2] );

    if( dfWeight > 0.0 )
    {
        char szBuffer[64];
        snprintf(szBuffer, sizeof(szBuffer), "%.2g", dfWeight);
        char* pszComma = strchr(szBuffer, ',');
        if (pszComma)
            *pszComma = '.';
        osStyle += CPLString().Printf( ",w:%smm", szBuffer );
    }

    if( pszPattern )
    {
        osStyle += ",p:\"";
        osStyle += pszPattern;
        osStyle += "\"";
    }

    osStyle += ")";
    
    poFeature->SetStyleString( osStyle );
#endif
}

#ifdef notdef
/************************************************************************/
/*                            OCSTransformer                            */
/************************************************************************/

class OCSTransformer : public OGRCoordinateTransformation
{
private:
    double adfN[3];
    double adfAX[3];
    double adfAY[3];
    
public:
    OCSTransformer( double adfN[3] ) {
        static const double dSmall = 1.0 / 64.0;
        static const double adfWZ[3] = {0, 0, 1};
        static const double adfWY[3] = {0, 1, 0};

        memcpy( this->adfN, adfN, sizeof(double)*3 );

    if ((ABS(adfN[0]) < dSmall) && (ABS(adfN[1]) < dSmall))
            CrossProduct(adfWY, adfN, adfAX);
    else
            CrossProduct(adfWZ, adfN, adfAX);

    Scale2Unit( adfAX );
    CrossProduct(adfN, adfAX, adfAY);
    Scale2Unit( adfAY );
    }

    void CrossProduct(const double *a, const double *b, double *vResult) {
        vResult[0] = a[1] * b[2] - a[2] * b[1];
        vResult[1] = a[2] * b[0] - a[0] * b[2];
        vResult[2] = a[0] * b[1] - a[1] * b[0];
    }

    void Scale2Unit(double* adfV) {
    double dfLen=sqrt(adfV[0]*adfV[0] + adfV[1]*adfV[1] + adfV[2]*adfV[2]);
    if (dfLen != 0)
    {
            adfV[0] /= dfLen;
            adfV[1] /= dfLen;
            adfV[2] /= dfLen;
    }
    }
    OGRSpatialReference *GetSourceCS() { return NULL; }
    OGRSpatialReference *GetTargetCS() { return NULL; }
    int Transform( int nCount, 
                   double *x, double *y, double *z )
        { return TransformEx( nCount, x, y, z, NULL ); }
    
    int TransformEx( int nCount, 
                     double *adfX, double *adfY, double *adfZ = NULL,
                     int *pabSuccess = NULL )
        {
            int i;
            for( i = 0; i < nCount; i++ )
            {
                double x = adfX[i], y = adfY[i], z = adfZ[i];
                
                adfX[i] = x * adfAX[0] + y * adfAY[0] + z * adfN[0];
                adfY[i] = x * adfAX[1] + y * adfAY[1] + z * adfN[1];
                adfZ[i] = x * adfAX[2] + y * adfAY[2] + z * adfN[2];

                if( pabSuccess )
                    pabSuccess[i] = TRUE;
            }
            return TRUE;
        }
};

/************************************************************************/
/*                        ApplyOCSTransformer()                         */
/*                                                                      */
/*      Apply a transformation from OCS to world coordinates if an      */
/*      OCS vector was found in the object.                             */
/************************************************************************/

void OGRDWGLayer::ApplyOCSTransformer( OGRGeometry *poGeometry )

{
    if( oStyleProperties.count("210_N.dX") == 0
        || oStyleProperties.count("220_N.dY") == 0
        || oStyleProperties.count("230_N.dZ") == 0 )
        return;

    if( poGeometry == NULL )
        return;

    double adfN[3];

    adfN[0] = CPLAtof(oStyleProperties["210_N.dX"]);
    adfN[1] = CPLAtof(oStyleProperties["220_N.dY"]);
    adfN[2] = CPLAtof(oStyleProperties["230_N.dZ"]);

    OCSTransformer oTransformer( adfN );

    poGeometry->transform( &oTransformer );
}
#endif

/************************************************************************/
/*                            TextUnescape()                            */
/*                                                                      */
/*      Unexcape DWG style escape sequences such as \P for newline      */
/*      and \~ for space, and do the recoding to UTF8.                  */
/************************************************************************/

CPLString OGRDWGLayer::TextUnescape( OdString osOdInput )

{
    CPLString osResult;
    CPLString osInput = (const char *) osOdInput;
    
/* -------------------------------------------------------------------- */
/*      Translate text from Win-1252 to UTF8.  We approximate this      */
/*      by treating Win-1252 as Latin-1.  Note that we likely ought     */
/*      to be consulting the $DWGCODEPAGE header variable which         */
/*      defaults to ANSI_1252 if not set.                               */
/* -------------------------------------------------------------------- */
    osInput.Recode( poDS->GetEncoding(), CPL_ENC_UTF8 );

    const char *pszInput = osInput.c_str();

/* -------------------------------------------------------------------- */
/*      Now translate escape sequences.  They are all plain ascii       */
/*      characters and won't have been affected by the UTF8             */
/*      recoding.                                                       */
/* -------------------------------------------------------------------- */
    while( *pszInput != '\0' )
    {
        if( pszInput[0] == '\\' && pszInput[1] == 'P' )
        {
            osResult += '\n';
            pszInput++;
        }
        else if( pszInput[0] == '\\' && pszInput[1] == '~' )
        {
            osResult += ' ';
            pszInput++;
        }
        else if( pszInput[0] == '\\' && pszInput[1] == 'U' 
                 && pszInput[2] == '+' )
        {
            CPLString osHex;
            int iChar;

            osHex.assign( pszInput+3, 4 );
            sscanf( osHex.c_str(), "%x", &iChar );

            wchar_t anWCharString[2];
            anWCharString[0] = (wchar_t) iChar;
            anWCharString[1] = 0;
            
            char *pszUTF8Char = CPLRecodeFromWChar( anWCharString,
                                                    CPL_ENC_UCS2, 
                                                    CPL_ENC_UTF8 );

            osResult += pszUTF8Char;
            CPLFree( pszUTF8Char );
            
            pszInput += 6;
        }
        else if( pszInput[0] == '\\' && pszInput[1] == '\\' )
        {
            osResult += '\\';
            pszInput++;
        }
        else 
            osResult += *pszInput;

        pszInput++;
    }
    
    return osResult;
}

/************************************************************************/
/*                           TranslateMTEXT()                           */
/************************************************************************/

OGRFeature *OGRDWGLayer::TranslateMTEXT( OdDbEntityPtr poEntity )

{
    OdDbMTextPtr poMTE = OdDbMText::cast( poEntity );
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    TranslateGenericProperties( poFeature, poEntity );

/* -------------------------------------------------------------------- */
/*      Set the location.                                               */
/* -------------------------------------------------------------------- */
    OdGePoint3d oLocation = poMTE->location();

    poFeature->SetGeometryDirectly( 
        new OGRPoint( oLocation.x, oLocation.y, oLocation.z ) );

/* -------------------------------------------------------------------- */
/*      Apply text after stripping off any extra terminating newline.   */
/* -------------------------------------------------------------------- */
    CPLString osText = TextUnescape( poMTE->contents() );

    if( osText != "" && osText[osText.size()-1] == '\n' )
        osText.resize( osText.size() - 1 );

    poFeature->SetField( "Text", osText );

/* -------------------------------------------------------------------- */
/*      We need to escape double quotes with backslashes before they    */
/*      can be inserted in the style string.                            */
/* -------------------------------------------------------------------- */
    if( strchr( osText, '"') != NULL )
    {
        CPLString osEscaped;
        size_t iC;

        for( iC = 0; iC < osText.size(); iC++ )
        {
            if( osText[iC] == '"' )
                osEscaped += "\\\"";
            else
                osEscaped += osText[iC];
        }
        osText = osEscaped;
    }

/* -------------------------------------------------------------------- */
/*      Work out the color for this feature.                            */
/* -------------------------------------------------------------------- */
    int nColor = 256;

    if( oStyleProperties.count("Color") > 0 )
        nColor = atoi(oStyleProperties["Color"]);

    // Use layer color? 
    if( nColor < 1 || nColor > 255 )
    {
        CPLString osLayer = poFeature->GetFieldAsString("Layer");
        const char *pszValue = poDS->LookupLayerProperty( osLayer, "Color" );
        if( pszValue != NULL )
            nColor = atoi(pszValue);
    }
        
/* -------------------------------------------------------------------- */
/*      Prepare style string.                                           */
/* -------------------------------------------------------------------- */
    double dfAngle = poMTE->rotation() * 180 / PI;
    double dfHeight = poMTE->textHeight();
    int nAttachmentPoint = (int) poMTE->attachment();

    CPLString osStyle;
    char szBuffer[64];
    char* pszComma;

    osStyle.Printf("LABEL(f:\"Arial\",t:\"%s\"",osText.c_str());

    if( dfAngle != 0.0 )
    {
        snprintf(szBuffer, sizeof(szBuffer), "%.3g", dfAngle);
        pszComma = strchr(szBuffer, ',');
        if (pszComma)
            *pszComma = '.';
        osStyle += CPLString().Printf(",a:%s", szBuffer);
    }

    if( dfHeight != 0.0 )
    {
        snprintf(szBuffer, sizeof(szBuffer), "%.3g", dfHeight);
        pszComma = strchr(szBuffer, ',');
        if (pszComma)
            *pszComma = '.';
        osStyle += CPLString().Printf(",s:%sg", szBuffer);
    }

    if( nAttachmentPoint >= 0 && nAttachmentPoint <= 9 )
    {
        const static int anAttachmentMap[10] = 
            { -1, 7, 8, 9, 4, 5, 6, 1, 2, 3 };
        
        osStyle += 
            CPLString().Printf(",p:%d", anAttachmentMap[nAttachmentPoint]);
    }
#ifdef notdef
    if( nColor > 0 && nColor < 256 )
    {
        const unsigned char *pabyDWGColors = OGRDWGDriver::GetDWGColorTable();
        osStyle += 
            CPLString().Printf( ",c:#%02x%02x%02x", 
                                pabyDWGColors[nColor*3+0],
                                pabyDWGColors[nColor*3+1],
                                pabyDWGColors[nColor*3+2] );
    }
#endif
    osStyle += ")";

    poFeature->SetStyleString( osStyle );

    return poFeature;
}

/************************************************************************/
/*                           TranslateTEXT()                            */
/************************************************************************/

OGRFeature *OGRDWGLayer::TranslateTEXT( OdDbEntityPtr poEntity )

{
    OdDbTextPtr poText = OdDbText::cast( poEntity );
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    TranslateGenericProperties( poFeature, poEntity );

/* -------------------------------------------------------------------- */
/*      Set the location.                                               */
/* -------------------------------------------------------------------- */
    OdGePoint3d oLocation = poText->position();

    poFeature->SetGeometryDirectly( 
        new OGRPoint( oLocation.x, oLocation.y, oLocation.z ) );

/* -------------------------------------------------------------------- */
/*      Apply text after stripping off any extra terminating newline.   */
/* -------------------------------------------------------------------- */
    CPLString osText = TextUnescape( poText->textString() );

    if( osText != "" && osText[osText.size()-1] == '\n' )
        osText.resize( osText.size() - 1 );

    poFeature->SetField( "Text", osText );

/* -------------------------------------------------------------------- */
/*      We need to escape double quotes with backslashes before they    */
/*      can be inserted in the style string.                            */
/* -------------------------------------------------------------------- */
    if( strchr( osText, '"') != NULL )
    {
        CPLString osEscaped;
        size_t iC;

        for( iC = 0; iC < osText.size(); iC++ )
        {
            if( osText[iC] == '"' )
                osEscaped += "\\\"";
            else
                osEscaped += osText[iC];
        }
        osText = osEscaped;
    }

/* -------------------------------------------------------------------- */
/*      Prepare style string.                                           */
/* -------------------------------------------------------------------- */
    double dfAngle = poText->rotation() * 180 / PI;
    double dfHeight = poText->height();

    CPLString osStyle;
    char szBuffer[64];
    char* pszComma;

    osStyle.Printf("LABEL(f:\"Arial\",t:\"%s\"",osText.c_str());

    if( dfAngle != 0.0 )
    {
        snprintf(szBuffer, sizeof(szBuffer), "%.3g", dfAngle);
        pszComma = strchr(szBuffer, ',');
        if (pszComma)
            *pszComma = '.';
        osStyle += CPLString().Printf(",a:%s", szBuffer);
    }

    if( dfHeight != 0.0 )
    {
        snprintf(szBuffer, sizeof(szBuffer), "%.3g", dfHeight);
        pszComma = strchr(szBuffer, ',');
        if (pszComma)
            *pszComma = '.';
        osStyle += CPLString().Printf(",s:%sg", szBuffer);
    }

    // add color!

    osStyle += ")";

    poFeature->SetStyleString( osStyle );

    return poFeature;
}

/************************************************************************/
/*                           TranslatePOINT()                           */
/************************************************************************/

OGRFeature *OGRDWGLayer::TranslatePOINT( OdDbEntityPtr poEntity )

{
    OdDbPointPtr poPE = OdDbPoint::cast( poEntity );
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    TranslateGenericProperties( poFeature, poEntity );

    OdGePoint3d oPoint = poPE->position();

    poFeature->SetGeometryDirectly( new OGRPoint( oPoint.x, oPoint.y, oPoint.z ) );

    return poFeature;
}

/************************************************************************/
/*                        TranslateLWPOLYLINE()                         */
/************************************************************************/

OGRFeature *OGRDWGLayer::TranslateLWPOLYLINE( OdDbEntityPtr poEntity )

{
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    OdDbPolylinePtr poPL = OdDbPolyline::cast( poEntity );

    TranslateGenericProperties( poFeature, poEntity );

/* -------------------------------------------------------------------- */
/*      Create a polyline geometry from the vertices.                   */
/* -------------------------------------------------------------------- */
    OGRLineString *poLS = new OGRLineString();

    for (unsigned int i = 0; i < poPL->numVerts(); i++)
    {
        OdGePoint3d oPoint;
        poPL->getPointAt( i, oPoint );
     
        poLS->addPoint( oPoint.x, oPoint.y, oPoint.z );
    }

    poFeature->SetGeometryDirectly( poLS );

    //PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                        Translate2DPOLYLINE()                         */
/************************************************************************/

OGRFeature *OGRDWGLayer::Translate2DPOLYLINE( OdDbEntityPtr poEntity )

{
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    OdDb2dPolylinePtr poPL = OdDb2dPolyline::cast( poEntity );

    TranslateGenericProperties( poFeature, poEntity );

/* -------------------------------------------------------------------- */
/*      Create a polyline geometry from the vertices.                   */
/* -------------------------------------------------------------------- */
    OGRLineString *poLS = new OGRLineString();
    OdDbObjectIteratorPtr poIter = poPL->vertexIterator();

    while( !poIter->done() )
    {
        OdDb2dVertexPtr poVertex = poIter->entity();
        OdGePoint3d oPoint = poPL->vertexPosition( *poVertex );
        poLS->addPoint( oPoint.x, oPoint.y, oPoint.z );
        poIter->step();
    }

    poFeature->SetGeometryDirectly( poLS );

    //PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                           TranslateLINE()                            */
/************************************************************************/

OGRFeature *OGRDWGLayer::TranslateLINE( OdDbEntityPtr poEntity )

{
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    OdDbLinePtr poPL = OdDbLine::cast( poEntity );

    TranslateGenericProperties( poFeature, poEntity );

/* -------------------------------------------------------------------- */
/*      Create a polyline geometry from the vertices.                   */
/* -------------------------------------------------------------------- */
    OGRLineString *poLS = new OGRLineString();
    OdGePoint3d oPoint;

    poPL->getStartPoint( oPoint );
    poLS->addPoint( oPoint.x, oPoint.y, oPoint.z );

    poPL->getEndPoint( oPoint );
    poLS->addPoint( oPoint.x, oPoint.y, oPoint.z );

    poFeature->SetGeometryDirectly( poLS );

    //PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                          TranslateCIRCLE()                           */
/************************************************************************/

OGRFeature *OGRDWGLayer::TranslateCIRCLE( OdDbEntityPtr poEntity )

{
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    OdDbCirclePtr poC = OdDbCircle::cast( poEntity );

    TranslateGenericProperties( poFeature, poEntity );

/* -------------------------------------------------------------------- */
/*      Get geometry information.                                       */
/* -------------------------------------------------------------------- */
    OdGePoint3d oCenter = poC->center();
    double dfRadius = poC->radius();

/* -------------------------------------------------------------------- */
/*      Create geometry                                                 */
/* -------------------------------------------------------------------- */
    OGRGeometry *poCircle = 
        OGRGeometryFactory::approximateArcAngles( 
            oCenter.x, oCenter.y, oCenter.z,
            dfRadius, dfRadius, 0.0, 0.0, 360.0, 0.0 );

    poFeature->SetGeometryDirectly( poCircle );
    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                            AngleCorrect()                            */
/*                                                                      */
/*      Convert from a "true" angle on the ellipse as returned by       */
/*      the DWG API to an angle of rotation on the ellipse as if the    */
/*      ellipse were actually circular.                                 */
/************************************************************************/

double OGRDWGLayer::AngleCorrect( double dfTrueAngle, double dfRatio )

{
    double dfRotAngle;
    double dfDeltaX, dfDeltaY;

    dfTrueAngle *= (PI / 180); // convert to radians.

    dfDeltaX = cos(dfTrueAngle);
    dfDeltaY = sin(dfTrueAngle);

    dfRotAngle = atan2( dfDeltaY, dfDeltaX * dfRatio);

    dfRotAngle *= (180 / PI); // convert to degrees.

    if( dfTrueAngle < 0 && dfRotAngle > 0 )
        dfRotAngle -= 360.0;
    
    if( dfTrueAngle > 360 && dfRotAngle < 360 )
        dfRotAngle += 360.0;

    return dfRotAngle;
}

/************************************************************************/
/*                          TranslateELLIPSE()                          */
/************************************************************************/

OGRFeature *OGRDWGLayer::TranslateELLIPSE( OdDbEntityPtr poEntity )

{
    OdDbEllipsePtr poEE = OdDbEllipse::cast( poEntity );
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    TranslateGenericProperties( poFeature, poEntity );

/* -------------------------------------------------------------------- */
/*      Get some details.                                               */
/* -------------------------------------------------------------------- */
    double dfStartAngle, dfEndAngle, dfRatio;
    OdGePoint3d oCenter;
    OdGeVector3d oMajorAxis, oUnitNormal;

    // note we reverse start and end angles to account for ogr orientation.
    poEE->get( oCenter, oUnitNormal, oMajorAxis, 
               dfRatio, dfEndAngle, dfStartAngle ); 

    dfStartAngle = -1 * dfStartAngle * 180 / PI;
    dfEndAngle   = -1 * dfEndAngle * 180 / PI;

/* -------------------------------------------------------------------- */
/*      The DWG SDK expresses the angles as the angle to a real         */
/*      point on the ellipse while DXF and the OGR "arc angles" API     */
/*      work in terms of an angle of rotation on the ellipse as if      */
/*      the ellipse were actually circular.  So we need to "correct"    */
/*      for the ratio.                                                  */
/* -------------------------------------------------------------------- */
    dfStartAngle = AngleCorrect( dfStartAngle, dfRatio );
    dfEndAngle = AngleCorrect( dfEndAngle, dfRatio );
    
    if( dfStartAngle > dfEndAngle )
        dfEndAngle += 360.0;

/* -------------------------------------------------------------------- */
/*      Compute primary and secondary axis lengths, and the angle of    */
/*      rotation for the ellipse.                                       */
/* -------------------------------------------------------------------- */
    double dfPrimaryRadius, dfSecondaryRadius;
    double dfRotation;

    dfPrimaryRadius = sqrt( oMajorAxis.x * oMajorAxis.x
                            + oMajorAxis.y * oMajorAxis.y
                            + oMajorAxis.z * oMajorAxis.z );

    dfSecondaryRadius = dfRatio * dfPrimaryRadius;

    dfRotation = -1 * atan2( oMajorAxis.y, oMajorAxis.x ) * 180 / PI;

/* -------------------------------------------------------------------- */
/*      Create geometry                                                 */
/* -------------------------------------------------------------------- */
    OGRGeometry *poEllipse = 
        OGRGeometryFactory::approximateArcAngles( 
            oCenter.x, oCenter.y, oCenter.z,
            dfPrimaryRadius, dfSecondaryRadius, dfRotation, 
            dfStartAngle, dfEndAngle, 0.0 );

    poFeature->SetGeometryDirectly( poEllipse );

//    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                            TranslateARC()                            */
/************************************************************************/

OGRFeature *OGRDWGLayer::TranslateARC( OdDbEntityPtr poEntity )

{
    OdDbArcPtr poAE = OdDbArc::cast( poEntity );
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    double dfRadius = 0.0;
    double dfStartAngle = 0.0, dfEndAngle = 360.0;
    OdGePoint3d oCenter;

    TranslateGenericProperties( poFeature, poEntity );

/* -------------------------------------------------------------------- */
/*      Collect parameters.                                             */
/* -------------------------------------------------------------------- */
    dfEndAngle = -1 * poAE->startAngle() * 180 / PI;
    dfStartAngle = -1 * poAE->endAngle() * 180 / PI;
    dfRadius = poAE->radius();
    oCenter = poAE->center();
    
/* -------------------------------------------------------------------- */
/*      Create geometry                                                 */
/* -------------------------------------------------------------------- */
    if( dfStartAngle > dfEndAngle )
        dfEndAngle += 360.0;

    OGRGeometry *poArc = 
        OGRGeometryFactory::approximateArcAngles( 
            oCenter.x, oCenter.y, oCenter.z,
            dfRadius, dfRadius, 0.0, dfStartAngle, dfEndAngle, 0.0 );

    poFeature->SetGeometryDirectly( poArc );

//    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                          TranslateSPLINE()                           */
/************************************************************************/

void rbspline(int npts,int k,int p1,double b[],double h[], double p[]);
void rbsplinu(int npts,int k,int p1,double b[],double h[], double p[]);

OGRFeature *OGRDWGLayer::TranslateSPLINE( OdDbEntityPtr poEntity )

{
    OdDbSplinePtr poSpline = OdDbSpline::cast( poEntity );
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    std::vector<double> adfControlPoints;
    int nDegree, i;

    TranslateGenericProperties( poFeature, poEntity );

    nDegree = poSpline->degree();

/* -------------------------------------------------------------------- */
/*      Collect the control points in our vector.                       */
/* -------------------------------------------------------------------- */
    int nControlPoints = poSpline->numControlPoints();

    adfControlPoints.push_back( 0.0 ); // some sort of control info.
    
    for( i = 0; i < nControlPoints; i++ )
    {
        OdGePoint3d oCP;

        poSpline->getControlPointAt( i, oCP );

        adfControlPoints.push_back( oCP.x );
        adfControlPoints.push_back( oCP.y );
        adfControlPoints.push_back( 0.0 );
    }

/* -------------------------------------------------------------------- */
/*      Process values.                                                 */
/* -------------------------------------------------------------------- */
    if( poSpline->isClosed() )
    {
        for( i = 0; i < nDegree; i++ )
        {
            adfControlPoints.push_back( adfControlPoints[i*3+1] );
            adfControlPoints.push_back( adfControlPoints[i*3+2] );
            adfControlPoints.push_back( adfControlPoints[i*3+3] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Interpolate spline                                              */
/* -------------------------------------------------------------------- */
    std::vector<double> h, p;

    h.push_back(1.0);
    for( i = 0; i < nControlPoints; i++ )
        h.push_back( 1.0 );
    
    // resolution:
    //int p1 = getGraphicVariableInt("$SPLINESEGS", 8) * npts;
    int p1 = nControlPoints * 8;

    p.push_back( 0.0 );
    for( i = 0; i < 3*p1; i++ )
        p.push_back( 0.0 );

    if( poSpline->isClosed() )
        rbsplinu( nControlPoints, nDegree+1, p1, &(adfControlPoints[0]), 
                  &(h[0]), &(p[0]) );
    else
        rbspline( nControlPoints, nDegree+1, p1, &(adfControlPoints[0]), 
                  &(h[0]), &(p[0]) );
    
/* -------------------------------------------------------------------- */
/*      Turn into OGR geometry.                                         */
/* -------------------------------------------------------------------- */
    OGRLineString *poLS = new OGRLineString();

    poLS->setNumPoints( p1 );
    for( i = 0; i < p1; i++ )
        poLS->setPoint( i, p[i*3+1], p[i*3+2] );

    poFeature->SetGeometryDirectly( poLS );

    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                      GeometryInsertTransformer                       */
/************************************************************************/


class GeometryInsertTransformer : public OGRCoordinateTransformation
{
public:
    GeometryInsertTransformer() : 
            dfXOffset(0),dfYOffset(0),dfZOffset(0),
            dfXScale(1.0),dfYScale(1.0),dfZScale(1.0),
            dfAngle(0.0) {}

    double dfXOffset;
    double dfYOffset;
    double dfZOffset;
    double dfXScale;
    double dfYScale;
    double dfZScale;
    double dfAngle;

    OGRSpatialReference *GetSourceCS() { return NULL; }
    OGRSpatialReference *GetTargetCS() { return NULL; }
    int Transform( int nCount, 
                   double *x, double *y, double *z )
        { return TransformEx( nCount, x, y, z, NULL ); }
    
    int TransformEx( int nCount, 
                     double *x, double *y, double *z = NULL,
                     int *pabSuccess = NULL )
        {
            int i;
            for( i = 0; i < nCount; i++ )
            {
                double dfXNew, dfYNew;

                x[i] *= dfXScale;
                y[i] *= dfYScale;
                z[i] *= dfZScale;

                dfXNew = x[i] * cos(dfAngle) - y[i] * sin(dfAngle);
                dfYNew = x[i] * sin(dfAngle) + y[i] * cos(dfAngle);

                x[i] = dfXNew;
                y[i] = dfYNew;

                x[i] += dfXOffset;
                y[i] += dfYOffset;
                z[i] += dfZOffset;

                if( pabSuccess )
                    pabSuccess[i] = TRUE;
            }
            return TRUE;
        }
};

/************************************************************************/
/*                          TranslateINSERT()                           */
/************************************************************************/

OGRFeature *OGRDWGLayer::TranslateINSERT( OdDbEntityPtr poEntity )

{
    OdDbBlockReferencePtr poRef = OdDbBlockReference::cast( poEntity );
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    TranslateGenericProperties( poFeature, poEntity );

/* -------------------------------------------------------------------- */
/*      Collect parameters from the object.                             */
/* -------------------------------------------------------------------- */
    GeometryInsertTransformer oTransformer;
    CPLString osBlockName;
    double dfAngle = poRef->rotation() * 180 / PI;
    OdGePoint3d oPosition = poRef->position();
    OdGeScale3d oScale = poRef->scaleFactors();

    oTransformer.dfXOffset = oPosition.x;
    oTransformer.dfYOffset = oPosition.y;
    oTransformer.dfZOffset = oPosition.z;

    oTransformer.dfXScale = oScale.sx;    
    oTransformer.dfYScale = oScale.sy;    
    oTransformer.dfZScale = oScale.sz;

    OdDbBlockTableRecordPtr poBlockRec = poRef->blockTableRecord().openObject();
    if (poBlockRec.get())
        osBlockName = (const char *) poBlockRec->getName();

/* -------------------------------------------------------------------- */
/*      In the case where we do not inlined blocks we just capture      */
/*      info on a point feature.                                        */
/* -------------------------------------------------------------------- */
    if( !poDS->InlineBlocks() )
    {
        poFeature->SetGeometryDirectly(
            new OGRPoint( oTransformer.dfXOffset, 
                          oTransformer.dfYOffset,
                          oTransformer.dfZOffset ) );

        poFeature->SetField( "BlockName", osBlockName );

        poFeature->SetField( "BlockAngle", dfAngle );
        poFeature->SetField( "BlockScale", 3, &(oTransformer.dfXScale) );

        return poFeature;
    }

/* -------------------------------------------------------------------- */
/*      Lookup the block.                                               */
/* -------------------------------------------------------------------- */
    DWGBlockDefinition *poBlock = poDS->LookupBlock( osBlockName );
    
    if( poBlock == NULL )
    {
        delete poFeature;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Transform the geometry.                                         */
/* -------------------------------------------------------------------- */
    if( poBlock->poGeometry != NULL )
    {
        OGRGeometry *poGeometry = poBlock->poGeometry->clone();

        poGeometry->transform( &oTransformer );

        poFeature->SetGeometryDirectly( poGeometry );
    }

/* -------------------------------------------------------------------- */
/*      If we have complete features associated with the block, push    */
/*      them on the pending feature stack copying over key override     */
/*      information.                                                    */
/*                                                                      */
/*      Note that while we transform the geometry of the features we    */
/*      don't adjust subtle things like text angle.                     */
/* -------------------------------------------------------------------- */
    unsigned int iSubFeat;

    for( iSubFeat = 0; iSubFeat < poBlock->apoFeatures.size(); iSubFeat++ )
    {
        OGRFeature *poSubFeature = poBlock->apoFeatures[iSubFeat]->Clone();

        if( poSubFeature->GetGeometryRef() != NULL )
            poSubFeature->GetGeometryRef()->transform( &oTransformer );

        poSubFeature->SetField( "EntityHandle",
                                poFeature->GetFieldAsString("EntityHandle") );

        apoPendingFeatures.push( poSubFeature );
    }

/* -------------------------------------------------------------------- */
/*      Return the working feature if we had geometry, otherwise        */
/*      return NULL and let the machinery find the rest of the          */
/*      features in the pending feature stack.                          */
/* -------------------------------------------------------------------- */
    if( poBlock->poGeometry == NULL )
    {
        delete poFeature;
        return NULL;
    }
    else
    {
        return poFeature;
    }
}

/************************************************************************/
/*                      GetNextUnfilteredFeature()                      */
/************************************************************************/

OGRFeature *OGRDWGLayer::GetNextUnfilteredFeature()

{
    OGRFeature *poFeature = NULL;

/* -------------------------------------------------------------------- */
/*      If we have pending features, return one of them.                */
/* -------------------------------------------------------------------- */
    if( !apoPendingFeatures.empty() )
    {
        poFeature = apoPendingFeatures.front();
        apoPendingFeatures.pop();

        poFeature->SetFID( iNextFID++ );
        return poFeature;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the next entity.                                          */
/* -------------------------------------------------------------------- */
    while( poFeature == NULL && !poEntIter->done() )
    {

        OdDbObjectId oId = poEntIter->objectId();
        OdDbEntityPtr poEntity = OdDbEntity::cast( oId.openObject() );
        
        if (poEntity.isNull())
            return NULL;

/* -------------------------------------------------------------------- */
/*      What is the class name for this entity?                         */
/* -------------------------------------------------------------------- */
        OdRxClass *poClass = poEntity->isA();
        const OdString osName = poClass->name();
        const char *pszEntityClassName = (const char *) osName;
        
/* -------------------------------------------------------------------- */
/*      Handle the entity.                                              */
/* -------------------------------------------------------------------- */
        oStyleProperties.clear();

        if( EQUAL(pszEntityClassName,"AcDbPoint") )
        {
            poFeature = TranslatePOINT( poEntity );
        }
        else if( EQUAL(pszEntityClassName,"AcDbLine") )
        {
            poFeature = TranslateLINE( poEntity );
        }
        else if( EQUAL(pszEntityClassName,"AcDbPolyline") )
        {
            poFeature = TranslateLWPOLYLINE( poEntity );
        }
        else if( EQUAL(pszEntityClassName,"AcDb2dPolyline") )
        {
            poFeature = Translate2DPOLYLINE( poEntity );
        }
        else if( EQUAL(pszEntityClassName,"AcDbEllipse") )
        {
            poFeature = TranslateELLIPSE( poEntity );
        }
        else if( EQUAL(pszEntityClassName,"AcDbArc") )
        {
            poFeature = TranslateARC( poEntity );
        }
        else if( EQUAL(pszEntityClassName,"AcDbMText") )
        {
            poFeature = TranslateMTEXT( poEntity );
        }
        else if( EQUAL(pszEntityClassName,"AcDbText") )
        {
            poFeature = TranslateTEXT( poEntity );
        }
        else if( EQUAL(pszEntityClassName,"AcDbAlignedDimension") 
                 || EQUAL(pszEntityClassName,"AcDbRotatedDimension") )
        {
            poFeature = TranslateDIMENSION( poEntity );
        }
        else if( EQUAL(pszEntityClassName,"AcDbCircle") )
        {
            poFeature = TranslateCIRCLE( poEntity );
        }
        else if( EQUAL(pszEntityClassName,"AcDbSpline") )
        {
            poFeature = TranslateSPLINE( poEntity );
        }
        else if( EQUAL(pszEntityClassName,"AcDbHatch") )
        {
            poFeature = TranslateHATCH( poEntity );
        }
        else if( EQUAL(pszEntityClassName,"AcDbBlockReference") )
        {
            poFeature = TranslateINSERT( poEntity );
        }
        else
        {
            CPLDebug( "DWG", "Ignoring entity '%s'.", pszEntityClassName );
        }

        poEntIter->step();
    }

/* -------------------------------------------------------------------- */
/*      Set FID.                                                        */
/* -------------------------------------------------------------------- */
    if( poFeature != NULL )
    {
        poFeature->SetFID( iNextFID++ );
        m_nFeaturesRead++;
    }
    
    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRDWGLayer::GetNextFeature()

{
    while( TRUE )
    {
        OGRFeature *poFeature = GetNextUnfilteredFeature();

        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
             || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature ) ) )
        {
            return poFeature;
        }

        delete poFeature;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDWGLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;
    else
        return FALSE;
}

