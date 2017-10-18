// Copyright 2017 InnoVisioNate Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Library.h"

#include "element.h"
#include "Object.h"
#include "Dictionary.h"


   PdfElement::PdfElement(PdfObject *po,PdfDictionary *pd,BYTE *pStart,BYTE *pLast) :

      PdfEntity(edElement,esdScanDown,pStart,pLast),

      pValue(NULL),
      valueSize(-1L),
      vt(vtUnspecified),
      vst(vstUnspecified),
      pDictionary(NULL),
      pParentObject(po),
      pParentDictionary(pd)

   {

   memset(szName,0,sizeof(szName));

   if ( ! BinaryDataSize() )
      return;

   BYTE *pName = Storage() + strlen(entityDelimiters[edElement]);

   strncpy(szName,(char *)pName,sizeof(szName) - 1);

   BYTE *p = (BYTE *)(szName + strlen(szName) - 1);

   if ( *p == entityDelimiters[edElement][0] ) {
      *p = '\0';
      p--;
   }

   while ( ' ' == *p )
      p--;
   *(p + 1) = '\0';

   BYTE *pNext = pdfUtility.ScanDelimiter(edElement,esdScanDown,edeStart,pStart + Offset() + 1,pLast);

   if ( ! pNext )
      pNext = pLast;
   
   p = pStart + Offset() + strlen((char *)szName) + 1;
   while ( ' ' == *p || '\t' == *p || 0x0A == *p || 0x0D == *p )
      p++;

   if ( p == pNext ) {

      BYTE *pAfterValue = pdfUtility.ScanDelimiter(edElement,esdScanDown,edeStart,pNext + 1,pLast);

      if ( pAfterValue )
         valueSize = (long)(pAfterValue - pNext - 1);
      else {
         BYTE *pTest = p + 1;
         valueSize = 0;
         while ( pTest < pLast ) {
            if ( ' ' == *pTest || '\t' == *pTest || 0x0A == *pTest || 0x0D == *p )
               break;
            bool isDelimiter = false;
            for ( long k = 0; k < edCount; k++ ) {
               if ( ! strncmp((char *)pTest,entityDelimiters[k],strlen(entityDelimiters[k])) ) {
                  isDelimiter = true;
                  break;
               }
            }
            if ( isDelimiter )
               break;
            valueSize++;
            pTest++;
         }
      }

      long extendBytes = (long)(p - (pStart + Offset() + (long)strlen(szName)) + valueSize);

      ReallocateStorage(pStart + Offset(),BinaryDataSize() + extendBytes,Offset());

      pValue = Storage() + strlen(szName) + strlen(entityDelimiters[edElement]);

   } else {

      ReallocateStorage(pStart + Offset(),(long)(pNext - (pStart + Offset())),Offset());

      pValue = Storage() + strlen(entityDelimiters[edElement]) + strlen(szName);

   } 

   long whiteSpace = 0;
   while ( ' ' == *pValue || '\t' == *pValue || 0x0A == *pValue || 0x0D == *pValue ) {
      pValue++;
      whiteSpace++;
   }

   valueSize = (long)((BYTE *)(Storage() + BinaryDataSize()) - pValue);

   BYTE *pEnd = pValue + valueSize - 1;
   while ( pEnd > pValue && ( 0x0A == *pEnd || 0x0D == *pEnd ) )
      pEnd--;

   valueSize = (long)(pEnd - pValue + 1);

   /*
      A string object shall consist of a series of zero or more bytes. 
      String objects are not integer objects, but are stored in a more 
      compact format. The length of a string may be subject to implementation 
      limits; see Annex C.

      String objects shall be written in one of the following two ways:

         •  As a sequence of literal characters enclosed in parentheses ( ) 
            (using LEFT PARENTHESIS (28h) and RIGHT PARENTHESIS (29h)); 
            see 7.3.4.2, "Literal Strings."

         •  As hexadecimal data enclosed in angle brackets < > (using LESS-THAN SIGN 
            (3Ch) and GREATER-THAN SIGN (3Eh)); 
            see 7.3.4.3, "Hexadecimal Strings."
   */

   if ( 0 == strncmp(entityDelimiters[edString],(char *)pValue,strlen(entityDelimiters[edString])) ) {

      vt = vtString;

      while ( ')' != pValue[valueSize] && valueSize > 0 )
         valueSize--;

      if ( valueSize > 0 )
         pValue[valueSize] = '\0';

      pValue++;

      if ( 0xFE == pValue[0] && (valueSize > 1 && 0xFF == pValue[1]) )
         vst = vstUnicode;

      pdfUtility.UnescapeInPlace(pValue,&valueSize);

   } else if ( 0 == strncmp(entityDelimiters[edHexEncodedString],(char *)pValue,strlen(entityDelimiters[edHexEncodedString])) && '<' != pValue[1] ) {

      vt = vtHexEncodedString;

      while ( '>' != pValue[valueSize] && valueSize > 0 )
         valueSize--;

      if ( valueSize > 0 )
         pValue[valueSize] = '\0';

      valueSize--;

      pValue++;

      valueSize /= 2;

      pdfUtility.ASCIIHexDecodeInPlace((char *)pValue);

   } else if ( 0 == strncmp(entityDelimiters[edArray],(char *)pValue,strlen(entityDelimiters[edArray])) ) {

      vt = vtArray;

   } else if ( 0 == strncmp(entityDelimiters[edDictionary],(char *)pValue,strlen(entityDelimiters[edDictionary])) ) {

      vt = vtDictionary;

      BYTE *pEnd = pValue + valueSize;

      while ( pEnd > pValue && ( 0x0A == *pEnd || 0x0D == *pEnd ) )
         pEnd--;

      valueSize = (long)(pEnd - pValue + 1);

      pDictionary = new PdfDictionary(pParentDictionary -> Object(),pValue,valueSize);

   }
   
   return;
   }


   PdfElement::~PdfElement() {
   if ( pDictionary )
      delete pDictionary;
   return;
   }


   void PdfElement::Encrypt() {
   switch ( vt ) {
   case vtString:
      pParentObject -> Encrypt(pValue,valueSize);
      break;
   case vtDictionary:
      pDictionary -> Encrypt();
   default:
      break;
   }
   return;
   }

   
   void PdfElement::Decrypt() {
   switch ( vt ) {
   case vtString:
      pParentObject -> Decrypt(pValue,valueSize);
      break;
   case vtDictionary:
      pDictionary -> Decrypt();
   default:
      break;
   }
   return;
   }

   
   long PdfElement::StringWrite(char *pString,bool sizeOnly) {

   if ( ! Storage() )
      return 0L;

   long bytesWritten = 0;

   switch ( vt ) {

   case vtString:

      bytesWritten += ! sizeOnly ? (long)sprintf(pString + bytesWritten,"/") : 1;
      bytesWritten += ! sizeOnly ? (long)sprintf(pString + bytesWritten,szName) : (long)strlen(szName);
      bytesWritten += ! sizeOnly ? (long)sprintf(pString + bytesWritten,entityDelimiters[edString]) : (long)strlen(entityDelimiters[edString]);
      if ( vstUnicode == vst ) {
         if ( ! sizeOnly )
            (BYTE *)memcpy((BYTE *)(pString + bytesWritten),pValue,valueSize);
         bytesWritten += valueSize;
      } else
         bytesWritten += ! sizeOnly ? (long)sprintf(pString + bytesWritten,(char *)pValue) : (long)strlen((char *)pValue);

      bytesWritten += ! sizeOnly ? (long)sprintf(pString + bytesWritten,entityDelimiters[edString] + (long)strlen(entityDelimiters[edString]) + 1) : (long)strlen(entityDelimiters[edString] + (long)strlen(entityDelimiters[edString]) + 1);

      break;

   case vtHexEncodedString:

      char *pszEncoded;

      bytesWritten += ! sizeOnly ? (long)sprintf(pString + bytesWritten,"/") : 1;
      bytesWritten += ! sizeOnly ? (long)sprintf(pString + bytesWritten,szName) : (long)strlen(szName);
      bytesWritten += ! sizeOnly ? (long)sprintf(pString + bytesWritten,entityDelimiters[edHexEncodedString]) : (long)strlen(entityDelimiters[edHexEncodedString]);

      pdfUtility.ASCIIHexEncode((char *)pValue,valueSize,&pszEncoded);

      bytesWritten += ! sizeOnly ? (long)sprintf(pString + bytesWritten,pszEncoded) : (long)strlen(pszEncoded);

      delete [] pszEncoded;

      bytesWritten += ! sizeOnly ? (long)sprintf(pString + bytesWritten,entityDelimiters[edHexEncodedString] + (long)strlen(entityDelimiters[edHexEncodedString]) + 1) : 
                           (long)strlen(entityDelimiters[edHexEncodedString] + (long)strlen(entityDelimiters[edHexEncodedString]) + 1);
      break;

   case vtDictionary:

      bytesWritten += ! sizeOnly ? (long)sprintf(pString + bytesWritten,"/") : 1;
      bytesWritten += ! sizeOnly ? (long)sprintf(pString + bytesWritten,szName) : (long)strlen(szName);
      bytesWritten += pDictionary -> StringWrite(pString + bytesWritten,sizeOnly);
      break;

   default:

      if ( ! sizeOnly )
         (BYTE *)memcpy((BYTE *)(pString + bytesWritten),Storage(),BinaryDataSize());

      bytesWritten += BinaryDataSize();

   }

   return bytesWritten;
   }


   long PdfElement::Write(FILE *fOutput,bool writeUncompressed) {

   long bytesWritten = 0;

   switch ( vt ) {

   case vtString:

      bytesWritten += (long)fwrite("/",1,1,fOutput);
      bytesWritten += (long)fwrite(szName,1,strlen(szName),fOutput);
      bytesWritten += (long)fwrite(entityDelimiters[edString],1,strlen(entityDelimiters[edString]),fOutput);
      if ( vstUnicode == vst )
         bytesWritten += (long)fwrite(pValue,valueSize,1,fOutput);
      else
         bytesWritten += (long)fprintf(fOutput,"%s",pValue);
      bytesWritten += (long)fwrite(entityDelimiters[edString] + strlen(entityDelimiters[edString]) + 1,1,
                                       strlen(entityDelimiters[edString] + strlen(entityDelimiters[edString]) + 1),fOutput);
      break;

   case vtHexEncodedString:

      char *pszEncoded;

      bytesWritten += (long)fwrite("/",1,1,fOutput);
      bytesWritten += (long)fwrite(szName,1,strlen(szName),fOutput);
      bytesWritten += (long)fwrite(entityDelimiters[edHexEncodedString],1,strlen(entityDelimiters[edHexEncodedString]),fOutput);

      pdfUtility.ASCIIHexEncode((char *)pValue,valueSize,&pszEncoded);

      bytesWritten += (long)fprintf(fOutput,"%s",pszEncoded);

      delete [] pszEncoded;

      bytesWritten += (long)fwrite(entityDelimiters[edHexEncodedString] + strlen(entityDelimiters[edHexEncodedString]) + 1,1,
                                 strlen(entityDelimiters[edHexEncodedString] + strlen(entityDelimiters[edHexEncodedString]) + 1),fOutput);
      break;

   case vtDictionary:

#ifdef REMOVE_PERMS

      if ( 0 == strncmp(szName,"Perms",5) && 5 == strlen(szName) )
         return 0;

#endif

      bytesWritten += (long)fwrite("/",1,1,fOutput);
      bytesWritten += (long)fwrite(szName,1,strlen(szName),fOutput);
      bytesWritten += pDictionary -> Write(fOutput);
      break;

   default:

#if 1

#ifdef REMOVE_ACROFORM

      if ( 0 == strncmp(szName,"AcroForm",8) && 8 == strlen(szName) )
         return 0;

#endif

#ifdef REMOVE_STRUCT_TREE_ROOT
   
      if ( 0 == strncmp(szName,"StructTreeRoot",14) && 14 == strlen(szName) )
         return 0;

#endif

#ifdef REMOVE_NAMES

      if ( 0 == strcmp(szName,"Names") && 5 == strlen(szName) )
         return 0;

#endif

#ifdef REMOVE_METADATA

      if ( 0 == strcmp(szName,"Metadata") && 8 == strlen(szName) )
         return 0;

#endif

      return (long)fwrite(Storage(),1,BinaryDataSize(),fOutput);

#else
      bytesWritten += fwrite("/",1,1,fOutput);
      bytesWritten += fwrite(szName,1,strlen(szName),fOutput);
      bytesWritten += fwrite(pValue,1,valueSize,fOutput);
#endif
   }

   return bytesWritten;
   }