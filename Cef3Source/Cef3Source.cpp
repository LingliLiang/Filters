
#include "stdafx.h"
#include "Cef3Source.h"
#include "DibHelper.h"
#include "Guids.h"
#include <cassert>

// UNITS = 10 ^ 7  
// UNITS / 30 = 30 fps;
// UNITS / 20 = 20 fps, etc
const REFERENCE_TIME FPS_30 = UNITS / 30;
const REFERENCE_TIME FPS_20 = UNITS / 20;
const REFERENCE_TIME FPS_10 = UNITS / 10;
const REFERENCE_TIME FPS_5  = UNITS / 5;
const REFERENCE_TIME FPS_4  = UNITS / 4;
const REFERENCE_TIME FPS_3  = UNITS / 3;
const REFERENCE_TIME FPS_2  = UNITS / 2;
const REFERENCE_TIME FPS_1  = UNITS / 1;
// Default capture and display desktop 10 times per second
const REFERENCE_TIME rtDefaultFrameLength = FPS_10;

/**
*
*  CCefPushPin Class
*  
*
*/

CCefPushPin::CCefPushPin(HRESULT *phr, CSource *pFilter)
        : CSourceStream(NAME("Push Source Web"), phr, pFilter, L"Video"),
        m_FramesWritten(0),
        m_bZeroMemory(0),
        m_iFrameNumber(0),
        m_rtFrameLength(rtDefaultFrameLength), 
        m_nCurrentBitDepth(32),
		m_bGrabBuffer(FALSE),
		m_hThread(NULL),
		m_hNewGrabEvent(NULL),
		m_pBuffer(NULL)
{
    // Get the device context of the main display
    HDC hDC;
    hDC = CreateDC(TEXT("DISPLAY"), NULL, NULL, NULL);

    // Get the dimensions of the main desktop window
    m_rScreen.left   = m_rScreen.top = 0;
    m_rScreen.right  = GetDeviceCaps(hDC, HORZRES);
    m_rScreen.bottom = GetDeviceCaps(hDC, VERTRES);

    // Save dimensions for later use in FillBuffer()
    m_iImageWidth  = m_rScreen.right  - m_rScreen.left;
    m_iImageHeight = m_rScreen.bottom - m_rScreen.top;
	m_ulBufferSize = m_iImageWidth*m_iImageHeight*4;
	m_pBuffer = new BYTE[m_ulBufferSize];
	memset(m_pBuffer,0,m_ulBufferSize);
    // Release the device context
    DeleteDC(hDC);
	m_hNewRenderEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(m_hNewRenderEvent);
	::ResetEvent(m_hNewRenderEvent);
}

CCefPushPin::~CCefPushPin()
{   
    DbgLog((LOG_TRACE, 3, TEXT("Frames written %d"), m_iFrameNumber));
	::CloseHandle(m_hNewRenderEvent);
	m_hNewRenderEvent = NULL;
	if(m_pBuffer)
	{
		delete m_pBuffer;
		m_pBuffer = NULL;
	}
}


HRESULT CCefPushPin::GetMediaType(int iPosition, CMediaType *pmt)
{
    CheckPointer(pmt,E_POINTER);
    CAutoLock cAutoLock(m_pFilter->pStateLock());

    if(iPosition < 0)
        return E_INVALIDARG;

    // Have we run off the end of types?
    if(iPosition > 0)
        return VFW_S_NO_MORE_ITEMS;

    VIDEOINFO *pvi = (VIDEOINFO *) pmt->AllocFormatBuffer(sizeof(VIDEOINFO));
    if(NULL == pvi)
        return(E_OUTOFMEMORY);

    // Initialize the VideoInfo structure before configuring its members
    ZeroMemory(pvi, sizeof(VIDEOINFO));

    switch(iPosition)
    {
        case 0:
        {    
            // Return our highest quality 32bit format

            // Since we use RGB888 (the default for 32 bit), there is
            // no reason to use BI_BITFIELDS to specify the RGB
            // masks. Also, not everything supports BI_BITFIELDS
            pvi->bmiHeader.biCompression = BI_RGB;
            pvi->bmiHeader.biBitCount    = 32;
            break;
        }
    }

    // Adjust the parameters common to all formats
    pvi->bmiHeader.biSize       = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth      = m_iImageWidth;
    pvi->bmiHeader.biHeight     = m_iImageHeight;
    pvi->bmiHeader.biPlanes     = 1;
    pvi->bmiHeader.biSizeImage  = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    pmt->SetTemporalCompression(FALSE);

    // Work out the GUID for the subtype from the header info.
    pmt->SetSubtype(&MEDIASUBTYPE_ARGB32);
    pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);

	DbgLog((LOG_TRACE, 3, TEXT("CCefPushPin::GetMediaType(%d)"), iPosition));

    return NOERROR;

} // GetMediaType


