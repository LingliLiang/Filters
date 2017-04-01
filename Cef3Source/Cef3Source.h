#include "BrowserEventHandler.h"
#include "RingQueue.h"

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

	void ClearQueue();

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
	struct _tagRenderBuffer
	{
		unsigned long ulBufferSize;
		unsigned char* pBuffer;
		unsigned long AddRef()
		{
			return ::InterlockedIncrement(&ref);
		}
		unsigned long Release()
		{
			unsigned long ret = ::InterlockedDecrement(&ref);
			 if(ret == 0)
			 {
				 delete this;
			 }
			 return ret;
		}
		bool NewAndCopy(void * src, unsigned long size)
		{
			if(pBuffer || !src) return false;
			pBuffer = new unsigned char[size];
			if(!pBuffer) return false;
			ulBufferSize = size;
			::memcpy_s(pBuffer,ulBufferSize,src,size);
			return true;
		}
		_tagRenderBuffer():ref(0),pBuffer(NULL),ulBufferSize(0){}
		~_tagRenderBuffer()
		{
			if(pBuffer && ulBufferSize)
			{
				::OutputDebugStringA("~_tagRenderBuffer()\n");
				delete [] pBuffer;
				pBuffer = NULL;
			}
		}
	private:
		volatile unsigned long ref;
	};
	
	template <class T>
	class ScopedPtr
	{
	public:
		ScopedPtr(T* t)
		{
			p_ = t;
			if (p_)
				p_->AddRef();
		}
		ScopedPtr():p_(0){}
		~ScopedPtr()
		{
			Clear();
		}
		ScopedPtr(const ScopedPtr<T>& r): p_(r.p_) 
		{
			if (p_)
				p_->AddRef();
		}
		void operator=(const ScopedPtr<T>& r)
		{
			Clear();
			p_ = r.p_;
			if (p_)
				p_->AddRef();
		}
		void Clear()
		{
			if (p_)
				p_->Release();
		}
		T* operator->() const
		{
			return p_;
		}
		T* get() const
		{
			return p_;
		}
	private:
		T* p_;
	};

	uqueue::RingQueue<ScopedPtr<_tagRenderBuffer>> m_Queue;

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

