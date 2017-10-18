// Copyright 2017 InnoVisioNate Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "Encryption.h"

#define MD5_HASHBYTES 16

   BYTE PdfEncryption::passwordPaddingString[] = {0x28,0xBF,0x4E,0x5E,0x4E,0x75,0x8A,0x41,0x64,0x00,0x4E,0x56,0xFF,0xFA,0x01,0x08,
                                                      0x2E,0x2E,0x00,0xB6,0xD0,0x68,0x3E,0x80,0x2F,0x0C,0xA9,0xFE,0x64,0x53,0x69,0x7A};

   AESImplementation PdfEncryption::aesImplementation;

   PdfEncryption::PdfEncryption(PdfDocument *pp,PdfObject *pEO) :

      pDocument(pp),
      pEncryptionObject(pEO),
      version(0),
      revision(0),
      permissionFlags(0),
      keyLength(0),
      keyByteLength(0),
      pDocumentId(NULL),

      oValueLength(0),
      uValueLength(0),
      documentIdLength(0),
      aesKeyLength(0),

      encryptMetaData(true),

      pCryptFilterDictionary(NULL)

   {

   memset(encryptionKey,0,sizeof(encryptionKey));
   memset(userPasswordEncryptionKey,0,sizeof(userPasswordEncryptionKey));

   memset(szDocumentId,0,sizeof(szDocumentId));

   memset(calculatedOValue,0,sizeof(calculatedOValue));
   memset(calculatedUValue,0,sizeof(calculatedUValue));

   memset(rawOValue,0,sizeof(rawOValue));
   memset(rawUValue,0,sizeof(rawUValue));

   memset(userPassword,0,sizeof(userPassword));
   memset(ownerPassword,0,sizeof(ownerPassword));

   memset(szCFMValue,0,sizeof(szCFMValue));

   PdfDictionary *pDictionary = pEncryptionObject -> Dictionary();

   BYTE *pValue = pDictionary -> Value("Filter");

   if ( ! pValue )
      return;

   if ( strncmp((char *)pValue,"/Standard",10) )
      return;

   pDocumentId = (unsigned char *)pDocument -> ID();

   if ( ! pDocumentId )
      return;

   char szTemp[256];

   strcpy(szTemp,(char *)pDocumentId);

   if ( strstr(szTemp,"><") )
      *(strstr(szTemp,"><")) = '\0';

   pDocumentId = (unsigned char *)szTemp;

   if ( '[' == *pDocumentId )
      pDocumentId++;

   if ( '<' == *pDocumentId )
      pDocumentId++;

   strcpy(szDocumentId,(char *)pDocumentId);

   documentIdLength = (long)strlen(szDocumentId);

   pdfUtility.ASCIIHexDecodeInPlace(szDocumentId);

   pDocumentId = (unsigned char *)szDocumentId;

   documentIdLength /= 2;

   pValue = pDictionary -> Value("V");
   
   version = 0L;

   if ( pValue )
      version = atol((char *)pValue);

   if ( version < 2 || version > 4 )
      return;

   pValue = pDictionary -> Value("R");

   revision = atol((char *)pValue);

   pValue = pDictionary -> Value("Length");
   
   keyLength = 40;
   if ( pValue )
      keyLength = atol((char *)pValue);

   if ( 2 == revision )
      keyLength = 40;

   keyByteLength = keyLength / 8;

   pValue = pDictionary -> Value("P");

   permissionFlags = atol((char *)pValue);

   unsigned char *pOValue = (unsigned char *)pDictionary -> Value("O");

   long n = pDictionary -> ValueSize("O");

   oValueLength = 0;

   if ( '(' == *pOValue ) {
      memcpy(rawOValue,pOValue + 1,n - 2);
      n -= 2;
      long j = 0;
      while ( j < n ) {
         if ( '\\' == rawOValue[j] && ')' == rawOValue[j + 1] ) {
            unsigned char bTemp[32];
            memcpy(bTemp,rawOValue + j + 1,n - j - 1);
            memcpy(rawOValue + j,bTemp,n - j - 1);
            rawOValue[n - 1] = '\0';
            n--;
         }
         j++;
      }
   } else {
      memcpy(rawOValue,pOValue,n);
   }

   oValueLength = n;

   unsigned char *pUValue = (unsigned char *)pDictionary -> Value("U");

   n = pDictionary -> ValueSize("U");

   uValueLength = 0;

   if ( '(' == *pUValue ) {
      memcpy(rawUValue,pUValue + 1,n - 2);
      n -= 2;
      long j = 0;
      while ( j < n ) {
         if ( '\\' == rawUValue[j] && ')' == rawUValue[j + 1] ) {
            unsigned char bTemp[32];
            memcpy(bTemp,rawUValue + j + 1,n - j - 1);
            memcpy(rawUValue + j,bTemp,n - j - 1);
            rawUValue[n - 1] = '\0';
            n--;
         }
         j++;
      }
   } else {
      memcpy(rawUValue,pUValue,n);
   }

   uValueLength = n;

   pValue = pDictionary -> Value("EncryptMetaData");
   if ( pValue )
      encryptMetaData = ( 0 == strncmp((char *)pValue,"true",4) || 0 == strncmp((char *)pValue,"TRUE",4) );

   if ( 4 == version ) {

      pCryptFilterDictionary = pDictionary -> GetDictionary("CF");

      if ( ! pCryptFilterDictionary )
         return;

      BYTE *pStmF = pDictionary -> Value("StmF");

      BYTE *pStrF = pDictionary -> Value("StrF");

      if ( strncmp((char *)pStmF,"/StdCF",6) && strncmp((char *)pStmF,"StdCF",5) &&
            strncmp((char *)pStmF,"/Identity",9) && strncmp((char *)pStmF,"Identity",8) )
         return;

      PdfDictionary *pStandardCryptFilterDictionary = NULL;

      if ( strstr((char *)pStmF,"StdCF") ) {

         PdfDictionary *pStandardCryptFilterDictionary = pCryptFilterDictionary -> GetDictionary((char *)pStmF);

         if ( ! pStandardCryptFilterDictionary )
            pStandardCryptFilterDictionary = pCryptFilterDictionary -> GetDictionary((char *)pStmF + 1);
         
         BYTE *pAuthCode = pStandardCryptFilterDictionary -> Value("CFM");

         if ( ! pAuthCode )
            pAuthCode = pStandardCryptFilterDictionary -> Value("/CFM");

         if ( ! pAuthCode )
            return;

         if ( strncmp((char *)pAuthCode,"V2",3) && strncmp((char *)pAuthCode,"/V2",3) && 
                  strncmp((char *)pAuthCode,"AESV2",5) && strncmp((char *)pAuthCode,"/AESV2",6) )
            return;

         if ( 0 == strncmp((char *)pAuthCode,"V2",3) || 0 == strncmp((char *)pAuthCode,"/V2",3) )
            strcpy(szCFMValue,"V2");
         else 
//            strcpy(szCFMValue,"AESV2");
return;

         BYTE *pAuthEvent = pStandardCryptFilterDictionary -> Value("AuthEvent");

         if ( ! pAuthEvent )
            pAuthEvent = pStandardCryptFilterDictionary -> Value("/AuthEvent");

         if ( ! pAuthEvent )
            return;

         if ( strncmp((char *)pAuthEvent,"DocOpen",7) && strncmp((char *)pAuthEvent,"/DocOpen",8) )
            return;

         pValue = pStandardCryptFilterDictionary -> Value("Length");
         if ( ! pValue )
            return;

         keyByteLength = atol((char *)pValue);

         keyLength = 8 * keyByteLength;

         version = 2;

         revision = 3;

      }


   }

   return;
   }


   long PdfEncryption::Authenticate(char *pszUserPassword,char *pszOwnerPassword) {

   if ( pszOwnerPassword )
      strcpy(ownerPassword,pszOwnerPassword);
   else
      ownerPassword[0] = '\0';

   if ( pszUserPassword )
      strcpy(userPassword,pszUserPassword);
   else 
      userPassword[0] = '\0';

   computeOwnerPasswordValue();

   computeEncryptionKey(userPassword);

   memcpy(userPasswordEncryptionKey,encryptionKey,sizeof(encryptionKey));

#if 0
   if ( pszUserPassword && strlen(pszUserPassword) )
      computeEncryptionKey(userPassword);
   else
      computeEncryptionKey(ownerPassword);
#endif

   computeUserPasswordValue();

   if ( memcmp(calculatedUValue,rawUValue,16) ) {
      return 0;
   }

   return 1;
   }


   long PdfEncryption::Encrypt(PdfObject *pEncryptedObject,BYTE *pInputData,long dataSize) {
   if ( ! encryptMetaData ) {
   }
   return decipher(pEncryptedObject,pInputData,dataSize);
   }


   long PdfEncryption::Decrypt(PdfObject *pEncryptedObject,BYTE *pInputData,long dataSize) {
   if ( ! encryptMetaData ) {
   }
   return decipher(pEncryptedObject,pInputData,dataSize);
   }


   long PdfEncryption::decipher(PdfObject *pEncryptedObject,BYTE *pInputData,long dataSize) {

   if ( ! pEncryptionObject )
      return 0;

   PdfDictionary *pDictionary = pEncryptedObject -> Dictionary();
   if ( pDictionary ) {
      BYTE *pValue = pDictionary -> Value("Type");
      if ( pValue && 0 == strncmp((char *)pValue,"/XRef",5) ) 
         return 0;
   }

   /*
   Algorithm 3.1 Encryption of data using the RC4 or AES algorithms

   1. Obtain the object number and generation number from the object identifier 
      of the string or stream to be encrypted (see Section 3.2.9, “Indirect Objects”). 
      If the string is a direct object, use the identifier of the indirect object containing it.
   */

   long objectId = pEncryptedObject -> Id();
   long generation = pEncryptedObject -> Generation();

   /*
   2. Treating the object number and generation number as binary integers, 
      extend the original n-byte encryption key to n + 5 bytes by appending 
      the low-order 3 bytes of the object number and the low-order 2 bytes 
      of the generation number in that order, low-order byte first. 
      (n is 5 unless the value of V in the encryption dictionary is 
      greater than 1, in which case n is the value of Length divided by 8.)

      If using the AES algorithm, extend the encryption key an additional 4 bytes 
      by adding the value "sAlT", which corresponds to the hexadecimal 
      values 0x73, 0x41, 0x6C, 0x54. (This addition is done for backward 
      compatibility and is not intended to provide additional security.)
   */

   unsigned char thisKey[MD5_HASHBYTES + 5 + 4];
   unsigned char digest[MD5_HASHBYTES];

   memcpy(thisKey,encryptionKey,keyByteLength);

   thisKey[keyByteLength + 0] = 0xFF & objectId;
   thisKey[keyByteLength + 1] = 0xFF & (objectId >> 8);
   thisKey[keyByteLength + 2] = 0xFF & (objectId >> 16);
   thisKey[keyByteLength + 3] = 0xFF & generation;
   thisKey[keyByteLength + 4] = 0xFF & (generation >> 8);

   long newKeyLength = keyByteLength + 5;

   if ( 4 == revision ) {
      newKeyLength += 4;
      thisKey[newKeyLength - 4] = 0x73;
      thisKey[newKeyLength - 3] = 0x41;
      thisKey[newKeyLength - 2] = 0x6C;
      thisKey[newKeyLength - 1] = 0x54;
   }

   /*
   3. Initialize the MD5 hash function and pass the result of step 2 as input to this function.
   */
   
   MD5Context context;
   MD5Init(&context);
   MD5Update(&context,thisKey,newKeyLength);
   MD5Final(digest,&context);

   /*
   4. Use the first (n + 5) bytes, up to a maximum of 16, of the output from the MD5 hash 
      as the key for the RC4 or AES symmetric key algorithms, along with the string or 
      stream data to be encrypted.

      If using the AES algorithm, the Cipher Block Chaining (CBC) mode, which requires an 
      initialization vector, is used. The block size parameter is set to 16 bytes, and the 
      initialization vector is a 16-byte random number that is stored as the first 16 bytes 
      of the encrypted stream or string.

      The output is the encrypted data to be stored in the PDF file.
   */

   if ( 4 > revision || 0 == strncmp(szCFMValue,"V2",3) )
      RC4(digest,keyByteLength == 5 ? 10 : 16,pInputData,dataSize,pInputData);
   else
      AES(digest,keyByteLength == 5 ? 10 : 16,pInputData,dataSize,pInputData);

   return 1;
   }


   void PdfEncryption::MD5Init(MD5Context *ctx) {
   ctx -> buf[0] = 0x67452301;
   ctx -> buf[1] = 0xEFCDAB89;
   ctx -> buf[2] = 0x98BADCFE;
   ctx -> buf[3] = 0x10325476;
   ctx -> bits[0] = 0;
   ctx -> bits[1] = 0;
   return;
   }


   char* PdfEncryption::MD5End(MD5Context *ctx, char *buf) {

   unsigned char digest[MD5_HASHBYTES];

   char hex[] = "0123456789ABCDEF";

   if ( ! buf )
      buf = new char[MD5_HASHBYTES + MD5_HASHBYTES];
    
   MD5Final(digest,ctx);

   for ( long k = 0; k < MD5_HASHBYTES; k++ ) {
      buf[k + k] = hex[digest[k] >> 4];
      buf[k + k + 1 ] = hex[digest[k] & 0x0f];
   }

   buf[MD5_HASHBYTES + MD5_HASHBYTES] = '\0';

   return buf;

   }

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
   void PdfEncryption::MD5Final(unsigned char digest[MD5_HASHBYTES], MD5Context *ctx) {

   unsigned count;
   unsigned char *p;

   /* Compute number of bytes mod 64 */

   count = (ctx -> bits[0] >> 3) & 0x3F; 

   /* Set the first char of padding to 0x80.  This is safe since there is
      always at least one byte free */

   p = ctx -> input + count;

   *p++ = 0x80;

   /* Bytes of padding needed to make 64 bytes */

   count = 64 - 1 - count;

   /* Pad out to 56 mod 64 */

   if ( count < 8 ) {

      /* Two lots of padding:  Pad the first block to 64 bytes */
      memset(p, 0, count);
      MD5Transform(ctx -> buf, reinterpret_cast<unsigned int *>(ctx -> input));

      /* Now fill the next block with 56 bytes */
      memset(ctx -> input, 0, 56);

   } else {
      /* Pad block to 56 bytes */
      memset(p, 0, count - 8);   
   }

   /* Append length in bits and transform */
   reinterpret_cast<unsigned int *>(ctx -> input)[14] = ctx -> bits[0];
   reinterpret_cast<unsigned int *>(ctx -> input)[15] = ctx -> bits[1];

   MD5Transform(ctx -> buf, reinterpret_cast<unsigned int *>(ctx -> input));

   memcpy(digest, ctx -> buf, MD5_HASHBYTES);

   memset( reinterpret_cast<char *>(ctx), 0, sizeof(ctx));       /* In case it's sensitive */

   return;
   }


   void PdfEncryption::MD5Update(MD5Context *ctx, unsigned char *buf, unsigned len) {

   unsigned int t;

   /* Update bitcount */

   t = ctx -> bits[0];

   if ( (ctx -> bits[0] = t + ( static_cast<unsigned int>(len) << 3)) < t) {

        ctx -> bits[1]++;         /* Carry from low to high */
   }

   ctx -> bits[1] += len >> 29;

   t = (t >> 3) & 0x3f;        /* Bytes already in shsInfo -> data */

   /* Handle any leading odd-sized chunks */

   if ( t ) {

      unsigned char *p = static_cast<unsigned char *>(ctx -> input) + t;

      t = 64 - t;
      if ( len < t ) {
         memcpy(p, buf, len);
         return;
      }

      memcpy(p, buf, t);
      MD5Transform(ctx -> buf, reinterpret_cast<unsigned int *>(ctx -> input));
      buf += t;
      len -= t;
   }

   /* Process data in 64-byte chunks */

   while ( len >= 64 ) {
      memcpy(ctx -> input, buf, 64);
      MD5Transform(ctx -> buf, reinterpret_cast<unsigned int *>(ctx -> input));
      buf += 64;
      len -= 64;
   }

   /* Handle any remaining bytes of data. */

   memcpy(ctx -> input, buf, len);
   
   return;

}