HRESULT CCefPushPin::CheckMediaType(const CMediaType *pMediaType)
{
    CheckPointer(pMediaType,E_POINTER);

    if((*(pMediaType->Type()) != MEDIATYPE_Video) ||   // we only output video
        !(pMediaType->IsFixedSize()))                  // in fixed size samples
    {                                                  
        return E_INVALIDARG;
    }

    // Check for the subtypes we support
    const GUID *SubType = pMediaType->Subtype();
    if (SubType == NULL)
        return E_INVALIDARG;

    if((*SubType != MEDIASUBTYPE_ARGB32))
    {
        return E_INVALIDARG;
    }

    // Get the format area of the media type
    VIDEOINFO *pvi = (VIDEOINFO *) pMediaType->Format();

    if(pvi == NULL)
        return E_INVALIDARG;

    // Check if the image width & height have changed
    if(    pvi->bmiHeader.biWidth   != m_iImageWidth || 
       abs(pvi->bmiHeader.biHeight) != m_iImageHeight)
    {
        // If the image width/height is changed, fail CheckMediaType() to force
        // the renderer to resize the image.
        return E_INVALIDARG;
    }

    // Don't accept formats with negative height, which would cause the desktop
    // image to be displayed upside down.
    if (pvi->bmiHeader.biHeight < 0)
        return E_INVALIDARG;

	DbgLog((LOG_TRACE, 3, TEXT("CCefPushPin::CheckMediaType(%d)"), 0));

    return S_OK;  // This format is acceptable.

} // CheckMediaType


HRESULT CCefPushPin::DecideBufferSize(IMemAllocator *pAlloc,
                                      ALLOCATOR_PROPERTIES *pProperties)
{
    CheckPointer(pAlloc,E_POINTER);
    CheckPointer(pProperties,E_POINTER);

    CAutoLock cAutoLock(m_pFilter->pStateLock());
    HRESULT hr = NOERROR;

    VIDEOINFO *pvi = (VIDEOINFO *) m_mt.Format();
    pProperties->cBuffers = 1;
    pProperties->cbBuffer = pvi->bmiHeader.biSizeImage;

    ASSERT(pProperties->cbBuffer);

    // Ask the allocator to reserve us some sample memory. NOTE: the function
    // can succeed (return NOERROR) but still not have allocated the
    // memory that we requested, so we must check we got whatever we wanted.
    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProperties,&Actual);
    if(FAILED(hr))
    {
        return hr;
    }

    // Is this allocator unsuitable?
    if(Actual.cbBuffer < pProperties->cbBuffer)
    {
        return E_FAIL;
    }

    // Make sure that we have only 1 buffer (we erase the ball in the
    // old buffer to save having to zero a 200k+ buffer every time
    // we draw a frame)
    ASSERT(Actual.cBuffers == 1);
    return NOERROR;

} // DecideBufferSize


HRESULT CCefPushPin::SetMediaType(const CMediaType *pMediaType)
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());

    // Pass the call up to my base class
    HRESULT hr = CSourceStream::SetMediaType(pMediaType);

    if(SUCCEEDED(hr))
    {
        VIDEOINFO * pvi = (VIDEOINFO *) m_mt.Format();
        if (pvi == NULL)
            return E_UNEXPECTED;

        switch(pvi->bmiHeader.biBitCount)
        {
            case 32:    // RGB32/ARGB32
                // Save the current media type and bit depth
                m_MediaType = *pMediaType;
                m_nCurrentBitDepth = pvi->bmiHeader.biBitCount;
                hr = S_OK;
                break;

            default:
                // We should never agree any other media types
                ASSERT(FALSE);
                hr = E_INVALIDARG;
                break;
        }
    }

	DbgLog((LOG_TRACE, 3, TEXT("CCefPushPin::SetMediaType(%d)"), 0));

    return hr;

} // SetMediaType


