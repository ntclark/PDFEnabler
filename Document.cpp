
#include "pdfEnabler.h"

#include "Library.h"

#include "Document.h"
#include "XReference.h"

   BYTE eol = 0x0D;

   PdfObject *PdfDocument::pEncryptionObject = NULL;
   PdfEncryption * PdfDocument::pEncryptionUtility = NULL;

   char PdfDocument::szErrorMessage[1024];

   PdfDocument::PdfDocument(PdfEnabler *pp) :

      PdfEntity(NULL),

      pParent(pp),
      refCount(0),
      preambleLength(0),
      initialMaxObjectId(0),
      writeUncompressed(false),

      pXReference(NULL),
      pCatalogObject(NULL),
      pPageTreeRootObject(NULL),
      pageObjectCount(0),
      pageCount(0),
      pPageNumberTree(NULL)
   {
   return;
   }

   PdfDocument::PdfDocument(char *pszFileName) :

      PdfEntity(fopen(pszFileName,"rb")),

      pParent(NULL),
      refCount(0),
      preambleLength(0),
      initialMaxObjectId(0),
      writeUncompressed(false),

      pXReference(NULL),
      pCatalogObject(NULL),
      pPageTreeRootObject(NULL),
      pageObjectCount(0),
      pageCount(0),
      pPageNumberTree(NULL)
   {
   strcpy(szFileName,pszFileName);
   parseDocument();
   return;
   }


   PdfDocument::~PdfDocument() {

   for ( std::map<long,PdfPage *>::iterator it = pages.begin(); it != pages.end(); it++ )
      delete (*it).second;
   pages.clear();

   for ( std::list<PdfObject *>::iterator it = objectList.begin(); it != objectList.end(); it++ ) 
      delete (*it);
   objectList.clear();

   if ( pPageNumberTree )
      delete pPageNumberTree;

   if ( pXReference )
      delete pXReference;

//   if ( pEncryptionUtility )
//      delete pEncryptionUtility;

   return;
   }


   void PdfDocument::parseDocument() {

   resetEncryptionObject();

   BYTE *pData = Storage();
   BYTE *pStart = pData;
   BYTE *pEnd = pStart + BinaryDataSize();

   for ( ; ; ) {

      long entityLength;

      BYTE *pObjectData = pdfUtility.ScanObject(edObject,pStart,pEnd,&entityLength);

      if ( ! pObjectData )
         break;

      BYTE *p = pObjectData;

      RETREAT_TO_BOL(p,pStart)

      PdfObject *pObject = new PdfObject(this,p,entityLength + pObjectData - p);

      pObject -> FileOffset(p - pData);

      AddObject(pObject);

      if ( ! preambleLength ) 
         preambleLength = p - pStart;

      pStart = pObjectData + entityLength;

   }

   pXReference = new PdfXReference(this,Storage(),Storage() + BinaryDataSize());

   long encryptionObjectId = pXReference -> FindEncryptionObject();

   if ( encryptionObjectId ) {
      pEncryptionObject = FindObjectById(encryptionObjectId);
      pEncryptionUtility = new PdfEncryption(this,pEncryptionObject); 
      if ( ! pEncryptionUtility -> Authenticate("antsap","pubfly") ) {
         if ( ! pEncryptionUtility -> Authenticate(NULL,NULL) ) {
//            delete pEncryptionUtility;
//            pEncryptionUtility = NULL;
         }
      }
   }

   DecryptAll();

//
// NTC: 05-03-2012: Decompression takes a lot of memory. I am going to attempt to refactor a little bit to 
// decompress on an as-needed basis.
//
//   DecompressAll();
//
// I have not found any significant place(s) where the above change caused an impact - except in unpackObjectStreams, Unwind() and Rewind().
//
// There is usually no reason to inspect and/or manipulate the data in the stream. The exceptions are cross reference streams and
// object streams. Or, when needing to parse the postscript in them. Otherwise, I think the above change affects only a few other
// places, but has a huge impact on memory and performance.
//

   pXReference -> UnWind();

   unpackObjectStreams();

   initialMaxObjectId = NewObjectId() - 1;

   long pageTreeRootObjectNumber = -1L;

   BYTE *pValue = pXReference -> Value("Root");

   if ( pValue ) {

      pageTreeRootObjectNumber = pdfUtility.IntegerValue((char *)pValue);

      if ( objectMap.find(pageTreeRootObjectNumber) != objectMap.end() )

         pCatalogObject = objectMap[pageTreeRootObjectNumber];

   }

   if ( ! pCatalogObject ) 
      pCatalogObject = FindObjectByType("/Catalog");

   if ( pCatalogObject ) {

      PdfDictionary *pPageLabelsDictionary = pCatalogObject -> Dictionary("PageLabels");

      if ( pPageLabelsDictionary )
         pPageNumberTree = new PdfNumberTree(pPageLabelsDictionary);

      BYTE *pValue = pCatalogObject -> Value("Pages");

      if ( pValue ) {

         pageTreeRootObjectNumber = pdfUtility.IntegerValue((char *)pValue);

         if ( objectMap.find(pageTreeRootObjectNumber) != objectMap.end() )

            pPageTreeRootObject = objectMap[pageTreeRootObjectNumber];

         else

            pPageTreeRootObject = NULL;

      }

   }

   if ( pPageTreeRootObject ) {

      pageObjectCount = 0;

      BYTE *pValue = pPageTreeRootObject -> Value("Count");

      if ( pValue )
         pageCount = atol((char *)pValue);

      walkPageObjectTreeNode(pageTreeRootObjectNumber);

   }

#ifdef DUMP_DICTIONARIES
   dumpDictionaries(" afterRead");
#endif

#ifdef DUMP_STREAMS
   dumpStreams(" afterRead");
#endif

#ifdef DUMP_TRAILERS
   dumpTrailers(szFileName,NULL," Input Trailers");
#endif

#ifdef DUMP_XREF
   pXReference -> dumpInitialXRef(Storage(),szFileName,"initial");
#endif

   return;
   }


   void PdfDocument::AddObject(PdfObject *p) {
   objectMap[p -> Id()] = p;
   objectList.insert(objectList.end(),p);
   return;
   }


   PdfPage *PdfDocument::GetPage(long pageNumber,char *pszPageLabel) {

   if ( ! pPageTreeRootObject )
      return NULL;

   if ( pPageNumberTree && pszPageLabel && pszPageLabel[0] ) {
      long index = 0;
      for ( std::list<char *>::iterator it = pPageNumberTree -> begin(); it != pPageNumberTree -> end(); it++ ) {
         index++;
         if ( strlen((*it)) != strlen(pszPageLabel) )
            continue;
         if ( 0 == strcmp((*it),pszPageLabel) )
            return pages[index];
      }
   }

   if ( pages.find(pageNumber) == pages.end() )
      return NULL;

   return pages[pageNumber];
   }   


   long PdfDocument::PageFromLabel(char *pszPageLabel) {

   if ( ! pPageNumberTree )
      return -1L;

   if ( ! pszPageLabel || ! pszPageLabel[0] )
      return -1L;

   long index = 0;
   for ( std::list<char *>::iterator it = pPageNumberTree -> begin(); it != pPageNumberTree -> end(); it++ ) {
      index++;
      if ( strlen((*it)) != strlen(pszPageLabel) )
         continue;
      if ( 0 == strcmp((*it),pszPageLabel) )
         return index;
   } 
  
   return -1L;
   }


   char *PdfDocument::LabelFromPage(long pageNumber) {

   if ( ! pPageNumberTree )
      return NULL;

   long index = 0;

   for ( std::list<char *>::iterator it = pPageNumberTree -> begin(); it != pPageNumberTree -> end(); it++ ) {
      index++;
      if ( index == pageNumber )
         return (*it);
   }

   return NULL;
   }


   long PdfDocument::walkPageObjectTreeNode(long nodeNumber) {
   PdfObject *pObject = objectMap[nodeNumber];
   if ( ! pObject )
      return 0;
   BYTE *pValue = pObject -> Value("Kids");
   if ( ! pValue ) {
      pages[pageObjectCount + 1] = new PdfPage(this,pageObjectCount + 1,pObject);
      pageObjectCount++;
      return 0;
   }
   long k = 1;
   long childNodeNumber = -1L;
   while ( -1L != (childNodeNumber = pdfUtility.IntegerValueFromReferenceArray((char *)pValue,k)) ) {
      walkPageObjectTreeNode(childNodeNumber);
      k++;
   }
   return 0;
   }


   long PdfDocument::GetPageCount() {
   return pageCount;
   }


   char * PdfDocument::ID() {
   if ( ! pXReference )
      return NULL;
   return (char *)pXReference -> Value("ID");   
   }


   PdfObject * PdfDocument::SearchByIdAndFileOffset(long id,long fileOffset) {
   for ( std::list<PdfObject *>::iterator it = objectList.begin(); it != objectList.end(); it++ )
      if ( (*it) -> Id() == id && (*it) -> FileOffset() == fileOffset )
         return (*it);
   return NULL;
   }


   PdfObject * PdfDocument::FindObjectByType(char *pszType) {

   for ( std::map<long,PdfObject *>::iterator it = objectMap.begin(); it != objectMap.end(); it++ ) {
   
      PdfObject *pObject = (*it).second;
   
      BYTE *pValue = pObject -> Value("Type");

      if ( ! pValue )
         continue;

      if ( strncmp((char *)pValue,pszType,strlen(pszType)) )
         continue;

      return pObject;
   
   }

   return NULL;
   }


   std::list<PdfObject *> * PdfDocument::CreateObjectListByType(char *pszType) {

   std::list<PdfObject *> *pNewObjectList = NULL;

   for ( std::list<PdfObject *>::iterator it = objectList.begin(); it != objectList.end(); it++ ) {
   
      PdfObject *pObject = (*it);
   
      BYTE *pValue = pObject -> Value("Type");

      if ( ! pValue )
         continue;

      if ( strncmp((char *)pValue,pszType,strlen(pszType)) )
         continue;

      if ( ! pNewObjectList )
         pNewObjectList = new std::list<PdfObject *>();

      pNewObjectList -> insert(pNewObjectList -> end(),pObject);
   
   }

   return pNewObjectList;
   }


   PdfObject * PdfDocument::FindObjectByDictionaryEntry(char *pszEntry) {

   for ( std::map<long,PdfObject *>::iterator it = objectMap.begin(); it != objectMap.end(); it++ ) {
   
      PdfObject *pObject = (*it).second;
   
      BYTE *pValue = pObject -> Value(pszEntry);

      if ( pValue )
         return pObject;
   
   }

   return NULL;
   }


   PdfObject * PdfDocument::FindObjectByNamedDictionaryEntry(char *pszEntry,char *pszValue) {

   for ( std::map<long,PdfObject *>::iterator it = objectMap.begin(); it != objectMap.end(); it++ ) {
   
      PdfObject *pObject = (*it).second;
   
      BYTE *pValue = pObject -> Value(pszEntry);

      if ( ! pValue )
         continue;

      if ( strlen((char *)pValue) != strlen(pszValue) )
         continue;

      if ( strcmp((char *)pValue,pszValue) )
         continue;

      return pObject;
   
   }

   return NULL;
   }


   PdfObject * PdfDocument::FindObjectById(long id) {
#if 0
   if ( objectMap.find(id) == objectMap.end() )
      return NULL;
   return objectMap.find(id) -> second;
#else
   for ( std::list<PdfObject *>::iterator it = objectList.begin(); it != objectList.end(); it++ ) {
      PdfObject *p = (*it);
      if ( p -> Id() == id )
         return p;
   }
   return NULL;
#endif
   }


   PdfObject *PdfDocument::IndirectObject(char *pszObjectReference) {

   long n = strlen(pszObjectReference);

   char *pszCopy = new char[n + 1];
   memset(pszCopy,0,(n + 1) * sizeof(char));

   strcpy(pszCopy,pszObjectReference);

   char *p = pszCopy;
   char *pEnd = p + n;
   while ( p < pEnd && ! ( '0' <= *p && *p <= '9' ) ) 
      p++;

   if ( p == pEnd ) {
      delete [] pszCopy;
      return NULL;
   }

   n = atol(p);

   delete [] pszCopy;

   return objectMap[n];
   }


   long PdfDocument::Encrypt(PdfObject *pObject,BYTE *pInputData,long dataSize) { 
   if ( ! pEncryptionUtility ) 
      return 0; 
   return pEncryptionUtility -> Encrypt(pObject,pInputData,dataSize);
   }


   long PdfDocument::Decrypt(PdfObject *pObject,BYTE *pInputData,long dataSize) { 
   if ( ! pEncryptionUtility ) 
      return 0; 
   return pEncryptionUtility -> Decrypt(pObject,pInputData,dataSize);
   }


   long PdfDocument::NewObjectId() {
   return pXReference -> MaxObjectId() + 1;
   }


   void PdfDocument::DecryptAll() {
   for ( std::list<PdfObject *>::iterator it = objectList.begin(); it != objectList.end(); it++ )
      (*it) -> Decrypt();
   return;
   }


   void PdfDocument::EncryptAll() {
   for ( std::list<PdfObject *>::iterator it = objectList.begin(); it != objectList.end(); it++ )
      (*it) -> Encrypt();
   return;
   }


   void PdfDocument::DecompressAll() {
   for ( std::list<PdfObject *>::iterator it = objectList.begin(); it != objectList.end(); it++ ) {
      PdfStream *pStream = (*it) -> Stream();
      if ( pStream ) {
         pStream -> Decompress();
      }
   }
   return;
   }


   void PdfDocument::CompressAll() {
   for ( std::list<PdfObject *>::iterator it = objectList.begin(); it != objectList.end(); it++ ) {
      PdfStream *pStream = (*it) -> Stream();
      if ( pStream ) {
         pStream -> Compress();
      }
   }
   return;
   }



   void PdfDocument::unpackObjectStreams() {

   for ( std::list<PdfObject *>::iterator it = objectList.begin(); it != objectList.end(); it++ ) {

      PdfObject *po = (*it);

      PdfDictionary *pDictionary = po -> Dictionary();

      if ( ! pDictionary )
         continue;

      PdfStream *pStream = po -> Stream();

      if ( ! pStream )
         continue; 

      BYTE *pValue = pDictionary -> Value("Type");

      if ( ! pValue )
         continue;

      if ( strncmp((char *)pValue,"/ObjStm",7) )
         continue;

      objectStreams.insert(objectStreams.end(),po);

      pValue = pDictionary -> Value("First");
   
      long streamObjectOffsetFirst = pdfUtility.IntegerValue((char *)pValue);
   
      pValue = pDictionary -> Value("N");
   
      long streamObjectCount = pdfUtility.IntegerValue((char *)pValue);
   
      long *pObjectPairs = new long[2 * (streamObjectCount + 1)];
   
      memset(pObjectPairs,0,2 * (streamObjectCount + 1) * sizeof(long));
   
      BYTE *pStreamUncompressedData = pStream -> DecompressedStorage();

      if ( ! pStreamUncompressedData )
         continue;
   
      long streamLength = pStream -> BinaryDataSize();
   
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

      pObjectPairs[2 * streamObjectCount] = 0;
      pObjectPairs[2 * streamObjectCount + 1] = streamLength;
   
      BYTE *pObject = pStreamUncompressedData + pObjectPairs[1];
   
      n = 0;
   
      for ( long j = 0; j < streamObjectCount; j++, n += 2 ) {
   
         long objectLength = pObjectPairs[n + 3] - pObjectPairs[n + 1];

         PdfObject *pNewObject = new PdfObject(this,pObjectPairs[n],0,pStreamUncompressedData + pObjectPairs[n + 1],objectLength);
         pNewObject -> InObjectStream(po -> Id());
   
         AddObject(pNewObject);
   
      }
   
      delete [] pObjectPairs;

   }

   return;
   }


   void PdfDocument::repackObjectStreams() {

   /*
      3.4.6 Object Streams

      PDF 1.5 introduces a new kind of stream, an object stream, which contains a sequence of PDF objects.
      The purpose of object streams is to allow a greater number of PDF objects to be compressed, thereby 
      substantially reducing the size of PDF files. The objects in the stream are referred to as compressed 
      objects. (This term is used regardless of whether the stream is actually encoded with a compression filter.)

      Any PDF object can appear in an object stream, with the following exceptions:
         •Stream objects
         •Objects with a generation number other than zero
         •A document’s encryption dictionary (see Section 3.5, “Encryption”)
         •An object representing the value of the Length entry in an object stream dictionary

   */

   for ( std::list<PdfObject *>::iterator it = objectStreams.begin(); it != objectStreams.end(); it++ ) {

      PdfObject *pObject = (*it);

      PdfDictionary *pDictionary = pObject -> Dictionary();

      PdfStream *pStream = pObject -> Stream();

      /*
         Type  name        (Required) The type of PDF object that this dictionary describes; 
                           must be ObjStm for an object stream.

         N     integer     (Required) The number of compressed objects in the stream.

         First integer     (Required) The byte offset (in the decoded stream) of the first compressed object.

         Extends  stream   (Optional) A reference to an object stream, of which the current object stream is 
                           considered an extension. Both streams are considered part of a collection of object 
                           streams (see below). A given collection consists of a set of streams whose 
                           Extends links form a directed acyclic graph.   

         The stream data in an object stream consists of the following items:

            •  N pairs of integers, where the first integer in each pair represents 
               the object number of a compressed object and the second integer 
               represents the byte offset of that object, relative to the first 
               one. The offsets must be in increasing order, but there is no 
               restriction on the order of object numbers.

         Note: The byte offset in the decoded stream of the first object is the 
               value of the First entry.

            •  The N objects stored consecutively. Only the object values are stored in the 
               stream; the obj and endobj keywords are not used. A compressed dictionary or 
               array may contain indirect references.

         Note: It is illegal for a compressed object to consist of only an indirect 
               reference; for example, 3 0 R.

         By contrast, dictionaries and arrays in content streams (Section 3.7.1) may not 
         contain indirect references. In an encrypted file, strings occurring anywhere 
         in an object stream must not be separately encrypted, since the entire object 
         stream is encrypted.

      */

      BYTE *pValue = pDictionary -> Value("N");
   
      long streamObjectCount = pdfUtility.IntegerValue((char *)pValue);
   
      long *pObjectPairs = new long[2 * (streamObjectCount + 1)];
   
      memset(pObjectPairs,0,2 * (streamObjectCount + 1) * sizeof(long));
//
//NTC: 03-14-2012: I discovered that the stream was compressed at this point and was causing the
// values parsed from it below to be nonsense.
// It is not clear why the stream was compressed or if the document was getting written twice without an intervening
// open that would have uncompressed the first write.
// In any case - detecting and uncompressing the stream fixed the problem, however, there are other places
// where object stream manipulation is occurring where this check for compression is not made.
//
      BYTE *pStreamUncompressedData = pStream -> DecompressedStorage();
   
      for ( long j = 0; j < 2 * streamObjectCount; j += 2 ) 
         pObjectPairs[j] = pdfUtility.IntegerValueFromArray((char *)pStreamUncompressedData,j + 1);

      pObjectPairs[2 * streamObjectCount] = 0;
   
      BYTE *pUncompressedSpace = new BYTE[65535];

      long bytesCopied = 0;

      for ( long j = 0; j < 2 * streamObjectCount; j += 2 ) {
   
         long objectId = pObjectPairs[j];

         pObjectPairs[j + 1] = bytesCopied;
   
         PdfObject *pMemberObject = FindObjectById(objectId);
   
         long n = 0;

         if ( pMemberObject -> Dictionary() ) {
            n = pMemberObject -> Dictionary() -> BinaryDataSize();
            memcpy(pUncompressedSpace + bytesCopied,pMemberObject -> Dictionary() -> Storage(),n);
            bytesCopied += n;
         }

         n = pMemberObject -> BinaryDataSize();
         if ( n ) {
            memcpy(pUncompressedSpace + bytesCopied,pMemberObject -> Storage(),n);
            bytesCopied += n;
         }

         if ( pMemberObject -> Stream() ) {
            n = pMemberObject -> Stream() -> BinaryDataSize();
            memcpy(pUncompressedSpace + bytesCopied,pMemberObject -> Stream() -> Storage(),n);
            bytesCopied += n;
         }

      }

      BYTE *pNewPreamble = new BYTE[16384];

      long preambleBytesWritten = 0;

      for ( long j = 0; j < 2 * streamObjectCount; j += 2 )
         preambleBytesWritten += sprintf((char *)(pNewPreamble + preambleBytesWritten),"%ld %ld ",pObjectPairs[j],pObjectPairs[j + 1]);

      BYTE *pFinalResult = new BYTE[preambleBytesWritten + bytesCopied];

      memcpy(pFinalResult,pNewPreamble,preambleBytesWritten);

      memcpy(pFinalResult + preambleBytesWritten,pUncompressedSpace,bytesCopied);

      pStream -> ReallocateStorage(pFinalResult,preambleBytesWritten + bytesCopied,0);

      pDictionary -> SetValue("First",preambleBytesWritten);

      delete [] pFinalResult;
      delete [] pNewPreamble;
      delete [] pUncompressedSpace;
   
      delete [] pObjectPairs;

   }

   return;
   }


   long PdfDocument::counterValue(char *pszCounterName) {

   PdfObject *pObject = FindObjectByDictionaryEntry(pszCounterName);

   if ( ! pObject ) {
      long objectId = NewObjectId();
      char szObject[MAX_PATH];
      sprintf(szObject,"%ld 0 obj<</%s 0>>endobj",objectId,pszCounterName);
      pObject = new PdfObject(this,(BYTE *)szObject,strlen(szObject));
      AddObject(pObject);
      if ( XReference() )
         XReference() -> AddObject(pObject);
      return 0L;
   }

   PdfDictionary *pDictionary = pObject -> Dictionary();
   
   BYTE *pValue = pDictionary -> Value(pszCounterName);

   return atol((char *)pValue);
   }


   long PdfDocument::incrementCounter(char *pszCounterName) {

   PdfObject *pObject = FindObjectByDictionaryEntry(pszCounterName);

   if ( ! pObject ) {
      long objectId = NewObjectId();
      char szObject[MAX_PATH];
      sprintf(szObject,"%ld 0 obj<</%s 1>>endobj",objectId,pszCounterName);
      pObject = new PdfObject(this,(BYTE *)szObject,strlen(szObject));
      AddObject(pObject);
      if ( XReference() )
         XReference() -> AddObject(pObject);
      return 0L;
   }

   PdfDictionary *pDictionary = pObject -> Dictionary();
   
   BYTE *pValue = pDictionary -> Value(pszCounterName);

   long currentValue = atol((char *)pValue);

   char szValue[32];

   sprintf(szValue,"%ld",currentValue + 1);

   pDictionary -> SetValue(pszCounterName,(BYTE *)szValue,false);

   return currentValue + 1;
   }


