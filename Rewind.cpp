// Copyright 2017, 2018, 2019 InnoVisioNate Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Library.h"

#include "XReference.h"


   long PdfXReference::ReWind() {

   if ( 0 == xRefObjects.size() )
      return 0;

   char szTemp[1024];

   long totalMotion = 0;

   /*

      7.5.8 Cross-Reference Streams

      7.5.8.1  General

      Beginning with PDF 1.5, cross-reference information may be stored in a cross-reference 
      stream instead of in a cross-reference table. Cross-reference streams provide the 
      following advantages:

         •  A more compact representation of cross-reference information

         •  The ability to access compressed objects that are stored in object streams 
            (see 7.5.7, "Object Streams") and to allow new cross-reference entry types 
            to be added in the future

      Cross-reference streams are stream objects (see 7.3.8, "Stream Objects"), and 
      contain a dictionary and a data stream. Each cross-reference stream contains 
      the information equivalent to the cross-reference table (see 7.5.4, "Cross-Reference Table") 
      and trailer (see 7.5.5, "File Trailer") for one cross-reference section.

   */

   for ( std::list<PdfCrossReferenceStream *>::iterator itXRefObjects = xRefObjects.begin(); itXRefObjects != xRefObjects.end(); itXRefObjects++ ) {

      PdfCrossReferenceStream *pXRefStreamObject = (*itXRefObjects);

      PdfObject *pXRefObject = pXRefStreamObject -> pObject;

      PdfDictionary *pDictionary = pXRefObject -> Dictionary();

      if ( ! pDictionary )
         continue;

      if ( pXRefStreamObject -> pPreviousXRefStream ) {
         sprintf(szTemp,"%010ld",pXRefStreamObject -> pPreviousXRefStream -> pObject -> FileOffset());
         pXRefStreamObject -> pObject -> Dictionary() -> SetValue("Prev",(BYTE *)szTemp);
      }

      if ( 0 == pXRefStreamObject -> entries.size() )
         continue;

      /*

      7.5.8.2  Cross-Reference Stream Dictionary

      Cross-reference streams may contain the entries shown in Table 17 in 
      addition to the entries common to all streams (Table 5) and trailer 
      dictionaries (Table 15). Since some of the information in the cross-reference 
      stream is needed by the conforming reader to construct the index that allows 
      indirect references to be resolved, the entries in cross-reference streams 
      shall be subject to the following restrictions:

         •  The values of all entries shown in Table 17 shall be direct objects; 
            indirect references shall not be permitted. For arrays (the Index and W entries), 
            all of their elements shall be direct objects as well. If the stream is encoded, 
            the Filter and DecodeParms entries in Table 5 shall also be direct objects.

         •  Other cross-reference stream entries not listed in Table 17 may be indirect; 
            in fact, some (such as Root in Table 15) shall be indirect.

         •  The cross-reference stream shall not be encrypted and strings appearing in the 
            cross-reference stream dictionary shall not be encrypted. It shall not have a 
            Filter entry that specifies a Crypt filter

      */

      /*
         Table 17 – Additional entries specific to a cross-reference stream dictionary

         key         type     description

         Type        name     (Required) The type of PDF object that this dictionary describes; 
                              shall be XRef for a cross-reference stream.

         Size        integer  (Required) The number one greater than the highest object number 
                              used in this section or in any section for which this shall be an 
                              update. It shall be equivalent to the Size entry in a trailer dictionary.

         Index       array    (Optional) An array containing a pair of integers for each subsection 
                              in this section. The first integer shall be the first object number in 
                              the subsection; the second integer shall be the number of entries in the subsection
                              The array shall be sorted in ascending order by object number. Subsections 
                              cannot overlap; an object number may have at most one entry in a section.
                              Default value: [0 Size].

         Prev        integer  (Present only if the file has more than one cross-reference stream; 
                              not meaningful in hybrid-reference files; see 7.5.8.4, "Compatibility with 
                              Applications That Do Not Support Compressed Reference Streams") The byte 
                              offset in the decoded stream from the beginning of the file to the beginning 
                              of the previous cross-reference stream. This entry has the same function 
                              as the Prev entry in the trailer dictionary (Table 15).

         W           array    (Required) An array of integers representing the size of the fields in a 
                              single cross-reference entry. Table 18 describes the types of entries and 
                              their fields. For PDF 1.5, W always contains three integers; the value of 
                              each integer shall be the number of bytes (in the decoded stream) of the 
                              corresponding field.

                              EXAMPLE[1 2 1] means that the fields are one byte, two bytes, and one byte, respectively.

                              A value of zero for an element in the W array indicates that the corresponding 
                              field shall not be present in the stream, and the default value shall be used, 
                              if there is one. If the first element is zero, the type field shall not be 
                              present, and shall default to type 1.

                              The sum of the items shall be the total length of each entry; it can be used 
                              with the Index array to determine the starting position of each subsection.

                              Different cross-reference streams in a PDF file may use different values for W.

      */

      BYTE *pValue = pDictionary -> Value("W");

      long wArraySize = pdfUtility.ArraySize((char *)pValue);
   
      long *pWArray = new long[wArraySize];
   
      for ( long k = 0; k < wArraySize; k++ )
         pWArray[k] = pdfUtility.IntegerValueFromArray((char *)pValue,k + 1);

      long wArrayByteLength = pWArray[0] + pWArray[1] + pWArray[2];

      BYTE *pUncompressed = new BYTE[DEFLATOR_CHUNK_SIZE];

      memset(pUncompressed,0,DEFLATOR_CHUNK_SIZE * sizeof(BYTE));

      BYTE bValues[16];

      long firstObjectId = -1L;
      long subSectionObjectCount = 0;
      long maxId = -1L;

      long bytesCopied = 0;

      /*
         7.5.8.3  Cross-Reference Stream Data

         Each entry in a cross-reference stream shall have one or more fields, the first 
         of which designates the entry’s type (see Table 18). In PDF 1.5 through PDF 1.7, 
         only types 0, 1, and 2 are allowed. Any other value shall be interpreted as a 
         reference to the null object, thus permitting new entry types to be defined in the future.

         The fields are written in increasing order of field number; the length of each 
         field shall be determined by the corresponding value in the W entry (see Table 17). 
         Fields requiring more than one byte are stored with the high-order byte first.

      */

      for ( xrefEntry *pe : pXRefStreamObject -> entries ) {

         /*
            Table 18 – Entries in a cross-reference stream

            Type  Field Description

         */

         switch ( pe -> type ) {

         case 0: {

            /*
            0     1     The type of this entry, which shall be 0. Type 0 entries define the 
                        linked list of free objects (corresponding to f entries in a cross-reference table).
                  2     The object number of the next free object.
                  3     The generation number to use if this object number is used again.
            */

            bValues[0] = 0x00;

            long v = htonl(pe -> objectId);

            BYTE *pV = reinterpret_cast<BYTE *>(&v) + (sizeof(long) - pWArray[1]);

            switch ( pWArray[1] ) {
            case 4:
               bValues[pWArray[0] + 3] = pV[3];

            case 3:
               bValues[pWArray[0] + 2] = pV[2];

            case 2:
               bValues[pWArray[0] + 1] = pV[1];

            case 1:
               bValues[pWArray[0]] = pV[0];

            }

            v = htonl(pe -> generationNumber);

            pV = reinterpret_cast<BYTE *>(&v) + (sizeof(long) - pWArray[2]);

            switch ( pWArray[2] ) {
            case 4:
               bValues[pWArray[0] + pWArray[1] + 3] = pV[3];

            case 3:
               bValues[pWArray[0] + pWArray[1] + 2] = pV[2];

            case 2:
               bValues[pWArray[0] + pWArray[1] + 1] = pV[1];

            case 1:
               bValues[pWArray[0] + pWArray[1]] = pV[0];

            }

            break;
            }

         case 1: {

            /*
            1     1     The type of this entry, which shall be 1. Type 1 entries define objects 
                        that are in use but are not compressed (corresponding to n entries in a 
                        cross-reference table).
                  2     The byte offset of the object, starting from the beginning of the file.
                  3     The generation number of the object. Default value: 0.
            */
//
// 8-20-2011: I encountered the following #if block. Prior to this date, it started with #if 0
// However, I'm getting an objectId of 0 now, which previously had been taken care of, but not now ??
// On this date, I switched it to "#if 1", and took out the else condition after the non-zero 
// test block.
//
// The real issue is why I'm getting the 0 objectId (was signing the IRS w9 form on a second
// signature area) ?
//
// Resolution
//
// 8-21-2011: It is because multiple objects with the same object ID were in the PDF file. I
// see no rule where this is actually illegal.
// Note that the objectMap currently (may) have a problem with this because when the same
// id is encountered - it will just replace the existing entry at that index, in contrast to the
// objectList - which will have the duplicate object placed at the end of the list.
//
// I set the value of fileOffset for each object at initial document object parsing time in order to
// find objects (by SearchByIdAndFileOffset) at XReference parsing time (Unwind) by both their ID and the initial
// location in the file. Thus, XReference (Unwind) parsing must always happen after initial document parsing,
// which was probably always the case.
//
#if 0
            PdfObject *po = NULL;

            if ( 0 != pe -> objectId ) {

               po = (*pObjectMap)[pe -> objectId];

               totalMotion += abs(pe -> fileOffset - po -> FileOffset());

               pe -> generationNumber = po -> Generation();
               pe -> fileOffset = po -> FileOffset();

            } //else

               //totalMotion += 1;

#else

//
//NTC: 11-11-2017: Somehow pe -> pObject is NULL when signing an IRS 1040.
// Today I put in this protection against that, and the resulting signed PDF document
// seems to be fine. 
// However, I'm not entirely convinced that the reason why pe -> pObject is NULL and that
// it may indicate a problem somewhere else.
//

if ( pe -> pObject ) {
            totalMotion += abs(pe -> fileOffset - pe -> pObject -> FileOffset());
            pe -> fileOffset = pe -> pObject -> FileOffset();
            pe -> generationNumber = pe -> pObject -> Generation();
}

#endif

            bValues[0] = 0x01;

            long v = htonl(pe -> fileOffset);

            BYTE *pV = reinterpret_cast<BYTE *>(&v) + (sizeof(long) - pWArray[1]);

            switch ( pWArray[1] ) {
            case 4:
               bValues[pWArray[0] + 3] = pV[3];

            case 3:
               bValues[pWArray[0] + 2] = pV[2];

            case 2:
               bValues[pWArray[0] + 1] = pV[1];

            case 1:
               bValues[pWArray[0]] = pV[0];

            }

            v = htonl(pe -> generationNumber);

            pV = reinterpret_cast<BYTE *>(&v) + (sizeof(long) - pWArray[2]);

            switch ( pWArray[2] ) {
            case 4:
               bValues[pWArray[0] + pWArray[1] + 3] = pV[3];

            case 3:
               bValues[pWArray[0] + pWArray[1] + 2] = pV[2];

            case 2:
               bValues[pWArray[0] + pWArray[1] + 1] = pV[1];

            case 1:
               bValues[pWArray[0] + pWArray[1]] = pV[0];

            }

            if ( pe -> objectId ) {
               if ( -1L == firstObjectId )
                  firstObjectId = pe -> objectId;
               maxId = max(maxId,pe -> objectId);
            }

            break;
            }

         case 2: {

            /*
            2     1     The type of this entry, which shall be 2. Type 2 entries define compressed objects.
                  2     The object number of the object stream in which this object is stored. 
                        (The generation number of the object stream shall be implicitly 0.)
                  3     The index of this object within the object stream.
            */

            bValues[0] = 0x02;

            long v = htonl(pe -> objectStreamId);

            BYTE *pV = reinterpret_cast<BYTE *>(&v) + (sizeof(long) - pWArray[1]);

            switch ( pWArray[1] ) {
            case 4:
               bValues[pWArray[0] + 3] = pV[3];

            case 3:
               bValues[pWArray[0] + 2] = pV[2];

            case 2:
               bValues[pWArray[0] + 1] = pV[1];

            case 1:
               bValues[pWArray[0]] = pV[0];

            }

            v = htonl(pe -> objectStreamIndex);

            pV = reinterpret_cast<BYTE *>(&v) + (sizeof(long) - pWArray[2]);

            switch ( pWArray[2] ) {
            case 4:
               bValues[pWArray[0] + pWArray[1] + 3] = pV[3];

            case 3:
               bValues[pWArray[0] + pWArray[1] + 2] = pV[2];

            case 2:
               bValues[pWArray[0] + pWArray[1] + 1] = pV[1];

            case 1:
               bValues[pWArray[0] + pWArray[1]] = pV[0];

            }

            if ( -1L == firstObjectId )
               firstObjectId = pe -> objectId;

            break;
            }

         }

         memcpy(pUncompressed + bytesCopied,bValues,wArrayByteLength);
   
         bytesCopied += wArrayByteLength;

         subSectionObjectCount++;
   
      }

      /*
         Size  integer  (Required) The number one greater than the highest object number used 
                        in this section or in any section for which this shall be an update. 
                        It shall be equivalent to the Size entry in a trailer dictionary.
      */

      sprintf(szTemp,"%ld",pXRefStreamObject -> initialSizeValue + (long)pXRefStreamObject -> entries.size() - pXRefStreamObject -> initialEntriesCount);

      pDictionary -> SetValue("Size",(BYTE *)szTemp);

      /*
         Index array    (Optional) An array containing a pair of integers for each subsection in 
                        this section. The first integer shall be the first object number in the 
                        subsection; the second integer shall be the number of entries in the subsection

                        The array shall be sorted in ascending order by object number. Subsections 
                        cannot overlap; an object number may have at most one entry in a section.
                        Default value: [0 Size].
      */

      szTemp[0] = '[';
      szTemp[1] = '\0';
      for ( std::list<xrefEntrySubSection *>::iterator it = pXRefStreamObject -> entrySubSections.begin(); it != pXRefStreamObject -> entrySubSections.end(); it++ )
         sprintf(szTemp + strlen(szTemp)," %ld %ld ",(*it) -> firstObjectId,(*it) -> objectCount);
      sprintf(szTemp + strlen(szTemp),"]");

      pDictionary -> SetValue("Index",(BYTE *)szTemp);

//
//NTC: 03-15-2012: See comment dated 03-14-2012 in repackObjectStreams in source Document.cpp
// Is this the place where the stream referenced there is getting compressed ? and a subsequent
// write of the document is encountering this stream as compressed when it was previously working
// 
      pXRefObject -> Stream() -> Compress(pUncompressed,bytesCopied);

      delete [] pUncompressed;

      delete [] pWArray;

   }

   return totalMotion;
   }