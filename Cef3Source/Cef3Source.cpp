
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

SIZE CalcImgFitSize(unsigned long ulWidth, unsigned long ulHeight, unsigned long image_width, unsigned long image_height)
	{
		float x_sc = 0.0;
		float y_sc = 0.0;
		int insetDH = 0;
		int insetDW = 0;
		long ctl_w = (long)ulWidth;
		long ctl_h = (long)ulHeight;
		x_sc = (float)ctl_w/image_width;
		y_sc = (float)ctl_h/image_height;
		if(image_width && image_height)
		{
			if(image_width > ctl_w && image_height > ctl_h)
			{
				if(image_width > image_height)
				{
					insetDH = ctl_h - (long)(image_height*x_sc);
					insetDW = ctl_w - (long)(image_width*x_sc);
					if(insetDH < 0)
					{
						insetDH = ctl_h - (long)(image_height*y_sc);
						insetDW = ctl_w - (long)(image_width*y_sc);
					}
				}
				else 
				{
					insetDH = ctl_h - (long)(image_height*y_sc);
					insetDW = ctl_w - (long)(image_width*y_sc);
					if(insetDW < 0)
					{
						insetDH = ctl_h - (long)(image_height*x_sc);
						insetDW = ctl_w - (long)(image_width*x_sc);
					}
				}
			}
			else if(image_width > ctl_w && image_height < ctl_h)
			{
				insetDH = ctl_h - (long)(image_height*x_sc);
				insetDW = ctl_w - (long)(image_width*x_sc);
			}
			else if(image_width < ctl_w && image_height > ctl_h)
			{
				insetDH = ctl_h - (long)(image_height*y_sc);
				insetDW = ctl_w- (long)(image_width*y_sc);
			}
			else
			{
				insetDH = ctl_h - image_height;
				insetDW = ctl_w - image_width;
			}
		}
		SIZE inset = {insetDW/2 , insetDH/2 };
		return inset;
	}