#if defined REMOVE_ENCRYPTION || defined REMOVE_COMPRESSION
   void PdfDocument::RemoveEncryption() {
   DecryptAll();
   resetEncryptionObject();
   pXReference -> RemoveEncryption();
   return;
   }
#endif


   long PdfDocument::Write(char *pszFileName) {

#ifdef DUMP_DICTIONARIES
   dumpDictionaries(" before Write");
#endif

   FILE *fOutput = fopen(pszFileName,"wb+");

   if ( ! fOutput )
      return 0L;

   pXReference -> ReWind();

#if defined REMOVE_ENCRYPTION || defined REMOVE_COMPRESSION
   RemoveEncryption();
#endif

   repackObjectStreams();

#if ! defined REMOVE_COMPRESSION
   CompressAll();
#endif

#if ! defined REMOVE_ENCRYPTION 
   EncryptAll();
#endif

   do {

      fseek(fOutput,0,SEEK_SET);

      fwrite(Storage(),preambleLength,1,fOutput);

      for ( std::list<PdfObject *>::iterator it = objectList.begin(); it != objectList.end(); it++ )
         (*it) -> Write(fOutput,writeUncompressed);

      pXReference -> Write(fOutput,writeUncompressed);

   } while ( 0 != pXReference -> ReWind() );

   fclose(fOutput);

#ifdef DUMP_TRAILERS
   dumpTrailers(szFileName,pszFileName," Output Trailers");
#endif

#ifdef DUMP_XREF
    dumpXRef(pszFileName," after Write");
#endif

#ifdef DUMP_STREAMS
   dumpStreams(" after Write");
#endif

#ifdef DUMP_DICTIONARIES
   dumpDictionaries(" after Write");
#endif

   return 1L;
   }   


   long PdfDocument::WriteUncompressed(char *pszFileName) {
   DecompressAll();
   writeUncompressed = true;
   long rc = Write(pszFileName);
   writeUncompressed = false;
   return rc;
   }


