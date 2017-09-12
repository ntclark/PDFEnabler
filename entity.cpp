
#include "Library.h"

#include "entity.h"

   PdfEntity::PdfEntity() :

      pData(NULL),
      dataSize(0),
      offset(0)

   {
   }


   PdfEntity::PdfEntity(BYTE *pSpace,long size) :

      pData(NULL),
      dataSize(size),
      offset(0)

   {
   pData = new BYTE[dataSize + 1];
   memset(pData,0,(dataSize + 1) * sizeof(BYTE));
   memcpy(pData,pSpace,dataSize);
   return;
   }


   PdfEntity::PdfEntity(EntityDelimiter ed,EntityScanDirection scanDirection,BYTE *pStart,BYTE *pLast) :

      pData(NULL),
      dataSize(0),
      offset(0)

   {

   switch ( scanDirection ) {

   case esdScanUp: {

      BYTE *pEnd = pdfUtility.ScanDelimiter(ed,esdScanUp,edeStart,pStart,pLast);

      if ( ! pEnd )
         return;

      dataSize = (long)(pStart - pEnd);
   
      pData = new BYTE[dataSize + 1];
      memset(pData,0,(dataSize + 1) * sizeof(BYTE));

      memcpy(pData,pEnd,dataSize);

      offset = (long)(pEnd - pLast);

      }
      break;

   case esdScanDown: {

      BYTE *pTop = pdfUtility.ScanObject(ed,pStart,pLast,&dataSize);

      if ( ! pTop )
         return;

      pData = new BYTE[dataSize + 1];
      pData[dataSize] = '\0';
      memcpy(pData,pTop,dataSize);

      offset = (long)(pTop - pStart);

      }
      break;

   }

   }


   PdfEntity::PdfEntity(FILE *fInput) :

      pData(NULL),
      dataSize(0),
      offset(0)

   {

   if ( ! fInput ) 
      return;

   takeFile(fInput);

   return;
   }


   void PdfEntity::takeFile(FILE *fInput) {
   fseek(fInput,0,SEEK_END);
   dataSize = ftell(fInput);
   pData = new BYTE[dataSize + 16];
   memset(pData + dataSize,0,16 * sizeof(BYTE));
   fseek(fInput,0,SEEK_SET);
   fread(pData,dataSize,1,fInput);
   offset = 0;
   return;
   }


   PdfEntity::~PdfEntity() {
   if ( pData )
      delete [] pData;
   }


   BYTE *PdfEntity::ReallocateStorage(BYTE *pFrom,long newSize,long newOffset) {

   if ( pData )
      delete [] pData;

   if ( ! pFrom ) {
      dataSize = 0;
      pData = NULL;
      offset = newOffset;
      return pData;
   }

   dataSize = newSize;
   pData = new BYTE[dataSize + 1];
   pData[dataSize] = '\0';
   memcpy(pData,pFrom,dataSize);

   offset = newOffset;

   return pData;
   }


   long PdfEntity::StringWrite(char *pString,bool sizeOnly) {

   if ( ! pData )
      return 0L;

   long printSize = dataSize;

   char *p = (char *)pData + printSize - 1;

   while ( p >= (char *)pData && ( ' ' == *p || 0x0A == *p || 0x0D == *p ) ) {
      printSize--;
      p--;
   }

   if ( sizeOnly )
      return printSize;

   (BYTE *)memcpy((BYTE *)pString,(BYTE *)pData,printSize);
   return printSize;
   }

   long PdfEntity::Write(FILE *fOutput,bool writeUncompressed) {
//
// Change made on 1-26-10: Should a CR be written if the entity is blank
// Double check this.
//
#if 0
   if ( ! pData )
      return fprintf(fOutput,"\n");
#else
   if ( ! pData )
      return 0;
#endif

#if 1

   long printSize = dataSize;

   char *p = (char *)pData + printSize - 1;

   while ( p >= (char *)pData && ( ' ' == *p || 0x0A == *p || 0x0D == *p ) ) {
      printSize--;
      p--;
   }

// Change made on 1-26-10: Should a CR be written if the entity is blank
// Double check this.
#if 1
   if ( 0 == printSize )
      return 0;
#endif

   return (long)fwrite(pData,printSize,1,fOutput) + (long)fprintf(fOutput,"%c",eol);

#else

   return fwrite(pData,dataSize,1,fOutput);

#endif
   }