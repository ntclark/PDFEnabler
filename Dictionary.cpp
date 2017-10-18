// Copyright 2017 InnoVisioNate Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Library.h"

#include "Object.h"
#include "Dictionary.h"
#include "Document.h"

//
// Do not use this constructor to parse a dictionary, it assumes all of the
// specified data is the dictionary
//
   PdfDictionary::PdfDictionary(PdfObject *po,BYTE *pStart,long dataSize) :

      PdfEntity(pStart,dataSize),
   
      pParentObject(po),

      modified(false)

   {
   createElements();
   return;
   }


   PdfDictionary::PdfDictionary(PdfObject *po,BYTE *pStart,BYTE *pLast) :

      PdfEntity(edDictionary,esdScanDown,pStart,pLast),

      pParentObject(po),

      modified(false)

   {
   createElements();
   return;
   }


   PdfDictionary::PdfDictionary(PdfDictionary &copyFrom) :

//      PdfEntity(copyFrom),
      PdfEntity(edDictionary,esdScanDown,copyFrom.Storage(),copyFrom.Storage() + copyFrom.BinaryDataSize()),
      pParentObject(copyFrom.pParentObject),
      modified(copyFrom.modified)

   {
   createElements();
   return;
   }


   long PdfDictionary::createElements() {

   if ( ! BinaryDataSize() )
      return 0;

   for ( std::list<PdfElement *>::iterator it = elementList.begin(); it != elementList.end(); it++ ) 
      delete (*it);

   elementMap.clear();
   elementList.clear();

   long delimiterSize = (long)strlen(entityDelimiters[edDictionary]);

   BYTE *pTop = Storage() + delimiterSize;
   BYTE *pFirst = pTop;

   delimiterSize += (long)strlen(entityDelimiters[edDictionary] + delimiterSize + 1);

   BYTE *pBottom = pTop + BinaryDataSize() - delimiterSize;
   while ( pBottom > pTop && strncmp(">>",(char *)pBottom,2) )
      pBottom--;
   
   long lengthFound = 0;

   while ( pTop < pBottom ) {

      PdfElement *pElement = new PdfElement(pParentObject,this,pTop,pBottom);

      if ( ! pElement -> BinaryDataSize() ) {
         delete pElement;
         break;
      }

      elementMap[pdfUtility.HashCode(pElement -> Name())] = pElement;
      elementList.insert(elementList.end(),pElement);

      lengthFound += pElement -> BinaryDataSize();

      pTop = pFirst + lengthFound;

   }

   return 1;

   }


   PdfElement *PdfDictionary::Element(char *pszKeyName) {
   long hashCode = pdfUtility.HashCode(pszKeyName);
   if ( elementMap.find(hashCode) == elementMap.end() )
      return NULL;
   PdfElement *pElement = elementMap[hashCode];
   return pElement;
   }


   long PdfDictionary::ValueSize(char *pszKeyName) {
   PdfElement *pElement = Element(pszKeyName);
   if ( pElement )
      return pElement -> ValueSize();
   return 0;
   }



   BYTE *PdfDictionary::Value(char *pszKeyName) {

   PdfElement *pElement = Element(pszKeyName);

   if ( pElement ) {
#if 0
      if ( pdfUtility.IsIndirect((char *)pElement -> Value()) ) {
         PdfObject *pReferencedObject = pParentObject -> Document() -> IndirectObject((char *)pElement -> Value());
         if ( ! pReferencedObject ) 
            return REFERENCE_TO_NONLOADED_OBJECT;
         return pReferencedObject -> Dictionary() -> Value(pszKeyName);
      }
#endif
      return pElement -> Value();
   }

   char *p = strchr(pszKeyName,':');

   if ( p ) {

      *p = '\0';

      pElement = Element(pszKeyName);

      *p = ':';

      if ( pElement && pElement -> Dictionary() )
         return pElement -> Dictionary() -> Value(p + 1);

   }

   return NULL;
   }


   BYTE *PdfDictionary::DeReferencedValue(char *pszKeyName) {

   PdfElement *pElement = Element(pszKeyName);

   if ( pElement ) {
      if ( pdfUtility.IsIndirect((char *)pElement -> Value()) ) {
         PdfObject *pReferencedObject = pParentObject -> Document() -> IndirectObject((char *)pElement -> Value());
         if ( ! pReferencedObject ) 
            return (BYTE *)REFERENCE_TO_NONLOADED_OBJECT;
         if ( pReferencedObject -> Dictionary() ) {
            BYTE *pTest = pReferencedObject -> DeReferencedValue(pszKeyName);
            if ( pTest )
               return pTest;
         }
         return pReferencedObject -> Storage();
      }
      return pElement -> Value();
   }

   char *p = strchr(pszKeyName,':');

   if ( p ) {

      *p = '\0';

      pElement = Element(pszKeyName);

      *p = ':';

      if ( pElement && pElement -> Dictionary() )
         return pElement -> Dictionary() -> Value(p + 1);

   }

   return NULL;
   }


   PdfObject *PdfDictionary::Object(char *pszKeyName) {

   PdfElement *pElement = Element(pszKeyName);

   if ( ! pElement ) 
      return NULL;

   if ( pdfUtility.IsIndirect((char *)pElement -> Value()) ) 
      return pParentObject -> Document() -> IndirectObject((char *)pElement -> Value());

   return NULL;
   }


   long PdfDictionary::AddValue(char *pszValue) {

   BYTE *pTop = (BYTE *)pszValue;
   BYTE *pBottom = pTop + strlen(pszValue);

   PdfElement *pElement = new PdfElement(pParentObject,this,pTop,pBottom);

   if ( ! pElement -> BinaryDataSize() ) {
      delete pElement;
      return 0L;
   }

   elementMap[pdfUtility.HashCode(pElement -> Name())] = pElement;
   elementList.insert(elementList.end(),pElement);

   long n = StringWrite(NULL,true);

   BYTE *pNewData = new BYTE[n];
   StringWrite((char *)pNewData,false);
   ReallocateStorage(pNewData,n,0);

   return 1L;
   }


   long PdfDictionary::RemoveValue(char *pszKeyName) {

   long hashCode = pdfUtility.HashCode(pszKeyName);

#if 1
   if ( elementMap.find(hashCode) == elementMap.end() ) {

      char szTemp[MAX_PATH];
      strcpy(szTemp,pszKeyName);

      char *p = strchr(szTemp,':');
      if ( ! p )
         return 0;

      *p = '\0';

      char *pszElementName = p + 1;
      char *pszContainingDictionary = szTemp;

      hashCode = pdfUtility.HashCode(pszContainingDictionary);
#endif

      if ( elementMap.find(hashCode) == elementMap.end() )
         return 0;

#if 1
      PdfElement *pParentElement = (*elementMap.find(hashCode)).second;

      PdfDictionary *pDictionary = pParentElement -> Dictionary(); // This is the /DecodeParms dictionary

      if ( ! pDictionary )
         return 0;

      BYTE *pParentBytes = Storage();
      BYTE *pElementBytes = pDictionary -> Storage();
      long nameSize = (long)strlen(pszContainingDictionary);

      BYTE *pStart = pParentBytes;
      BYTE *pEnd = pParentBytes + BinaryDataSize();

      while ( pStart < pEnd && memcmp(pStart,pszContainingDictionary,nameSize) ) 
         pStart++;

      if ( pStart >= pEnd )
         return pDictionary -> RemoveValue(pszElementName);

      //pStart points to the main dictionary

      long innerHashCode = pdfUtility.HashCode(pszElementName);

      std::map<long,PdfElement *> *pInnerMap = &pDictionary -> elementMap;
      std::map<long,PdfElement *>::iterator it = pInnerMap -> find(innerHashCode);

      PdfElement *pElement = (*it).second;

      // pElement is the Predictor element

      while ( pStart < pEnd && memcmp(pStart,pElement -> Storage(),pElement -> BinaryDataSize()) )
         pStart++;

      if ( pStart >= pEnd )
         return pDictionary -> RemoveValue(pszElementName);

      memset(pStart,' ',pElement -> BinaryDataSize());

      pStart = pParentElement -> Storage();
      pEnd = pParentBytes + pParentElement -> BinaryDataSize();

      while ( pStart < pEnd && memcmp(pStart,pElement -> Storage(),pElement -> BinaryDataSize()) )
         pStart++;

      if ( pStart >= pEnd )
         return pDictionary -> RemoveValue(pszElementName);

      memset(pStart,' ',pElement -> BinaryDataSize());

      return pDictionary -> RemoveValue(pszElementName);
   }

#endif

   PdfElement *p = (*elementMap.find(hashCode)).second;

#if 1

   BYTE *pParentBytes = Storage();
   BYTE *pElementBytes = p -> Storage();

   BYTE *pStart = pParentBytes;
   BYTE *pEnd = pParentBytes + BinaryDataSize();

   while ( pStart < pEnd && memcmp(pStart,pElementBytes,p -> BinaryDataSize()) ) 
      pStart++;

   if ( pStart != pEnd ) 
      memset(pStart,' ',p -> BinaryDataSize());

#endif   

   elementList.remove(p);
   elementMap.erase(hashCode);

   delete p;

   return 1;
   }



   PdfDictionary *PdfDictionary::GetDictionary(char *pszKeyName) {

   long hashCode = pdfUtility.HashCode(pszKeyName);

   if ( elementMap.find(hashCode) == elementMap.end() )
      return NULL;

   PdfElement *pElement = elementMap[hashCode];

   PdfDictionary *pResult = pElement -> Dictionary();

   if ( pResult )
      return pResult;

   if ( pdfUtility.IsIndirect((char *)pElement -> Value()) ) 
      return Object() -> Document() -> FindObjectById(atol((char *)pElement -> Value())) -> Dictionary();

   return NULL;
   }


