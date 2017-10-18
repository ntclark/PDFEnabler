// Copyright 2017 InnoVisioNate Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "Encryption.h"

   void PdfEncryption::computeEncryptionKey(char *pszPassword) {

   unsigned char digest[MD5_HASHBYTES];

   unsigned char paddedPassword[64];

   /*

      Algorithm 2: Computing an encryption key

      a) Pad or truncate the password string to exactly 32 bytes. If the password string is more 
         than 32 bytes long, use only its first 32 bytes; if it is less than 32 bytes long, pad 
         it by appending the required number of additional bytes from the beginning of the 
            following padding string:

            < 28 BF 4E 5E 4E 75 8A 41 64 00 4E 56 FF FA 01 08
               2E 2E 00 B6 D0 68 3E 80 2F 0C A9 FE 64 53 69 7A >

         That is, if the password string is n bytes long, append the first 32 - n bytes of the 
         padding string to the end of the password string. If the password string is 
         empty (zero-length), meaning there is no user password, substitute the entire 
         padding string in its place.

   */

   long n = (long)strlen(pszPassword);

   memcpy(paddedPassword,pszPassword,min(n,32));
   memcpy(paddedPassword + n,passwordPaddingString,32 - min(n,32));

   /*
      b) Initialize the MD5 hash function and pass the result of step (a) as input to this function.
   */

   memset(digest,0,sizeof(digest));

   MD5Context context;

   MD5Init(&context);

   MD5Update(&context,paddedPassword,32);

   /*
      c) Pass the value of the encryption dictionary’s O entry to the MD5 hash function. 
         ("Algorithm 3: Computing the encryption dictionary’s O (owner password) value" 
         shows how the O value is computed.)
   */

   MD5Update(&context,rawOValue,oValueLength);

   /*
      d) Convert the integer value of the P entry to a 32-bit unsigned binary number 
         and pass these bytes to the MD5 hash function, low-order byte first.
   */

   unsigned char permissions[4];

   permissions[0] = (unsigned char)( permissionFlags        & 0xFF);
   permissions[1] = (unsigned char)((permissionFlags >>  8) & 0xFF);
   permissions[2] = (unsigned char)((permissionFlags >> 16) & 0xFF);
   permissions[3] = (unsigned char)((permissionFlags >> 24) & 0xFF);

   MD5Update(&context,permissions,4);

   /*
      e) Pass the first element of the file’s file identifier array (the value of the 
         ID entry in the document’s trailer dictionary; see Table 15) to the MD5 hash function.

      NOTE  The first element of the ID array generally remains the same for a given document. 
            However, in some situations, conforming writers may regenerate the ID array if a 
            new generation of a document is created. Security handlers are encouraged not to 
            rely on the ID in the encryption key computation.
   */

   MD5Update(&context,pDocumentId,documentIdLength);

   /*
      f) (Security handlers of revision 4 or greater) If document metadata is not being encrypted, 
         pass 4 bytes with the value 0xFFFFFFFF to the MD5 hash function.
   */

   if ( 3 < revision && ! encryptMetaData ) {
      memset(permissions,0xFF,sizeof(permissions));
      MD5Update(&context,permissions,4);
   }

   /*
      g) Finish the hash.
   */

   MD5Final(digest,&context);

   /*
      h) (Security handlers of revision 3 or greater) Do the following 50 times: Take the output 
         from the previous MD5 hash and pass the first n bytes of the output as input into a new 
         MD5 hash, where n is the number of bytes of the encryption key as defined by the value 
         of the encryption dictionary’s Length entry.
   */

   if ( 2 < revision ) {
      for ( long k = 0; k < 50; k++ ) {
         MD5Init(&context);
         MD5Update(&context,digest,keyByteLength);
         MD5Final(digest,&context);      
      }
   }

   /*
      i) Set the encryption key to the first n bytes of the output from the final MD5 hash, where n 
         shall always be 5 for security handlers of revision 2 but, for security handlers of 
         revision 3 or greater, shall depend on the value of the encryption dictionary’s Length entry.

      This algorithm, when applied to the user password string, produces the encryption key used to 
      encrypt or decrypt string and stream data according to "Algorithm 1: Encryption of data using 
      the RC4 or AES algorithms"in 7.6.2, "General Encryption Algorithm."

   */

   memset(encryptionKey,0,sizeof(encryptionKey));

   memcpy(encryptionKey,digest,keyByteLength);

   return;
   }


   void PdfEncryption::computeOwnerPasswordValue() {

   unsigned char digest[MD5_HASHBYTES];

   unsigned char paddedOwnerPassword[64];
   unsigned char paddedUserPassword[64];

   memset(paddedOwnerPassword,0,sizeof(paddedOwnerPassword));
   memset(paddedUserPassword,0,sizeof(paddedUserPassword));

   /*

      Algorithm 3: Computing the encryption dictionary’s O (owner password) value

      a) Pad or truncate the owner password string as described in step (a) of 
         "Algorithm 2: Computing an encryption key". If there is no owner password, 
         use the user password instead.

   */

   long n = (long)strlen(ownerPassword);
   if ( ! n ) {
      n = (long)strlen(userPassword);
      memcpy(paddedOwnerPassword,userPassword,min(n,32));
   } else
      memcpy(paddedOwnerPassword,ownerPassword,min(n,32));
   memcpy(paddedOwnerPassword + n,passwordPaddingString,32 - min(n,32));

   /*
      b) Initialize the MD5 hash function and pass the result of step (a) 
         as input to this function.
   */

   MD5Context context;

   MD5Init(&context);

   MD5Update(&context,paddedOwnerPassword,32);

   /*
      c) (Security handlers of revision 3 or greater) Do the following 50 times: 
         Take the output from the previous MD5 hash and pass it as input into a new MD5 hash.
   */

   MD5Final(digest,&context);

   if ( 2 < revision ) {
      for ( long k = 0; k < 50; k++ ) {
         MD5Init(&context);
         MD5Update(&context,digest,keyByteLength);
         MD5Final(digest,&context);      
      }
   }

   /*
      d) Create an RC4 encryption key using the first n bytes of the output from the final 
         MD5 hash, where n shall always be 5 for security handlers of revision 2 but, for 
         security handlers of revision 3 or greater, shall depend on the value of the 
         encryption dictionary’s Length entry.
   */

   unsigned char ownerKey[MD5_HASHBYTES];

   memset(ownerKey,0,sizeof(ownerKey));

   memcpy(ownerKey,digest,keyByteLength);

   /*
      e) Pad or truncate the user password string as described in step (a) of 
         "Algorithm 2: Computing an encryption key".
   */

   n = (long)strlen(userPassword);
   memcpy(paddedUserPassword,userPassword,min(n,32));
   memcpy(paddedUserPassword + n,passwordPaddingString,32 - min(n,32));

   /*
      f) Encrypt the result of step (e), using an RC4 encryption function with the 
         encryption key obtained in step (d).
   */

   memset(calculatedOValue,0,sizeof(calculatedOValue));

   memcpy(calculatedOValue,paddedUserPassword,32);

   RC4(ownerKey,keyByteLength,calculatedOValue,32,calculatedOValue);

   /*
      g) (Security handlers of revision 3 or greater) Do the following 19 times: 
         Take the output from the previous invocation of the RC4 function and pass 
         it as input to a new invocation of the function; use an encryption key 
         generated by taking each byte of the encryption key obtained in step (d) 
         and performing an XOR (exclusive or) operation between that byte and 
         the single-byte value of the iteration counter (from 1 to 19).
   */

   if ( 2 < revision ) {

      unsigned char tempKey[MD5_HASHBYTES];
   
      for ( unsigned char bCounter = 0x01; bCounter < 0x14; bCounter += 0x01 ) {
   
         for ( long j = 0; j < keyByteLength; j++ ) 
            tempKey[j] = ownerKey[j] ^ bCounter;
   
         RC4(tempKey,keyByteLength,calculatedOValue,32,calculatedOValue);
   
      }
   
   }

   /*
      h) Store the output from the final invocation of the RC4 function as the 
         value of the O entry in the encryption dictionary.
   */


   /*

      Algorithm 3.7 Authenticating the owner password

      3.7.1. Compute an encryption key from the supplied password 
         string, as described in steps 1 to 4 of Algorithm 3.3.

   */


   /*
      3.7.2. (Revision 2 only) Decrypt the value of the encryption dictionary’s O entry, 
         using an RC4 encryption function with the encryption key computed in step 1.

         (Revision 3 or greater) Do the following 20 times: 

         Decrypt the value of the encryption dictionary’s O entry (first iteration) or the 
         output from the previous iteration (all subsequent iterations), 
         using an RC4 encryption function with a different encryption key at 
         each iteration. The key is generated by taking the original key 
         (obtained in step 1) and performing an XOR (exclusive or) operation between 
         each byte of the key and the single-byte value of the iteration counter (from 19 to 0).
   */

   unsigned char userPasswordString[32];

   memset(userPasswordString,0,sizeof(userPasswordString));

   memcpy(userPasswordString,calculatedOValue,32);

   if ( 2 == revision ) {

      RC4(ownerKey,keyByteLength,userPasswordString,32,userPasswordString);

   } else {

      unsigned char tempKey[MD5_HASHBYTES];

      for ( unsigned char bCounter = 0x13; bCounter != 0xFF; bCounter -= 0x01 ) {
   
         for ( long j = 0; j < keyByteLength; j++ )
            tempKey[j] = ownerKey[j] ^ bCounter;
   
         RC4(tempKey,keyByteLength,userPasswordString,32,userPasswordString);
   
      }

   }

   /*
      3. The result of step 2 purports to be the user password. 
         Authenticate this user password using Algorithm 3.6. 
         If it is correct, the password supplied is the correct owner password.
   */

   return;
   }


   void PdfEncryption::computeUserPasswordValue() {

   unsigned char digest[MD5_HASHBYTES + 1];

   /*
   Algorithm 3.5 Computing the encryption dictionary’s U (user password) value (Revision 3 or greater)

      3.5.1. Create an encryption key based on the user password string, as described in Algorithm 3.2.

   */

   /*
      3.5.2. Initialize the MD5 hash function and pass the 32-byte padding string shown in step 1 
         of Algorithm 3.2 as input to this function.
   */

   MD5Context context;
   MD5Init(&context);

   MD5Update(&context,passwordPaddingString,32);

   /*
      3.5.3. Pass the first element of the file’s file identifier array (the value of the ID entry 
         in the document’s trailer dictionary; see Table 3.13 on page 97) to the hash function 
         and finish the hash. (See implementation note 26 in Appendix H.)
   */

   // !!!!!!!!!!!!!!!!!!!!!!!!!!!! Do NOT forget that the document ID is AsciiHEX encoded and needs to be DECODED !!!!!!!!!!!!!!!!!!!!!!!!!!!

   MD5Update(&context,pDocumentId,documentIdLength);
   
   /*
      3.5.4. Encrypt the 16-byte result of the hash, using an RC4 encryption 
         function with the encryption key from step 1.
   */

   MD5Final(digest,&context); 

   RC4(userPasswordEncryptionKey,keyByteLength,digest,16,digest);

   /*
      3.5.5. Do the following 19 times: Take the output from the previous invocation of the 
         RC4 function and pass it as input to a new invocation of the function; use an 
         encryption key generated by taking each byte of the original encryption key 
         (obtained in step 1) and performing an XOR (exclusive or) operation between 
         that byte and the single-byte value of the iteration counter (from 1 to 19).
   */

   unsigned char tempKey[MD5_HASHBYTES];

   for ( unsigned char bCounter = 0x01; bCounter < 0x14; bCounter += 0x01 ) {

     for ( long j = 0; j < keyByteLength; j++ )
        tempKey[j] = userPasswordEncryptionKey[j] ^ bCounter;

     RC4(tempKey,keyByteLength,digest,16,digest);

   }

   memcpy(calculatedUValue,digest,16);

   /*
      3.5.6. Append 16 bytes of arbitrary padding to the output from the final invocation of the 
         RC4 function and store the 32-byte result as the value of the U entry in the encryption dictionary.
   */

   /*
      Algorithm 3.6 Authenticating the user password

      3.6.1. Perform all but the last step of Algorithm 3.4 (Revision 2) or Algorithm 3.5 (Revision 3 or greater) 
         using the supplied password string.
   
         3.5.1. Create an encryption key based on the user password string, as described in Algorithm 3.2.

            = userKey

   */

   return;
   }


