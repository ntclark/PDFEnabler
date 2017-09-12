#include "Library.h"

#include "NumberTree.h"
#include "Document.h"

static char romanNumerals[][8] = {"","i","ii","iii","iv","v","vi","vii","viii","ix","x","xi","xii","xiii","xiv","xv","xvi","xvii","xviii","xiix","xix","xx",0};
static char RomanNumerals[][8] = {"","I","II","III","IV","V","VI","VII","VIII","IX","X","XI","XII","XIII","XIV","XV","XVI","XVII","XVIII","XIIX","XIX","XX",0};


   PdfNumberTree::PdfNumberTree(PdfDictionary *pp) :
      pParent(pp)
   {

   BYTE *pEntry = pParent -> DeReferencedValue("Nums");

   if ( ! pEntry )
      pEntry = pParent -> DeReferencedValue("Kids");

   if ( ! pEntry )
      return;

   long n = 4 * (long)strlen((char *)pEntry) + 1;

   char *pTreeEntries = new char[n];

   memset(pTreeEntries,0,n * sizeof(char));

   char *pTarget = pTreeEntries;
   char *p = (char *)pEntry;
   char *pEnd = (char *)pEntry + strlen((char *)pEntry);
   n = 0;
   while ( p < pEnd ) {
      if ( '<' == *p && '<' == *(p + 1) && ( p > (char *)pEntry && ' ' != *(p - 1) ) ) {
         *pTarget = ' ';
         pTarget++;
         n++;
      } else if ( '>' == *p && '>' == *(p + 1) && ( p < pEnd - 2 && ' ' != *(p + 2) ) ) {
         *pTarget = *p;
         pTarget++;
         p++;
         *pTarget = *p;
         pTarget++;
         p++;
         *pTarget = ' ';
         pTarget++;
      }
      *pTarget = *p;
      pTarget++;
      p++;
      n++;
   }

   pEnd = pTreeEntries + strlen((char *)pTreeEntries);

   std::list<PdfDictionary *> rangeDictionaries;
   std::list<long> rangeIndexes;

   p = pTreeEntries;
   while ( p < pEnd && ! ( '0' <= *p && *p <= '9' ) ) 
      p++;

   // *p is a digit

   do {

      long entryIndex = atol(p);

      while ( p < pEnd && '0' <= *p && *p <= '9' ) 
         p++;

      // *p is not a digit

      while ( p < pEnd && ( '\t' == *p || ' ' == *p || 0x0A == *p || 0x0D == *p ) ) 
         p++;

      // *p is the next token

      PdfDictionary *pRangeDictionary = NULL;

      if ( 0 == strncmp(p,entityDelimiters[edDictionary],strlen(entityDelimiters[edDictionary])) ) {

         pRangeDictionary = new PdfDictionary(pParent -> Object(),(BYTE *)p,(BYTE *)pEnd);

         additionalDictionaries.insert(additionalDictionaries.end(),pRangeDictionary);

         p += pRangeDictionary -> BinaryDataSize();
   
         while ( p < pEnd && ( '\t' == *p || ' ' == *p || 0x0A == *p || 0x0D == *p ) ) 
            p++;

      } else {

         char referenceId[3][8];

         memset(referenceId,0,sizeof(referenceId));

         for ( long k = 0; k < 8; k++, p++ ) {
            if ( p == pEnd || '\t' == *p || ' ' == *p || 0x0A == *p || 0x0D == *p || ']' == *p ) 
               break;
            referenceId[0][k] = *p;
         }

         while ( p < pEnd && ( '\t' == *p || ' ' == *p || 0x0A == *p || 0x0D == *p ) ) 
            p++;

         // *p is the next token

         if ( p < pEnd ) {

            for ( long k = 0; k < 8; k++, p++ ) {
               if ( p == pEnd || '\t' == *p || ' ' == *p || 0x0A == *p || 0x0D == *p || ']' == *p ) 
                  break;
               referenceId[1][k] = *p;
            }

            while ( p < pEnd && ( '\t' == *p || ' ' == *p || 0x0A == *p || 0x0D == *p ) ) 
               p++;

            // *p is the next token

            if ( p < pEnd ) {

               for ( long k = 0; k < 8; k++, p++ ) {
                  if ( p == pEnd || '\t' == *p || ' ' == *p || 0x0A == *p || 0x0D == *p || ']' == *p ) 
                     break;
                  referenceId[2][k] = *p;
               }

            }

         }

         if ( 'R' == referenceId[2][0] && '\0' == referenceId[2][1] ) {

            std::map<long,PdfObject *> *pObjects = pParent -> Object() -> Document() -> ObjectMap();

            PdfObject *pReferencedObject = (*pObjects)[atol(referenceId[0])];

            if ( pReferencedObject )
               pRangeDictionary = pReferencedObject -> Dictionary();
      
         }

         if ( p < pEnd ) {
            while ( p < pEnd && ( '\t' == *p || ' ' == *p || 0x0A == *p || 0x0D == *p || ']' == *p ) ) 
               p++;
         }

      }

      if ( ! pRangeDictionary )
         break;

      rangeDictionaries.insert(rangeDictionaries.end(),pRangeDictionary);

      rangeIndexes.insert(rangeIndexes.end(),entryIndex);

   } while ( p && p < pEnd && ']' != *p);   

   std::list<long>::iterator iIndexes = rangeIndexes.begin();

   long maxIndex = (long)rangeDictionaries.size() - 1;
   long thisIndex = 0;

   for ( std::list<PdfDictionary *>::iterator it = rangeDictionaries.begin();
            it != rangeDictionaries.end(); it++, iIndexes++, thisIndex++ ) {

      long index = (*iIndexes);
      PdfDictionary *pRangeDictionary = (*it);

      long nextIndex = 0L;
      if ( thisIndex < maxIndex ) {
         iIndexes++;
//iIndexes++ != rangeIndexes.end() ) 
         nextIndex = (*iIndexes);
         --iIndexes;
      }

      long countEntries = 1;
      if ( nextIndex )
         countEntries = nextIndex - index;

      BYTE *pPrefixValue = pRangeDictionary -> Value("P");
      BYTE *pStyleValue = pRangeDictionary -> Value("S");
      BYTE *pStartValue = pRangeDictionary -> Value("St");

      long pageNumber = 1;

      if ( pStartValue ) 
         pageNumber = atol((char *)pStartValue);

      if ( pStyleValue && '/' == *pStyleValue )
         pStyleValue++;

      for ( long k = 0; k < countEntries; k++ ) {

         char szEntry[128];

         memset(szEntry,0,sizeof(szEntry));

         if ( pPrefixValue ) 
            sprintf(szEntry,"%s",(char *)pPrefixValue);

         if ( ! pPrefixValue || ( pPrefixValue && strncmp((char *)pPrefixValue,"Contents",8) ) ) {

            if ( pStyleValue ) {
               switch ( *pStyleValue ) {
               case 'D':
                  sprintf(szEntry + strlen(szEntry),"%ld",pageNumber);
                  break;
               case 'r':
                  sprintf(szEntry + strlen(szEntry),"%s",romanNumerals[pageNumber]);
                  break;
               case 'R': 
                  sprintf(szEntry + strlen(szEntry),"%s",RomanNumerals[min(pageNumber,20)]);
                  break;
               case 'a':
                  sprintf(szEntry + strlen(szEntry),"%c",'a' + pageNumber - 1);
                  break;
               case 'A':
                  sprintf(szEntry + strlen(szEntry),"%c",'A' + pageNumber - 1);
                  break;
               }

            }

         }

         char *pszEntry = new char[strlen(szEntry) + 1];
         strcpy(pszEntry,szEntry);

         insert(end(),pszEntry);

         pageNumber++;

      }

   }

   delete [] pTreeEntries;

   return;
   }


   PdfNumberTree::~PdfNumberTree() {
   for ( std::list<char *>::iterator it = begin(); it != end(); it++ )
      delete [] (*it);
   clear();
   for ( std::list<PdfDictionary *>::iterator it = additionalDictionaries.begin(); it != additionalDictionaries.end(); it++ )
      delete (*it);
   additionalDictionaries.clear();
   }
      