/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))   
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
        ( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
   void PdfEncryption::MD5Transform(unsigned int buf[4], unsigned int const in[16]) {

   register unsigned int a, b, c, d;

   a = buf[0];
   b = buf[1];
   c = buf[2];
   d = buf[3];
 
   MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7); 
   MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
   MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
   MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
   MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7); 
   MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
   MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
   MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22); 
   MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);  
   MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12); 
   MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
   MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
   MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7); 
   MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
   MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
   MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);
 
   MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);  
   MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);  
   MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
   MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20); 
   MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);  
   MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9); 
   MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
   MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20); 
   MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);  
   MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9); 
   MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14); 
   MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20); 
   MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
   MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);  
   MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
   MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);
 
   MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
   MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
   MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
   MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
   MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);  
   MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11); 
   MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16); 
   MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
   MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4); 
   MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11); 
   MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16); 
   MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23); 
   MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);  
   MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
   MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
   MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23); 
 
   MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
   MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
   MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
   MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21); 
   MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6); 
   MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10); 
   MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
   MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21); 
   MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);  
   MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
   MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15); 
   MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
   MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);  
   MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
   MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15); 
   MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21); 
 
   buf[0] += a;
   buf[1] += b;
   buf[2] += c;
   buf[3] += d;

   return;
   }


   void PdfEncryption::RC4(unsigned char* key, long keylen,unsigned char* textin, long textlen,unsigned char* textout) {

   unsigned char rc4[256];

   for ( unsigned char k = 0; k < 256; k++ ) 
      rc4[k] = k;

   int j = 0;
   int t = 0;

   for ( int k = 0; k < 256; k++ ) {
      t = rc4[k];
      j = (j + t + key[k % keylen]) % 256;
      rc4[k] = rc4[j];
      rc4[j] = t;
   }

   int a = 0;
   int b = 0;

   unsigned char uc;

   for ( int k = 0; k < textlen; k++ ) {

      a = (a + 1) % 256;

      t = rc4[a];

      b = (b + t) % 256;

      rc4[a] = rc4[b];

      rc4[b] = t;

      uc = rc4[(rc4[a] + rc4[b]) % 256];

      textout[k] = textin[k] ^ uc;

   }

   return;

   }