HRESULT CCefPushPin::FillBuffer(IMediaSample *pSample)
{
	BYTE *pData = NULL;
    long cbData = 0;

    CheckPointer(pSample, E_POINTER);

    CAutoLock cAutoLockShared(&m_cSharedState);

    // Access the sample's data buffer
    pSample->GetPointer(&pData);
    cbData = pSample->GetSize();

    // Check that we're still using video
    ASSERT(m_mt.formattype == FORMAT_VideoInfo);

    VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER*)m_mt.pbFormat;

	// Copy the DIB bits over into our filter's output buffer.
    // Since sample size may be larger than the image size, bound the copy size.
    int nSize = min(pVih->bmiHeader.biSizeImage, (DWORD) cbData);

	if(m_renderMode == WindowLess)
	{
		CAutoLock cAutoLockShared(&m_cGrabOpt);
		memcpy_s(pData,cbData,m_pBuffer,nSize);
	}
	else
	{
		while (true)
		{
			WaitForSingleObject(m_hNewRenderEvent, 100);
			::ResetEvent(m_hNewRenderEvent);
		}
	}

	// Set the timestamps that will govern playback frame rate.
	// If this file is getting written out as an AVI,
	// then you'll also need to configure the AVI Mux filter to 
	// set the Average Time Per Frame for the AVI Header.
    // The current time is the sample's start.
    REFERENCE_TIME rtStart = m_iFrameNumber * m_rtFrameLength;
    REFERENCE_TIME rtStop  = rtStart + m_rtFrameLength;

    pSample->SetTime(&rtStart, &rtStop);
    m_iFrameNumber++;

	// Set TRUE on every sample for uncompressed frames
    pSample->SetSyncPoint(TRUE);

    return S_OK;
}

HRESULT CCefPushPin::OnThreadCreate(void)
{
	if(m_renderMode != WindowLess)
		StartGrabThread();
	return __super::OnThreadCreate();
}

HRESULT CCefPushPin::OnThreadDestroy(void)
{
	if (m_renderMode != WindowLess)
		StopGrabThread();
	return __super::OnThreadDestroy();
}

HRESULT CCefPushPin::OnThreadStartPlay(void)
{
	return __super::OnThreadStartPlay();
}

BOOL CCefPushPin::StartGrabThread()
{
	if(m_bGrabBuffer)
		return TRUE;
	m_hThread = ::CreateThread(NULL,0,&GrabThreadProc,(void*)this,CREATE_SUSPENDED,NULL);
	if(!m_hThread)
		return FALSE;
	m_bGrabBuffer = 1;
	::ResumeThread(m_hThread);
	return TRUE;
}

BOOL CCefPushPin::StopGrabThread()
{
	if(!m_bGrabBuffer)
		return TRUE;
	m_bGrabBuffer = 0;
	::SetEvent(m_hNewGrabEvent);
	::WaitForSingleObject(m_hThread,INFINITE);
	::CloseHandle(m_hThread);
	m_hThread = NULL;
	return TRUE;
}

DWORD WINAPI CCefPushPin::GrabThreadProc(LPVOID lpParameter)
{
	CCefPushPin* pThis = static_cast<CCefPushPin*>(lpParameter);
	assert(pThis);
	if(pThis) pThis->DoGrab();
	return 0;
}

void CCefPushPin::DoGrab()
{
	long lFps = (LONG)m_rtFrameLength / 10000;
	long bFlag = 0;
	if(lFps<16) lFps = 16;
	m_hNewGrabEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(m_hNewGrabEvent);
	if (!m_hNewGrabEvent) return ;
	while(m_bGrabBuffer)
	{
		//Grab webbrowser instance buffer
		HDIB hDib = NULL;
		{
			CAutoLock cAutoLockShared(&m_cGrabOpt);
			//hDib = CopyScreenToBitmap(&m_rScreen, m_pBuffer, (BITMAPINFO *) &(m_header));
			::SetEvent(m_hNewRenderEvent);
		}
		if (hDib) DeleteObject(hDib);
		::WaitForSingleObject(m_hNewGrabEvent, lFps);
		::ResetEvent(m_hNewGrabEvent);
	}
	::CloseHandle(m_hNewGrabEvent);
	m_hNewGrabEvent = NULL;
}

