
#include "Library.h"

#include "XReference.h"


   PdfXReference::PdfXReference(PdfDocument *pp,BYTE *pDocumentStart,BYTE *pDocumentEnd) :

      PdfEntity(edXReference,esdScanUp,pDocumentEnd,pDocumentStart),

      pDocument(pp),

      pTrailerForNewObjects(NULL),
      pXRefStreamForNewObjects(NULL)

   {

   BYTE *p = Storage();

   while ( *p != 0x0A && *p != 0x0D )
      p++;
   while ( *p == 0x0A || *p == 0x0D )
      p++;
   
   p = pDocumentStart + atol((char *)p);

   while ( *p == 0x0A || *p == 0x0D )
      p++;

   BYTE *pHybridXRefObjectSpace = NULL;

   if ( 0 == strncmp((char *)p,"xref",4) ) {

      BYTE *pTrailerBytes = p;

      PdfTrailer *pAssignPrevious = NULL;

      do {

         while ( pDocumentStart < (BYTE *)pTrailerBytes && strncmp("xref",(char *)pTrailerBytes,4) )
            pTrailerBytes--;

         if ( (BYTE *)pTrailerBytes == pDocumentStart )
            break;

         ADVANCE_THRU_EOL(pTrailerBytes)
   
         PdfTrailer *pTrailer = new PdfTrailer(pDocument,pTrailerBytes);

         if ( pAssignPrevious ) {
            pAssignPrevious -> pPreviousTrailer = pTrailer;
            pTrailer -> pNextTrailer = pAssignPrevious;
            pAssignPrevious = NULL;
         }
   
         pTrailerBytes = parseEntries(pTrailer,pTrailerBytes,pDocumentEnd);

         trailerObjects.insert(trailerObjects.end(),pTrailer);

         pTrailerBytes = pTrailer -> Dictionary() -> Value("XRefStm");

         if ( pTrailerBytes )
            pTrailer -> crossReferenceStreamObjectId = atol((char *)(pDocumentStart + atol((char *)pTrailerBytes)));

         pTrailerBytes = pTrailer -> Dictionary() -> Value("Prev");
   
         if ( pTrailerBytes ) {
            pAssignPrevious = pTrailer;
            pTrailerBytes = pDocumentStart + atol((char *)pTrailerBytes);
         }

      } while ( pTrailerBytes ) ;

      PdfObject *pX = pDocument -> FindObjectByType("/XRef");

      if ( ! pX )
         return;

      PdfCrossReferenceStream *pXRefObject = new PdfCrossReferenceStream(pX);

      pHybridXRefObjectSpace = new BYTE[64 + pXRefObject -> pObject -> Dictionary() -> BinaryDataSize() + pXRefObject -> pObject -> BinaryDataSize()];

      memset(pHybridXRefObjectSpace,0,64 + pXRefObject -> pObject -> Dictionary() -> BinaryDataSize() + pXRefObject -> pObject -> BinaryDataSize());

      long bytesWritten = sprintf((char *)pHybridXRefObjectSpace,"%ld %ld obj%c",pXRefObject -> pObject -> Id(),pXRefObject -> pObject -> Generation(),eol);

      memcpy(pHybridXRefObjectSpace + bytesWritten,pXRefObject -> pObject -> Dictionary() -> Storage(),pXRefObject -> pObject -> Dictionary() -> BinaryDataSize());

      bytesWritten += pXRefObject -> pObject -> Dictionary() -> BinaryDataSize();

      memcpy(pHybridXRefObjectSpace + bytesWritten,pXRefObject -> pObject -> Storage(),pXRefObject -> pObject -> BinaryDataSize());

      bytesWritten += pXRefObject -> pObject -> BinaryDataSize();

      p = pHybridXRefObjectSpace;

   }

   BYTE *pValue = p;

   PdfCrossReferenceStream *pAssignXRefPrevious = NULL;

   do { 

      PdfObject *pX = pDocument -> FindObjectById(atol((char *)pValue));

      if ( ! pX ) {
         if ( pHybridXRefObjectSpace )
            delete [] pHybridXRefObjectSpace;
         return;
      }

      PdfCrossReferenceStream *pXRefStream = new PdfCrossReferenceStream(pX);

      if ( pAssignXRefPrevious ) {
         pAssignXRefPrevious -> pPreviousXRefStream = pXRefStream;
         pXRefStream -> pNextXRefStream = pAssignXRefPrevious;
         pAssignXRefPrevious = NULL;
      }

      char szPostAmble[256];

      long postAmbleSize;

      postAmbleSize = sprintf(szPostAmble,"startxref%c\n%010ld%c\n%%%%EOF%c\n",eol,0,eol,eol);

#ifdef REMOVE_COMPRESSION
      pXRefStream -> pObject -> SetReservedPrintSize( 3.0 * (pXRefStream -> pObject -> BinaryDataSize() + pXRefStream -> pObject -> Dictionary() -> BinaryDataSize()));
#else
      pXRefStream -> pObject -> SetReservedPrintSize( (long)(1.25 * (double)(postAmbleSize + pXRefStream -> pObject -> BinaryDataSize() + pXRefStream -> pObject -> Dictionary() -> BinaryDataSize())));
#endif   

      pXRefStream -> pObject -> SetPostAmble((BYTE *)szPostAmble,postAmbleSize);

      xRefObjects.insert(xRefObjects.end(),pXRefStream);

      pValue = pXRefStream -> pObject -> Dictionary() -> Value("Prev");

      if ( pValue ) {
         pAssignXRefPrevious = pXRefStream;
         pValue = pDocumentStart + atol((char *)pValue);
      }

      if ( pHybridXRefObjectSpace )
         delete [] pHybridXRefObjectSpace;

      pHybridXRefObjectSpace = NULL;

   } while ( pValue ) ;

   return;
   }


   PdfXReference::~PdfXReference() {
   for ( std::list<PdfCrossReferenceStream *>::iterator itXRefStreams = xRefObjects.begin(); itXRefStreams != xRefObjects.end(); itXRefStreams++ ) 
      delete (*itXRefStreams);
   xRefObjects.clear();
   for ( std::list<PdfTrailer *>::iterator itTrailers = trailerObjects.begin(); itTrailers != trailerObjects.end(); itTrailers++ ) 
      delete (*itTrailers);
   trailerObjects.clear();
   return;
   }


   long PdfXReference::MaxObjectId() {

   long maxId = -32768;

#if 0

   for ( std::list<PdfCrossReferenceStream *>::iterator itXRefStreams = xRefObjects.begin(); itXRefStreams != xRefObjects.end(); itXRefStreams++ ) {
      for ( std::list<xrefEntry *>::iterator itEntries = (*itXRefStreams) -> entries.begin(); itEntries != (*itXRefStreams) -> entries.end(); itEntries++ )
         if ( 0 != (*itEntries) -> type && 'f' != (*itEntries) -> status )
            maxId = max(maxId,(*itEntries) -> objectId);
   }

   for ( std::list<PdfTrailer *>::iterator itTrailers = trailerObjects.begin(); itTrailers != trailerObjects.end(); itTrailers++ ) {
      for ( std::list<xrefEntrySection *>::iterator itSections = (*itTrailers) -> entrySections.begin(); itSections != (*itTrailers) -> entrySections.end(); itSections++ ) {
         for ( std::list<xrefEntry *>::iterator itEntries = (*itSections) -> entries.begin(); itEntries != (*itSections) -> entries.end(); itEntries++ ) {
            if ( 'f' != (*itEntries) -> status )
               maxId = max(maxId,(*itEntries) -> objectId);
         }
      }
   }

#endif

/*
   NTC: 2-22-2010: It is not entirely clear whether it is necessary to do the above to search lists
   if the system will look at the document map as well.
   It may be that the document map does not always (?) contain objects that are embedded in XRef Streams (?)

   3-4-2010: The above is indeed the case. The IRS 1040 form contains an object 1782 that is itself an XRef
   but which is not in any of the standard cross reference sections or subsections (Though it is not clear
   whether that object IS in an XRef stream).

   Therefore, I have disabled the above two loops for now.

*/

   std::list<PdfObject *> * parentMap = pDocument -> ObjectList();

   for ( std::list<PdfObject *>::iterator itObjects = parentMap -> begin(); itObjects != parentMap -> end(); itObjects++ ) {
      if ( 'f' != (*itObjects) -> Status() )
         maxId = max(maxId,(*itObjects) -> Id());
   }

   return maxId;
   }


   long PdfXReference::FindEncryptionObject() {
   for ( std::list<PdfCrossReferenceStream *>::iterator itXrefObjects = xRefObjects.begin(); itXrefObjects != xRefObjects.end(); itXrefObjects++ ) {
      BYTE *pValue = (*itXrefObjects) -> pObject -> Dictionary() -> Value("Encrypt");
      if ( pValue ) 
         return atol((char *)pValue);
   }
   for ( std::list<PdfTrailer *>::iterator it = trailerObjects.begin(); it != trailerObjects.end(); it ++ ) {
      PdfTrailer *pTrailer = (*it);
      BYTE *pValue = pTrailer -> Dictionary() -> Value("Encrypt");
      if ( pValue ) 
         return atol((char *)pValue);
   }
   return 0;
   }


