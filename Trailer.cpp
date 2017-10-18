// Copyright 2017 InnoVisioNate Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Library.h"

#include "XReference.h"

   PdfTrailer::PdfTrailer(PdfDocument *pp,BYTE *pDocumentStart) :

      pDocument(pp),
      pDictionary(NULL),
      pPreviousTrailer(NULL),
      pNextTrailer(NULL),
      crossReferenceStreamObjectId(0),
      initialObjectCount(0),
      initialSizeValue(0),
      fileOffset(0)

   {

   ADVANCE_THRU_EOL(pDocumentStart)

   pDictionary = new PdfDictionary(NULL,pDocumentStart,pDocument -> Storage() + pDocument -> BinaryDataSize());

   BYTE *pValue = pDictionary -> Value("Size");
   if ( pValue )
      initialSizeValue = (long)atof((char *)pValue);

   return;
   }


   PdfTrailer::PdfTrailer(PdfTrailer &copyFrom) :

      pDocument(copyFrom.pDocument),
      pPreviousTrailer(NULL),
      pNextTrailer(NULL),
      crossReferenceStreamObjectId(0),
      initialObjectCount(0),
      initialSizeValue(0),
      fileOffset(0)

   {
   pDictionary = new PdfDictionary(*copyFrom.pDictionary);
   return;
   }


   PdfTrailer::~PdfTrailer() {
   for ( std::list<xrefEntrySection *>::iterator it = entrySections.begin(); it != entrySections.end(); it++ )
      delete (*it);
   entrySections.clear();
   }


   BYTE *PdfTrailer::Value(char *pszName) {
   if ( pDictionary )
      return pDictionary -> Value(pszName);
   return NULL;
   }


   long PdfTrailer::SetValue(char *pszKeyName,BYTE *pszValue) {
   if ( pDictionary ) 
      return pDictionary -> SetValue(pszKeyName,pszValue);
   return 0L;
   }
