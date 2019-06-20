// Copyright 2017, 2018, 2019 InnoVisioNate Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <tchar.h>
#include <olectl.h>

#include "Document.h"

   HRESULT __stdcall PdfDocument::QueryInterface(REFIID riid, void** ppv) {
   *ppv = 0;

   if ( IID_IUnknown == riid )
      *ppv = this;
   else

      if ( IID_IPdfDocument == riid )
         *ppv = static_cast<IPdfDocument *>(this);
      else

         return pParent -> QueryInterface(riid,ppv);

   AddRef();

   return S_OK;
   }


   ULONG __stdcall PdfDocument::AddRef() {
   return refCount++;
   }


   ULONG __stdcall PdfDocument::Release() { 
   if ( --refCount == 0 ) {
      delete this;
      return 0;
   }
   return refCount;
   }


   // IDispatch

   STDMETHODIMP PdfDocument::GetTypeInfoCount(UINT * pctinfo) { 
   *pctinfo = 1;
   return S_OK;
   } 


   long __stdcall PdfDocument::GetTypeInfo(UINT itinfo,LCID lcid,ITypeInfo **pptinfo) { 
   *pptinfo = NULL; 
   if ( itinfo != 0 ) 
      return DISP_E_BADINDEX; 
   *pptinfo = pITypeInfo;
   pITypeInfo -> AddRef();
   return S_OK; 
   } 
 

   STDMETHODIMP PdfDocument::GetIDsOfNames(REFIID riid,OLECHAR** rgszNames,UINT cNames,LCID lcid, DISPID* rgdispid) { 
   return DispGetIDsOfNames(pITypeInfo,rgszNames,cNames,rgdispid);
   }


   STDMETHODIMP PdfDocument::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, 
                                           WORD wFlags,DISPPARAMS FAR* pdispparams, VARIANT FAR* pvarResult,
                                           EXCEPINFO FAR* pexcepinfo, UINT FAR* puArgErr) { 
   return DispInvoke(this,pITypeInfo,dispidMember,wFlags,pdispparams,pvarResult,pexcepinfo,puArgErr); 
   }


   // IPdfDocument


   HRESULT PdfDocument::Open(BSTR bstrFileName,BSTR bstrUserPassword,BSTR bstrOwnerPassword) {
   WideCharToMultiByte(CP_ACP,0,bstrFileName,-1,szFileName,MAX_PATH,NULL,0);
   FILE *fInput = fopen(szFileName,"rb");
   if ( ! fInput )
      return E_FAIL;
   takeFile(fInput);
   fclose(fInput);
   parseDocument();
   return S_OK;
   }


   HRESULT PdfDocument::Page(long pageNumber,char *pszPageLabel,IPdfPage **ppPage) {
   if ( ! ppPage )
      return E_POINTER;
   *ppPage = NULL;
   PdfPage *pPage = GetPage(pageNumber,pszPageLabel);
   if ( ! pPage )
      return E_FAIL;
   pPage -> QueryInterface(IID_IPdfPage,reinterpret_cast<void **>(ppPage));
   return S_OK;
   }


   HRESULT PdfDocument::PageFromLabel(char *pszPageLabel,long *pPageNumber) {
   if ( ! pPageNumber )
      return E_POINTER;
   *pPageNumber = PageFromLabel(pszPageLabel);
   return S_OK;
   }


   HRESULT PdfDocument::LabelFromPage(long pageNumber,long stringSize,char *pszPageLabel) {
   if ( ! pszPageLabel )
      return E_POINTER;
   char *p = LabelFromPage(pageNumber);
   if ( p )
      strncpy(pszPageLabel,p,stringSize);
   else
      pszPageLabel[0] = '\0';
   return S_OK;
   }


   HRESULT PdfDocument::Write(BSTR bstrOutputFile) {
   char szOutputFile[MAX_PATH];
   WideCharToMultiByte(CP_ACP,0,bstrOutputFile,-1,szOutputFile,MAX_PATH,NULL,0);
   if ( ! Write(szOutputFile) )
      return E_FAIL;
   return S_OK;
   }


   HRESULT PdfDocument::WriteUncompressed(BSTR bstrOutputFile) {
   char szOutputFile[MAX_PATH];
   WideCharToMultiByte(CP_ACP,0,bstrOutputFile,-1,szOutputFile,MAX_PATH,NULL,0);
   if ( ! WriteUncompressed(szOutputFile) )
      return E_FAIL;
   return S_OK;
   }

   HRESULT PdfDocument::QueryInfoSize(long *pSize) {

   if ( ! pSize )
      return E_POINTER;

   *pSize = 0;

   PdfObject *pInfoObject = pXReference -> InfoObject();
   if ( pInfoObject ) 
      *pSize += pInfoObject -> Dictionary() -> BinaryDataSize();

   std::list<PdfObject *> *pInfoObjects = NULL;

   pInfoObjects = CreateObjectListByType("/Metadata");

   if ( pInfoObjects ) {
      for ( std::list<PdfObject *>::iterator it = pInfoObjects -> begin(); it != pInfoObjects -> end(); it++ ) {
         pInfoObject = (*it);
         *pSize += 16;
         pInfoObject -> Stream() -> Decompress();
         *pSize += pInfoObject -> Stream() -> BinaryDataSize();
      }
      delete pInfoObjects;
   }

   return S_OK;
   }


   HRESULT PdfDocument::QueryInfo(char *pszBuffer) {

   if ( ! pszBuffer )
      return E_POINTER;

   long totalSize = 0;

   PdfObject *pInfoObject = pXReference -> InfoObject();
   if ( pInfoObject ) {
      memcpy(pszBuffer + totalSize,pInfoObject -> Dictionary() -> Storage(),pInfoObject -> Dictionary() -> BinaryDataSize());
      totalSize += pInfoObject -> Dictionary() -> BinaryDataSize();
   }

   std::list<PdfObject *> *pInfoObjects = NULL;

   pInfoObjects = CreateObjectListByType("/Metadata");

   if ( pInfoObjects ) {
      for ( std::list<PdfObject *>::iterator it = pInfoObjects -> begin(); it != pInfoObjects -> end(); it++ ) {
         pInfoObject = (*it);
         totalSize += sprintf(pszBuffer + totalSize,"\nMetaData:\n");
         pInfoObject -> Stream() -> Decompress();
         memcpy(pszBuffer + totalSize,pInfoObject -> Stream() -> Storage(),pInfoObject -> Stream() -> BinaryDataSize());
         totalSize += pInfoObject -> Stream() -> BinaryDataSize();
      }
#if 0
      delete pInfoObjects;
#endif
   }

   pszBuffer[totalSize] = '\0';

   return S_OK;
   }


   HRESULT PdfDocument::get_PageCount(long *pCount) {
   if ( ! pCount )
      return E_POINTER;
   *pCount = GetPageCount();
   return S_OK;
   }


   HRESULT PdfDocument::UpdateNamedCount(BSTR name,long currentCount,long maxCount) {
   char szTemp[MAX_PATH];
   WideCharToMultiByte(CP_ACP,0,name,-1,szTemp,MAX_PATH,0,0);
   PdfObject *pObject = FindObjectByNamedDictionaryEntry("CounterObject",szTemp);
   if ( ! pObject ) {
      long objectId = NewObjectId();
      char szObject[MAX_PATH];
      sprintf(szObject,"%ld 0 obj<</CounterObject %s/count %ld/maxCount %ld>>endobj",objectId,szTemp,currentCount,maxCount);
      pObject = new PdfObject(this,(BYTE *)szObject,(long)strlen(szObject));
      AddObject(pObject);
      if ( XReference() )
         XReference() -> AddObject(pObject);
      return S_OK;
   }
   PdfDictionary *pDictionary = pObject -> Dictionary();
   if ( ! pDictionary )
      return E_FAIL;
   pDictionary -> SetValue("count",currentCount);
   return S_OK;
   }


   HRESULT PdfDocument::get_NamedCount(BSTR name,long *pCount) {
   if ( ! pCount )
      return E_POINTER;
   *pCount = 0L;
   char szTemp[MAX_PATH];
   WideCharToMultiByte(CP_ACP,0,name,-1,szTemp,MAX_PATH,0,0);
   PdfObject *pObject = FindObjectByNamedDictionaryEntry("CounterObject",szTemp);
   if ( ! pObject )
      return E_FAIL;
   PdfDictionary *pDictionary = pObject -> Dictionary();
   if ( ! pDictionary )
      return E_FAIL;
   BYTE *bValue = pDictionary -> Value("count");
   if ( ! bValue )
      return E_FAIL;
   *pCount = atol((char *)bValue);
   return S_OK;
   }


   HRESULT PdfDocument::RemoveIndexedAddedStream(long index) {
   return removeIndexedAddedStream(index,true);
   }

   HRESULT PdfDocument::EraseIndexedAddedStream(long index) {
   return removeIndexedAddedStream(index,false);
   }

   HRESULT PdfDocument::removeIndexedAddedStream(long index,bool doRemove) {

   if ( 0 > index )
      return E_INVALIDARG;

   long countStreams = 0L;

   get_RemovableStreams(&countStreams);

   if ( index > (countStreams - 1) )
      return E_FAIL;

   if ( index == (countStreams - 1) )
      return removeLastAddedStream(doRemove);

   PdfObject *pObject = FindObjectByNamedDictionaryEntry("AddedStreams","cvStreams");

   if ( ! pObject )
      return E_FAIL;

   BYTE *pValues = pObject -> Value("entries");

   if ( ! pValues )
      return E_FAIL;

   char *p = (char *)pValues + strlen((char *)pValues);
   char *pEnd = p;
   char *pSucceeding = NULL;

   for ( long target = countStreams - 1; target > index - 1; target-- ) {

      pSucceeding = p;

      while ( 1 ) {
      
         p--;

         while ( ',' != *p && '(' != *p && '|' != *p && '@' != *p )
            p--;

         if ( '|' == *p )
            break;

         if ( '@' == *p )
            break;

         if ( '(' == *p ) 
            break;

      }

   }

   BYTE *pReplace = NULL;

   if ( doRemove ) {
      pReplace = new BYTE[strlen((char *)pValues)];
      memset(pReplace,0,(strlen((char *)pValues)) * sizeof(BYTE));
      memcpy(pReplace,pValues,(BYTE *)p - pValues);
      memcpy(pReplace + ((BYTE *)p - pValues),pSucceeding,pEnd - pSucceeding);
   }

   if ( '@' == *p )
      p++;

   if ( ',' == *p )
      p++;

   PdfObject *pTheObjectWithStream = objectMap[atol(p)];

   if ( pTheObjectWithStream )
      pTheObjectWithStream -> RemoveStream();

   while ( ! ( ',' == *p ) )
      p++;

   if ( ',' == *p )
      p++;

   pTheObjectWithStream = objectMap[atol(p)];

   if ( pTheObjectWithStream )
      pTheObjectWithStream -> RemoveStream();

   if ( doRemove ) {
      pObject -> SetValue("entries",pReplace);
      delete [] pReplace;
   }

   return S_OK;
   }

   HRESULT PdfDocument::removeAddedStreamByObjectID(long objectId) {

   PdfObject *pObject = FindObjectByNamedDictionaryEntry("AddedStreams","cvStreams");

   if ( ! pObject )
      return E_FAIL;

   BYTE *pValues = pObject -> Value("entries");

   if ( ! pValues )
      return E_FAIL;

   char *p = (char *)pValues + strlen((char *)pValues);
   char *pEnd = p;
   char *pSucceeding = NULL;

   while ( 1 ) {

      pSucceeding = p;

      while ( 1 ) {
      
         p--;

         while ( ',' != *p && '(' != *p && '|' != *p && '@' != *p )
            p--;

         if ( '|' == *p )
            break;

         if ( '@' == *p )
            break;

         if ( '(' == *p ) 
            break;

      }

      if ( '|' == *p )
         break;

      if ( '(' == *p )
         break;

      char *pMaybe = p;

      if ( '@' == *p )
         p++;

      if ( ',' == *p )
         p++;

      if ( ! ( atol(p) == objectId ) ) {
         p = pMaybe;
         continue;
      }

      BYTE *pReplace = NULL;

      pReplace = new BYTE[strlen((char *)pValues)];
      memset(pReplace,0,(strlen((char *)pValues)) * sizeof(BYTE));
      memcpy(pReplace,pValues,(BYTE *)pMaybe - pValues);
      memcpy(pReplace + ((BYTE *)pMaybe - pValues),pSucceeding,pEnd - pSucceeding);

      PdfObject *pTheObjectWithStream = objectMap[atol(p)];

      if ( pTheObjectWithStream )
         pTheObjectWithStream -> RemoveStream();

      pTheObjectWithStream = objectMap[atol(p) + 1];

      if ( pTheObjectWithStream )
         pTheObjectWithStream -> RemoveStream();

      pObject -> SetValue("entries",pReplace);

      delete [] pReplace;

      return S_OK;

   }

   return E_FAIL;
   }


   HRESULT PdfDocument::RemoveLastAddedStream() {
   return removeLastAddedStream(true);
   }

   HRESULT PdfDocument::EraseLastAddedStream() {
   return removeLastAddedStream(false);
   }

   HRESULT PdfDocument::removeLastAddedStream(bool doRemove) {

   PdfObject *pObject = FindObjectByNamedDictionaryEntry("AddedStreams","cvStreams");

   if ( ! pObject )
      return E_FAIL;

   BYTE *pValues = pObject -> Value("entries");
   if ( ! pValues )
      return E_FAIL;

   char *p = (char *)pValues + strlen((char *)pValues);

   while ( 1 ) {
   
      p--;

      while ( ',' != *p && '(' != *p && '|' != *p && '@' != *p )
         p--;

      if ( '|' == *p )
         break;

      if ( '@' == *p )
         break;

      if ( '(' == *p ) 
         break;

      PdfObject *pTheObjectWithStream = objectMap[atol(p + 1)];

      if ( pTheObjectWithStream )
         pTheObjectWithStream -> RemoveStream();

   }

   if ( doRemove ) {
      if ( '@' == *p )
         *(p - 1) = '\0';

      pObject -> SetValue("entries",pValues);
   }

   return S_OK;
   }