#if 0
The “Encryption” section of the PDF Reference (section 3.5) mentions that when the encryption dictionary entry with a key of 
/V has a value of 3, then document de/encryption is via “an unpublished algorithm that permits encryption key lengths ranging 
from 40 to 128 bits.” As far as I can tell, this algorithm is in fact unpublished – by anyone. The closest I could find was a 
reference to it in one of Dmitri Sklyarov’s 2001 DEFCON slides. Yeah, that Sklyarov, those DEFCON slides. Maybe he described 
the whole algorithm in his talk, but the DEFCON A/V archives for that year seem to be down. So I sighed, put on my reversing 
cap, and figured it out.

The standard object-key-derivation algorithm (section 3.5.1, “General Encryption Algorithm”) accepts as inputs the file encryption 
key, the object number, and the generation number, and produces as out put a key for a symmetric cipher. The “unpublished” algorithm 
accepts the same inputs and also produces a symmetric cipher key. It presumably could be used with either RC4 or AES as documented 
for /V values of 1 and 2, although I’ve so far only seen RC4 used.

The unpublished algorithm in use when /V is 3 is as follows (mimicking algorithm 3.5.1):

1. Obtain the object number and generation number from the object identifier of the string or stream to be encrypted. If the string 
is a direct object, use the identifier of the indirect object containing it. Substitute the object number with the result of 
exclusive-or-ing it with the hexadecimal value 0x3569AC. Substitute the generation number with the result of exclusive-or-ing 
it with the hexadecimal value 0xCA96.

2. Treating the substituted object and generation numbers as binary integers, extend the original n-byte encryption key to 
n + 5 bytes by appending the low-order byte of the object number, the low-order byte of the generation number, the second-lowest 
byte of the object number, the second-lowest byte of the generation number, and third-lowest byte of the object number in that 
order, low-order byte first. Extend the encryption key an additional 4 bytes by adding the value "sAlT", which corresponds to 
the hexadecimal values 0x73, 0x41, 0x6C, 0x54.

3. Initialize the MD5 hash function and pass the result of step 2 as input to this function.

4. Use the first (n + 5) bytes, up to a maximum of 16, of the output from the MD5 hash as the key for the symmetric-key algorithm, 
along with the string or stream data to be encrypted.

Now hopefully Google will be kind enough to index this in a way that lets other people find it. 

#endif