// Copyright 2017 InnoVisioNate Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "Encryption.h"

   void PdfEncryption::AES(unsigned char *pKey, long keySize,unsigned char *pInput,long inputSize,unsigned char *pOutput) { 

   long padLength = 16 - (inputSize % 16);

   if ( ! padLength )
      padLength = 16;

//padLength = 16 - padLength;
   
   BYTE *pNewInput = new BYTE[inputSize + padLength];

   memcpy(pNewInput,pInput,inputSize);

   for ( long k = inputSize; k < inputSize + padLength; k++ )
      pNewInput[k] = (unsigned char)padLength;

   BYTE *pNewOutput = new BYTE[inputSize + padLength];

   aesImplementation.init(AESImplementation::CBC,AESImplementation::Decrypt,pKey,AESImplementation::Key16Bytes,pKey + 16);

   long rc = aesImplementation.padDecrypt(pNewInput,inputSize + padLength,pNewOutput);

   return;
   }

#if 0
void
wxPdfEncrypt::AES(unsigned char* key, int WXUNUSED(keylen),
                  unsigned char* textin, int textlen,
                  unsigned char* textout)
{

  GenerateInitialVector(textout);

  m_aes->init(wxPdfRijndael::CBC, wxPdfRijndael::Encrypt, key, wxPdfRijndael::Key16Bytes, textout);

  int offset = CalculateStreamOffset();

  int len = m_aes->padEncrypt(&textin[offset], textlen, &textout[offset]);

  // It is a good idea to check the error code
  if (len < 0)
  {
    wxLogError(_T("wxPdfEncrypt::AES: Error on encrypting."));
  }
}

void
wxPdfEncrypt::GenerateInitialVector(unsigned char iv[16])
{
  wxString keyString = wxPdfDocument::GetUniqueId();

  const char* key = (const char*) keyString.c_str();

  GetMD5Binary((const unsigned char*) key, keyString.Length(), iv);
}

int
wxPdfEncrypt::CalculateStreamLength(int length)
{
  int realLength = length;
  if (m_rValue == 4)
  {
//    realLength = (length % 0x7ffffff0) + 32;
    realLength = ((length + 15) & ~15) + 16;
    if (length % 16 == 0)
    {
      realLength += 16;
    }
  }
  return realLength;
}

int
wxPdfEncrypt::CalculateStreamOffset()
{
  int offset = 0;
  if (m_rValue == 4)
  {
    offset = 16;
  }
  return offset;
}

wxString
wxPdfEncrypt::CreateDocumentId()
{
  wxString documentId;
  unsigned char id[16];
  GenerateInitialVector(id);
  int k;
  for (k = 0; k < 16; k++)
  {
    documentId.Append(id[k]);
  }
  return documentId;
}
#endif