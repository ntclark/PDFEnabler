
#include "Library.h"

#include "Stream.h"
#include "Object.h"
#include "XReference.h"

   PdfStream::PdfStream(PdfObject *po,PdfDictionary *pd,BYTE *pTop,BYTE *pBottom) :

      PdfEntity(edStream,esdScanDown,pTop,pBottom),

      pObject(po),
      pDictionary(pd),

      isUncompressed(false),
      isDecrypted(false),

      rawInputLength(0L),
      rawInput(NULL)

   {

   BYTE *pStart = Storage();

   if ( ! pStart )
      return;

   if ( strncmp((char *)pStart,"stream",6) )
      return;

   pStart += 6;

   while ( 0x0A == *pStart || 0x0D == *pStart )
      pStart++;

   BYTE *pEnd = Storage() + BinaryDataSize();

   while ( strncmp((char *)pEnd,"endstream",9) && pEnd > pStart )
      pEnd--;

   pEnd--;

   while ( pEnd > pStart && ( 0x0A == *pEnd || 0x0D == *pEnd ) ) 
      pEnd--;

   pEnd++;

   long newSize = (long)(pEnd - pStart);

   if ( ! newSize )
      return;

   BYTE *pNew = new BYTE[newSize + 1];

   pNew[newSize] = '\0';

   memcpy(pNew,pStart,newSize);

   ReallocateStorage(pNew,newSize,0);

   delete [] pNew;

   return;
   }


   PdfStream::~PdfStream() {
   if ( rawInput )
      delete [] rawInput;
   return;
   }


   BYTE *PdfStream::ReallocateStorage(BYTE *pNewData,long dataSize,long offset) {
   PdfEntity::ReallocateStorage(pNewData,dataSize,offset);
   if ( pDictionary )
      pDictionary -> SetLength(BinaryDataSize());
   return Storage();
   }


   void PdfStream::Decrypt() {
   if ( isDecrypted )
      return;
   isDecrypted = true;
   pObject -> Decrypt(Storage(),BinaryDataSize());
   return;
   }


   void PdfStream::Encrypt() {
   if ( ! isDecrypted )
      return;
   isDecrypted = false;
   pObject -> Encrypt(Storage(),BinaryDataSize());
   return;
   }


   void PdfStream::Decompress() {

   if ( isUncompressed )
      return;

   BYTE *pValue = pDictionary -> Value("Subtype");

   if ( pValue && 0 == strncmp((char *)pValue,"/Image",6) )
      return;

   isUncompressed = true;

   BYTE *pszDecoder = pDictionary -> Value("Filter");

   if ( ! pszDecoder ) 
      return;

   if ( strncmp((char *)pszDecoder,"/FlateDecode",12) )
      return;

#ifdef USE_INPUT_COMPRESSION
   if ( rawInput )
      delete [] rawInput;

   rawInputLength = BinaryDataSize();

   rawInput = new BYTE[rawInputLength];

   memcpy(rawInput,Storage(),rawInputLength);
#endif

   inflate();

#ifdef REMOVE_COMPRESSION
   pDictionary -> RemoveValue("Filter");
#endif

   return;
   }


   void PdfStream::Compress(BYTE *pNewData,long dataSize) {
   isUncompressed = true;
   ReallocateStorage(pNewData,dataSize,0);
   Compress();
   return;
   }


   void PdfStream::Compress() {

   if ( ! isUncompressed )
      return;

   isUncompressed = false;

   BYTE *pszDecoder = pDictionary -> Value("Filter");

   if ( ! pszDecoder ) 
      return;

   if ( strncmp((char *)pszDecoder,"/FlateDecode",12) )
      return;

   deflate();

   return;
   }


   long PdfStream::deflate() {

   long predictor,columns,bitsPerComponent,componentsPerSample;
   long defaults[] = {1,1,8,1,0};
   long *targets[] = {&predictor,&columns,&bitsPerComponent,&componentsPerSample};
   char pszParms[][32] = {"Predictor","Columns","BitsPerComponent","Colors",""};

   BYTE *pValue = NULL;

   char szTemp[128];

   for ( long k = 0; 1; k++ ) {

      if ( ! pszParms[k][0] )
         break;

      *targets[k] = defaults[k];

      sprintf(szTemp,"DecodeParms:%s",pszParms[k]);
      pValue = pDictionary -> Value(szTemp);

      if ( ! pValue )
         pValue = pDictionary -> Value(pszParms[k]);

      if ( ! pValue ) 
         continue;

      *targets[k] = pdfUtility.IntegerValue((char *)pValue);

   }

   if ( 1L == predictor ) {

      BYTE *pResult;

      long countBytes = pdfUtility.deflate(Storage(),BinaryDataSize(),&pResult);

#ifdef USE_INPUT_COMPRESSION
      ReallocateStorage(rawInput,rawInputLength,0);
#else
      ReallocateStorage(pResult,countBytes,0);
#endif

      delete [] pResult;

      return countBytes;
   }

   BYTE *pResult;

   long countBytes = pdfUtility.deflate(Storage(),BinaryDataSize(),&pResult,predictor,columns,bitsPerComponent,componentsPerSample);

#ifdef USE_INPUT_COMPRESSION
   ReallocateStorage(rawInput,rawInputLength,0);
#else
   ReallocateStorage(pResult,countBytes,0);
#endif

   delete [] pResult;

   return countBytes;
   }


   long PdfStream::inflate() {

   long predictor,columns,bitsPerComponent,componentsPerSample;
   long defaults[] = {1,1,8,1,0};
   long *targets[] = {&predictor,&columns,&bitsPerComponent,&componentsPerSample};
   char pszParms[][32] = {"Predictor","Columns","BitsPerComponent","Colors",""};
   char szTemp[128];

   BYTE *pValue = NULL;

   for ( long k = 0; 1; k++ ) {

      if ( ! pszParms[k][0] )
         break;

      *targets[k] = defaults[k];

      sprintf(szTemp,"DecodeParms:%s",pszParms[k]);
      pValue = pDictionary -> Value(szTemp);
      if ( ! pValue ) {
         pValue = pDictionary -> Value(pszParms[k]);
      }

      if ( ! pValue ) 
         continue;

      *targets[k] = pdfUtility.IntegerValue((char *)pValue);

   }

   BYTE *pResult = NULL;

   long currentBytes = BinaryDataSize();
   long divisor = bitsPerComponent * componentsPerSample * columns / 8;
   long rows = currentBytes / divisor;
   if ( rows * divisor < currentBytes )
      rows++;

   long countBytes = pdfUtility.inflate(Storage(),currentBytes,&pResult,predictor,columns,rows,bitsPerComponent,componentsPerSample);

   ReallocateStorage(pResult,countBytes,0);   

#ifdef REMOVE_PREDICTOR
   pDictionary -> RemoveValue("DecodeParms:Predictor");
#endif

   return countBytes;
   }


   long PdfStream::StringWrite(char *pString,bool sizeOnly) {

   char szTemp[32];
   sprintf(szTemp,"stream%c%c",0x0D,0x0A);

   long bytesWritten = ! sizeOnly ? (long)sprintf(pString,szTemp) : (long)strlen(szTemp);

   if ( ! sizeOnly )
      (BYTE *)memcpy((BYTE *)(pString + bytesWritten),Storage(),BinaryDataSize());

   bytesWritten += BinaryDataSize();

   sprintf(szTemp,"%c\nendstream%c",eol,eol);

   bytesWritten += ! sizeOnly ? (long)sprintf(pString + bytesWritten,szTemp) : (long)strlen(szTemp);

   return bytesWritten;
   }


   long PdfStream::Write(FILE *fOutput,bool writeUncompressed) {
   long bytesWritten = (long)fprintf(fOutput,"stream%c%c",0x0D,0x0A);
   if ( writeUncompressed) {
      Decompress();
   }
#ifdef USE_INPUT_COMPRESSION
   if ( rawInput )
      bytesWritten += fwrite(rawInput,rawInputLength,1,fOutput);
   else
      bytesWritten += fwrite(Storage(),BinaryDataSize(),1,fOutput);
#else
   bytesWritten += (long)fwrite(Storage(),BinaryDataSize(),1,fOutput);
#endif
   return bytesWritten + (long)fprintf(fOutput,"%c\nendstream%c",eol,eol);
   }


   long PdfStream::WriteHex(FILE *fOutput) {

   char szHex[] = {"0123456789abcdef"};

   long outputCount = 0;

   BYTE *pInput = Storage();

   long n = BinaryDataSize();

   for ( long k = 0; k < n; k++ ) {

#ifdef REMOVE_COMPRESSION
      fprintf(fOutput,"%c",pInput[k]);
#else
      long j = (pInput[k] & 0xF0) >> 4;
      fprintf(fOutput,"%c",szHex[j]);
      j = (pInput[k] & 0x0F);
      fprintf(fOutput,"%c",szHex[j]);
#endif

      outputCount++;

      if ( 80 == outputCount ) {
         fprintf(fOutput,"\n");
         outputCount = 0;
      }

   }

   fprintf(fOutput,"\n");

   return 0;
   }