#if defined REMOVE_ENCRYPTION || defined REMOVE_COMPRESSION

   void PdfXReference::RemoveEncryption() {

   long objectId = FindEncryptionObject();
   if ( objectId ) {
      PdfObject *po = pDocument->FindObjectById(objectId);
      if ( po )
         po -> RemoveDictionary();
   }

   for ( std::list<PdfCrossReferenceStream *>::iterator itXrefObjects = xRefObjects.begin(); itXrefObjects != xRefObjects.end(); itXrefObjects++ ) {
      (*itXrefObjects) -> pObject -> Dictionary() -> RemoveValue("Encrypt");
   }

   for ( std::list<PdfTrailer *>::iterator it = trailerObjects.begin(); it != trailerObjects.end(); it ++ ) {
      PdfTrailer *pTrailer = (*it);
      pTrailer -> Dictionary() -> RemoveValue("Encrypt");
   }

   return;
   }

#endif


   BYTE * PdfXReference::Value(char *pszName) {

   static BYTE *pValue;

   for ( std::list<PdfCrossReferenceStream *>::iterator it = xRefObjects.begin(); it != xRefObjects.end(); it++ ) {
   
      PdfObject *pObject = (*it) -> pObject;

      PdfDictionary *pDictionary = pObject -> Dictionary();
   
      if ( ! pDictionary )
         continue;

      pValue = pDictionary -> Value(pszName);

      if ( pValue )
         return pValue;

   }

   for ( std::list<PdfTrailer *>::iterator itTrailer = trailerObjects.begin(); itTrailer != trailerObjects.end(); itTrailer++ ) {

      pValue = (*itTrailer) -> Value(pszName);

      if ( pValue )
         return pValue;

   }

   return NULL;

   }


   PdfObject *PdfXReference::RootXRefObject() {
   if ( 0 == xRefObjects.size() )
      return NULL;
   return xRefObjects.front() -> pObject;
   }


   BYTE * PdfXReference::parseEntries(PdfTrailer *pTrailer,BYTE *pStart,BYTE *pEnd) {

   long objectCount = 0L;

   while ( pStart < pEnd && strncmp((char *)pStart,"trailer",7) ) {

      xrefEntrySection *pSection = new xrefEntrySection();

      pTrailer -> entrySections.insert(pTrailer -> entrySections.end(),pSection);

      {
      char szTemp[128];
      memcpy(szTemp,pStart,min(pEnd - pStart,128));
      pSection -> firstObjectId = atol(strtok(szTemp," \n"));
      objectCount = atol(strtok(NULL," \n"));
      }

      ADVANCE_THRU_EOL(pStart)
      
      for ( long k = 0; k < objectCount; k++ ) {

         xrefEntry *pe = new xrefEntry(NULL);

         {
         char szTemp[128];
         memcpy(szTemp,pStart,min(pEnd - pStart,128));
         pe -> fileOffset = atol(strtok(szTemp," \n"));
         char *p = strtok(NULL," \n");
         pe -> generationNumber = atol(p);
         p = p + strlen(p) + 1;
         pe -> status = *p;
         }

         if ( 'n' == pe -> status ) {
            pe -> objectId = pSection -> firstObjectId + k;
         } else {
            pe -> objectId = pe -> fileOffset;
            pe -> fileOffset = -1L;
         }

         pSection -> entries.insert(pSection -> entries.end(),pe);

         ADVANCE_THRU_EOL(pStart)

      }

   }

   return pStart;
   }


   void PdfXReference::AddObject(PdfObject *pNewObject) {

   xrefEntry *pe = new xrefEntry(pNewObject);

   pe -> type = 1;
   pe -> objectId = pNewObject -> Id();
   pe -> generationNumber = pNewObject -> Generation();
   pe -> status = 'n';

   if ( ! pTrailerForNewObjects && ! pXRefStreamForNewObjects ) {

      if ( trailerObjects.size() ) {

         PdfTrailer *pRootTrailer = trailerObjects.front();

         pTrailerForNewObjects = new PdfTrailer(*pRootTrailer);

         pTrailerForNewObjects -> pNextTrailer = pRootTrailer -> pNextTrailer;

         pTrailerForNewObjects -> pPreviousTrailer = pRootTrailer;

         pRootTrailer -> pNextTrailer = pTrailerForNewObjects;

         pTrailerForNewObjects -> Dictionary() -> RemoveValue("XRefStm");

         pTrailerForNewObjects -> Dictionary() -> SetValue("Prev",(BYTE *)"0000000000",true);

         xrefEntrySection *pTrailer_EntrySection = new xrefEntrySection(pe -> objectId);

         pTrailerForNewObjects -> entrySections.insert(pTrailerForNewObjects -> entrySections.end(),pTrailer_EntrySection);

         trailerObjects.insert(trailerObjects.end(),pTrailerForNewObjects);

      } else {

         if ( ! xRefObjects.size() ) {
            delete pe;
            return;
         }

         pXRefStreamForNewObjects = NULL;

         long maxObjectId = -32768;

         for ( std::list<PdfCrossReferenceStream *>::iterator it = xRefObjects.begin(); it != xRefObjects.end(); it++ ) {
            long moid = (*it) -> maxObjectId();
            if ( moid > maxObjectId ) {
               maxObjectId = moid;
               pXRefStreamForNewObjects = (*it);
            }
         }

         if ( ! pXRefStreamForNewObjects ) {
            delete pe;
            return;
         }

      }

   }

   if ( pTrailerForNewObjects ) {
      xrefEntrySection *pTrailer_EntrySection = pTrailerForNewObjects -> entrySections.front();
      pTrailer_EntrySection -> entries.insert(pTrailer_EntrySection -> entries.end(),pe);
      return;
   }

   if ( pXRefStreamForNewObjects )
      pXRefStreamForNewObjects -> addEntry(pe);

   return;
   }


   long PdfXReference::WriteObjectTable(FILE *fOutput,bool writeUncompressed) {

   if ( 0 == trailerObjects.size() )
      return 0;

   /*
      7.5.4 Cross-Reference Table

      The cross-reference table contains information that permits random access to indirect
      objects within the file so that the entire file need not be read to locate any 
      particular object. The table shall contain a one-line entry for each indirect 
      object, specifying the byte offset of that object within the body of the file. 
      (Beginning with PDF 1.5, some or all of the cross-reference information may 
      alternatively be contained in cross-reference streams; see 7.5.8, "Cross-Reference Streams.")

      NOTE 1   The cross-reference table is the only part of a PDF file with a fixed format, 
               which permits entries in the table to be accessed randomly.

      The table comprises one or more cross-reference sections. Initially, the entire 
      table consists of a single section (or two sections if the file is linearized; 
      see Annex F). One additional section shall be added each time the file is 
      incrementally updated (see 7.5.6, "Incremental Updates").

      Each cross-reference section shall begin with a line containing the keyword xref. 
      Following this line shall be one or more cross-reference subsections, which 
      may appear in any order. For a file that has never been incrementally updated, 
      the cross-reference section shall contain only one subsection, whose object 
      numbering begins at 0.

      NOTE 2   The subsection structure is useful for incremental updates, since 
               it allows a new cross-reference section to be added to the PDF file, 
               containing entries only for objects that have been added or deleted.

      Each cross-reference subsection shall contain entries for a contiguous range 
      of object numbers. The subsection shall begin with a line containing two numbers 
      separated by a SPACE (20h), denoting the object number of the first object in 
      this subsection and the number of entries in the subsection.

   */

   std::map<long,PdfObject *> *objects = pDocument -> ObjectMap();

   char szTemp[MAX_PATH];

   long initialFileOffset = ftell(fOutput);

// Note: 1-25-2010:
// A problem document was misaligned by 1-byte in the cross reference tables.
// Changing the iteration counter to 3 in the below statement fixed this problem.
// It is probably best to see if there is a detectable closure criteria on these iterations
// or, whether it is possible to do this without iterating.

   long totalObjectCount = 0L;

   for ( long iteration = 0; iteration < 3; iteration++ ) {

      fseek(fOutput,initialFileOffset,SEEK_SET);

      for ( std::list<PdfTrailer *>::iterator itTrailer = trailerObjects.begin(); itTrailer != trailerObjects.end(); itTrailer++ ) {

         PdfTrailer *pTrailer = (*itTrailer);

         pTrailer -> fileOffset = ftell(fOutput);

         if ( pTrailer -> pNextTrailer ) {
            sprintf(szTemp,"%010ld",pTrailer -> fileOffset);
            pTrailer -> pNextTrailer -> Dictionary() -> SetValue("Prev",(BYTE *)szTemp);         
         }

         fprintf(fOutput,"xref%c\n",eol);
      
         long totalSize = pTrailer -> entrySections.front() -> firstObjectId;

         for ( std::list<xrefEntrySection *>::iterator itSection = pTrailer -> entrySections.begin(); itSection != pTrailer -> entrySections.end(); itSection++ ) {

            xrefEntrySection *pSection = (*itSection);

            fprintf(fOutput,"%ld %zd%c\n",pSection -> firstObjectId,pSection -> entries.size(),eol);

         /*
            A given object number shall not have an entry in more than one subsection within a single section.

            Following this line are the cross-reference entries themselves, one per line. Each entry shall 
            be exactly 20 bytes long, including the end-of-line marker. There are two kinds of 
            cross-reference entries: one for objects that are in use and another for objects 
            that have been deleted and therefore are free. Both types of entries have
            similar basic formats, distinguished by the keyword n (for an in-use entry) or 
            f (for a free entry). The format of an in-use entry shall be:

               nnnnnnnnnn ggggg n eol

            where:

               nnnnnnnnnn shall be a 10-digit byte offset in the decoded stream

               ggggg shall be a 5-digit generation number

               n shall be a keyword identifying this as an in-use entry

               eol shall be a 2-character end-of-line sequence

            The byte offset in the decoded stream shall be a 10-digit number, padded 
            with leading zeros if necessary, giving the number of bytes from the 
            beginning of the file to the beginning of the object. It shall be separated 
            from the generation number by a single SPACE. The generation number shall 
            be a 5-digit number, also padded with leading zeros if necessary. 
            Following the generation number shall be a single SPACE, the keyword n, 
            and a 2-character end-of-line sequence consisting of one of the following: 
            SP CR, SP LF, or CR LF. Thus, the overall length of the entry shall always be 
            exactly 20 bytes.

            The cross-reference entry for a free object has essentially the same format, 
            except that the keyword shall be finstead of n and the interpretation of the 
            first item is different:

               nnnnnnnnnn ggggg f eol

            where:

               nnnnnnnnnn shall be the 10-digit object number of the next free object

               ggggg shall be a 5-digit generation number

               f shall be a keyword identifying this as a free entry

               eol shall be a 2-character end-of-line sequence

            There are two ways an entry may be a member of the free entries list. 
            Using the basic mechanism the free entries in the cross-reference table 
            may form a linked list, with each free entry containing the object number 
            of the next. The first entry in the table (object number 0) shall always be 
            free and shall have a generation number of 65,535; it is shall be the head 
            of the linked list of free objects. The last free entry (the tail of the 
            linked list) links back to object number 0. Using the second mechanism, 
            the table may contain other free entries that link back to object number 
            0 and have a generation number of 65,535, even though these entries are not 
            in the linked list itself.

            Except for object number 0, all objects in the cross-reference table shall 
            initially have generation numbers of 0. When an indirect object is deleted, 
            its cross-reference entry shall be marked free and it shall be added to the 
            linked list of free entries. The entry’s generation number shall be incremented 
            by 1 to indicate the generation number to be used the next time an object with 
            that object number is created. Thus, each time the entry is reused, it is given 
            a new generation number. The maximum generation number is 65,535; when a 
            cross-reference entry reaches this value, it shall never be reused.
   
            The cross-reference table (comprising the original cross-reference section and all 
            update sections) shall contain one entry for each object number from 0 to the 
            maximum object number defined in the file, even if one or more of the object 
            numbers in this range do not actually occur in the file.

         */      

            for ( std::list<xrefEntry *>::iterator itEntries = pSection -> entries.begin(); itEntries != pSection -> entries.end(); itEntries++ ) {

               xrefEntry *pe = (*itEntries);

               if ( pe -> objectId ) {

                  if ( 0 == iteration )
                     totalObjectCount = max(totalObjectCount,pe -> objectId);

                  if ( 'n' == pe -> status ) {

                     PdfObject *pObject = (*objects)[pe -> objectId];

                     if ( pObject )
                        fprintf(fOutput,"%010ld %05ld %c%c\n",pObject -> FileOffset(),pObject -> Generation(),pObject -> Status(),eol);
                     else
                        fprintf(fOutput,"%010ld %05ld %c%c\n",0,0,'n',eol);

                  } else {

                     fprintf(fOutput,"%010ld %05ld %c%c\n",pe -> objectId,pe -> generationNumber,'f',eol);

                  }

               } else {
// Change made on 8/17/09: A .pdf from Adobe Illustrator has 1 as the generation number here
#if 0
                  fprintf(fOutput,"%010ld 65535 f%c\n",0,eol);
#else
                  fprintf(fOutput,"%010ld 00001 f%c\n",0,eol);
#endif
               }

               totalSize++;

            }

         }

         fprintf(fOutput,"trailer\n");

         sprintf(szTemp,"%05ld",totalObjectCount + 1);
   
         pTrailer -> Dictionary() -> SetValue("Size",(BYTE *)szTemp);

         if ( pTrailer -> crossReferenceStreamObjectId ) {
            PdfObject *po = pDocument -> FindObjectById(pTrailer -> crossReferenceStreamObjectId);
            sprintf(szTemp,"%010ld",po -> FileOffset());
            pTrailer -> Dictionary() -> SetValue("XRefStm",(BYTE *)szTemp);
         }

         pTrailer -> Dictionary() -> Write(fOutput,writeUncompressed);

         fprintf(fOutput,"\nstartxref\n%ld\n%%%%EOF\n\n",0);

      }

   }

   if ( pTrailerForNewObjects )
      fprintf(fOutput,"\nstartxref\n%ld\n%%%%EOF",pTrailerForNewObjects -> fileOffset);
   else
      fprintf(fOutput,"\nstartxref\n%ld\n%%%%EOF",initialFileOffset);

   return 1;
   }



   void PdfXReference::SetPostAmble(FILE *fOutput) {

   if ( 1 == xRefObjects.size() ) {

      std::list<PdfCrossReferenceStream *>::iterator it = xRefObjects.begin();
   
      PdfObject *pXRefObject = (*it) -> pObject;

      long fileOffset = ftell(fOutput);

      char szPostAmble[256];
   
      long postAmbleSize = sprintf(szPostAmble,"startxref%c\n%010ld%c\n%%%%EOF%c\n",eol,fileOffset,eol,eol);

      pXRefObject -> SetPostAmble((BYTE *)szPostAmble,postAmbleSize);

   }

   return;
   }


   long PdfXReference::Write(FILE *fOutput,bool writeUncompressed) {

   if ( 0 != xRefObjects.size() ) {
      fprintf(fOutput,"\nstartxref\n%ld\n%%%%EOF",xRefObjects.front() -> pObject -> FileOffset());
      fprintf(fOutput,"\n");
   }

   WriteObjectTable(fOutput,writeUncompressed);

   return 1;
   }


