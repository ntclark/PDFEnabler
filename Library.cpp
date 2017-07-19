
#pragma once

#define DEFINE_DATA

#include "Library.h"

   PdfUtility pdfUtility;

   PdfUtility::PdfUtility() {};

   BYTE *PdfUtility::ScanObject(EntityDelimiter ed,BYTE *pStart,BYTE *pEnd,long *foundLength) {

   char *pStartDelimiter = entityDelimiters[ed];
   long startLen = strlen(pStartDelimiter);
   char *pEndDelimiter = pStartDelimiter + startLen + 1;
   long endLen = strlen(pEndDelimiter);

   *foundLength = 0;

   BYTE *p = pStart;

   while ( p < pEnd && ( *p ? strncmp((char *)p,pStartDelimiter,startLen) : 1 ) )
      p++;

   if ( p == pEnd )
      return NULL;

   BYTE *pFoundStart = p;

   if ( '~' == pEndDelimiter[0] ) {

      p++;

//#define WHITE_SPACE "[] ()\t\x0A\x0D/"

      while ( p < pEnd && ! strchr(WHITE_SPACE,*p) && strncmp(entityDelimiters[edDictionary],(char *)p,2) )
         p++;

      if ( 0 == strncmp(entityDelimiters[edDictionary],(char *)p,2) )
         p--;

      if ( p != pEnd && strchr(WHITE_SPACE,*p) )
         p--;

   } else {

      if ( edObject == ed ) {

         while ( p < pEnd ) {

            if (  *p && 0 == strncmp((char *)p,pEndDelimiter,endLen) ) {
               break;
            }

            p++;

         }

      } else {

      long n = 0;

      p++;

      while ( p < pEnd && 
                  ( ( *p ? strncmp((char *)p,pEndDelimiter,endLen) : 1 ) || n ) ) {
         if ( *p && 0 == strncmp((char *)p,pStartDelimiter,startLen) ) {
            n++;
            p += endLen - 1;
         }
         if ( *p && 0 == strncmp((char *)p,pEndDelimiter,endLen) ) {
            n--;
            p += endLen - 1;
         }
         p++;
      }

      }

   }

   *foundLength = p + endLen - pFoundStart;

   return pFoundStart;
   }


   BYTE *PdfUtility::ScanDelimiter(EntityDelimiter ed,EntityScanDirection esd,EntityDelimiterEnd ede,BYTE *pStart,BYTE *pEnd) {

   char *pDelimiter = NULL;
   long len = 0;
   char *pOtherDelimiter = NULL;
   long otherLen = 0;
   if ( edeStart == ede ) {
      pDelimiter = entityDelimiters[ed];
      len = strlen(pDelimiter);
      pOtherDelimiter = pDelimiter + len + 1;
      otherLen = strlen(pOtherDelimiter);
   } else {
      pOtherDelimiter = entityDelimiters[ed];
      otherLen = strlen(pOtherDelimiter);
      pDelimiter = pOtherDelimiter + otherLen + 1;
      len = strlen(pDelimiter);
   }

   BYTE *p = pStart;

   switch ( esd ) {

   case esdScanDown:

      if ( '~' == pDelimiter[0] ) {

         while ( p < pEnd && ! strchr(WHITE_SPACE,*p) ) //'[' != *p && ' ' != *p && '\t' != *p && 0x0A != *p && 0x0D != *p && '/' != *p )
            p++;
         if ( p != pEnd && strchr(WHITE_SPACE,*p) )
            p--;

      } else {

         char *pDictionaryStart = entityDelimiters[edDictionary];
         long dictionaryStartLen = strlen(pDictionaryStart);
         char *pDictionaryEnd = entityDelimiters[edDictionary] + dictionaryStartLen + 1;
         long dictionaryEndLen = strlen(pDictionaryEnd);
         char *pArrayStart = entityDelimiters[edArray];
         char arrayStartLen = strlen(pArrayStart);
         char *pArrayEnd = entityDelimiters[edArray] + arrayStartLen + 1;
         long arrayEndLen = strlen(pArrayEnd);

         long nDictionary = 0;
         long nArray = 0;
         long nParenthesis = 0;

         while ( ( p < pEnd && ( *p ? strncmp((char *)p,pDelimiter,len) : 1 ) ) || nDictionary || nArray ) {

            if ( edElement == ed && 0 == strncmp((char *)p,pDictionaryStart,dictionaryStartLen) ) {

               nDictionary++;

               p += 2;
               continue;

            } else if ( edElement == ed && 0 == strncmp((char *)p,pDictionaryEnd,dictionaryEndLen) ) {

               nDictionary--;

               if ( 0 > nDictionary )
                  nDictionary = 0;

               p += 2;
               continue;

            } 

            if ( edElement == ed && '(' == *p ) {
               if ( '\\' != *(p - 1) )
                  nParenthesis++;
            }

            if ( edElement == ed && ')' == *p ) {
               if ( '\\' != *(p - 1) )
                  nParenthesis--;
               if ( 0 > nParenthesis )
                  nParenthesis = 0;
            }

            if ( ! nParenthesis ) {

               if ( edElement == ed && 0 == strncmp((char *)p,pArrayStart,arrayStartLen) ) {

                  nArray++;

               } else if ( edElement == ed && 0 == strncmp((char *)p,pArrayEnd,arrayEndLen) ) {

                  nArray--; 
                  if ( 0 > nArray ) 
                     nArray = 0;

               }

            }

            p++;

// 
// NTC: 09-02-2014: Found what looks like an unterminated array in "ColorSpace[/Indexed /DeviceRGB 255(-----------"
// There was no terminating "]" (?) - either a parsing error on entry here - or incorrect handling of /DeviceRGB specification (?!)
//
if ( p >= pEnd ) 
return NULL;

            if ( nParenthesis && ( *p ? 0 == strncmp((char *)p,pDelimiter,len) : 0 ) )
               p++;

         }

      }

      if ( p == pEnd )
         return NULL;
      if ( edeEnd == ede )
         return p + len;
      else
         return p;

   case esdScanUp:

      while ( 1 ) {
         while ( p > pEnd && strncmp((char *)p,pDelimiter,len) )
            p--;
         if ( p == pEnd && strncmp((char *)p,pDelimiter,len) )
            return NULL;
         if ( p == pEnd )
            return p;
         if ( otherLen && otherLen > len ) {
            p -= otherLen - len;
            if ( p < pEnd )
               return NULL;
            if ( strncmp((char *)p,pOtherDelimiter,otherLen) )
               return p + otherLen - len;
            p += otherLen - len - 1;
         } else if ( otherLen && len > otherLen ) {
            p -= len - otherLen;
            if ( p < pEnd )
               return NULL;
            if ( strncmp((char *)p,pOtherDelimiter,otherLen) )
               return p + len - otherLen;
            p += len - otherLen - 1;
         }
         else
            break;
      }
      return p;

   }

   return NULL;

   }


   long PdfUtility::HashCode(char *pszInput) {
   long hashCode = 0L;
   long part = 0L;
   long n = strlen(pszInput);
   char *psz = new char[n + 4];
   memset(psz,0,(n + 4) * sizeof(char));
   strcpy(psz,pszInput);
   char *p = psz;
   for ( long k = 0; k < n; k += 4 ) {
      memcpy(&part,p,4 * sizeof(char));
      hashCode ^= part;
      p += 4;
   }
   delete [] psz;
   return hashCode;
   }


   long PdfUtility::IntegerValue(char *pszValue) {

   char *pValue = new char[strlen(pszValue) + 1];

   memset(pValue,0,(strlen(pszValue) + 1) * sizeof(char));

   strcpy(pValue,pszValue);

   char *p = strtok(pValue," ");

   if ( ! p ) {
      delete [] pValue;
      return -1L;
   }

   long objectNumber = atol(p);

   p = strtok(NULL," ");
   
   if ( ! p ) {
      delete [] pValue;
      return objectNumber;
   }

   long generationNumber = atol(p);

   p = strtok(NULL," ");

   if ( ! p ) {
      delete [] pValue;
      return objectNumber;
   }

   if ( 'R' == *p ) {
      delete [] pValue;
      return objectNumber;
   }

   delete [] pValue;
   return objectNumber;
   }

   
   long PdfUtility::IntegerValueFromReferenceArray(char *pszValue,long oneBasedIndex) {

   char *pValue = new char[strlen(pszValue) + 1];

   memset(pValue,0,(strlen(pszValue) + 1) * sizeof(char));

   strcpy(pValue,pszValue);

   char *p = strtok(pValue,ARRAY_START_DELIMITERS);

   if ( ! p ) {
      delete [] pValue;
      return -1L;
   }

   for ( long k = 0; k < oneBasedIndex - 1; k++ ) {

      p = strtok(NULL,ARRAY_ITEM_DELIMITERS);
      if ( ! p ) {
         delete [] pValue;
         return -1L;
      }
      p = strtok(NULL,ARRAY_ITEM_DELIMITERS);
      if ( ! p ) {
         delete [] pValue;
         return -1L;
      }
      p = strtok(NULL,ARRAY_ITEM_DELIMITERS);
      if ( ! p ) {
         delete [] pValue;
         return -1L;
      }
   }

   if ( ! p ) {
      delete [] pValue;
      return -1L;
   }

   while ( ' ' == *p )
      p++;

   if ( ']' == *p ) {
      delete [] pValue;
      return -1L;
   }

   long objectNumber = atol(p);

   p = strtok(NULL,ARRAY_ITEM_DELIMITERS);
   
   if ( ! p ) {
      delete [] pValue;
      return objectNumber;
   }

   long generationNumber = atol(p);

   p = strtok(NULL,ARRAY_ITEM_DELIMITERS);

   if ( ! p ) {
      delete [] pValue;
      return objectNumber;
   }

   if ( 'R' == *p ) {
      delete [] pValue;
      return objectNumber;
   }

   delete [] pValue;
   return objectNumber;
   }


   char *PdfUtility::StringValueFromArray(char *pszValue,long oneBasedIndex) {

   static char szValue[4096];

   memset(szValue,0,sizeof(szValue));

   strcpy(szValue,pszValue);

   char *p = strtok(szValue,ARRAY_START_DELIMITERS);
   if ( ! p )
      return NULL;

   for ( long k = 0; k < oneBasedIndex - 1; k++ ) {
      p = strtok(NULL,ARRAY_ITEM_DELIMITERS);
      if ( ! p )
         return NULL;
   }

   if ( ! *p )
      return p;

   if ( ']' == p[strlen(p) - 1] )
      p[strlen(p) - 1] = '\0';

   return p;
   }

   long PdfUtility::IntegerValueFromArray(char *pszValue,long oneBasedIndex) {

   char *pValue = new char[strlen(pszValue) + 1];

   memset(pValue,0,(strlen(pszValue) + 1) * sizeof(char));

   strcpy(pValue,pszValue);

   char *p = strtok(pValue,ARRAY_START_DELIMITERS);
   if ( ! p ) {
      delete [] pValue;
      return -1;
   }

   for ( long k = 0; k < oneBasedIndex - 1; k++ ) {
      p = strtok(NULL,ARRAY_ITEM_DELIMITERS);
      if ( ! p ) {
         delete [] pValue;
         return -1;
      }
   }
   if ( ! p ) {
      delete [] pValue;
      return -1L;
   }

   long value = atol(p);

   delete [] pValue;

   return value;
   }


   long PdfUtility::ArraySize(char *pszValue) {

   char *pValue = new char[strlen(pszValue) + 1];

   memset(pValue,0,(strlen(pszValue) + 1) * sizeof(char));

   strcpy(pValue,pszValue);   

   char *p = strtok(pValue,ARRAY_START_DELIMITERS);

   if ( ! p ) {
      delete [] pValue;
      return 0;
   }

   long countItems = 0;

   while ( p ) {

      if ( ']' == *p )
         break;

      countItems++;

      p = strtok(NULL,ARRAY_ITEM_DELIMITERS);

      if ( ! p || ']' == *p )
         break;

   }

   delete [] pValue;

   return countItems;
   }


   void PdfUtility::ASCIIHexDecodeInPlace(char *pszInput) {

   long n = strlen(pszInput);

   long j = 0;

   for ( long k = 0; k < n; j++, k += 2 ) {

      char c1 = pszInput[k];
      char c2 = pszInput[k + 1];

      BYTE v1 = 0x00;

      if ( '0' <= c1 && '9' >= c1 )
         v1 = c1 - '0';
      else if ( 'A' <= c1 && 'F' >= c1 )
         v1 = c1 - 'A' + 10;
      else if ( 'a' <= c1 && 'f' >= c1 )
         v1 = c1 - 'a' + 10;

      BYTE v2 = 0x00;

      if ( '0' <= c2 && '9' >= c2 )
         v2 = c2 - '0';
      else if ( 'A' <= c2 && 'F' >= c2 )
         v2 = c2 - 'A' + 10;
      else if ( 'a' <= c2 && 'f' >= c2 )
         v2 = c2 - 'a' + 10;

      pszInput[j] = (v1 << 4) + v2;

   }   

   pszInput[j] = '\0';

   return;
   }


   void PdfUtility::ASCIIHexEncode(char *pszInput,long valueSize,char **ppszResult) {

   *ppszResult = new char[2 * valueSize + 1];
   memset(*ppszResult,0,(2 * valueSize + 1) * sizeof(char));

   char *p = pszInput;
   char *pEnd = p + valueSize;
   char *pTarget = *ppszResult;

   while ( p < pEnd ) {
  
      *pTarget = (*p & 0xF0) >> 4;
      *pTarget += (*pTarget > 9 ? 'a' - 10 : '0');

      pTarget++;

      *pTarget = (*p & 0x0F);
      *pTarget += (*pTarget > 9 ? 'a' - 10 : '0');
   
      pTarget++;

      p++;

   }

   return;
   }
   

   void PdfUtility::UnescapeInPlace(BYTE *pInput,long *pCntBytes) {

   long n = *pCntBytes;

   BYTE *newBytes = new BYTE[n];
/*
      Table 3 – Escape sequences in literal strings

      Sequence    Meaning
      \n          LINE FEED         (0Ah) (LF)
      \r          CARRIAGE RETURN   (0Dh) (CR)
      \t          HORIZONTAL TAB    (09h) (HT)
      \b          BACKSPACE         (08h) (BS)
      \f          FORM FEED         (FF)
      \(          LEFT PARENTHESIS  (28h)
      \)          RIGHT PARENTHESIS (29h)
      \\          REVERSE SOLIDUS   (5Ch) (Backslash)
      \ddd        Character code ddd (octal)

*/

   long j = 0;
   long k = 0;
   while ( j < n ) {
      if ( '\\' == pInput[j] ) {
         if ( ')' == pInput[j + 1] ) {
            j++;
            continue;
         }
         if ( '(' == pInput[j + 1] ) {
            j++;
            continue;
         }
      }
      newBytes[k] = pInput[j];
      k++;
      j++;
   }

   memset(pInput,0,k * sizeof(BYTE));
   memcpy(pInput,newBytes,k);

   delete [] newBytes;

   *pCntBytes = k;

   return;
   }

   
