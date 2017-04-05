#pragma once


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
	ScopedPtr() :p_(0) {}
	~ScopedPtr()
	{
		Clear();
	}
	ScopedPtr(const ScopedPtr<T>& r) : p_(r.p_)
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

template <class T>
class PassPtr
{
public:
	PassPtr(T* t)
	{
		p_ = t;
		if (p_)
			p_->AddRef();
	}
	PassPtr() :p_(0) {}
	~PassPtr()
	{
		Clear();
	}
	PassPtr(const PassPtr<T>& r) : p_(r.p_)
	{
		if (p_)
		{
			p_->AddRef();
			const_cast< PassPtr<T>&>(r).Clear();
		}
	}
	void operator=(const PassPtr<T>& r)
	{
		Clear();
		p_ = r.p_;
		if (p_)
		{
			p_->AddRef();
			const_cast< PassPtr<T>&>(r).Clear();
		}
	}
	void Clear()
	{
		if (p_)
		{
			p_->Release();
			p_ = 0;
		}
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
