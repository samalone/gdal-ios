/******************************************************************************
*
* Project:  OpenGIS Simple Features Reference Implementation
* Purpose:  Standard includes and class definitions ArcObjects OGR driver.
* Author:   Ragi Yaser Burhum, ragi@burhum.com
*
******************************************************************************
* Copyright (c) 2009, Ragi Yaser Burhum
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

#ifndef _OGR_FGDB_H_INCLUDED
#define _OGR_FGDB_H_INCLUDED

#include "ogrsf_frmts.h"

#include <vector>
#include "cpl_string.h"

//COM ATL Includes
#include <atlbase.h>
//#include <atlcom.h>
//#include <atlctl.h>
//#include <atlstr.h> //CString

#ifndef WIN32
#define LINUX_FILEGDB_API
#include <wctype.h>
#define FAILED(hr) ((hr) < 0)
#endif


#include "Table.h"
#include "Geodatabase.h"
#include "GeodatabaseManagement.h"
#include "Row.h"
#include "Util.h"

#include "cpl_minixml.h"

using namespace FileGDBAPI;



/************************************************************************/
/*                            FGdbLayer                                  */
/************************************************************************/

class FGdbDataSource;

class FGdbLayer : public OGRLayer
{
public:

  FGdbLayer();
  virtual ~FGdbLayer();

  bool Initialize(FGdbDataSource* pParentDataSource, Table* pTable, std::wstring wstrTablePath);

  // virtual const char *GetName();

  virtual const char* GetFIDColumn() { return m_strOIDFieldName.c_str(); }
  virtual const char* GetGeometryColumn() { return m_strShapeFieldName.c_str(); }

  virtual void        ResetReading();
  virtual OGRFeature* GetNextFeature();
  virtual OGRFeature* GetFeature( long nFeatureId );

  long GetTable(Table** ppTable);


  virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce );
  virtual int         GetFeatureCount( int bForce );
  virtual OGRErr      SetAttributeFilter( const char *pszQuery );
  virtual void 	      SetSpatialFilterRect (double dfMinX, double dfMinY, double dfMaxX, double dfMaxY);
  virtual void        SetSpatialFilter( OGRGeometry * );

  /*
  virtual OGRErr      CreateField( OGRFieldDefn *poFieldIn,
  int bApproxOK );

  virtual OGRErr      SetFeature( OGRFeature *poFeature );
  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
  virtual OGRErr      DeleteFeature( long nFID );
  */
  OGRFeatureDefn *    GetLayerDefn() { return m_pFeatureDefn; }

  virtual OGRSpatialReference *GetSpatialRef() { return m_pSRS; }

  virtual int         TestCapability( const char * );

protected:

  bool GDBToOGRFields(CPLXMLNode* psFields);  
  bool ParseGeometryDef(CPLXMLNode* psGeometryDef);
  bool ParseSpatialReference(CPLXMLNode* psSpatialRefNode, std::string* pOutWkt);

  bool OGRFeatureFromGdbRow(Row* pRow, OGRFeature** ppFeature);
  
  FGdbDataSource* m_pDS;
  Table* m_pTable;
  OGRFeatureDefn* m_pFeatureDefn;
  OGRSpatialReference* m_pSRS;

  std::string m_strName; //contains underlying FGDB table name (not catalog name)

  std::string m_strOIDFieldName;
  std::string m_strShapeFieldName;

  std::wstring m_wstrTablePath;

  std::wstring m_wstrSubfields;
  std::wstring m_wstrWhereClause;
  OGRGeometry* m_pOGRFilterGeometry;
  EnumRows*    m_pEnumRows;

  bool        m_bFilterDirty; //optimization to avoid multiple calls to search until necessary


  std::vector<std::wstring> m_vOGRFieldToESRIField; //OGR Field Index to ESRI Field Name Mapping

  //buffers are used for avoiding constant reallocation of temp memory
  //unsigned char* m_pBuffer;
  //long  m_bufferSize; //in bytes
  
  bool  m_supressColumnMappingError;
  bool  m_forceMulti;
};

/************************************************************************/
/*                           FGdbDataSource                            */
/************************************************************************/
class FGdbDataSource : public OGRDataSource
{

public:
  FGdbDataSource();
  virtual ~FGdbDataSource();


  int         Open(Geodatabase* pGeodatabase, const char *, int );

  const char* GetName() { return m_pszName; }
  int         GetLayerCount() { return static_cast<int>(m_layers.size()); }

  OGRLayer*   GetLayer( int );


  /*
  virtual OGRLayer* CreateLayer( const char *,
  OGRSpatialReference* = NULL,
  OGRwkbGeometryType = wkbUnknown,
  char** = NULL );

  */
  virtual OGRErr DeleteLayer( int );

  int TestCapability( const char * );

  Geodatabase* GetGDB() { return m_pGeodatabase; }

  /*
  protected:

  void EnumerateSpatialTables();
  void OpenSpatialTable( const char* pszTableName );
  */
protected:
  bool LoadLayers(const std::vector<std::wstring> & typesRequested, const std::wstring & parent);

  char* m_pszName;
  std::vector <FGdbLayer*> m_layers;
  Geodatabase* m_pGeodatabase;

};

/************************************************************************/
/*                              FGdbDriver                                */
/************************************************************************/

class FGdbDriver : public OGRSFDriver
{

public:
  FGdbDriver();
  virtual ~FGdbDriver();

  const char *GetName();
  virtual OGRDataSource *Open( const char *, int );
  int TestCapability( const char * );
  virtual OGRDataSource *CreateDataSource( const char *pszName, char ** = NULL);

  void OpenGeodatabase(std::string, Geodatabase** ppGeodatabase);

private:

};

CPL_C_START
void CPL_DLL RegisterOGRfilegdb();
CPL_C_END

#endif /* ndef _OGR_PG_H_INCLUDED */


