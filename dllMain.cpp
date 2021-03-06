// Copyright 2017, 2018, 2019 InnoVisioNate Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlwapi.h>
#include <ShlObj.h>

#include <olectl.h>
#include <stdio.h>

#define OBJECT_NAME "InnoVisioNate.PdfEnabler"
#define OBJECT_NAME_V "InnoVisioNate.PdfEnabler.1"
#define OBJECT_VERSION "1.0"
#define OBJECT_CLSID CLSID_PdfEnabler
#define OBJECT_LIBID LIBID_PdfEnabler

#define DEFINE_DATA

#include "pdfEnabler.h"

   OLECHAR wstrModuleName[256];

   extern "C" int GetCommonAppDataLocation(HWND hwnd,char *);
   extern "C" int GetLocation(HWND hwnd,long key,char *szFolderLocation);

   extern "C" BOOL WINAPI DllMain(HINSTANCE hI, DWORD dwReason, LPVOID) {

   switch ( dwReason ) {

   case DLL_PROCESS_ATTACH: {

      hModule = reinterpret_cast<HMODULE>(hI);

      GetModuleFileName(hModule,szModuleName,1024);
      memset(wstrModuleName,0,sizeof(wstrModuleName));
      MultiByteToWideChar(CP_ACP, 0, szModuleName, -1, wstrModuleName, 256);  

      }

      break;
  
   case DLL_PROCESS_DETACH:
      break;
  
   }
  
   return TRUE;
   }
  

   extern "C" int __cdecl GetCommonAppDataLocation(HWND hwnd,char *szFolderLocation) {
   GetLocation(hwnd,CSIDL_COMMON_APPDATA,szFolderLocation);
   return 0;
   }

   extern "C" int GetLocation(HWND hwnd,long key,char *szFolderLocation) {

   ITEMIDLIST *ppItemIDList;
   IShellFolder *pIShellFolder;
   LPCITEMIDLIST pcParentIDList;

   HRESULT wasInitialized = CoInitialize(NULL);

   szFolderLocation[0] = '\0';

   HRESULT rc = SHGetFolderLocation(hwnd,key,NULL,0,&ppItemIDList);

   if ( S_OK != rc ) {
      char szMessage[256];
      sprintf(szMessage,"SHGetFolderLocation returned %ld",rc);
      MessageBox(NULL,szMessage,"Error",MB_OK);
      szFolderLocation[0] = '\0';
      return 0;
   }

   rc = SHBindToParent(ppItemIDList, IID_IShellFolder, (void **) &pIShellFolder, &pcParentIDList);

   if ( S_OK == rc ) {

      STRRET strRet;
      rc = pIShellFolder -> GetDisplayNameOf(pcParentIDList,SHGDN_FORPARSING,&strRet);
      pIShellFolder -> Release();
      if ( S_OK == rc ) {
         WideCharToMultiByte(CP_ACP,0,strRet.pOleStr,-1,szFolderLocation,MAX_PATH,0,0);
      } else {
         char szMessage[256];
         sprintf(szMessage,"GetDisplayNameOf returned %ld",rc);
         MessageBox(NULL,szMessage,"Error",MB_OK);
         szFolderLocation[0] = '\0';
         return 0;
      }
   } else {
      char szMessage[256];
      sprintf(szMessage,"SHBindToParent returned %ld",rc);
      MessageBox(NULL,szMessage,"Error",MB_OK);
      szFolderLocation[0] = '\0';
      return 0;
   }

   if ( S_FALSE == wasInitialized )
      CoUninitialize();

   return 0;
   }
  
   class Factory : public IClassFactory {
   public:
  
      STDMETHOD (QueryInterface)(REFIID riid,void **ppv);
      STDMETHOD_ (ULONG, AddRef)();
      STDMETHOD_ (ULONG, Release)();
      STDMETHOD (CreateInstance)(IUnknown *punkOuter, REFIID riid, void **ppv);
      STDMETHOD (LockServer)(BOOL fLock);
  
      Factory() : refCount(0) {};
      ~Factory() {};
  
   private:
      int refCount;
   };
  
  
   static Factory factory;
  
   STDAPI DllCanUnloadNow(void) {
   return S_OK;
   }
  
  
  
   STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppObject) {
   *ppObject = NULL;
   if ( rclsid != OBJECT_CLSID ) 
      return CLASS_E_CLASSNOTAVAILABLE;
   return factory.QueryInterface(riid,ppObject);
   }
  
  
   STDAPI DllRegisterServer() {

   HRESULT rc = S_OK;
   ITypeLib *ptLib;
   HKEY keyHandle,clsidHandle;
   DWORD disposition;
   char szTemp[256],szCLSID[256];
   LPOLESTR oleString;
  
   StringFromCLSID(OBJECT_CLSID,&oleString);
   WideCharToMultiByte(CP_ACP,0,oleString,-1,szCLSID,256,0,0);
  
   if ( S_OK != LoadTypeLib(wstrModuleName,&ptLib) )
      rc = ResultFromScode(SELFREG_E_TYPELIB);
   else
      if ( S_OK != RegisterTypeLib(ptLib,wstrModuleName,NULL) )
         rc = ResultFromScode(SELFREG_E_TYPELIB);

   RegOpenKeyEx(HKEY_CLASSES_ROOT,"CLSID",0,KEY_CREATE_SUB_KEY,&keyHandle);
  
      RegCreateKeyEx(keyHandle,szCLSID,0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&clsidHandle,&disposition);
      sprintf(szTemp,"InnoVisioNate Pdf Enabler Object");
      RegSetValueEx(clsidHandle,NULL,0,REG_SZ,(BYTE *)szTemp,(DWORD)strlen(szTemp));
  
      sprintf(szTemp,"Control");
      RegCreateKeyEx(clsidHandle,szTemp,0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&keyHandle,&disposition);
      sprintf(szTemp,"");
      RegSetValueEx(keyHandle,NULL,0,REG_SZ,(BYTE *)szTemp,(DWORD)strlen(szTemp));
  
      sprintf(szTemp,"ProgID");
      RegCreateKeyEx(clsidHandle,szTemp,0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&keyHandle,&disposition);
      sprintf(szTemp,OBJECT_NAME_V);
      RegSetValueEx(keyHandle,NULL,0,REG_SZ,(BYTE *)szTemp,(DWORD)strlen(szTemp));
  
      sprintf(szTemp,"InprocServer");
      RegCreateKeyEx(clsidHandle,szTemp,0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&keyHandle,&disposition);
      RegSetValueEx(keyHandle,NULL,0,REG_SZ,(BYTE *)szModuleName,(DWORD)strlen(szModuleName));
  
      sprintf(szTemp,"InprocServer32");
      RegCreateKeyEx(clsidHandle,szTemp,0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&keyHandle,&disposition);
      RegSetValueEx(keyHandle,NULL,0,REG_SZ,(BYTE *)szModuleName,(DWORD)strlen(szModuleName));
//      RegSetValueEx(keyHandle,"ThreadingModel",0,REG_SZ,(BYTE *)"Free",5);
      RegSetValueEx(keyHandle,"ThreadingModel",0,REG_SZ,(BYTE *)"Apartment",9);
  
      sprintf(szTemp,"LocalServer");
      RegCreateKeyEx(clsidHandle,szTemp,0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&keyHandle,&disposition);
      RegSetValueEx(keyHandle,NULL,0,REG_SZ,(BYTE *)szModuleName,(DWORD)strlen(szModuleName));
    
      sprintf(szTemp,"TypeLib");
      RegCreateKeyEx(clsidHandle,szTemp,0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&keyHandle,&disposition);
    
      StringFromCLSID(OBJECT_LIBID,&oleString);
      WideCharToMultiByte(CP_ACP,0,oleString,-1,szTemp,256,0,0);
      RegSetValueEx(keyHandle,NULL,0,REG_SZ,(BYTE *)szTemp,(DWORD)strlen(szTemp));
        
      sprintf(szTemp,"ToolboxBitmap32");
      RegCreateKeyEx(clsidHandle,szTemp,0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&keyHandle,&disposition);
//      sprintf(szTemp,"%s, 1",szModuleName);
//      RegSetValueEx(keyHandle,NULL,0,REG_SZ,(BYTE *)szTemp,strlen(szModuleName));
  
      sprintf(szTemp,"Version");
      RegCreateKeyEx(clsidHandle,szTemp,0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&keyHandle,&disposition);
      sprintf(szTemp,OBJECT_VERSION);
      RegSetValueEx(keyHandle,NULL,0,REG_SZ,(BYTE *)szTemp,(DWORD)strlen(szTemp));
  
      sprintf(szTemp,"MiscStatus");
      RegCreateKeyEx(clsidHandle,szTemp,0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&keyHandle,&disposition);
      sprintf(szTemp,"0");
      RegSetValueEx(keyHandle,NULL,0,REG_SZ,(BYTE *)szTemp,(DWORD)strlen(szTemp));
  
      sprintf(szTemp,"1");
      RegCreateKeyEx(keyHandle,szTemp,0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&keyHandle,&disposition);
      sprintf(szTemp,"%ld",
                 OLEMISC_ALWAYSRUN |
                 OLEMISC_ACTIVATEWHENVISIBLE | 
                 OLEMISC_RECOMPOSEONRESIZE | 
                 OLEMISC_INSIDEOUT |
                 OLEMISC_SETCLIENTSITEFIRST |
                 OLEMISC_CANTLINKINSIDE );
//sprintf(szTemp,"%ld",131473L);
      RegSetValueEx(keyHandle,NULL,0,REG_SZ,(BYTE *)szTemp,(DWORD)strlen(szTemp));
  
   RegCreateKeyEx(HKEY_CLASSES_ROOT,OBJECT_NAME,0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&keyHandle,&disposition);
      RegCreateKeyEx(keyHandle,"CurVer",0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&keyHandle,&disposition);
      sprintf(szTemp,OBJECT_NAME_V);
      RegSetValueEx(keyHandle,NULL,0,REG_SZ,(BYTE *)szTemp,(DWORD)strlen(szTemp));
  
   RegCreateKeyEx(HKEY_CLASSES_ROOT,OBJECT_NAME_V,0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&keyHandle,&disposition);
      RegCreateKeyEx(keyHandle,"CLSID",0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&keyHandle,&disposition);
      RegSetValueEx(keyHandle,NULL,0,REG_SZ,(BYTE *)szCLSID,(DWORD)strlen(szCLSID));
  
   return S_OK;
   }
  
  
   STDAPI DllUnregisterServer() {

   HRESULT rc = S_OK;
   HKEY keyHandle;
   char szCLSID[256];
   LPOLESTR oleString;
  
   StringFromCLSID(OBJECT_CLSID,&oleString);
   WideCharToMultiByte(CP_ACP,0,oleString,-1,szCLSID,256,0,0);
  
   RegOpenKeyEx(HKEY_CLASSES_ROOT,"CLSID",0,KEY_CREATE_SUB_KEY,&keyHandle);

   rc = SHDeleteKey(keyHandle,szCLSID);

   rc = SHDeleteKey(HKEY_CLASSES_ROOT,OBJECT_NAME);

   rc = SHDeleteKey(HKEY_CLASSES_ROOT,OBJECT_NAME_V);

   return S_OK;
   }
  
  
   long __stdcall Factory::QueryInterface(REFIID iid, void **ppv) { 
   *ppv = NULL; 
   if ( iid == IID_IUnknown || iid == IID_IClassFactory ) 
      *ppv = this; 
   else 
      return E_NOINTERFACE; 
   AddRef(); 
   return S_OK; 
   } 
  
  
   unsigned long __stdcall Factory::AddRef() { 
   return ++refCount; 
   } 
  
  
   unsigned long __stdcall Factory::Release() { 
   return --refCount;
   } 
  
  
   HRESULT STDMETHODCALLTYPE Factory::CreateInstance(IUnknown *punkOuter, REFIID riid, void **ppv) { 
   HRESULT hres;
   *ppv = NULL; 
   PdfEnabler *pef = new PdfEnabler(punkOuter);
   hres = pef -> QueryInterface(riid,ppv);
   if ( !*ppv ) delete pef;
   return hres;
   } 
  
  
   long __stdcall Factory::LockServer(int fLock) { 
   return S_OK; 
   }