
#include <windows.h>
#include <gdiplus.h>

#include <tchar.h>
#include <olectl.h>

#include "Document.h"

   HRESULT __stdcall PdfPage::QueryInterface(REFIID riid, void** ppv) {
   *ppv = 0;

   if ( IID_IUnknown == riid )
      *ppv = this;
   else

      if ( IID_IPdfPage == riid )
         *ppv = static_cast<IPdfPage *>(this);
      else

         return pDocument -> QueryInterface(riid,ppv);

   AddRef();

   return S_OK;
   }


   ULONG __stdcall PdfPage::AddRef() {
   return refCount++;
   }


   ULONG __stdcall PdfPage::Release() { 
   if ( --refCount == 0 ) {
      delete this;
      return 0;
   }
   return refCount;
   }


   // IDispatch

   STDMETHODIMP PdfPage::GetTypeInfoCount(UINT * pctinfo) { 
   *pctinfo = 1;
   return S_OK;
   } 


   long __stdcall PdfPage::GetTypeInfo(UINT itinfo,LCID lcid,ITypeInfo **pptinfo) { 
   *pptinfo = NULL; 
   if ( itinfo != 0 ) 
      return DISP_E_BADINDEX; 
   *pptinfo = pITypeInfo;
   pITypeInfo -> AddRef();
   return S_OK; 
   } 
 

   STDMETHODIMP PdfPage::GetIDsOfNames(REFIID riid,OLECHAR** rgszNames,UINT cNames,LCID lcid, DISPID* rgdispid) { 
   return DispGetIDsOfNames(pITypeInfo,rgszNames,cNames,rgdispid);
   }


   STDMETHODIMP PdfPage::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, 
                                           WORD wFlags,DISPPARAMS FAR* pdispparams, VARIANT FAR* pvarResult,
                                           EXCEPINFO FAR* pexcepinfo, UINT FAR* puArgErr) { 
   return DispInvoke(this,pITypeInfo,dispidMember,wFlags,pdispparams,pvarResult,pexcepinfo,puArgErr); 
   }


   // IPdfPage


   HRESULT PdfPage::AddStream(BYTE *pStreamData,long dataSize,long identifier) {
   addStreamInternal(pStreamData,dataSize,identifier);
   return S_OK;
   }


   HRESULT PdfPage::PageSize(RECT *pRect) {
   if ( ! pRect )
      return E_POINTER;
   getPageSize(pRect);
   return S_OK;
   }


   HRESULT PdfPage::DisplayedPageSize(RECT *pRect) {
   if ( ! pRect )
      return E_POINTER;
   getPageSize(pRect);
   double rotation;
   getRotation(&rotation);
   if ( ! ( 0.0 == rotation ) && 0 == ((long)rotation % 90) ) {
      long t = pRect -> bottom;
      pRect -> bottom = pRect -> right;
      pRect -> right = t;
      t = pRect -> top;
      pRect -> top = pRect -> left;
      pRect -> left = t;
   }
   return S_OK;
   }


   HRESULT PdfPage::Rotation(double *pRotation) {
   if ( ! pRotation )
      return E_POINTER;
   getRotation(pRotation);
   return S_OK;
   }


   HRESULT PdfPage::AddImage(double fromLeft,double fromTop,double scaleX,double scaleY,HBITMAP bitmapHandle) {

   if ( ! bitmapHandle )
      return E_FAIL;

   long imageSize = 0L;
   long bitmapWidth = 0L, bitmapHeight = 0L;
   long componentsPerSample = 3L;
   long predictor = 12L;
   long bitsPerComponent = 8L;

   BYTE *pImageData = NULL;

   pImageData = BitmapBits(bitmapHandle,&imageSize,&bitmapWidth,&bitmapHeight);

   double rotation;

   Rotation(&rotation);

   if ( 90.0 == rotation )  {

      BYTE *pNewImageData = new BYTE[imageSize];

      long newRowSize = 3 * bitmapHeight;
      long oldRowSize = 3 * bitmapWidth;

      long oldRowIndex = 0;
      long n = oldRowSize - 3;

      for ( long k = 0; k < newRowSize; k += 3 ) {

         for ( long j = 0; j < bitmapWidth; j++ ) {

            pNewImageData[j * newRowSize + k + 0] = pImageData[ oldRowIndex * oldRowSize + n + 0 ];
            pNewImageData[j * newRowSize + k + 1] = pImageData[ oldRowIndex * oldRowSize + n + 1 ];
            pNewImageData[j * newRowSize + k + 2] = pImageData[ oldRowIndex * oldRowSize + n + 2 ];

            n -= 3;

            if ( 0 > n ) {
               oldRowIndex++;
               n = oldRowSize - 3;
            }

         }

      }

      long t = bitmapWidth;
      bitmapWidth = bitmapHeight;
      bitmapHeight = t;

      delete [] pImageData;

      pImageData = pNewImageData;

   }

   BYTE *pResult = NULL;

   long countBytes = pdfUtility.deflate(pImageData,imageSize,&pResult,predictor,bitmapWidth,bitsPerComponent,componentsPerSample);

   delete [] pImageData;

   imageSize = countBytes;

   pImageData = pResult;

   long objectId = pDocument -> NewObjectId();

   long dataSize = imageSize + 1024;

   BYTE *pNewData = new BYTE[dataSize];

   memset(pNewData,0,dataSize * sizeof(BYTE));

   long streamLength = sprintf((char *)pNewData,"<</Type/XObject/Subtype/Image/BitsPerComponent %ld/ColorSpace/DeviceRGB"
                                                   "/Filter/FlateDecode/DecodeParms<</Predictor %ld /Columns %ld /Colors %ld>>/Height %ld/Length %ld/Width %ld>>\nstream\n",
                                                      bitsPerComponent,predictor,bitmapWidth,componentsPerSample,bitmapHeight,imageSize,bitmapWidth);

   memcpy(pNewData + streamLength,pImageData,imageSize);

   streamLength += imageSize;

   streamLength += sprintf((char *)(pNewData + streamLength),"\nendstream\nendobj\n");

   PdfObject *pNewObject = new PdfObject(pDocument,objectId,0,pNewData,streamLength);

   pNewObject -> Stream() -> SetCompressed();

   pDocument -> AddObject(pNewObject);

   if ( pDocument -> XReference() )
      pDocument -> XReference() -> AddObject(pNewObject);

   char szImageName[64];

   sprintf(szImageName,"/cvImage%ld",pDocument -> counterValue("cvImages") + 1);

   pDocument -> incrementCounter("cvImages");

   char szResource[1024];

   PdfDictionary *pResourceDictionary = pContentsObject -> Dictionary("Resources");

   if ( pResourceDictionary ) {
   
      PdfDictionary *pExternalObjectsDictionary = pResourceDictionary -> GetDictionary("XObject");

      if ( pExternalObjectsDictionary ) {

         sprintf(szResource,"%s %ld 0 R",szImageName,objectId);

         pExternalObjectsDictionary -> AddValue(szResource);

      } else {

         sprintf(szResource,"/XObject <<%s %ld 0 R>>",szImageName,objectId);

         pResourceDictionary -> AddValue(szResource);

      }

      PdfElement *pProcSet = pResourceDictionary -> Element("ProcSet");

      if ( pProcSet ) {

         BYTE *pValue = pProcSet -> Value();

         pResourceDictionary -> SetValue("ProcSet",pdfUtility.addToArray((BYTE *)"/PDF /ImageB /ImageC",pValue),false);

      } else

         pResourceDictionary -> AddValue("/ProcSet [/PDF /ImageB /ImageC ]");

   } else {

      sprintf(szResource,"/Resources <</XObject <<%s %ld 0 R>>>>",szImageName,objectId);

      pContentsObject -> Dictionary() -> AddValue(szResource);

      pContentsObject -> Dictionary() -> AddValue("/ProcSet [/PDF /ImageB /ImageC]");

   }

   char szStream[1024];
   char szContent[256];

   RECT pageSize;
   PageSize(&pageSize);

   long x = (long)((double)fromLeft * 72.0);
   long y = pageSize.bottom - (long)((double)fromTop * 72.0) - (long)(scaleY * (double)bitmapHeight);

   if ( 90.0 == rotation ) {
      y = (long)((double)fromLeft * 72.0);
      x = (long)((double)fromTop * 72.0);
   }

   sprintf(szContent,"q\n1 0 0 1 %ld %ld cm\n%lf 0 0 %lf 0 0 cm\n%s Do\nQ\n",x,y,(double)bitmapWidth * scaleX,(double)bitmapHeight * scaleY,szImageName);

   sprintf(szStream,"%ld 0 obj<</BBox[0.0 0.0 %ld %ld]/FormType 1/Length %ld/Matrix[1.0 0.0 0.0 1.0 0.0 0.0]/Resources<</ProcSet[/PDF]/XObject<<%s %ld 0 R>>>>/Subtype/Form/Type/XObject>>\n"
                              "stream\n%s\nendstream\nendobj",pDocument -> NewObjectId(),bitmapWidth,bitmapHeight,(long)strlen(szContent),szImageName,objectId,szContent);

   PdfObject *pRenderObject = new PdfObject(pDocument,(BYTE *)szStream,(long)strlen(szStream));

   pDocument -> AddObject(pRenderObject);

   if ( pDocument -> XReference() )
      pDocument -> XReference() -> AddObject(pRenderObject);

   addContentsReference(pRenderObject);

   delete [] pImageData;
   delete [] pNewData;

   return S_OK;
   }


   HRESULT PdfPage::AddImageFromFile(double fromLeft,double fromTop,double scaleX,double scaleY,BSTR bstrFileName) {

   ULONG_PTR gdiplusToken;
   Gdiplus::GdiplusStartupInput gdiplusStartupInput = 0L;

   Gdiplus::GdiplusStartup(&gdiplusToken,&gdiplusStartupInput,NULL);

   Gdiplus::Bitmap *pImage = new Gdiplus::Bitmap(bstrFileName);

   HBITMAP hBitmap = NULL;

   pImage -> GetHBITMAP(NULL,&hBitmap);

   HRESULT rc = AddImage(fromLeft,fromTop,scaleX,scaleY,hBitmap);

   delete pImage;

   Gdiplus::GdiplusShutdown(gdiplusToken);

   return rc;
   }


   HRESULT PdfPage::AddSizedImageFromFile(double fromLeft,double fromTop,double width,double height,BSTR bstrFileName) {

   ULONG_PTR gdiplusToken;
   Gdiplus::GdiplusStartupInput gdiplusStartupInput = 0L;

   Gdiplus::GdiplusStartup(&gdiplusToken,&gdiplusStartupInput,NULL);

   Gdiplus::Bitmap *pImage = new Gdiplus::Bitmap(bstrFileName);

   double scaleX = 72.0 * width / (double)pImage -> GetWidth();
   double scaleY = 72.0 * height / (double)pImage -> GetHeight();

   if ( -1.0 == width )
      scaleX = 72.0 * height / (double)pImage -> GetHeight();
   else if ( -1.0 == height )
      scaleY = 72.0 * width / (double)pImage -> GetWidth();

   delete pImage;

   Gdiplus::GdiplusShutdown(gdiplusToken);

   return AddImageFromFile(fromLeft,fromTop,scaleX,scaleY,bstrFileName);
   }


   HRESULT PdfPage::AddCenteredImageFromFile(double fromLeft,double fromTop,BSTR bstrFileName) {

   RECT rcPage;

   DisplayedPageSize(&rcPage);

   ULONG_PTR gdiplusToken;
   Gdiplus::GdiplusStartupInput gdiplusStartupInput = 0L;

   Gdiplus::GdiplusStartup(&gdiplusToken,&gdiplusStartupInput,NULL);

   Gdiplus::Bitmap *pImage = new Gdiplus::Bitmap(bstrFileName);

   long cxPage = rcPage.right - rcPage.left;
   long cyPage = rcPage.bottom - rcPage.top;

   double cxDocumentInches = (double)cxPage / 72.0;
   double cyDocumentInches = (double)cyPage / 72.0;
   double cxImageInches = (double)pImage -> GetWidth() / (double)pImage -> GetHorizontalResolution();
   double cyImageInches = (double)pImage -> GetHeight() / (double)pImage -> GetVerticalResolution();

   double newFromLeft = fromLeft;
   double newFromTop = fromTop;

   double scale = 0.0;

   if ( cxPage > cyPage ) {

      scale = (cxDocumentInches - 2 * fromLeft) / cxImageInches;

      newFromLeft = cxDocumentInches / 2.0 - cxImageInches * scale / 2.0;
      newFromTop = cyDocumentInches - (cyDocumentInches / 2.0 + cyImageInches * scale / 2.0);

      if ( cyDocumentInches < newFromTop ) {

         scale = (cyDocumentInches - 2 * fromTop) / cyImageInches;

         newFromLeft = cxDocumentInches / 2.0 - cxImageInches * scale / 2.0;
         newFromTop = cyDocumentInches - (cyDocumentInches / 2.0 + cyImageInches * scale / 2.0);

      }

   } else {

      scale = (cxDocumentInches - 2 * fromLeft) / cxImageInches;

      newFromLeft = cxDocumentInches / 2.0 - cxImageInches * scale / 2.0;
      newFromTop = cyDocumentInches / 2.0 - cyImageInches * scale / 2.0;

      if ( 0.0 > cyDocumentInches ) {

         scale = (cyDocumentInches - 2 * fromTop) / cyImageInches;

         newFromLeft = cxDocumentInches / 2.0 - cxImageInches * scale / 2.0;
         newFromTop = cyDocumentInches / 2.0 - cyImageInches * scale / 2.0;

      }

   }

   scale = (double)cxPage / (double)pImage -> GetWidth();

   scale *= (cxDocumentInches - 2 * newFromLeft) / cxDocumentInches;

   delete pImage;

   Gdiplus::GdiplusShutdown(gdiplusToken);

   return AddImageFromFile(newFromLeft,newFromTop,scale,scale,bstrFileName);
   }


   HRESULT PdfPage::get_PageNumber(long *pn) {
   if ( ! pn )
      return E_POINTER;
   *pn = pageNumber;
   return S_OK;
   }


   HRESULT PdfPage::ParseText(HDC hdc,RECT *prcWindowsClip,void *pvIPostScriptTakeText) {
   return parseText(hdc,prcWindowsClip,pvIPostScriptTakeText);
   }


   HRESULT PdfPage::GetNativePdfPage(void **ppvNativePdfPage) {
   *ppvNativePdfPage = reinterpret_cast<void *>(this);
   return S_OK;
   }

   HRESULT PdfPage::GetLastError(char **ppszError) {
   if ( ! ppszError )
      return E_POINTER;
   *ppszError = szErrorMessage;
   return S_OK;
   }


   HRESULT PdfPage::AddBinaryObjectFromFile(BSTR objectName,BSTR fileName) {

   FILE *fObject = _wfopen(fileName,L"rb");

   if ( ! fObject )
      return E_FAIL;

   fseek(fObject,0,SEEK_END);

   long objectSize = ftell(fObject);

   fseek(fObject,0,SEEK_SET);

   BYTE *pData = new BYTE[objectSize];

   fread(pData,1,objectSize,fObject);

   fclose(fObject);

   BYTE *pResult = NULL;

   long compressedDataSize = pdfUtility.deflate(pData,objectSize,&pResult,1,0,0,0);

   delete [] pData;

   long objectId = pDocument -> NewObjectId();

   long dataSize = compressedDataSize + 1024;

   BYTE *pNewData = new BYTE[dataSize];

   memset(pNewData,0,dataSize * sizeof(BYTE));

   char szName[128];

   WideCharToMultiByte(CP_ACP,0,objectName,-1,szName,128,0,0);

   long streamLength = sprintf((char *)pNewData,"<</EmbeddedObject %s/Filter/FlateDecode/DecodeParms<</Predictor 1/Length %ld>>>>\nstream\n",szName,compressedDataSize);

   memcpy(pNewData + streamLength,pResult,compressedDataSize);

   streamLength += compressedDataSize;

   streamLength += sprintf((char *)(pNewData + streamLength),"\nendstream\nendobj\n");

   PdfObject *pNewObject = new PdfObject(pDocument,objectId,0,pNewData,streamLength);

   pNewObject -> Stream() -> SetCompressed();

   pDocument -> AddObject(pNewObject);

   if ( pDocument -> XReference() )
      pDocument -> XReference() -> AddObject(pNewObject);

   return S_OK;
   }


   HRESULT PdfPage::GetBinaryObjectToFile(BSTR objectName,BSTR fileName) {

   char szName[128];

   WideCharToMultiByte(CP_ACP,0,objectName,-1,szName,128,0,0);

   PdfObject *pObject = pDocument -> FindObjectByNamedDictionaryEntry("EmbeddedObject",szName);

   if ( ! pObject )
      return E_FAIL;

   FILE *fData = _wfopen(fileName,L"wb");
   if ( ! fData )
      return E_FAIL;

   PdfStream *pStream = pObject -> Stream();

   if ( ! pStream ) {
      fclose(fData);
      return E_FAIL;
   }

   pStream -> Decompress();

   fwrite(pStream -> Storage(),1,pStream -> BinaryDataSize(),fData);

   fclose(fData);

   return S_OK;
   }