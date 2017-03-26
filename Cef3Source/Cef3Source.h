#include "BrowserEventHandler.h"

class CCefSource;

class CCefPushPin 
	: public CSourceStream
	, public CBrowserBase
{
	friend class CCefSource;

public:

	CCefPushPin(HRESULT *phr, CSource *pFilter);
	~CCefPushPin();

	// Override the version that offers exactly one media type
	virtual HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest);
	virtual HRESULT FillBuffer(IMediaSample *pSample);

	// Set the agreed media type and set up the necessary parameters
	virtual HRESULT SetMediaType(const CMediaType *pMediaType);

	// Support multiple display formats
	virtual HRESULT CheckMediaType(const CMediaType *pMediaType);
	virtual HRESULT GetMediaType(int iPosition, CMediaType *pmt);

	virtual HRESULT OnThreadCreate(void);
	virtual HRESULT OnThreadDestroy(void);
	virtual HRESULT OnThreadStartPlay(void);

	// Quality control
	// Not implemented because we aren't going in real time.
	// If the file-writing filter slows the graph down, we just do nothing, which means
	// wait until we're unblocked. No frames are ever dropped.
	STDMETHODIMP Notify(IBaseFilter *pSelf, Quality q)
	{
		return E_FAIL;
	}
private:
	//envents handler for browser
	virtual void RenderBuffer(void * pBuffer, ULONG len);

private:
	//HDC grab thread, when webbrowser in popup mode
	BOOL StartGrabThread();
	BOOL StopGrabThread();
	static DWORD WINAPI GrabThreadProc(LPVOID lpParameter);
	void DoGrab();
	HANDLE m_hThread;
	HANDLE m_hNewGrabEvent;
	BOOL m_bGrabBuffer; 

	HANDLE m_hNewRenderEvent;

	BYTE* m_pBuffer;
	ULONG m_ulBufferSize;

	CCritSec m_cGrabOpt;

protected:

	int m_FramesWritten;				// To track where we are in the file
	BOOL m_bZeroMemory;                 // Do we need to clear the buffer?
	CRefTime m_rtSampleTime;	        // The time stamp for each sample

	int m_iFrameNumber;
	const REFERENCE_TIME m_rtFrameLength;

	RECT m_rScreen;                     // Rect containing entire screen coordinates

	int m_iImageHeight;                 // The current image height
	int m_iImageWidth;                  // And current image width
	int m_iRepeatTime;                  // Time in msec between frames
	int m_nCurrentBitDepth;             // Screen bit depth


	CMediaType m_MediaType;
	CCritSec m_cSharedState;            // Protects our internal state
	CImageDisplay m_Display;            // Figures out our media type for us
};


class CCefSource
	: public CSource
	, public IFileSourceFilter
{

private:
	// Constructor is private because you have to use CreateInstance
	CCefSource(IUnknown *pUnk, HRESULT *phr);
	~CCefSource();

	CCefPushPin *m_pPin;
	
	 CString     m_pFileName;
public:
	static CUnknown * WINAPI CreateInstance(IUnknown *pUnk, HRESULT *phr);  
	DECLARE_IUNKNOWN
public:
	 STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv)
    {
        if (riid == IID_IFileSourceFilter) {
            return GetInterface((IFileSourceFilter *)this, ppv);
        } else {
            return CSource::NonDelegatingQueryInterface(riid, ppv);
        }
    }

	 /*  IFileSourceFilter methods */
        virtual HRESULT STDMETHODCALLTYPE Load( 
            /* [in] */ LPCOLESTR pszFileName,
            /* [annotation][unique][in] */ 
            __in_opt  const AM_MEDIA_TYPE *pmt);
        
        virtual HRESULT STDMETHODCALLTYPE GetCurFile( 
            /* [annotation][out] */ 
            __out  LPOLESTR *ppszFileName,
            /* [annotation][out] */ 
            __out_opt  AM_MEDIA_TYPE *pmt);
};

