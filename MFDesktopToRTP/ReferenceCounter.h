/************************************************************************************************************************************************************************************************************
*FILE:  SmartPtr..h
*
*DESCRIPTION - Takes care of Incrementing and Decrementing the reference countd and Releasing the memory when the count reaches zero
*
*
*AUTHOR		: RAMESH KUMAR K
*
*Date: OCT 2016
**************************************************************************************************************************************************************************************************************/

#ifndef __REFERENCE_COUNTER_H__
#define __REFERENCE_COUNTER_H__
#include <cassert>

/**
* Base class for all classes that support reference counting
*/
class IReferenceCounter
{
public:

	IReferenceCounter() : _cRef(1) {}

	virtual ~IReferenceCounter() {}


	virtual IFACEMETHODIMP_(ULONG) AddRef()

	{

		return InterlockedIncrement(&_cRef);

	}


	virtual IFACEMETHODIMP_(ULONG) Release()

	{
		assert(_cRef > 0);

		LONG cRef = InterlockedDecrement(&_cRef);

		if (!cRef)

			delete this;

		return cRef;

	}

	template <class T>
	IFACEMETHODIMP_(T*) DetachObject()

	{
		assert(_cRef > 0);

		InterlockedDecrement(&_cRef);

		return (T*)(this);

	}

	virtual int getReferenceCount()
	{
		return _cRef;
	}

private:

	mutable LONG _cRef;
};
#endif