#ifdef DUMP_DICTIONARIES

   void PdfDocument::dumpDictionaries(char *szPostAmble) {

   char szTemp[MAX_PATH];

   strcpy(szTemp,szFileName);
   char *p = strrchr(szTemp,'.');
   if ( p )
      *p = '\0';

   char szDump[MAX_PATH];
   
   sprintf(szDump,"%s%s%s",szTemp,"-dictionaries-",szPostAmble);

   FILE *fOutput = fopen(szDump,"wt");

   for ( std::map<long,PdfObject *>::iterator itObjects = objectMap.begin(); itObjects != objectMap.end(); itObjects++ ) {

      PdfObject *po = (*itObjects).second;

      fprintf(fOutput,"%ld %ld obj\n",po -> Id(),po -> Generation());

      if ( ! po -> Dictionary() ) {
         fprintf(fOutput,"endobj\n");
         continue;
      }

      po -> Dictionary() -> Write(fOutput);

      fprintf(fOutput,"\n");

   }

   fclose(fOutput);

   return;
   }
#endif


#ifdef DUMP_STREAMS
   void PdfDocument::dumpStreams(char *szPostAmble) {

#if ! defined REMOVE_COMPRESSION
   DecryptAll();
   DecompressAll();
#endif

   char szTemp[MAX_PATH];

   strcpy(szTemp,szFileName);
   char *p = strrchr(szTemp,'.');
   if ( p )
      *p = '\0';

   char szDump[MAX_PATH];
   
   sprintf(szDump,"%s%s%s",szTemp,"-streams-",szPostAmble);

   FILE *fOutput = fopen(szDump,"wt");

   for ( std::map<long,PdfObject *>::iterator itObjects = objectMap.begin(); itObjects != objectMap.end(); itObjects++ ) {

      PdfObject *po = (*itObjects).second;

      fprintf(fOutput,"%ld %ld obj\n",po -> Id(),po -> Generation());

      if ( ! po -> Stream() ) {
         fprintf(fOutput,"endobj\n");
         continue;
      }

      po -> Stream() -> WriteHex(fOutput);

      fprintf(fOutput,"\n");

   }

   fclose(fOutput);

   return;
   }
