// Copyright 2017 InnoVisioNate Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Library.h"

#include "XReference.h"


   PdfCrossReferenceStream::PdfCrossReferenceStream(PdfObject *p ) : 
      pObject(p), 
      pPreviousXRefStream(NULL), 
      pNextXRefStream(NULL), 
      initialSizeValue(0), 
      initialEntriesCount(0) 
   {
   memset(szInitialWArray,0,sizeof(szInitialWArray));
   return;
   }


   PdfCrossReferenceStream::~PdfCrossReferenceStream() {
   for ( std::list<xrefEntry *>::iterator it = entries.begin(); it != entries.end(); it++ )
      delete (*it);
   entries.clear();
   for ( std::list<xrefEntrySubSection *>::iterator it = entrySubSections.begin(); it != entrySubSections.end(); it++ )
      delete (*it);
   entrySubSections.clear();
   return;
   }


   long PdfCrossReferenceStream::maxObjectId() {
   if ( 0 == entrySubSections.size() ) {
      long maxValue = -32768;
      for ( std::list<xrefEntry *>::iterator it = entries.begin(); it != entries.end(); it++ )
         if ( (*it) -> objectId > maxValue )
            maxValue = (*it) -> objectId;
      return maxValue;
   }
   xrefEntrySubSection *p = entrySubSections.back();
   return p -> firstObjectId + p -> objectCount - 1;
   }


   long PdfCrossReferenceStream::addEntry(xrefEntry *pEntry) {
   if ( 0 == entrySubSections.size() ) {
      entries.insert(entries.end(),pEntry);
      return 0L;
   }
   xrefEntrySubSection *pSubSection = entrySubSections.back();
   pSubSection -> objectCount++;
   entries.insert(entries.end(),pEntry);
   return 1L;
   }