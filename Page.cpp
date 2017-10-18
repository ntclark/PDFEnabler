// Copyright 2017 InnoVisioNate Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdfEnabler.h"

#include "Library.h"

#include "Page.h"
#include "Document.h"

   char PdfPage::szErrorMessage[1024];

   PdfPage::PdfPage(PdfDocument *pp,long pn,PdfObject *po) :

      pDocument(pp),
      pContentsObject(po),
      pageNumber(pn),
      refCount(1)

   {
   }


   PdfPage::~PdfPage() {
   }


   long PdfPage::getPageSize(RECT *pRect) {

   PdfObject *pAncestor = pContentsObject;

   BYTE *pValue = pAncestor -> DeReferencedValue("MediaBox");

   while ( ! pValue ) {

      pAncestor = pAncestor -> DeReferencedObject("Parent");

      if ( ! pAncestor ) {
         memset(pRect,0,sizeof(RECT));
         return 0;
      }

      pValue = pAncestor -> DeReferencedValue("MediaBox");

   }

   if ( ! pValue ) {
      memset(pRect,0,sizeof(RECT));
      return 0;
   }

   pRect -> left = pdfUtility.IntegerValueFromArray((char *)pValue,1);
   pRect -> top = pdfUtility.IntegerValueFromArray((char *)pValue,2);
   pRect -> right = pdfUtility.IntegerValueFromArray((char *)pValue,3);
   pRect -> bottom = pdfUtility.IntegerValueFromArray((char *)pValue,4);

   return 1;
   }


   long PdfPage::getRotation(double *pRotation) {

   BYTE *pValue = pContentsObject -> Value("Rotate");

   if ( ! pValue ) {

      pValue = pContentsObject -> Value("Parent");

      if ( pValue ) {
         PdfObject *pParentObject = pContentsObject -> DeReferencedObject("Parent");
         if ( pParentObject )
            pValue = pParentObject -> DeReferencedValue("Rotate");
         else
            pValue = NULL;
      } 

   }

   if ( ! pValue ) {
      *pRotation = 0.0;
      return 0;
   }

   *pRotation = atof((char *)pValue);

   return 1;
   }

   
   long PdfPage::addStreamInternal(BYTE *pData,long dataSize,long identifier,long embedCountOverride) {

#ifdef DOUBLE_EMBED
   long embedCount = 2;
#else
   long embedCount = 1;
#endif

   if ( -1L != embedCountOverride )
      embedCount = embedCountOverride;

   long objectId = 0L;

   long newObjectIds[] = {0,0,0};

   for ( long embedIndex = 0; embedIndex < embedCount; embedIndex ++ ) {

      objectId = pDocument -> NewObjectId();

      newObjectIds[embedIndex] = objectId;

      BYTE *pNewData = new BYTE[dataSize + 256];

      memset(pNewData,0,(dataSize + 256) * sizeof(BYTE));

      long streamLength = sprintf((char *)pNewData,"<</Length %ld>>\nstream\n",dataSize);

      memcpy(pNewData + streamLength,pData,dataSize);

#if 1
      if ( embedIndex ) {
         char *px = strstr((char *)pNewData,"0.0 SC");
         if ( px ) {
            for ( long k = 0; k < 7; k++ )
               px[k] = ' ';
         }
      }
#endif

      streamLength += dataSize;

      streamLength += sprintf((char *)(pNewData + streamLength),"\nendstream\nendobj\n");

      PdfObject *pNewObject = new PdfObject(pDocument,objectId,0,pNewData,streamLength);

      delete [] pNewData;

      pDocument -> AddObject(pNewObject);

      if ( pDocument -> XReference() )
         pDocument -> XReference() -> AddObject(pNewObject);

      BYTE *pValue = pContentsObject -> Value("Contents");

      if ( ! pValue ) {
         char szTemp[32];
         sprintf(szTemp,"[ %ld 0 R ]",objectId);
         pContentsObject -> Dictionary() -> SetValue("Contents",(BYTE *)szTemp,true);
         pValue = pContentsObject -> Value("Contents");
         continue;
      }

      long n = (long)strlen((char *)pValue);

      char *pszCurrentContents = new char[n + 2];

      memset(pszCurrentContents,0,(n + 2) * sizeof(char));

      strcpy((char *)pszCurrentContents,(char *)pValue);

      BYTE *pStart = (BYTE *)pszCurrentContents;
      
      while ( ' ' == *pStart )
         pStart++;
      
      BYTE *p = pStart + strlen((char *)pStart) - 1;

      while ( *p == 0x0A || *p == 0x0D || ' ' == *p )
         p--;

      p++;

      *p = '\0';

      p--;

      char *pszFinalContents = new char[n + 64];
      memset(pszFinalContents,0,(n + 64) * sizeof(char));

      if ( ']' == *p ) {
         *p = '\0';
         p--;
         while ( p > pStart && ' ' == *p )
            p--;
         p++;
         *p = '\0';
         p = pStart;
         while ( ' ' == *p )
            p++;
         if ( '[' == *p )
            p++;
         while ( ' ' == *p )
            p++;
         if ( 0 == embedIndex )
            sprintf(pszFinalContents,"[ %ld 0 R %s ]",objectId,p);
         else
            sprintf(pszFinalContents,"[ %s %ld 0 R ]",p,objectId);
      } else {
         if ( 0 == embedIndex )
            sprintf(pszFinalContents,"[ %ld 0 R %s ]",objectId,pStart);
         else
            sprintf(pszFinalContents,"[ %s %ld 0 R ]",pStart,objectId);
      }

      pContentsObject -> SetValue("Contents",(BYTE *)pszFinalContents);

      delete [] pszCurrentContents;
      delete [] pszFinalContents;

   }

   char szX[32];
   sprintf(szX,"%ld",newObjectIds[0]);

   pDocument -> InsertIntoStreamList("CVAddedStreamList",szX);

   sprintf(szX,"%ld",identifier);

   pDocument -> InsertIntoStreamList("CVAddedStreamListIDs",szX);

   PdfObject *pAddedStreamsObject = pDocument -> FindObjectByNamedDictionaryEntry("AddedStreams","cvStreams");

   if ( ! pAddedStreamsObject ) {
      long objectId = pDocument -> NewObjectId();
      char szObject[MAX_PATH];
      sprintf(szObject,"%ld 0 obj<</AddedStreams cvStreams/entries (|)>>endobj",objectId);
      pAddedStreamsObject = new PdfObject(pDocument,(BYTE *)szObject,(long)strlen(szObject));
      pDocument -> AddObject(pAddedStreamsObject);
      if ( pDocument -> XReference() )
         pDocument -> XReference() -> AddObject(pAddedStreamsObject);
   }

   PdfDictionary *pDictionary = pAddedStreamsObject -> Dictionary();
   
   BYTE *pValue = pDictionary -> Value("entries");

   long n = (long)strlen((char *)pValue) + 32;

   BYTE *pNewValue = new BYTE[n];

   memset(pNewValue,0,n * sizeof(BYTE));

   memcpy(pNewValue,pValue,n - 32);

   sprintf((char *)pNewValue + strlen((char *)pNewValue),",@,%ld",newObjectIds[0]);

   if ( newObjectIds[1] )
      sprintf((char *)pNewValue + strlen((char *)pNewValue),",%ld",newObjectIds[1]);

   pDictionary -> SetValue("entries",pNewValue);

   delete [] pNewValue;

   return objectId;
   }


   long PdfPage::addContentsReference(PdfObject *pNewObject) {

   BYTE *pValue = pContentsObject -> Value("Contents");

   if ( ! pValue ) {
      char szTemp[32];
      sprintf(szTemp,"[ %ld 0 R ]",pNewObject -> Id());
      pContentsObject -> Dictionary() -> SetValue("Contents",(BYTE *)szTemp,true);
      return 1L;
   }

   pContentsObject -> SetValue("Contents",pdfUtility.addReferenceToArray(pNewObject -> Id(),pValue));

   return 0L;
   }


   PdfDictionary *PdfPage::findResource(char *pszResourceName,char *pszSubdictionaryName) {

   PdfDictionary *pResourceDictionary = pContentsObject -> Dictionary("Resources");

   if ( ! pResourceDictionary )
      return NULL;

   pResourceDictionary = pResourceDictionary -> GetDictionary(pszSubdictionaryName);

   if ( ! pResourceDictionary )
      return NULL;

   PdfElement *pElement = pResourceDictionary -> Element(pszResourceName);

   if ( pElement ) {
      if ( pdfUtility.IsIndirect((char *)pElement -> Value()) ) 
         return Object() -> Document() -> FindObjectById(atol((char *)pElement -> Value())) -> Dictionary();
      return new PdfDictionary(pResourceDictionary -> Object(),pElement -> Value(),pElement -> ValueSize());
   }

   return pResourceDictionary;
   }