#endif


#ifdef DUMP_TRAILERS

   void PdfDocument::dumpTrailers(char *pszSourceFileName,char *pszProducedFileName,char *szPostAmble) {

   char szTemp[MAX_PATH];

   strcpy(szTemp,szFileName);
   char *p = strrchr(szTemp,'.');
   if ( p )
      *p = '\0';

   char szDump[MAX_PATH];
   
   sprintf(szDump,"%s%s%s",szTemp,"-trailers-",szPostAmble);

   FILE *fOutput = fopen(szDump,"wb");

   pXReference -> dumpTrailers(pszSourceFileName,pszProducedFileName,fOutput);

//   pXReference -> Write(fOutput);

   fclose(fOutput);

   return;
   }

#endif


#ifdef DUMP_XREF
   void PdfDocument::dumpXRef(char *szSourceFileName,char *pszFilePostAmble) {
   FILE *fDocument = fopen(szSourceFileName,"rb");
   fseek(fDocument,0,SEEK_END);
   long fileSize = ftell(fDocument);
   BYTE *bDocument = new BYTE[fileSize];
   fseek(fDocument,0,SEEK_SET);
   long k = fread(bDocument,1,fileSize,fDocument);
   fclose(fDocument);
   pXReference -> dumpXRef(bDocument,szFileName,pszFilePostAmble);
   delete [] bDocument;
   return;
   }
#endif