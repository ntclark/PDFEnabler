
#include "pdfEnabler.h"

#include "Document.h"

   HRESULT PdfEnabler::Document(IPdfDocument **ppDocument) {

   if ( ! ppDocument )
      return E_POINTER;

   PdfDocument *pDocument = new PdfDocument(this);

   pDocument -> QueryInterface(IID_IPdfDocument,reinterpret_cast<void **>(ppDocument));

   return S_OK;
   }