#if 0
   PdfNumberTree *PdfDictionary::NumberTree() {
   if ( ! pNumberTree )
      pNumberTree = new PdfNumberTree(this);
   return pNumberTree;
   }
#endif



   long PdfDictionary::SetValue(char *pszKeyName,long value,bool autoAdd) {
   char szTemp[32];
   sprintf(szTemp,"%ld",value);
   return SetValue(pszKeyName,(BYTE *)szTemp,autoAdd);
   }


   long PdfDictionary::SetValue(char *pszKeyName,BYTE *pszValue,bool autoAdd) {

/*
   NTC: 2-14-2010: There is a bug in this function. When an element is removed from a dictionary (RemoveValue)
   only it's previously parsed element is removed from the element list.
   However, this function re-parses the original data, essentially putting the removed item back in the list
*/

   PdfElement *pElement = NULL;

   long hashCode = pdfUtility.HashCode(pszKeyName);

   if ( elementMap.find(hashCode) != elementMap.end() )
      pElement = elementMap[hashCode];
   else 
      if ( ! autoAdd )
         return 0;
   
   long totalSize = 0;

   long crlfSize = sizeof("\n");

   for ( std::list<PdfElement *>::iterator it = elementList.begin(); it != elementList.end(); it++ )
      totalSize += (*it) -> StringWrite(NULL,true);

   if ( pElement )
      totalSize -= pElement -> StringWrite(NULL,true);

   totalSize += (long)strlen(pszKeyName) + (long)strlen(entityDelimiters[edElement]);
   totalSize += (long)strlen((char *)pszValue) + 1 + crlfSize + 2 * (long)strlen(entityDelimiters[edDictionary]);

   BYTE *pNewData = new BYTE[totalSize + 1];

   memset(pNewData,0,(totalSize + 1) * sizeof(char));

   BYTE *p = pNewData;

   long bytesWritten = sprintf((char *)p,"%s",entityDelimiters[edDictionary]);

   p += bytesWritten;

   for ( std::list<PdfElement *>::iterator it = elementList.begin(); it != elementList.end(); it++ ) {

      PdfElement *pe = (*it);

      if ( pe != pElement ) {
#if 0
         memcpy(p,pe -> Storage(),pe -> BinaryDataSize());
         bytesWritten += pe -> BinaryDataSize();
#else
         bytesWritten += pe -> StringWrite((char *)p,false);
#endif
      } else {

//
// 3-7-2010: It is not clear why the space is not written between an array value and it's element name.
// I have commented this out
//         if ( '[' == pszValue[0] )
//            bytesWritten += sprintf((char *)p,"/%s%s",pszKeyName,(char *)pszValue);
//         else
//
         if ( pElement -> ValueType() == PdfElement::vtString )
            bytesWritten += sprintf((char *)p,"/%s (%s)",pszKeyName,(char *)pszValue);
         else
            bytesWritten += sprintf((char *)p,"/%s %s",pszKeyName,(char *)pszValue);

      }

      p = pNewData + bytesWritten;

   }
   
   if ( ! pElement )
      bytesWritten += sprintf((char *)p,"/%s %s",pszKeyName,(char *)pszValue);

   bytesWritten += sprintf((char *)(pNewData + bytesWritten),"%s",entityDelimiters[edDictionary] + strlen(entityDelimiters[edDictionary]) + 1);

   ReallocateStorage(pNewData,bytesWritten,0);

   createElements();

   return 1;
   }


   void PdfDictionary::SetLength(long newValue) {

   BYTE *pValue = Value("Length");

   if ( ! pValue )
      return;

   if ( ! pdfUtility.IsIndirect((char *)pValue) ) {
      SetValue("Length",newValue);
      return;
   }

   long objectId = atol((char *)pValue);
   PdfObject *pReferencedObject = pParentObject -> Document() -> FindObjectById(objectId);
   if ( ! pReferencedObject )
      return;

   char szTemp[32];
   sprintf(szTemp,"%ld%c",newValue,eol);

   pReferencedObject -> ReallocateStorage((BYTE *)szTemp,(long)strlen(szTemp),0);

   return;
   }


   void PdfDictionary::Decrypt() {
   for ( std::list<PdfElement *>::iterator it = elementList.begin(); it != elementList.end(); it++ )
      (*it) -> Decrypt();
   return;
   }


   void PdfDictionary::Encrypt() {
   for ( std::list<PdfElement *>::iterator it = elementList.begin(); it != elementList.end(); it++ )
      (*it) -> Encrypt();
   return;
   }


   PdfDictionary::~PdfDictionary() {
   for ( std::list<PdfElement *>::iterator it = elementList.begin(); it != elementList.end(); it++ )
      delete (*it);
   elementMap.clear();
   elementList.clear();
   }


   long PdfDictionary::StringWrite(char *pString,bool sizeOnly) {

   long bytesWritten = 0L;

   bytesWritten += ! sizeOnly ? (long)sprintf(pString,entityDelimiters[edDictionary]) : (long)strlen(entityDelimiters[edDictionary]);

   for ( std::list<PdfElement *>::iterator it = elementList.begin(); it != elementList.end(); it++ )
      bytesWritten += (*it) -> StringWrite(pString + bytesWritten,sizeOnly);

   return bytesWritten + ( ! sizeOnly ? (long)sprintf(pString + bytesWritten,"%s",entityDelimiters[edDictionary] + (long)strlen(entityDelimiters[edDictionary]) + 1) : 
                                       (long)strlen(entityDelimiters[edDictionary] + (long)strlen(entityDelimiters[edDictionary]) + 1) );
   }


   long PdfDictionary::Write(FILE *fOutput,bool writeUncompressed) {

   long bytesWritten = fprintf(fOutput,"%s",entityDelimiters[edDictionary]);

   for ( std::list<PdfElement *>::iterator it = elementList.begin(); it != elementList.end(); it++ ) {
      if ( writeUncompressed ) {
         if ( 0 == strcmp((char *)((*it) -> Name()),"Filter") )
            continue;
      }
      bytesWritten += (*it) -> Write(fOutput,writeUncompressed);
   }


// Change made on 8/17/09: Is it okay to not print cr-lf after dictionary ?
#if 0
   return bytesWritten + fprintf(fOutput,"%s%c",entityDelimiters[edDictionary] + strlen(entityDelimiters[edDictionary]) + 1,eol);
#else
   return bytesWritten + fprintf(fOutput,"%s",entityDelimiters[edDictionary] + strlen(entityDelimiters[edDictionary]) + 1);
#endif

   }