#if 0

wxPdfTokenizer::GetHex(int v)
{
  if (v >= '0' && v <= '9')
    return v - '0';
  if (v >= 'A' && v <= 'F')
    return v - 'A' + 10;
  if (v >= 'a' && v <= 'f')
    return v - 'a' + 10;
  return -1;
}


  wxMemoryInputStream in(*osIn);
  wxMemoryOutputStream* osOut = new wxMemoryOutputStream();
  size_t inLength = in.GetSize();
  size_t k;

  bool first = true;

  int n1 = 0;

  for (k = 0; k < inLength; ++k)
  {
    int ch = in.GetC() & 0xff;
    if (ch == '>')
      break;

    if (wxPdfTokenizer::IsWhitespace(ch))
      continue;

    int n = wxPdfTokenizer::GetHex(ch);

    if (first)
    {
      n1 = n;
    }
    else
    {
      osOut->PutC((char)(((n1 << 4) + n) & 0xff));
    }
    first = !first;
  }
  if (!first)
  {
    osOut->PutC((char)((n1 << 4) & 0xff));
  }
  osOut->Close();
  return osOut;

#endif

   bool PdfUtility::IsArray(char *pszReference) {
   return ! ( NULL == strchr(pszReference,'[') );
   }


   bool PdfUtility::IsIndirect(char *pszReference) {

   long n = strlen(pszReference);

   char *pszCopy = new char[n + 1];
   memset(pszCopy,0,(n + 1) * sizeof(char));

   strcpy(pszCopy,pszReference);

   char referenceId[3][8];
   memset(referenceId,0,sizeof(referenceId));

   char *p = pszCopy;
   char *pEnd = p + n;

   for ( long k = 0; k < 2; k++ ) {

      while ( p < pEnd && ! ( '0' <= *p && *p <= '9' ) ) {
         if ( ' ' != *p ) {
            delete [] pszCopy;
            return false;
         }
         p++;
      }

      if ( p == pEnd ) {
         delete [] pszCopy;
         return false;
      }

      while ( p < pEnd && '0' <= *p && *p <= '9' ) 
         p++;

      if ( p == pEnd ) {
         delete [] pszCopy;
         return false;
      }

   }

   while ( p < pEnd && ' ' == *p )
      p++;

   if ( p == pEnd ) {
      delete [] pszCopy;
      return false;
   }

   if ( p < pEnd && isalpha(*(p + 1)) ) {
      delete [] pszCopy;
      return false;
   }

   if ( 'R' != *p ) {
      delete [] pszCopy;
      return false;
   }

   delete [] pszCopy;

   return true;
   }


   BYTE *PdfUtility::addToArray(BYTE *additionalData,BYTE *pCurrentValue) {

   static BYTE szCurrentContents[1024];

   long n = strlen((char *)pCurrentValue);

   memset(szCurrentContents,0,sizeof(szCurrentContents));

   strcpy((char *)szCurrentContents,(char *)pCurrentValue);

   BYTE *pStart = szCurrentContents;
   
   while ( ' ' == *pStart )
      pStart++;
   
   BYTE *p = pStart + strlen((char *)pStart) - 1;

   while ( *p == 0x0A || *p == 0x0D || ' ' == *p )
      p--;

   p++;

   *p = '\0';

   p--;

   char *pszFinalContents = new char[n + 64];

   memset(pszFinalContents,0,(n + 64) * sizeof(char));

   if ( ']' == *p ) {
      *p = '\0';
      p--;
      while ( p > pStart && ' ' == *p )
         p--;
      p++;
      *p = '\0';
      p = pStart;
      while ( ' ' == *p )
         p++;
      if ( '[' == *p )
         p++;
      while ( ' ' == *p )
         p++;
      sprintf(pszFinalContents,"[ %s %s ]",p,(char *)additionalData);
   } else {
      sprintf(pszFinalContents,"[ %s %s ]",pStart,(char *)additionalData);
   }

   strcpy((char *)szCurrentContents,pszFinalContents);

   delete [] pszFinalContents;

   return szCurrentContents;
   }



   BYTE *PdfUtility::addValueToArray(long value,BYTE *pCurrentValue) {

   BYTE szValue[32];
   memset(szValue,0,sizeof(szValue));

   sprintf((char *)szValue,"%ld",value);

   return addToArray(szValue,pCurrentValue);
   }

   

   BYTE *PdfUtility::addReferenceToArray(long objectId,BYTE *pCurrentValue) {

   static BYTE szCurrentContents[1024];

   long n = strlen((char *)pCurrentValue);

   memset(szCurrentContents,0,sizeof(szCurrentContents));

   strcpy((char *)szCurrentContents,(char *)pCurrentValue);

   BYTE *pStart = szCurrentContents;
   
   while ( ' ' == *pStart )
      pStart++;
   
   BYTE *p = pStart + strlen((char *)pStart) - 1;

   while ( *p == 0x0A || *p == 0x0D || ' ' == *p )
      p--;

   p++;

   *p = '\0';

   p--;

   char *pszFinalContents = new char[n + 64];

   memset(pszFinalContents,0,(n + 64) * sizeof(char));

   if ( ']' == *p ) {
      *p = '\0';
      p--;
      while ( p > pStart && ' ' == *p )
         p--;
      p++;
      *p = '\0';
      p = pStart;
      while ( ' ' == *p )
         p++;
      if ( '[' == *p )
         p++;
      while ( ' ' == *p )
         p++;
      sprintf(pszFinalContents,"[ %s %ld 0 R ]",p,objectId);
   } else {
      sprintf(pszFinalContents,"[ %s %ld 0 R ]",pStart,objectId);
   }

   strcpy((char *)szCurrentContents,pszFinalContents);

   delete [] pszFinalContents;

   return szCurrentContents;
   }