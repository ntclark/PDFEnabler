
#include "Library.h"

#include "Object.h"
#include "Document.h"

   PdfObject::PdfObject(PdfDocument *pp,BYTE *pSourceData,long dSize) :

      pDocument(pp),

      PdfEntity(pSourceData,dSize),

      id(0),
      generation(0),
      status('n'),
      fileOffset(0),
      inObjectStream(0),

      postAmble(NULL),
      postAmbleSize(0),
      reservedPrintSize(-1L),

      pDictionary(NULL),
      pStream(NULL)

   {

   if ( ! Storage() )
      return;

   sscanf((char *)Storage(),"%ld %ld",&id,&generation);

   BYTE *pStart = pSourceData + Offset();
   BYTE *pEnd = pSourceData + dSize;

   while ( pStart < pEnd && ( ' ' == *pStart || ('0' <= *pStart && *pStart <= '9') ) )
      pStart++;

   if ( 0 == strncmp((char *)pStart,"obj",3) )
      pStart += 3;

   if ( 0x0A == *pStart || 0x0D == *pStart )
      pStart += 1;

   if ( 0x0A == *pStart || 0x0D == *pStart )
      pStart += 1;

   pEnd = pSourceData + Offset() + BinaryDataSize() - 1;

   while ( pEnd > pStart && strncmp((char *)pEnd,"endobj",6) )
      pEnd--;

   if ( pEnd > pStart ) 
      ReallocateStorage(pStart,(long)(pEnd - pStart),(long)(Offset() + (pStart - pSourceData)));   

   bool isArrayObject = false;
   if ( 0 == strncmp((char *)pStart,entityDelimiters[edArray],strlen(entityDelimiters[edArray])) ) 
      isArrayObject = true;

   if ( ! isArrayObject )
      findDictionary();

   findStream();

#ifdef REMOVE_LINEARIZATION
   if ( pDictionary )
      pDictionary -> RemoveValue("Linearized");
#endif

   return;
   }


   /*
      This constructor must be called with uncompressed and un-encrypted data at all times
   */

    PdfObject::PdfObject(PdfDocument *pd,long id,long gen,BYTE *pData,long dSize) :

      pDocument(pd),

      PdfEntity(pData,dSize),

      id(id),
      generation(gen),
      status('n'),
      fileOffset(0),
      inObjectStream(0),

      postAmble(NULL),
      postAmbleSize(0),
      reservedPrintSize(-1L),

      pDictionary(NULL),
      pStream(NULL)

   {

// 8/21/09: Does this object have an "[" as it's first character ?
// No implementation in case it does ?!!
//
   findDictionary();

   findStream();

   if ( pStream  ) {
      pStream -> SetUncompressed();
      pStream -> SetDecrypted();
   }

#ifdef REMOVE_LINEARIZATION
   if ( pDictionary )
      pDictionary -> RemoveValue("Linearized");
#endif

   return;
   }

    PdfObject::PdfObject(PdfDocument *pd,BYTE *pInput,BYTE *pTop) :

      pDocument(pd),

      PdfEntity(edObject,esdScanUp,pInput,pTop),

      id(0),
      generation(0),
      status('n'),
      fileOffset(0),
      inObjectStream(0),

      postAmble(NULL),
      postAmbleSize(0),
      reservedPrintSize(-1L),

      pDictionary(NULL),
      pStream(NULL)

   {

   BYTE *pStart = Storage();

   if ( pStart ) {
      pStart = pTop + Offset();
      RETREAT_TO_BOL(pStart,pTop)
      long preambleLength = (long)(pTop + Offset() - pStart);
      ReallocateStorage(pStart,preambleLength + BinaryDataSize(),(long)(pStart - pTop));
      sscanf((char *)Storage(),"%ld %ld",&id,&generation);
   }

   findDictionary();

   findStream();

#ifdef REMOVE_LINEARIZATION
   if ( pDictionary )
      pDictionary -> RemoveValue("Linearized");
#endif

   return;
   }


   PdfObject::~PdfObject() {
   if ( pDictionary )
      delete pDictionary;
   if ( pStream )
      delete pStream;
   if ( postAmbleSize )
      delete postAmble;
   for ( std::list<PdfDictionary *>::iterator it = additionalDictionaries.begin(); it != additionalDictionaries.end(); it++ ) 
      delete (*it);
   additionalDictionaries.clear();
   }


   void PdfObject::SetPostAmble(BYTE *pData,long dataSize) {
   if ( postAmble )
      delete [] postAmble;
   postAmble = new BYTE[dataSize + 1];
   postAmble[dataSize] = '\0';
   memcpy(postAmble,pData,dataSize);
   postAmbleSize = dataSize;
   return;
   }


   long PdfObject::findDictionary() {

   pDictionary = new PdfDictionary(this,Storage(),Storage() + BinaryDataSize());

   long lengthDictionary = pDictionary -> BinaryDataSize();

   if ( ! lengthDictionary ) {
      delete pDictionary;
      pDictionary = NULL;
      return 0;
   }

   long offsetDictionary = pDictionary -> Offset();

   if ( 0 == offsetDictionary ) {
      BYTE *pNew = new BYTE[BinaryDataSize() - lengthDictionary];
      memcpy(pNew,Storage() + lengthDictionary,BinaryDataSize() - lengthDictionary);
      ReallocateStorage(pNew,BinaryDataSize() - lengthDictionary,Offset() + lengthDictionary);
      return 1;
   }

   if ( BinaryDataSize() == lengthDictionary ) {
      ReallocateStorage(NULL,0,0);      
      return 1;
   }

   BYTE *pTopHalf = new BYTE[BinaryDataSize() - lengthDictionary + 1];

   memset(pTopHalf,0,(BinaryDataSize() - lengthDictionary + 1) * sizeof(BYTE));

   memcpy(pTopHalf,Storage(),offsetDictionary);

   memcpy(pTopHalf + offsetDictionary,Storage() + offsetDictionary + lengthDictionary,BinaryDataSize() - lengthDictionary - offsetDictionary);

   long newSize = BinaryDataSize() - lengthDictionary;

   ReallocateStorage(pTopHalf,newSize,0);

   delete [] pTopHalf;

   if ( ! pDictionary -> BinaryDataSize() ) {
      delete pDictionary;
      pDictionary = NULL;
   }

   return 1;
   }


   long PdfObject::findStream() {

   pStream = new PdfStream(this,pDictionary,Storage(),Storage() + BinaryDataSize());

   if ( ! pStream -> BinaryDataSize() ) {
      delete pStream;
      pStream = NULL;
      return 0;
   }

   return 1;
   }


   BYTE *PdfObject::Value(char *pszKeyName) {
   if ( pDictionary ) 
      return pDictionary -> Value(pszKeyName);
   return NULL;
   }


   BYTE *PdfObject::DeReferencedValue(char *pszKeyName) {
   if ( pDictionary ) 
      return pDictionary -> DeReferencedValue(pszKeyName);
   return NULL;
   }


   long PdfObject::SetValue(char *pszKeyName,BYTE *pszValue) {
   if ( pDictionary ) 
      return pDictionary -> SetValue(pszKeyName,pszValue);
   return 0L;
   }


   PdfObject *PdfObject::DeReferencedObject(char *pszKeyName) {

   if ( ! pDictionary )
      return NULL;

   BYTE *pValue = Value(pszKeyName);

   if ( ! pValue )
      return NULL;

   if ( ! pdfUtility.IsIndirect((char *)pValue) )
      return NULL;

   return Document() -> IndirectObject((char *)pValue);
   }


   PdfDictionary *PdfObject::Dictionary(char *pszDictionaryname) {

   BYTE *pValue = Value(pszDictionaryname);

   if ( ! pValue )
      return NULL;

   if ( pdfUtility.IsIndirect((char *)pValue) )
      return Document() -> IndirectObject((char *)pValue) -> Dictionary();

// 8-13-2011: ??? It is not clear why this was creating a new dictionary 
// rather than return the existing dictionary element (?!)
//
#if 0
   PdfDictionary *pDictionary = new PdfDictionary(this,pValue,strlen((char *)pValue));
   additionalDictionaries.insert(additionalDictionaries.end(),pDictionary);
#endif

   return Dictionary() -> GetDictionary(pszDictionaryname);
   }


   void PdfObject::RemoveDictionary() {
   if ( ! pDictionary )
      return;
   delete pDictionary;
   pDictionary = NULL;
   BYTE szTemp[32];
   sprintf((char *)szTemp,"%%comment %c",0x0D);
   ReallocateStorage(szTemp,(long)strlen((char *)szTemp),0);
   return;
   }


   void PdfObject::RemoveStream() {
   if ( ! pStream )
      return;
   delete pStream;
   pStream = NULL;
   char szTemp[32];
   sprintf(szTemp,"stream endstream");
   pStream = new PdfStream(this,pDictionary,(BYTE *)szTemp,(BYTE *)(szTemp + strlen(szTemp)));
   SetValue("Length",(BYTE *)"0");
   return;
   }


   long PdfObject::Encrypt(BYTE *pInputData,long dataSize) { 
   if ( this == pDocument -> EncryptionObject() )
      return 0;
   if ( -1L == dataSize ) {
      if ( pDictionary )
         pDictionary -> Encrypt();
      if ( pStream )
         pStream -> Encrypt();
      return 1;
   }
   return pDocument -> Encrypt(this,pInputData,dataSize);
   }


   long PdfObject::Decrypt(BYTE *pInputData,long dataSize) { 
   if ( this == pDocument -> EncryptionObject() )
      return 0;
   if ( -1L == dataSize ) {
      if ( pDictionary )
         pDictionary -> Decrypt();
      if ( pStream )
         pStream -> Decrypt();
      return 1;
   }
   return pDocument -> Decrypt(this,pInputData,dataSize);
   }


   long PdfObject::StringWrite(char *pString,bool sizeOnly) {

   if ( InObjectStream() )
      return 0;

   long bytesWritten = 0L;

   char szTemp[32];

#ifdef CR_AFTER_OBJECT
#ifdef HARD_RETURN_AFTER_OBJECT
   sprintf(szTemp,"%ld 0 obj%c%c",Id(),0x0D,0x0A);
#else
   sprintf(szTemp,"%ld 0 obj%c",Id(),eol);
#endif
#else
   sprintf(szTemp,"%ld 0 obj",Id());
#endif

   bytesWritten += ! sizeOnly ? (long)sprintf(pString + bytesWritten,szTemp) : (long)strlen(szTemp);

   if ( ! pStream ) 
      bytesWritten += PdfEntity::StringWrite(pString + bytesWritten,sizeOnly);

   if ( pDictionary )
      bytesWritten += pDictionary -> StringWrite(pString + bytesWritten,sizeOnly);

   if ( pStream ) 
      bytesWritten += pStream -> StringWrite(pString + bytesWritten,sizeOnly);

   sprintf(szTemp,"endobj%c",eol);

   bytesWritten += ! sizeOnly ? (long)sprintf(pString + bytesWritten,szTemp) : (long)strlen(szTemp);

   if ( postAmble ) {
      if ( ! sizeOnly )
         (BYTE *)memcpy((BYTE *)(pString + bytesWritten),postAmble,postAmbleSize);
      bytesWritten += postAmbleSize;
   }

   return bytesWritten;
   }


   long PdfObject::Write(FILE *fOutput,bool writeUncompressed) {

   if ( InObjectStream() )
      return 0;

   fileOffset = ftell(fOutput);

// Change made on 8/17/09: Should there be a cr-lf after obj ?

#ifdef CR_AFTER_OBJECT
#ifdef HARD_RETURN_AFTER_OBJECT
   fprintf(fOutput,"%ld 0 obj%c%c",Id(),0x0D,0x0A);
#else
   fprintf(fOutput,"%ld 0 obj%c",Id(),eol);
#endif
#else
   fprintf(fOutput,"%ld 0 obj",Id());
#endif

   if ( ! pStream ) 
      PdfEntity::Write(fOutput,writeUncompressed);

   if ( pDictionary ) {
      pDictionary -> Write(fOutput,writeUncompressed);
   }

   if ( pStream ) 
      pStream -> Write(fOutput,writeUncompressed);

   fprintf(fOutput,"endobj%c",eol);

   if ( postAmble )
      fwrite(postAmble,postAmbleSize,1,fOutput);

   long totalOutput = ftell(fOutput) - fileOffset;

   if ( -1L == reservedPrintSize )
      return totalOutput;

   if ( totalOutput > reservedPrintSize )
      return totalOutput;

   for ( long k = 0; k < reservedPrintSize - totalOutput; k++ )
      fprintf(fOutput," ");

   return reservedPrintSize + fprintf(fOutput,"%c",eol);
   }