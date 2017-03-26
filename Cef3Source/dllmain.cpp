// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"

#include "Guids.h"
#include "Cef3Source.h"

//HINSTANCE g_hInst = NULL;

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	return DllEntryPoint((HINSTANCE)(hModule), ul_reason_for_call, lpReserved);
}

STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2( TRUE );

} // DllRegisterServer


STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2( FALSE );

} // DllUnregisterServer


// Note: It is better to register no media types than to register a partial 
// media type (subtype == GUID_NULL) because that can slow down intelligent connect 
// for everyone else.

// For a specialized source filter like this, it is best to leave out the 
// AMOVIESETUP_FILTER altogether, so that the filter is not available for 
// intelligent connect. Instead, use the CLSID to create the filter or just 
// use 'new' in your application.

const TCHAR* g_wszCefWebBrowser  =  TEXT("CEF WebBrowser Source");

// Filter setup data
const AMOVIESETUP_MEDIATYPE sudOpPinTypes =
{
    &MEDIATYPE_Video,       // Major type
    &MEDIASUBTYPE_ARGB32      // Minor type
};

const AMOVIESETUP_PIN sudOutputPinCefBrowser = 
{
    L"Video",      // Obsolete, not used.
    FALSE,          // Is this pin rendered?
    TRUE,           // Is it an output pin?
    FALSE,          // Can the filter create zero instances?
    FALSE,          // Does the filter create multiple instances?
    &CLSID_NULL,    // Obsolete.
    NULL,           // Obsolete.
    1,              // Number of media types.
    &sudOpPinTypes  // Pointer to media types.
};

const AMOVIESETUP_FILTER sudCefBrowser =
{
    &CLSID_CefWebBrowser,// Filter CLSID
    g_wszCefWebBrowser,       // String name
    MERIT_DO_NOT_USE,       // Filter merit
    1,                      // Number pins
    &sudOutputPinCefBrowser    // Pin details
};


// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance.
// We provide a set of filters in this one DLL.

CFactoryTemplate g_Templates[] = 
{
    { 
      g_wszCefWebBrowser,               // Name
      &CLSID_CefWebBrowser,       // CLSID
      CCefSource::CreateInstance, // Method to create an instance of MyComponent
      NULL,                           // Initialization function
      &sudCefBrowser           // Set-up information (for filters)
    }
};

int g_cTemplates = _countof(g_Templates); 