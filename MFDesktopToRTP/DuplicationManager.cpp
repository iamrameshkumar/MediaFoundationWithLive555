/************************************************************************************************************************************************************************************************************
*FILE:  Id3d11DuplicationManager.cpp
*
*DESCRIPTION - Contains methods to capture the screen using DirectX APIs
*
*
*Date: OCT 2016
**************************************************************************************************************************************************************************************************************/

/**************************************************************************************************************************************************************************************************************/
// includes

#include "DuplicationManager.h"
#include "comdef.h"

/***************************************************************************************************************************************************************************************************************/

//Function Definitions


/************************************************************************************************************************************************************************************************************
*Function:  ProcessFailure
*
*DESCRIPTION - Processes the Failure in screencapture and handles it accordinglys
*
*Returns	   Success or Failure
*
*Date: DEC 2016
**************************************************************************************************************************************************************************************************************/

HRESULT ProcessFailure(_In_opt_ ID3D11Device* Device, _In_ LPCWSTR Str, _In_ LPCWSTR Title, HRESULT hr)//, _In_opt_z_ HRESULT* ExpectedErrors = NULL)
{
	HRESULT TranslatedHr;

	// On an error check if the DX device is lost
	if (Device)
	{
		HRESULT DeviceRemovedReason = Device->GetDeviceRemovedReason();

		switch (DeviceRemovedReason)
		{
		case DXGI_ERROR_DEVICE_REMOVED:
		case DXGI_ERROR_DEVICE_RESET:
			case static_cast<HRESULT>(E_OUTOFMEMORY) :
			{
				// Our device has been stopped due to an external event on the GPU so map them all to
				// device removed and continue processing the condition
				TranslatedHr = DXGI_ERROR_DEVICE_REMOVED;
				break;
			}

			case S_OK:
			{
				// Device is not removed so use original error
				TranslatedHr = hr;
				break;
			}

			default:
			{
				// Device is removed but not a error we want to remap
				TranslatedHr = DeviceRemovedReason;
			}
		}
	}
	else
	{
		TranslatedHr = hr;
	}

	{
		_com_error err(TranslatedHr);
		LPCTSTR errMsg = err.ErrorMessage();
	}

	return TranslatedHr;
}

//
// Constructor sets up references / variables
//
cDuplicationManager::cDuplicationManager() : m_DeskDupl(nullptr),
m_AcquiredDesktopImage(nullptr),
m_MetaDataBuffer(nullptr),
m_MetaDataSize(0),
m_OutputNumber(0),
m_Device(nullptr)
{
	RtlZeroMemory(&m_OutputDesc, sizeof(m_OutputDesc));
}

//
// Destructor simply calls CleanRefs to destroy everything
//
cDuplicationManager::~cDuplicationManager()
{
	CleanUp();
}


BOOL cDuplicationManager::IsDeviceReady()
{
	if (m_DeskDupl)
	{
		return TRUE;
	}

	return FALSE;
}

bool IsDxgiFrameRotated(DXGI_MODE_ROTATION rotation) {
	switch (rotation) {
	case DXGI_MODE_ROTATION_IDENTITY:
	case DXGI_MODE_ROTATION_UNSPECIFIED:

	{
		return false;
	}

	break;

	case DXGI_MODE_ROTATION_ROTATE90:
	case DXGI_MODE_ROTATION_ROTATE180:
	case DXGI_MODE_ROTATION_ROTATE270:
	{
		return true;
	}

	break;
	}
}

