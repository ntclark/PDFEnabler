// Copyright 2017, 2018, 2019 InnoVisioNate Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdfEnabler.h"

#include "Document.h"

   HRESULT PdfEnabler::Document(IPdfDocument **ppDocument) {

   if ( ! ppDocument )
      return E_POINTER;

   PdfDocument *pDocument = new PdfDocument(this);

   pDocument -> QueryInterface(IID_IPdfDocument,reinterpret_cast<void **>(ppDocument));

   return S_OK;
   }