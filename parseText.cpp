
#include "Document.h"

#include "PostScript_i.h"
#include "PostScript_i.c"

   IPostScript *pIPostScript = NULL;

   HRESULT PdfDocument::parseText(long pageNumber,HDC hdc,RECT *prcWindowsClip,void *pvIPostScriptTakeText) {
   PdfPage *pPdfPage = GetPage(pageNumber,NULL);
   if ( ! ( S_OK == pPdfPage -> ParseText(hdc,prcWindowsClip,pvIPostScriptTakeText) ) ) {
      char *pszError = NULL;
      pPdfPage -> GetLastError(&pszError);
      strcpy(szErrorMessage,pszError);
      return E_FAIL;
   }
   return S_OK;
   }


   HRESULT PdfPage::parseText(HDC hdc,RECT *prcWindowsClip,void *pvIPostScriptTakeText) {

   char *pObjectIds = (char *)pContentsObject -> Value("Contents");

   if ( NULL == pObjectIds )
      return S_OK;

   char szOCXFile[MAX_PATH];

   strcpy(szOCXFile,szModuleName);

   char *p = strrchr(szOCXFile,'\\');
   *p = '\0';

   sprintf(szOCXFile + strlen(szOCXFile),"\\%s","PostScript.dll");
   FILE *fX = fopen(szOCXFile,"rb");
   if ( ! fX ) 
      return E_FAIL;
   fclose(fX);

   HMODULE hModule = LoadLibrary(szOCXFile);

   IClassFactory *pIClassFactory = NULL;

   long (__stdcall *cf)(REFCLSID rclsid, REFIID riid, void **ppObject) = NULL;

   cf = (long (__stdcall *)(REFCLSID rclsid, REFIID riid, void **ppObject))GetProcAddress(hModule,"DllGetClassObject");

   cf(CLSID_PostScript,IID_IClassFactory,reinterpret_cast<void **>(&pIClassFactory)); 

   pIClassFactory -> CreateInstance(NULL,IID_IPostScript,reinterpret_cast<void **>(&pIPostScript));

   HRESULT rc = S_OK;

   long objectCount = 1;
   bool isArray = pdfUtility.IsArray(pObjectIds);

   if ( isArray ) 
      objectCount = pdfUtility.ArraySize(pObjectIds);

   for ( long k = 0; k < objectCount; k++ ) {

      PdfObject *pObject = NULL;

      if ( isArray )
         pObject = pDocument -> FindObjectById(pdfUtility.IntegerValueFromReferenceArray(pObjectIds,k + 1));
      else
         pObject = pDocument -> IndirectObject(pObjectIds);

      if ( NULL == pObject )
         continue;

      PdfStream *pStream = pObject -> Stream();

      if ( NULL == pStream )
         continue;

      if ( ! pStream -> IsUncompressed() )
         pStream -> Decompress();

      if ( ! ( S_OK == pIPostScript -> ParseText((char *)pStream -> Storage(),pStream -> BinaryDataSize(),reinterpret_cast<void *>(this),pvIPostScriptTakeText,hdc,prcWindowsClip) ) ) {
         char *pszError = NULL;
         pIPostScript -> GetLastError(&pszError);
         strcpy(szErrorMessage,pszError);
         pIPostScript -> Release();
         return E_FAIL;
      }

   }

   pIPostScript -> Release();

   FreeLibrary(hModule);

   return S_OK;
   }