//
//NTC: ToDo: Implement the Seal capability. Traverse the array's of IDs and Streams backwards and stop
// if an index or objectID of 0 is encountered.
// Also implement adding the sentinal index to the two StreamLists
//
   HRESULT PdfDocument::RemoveStreamByID(long identifier) {

   PdfObject *pObjectIDs = FindObjectByDictionaryEntry("CVAddedStreamListIDs");

   if ( ! pObjectIDs )
      return E_FAIL;

   PdfDictionary *pDictionaryIDs = pObjectIDs -> Dictionary();

   if ( ! pDictionaryIDs )
      return E_FAIL;
   
   BYTE *pArrayIDs = pDictionaryIDs -> Value("CVAddedStreamListIDs");

   if ( ! pArrayIDs )
      return E_FAIL;


   PdfObject *pObjectStreams = FindObjectByDictionaryEntry("CVAddedStreamList");

   if ( ! pObjectStreams )
      return E_FAIL;

   PdfDictionary *pDictionaryStreams = pObjectStreams -> Dictionary();

   if ( ! pDictionaryStreams )
      return E_FAIL;

   BYTE *pArrayStreams = pDictionaryStreams -> Value("CVAddedStreamList");
   
   if ( ! pArrayStreams )
      return E_FAIL;


   long listArraySize = pdfUtility.ArraySize((char *)pArrayIDs);

   if ( 0 == listArraySize )
      return E_FAIL;

   long countRemoved = 0L;

   std::list<long> replaceIDs;
   std::list<long> replaceStreamIDs;

   for ( long k = listArraySize; k > 0; k-- ) {

      char *pDocumentDelimiter = pdfUtility.StringValueFromArray((char *)pArrayIDs,k);

      if ( pDocumentDelimiter && '|' == pDocumentDelimiter[0] ) 
         break;

      long arrayValue = pdfUtility.IntegerValueFromArray((char *)pArrayIDs,k);
      long objectId = pdfUtility.IntegerValueFromArray((char *)pArrayStreams,k);

      if ( arrayValue == identifier ) {

         if ( S_OK == removeAddedStreamByObjectID(objectId) )
            countRemoved++;

      } else {

         replaceIDs.insert(replaceIDs.end(),arrayValue);
         replaceStreamIDs.insert(replaceStreamIDs.end(),objectId);

      }

   }

   if ( 0 == countRemoved ) {
      replaceIDs.clear();
      replaceStreamIDs.clear();
      return E_FAIL;
   }

   char *pszReplaceIDs = new char[2 * strlen((char *)pArrayIDs)];
   char *pszReplaceStreamIDs = new char[2 * strlen((char *)pArrayStreams)];

   memset(pszReplaceIDs,0,2 * strlen((char *)pArrayIDs) * sizeof(char));
   memset(pszReplaceStreamIDs,0,2 * strlen((char *)pArrayStreams) * sizeof(char));

   char *pBegin = strrchr((char *)pArrayIDs,'|');

   if ( ! pBegin ) {
      pBegin = pszReplaceIDs;
      pBegin[0] = '[';
   } else {
      strncpy(pszReplaceIDs,(char *)pArrayIDs,(BYTE *)pBegin - pArrayIDs + 1);
      pBegin = pszReplaceIDs + ((BYTE *)pBegin - pArrayIDs);
   }

   for ( std::list<long>::reverse_iterator it = replaceIDs.rbegin(); it != replaceIDs.rend(); it++ )
      sprintf(pBegin + strlen(pBegin)," %ld ",(*it));

   pBegin[strlen(pBegin)] = ']';

   pBegin = strrchr((char *)pArrayStreams,'|');

   if ( ! pBegin ) {
      pBegin = pszReplaceStreamIDs;
      pBegin[0] = '[';
   } else {
      strncpy(pszReplaceStreamIDs,(char *)pArrayStreams,(BYTE *)pBegin - pArrayStreams + 1);
      pBegin = pszReplaceStreamIDs + ((BYTE *)pBegin - pArrayStreams);
   }

   for ( std::list<long>::reverse_iterator it = replaceStreamIDs.rbegin(); it != replaceStreamIDs.rend(); it++ )
      sprintf(pBegin + strlen(pBegin)," %ld ",(*it));

   pBegin[strlen(pBegin)] = ']';

   pDictionaryIDs -> SetValue("CVAddedStreamListIDs",(BYTE *)pszReplaceIDs);

   pDictionaryStreams -> SetValue("CVAddedStreamList",(BYTE *)pszReplaceStreamIDs);

   delete [] pszReplaceIDs;
   delete [] pszReplaceStreamIDs;

   replaceIDs.clear();
   replaceStreamIDs.clear();

   return S_OK;
   }

   void PdfDocument::InsertIntoStreamList(char *pszStreamList,char *pszValue) {

   PdfObject *pAddedStreamList = FindObjectByDictionaryEntry(pszStreamList);

   if ( ! pAddedStreamList ) {
      long objectId = NewObjectId();
      char szObject[MAX_PATH];
      sprintf(szObject,"%ld 0 obj<</%s []>>endobj",objectId,pszStreamList);
      pAddedStreamList = new PdfObject(this,(BYTE *)szObject,(long)strlen(szObject));
      AddObject(pAddedStreamList);
      if ( XReference() )
         XReference() -> AddObject(pAddedStreamList);
   }

   PdfDictionary *pAddedStreamListDictionary = pAddedStreamList -> Dictionary();
   
   BYTE *pAddedStreamListValue = pAddedStreamListDictionary -> Value(pszStreamList);

   pAddedStreamListDictionary -> SetValue(pszStreamList,pdfUtility.addToArray((BYTE *)pszValue,pAddedStreamListValue));

   return;
   }


   HRESULT PdfDocument::get_RemovableStreams(long *pCount) {

   if ( ! pCount )
      return E_POINTER;

   *pCount = 0;

   PdfObject *pObject = FindObjectByNamedDictionaryEntry("AddedStreams","cvStreams");

   if ( ! pObject )
      return S_OK;

   BYTE *pValues = pObject -> Value("entries");
   if ( ! pValues )
      return S_OK;

   char *p = (char *)pValues + strlen((char *)pValues);

   while ( 1 ) {
   
      p--;

      while ( ',' != *p && '(' != *p && '|' != *p && '@' != *p )
         p--;

      if ( '|' == *p )
         break;

      if ( '@' == *p ) {
         *pCount = *pCount + 1;
         p--;
      }

      if ( '(' == *p ) 
         break;

   }

   return S_OK;
   }


   HRESULT PdfDocument::Seal() {

   PdfObject *pObject = FindObjectByNamedDictionaryEntry("AddedStreams","cvStreams");

   if ( ! pObject )
      return E_FAIL;

   InsertIntoStreamList("CVAddedStreamList","|");
   InsertIntoStreamList("CVAddedStreamListIDs","|");

   BYTE *pValues = pObject -> Value("entries");

   if ( ! pValues ) {
      long objectId = NewObjectId();
      char szObject[MAX_PATH];
      sprintf(szObject,"%ld 0 obj<</AddedStreams cvStreams/entries (|)>>endobj",objectId);
      pObject = new PdfObject(this,(BYTE *)szObject,(long)strlen(szObject));
      AddObject(pObject);
      if ( XReference() )
         XReference() -> AddObject(pObject);
      return S_OK;
   }

   BYTE *pNewValues = new BYTE[strlen((char *)pValues) + 3];

   memset(pNewValues,0,strlen((char *)pValues) + 3);
   memcpy(pNewValues,pValues,strlen((char *)pValues));

   if ( '|' == pNewValues[strlen((char *)pValues) - 1] ) {
      delete [] pNewValues;
      return S_OK;
   }

   pNewValues[strlen((char *)pValues)] = ',';
   pNewValues[strlen((char *)pValues) + 1] = '|';

   pObject -> SetValue("entries",pNewValues);

   delete [] pNewValues;

   return S_OK;
   }


   HRESULT PdfDocument::FirstPage(IPdfPage **ppReturnedPage) {
   if ( ! ppReturnedPage )
      return E_POINTER;
   *ppReturnedPage = NULL;
   if ( 0 == pages.size() )
      return E_FAIL;
   for ( std::map<long,PdfPage *>::iterator it = pages.begin(); it != pages.end(); it++ ) {
      PdfPage *pFirstPage = (*it).second;
      return pFirstPage -> QueryInterface(IID_IPdfPage,reinterpret_cast<void **>(ppReturnedPage));
   }
   return NULL;
   }


   HRESULT PdfDocument::LastPage(IPdfPage **ppReturnedPage) {
   if ( ! ppReturnedPage )
      return E_POINTER;
   *ppReturnedPage = NULL;
   long maxPageNumber = -1L;
   long maxPageIndex = -1L;
   for ( std::map<long,PdfPage *>::iterator it = pages.begin(); it != pages.end(); it++ ) {
      PdfPage *pPage = (*it).second;
      if ( pPage -> PageNumber() > maxPageNumber ) {
         maxPageNumber = pPage -> PageNumber();
         maxPageIndex = (*it).first;
      }
   }
   if ( -1L == maxPageIndex ) 
      return E_FAIL;
   PdfPage *pLastPage = pages[maxPageIndex];
   return pLastPage -> QueryInterface(IID_IPdfPage,reinterpret_cast<void **>(ppReturnedPage));
   }


   HRESULT PdfDocument::AddPage(long *pPageNumber,IPdfPage **ppReturnedPage) {

   char szTemp[1024];

   IPdfPage *pIPdfPage = NULL;

   if ( ! ( S_OK == LastPage(&pIPdfPage) ) )
      return E_FAIL;

   PdfPage *pLastPage = static_cast<PdfPage *>(pIPdfPage);
   
   RECT pageSize;
   double rotation;

   pLastPage -> Rotation(&rotation);

   pLastPage -> PageSize(&pageSize);
   
   BYTE *parentReference = pLastPage -> Object() -> Dictionary() -> Value("Parent");

   PdfObject *pContents = FindObjectById(atol((char *)parentReference));

   if ( ! pContents ) {
      pIPdfPage -> Release();
      return E_FAIL;
   }

   sprintf(szTemp,"%ld 0 obj"
         "<</Contents [ ]/CropBox[0.0 0.0 %5.2lf %5.2lf]/MediaBox[0.0 0.0 %5.2lf %5.2lf]/Parent %s/Rotate %lf/Type/Page>>endobj",
               NewObjectId(),(double)(pageSize.right - pageSize.left),(double)(pageSize.bottom - pageSize.top),
                        (double)(pageSize.right - pageSize.left),(double)(pageSize.bottom - pageSize.top),(char *)parentReference,rotation);

   PdfObject *pNewPageObject = new PdfObject(this,(BYTE *)szTemp,(long)strlen(szTemp));

   AddObject(pNewPageObject);

   if ( XReference() )
      XReference() -> AddObject(pNewPageObject);

   BYTE *pKids = pContents -> Dictionary() -> Value("Kids");

   pContents -> Dictionary() -> SetValue("Kids",pdfUtility.addReferenceToArray(pNewPageObject -> Id(),pKids));

   BYTE *pCount = pContents -> Dictionary() -> Value("Count");

   long oldCount = atol((char *)pCount);

   char szCount[32];

   sprintf(szCount,"%ld",oldCount + 1);

   pContents -> Dictionary() -> SetValue("Count",(BYTE *)szCount);

   long pn;
   pIPdfPage -> get_PageNumber(&pn);

   PdfPage *pNewPage = new PdfPage(this,pn + 1,pNewPageObject);

   pages[++pageObjectCount] = pNewPage;

   if ( pPageNumber )
      *pPageNumber = pn + 1;

   if ( ppReturnedPage )
      pNewPage -> QueryInterface(IID_IPdfPage,reinterpret_cast<void **>(ppReturnedPage));

   pIPdfPage -> Release();

   return S_OK;
   }


   HRESULT PdfDocument::ReplicateStream(long streamNumber,long targetPage,POINT *pOriginalSource,POINT *pOffset,double scaleX,double scaleY) {

   PdfObject *pAddedStreamList = FindObjectByDictionaryEntry("CVAddedStreamList");

   if ( ! pAddedStreamList )
      return E_FAIL;

   PdfDictionary *pAddedStreamListDictionary = pAddedStreamList -> Dictionary();

   if ( ! pAddedStreamListDictionary )
      return E_FAIL;
   
   BYTE *pAddedStreamListValue = pAddedStreamListDictionary -> Value("CVAddedStreamList");

   if ( ! pAddedStreamListValue )
      return E_FAIL;

   long existingObjectId = pdfUtility.IntegerValueFromArray((char *)pAddedStreamListValue,streamNumber);

   PdfObject *pExistingStream = FindObjectById(existingObjectId);

   if ( ! pExistingStream )
      return E_FAIL;

   PdfPage *pPage = GetPage(targetPage,NULL);

   if ( ! pPage )
      return E_FAIL;

   double rotation = 0.0;
   RECT rcPage;

   pPage -> getRotation(&rotation);

   pPage -> getPageSize(&rcPage);

   PdfStream *pStream = pExistingStream -> Stream();

   long n = pStream -> BinaryDataSize();

   BYTE *pNewStream = new BYTE[n + 256];

   memset(pNewStream,0,(n + 256) * sizeof(BYTE));

   BYTE *pSource = pStream -> Storage();

   pNewStream[0] = '\0';

   double xOriginal = pOriginalSource -> x;
   double yOriginal = pOriginalSource -> y;
   long offsetX = pOffset -> x;
   long offsetY = pOffset -> y;

   sprintf((char *)pNewStream + strlen((char *)pNewStream)," q ");

   if ( 90.0 == rotation ) {
      long t = offsetX;
      offsetX = -offsetY;
      offsetY = t;
      double td = xOriginal;
      xOriginal = rcPage.right - yOriginal;
      yOriginal = td;
   } else if ( 180.0 == rotation ) {
      offsetX = -offsetX;
      offsetY = -offsetY;
      xOriginal = xOriginal - rcPage.right;
      yOriginal = yOriginal - rcPage.bottom;
   }

   sprintf((char *)pNewStream + strlen((char *)pNewStream)," %lf 0 0 %lf 0 0 cm \n",scaleX,scaleY);
   sprintf((char *)pNewStream + strlen((char *)pNewStream)," 1 0 0 1 %lf %lf cm \n",-(double)xOriginal,-(double)yOriginal);
   sprintf((char *)pNewStream + strlen((char *)pNewStream)," 1 0 0 1 %lf %lf cm \n",(double)xOriginal / scaleX,yOriginal / scaleY);
   sprintf((char *)pNewStream + strlen((char *)pNewStream)," 1 0 0 1 %lf %lf cm \n",(double)offsetX / scaleX,(double)offsetY / scaleY);

   BYTE *pTarget = pNewStream + strlen((char *)pNewStream);

   memcpy(pTarget,pSource,n);

   sprintf((char *)pTarget + n," Q ");

   long streamLength = (long)strlen((char *)pNewStream);

   BYTE *pNewObject = new BYTE[streamLength + 256];

   memset(pNewObject,0,(streamLength + 256) * sizeof(BYTE));

   sprintf((char *)pNewObject,"<</Length %ld>>\nstream\n",streamLength);
   
   memcpy(pNewObject + strlen((char *)pNewObject),pNewStream,streamLength);

   sprintf((char *)pNewObject + strlen((char *)pNewObject),"\nendstream\nendobj\n");

   streamLength = (long)strlen((char *)pNewObject);

   long objectId = NewObjectId();

   PdfObject *pAddedObject = new PdfObject(this,objectId,0,pNewObject,streamLength);

   AddObject(pAddedObject);

   if ( XReference() )
      XReference() -> AddObject(pAddedObject);

   pPage -> AddContentsReference(pAddedObject);

   delete [] pNewObject;

   delete [] pNewStream;

   return S_OK;
   }


   HRESULT PdfDocument::ParseText(long pageNumber,HDC hdc,RECT *prcWindowsClip,void *pvIPostScriptTakeText) {
   return parseText(pageNumber,hdc,prcWindowsClip,pvIPostScriptTakeText);
   }

   HRESULT PdfDocument::GetLastError(char **ppszError) {
   if ( ! ppszError )
      return E_POINTER;
   *ppszError = szErrorMessage;
   return S_OK;
   }

   HRESULT PdfDocument::ExtractFonts(char *pszDirectory,char **ppszRootNamesList) {

   if ( ! pszDirectory )
      return E_POINTER;

   if ( ! pszDirectory[0] )
      return E_INVALIDARG;

   if ( ! ppszRootNamesList )
      return E_POINTER;

   *ppszRootNamesList = NULL;

   long namesSize = 0;

   for ( std::list<PdfObject *>::iterator it = objectList.begin(); it != objectList.end(); it++ ) {

      PdfObject *pObject = (*it);

      if ( ! pObject -> Dictionary() )
         continue;

      PdfElement *pFontName = pObject -> Dictionary() -> Element("FontName");

      if ( ! pFontName )
         continue;

      bool isType1 = false;
      bool isType2 = false;
      bool isType3 = false;

      PdfElement *pFontFile = pObject -> Dictionary() -> Element("FontFile");

      if ( ! pFontFile ) {
         pFontFile = pObject -> Dictionary() -> Element("FontFile2");
         if ( pFontFile )
            isType2 = true;
      } else
         isType1 = true;

      if ( ! pFontFile ) {
         pFontFile = pObject -> Dictionary() -> Element("FontFile3");
         if ( pFontFile )
            isType3 = true;
      }

      if ( ! pFontFile )
         continue;

      if ( pObject -> Dictionary() -> Element("ToUnicode") )
         continue;

      BYTE *pValue = pFontFile->Value();

      if ( ! pValue )
         continue;

      char *p = (char *)pValue;

      while ( ' ' == *p )
         p++;
      
      if ( ! isdigit(*p) )
         continue;

      char *pIdStart = p;

      while ( isdigit(*p) )
         p++;

      if ( ! ( ' ' == *p ) )
         continue;

      *p = '\0';

      long id = atol(pIdStart);

      *p = ' ';

      pObject = objectMap[id];

      PdfStream *pStream = pObject -> Stream();

      if ( ! pStream )
         continue;

      char szFileName[MAX_PATH];

      char *pFileName = (char *)pFontName -> Value();

      if ( ! pFileName )
         continue;

      if ( '/' == pFileName[0] )
         pFileName++;

      p = strrchr(pFileName,'+');
      if ( p )
         pFileName = p + 1;

      p = pFileName + strlen(pFileName);

      while ( ! isalpha(*p) ) {
         *p = '\0';
         p--;
      }

      if ( isType1 )
         sprintf(szFileName,"%s\\%s.pfm",pszDirectory,pFileName);
      else if ( isType2 )
         sprintf(szFileName,"%s\\%s.ttf",pszDirectory,pFileName);
      else if ( isType3 )
         sprintf(szFileName,"%s\\%s.pfa",pszDirectory,pFileName);
      else
         continue;

      FILE *fFont = fopen(szFileName,"wb");

      pStream -> Decompress();

      fwrite(pStream -> DecompressedStorage(),1,pStream -> BinaryDataSize(),fFont);

      fclose(fFont);

      int n = (long)strlen(szFileName);

      char *pNewName = new char[n + 3];

      strcpy(pNewName,szFileName);

      pNewName[n + 1] = '\0';
      pNewName[n + 2] = '\0';

      if ( ! * ppszRootNamesList ) {
         *ppszRootNamesList = pNewName;
         namesSize = n + 1;
         continue;
      }

      char *pNames = new char[namesSize + n + 3  + 100];

      memcpy(pNames,*ppszRootNamesList,namesSize);

      strcpy(pNames + namesSize,pNewName);

      namesSize += (long)strlen(pNewName) + 1;

      pNames[namesSize] = '\0';

      delete [] *ppszRootNamesList;
      delete [] pNewName;
   
      *ppszRootNamesList = pNames;

   }

   return S_OK;
   }