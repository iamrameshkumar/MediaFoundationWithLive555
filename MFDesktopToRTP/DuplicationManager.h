// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#ifndef _DUPLICATIONMANAGER_H_
#define _DUPLICATIONMANAGER_H_

#include "CommonTypes.h"
#include "d3d11.h"
#include "dxgi1_2.h"
#include "ReferenceCounter.h"

class BitmapData : public IReferenceCounter
{
private:
	virtual ~BitmapData();
public:
	int bmpSize;
	int width;
	int height;
	int bitsPerPixel;
	int bytesPerPixel;
	int bytesPerRow;

	int RowPitch; // for desktop duplication

	byte * lpbitmap;
	//BITMAPINFO  dibInfo ;

	BitmapData(BitmapData *bmp = NULL);

	BitmapData(const BitmapData &bmp);
};

typedef struct	__MONITOR : public IReferenceCounter
{
	HMONITOR	hMon;
	HDC			hDC;
	LPRECT		scrRect;
	LPRECT      pRect;     //Used for high resolution monitor support
	LPARAM		data;

	__MONITOR() : IReferenceCounter()
	{
		hMon = 0;
		hDC = 0;
		pRect = 0;
		scrRect = 0;
		data = 0;
	}

	~__MONITOR()
	{
		if (scrRect)
		{
			delete scrRect;
		}

		if (pRect)
		{
			delete pRect;
		}
	}


} MONITOR;

enum DXGI_ERRORS { FRAME_ROTATION_UNSUPPORTED = -1, DEVICE_REMOVED, DEVICE_HUNG };


//
// Handles the task of duplicating an output.
//
class cDuplicationManager
{
public:
	cDuplicationManager();
	~cDuplicationManager();
	_Success_(*Timeout == false && return == DUPL_RETURN_SUCCESS) HRESULT GetFrame(_Out_ FRAME_DATA* Data, int timeout, _Out_ bool* Timeout);
	HRESULT DoneWithFrame();
	HRESULT InitDupl(_In_ ID3D11Device* Device, IDXGIAdapter *_pAdapter, IDXGIOutput *_pOutput, UINT Output);
	HRESULT GetMouse(_Inout_ PTR_INFO* PtrInfo, _In_ DXGI_OUTDUPL_FRAME_INFO* FrameInfo, INT OffsetX, INT OffsetY);
	void GetOutputDesc(_Out_ DXGI_OUTPUT_DESC* DescPtr);
	void Reset();
	void CleanUp();
	BOOL IsDeviceReady();

private:

	// vars 

		// Duplicated desktop image
	IDXGIOutputDuplication* m_DeskDupl;

	// Interface for desktop texture
	ID3D11Texture2D* m_AcquiredDesktopImage;
	_Field_size_bytes_(m_MetaDataSize) BYTE* m_MetaDataBuffer;
	UINT m_MetaDataSize;
	UINT m_OutputNumber;

	// Physical connection between video card and device
	DXGI_OUTPUT_DESC m_OutputDesc;

	// Virtual adapter - Used to create a device
	ID3D11Device* m_Device;
};

#endif