void CCefPushPin::RenderBuffer(void * pBuffer, ULONG len)
{
	{
		CAutoLock cAutoLockShared(&m_cGrabOpt);
		assert(len == m_ulBufferSize);
		memcpy_s(m_pBuffer,len,pBuffer,len);
	}
	::SetEvent(m_hNewRenderEvent);
}

/**
*
*  CCefSource Class
*
*/

CCefSource::CCefSource(IUnknown *pUnk, HRESULT *phr)
           : CSource(NAME("CEFWebBrowserSource"), pUnk, CLSID_CefWebBrowser)
{
	::CoInitialize(NULL);
    // The pin magically adds itself to our pin array.
    m_pPin = new CCefPushPin(phr, this);

	if (phr)
	{
		if (m_pPin == NULL)
			*phr = E_OUTOFMEMORY;
		else
			*phr = S_OK;
	}  
}


CCefSource::~CCefSource()
{
	if(m_pPin->IsInitialized())
		m_pPin->UnInit();
	delete m_pPin;
	::CoUninitialize();
}


CUnknown * WINAPI CCefSource::CreateInstance(IUnknown *pUnk, HRESULT *phr)
{
    CCefSource *pNewFilter = new CCefSource(pUnk, phr );

	if (phr)
	{
		if (pNewFilter == NULL) 
			*phr = E_OUTOFMEMORY;
		else
			*phr = S_OK;
	}
    return pNewFilter;

}

HRESULT CCefSource::Load( 
	LPCOLESTR pszFileName,
	__in_opt  const AM_MEDIA_TYPE *pmt)
    {
        CheckPointer(pszFileName, E_POINTER);

        // lstrlenW is one of the few Unicode functions that works on win95
        int cch = lstrlenW(pszFileName) + 1;

#ifndef UNICODE
        TCHAR *lpszFileName=0;
        lpszFileName = new char[cch * 2];
        if (!lpszFileName) {
      	    return E_OUTOFMEMORY;
        }
        WideCharToMultiByte(GetACP(), 0, pszFileName, -1,
    			lpszFileName, cch, NULL, NULL);
		 m_pFileName = lpszFileName;
		 delete [] lpszFileName;
#else
        TCHAR lpszFileName[MAX_PATH]={0};
        lstrcpy(lpszFileName, pszFileName);
		 m_pFileName = lpszFileName;
#endif
        //CAutoLock lck(&m_csFilter);

        /*  Check the file type */
        //CMediaType cmt;
        //if (NULL == pmt) {
        //    cmt.SetType(&MEDIATYPE_Video);
        //    cmt.SetSubtype(&MEDIASUBTYPE_NULL);
        //} else {
        //    cmt = *pmt;
        //}

		 if (!m_pFileName.IsEmpty())
		{

		}
		 m_pPin->m_fps = 12;
		 m_pPin->m_viewRc = CRect(0,0,m_pPin->m_iImageWidth,m_pPin->m_iImageHeight);
		 if(m_pPin->IsInitialized())
			   return E_UNEXPECTED;
		return m_pPin->InitInstance();
#ifdef DEBUG
	ATLTRACE("CCefSource::Load\n");
#endif
        return S_OK;
    }

HRESULT CCefSource::GetCurFile( 
	__out  LPOLESTR *ppszFileName,
	__out_opt  AM_MEDIA_TYPE *pmt)
    {
        CheckPointer(ppszFileName, E_POINTER);
        *ppszFileName = NULL;

        if (!m_pFileName.IsEmpty()) {
			DWORD n = sizeof(WCHAR)*(1+lstrlenW(m_pFileName.GetBuffer()));

            *ppszFileName = (LPOLESTR) CoTaskMemAlloc( n );
            if (*ppszFileName!=NULL) {
                  CopyMemory(*ppszFileName, m_pFileName, n);
            }
        }

        if (pmt!=NULL) {
			CopyMediaType(pmt, &m_pPin->m_MediaType);
        }
#ifdef DEBUG
	ATLTRACE("CCefSource::GetCurFile\n");
#endif
        return NOERROR;
    }