//
// Initialize duplication interfaces
//
HRESULT cDuplicationManager::InitDupl(_In_ ID3D11Device* Device, _In_ IDXGIAdapter *_pAdapter, _In_ IDXGIOutput *_pOutput, _In_ UINT Output)
{
	HRESULT hr = E_FAIL;

	if (!_pOutput || !_pAdapter || !Device)
	{
		return hr;
	}

	m_OutputNumber = Output;

	// Take a reference on the device
	m_Device = Device;
	m_Device->AddRef();

#if 0
	// Get DXGI device
	IDXGIDevice* DxgiDevice = nullptr;
	HRESULT hr = m_Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&DxgiDevice));
	if (FAILED(hr))
	{
		return ProcessFailure(nullptr, _T("Failed to QI for DXGI Device"), _T("Error"), hr);
	}

	// Get DXGI adapter
	IDXGIAdapter* DxgiAdapter = nullptr;
	hr = DxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&DxgiAdapter));
	DxgiDevice->Release();
	DxgiDevice = nullptr;
	if (FAILED(hr))
	{
		return ProcessFailure(m_Device, _T("Failed to get parent DXGI Adapter"), _T("Error"), hr);//, SystemTransitionsExpectedErrors);
	}

	// Get output
	IDXGIOutput* DxgiOutput = nullptr;
	hr = DxgiAdapter->EnumOutputs(Output, &DxgiOutput);
	DxgiAdapter->Release();
	DxgiAdapter = nullptr;
	if (FAILED(hr))
	{
		return ProcessFailure(m_Device, _T("Failed to get specified output in DUPLICATIONMANAGER"), _T("Error"), hr);//, EnumOutputsExpectedErrors);
	}

	DxgiOutput->GetDesc(&m_OutputDesc);

	IDXGIOutput1* DxgiOutput1 = nullptr;
	hr = DxgiOutput->QueryInterface(__uuidof(DxgiOutput1), reinterpret_cast<void**>(&DxgiOutput1));

#endif

	_pOutput->GetDesc(&m_OutputDesc);
	// QI for Output 1
	IDXGIOutput1* DxgiOutput1 = nullptr;
	hr = _pOutput->QueryInterface(__uuidof(DxgiOutput1), reinterpret_cast<void**>(&DxgiOutput1));

	if (FAILED(hr))
	{
		return ProcessFailure(nullptr, _T("Failed to QI for DxgiOutput1 in DUPLICATIONMANAGER"), _T("Error"), hr);
	}

	// Create desktop duplication
	hr = DxgiOutput1->DuplicateOutput(m_Device, &m_DeskDupl);

	DxgiOutput1->Release();
	DxgiOutput1 = nullptr;

	if (FAILED(hr) || !m_DeskDupl)
	{
		if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
		{
			return ProcessFailure(nullptr, _T("Maximum number of applications using Desktop Duplication API"), _T("Error"), hr);
		}

		return ProcessFailure(m_Device, _T("Failed to get duplicate output in DUPLICATIONMANAGER"), _T("Error"), hr);//, CreateDuplicationExpectedErrors);
	}
	else
	{
		DXGI_OUTDUPL_DESC desc_;

		m_DeskDupl->GetDesc(&desc_);

		if (IsDxgiFrameRotated(desc_.Rotation))
		{
			return DXGI_ERRORS::FRAME_ROTATION_UNSUPPORTED;
		}
	}

	return S_OK;
}

