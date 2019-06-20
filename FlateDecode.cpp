// Copyright 2017, 2018, 2019 InnoVisioNate Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Library.h"
#include <math.h>

#include "zlib.h"

   long PdfUtility::deflate(BYTE *pInitialSource,long initialSize,BYTE **ppResult,
                                 long predictor,long columns,long bitsPerComponent,long componentsPerSample) {

   if ( 1L == predictor ) {

      BYTE *pDeflatorOutput = new BYTE[DEFLATOR_CHUNK_SIZE];

      FILE *fOverFlow = NULL;

      char szOverFlowName[MAX_PATH];

      z_stream stream = {0};

      deflateInit(&stream,Z_DEFAULT_COMPRESSION);

      BYTE *pSource = pInitialSource;
      long sizeLeft = initialSize;

      stream.next_in = pSource;
      stream.avail_in = sizeLeft;

      stream.next_out = pDeflatorOutput;
      stream.avail_out = DEFLATOR_CHUNK_SIZE;

      long lastTotalOut = 0L;

      do { 

         if ( 0 == stream.avail_out ) {

            if ( ! fOverFlow ) {
               sprintf(szOverFlowName,_tempnam(NULL,NULL));
               fOverFlow = fopen(szOverFlowName,"wb");
            }

            fwrite(pDeflatorOutput,stream.total_out - lastTotalOut,1,fOverFlow);

            lastTotalOut = stream.total_out;

            pSource += DEFLATOR_CHUNK_SIZE;
            sizeLeft -= DEFLATOR_CHUNK_SIZE;

            stream.next_out = pDeflatorOutput;
            stream.avail_out = DEFLATOR_CHUNK_SIZE;

         }

         ::deflate(&stream,Z_SYNC_FLUSH);

      } while ( 0 == stream.avail_out );

      if ( fOverFlow ) {
         if ( stream.avail_out )
            fwrite(pDeflatorOutput,stream.total_out - lastTotalOut,1,fOverFlow);
         fclose(fOverFlow);
         delete [] pDeflatorOutput;
         pDeflatorOutput = new BYTE[stream.total_out];
         fOverFlow = fopen(szOverFlowName,"rb");
         fread(pDeflatorOutput,stream.total_out,1,fOverFlow);
         fclose(fOverFlow);
         DeleteFile(szOverFlowName);
      }

      *ppResult = pDeflatorOutput;

      deflateEnd(&stream);

      return stream.total_out;

   }

/*
      6.3. Filter type 1: Sub

      The Sub() filter transmits the difference between each byte and the value of the 
      corresponding byte of the prior pixel.
      To compute the Sub() filter, apply the following formula to each byte of the scanline:

         Sub(x) = Raw(x) - Raw(x-bpp)

      where x ranges from zero to the number of bytes representing the scanline minus one, 
      Raw() refers to the raw data byte at that byte position in the scanline, and bpp is 
      defined as the number of bytes per complete pixel, rounding up to one. For example, 
      for color type 2 with a bit depth of 16, bpp is equal to 6 (three samples, two 
      bytes per sample); for color type 0 with a bit depth of 2, bpp is equal to 1 (rounding up); 
      for color type 4 with a bit depth of 16, bpp is equal to 4 (two-byte grayscale sample, 
      plus two-byte alpha sample).
      
      Note this computation is done for each byte, regardless of bit depth. In a 16-bit image, 
      each MSB is predicted from the preceding MSB and each LSB from the preceding LSB, because 
      of the way that bpp is defined.

      Unsigned arithmetic modulo 256 is used, so that both the inputs and outputs fit into bytes. 
      The sequence of Sub values is transmitted as the filtered scanline.

      For all x < 0, assume Raw(x) = 0.

      To reverse the effect of the Sub() filter after decompression, output the following value:

         Sub(x) + Raw(x-bpp)

      (computed mod 256), where Raw() refers to the bytes already decoded.

      6.4. Filter type 2: Up

      The Up() filter is just like the Sub() filter except that the pixel immediately above the current pixel, rather than just to its left, 
      is used as the predictor.
      To compute the Up() filter, apply the following formula to each byte of the scanline:

         Up(x) = Raw(x) - Prior(x)

      where x ranges from zero to the number of bytes representing the scanline minus one, Raw() refers to the raw data 
      byte at that byte position in the scanline, and Prior(x) refers to the unfiltered bytes of the prior scanline.
      Note this is done for each byte, regardless of bit depth. Unsigned arithmetic modulo 256 is used, so that both the 
      inputs and outputs fit into bytes. The sequence of Up values is transmitted as the filtered scanline.
      On the first scanline of an image (or of a pass of an interlaced image), assume Prior(x) = 0 for all x.
      To reverse the effect of the Up() filter after decompression, output the following value:

         Up(x) + Prior(x)

      (computed mod 256), where Prior() refers to the decoded bytes of the prior scanline.

      6.5. Filter type 3: Average

      The Average() filter uses the average of the two neighboring pixels (left and above) to predict the value of a pixel.

      To compute the Average() filter, apply the following formula to each byte of the scanline:

         Average(x) = Raw(x) - floor((Raw(x-bpp)+Prior(x))/2)

      where x ranges from zero to the number of bytes representing the scanline minus one, Raw() refers to the raw data byte
      at that byte position in the scanline, Prior() refers to the unfiltered bytes of the prior scanline, and bpp is 
      defined as for the Sub() filter.

      Note this is done for each byte, regardless of bit depth. The sequence of Average values is transmitted as the filtered scanline.

      The subtraction of the predicted value from the raw byte must be done modulo 256, so that both the inputs and outputs
      fit into bytes. However, the sum Raw(x-bpp)+Prior(x) must be formed without overflow (using at least nine-bit arithmetic). 
      floor() indicates that the result of the division is rounded to the next lower integer if fractional; in other words, 
      it is an integer division or right shift operation.

      For all x < 0, assume Raw(x) = 0. On the first scanline of an image (or of a pass of an interlaced image), assume Prior(x) = 0 for all x.

      To reverse the effect of the Average() filter after decompression, output the following value:

         Average(x) + floor((Raw(x-bpp) + Prior(x))/2)

      where the result is computed mod 256, but the prediction is calculated in the same way as for encoding. Raw() refers 
      to the bytes already decoded, and Prior() refers to the decoded bytes of the prior scanline.

      6.6. Filter type 4: Paeth

      The Paeth() filter computes a simple linear function of the three neighboring pixels (left, above, upper left), then 
      chooses as predictor the neighboring pixel closest to the computed value. This technique is due to Alan W. Paeth [PAETH].

      To compute the Paeth() filter, apply the following formula to each byte of the scanline:

         Paeth(x) = Raw(x) - PaethPredictor(Raw(x-bpp), Prior(x), Prior(x-bpp))

      where x ranges from zero to the number of bytes representing the scanline minus one, Raw() refers to the raw data byte 
      at that byte position in the scanline, Prior() refers to the unfiltered bytes of the prior scanline, and bpp is defined 
      as for the Sub() filter.

      Note this is done for each byte, regardless of bit depth. Unsigned arithmetic modulo 256 is used, so that both the 
      inputs and outputs fit into bytes. The sequence of Paeth values is transmitted as the filtered scanline.

      The PaethPredictor() function is defined by the following pseudocode:

         function PaethPredictor (a, b, c)
         begin
            ; a = left, b = above, c = upper left
            p := a + b - c        ; initial estimate
            pa := abs(p - a)      ; distances to a, b, c
            pb := abs(p - b)
            pc := abs(p - c)
            ; return nearest of a,b,c,
            ; breaking ties in order a,b,c.
            if pa <= pb AND pa <= pc then return a
            else if pb <= pc then return b
            else return c
         end

      The calculations within the PaethPredictor() function must be performed exactly, without overflow. Arithmetic modulo 256 is to 
      be used only for the final step of subtracting the function result from the target byte value.

      Note that the order in which ties are broken is critical and must not be altered. The tie break order is: pixel to the left, 
      pixel above, pixel to the upper left. (This order differs from that given in Paeth's article.)

      For all x < 0, assume Raw(x) = 0 and Prior(x) = 0. On the first scanline of an image (or of a pass of an interlaced image), 
      assume Prior(x) = 0 for all x.

      To reverse the effect of the Paeth() filter after decompression, output the following value:

         Paeth(x) + PaethPredictor(Raw(x-bpp), Prior(x), Prior(x-bpp))

      (computed mod 256), where Raw() and Prior() refer to bytes already decoded. Exactly the same PaethPredictor() function is used by 
      both encoder and decoder.

*/

   long rowSize = columns * bitsPerComponent * componentsPerSample / 8;

   long rows = initialSize / rowSize;
   if ( rows * rowSize < initialSize )
      rows++;

   long predictorSize = initialSize + rows;

   predictorSize += (initialSize % rowSize) * rowSize;

   BYTE *pDeflatorInput = new BYTE[predictorSize];

   memset(pDeflatorInput,0,predictorSize);

   BYTE *pSource = pInitialSource;
   BYTE *pEnd = pSource + initialSize;
   BYTE *pTarget = pDeflatorInput;

   short pb = (short)(predictor > 9 ? predictor - 10 : predictor);
   BYTE predictorByte = (BYTE)pb;

   BYTE *pPriorRow = new BYTE[rowSize];

   memset(pPriorRow,0,rowSize);

   while ( pSource < pEnd ) {

      *pTarget = predictorByte;

      pTarget++;

      for ( long k = 0; k < rowSize; k++ ) {

         switch ( predictor ) {
         case 10:
            pTarget[k] = pSource[k];
            break;

         case 11:
            if ( k > componentsPerSample - 1 )
               pTarget[k] = pSource[k] - pSource[k - componentsPerSample];
            else
               pTarget[k] = pSource[k];
            break;

         case 12:
         case 15:
            pTarget[k] = pSource[k] - pPriorRow[k];
            pPriorRow[k] = pSource[k];
            break;

         case 13: {
            double op2 = 0.0;
            if ( k > componentsPerSample - 1 )
               op2 = pSource[k - componentsPerSample];
            pTarget[k] = pSource[k] - (BYTE)floor((pPriorRow[k] + op2) / 2.0);
            pPriorRow[k] = pSource[k];
            }
            break;

         case 14: {
            long a = 0;
            long b = pPriorRow[k];
            long c = 0;
            if ( k > componentsPerSample - 1 ) {
               a = pSource[k - componentsPerSample];
               c = pPriorRow[k - componentsPerSample];
            }
            pTarget[k] = pSource[k] - (BYTE)PaethPredictor(a,b,c);
            pPriorRow[k] = pSource[k];
            }
            break;

         }
      }

      pTarget += rowSize;

      pSource += rowSize;

   }
   
   if ( pSource > pEnd )
      pTarget -= (pSource - pEnd);

   long sizeLeft = (long)(pTarget - pDeflatorInput);

   long rc = deflate(pDeflatorInput,sizeLeft,ppResult);

   delete [] pDeflatorInput;

   delete [] pPriorRow;

   return rc;
   }


   long PdfUtility::PaethPredictor(long a, long b, long c) {
   long p = a + b - c;
   long pa = abs(p - a);
   long pb = abs(p - b);
   long pc = abs(p - c);
   if ( pa <= pb && pa <= pc )
      return a;
   if ( pb <= pc )
      return b;
   return c;
   }


   long PdfUtility::inflate(BYTE *pInitialSource,long initialSize,BYTE **ppResult,
                                 long predictor,long columns,long rows,long bitsPerComponent,long componentsPerSample) {

   BYTE *pDeflatorOutput = new BYTE[DEFLATOR_CHUNK_SIZE];

   memset(pDeflatorOutput,0,DEFLATOR_CHUNK_SIZE * sizeof(BYTE));

   long outputSize = DEFLATOR_CHUNK_SIZE;

   z_stream stream = {0};

   inflateInit(&stream);

   stream.next_in = pInitialSource;
   stream.avail_in = initialSize;

   FILE *fOverFlow = NULL;
   long totalSize = 0;
   char szOverFlowName[MAX_PATH];

   do { 

      stream.next_out = pDeflatorOutput;
      stream.avail_out = DEFLATOR_CHUNK_SIZE;

      if ( Z_STREAM_END == ::inflate(&stream,Z_NO_FLUSH) )
         break;

      if ( 0 == stream.avail_out ) {
         
         totalSize += DEFLATOR_CHUNK_SIZE;

         if ( ! fOverFlow ) {
            sprintf(szOverFlowName,_tempnam(NULL,NULL));
            fOverFlow = fopen(szOverFlowName,"wb");
         }

         fwrite(pDeflatorOutput,DEFLATOR_CHUNK_SIZE,1,fOverFlow);

      }

   } while ( 0 == stream.avail_out );

   outputSize = DEFLATOR_CHUNK_SIZE - stream.avail_out;

   if ( fOverFlow ) {
      totalSize += outputSize;
      fwrite(pDeflatorOutput,1,outputSize,fOverFlow);
      fclose(fOverFlow);
      delete [] pDeflatorOutput;
      pDeflatorOutput = new BYTE[totalSize];
      fOverFlow = fopen(szOverFlowName,"rb");
      fread(pDeflatorOutput,totalSize,1,fOverFlow);
      fclose(fOverFlow);
      DeleteFile(szOverFlowName);
      outputSize = totalSize;
   }

   if ( 1L == predictor ) {
      *ppResult = pDeflatorOutput;
      return outputSize;
   }

   long rowSize = columns * bitsPerComponent * componentsPerSample / 8;

   BYTE *pOutputStart = new BYTE[outputSize];

   memset(pOutputStart,0,outputSize);

   BYTE *pInput = pDeflatorOutput;
   BYTE *pEnd = pInput + outputSize;
   BYTE *pOutput = pOutputStart;

   BYTE *pPriorRow = new BYTE[rowSize + 1];
   BYTE predictorByte;

   memset(pPriorRow,0,rowSize + 1);

   while ( pInput < pEnd ) {

      predictorByte = *pInput;

      pInput++;

      for ( long k = 0; k < rowSize; k++ ) {
         switch ( predictorByte ) {
         case 0x01:
            if ( k > componentsPerSample - 1 )
               pOutput[k] = pInput[k] + pOutput[k - componentsPerSample];
            else
               pOutput[k] = pInput[k];
            break;
         case 0x02:
            pOutput[k] = pInput[k] + pPriorRow[k];
            pPriorRow[k] = pOutput[k];
            break;
         case 0x03: {
            double op2 = 0.0;
            if ( k > componentsPerSample - 1 )
               op2 = pOutput[k - componentsPerSample];
            pOutput[k] = pInput[k] + (BYTE)floor((pPriorRow[k] + op2) / 2.0);
            pPriorRow[k] = pOutput[k];
            }
            break;
         case 0x04: {
            long a = 0;
            long b = pPriorRow[k];
            long c = 0;
            if ( k > componentsPerSample - 1 ) {
               a = pOutput[k - componentsPerSample];
               c = pPriorRow[k - componentsPerSample];
            }
            pOutput[k] = pInput[k] + (BYTE)PaethPredictor(a,b,c);
            pPriorRow[k] = pOutput[k];
            }
            break;
         }
      }

      pInput += rowSize;
      pOutput += rowSize;

   }

   if ( pInput > pEnd )
      pOutput -= (pInput - pEnd);

   *ppResult = pOutputStart;

   delete [] pDeflatorOutput;
   delete [] pPriorRow;

   return (long)(pOutput - pOutputStart);
   }


