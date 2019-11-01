/************************************************************************************************************************************************************************************************************
*FILE:  SmartPtr..h
*
*DESCRIPTION - Takes care of Managing the references made by objects
*
*
*AUTHOR		: RAMESH KUMAR K
*
*Date: OCT 2016
**************************************************************************************************************************************************************************************************************/

#pragma once

template <typename T>
inline void SafeRelease(T **p)
{
	if (*p)
	{
		(*p)->Release();
		*p = NULL;
	}
}

/**
* A reference counting-managed pointer for classes derived from RefrenceCounter which can
* be used as C pointer
*/
template<class T>
class SmartPtr
{
public:
	//Construct using a C pointer
	//e.g. SmartPtr<T> x = new T();
	SmartPtr(T* ptr = NULL) : mPtr(ptr)
	{
		if (ptr != NULL) { ptr->AddRef(); }
	}

	//Copy constructor
	SmartPtr(const SmartPtr &ptr)
		: mPtr(ptr.mPtr)
	{
		if (mPtr != NULL) { mPtr->AddRef(); }
	}

	~SmartPtr()
	{
		if (mPtr != NULL) { mPtr->Release(); }
	}

	//Assign a pointer
	//e.g. x = new T();
	SmartPtr &operator=(T* ptr)
	{
		if (ptr != NULL)
		{
			ptr->AddRef();
		}

		if (mPtr != NULL)
		{
			mPtr->Release();
		}

		mPtr = ptr;

		return (*this);
	}

	//Assign another SmartPtr
	SmartPtr &operator=(const SmartPtr &ptr)
	{
		return (*this) = ptr.mPtr;
	}

	//Retrieve actual pointer
	T* get() const
	{
		return mPtr;
	}

	//Some overloaded operators to facilitate dealing with an SmartPtr as a convetional C pointer.
	//Without these operators, one can still use the less transparent get() method to access the pointer.
	T* operator->() const { return mPtr; }	//x->member

	T &operator*() const { return *mPtr; }	//*x, (*x).member

	operator T*() const { return mPtr; }		//T* y = x;

	operator bool() const { return mPtr != NULL; }	//if(x) {/*x is not NULL*/}

	bool operator==(const SmartPtr &ptr) { return mPtr == ptr.mPtr; }

	bool operator==(const T *ptr) { return mPtr == ptr; }

	//T* operator new(const T()) { (new T())->template DetachObject<T>(); } // needed to be tested

private:
	T *mPtr;	//Actual pointer
};