#ifdef DUMP_TRAILERS

   void PdfXReference::dumpTrailers(char *pszSourceFileName,char *pszProducedFileName,FILE *fOutput) {

   WriteObjectTable(fOutput,false);
#if 0
   fprintf(fOutput,"\n");

   for ( std::list<PdfTrailer *>::iterator itTrailer = trailerObjects.begin(); itTrailer != trailerObjects.end(); itTrailer++ ) {

      PdfTrailer *pTrailer = (*itTrailer);

      if ( pTrailer -> crossReferenceStreamObjectId ) {
         PdfObject *po = pDocument -> FindObjectById(pTrailer -> crossReferenceStreamObjectId);
po->Stream()->Compress();
         po -> Write(fOutput,false);
      }

   }
#endif

   return;
   }

#endif

#ifdef DUMP_XREF

   void PdfXReference::dumpInitialXRef(BYTE *bDocument,char *szDocumentName,char *pszFilePostAmble) {

   char szTemp[MAX_PATH];

   strcpy(szTemp,szDocumentName);
   char *p = strrchr(szTemp,'.');
   if ( p )
      *p = '\0';

   char szDump[MAX_PATH];
   BYTE bReference[33];

   sprintf(szDump,"%s%s%s",szTemp,"-xref-",pszFilePostAmble);

   FILE *fOutput = fopen(szDump,"wt");

   for ( std::list<PdfCrossReferenceStream *>::iterator it = xRefObjects.begin(); it != xRefObjects.end(); it++ ) {

      PdfCrossReferenceStream *pXRefStreamObject = (*it);

      PdfObject *pXRefObject = pXRefStreamObject -> pObject;

      PdfStream *pStream = pXRefObject -> Stream();
   
      if ( ! pStream )
         continue;

      PdfDictionary *pDictionary = pXRefObject -> Dictionary();

      if ( ! pDictionary )
         continue;
   
      BYTE *pValue = pDictionary -> Value("Size");
   
      if ( ! pValue ) 
         continue;
   
      long maxObjectId = atol((char *)pValue) - 1;
   
      long firstObjectId = 0;

      long objectCount = maxObjectId + 1;
   
      pValue = pDictionary -> Value("Index");

      long subSectionCount = 1;

      if ( pValue ) {
         firstObjectId = pdfUtility.IntegerValueFromArray((char *)pValue,1);
         objectCount = pdfUtility.IntegerValueFromArray((char *)pValue,2);
         subSectionCount = pdfUtility.ArraySize((char *)pValue) / 2;
      }

      pValue = (BYTE *)pXRefStreamObject -> szInitialWArray;

      long wArraySize = pdfUtility.ArraySize((char *)pValue);
   
      long *pWArray = new long[wArraySize];
   
      long entryByteSize = 0;

      for ( long k = 0; k < wArraySize; k++ ) {
         pWArray[k] = pdfUtility.IntegerValueFromArray((char *)pValue,k + 1);
         entryByteSize += pWArray[k];
      }
   
      unsigned long values[3];
   
      BYTE *pStreamData = pStream -> Storage();

      if ( ! pStreamData )
         continue;
   
      BYTE *pStreamDataEnd = pStreamData + pStream -> BinaryDataSize();   

      fprintf(fOutput,"\nW: [%ld %ld %ld]\n",pWArray[0],pWArray[1],pWArray[2]);
      fprintf(fOutput,"      type,   object id,   generation, objectstreamid,   objectstreamindex, status, fileOffset\n");

      while ( pStreamData < pStreamDataEnd ) {
   
         for ( long j = 0; j < wArraySize; j++ ) {
   
            values[j] = 0;
   
            for ( long n = entryByteSize - pWArray[j]; n < entryByteSize; n++ ) {
               values[j] = (values[j] << 8) + static_cast<unsigned char>(*pStreamData);
               pStreamData++;
            }
   
         }
   
         xrefEntry *pe = new xrefEntry();

         if ( 0 == values[0] && 0 == pWArray[0] )
            values[0] = 1;

         if ( 0 == values[0] ) {
            pe -> objectId = values[1];
            pe -> generationNumber = values[2];
            pe -> status = 'f';
            fprintf(fOutput,"entry: %05ld  %05ld  %05ld  %05ld  %05ld %c %l08d\n",pe -> type,pe -> objectId,pe -> generationNumber,pe -> objectStreamId,pe -> objectStreamIndex,pe -> status,pe -> fileOffset);
            continue;
         }

         if ( 1 == values[0] ) {

            pe -> type = 1;
            pe -> fileOffset = values[1];
            pe -> objectId = atol((char *)(pDocument -> Storage() + pe -> fileOffset));
            pe -> generationNumber = values[2];
            pe -> status = 'n';

            fprintf(fOutput,"entry: %05ld  %05ld  %05ld  %05ld  %05ld %c %08ld  ",pe -> type,pe -> objectId,pe -> generationNumber,pe -> objectStreamId,pe -> objectStreamIndex,pe -> status,pe -> fileOffset);

            memcpy((void *)bReference,(bDocument + pe -> fileOffset),32);

            for ( long j = 0; j < 32; j++ ) {
               if ( bReference[j] == 0x0D )
                  bReference[j] = '_';
               if ( bReference[j] == 0x0A )
                  bReference[j] = '_';
            }

            for ( long j = 0; j < 32; j++ )
               fprintf(fOutput,"%c",(char *)bReference[j]);

            fprintf(fOutput,"\n");

            continue;

         }
   
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
   
            BYTE *pStreamUncompressedData = pObjectStream -> Stream() -> Storage();

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

            fprintf(fOutput,"entry: %05ld  %05ld  %05ld  %05ld  %05ld %c %08ld  \n",pe->type,pe->objectId,pe->generationNumber,pe->objectStreamId,pe->objectStreamIndex,pe->status,pe->fileOffset);


         } 

      } 

      delete [] pWArray;

   }

   fclose(fOutput);

   return;
   }


   void PdfXReference::dumpXRef(BYTE *bDocument,char *szDocumentName,char *pszFilePostAmble) {

   char szTemp[MAX_PATH];

   strcpy(szTemp,szDocumentName);
   char *p = strrchr(szTemp,'.');
   if ( p )
      *p = '\0';

   char szDump[MAX_PATH];
   
   sprintf(szDump,"%s%s%s",szTemp,"-xref-",pszFilePostAmble);

   FILE *fOutput = fopen(szDump,"wt");

   BYTE bReference[33];

   std::map<long,PdfObject *> *pObjectMap = pDocument -> ObjectMap();

   for ( std::list<PdfCrossReferenceStream *>::iterator itXRefObjects = xRefObjects.begin(); itXRefObjects != xRefObjects.end(); itXRefObjects++ ) {

      PdfCrossReferenceStream *pXRefStreamObject = (*itXRefObjects);

      PdfObject *pXRefObject = pXRefStreamObject -> pObject;

      PdfDictionary *pDictionary = pXRefObject -> Dictionary();

      if ( ! pDictionary )
         continue;

      if ( 0 == pXRefStreamObject -> entries.size() )
         continue;

      BYTE *pValue = (BYTE *)pXRefStreamObject -> szInitialWArray;

      long wArraySize = pdfUtility.ArraySize((char *)pValue);
   
      long *pWArray = new long[wArraySize];
   
      for ( long k = 0; k < wArraySize; k++ )
         pWArray[k] = pdfUtility.IntegerValueFromArray((char *)pValue,k + 1);

      long wArrayByteLength = pWArray[0] + pWArray[1] + pWArray[2];

      fprintf(fOutput,"\nW: [%ld %ld %ld]\n",pWArray[0],pWArray[1],pWArray[2]);
      fprintf(fOutput,"      type,   object id,   generation, objectstreamid,   objectstreamindex, status, fileOffset\n");

      BYTE bValues[16];

      for ( std::list<xrefEntry *>::iterator itEntry = pXRefStreamObject -> entries.begin(); itEntry != pXRefStreamObject -> entries.end(); itEntry++ ) {

         xrefEntry *pe = (*itEntry);

         switch ( pe -> type ) {

         case 0: {

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

            fprintf(fOutput,"entry: %05ld  %05ld  %05ld  %05ld  %05ld %c %08ld\n",pe -> type,pe -> objectId,pe -> generationNumber,pe -> objectStreamId,pe -> objectStreamIndex,pe -> status,pe -> fileOffset);

            break;
            }

         case 1: {

            PdfObject *po = (*pObjectMap)[pe -> objectId];

            bValues[0] = 0x01;

            long v = htonl(po -> FileOffset());

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

            v = htonl(po -> Generation());

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

            fprintf(fOutput,"entry: %05ld  %05ld  %05ld  %05ld  %05ld %c %08ld  ",pe -> type,pe -> objectId,pe -> generationNumber,pe -> objectStreamId,pe -> objectStreamIndex,pe -> status,pe -> fileOffset);

            memcpy((void *)bReference,(bDocument + pe -> fileOffset),32);

            for ( long j = 0; j < 32; j++ ) {
               if ( bReference[j] == 0x0D )
                  bReference[j] = '_';
               if ( bReference[j] == 0x0A )
                  bReference[j] = '_';
            }

            for ( long j = 0; j < 32; j++ )
               fprintf(fOutput,"%c",(char *)bReference[j]);

            fprintf(fOutput,"\n");

            break;
            }

         case 2: {

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

            fprintf(fOutput,"entry: %05ld  %05ld  %05ld  %05ld  %05ld %c %08ld  \n",pe -> type,pe -> objectId,pe -> generationNumber,pe -> objectStreamId,pe -> objectStreamIndex,pe -> status,pe -> fileOffset);

            break;
            }

         }

      }

      delete [] pWArray;

   }
   
   fclose(fOutput);

   return;
   }

#endif

   PdfObject * PdfXReference::InfoObject() {
   for ( std::list<PdfCrossReferenceStream *>::iterator itXrefObjects = xRefObjects.begin(); itXrefObjects != xRefObjects.end(); itXrefObjects++ ) {
      BYTE *pValue = (*itXrefObjects) -> pObject -> Dictionary() -> Value("Info");
      if ( pValue ) 
         return pDocument ->  FindObjectById(atol((char *)pValue));
   }
   for ( std::list<PdfTrailer *>::iterator it = trailerObjects.begin(); it != trailerObjects.end(); it ++ ) {
      PdfTrailer *pTrailer = (*it);
      BYTE *pValue = pTrailer -> Dictionary() -> Value("Info");
      if ( pValue ) 
         return pDocument ->  FindObjectById(atol((char *)pValue));
   }
   return 0;
   }


