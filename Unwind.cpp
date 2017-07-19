
#include "Library.h"

#include "XReference.h"

   long PdfXReference::UnWind() {

   /*

      7.5.8 Cross-Reference Streams

      7.5.8.1  General

      Beginning with PDF 1.5, cross-reference information may be stored in a cross-reference
      stream instead of in a cross-reference table. Cross-reference streams provide 
      the following advantages:

         •  A more compact representation of cross-reference information

         •  The ability to access compressed objects that are stored in 
            object streams (see 7.5.7, "Object Streams") and to allow 
            new cross-reference entry types to be added in the future

      Cross-reference streams are stream objects (see 7.3.8, "Stream Objects"), and contain a 
      dictionary and a data stream. Each cross-reference stream contains the information 
      equivalent to the cross-reference table (see 7.5.4, "Cross-Reference Table") and 
      trailer (see 7.5.5, "File Trailer") for one cross-reference section.

   */

   for ( std::list<PdfCrossReferenceStream *>::iterator it = xRefObjects.begin(); it != xRefObjects.end(); it++ ) {

      PdfCrossReferenceStream *pXRefStreamObject = (*it);

      PdfObject *pXRefObject = pXRefStreamObject -> pObject;

      PdfStream *pStream = pXRefObject -> Stream();
   
      if ( ! pStream )
         continue;

      PdfDictionary *pDictionary = pXRefObject -> Dictionary();

      if ( ! pDictionary )
         continue;
   
      /*

         7.5.8.2  Cross-Reference Stream Dictionary

         Cross-reference streams may contain the entries shown in Table 17 in addition to 
         the entries common to all streams (Table 5) and trailer dictionaries (Table 15). 
         Since some of the information in the cross-reference stream is needed by the 
         conforming reader to construct the index that allows indirect references to be 
         resolved, the entries in cross-reference streams shall be subject to the 
         following restrictions:

            •  The values of all entries shown in Table 17 shall be direct objects; 
               indirect references shall not be permitted. For arrays (the Index and W entries), 
               all of their elements shall be direct objects as well. If the stream is encoded, 
               the Filter and DecodeParms entries in Table 5 shall also be direct objects.

            •  Other cross-reference stream entries not listed in Table 17 may be indirect; 
               in fact, some (such as Root in Table 15) shall be indirect.

            •  The cross-reference stream shall not be encrypted and strings appearing in the 
               cross-reference stream dictionary shall not be encrypted. It shall not have a 
               Filter entry that specifies a Crypt filter (see 7.4.10, "Crypt Filter").

         Table 17 – Additional entries specific to a cross-reference stream dictionary

         key      type     description

         Type     name     (Required) The type of PDF object that this dictionary describes; 
                           shall be XRef for a cross-reference stream.

      */

      /*
         Prev     integer  (Present only if the file has more than one cross-reference stream; 
                           not meaningful in hybrid-reference files; see 7.5.8.4, "Compatibility 
                           with Applications That Do Not Support Compressed Reference Streams") 
                           The byte offset in the decoded stream from the beginning of the file to 
                           the beginning of the previous cross-reference stream. This entry has the 
                           same function as the Prev entry in the trailer dictionary (Table 15).

      */

      BYTE *pValue = pDictionary -> Value("Prev");

      if ( pValue ) {
         char szTemp[32];
         sprintf(szTemp,"%010ld",atol((char *)pValue));
         pDictionary -> SetValue("Prev",(BYTE *)szTemp);
      }

      /*

         Size     integer  (Required) The number one greater than the highest object number used 
                           in this section or in any section for which this shall be an update. 
                           It shall be equivalent to the Size entry in a trailer dictionary.

      */

      pValue = pDictionary -> Value("Size");
   
      if ( ! pValue ) 
         continue;
   
      long maxObjectId = atol((char *)pValue) - 1;
   
      long firstObjectId = 0;

      long objectCount = maxObjectId + 1;
   
      /*
         Index    array    (Optional) An array containing a pair of integers for each subsection 
                           in this section. The first integer shall be the first object number 
                           in the subsection; the second integer shall be the number of entries 
                           in the subsection

                           The array shall be sorted in ascending order by object number. Subsections 
                           cannot overlap; an object number may have at most one entry in a section.

                           Default value: [0 Size].

      */

      pValue = pDictionary -> Value("Index");

      long subSectionCount = 1;

      if ( pValue ) {

         subSectionCount = pdfUtility.ArraySize((char *)pValue) / 2;

         for ( int k = 0; k < subSectionCount; k++ ) {
            firstObjectId = pdfUtility.IntegerValueFromArray((char *)pValue,2 * k + 1);
            objectCount = pdfUtility.IntegerValueFromArray((char *)pValue,2 * k + 2);
            xrefEntrySubSection *pSubSection = new xrefEntrySubSection(firstObjectId,objectCount);
            pXRefStreamObject -> entrySubSections.insert(pXRefStreamObject -> entrySubSections.end(),pSubSection);
         }

      }

      /*
         W        array    (Required) An array of integers representing the size of the fields in a 
                           single cross-reference entry. Table 18 describes the types of entries and 
                           their fields. For PDF 1.5, W always contains three integers; the value of 
                           each integer shall be the number of bytes (in the decoded stream) of the 
                           corresponding field.

                           EXAMPLE[1 2 1] means that the fields are one byte, two bytes, and one byte, 
                           respectively.  A value of zero for an element in the W array indicates that 
                           the corresponding field shall not be present in the stream, and the default 
                           value shall be used, if there is one. If the first element is zero, the type 
                           field shall not be present, and shall default to type 1. The sum of the items 
                           shall be the total length of each entry; it can be used with the Index array to 
                           determine the starting position of each subsection.

                           Different cross-reference streams in a PDF file may use different values for W.

      */

      pValue = pDictionary -> Value("W");
   
      strcpy(pXRefStreamObject -> szInitialWArray,(char *)pValue);

      long wArraySize = pdfUtility.ArraySize((char *)pValue);
   
      long *pWArray = new long[wArraySize];
   
      long entryByteSize = 0;

      for ( long k = 0; k < wArraySize; k++ ) {
         pWArray[k] = pdfUtility.IntegerValueFromArray((char *)pValue,k + 1);
         entryByteSize += pWArray[k];
      }
   
      unsigned long values[3];
   
      BYTE *pStreamData = pStream -> DecompressedStorage();

      if ( ! pStreamData )
         continue;

      BYTE *pStreamDataEnd = pStreamData + pStream -> BinaryDataSize();   

      while ( pStreamData < pStreamDataEnd ) {

         /*
            Table 18 – Entries in a cross-reference stream

            Type  Field Description

         */

         for ( long j = 0; j < wArraySize; j++ ) {
   
            values[j] = 0;
   
            for ( long n = entryByteSize - pWArray[j]; n < entryByteSize; n++ ) {
               values[j] = (values[j] << 8) + static_cast<unsigned char>(*pStreamData);
               pStreamData++;
            }
   
         }
   
         xrefEntry *pe = new xrefEntry(NULL);

         pXRefStreamObject -> entries.insert(pXRefStreamObject -> entries.end(),pe);

         if ( 0 == values[0] && 0 == pWArray[0] )
            values[0] = 1;

         /*
         0     1     The type of this entry, which shall be 0. Type 0 entries define the 
                     linked list of free objects (corresponding to f entries in a cross-reference table).
               2     The object number of the next free object.
               3     The generation number to use if this object number is used again.
         */

         if ( 0 == values[0] ) {
            pe -> objectId = values[1];
            pe -> generationNumber = values[2];
            pe -> status = 'f';
            continue;
         }

         /*
         1     1     The type of this entry, which shall be 1. Type 1 entries define objects 
                     that are in use but are not compressed (corresponding to n entries in a 
                     cross-reference table).
               2     The byte offset of the object, starting from the beginning of the file.
               3     The generation number of the object. Default value: 0.
         */

         if ( 1 == values[0] ) {
            pe -> type = 1;
            pe -> fileOffset = values[1];
            pe -> objectId = atol((char *)(pDocument -> Storage() + pe -> fileOffset));
            pe -> generationNumber = values[2];
            pe -> status = 'n';
            pe -> pObject = pDocument -> SearchByIdAndFileOffset(pe -> objectId,pe -> fileOffset);
            continue;
         }
   
         /*
         2     1     The type of this entry, which shall be 2. Type 2 entries define compressed objects.
               2     The object number of the object stream in which this object is stored. 
                     (The generation number of the object stream shall be implicitly 0.)
               3     The index of this object within the object stream.
         */

         if ( 2 == values[0] ) {

            pe -> type = 2;
            pe -> objectId = -1L;
            pe -> generationNumber = -1L;
            pe -> objectStreamId = values[1];
            pe -> objectStreamIndex = values[2];
            pe -> fileOffset = -1L;
            pe -> status = 'n';

            PdfObject *pObjectStream = pDocument -> FindObjectById(pe -> objectStreamId);
   
            PdfDictionary *pObjectStreamDictionary = pObjectStream -> Dictionary();
   
            pValue = pObjectStreamDictionary -> Value("First");
   
            long streamObjectOffsetFirst = pdfUtility.IntegerValue((char *)pValue);
   
            pValue = pObjectStreamDictionary -> Value("N");
   
            long streamObjectCount = pdfUtility.IntegerValue((char *)pValue);
   
            pValue = pObjectStreamDictionary -> Value("Length");
   
            long streamLength = pdfUtility.IntegerValue((char *)pValue);
   
            long *pObjectPairs = new long[2 * (streamObjectCount + 1)];
   
            memset(pObjectPairs,0,2 * (streamObjectCount + 1) * sizeof(long));
   
            BYTE *pStreamUncompressedData = pObjectStream -> Stream() -> DecompressedStorage();

            if ( ! pStreamUncompressedData )
               continue;
   
            streamLength = pObjectStream -> Stream() -> BinaryDataSize();
   
            long n = strlen((char *)pStreamUncompressedData);
            char *pArray = new char[n + 1];
            strcpy(pArray,(char *)pStreamUncompressedData);

            long id = atol(strtok(pArray," ]"));

            for ( long j = 0; j < 2 * streamObjectCount; j += 2 ) {
               pObjectPairs[j] = id;
               pObjectPairs[j + 1] = atol(strtok(NULL," ]")) + streamObjectOffsetFirst;
               char *px = strtok(NULL," ]");
               if ( ! px )
                  break;
               id = atol(px);
            }
   
            delete [] pArray;

            pe -> objectId = pObjectPairs[2 * pe -> objectStreamIndex];
   
            delete [] pObjectPairs;

         } 

      } 

#ifdef EXPAND_W_ARRAY
      char szTemp[64];
      long oldValue = pWArray[1];
      pWArray[0] = max(pWArray[0],1);
      pWArray[1] = max(pWArray[1],3);
      pWArray[2] = max(pWArray[2],1);
      pXRefObject -> SetReservedPrintSize( pXRefObject -> GetReservedPrintSize() + (pWArray[1] - oldValue) * pStream -> BinaryDataSize());
      sprintf(szTemp,"[%ld %ld %ld]",pWArray[0],pWArray[1],pWArray[2]);
      pDictionary -> SetValue("W",(BYTE *)szTemp);
#endif
   
      pXRefStreamObject -> initialSizeValue = atol((char *)pXRefStreamObject -> pObject -> Value("Size"));

      pXRefStreamObject -> initialEntriesCount = pXRefStreamObject -> entries.size();
   
      delete [] pWArray;

   }

   return 1;
   }