void SaveToFile(HDC hdcWnd,HBITMAP & hSrc,LPCTSTR fileName)
{
	BITMAP bmpSrc;
	// Get the BITMAP from the HBITMAP
	GetObject(hSrc, sizeof(BITMAP), &bmpSrc);

	BITMAPFILEHEADER   bmfHeader;
	BITMAPINFOHEADER   bi;

	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = bmpSrc.bmWidth;
	bi.biHeight = bmpSrc.bmHeight;
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	DWORD dwBmpSize = ((bmpSrc.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmpSrc.bmHeight;

	// Starting with 32-bit Windows, GlobalAlloc and LocalAlloc are implemented as wrapper functions that 
	// call HeapAlloc using a handle to the process's default heap. Therefore, GlobalAlloc and LocalAlloc 
	// have greater overhead than HeapAlloc.
	HANDLE hDIB = GlobalAlloc(GHND, dwBmpSize);
	char *lpbitmap = (char *)GlobalLock(hDIB);

	// Gets the "bits" from the bitmap and copies them into a buffer 
	// which is pointed to by lpbitmap.
	GetDIBits(hdcWnd, hSrc, 0,
		(UINT)bmpSrc.bmHeight,
		lpbitmap,
		(BITMAPINFO *)&bi, DIB_RGB_COLORS);

	// A file is created, this is where we will save the screen capture.
	HANDLE hFile = CreateFile(fileName,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);

	// Add the size of the headers to the size of the bitmap to get the total file size
	DWORD dwSizeofDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	//Offset to where the actual bitmap bits start.
	bmfHeader.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER);

	//Size of the file
	bmfHeader.bfSize = dwSizeofDIB;

	//bfType must always be BM for Bitmaps
	bmfHeader.bfType = 0x4D42; //BM   

	DWORD dwBytesWritten = 0;
	WriteFile(hFile, (LPSTR)&bmfHeader, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
	WriteFile(hFile, (LPSTR)&bi, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
	WriteFile(hFile, (LPSTR)lpbitmap, dwBmpSize, &dwBytesWritten, NULL);

	//Unlock and Free the DIB from the heap
	GlobalUnlock(hDIB);
	GlobalFree(hDIB);

	//Close the handle for the file that was created
	CloseHandle(hFile);
}

BOOL CopyWndToByte(HWND hWnd, CRect rect_fitin, BYTE* &pData_out,ULONG cbLen)
{
	BOOL result = 0;
	DWORD err = 0;
	CRect cliRc;
	CRect wndRc;
	CPoint ptlt;
	::GetClientRect(hWnd,&cliRc);
	::GetWindowRect(hWnd, &wndRc);
	ptlt.SetPoint(cliRc.left, cliRc.top);
	::ClientToScreen(hWnd,&ptlt);
	ptlt.x -= wndRc.left;
	ptlt.y -= wndRc.top;
	//cliRc.OffsetRect(ptlt);

	BITMAPINFOHEADER bphdr = {
		sizeof(BITMAPINFOHEADER),
		rect_fitin.Width(),
		rect_fitin.Height(),
		1,
		32,
		BI_RGB,0,
		0,0,0,0
	};

	HDC hdcWindow;
	HDC hdcMemDC = NULL;
	HBITMAP hbmWindow = NULL;
	HBITMAP oldBmp = NULL;

	hdcWindow = GetDC(hWnd);

	// Create a compatible DC which is used in a BitBlt from the window DC
	hdcMemDC = CreateCompatibleDC(hdcWindow);

	if (!hdcMemDC)
	{
		goto done;
	}
	// Create a compatible bitmap from the Window DC
	hbmWindow = CreateCompatibleBitmap(hdcWindow, cliRc.Width(), cliRc.Height());

	if (!hbmWindow)
	{
		goto done;
	}

	// Select the compatible bitmap into the compatible memory DC.
	oldBmp = (HBITMAP)SelectObject(hdcMemDC, hbmWindow);

	//This is the best stretch mode
	SetStretchBltMode(hdcMemDC, HALFTONE);

	//The source DC is the entire screen and the destination DC is the current window (HWND)
	if (!StretchBlt(hdcMemDC,
		0, 0,
		cliRc.right, cliRc.bottom,
		hdcWindow,
		cliRc.left, cliRc.top,
		cliRc.right,cliRc.bottom,
		SRCCOPY))
	{
		goto done;
	}

	

	//// Bit block transfer into our compatible memory DC.
	//if (!BitBlt(hdcMemDC,
	//	0, 0,
	//	cliRc.right, cliRc.bottom,
	//	hdcWindow,
	//	cliRc.left, cliRc.top,
	//	SRCCOPY))
	//{
	//	goto done;
	//}

	pData_out = new BYTE[cbLen];
	memset(pData_out,0, cbLen);
	GetDIBits(hdcMemDC, hbmWindow, 0,
		(UINT) bphdr.biHeight,
		pData_out,
		(BITMAPINFO *)&bphdr, DIB_RGB_COLORS);

	SelectObject(hdcMemDC,oldBmp);
	//Clean up
done:
	
	DeleteObject(hbmWindow);
	DeleteObject(hdcMemDC);
	ReleaseDC(hWnd, hdcWindow);

	return 1;
}

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
		m_Queue(32) // big queue take more memory,so set small size
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

    // Release the device context
    DeleteDC(hDC);
	//m_hNewRenderEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	//assert(m_hNewRenderEvent);
	//::ResetEvent(m_hNewRenderEvent);
}

CCefPushPin::~CCefPushPin()
{   
    DbgLog((LOG_TRACE, 3, TEXT("Frames written %d"), m_iFrameNumber));
	ClearQueue();
	//::CloseHandle(m_hNewRenderEvent);
	m_hNewRenderEvent = NULL;
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
    int nSize = std::min(pVih->bmiHeader.biSizeImage, (DWORD) cbData);

	Command com;
	
	do{
		BOOL bHaveSample = FALSE;
		//CAutoLock cAutoLockShared(&m_cGrabOpt);
		while (!CheckRequest(&com))
		{
			//buffer won't free when ref is big than 1,'release' is just name;
			//but , by using PassPtr<> now will be free.
			PassPtr<_tagRenderBuffer> release(0);
			if(m_Queue.empty()) 
			{
				::Sleep(100);
				continue;
			}
			if(m_Queue.size() > 1)
			{
				assert(m_Queue.lock_pop(release));
				memcpy_s(pData,cbData,release->pBuffer,nSize);
				::OutputDebugString(_T("New render buffer\n"));
			}
			else
			{
				PassPtr<_tagRenderBuffer>& front = m_Queue.front();
				memcpy_s(pData,cbData,front->pBuffer,nSize);
				::OutputDebugString(_T("Front render buffer\n"));
			}
			bHaveSample = TRUE;
			break;
		}
		if (bHaveSample){
			break;
		}

		if (com == CMD_RUN || com == CMD_PAUSE) {
			Reply(NOERROR);
		} else if (com != CMD_STOP) {
			Reply((DWORD) E_UNEXPECTED);
			DbgLog((LOG_ERROR, 1, TEXT("Unexpected command!!!")));
		}
	}while(com != CMD_STOP);

	if (com == CMD_STOP)
	{
		ATLTRACE(_T("End of stream.\n"));
		return S_FALSE;	//End of stream
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
	if(!IsInitialized())
		InitInstance();
	return __super::OnThreadCreate();
}

HRESULT CCefPushPin::OnThreadDestroy(void)
{
	if (m_renderMode != WindowLess)
		StopGrabThread();
	else
	{
		if(IsInitialized())
			UnInit();
	}
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
	long bFlag = 1;
	m_hNewGrabEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(m_hNewGrabEvent);
	if (!m_hNewGrabEvent) return ;
	HWND HBrowser = NULL;
	while (!IsInitialized())
	{
		::Sleep(100); //wait init
		if (m_bGrabBuffer == 0) { bFlag = 0;break; }
	}
	if (bFlag)
	{
		HBrowser = (HWND)m_spBrowser->GetBrowserWnd();
	}
	assert(HBrowser);
	while (m_bGrabBuffer)
	{
		
		//Grab webbrowser instance buffer
		HDIB hDib = NULL;
		{
			PassPtr<_tagRenderBuffer> buffer(new _tagRenderBuffer); //auto free
			if (CopyWndToByte(HBrowser, m_viewRc, buffer->pBuffer, m_ulBufferSize))
			{
				buffer->ulBufferSize = m_ulBufferSize;
				m_Queue.push(buffer); // if failed will drop this frame
			}
			//::SetEvent(m_hNewRenderEvent);
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
	assert(len == m_ulBufferSize);
	PassPtr<_tagRenderBuffer> buffer(new _tagRenderBuffer);
	buffer->NewAndCopy(pBuffer, len);
	if (!m_Queue.push(buffer)) //pass end buffer is invaild
	{
		//push failed ,queue if full
		assert(m_Queue.full());
		PassPtr<_tagRenderBuffer> release(0);
		m_Queue.lock_pop(release);
		assert(m_Queue.push(buffer));
	}
	::OutputDebugString(_T("RenderBuffer\n"));
}

void CCefPushPin::ClearQueue()
{
	//won't need clear, is auto free
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
		//	m_iImageWidth  = m_rScreen.right  - m_rScreen.left;
		//	m_iImageHeight = m_rScreen.bottom - m_rScreen.top;
		//	m_ulBufferSize = m_iImageWidth*m_iImageHeight*4;
		 }
		 m_pPin->m_fps = 30;
		 m_pPin->m_viewRc = CRect(0,0,m_pPin->m_iImageWidth,m_pPin->m_iImageHeight);
		 m_pPin->m_renderMode = AsPopup;
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