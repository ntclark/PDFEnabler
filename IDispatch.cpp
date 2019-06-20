// Copyright 2017, 2018, 2019 InnoVisioNate Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <tchar.h>
#include <olectl.h>

#include "pdfEnabler.h"

   HRESULT __stdcall PdfEnabler::QueryInterface(REFIID riid, void** ppv) {
   *ppv = 0;

   if ( IID_IUnknown == riid )
      *ppv = this;
   else

      if ( IID_IPdfEnabler == riid )
         *ppv = static_cast<IPdfEnabler *>(this);
      else

#if 0
      if ( riid == IID_IPersistStream )
         return pIGProperties -> QueryInterface(riid,ppv);
      else

      if ( riid == IID_IPersistStorage )
         return pIGProperties -> QueryInterface(riid,ppv);
      else

      if ( riid == IID_IHERSEvents )
         *ppv = static_cast<IHERSEvents *>(pHERSEvents);

      else
#endif
         return E_NOINTERFACE;

   AddRef();

   return S_OK;
   }


   ULONG __stdcall PdfEnabler::AddRef() {
   return refCount++;
   }


   ULONG __stdcall PdfEnabler::Release() { 
   if ( --refCount == 0 ) {
      delete this;
      return 0;
   }
   return refCount;
   }


   // IDispatch

   STDMETHODIMP PdfEnabler::GetTypeInfoCount(UINT * pctinfo) { 
   *pctinfo = 1;
   return S_OK;
   } 


   long __stdcall PdfEnabler::GetTypeInfo(UINT itinfo,LCID lcid,ITypeInfo **pptinfo) { 
   *pptinfo = NULL; 
   if ( itinfo != 0 ) 
      return DISP_E_BADINDEX; 
   *pptinfo = pITypeInfo;
   pITypeInfo -> AddRef();
   return S_OK; 
   } 
 

   STDMETHODIMP PdfEnabler::GetIDsOfNames(REFIID riid,OLECHAR** rgszNames,UINT cNames,LCID lcid, DISPID* rgdispid) { 
   return DispGetIDsOfNames(pITypeInfo,rgszNames,cNames,rgdispid);
   }


   STDMETHODIMP PdfEnabler::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, 
                                           WORD wFlags,DISPPARAMS FAR* pdispparams, VARIANT FAR* pvarResult,
                                           EXCEPINFO FAR* pexcepinfo, UINT FAR* puArgErr) { 
   return DispInvoke(this,pITypeInfo,dispidMember,wFlags,pdispparams,pvarResult,pexcepinfo,puArgErr); 
   }