//
// Retrieves mouse info and write it into PtrInfo
//
HRESULT cDuplicationManager::GetMouse(_Inout_ PTR_INFO* PtrInfo, _In_ DXGI_OUTDUPL_FRAME_INFO* FrameInfo, INT OffsetX, INT OffsetY)
{
	// A non-zero mouse update timestamp indicates that there is a mouse position update and optionally a shape change
	if (FrameInfo->LastMouseUpdateTime.QuadPart == 0)
	{
		return S_OK;
	}

	bool UpdatePosition = true;

	// Make sure we don't update pointer position wrongly
	// If pointer is invisible, make sure we did not get an update from another output that the last time that said pointer
	// was visible, if so, don't set it to invisible or update.
	if (!FrameInfo->PointerPosition.Visible && (PtrInfo->WhoUpdatedPositionLast != m_OutputNumber))
	{
		UpdatePosition = false;
	}

	// If two outputs both say they have a visible, only update if new update has newer timestamp
	if (FrameInfo->PointerPosition.Visible && PtrInfo->Visible && (PtrInfo->WhoUpdatedPositionLast != m_OutputNumber) && (PtrInfo->LastTimeStamp.QuadPart > FrameInfo->LastMouseUpdateTime.QuadPart))
	{
		UpdatePosition = false;
	}

	// Update position
	if (UpdatePosition)
	{
		PtrInfo->Position.x = FrameInfo->PointerPosition.Position.x + m_OutputDesc.DesktopCoordinates.left - OffsetX;
		PtrInfo->Position.y = FrameInfo->PointerPosition.Position.y + m_OutputDesc.DesktopCoordinates.top - OffsetY;
		PtrInfo->WhoUpdatedPositionLast = m_OutputNumber;
		PtrInfo->LastTimeStamp = FrameInfo->LastMouseUpdateTime;
		PtrInfo->Visible = FrameInfo->PointerPosition.Visible != 0;
	}

	// No new shape
	if (FrameInfo->PointerShapeBufferSize == 0)
	{
		return S_OK;
	}

	// Old buffer too small
	if (FrameInfo->PointerShapeBufferSize > PtrInfo->BufferSize)
	{
		if (PtrInfo->PtrShapeBuffer)
		{
			delete[] PtrInfo->PtrShapeBuffer;
			PtrInfo->PtrShapeBuffer = nullptr;
		}
		PtrInfo->PtrShapeBuffer = new (std::nothrow) BYTE[FrameInfo->PointerShapeBufferSize];
		if (!PtrInfo->PtrShapeBuffer)
		{
			PtrInfo->BufferSize = 0;
			return ProcessFailure(nullptr, _T("Failed to allocate memory for pointer shape in DUPLICATIONMANAGER"), _T("Error"), E_OUTOFMEMORY);
		}

		// Update buffer size
		PtrInfo->BufferSize = FrameInfo->PointerShapeBufferSize;
	}

	// Get shape
	UINT BufferSizeRequired;
	HRESULT hr = m_DeskDupl->GetFramePointerShape(FrameInfo->PointerShapeBufferSize, reinterpret_cast<VOID*>(PtrInfo->PtrShapeBuffer), &BufferSizeRequired, &(PtrInfo->ShapeInfo));

	if (FAILED(hr))
	{
		delete[] PtrInfo->PtrShapeBuffer;
		PtrInfo->PtrShapeBuffer = nullptr;
		PtrInfo->BufferSize = 0;
		return ProcessFailure(m_Device, _T("Failed to get frame pointer shape in DUPLICATIONMANAGER"), _T("Error"), hr);//, FrameInfoExpectedErrors);
	}

	return S_OK;
}


