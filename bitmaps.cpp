
#include "Library.h"

   BYTE *BitmapBits(HBITMAP hBitmap,long *cntBytes,long *pWidth,long *pHeight) {

   HDC hdcSource = CreateCompatibleDC(NULL);

   BITMAP bitMap;

   GetObject(hBitmap,sizeof(BITMAP),&bitMap);

   *pWidth = bitMap.bmWidth;
   *pHeight = bitMap.bmHeight;

   long colorTableSize = (long)sizeof(RGBQUAD) * ( 1 << (bitMap.bmPlanes * bitMap.bmBitsPixel) );

   long entireSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + colorTableSize;

   BYTE *pBuffer = new BYTE[entireSize];

   memset(pBuffer,0,entireSize * sizeof(BYTE));

   BITMAPFILEHEADER *pBitmapFileHeader = (BITMAPFILEHEADER *)pBuffer;

   BITMAPINFO *pBitmapInfo = (BITMAPINFO *)(pBuffer + sizeof(BITMAPFILEHEADER));

   BITMAPINFOHEADER *pBitmapInfoHeader = &(pBitmapInfo -> bmiHeader);
   
   pBitmapInfoHeader -> biSize = sizeof(BITMAPINFOHEADER); 
   pBitmapInfoHeader -> biWidth = bitMap.bmWidth;
   pBitmapInfoHeader -> biHeight = -bitMap.bmHeight;
   pBitmapInfoHeader -> biPlanes = bitMap.bmPlanes; 
   pBitmapInfoHeader -> biBitCount = 32;
   pBitmapInfoHeader -> biCompression = BI_RGB;

   if ( 1 == bitMap.bmBitsPixel ) {
      pBitmapInfoHeader -> biClrUsed = 2;
      pBitmapInfoHeader -> biClrImportant = 2;
   } else {
      pBitmapInfoHeader -> biClrUsed = 0;
      pBitmapInfoHeader -> biClrImportant = 0;
   }

   pBitmapInfoHeader -> biSizeImage = abs(pBitmapInfoHeader -> biHeight) * ((pBitmapInfoHeader -> biWidth * pBitmapInfoHeader -> biPlanes * pBitmapInfoHeader -> biBitCount + 31) & ~31) / 8 ;

   BYTE *pBits = new BYTE[pBitmapInfoHeader -> biSizeImage];

   memset(pBits,0,pBitmapInfoHeader -> biSizeImage);

   long rc = GetDIBits(hdcSource,hBitmap,0,bitMap.bmHeight,pBits,pBitmapInfo,DIB_RGB_COLORS);

   long k = 0;
   long j = 0;

   *cntBytes = 3 * pBitmapInfoHeader -> biSizeImage / 4;

   BYTE *pResult = new BYTE[*cntBytes];

   BYTE bFirst = pBits[0];

   for ( long y = 0; y < bitMap.bmHeight; y++ ) {
      for ( long x = 0; x < bitMap.bmWidth; x++ ) {
         pResult[k++] = pBits[j + 2];
         pResult[k++] = pBits[j + 1];
         pResult[k++] = pBits[j];
         j += 4;
      }
   }

   pResult[2] = bFirst;

   delete [] pBits;

   DeleteDC(hdcSource);

   return pResult;
   }


   void SaveBitmapFile(HDC hdcSource,HBITMAP hBitmap,char *pszBitmapFileName) {

   BITMAP bitMap;

   GetObject(hBitmap,sizeof(BITMAP),&bitMap);

   long colorTableSize = (long)sizeof(RGBQUAD) * ( 1 << (bitMap.bmPlanes * bitMap.bmBitsPixel) );

   long entireSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + colorTableSize;

   BYTE *pBuffer = new BYTE[entireSize];

   memset(pBuffer,0,entireSize * sizeof(BYTE));

   BITMAPFILEHEADER *pBitmapFileHeader = (BITMAPFILEHEADER *)pBuffer;

   BITMAPINFO *pBitmapInfo = (BITMAPINFO *)(pBuffer + sizeof(BITMAPFILEHEADER));

   BITMAPINFOHEADER *pBitmapInfoHeader = &(pBitmapInfo -> bmiHeader);
   
   pBitmapInfoHeader -> biSize = sizeof(BITMAPINFOHEADER); 
   pBitmapInfoHeader -> biWidth = bitMap.bmWidth;
   pBitmapInfoHeader -> biHeight = bitMap.bmHeight;
   pBitmapInfoHeader -> biPlanes = bitMap.bmPlanes; 
   pBitmapInfoHeader -> biBitCount = bitMap.bmBitsPixel;
   pBitmapInfoHeader -> biCompression = BI_RGB;
   if ( 1 == bitMap.bmBitsPixel ) {
      pBitmapInfoHeader -> biClrUsed = 2;
      pBitmapInfoHeader -> biClrImportant = 2;
   } else {
      pBitmapInfoHeader -> biClrUsed = 0;
      pBitmapInfoHeader -> biClrImportant = 0;
   }

   pBitmapInfoHeader -> biSizeImage = bitMap.bmHeight * ((bitMap.bmWidth * bitMap.bmPlanes * bitMap.bmBitsPixel + 31) & ~31) / 8 ;

   BYTE *pBits = new BYTE[pBitmapInfoHeader -> biSizeImage];

   memset(pBits,0,pBitmapInfoHeader -> biSizeImage);

   long rc = GetDIBits(hdcSource,hBitmap,0,bitMap.bmHeight,pBits,pBitmapInfo,DIB_RGB_COLORS);

   pBitmapFileHeader -> bfType = 0x4d42;

   pBitmapFileHeader -> bfSize = entireSize + pBitmapInfoHeader -> biSizeImage;

   pBitmapFileHeader -> bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + colorTableSize;

   FILE *fBitmap = fopen(pszBitmapFileName,"wb");

   fwrite(pBitmapFileHeader,sizeof(BITMAPFILEHEADER),1,fBitmap);

   fwrite(pBitmapInfoHeader,sizeof(BITMAPINFOHEADER),1,fBitmap);

   fwrite(pBitmapInfo -> bmiColors,colorTableSize,1,fBitmap);

   fwrite(pBits,pBitmapInfoHeader -> biSizeImage,1,fBitmap);

   fclose(fBitmap);

   delete [] pBuffer;
   delete [] pBits;

   return;
   }