//
// Get next frame and write it into Data
//
_Success_(*Timeout == false && return == DUPL_RETURN_SUCCESS)
HRESULT cDuplicationManager::GetFrame(_Out_ FRAME_DATA* Data, int timeout, _Out_ bool* Timeout)
{
	IDXGIResource* DesktopResource = nullptr;
	DXGI_OUTDUPL_FRAME_INFO FrameInfo;

	try
	{
		// If still holding old frame, destroy it
		if (m_AcquiredDesktopImage)
		{
			m_AcquiredDesktopImage->Release();
			m_AcquiredDesktopImage = nullptr;
		}

		// Get new frame
		HRESULT hr = m_DeskDupl->AcquireNextFrame(timeout, &FrameInfo, &DesktopResource);

		if (hr == DXGI_ERROR_WAIT_TIMEOUT)
		{
			*Timeout = true;
			return S_OK;
		}

		if (FAILED(hr))
		{
			return ProcessFailure(m_Device, _T("Failed to acquire next frame in DUPLICATIONMANAGER"), _T("Error"), hr);//, FrameInfoExpectedErrors);
		}

		if (DesktopResource)
		{
			// QI for IDXGIResource
			hr = DesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&m_AcquiredDesktopImage));
			DesktopResource->Release();
			DesktopResource = nullptr;
		}

		if (FAILED(hr))
		{
			return ProcessFailure(nullptr, _T("Failed to QI for ID3D11Texture2D from acquired IDXGIResource in DUPLICATIONMANAGER"), _T("Error"), hr);
		}

		// Get metadata
		if (FrameInfo.TotalMetadataBufferSize)
		{
			// Old buffer too small
			if (FrameInfo.TotalMetadataBufferSize > m_MetaDataSize)
			{
				if (m_MetaDataBuffer)
				{
					delete[] m_MetaDataBuffer;
					m_MetaDataBuffer = nullptr;
				}

				m_MetaDataBuffer = new (std::nothrow) BYTE[FrameInfo.TotalMetadataBufferSize];

				if (!m_MetaDataBuffer)
				{
					m_MetaDataSize = 0;
					Data->MoveCount = 0;
					Data->DirtyCount = 0;
					return ProcessFailure(nullptr, _T("Failed to allocate memory for metadata in DUPLICATIONMANAGER"), _T("Error"), E_OUTOFMEMORY);
				}

				m_MetaDataSize = FrameInfo.TotalMetadataBufferSize;
			}


			UINT BufSize = FrameInfo.TotalMetadataBufferSize;

			// Get move rectangles


			hr = m_DeskDupl->GetFrameMoveRects(BufSize, reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(m_MetaDataBuffer), &BufSize);

			if (FAILED(hr))
			{
				Data->MoveCount = 0;
				Data->DirtyCount = 0;
				return ProcessFailure(nullptr, L"Failed to get frame move rects in DUPLICATIONMANAGER", L"Error", hr);//, FrameInfoExpectedErrors);

			}

			Data->MoveCount = BufSize / sizeof(DXGI_OUTDUPL_MOVE_RECT);

			BYTE* DirtyRects = m_MetaDataBuffer + BufSize;
			BufSize = FrameInfo.TotalMetadataBufferSize - BufSize;

			// Get dirty rectangles
			hr = m_DeskDupl->GetFrameDirtyRects(BufSize, reinterpret_cast<RECT*>(DirtyRects), &BufSize);

			if (FAILED(hr))
			{
				Data->MoveCount = 0;
				Data->DirtyCount = 0;
				return ProcessFailure(nullptr, _T("Failed to get frame dirty rects in DUPLICATIONMANAGER"), _T("Error"), hr);//, FrameInfoExpectedErrors);
			}

			Data->DirtyCount = BufSize / sizeof(RECT);

			Data->MetaData = m_MetaDataBuffer;
		}

		Data->Frame = m_AcquiredDesktopImage;
		Data->FrameInfo = FrameInfo;

	}
	catch (...)
	{
		return S_FALSE;
	}

	return S_OK;
}

//
// Release frame
//
HRESULT cDuplicationManager::DoneWithFrame()
{
	if (m_AcquiredDesktopImage)
	{
		HRESULT hr = m_DeskDupl->ReleaseFrame();

		if (FAILED(hr))
		{
			return ProcessFailure(m_Device, _T("Failed to release frame in DUPLICATIONMANAGER"), _T("Error"), hr);//, FrameInfoExpectedErrors);
		}

		m_AcquiredDesktopImage->Release();
		m_AcquiredDesktopImage = nullptr;
	}

	return S_OK;
}

//
// Gets output desc into DescPtr
//
void cDuplicationManager::GetOutputDesc(_Out_ DXGI_OUTPUT_DESC* DescPtr)
{
	*DescPtr = m_OutputDesc;
}

void cDuplicationManager::Reset()
{
	CleanUp();
	m_DeskDupl = nullptr;
	m_AcquiredDesktopImage = nullptr;
	m_MetaDataBuffer = nullptr;
	m_MetaDataSize = 0;
	m_OutputNumber = 0;
	m_Device = nullptr;
	RtlZeroMemory(&m_OutputDesc, sizeof(m_OutputDesc));
}

void cDuplicationManager::CleanUp()
{
	if (m_DeskDupl)
	{
		m_DeskDupl->Release();
		m_DeskDupl = nullptr;
	}

	if (m_AcquiredDesktopImage)
	{
		m_AcquiredDesktopImage->Release();
		m_AcquiredDesktopImage = nullptr;
	}

	if (m_MetaDataBuffer)
	{
		delete[] m_MetaDataBuffer;
		m_MetaDataBuffer = nullptr;
	}

	if (m_Device)
	{
		m_Device->Release();
		m_Device = nullptr;
	}
}

