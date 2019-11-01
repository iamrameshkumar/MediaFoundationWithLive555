/************************************************************************************************************************************************************************************************************
*FILE:  Id3d11ImageCapturingModule.cpp
*
*DESCRIPTION - Contains methods to interface DirectX APIs to capture the screen
*
*
*Date: OCT 2016
**************************************************************************************************************************************************************************************************************/

/**************************************************************************************************************************************************************************************************************
*
* Includes
*
/**************************************************************************************************************************************************************************************************************/

#include "commontypes.h"
#include "DesktopDuplication.h"
#include "tchar.h"
#include "stdint.h"
#include <algorithm>
#include <ppl.h>

#include <mfplay.h>
#include <mfapi.h>
#include <mfreadwrite.h>
#include <mferror.h>

using namespace concurrency;

/**************************************************************************************************************************************************************************************************************
*
* mactro declarations
*
/**************************************************************************************************************************************************************************************************************/

#pragma comment(lib, "dxgi.lib")

/**************************************************************************************************************************************************************************************************************
*
* Static variable and enum declarations
*
/**************************************************************************************************************************************************************************************************************/

static const D3D_DRIVER_TYPE m_DriverTypes[] = {

	//Hardware based Rasterizer
	D3D_DRIVER_TYPE_HARDWARE,

	//High performance Software Rasterizer
	D3D_DRIVER_TYPE_WARP,

	//Software Rasterizer (Low performance but more accurate)
	D3D_DRIVER_TYPE_REFERENCE,

	//TODO: Explore other driver types
};


static const D3D_FEATURE_LEVEL m_FeatureLevel[] = {

	D3D_FEATURE_LEVEL_11_1,
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_3,
	D3D_FEATURE_LEVEL_9_2,
	D3D_FEATURE_LEVEL_9_1

	//TODO: Explore other features levels as well
};

static const D3D_FEATURE_LEVEL m_FeatureLevelFallback[] = {

	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_1

	//TODO: Explore other features levels as well
};


static int	m_DriversCount = ARRAYSIZE(m_DriverTypes);
static int	m_FeatureLevelsCount = ARRAYSIZE(m_FeatureLevel);;
static int	m_FallbackFeatureLevelsCount = ARRAYSIZE(m_FeatureLevelFallback);;;

/**************************************************************************************************************************************************************************************************************
*
* Function definitions
*
/**************************************************************************************************************************************************************************************************************/

cImageCapturingModuleId3d11Impl::cImageCapturingModuleId3d11Impl() :m_DuplicationManager()
{
	m_Id3d11DeviceContext = NULL;
	m_Id3d11Device = NULL;
	m_CurrentFrameTexture = NULL;
	m_BackUpTexture = NULL;
	m_LastErrorCode = S_OK;
	m_DllHandleD3D11 = NULL;
	m_FnD3D11CreateDevice = NULL;

	_pPreviousFrameImage = NULL;
	_pCurrentFrameImage = NULL;
}

cImageCapturingModuleId3d11Impl::~cImageCapturingModuleId3d11Impl()
{
	if (m_Id3d11DeviceContext)
	{
		m_Id3d11DeviceContext->Release();
		m_Id3d11DeviceContext = NULL;
	}

	if (m_Id3d11Device)
	{
		m_Id3d11Device->Release();

		m_Id3d11Device = NULL;
	}
}

DWORD cImageCapturingModuleId3d11Impl::loadD3D11FunctionsFromDll()
{
	DWORD errorCode = ERROR_INVALID_ACCESS;
	m_DllHandleD3D11 = LoadLibrary(D3_D11_DLL);

	if (m_DllHandleD3D11 != NULL)
	{
		m_FnD3D11CreateDevice = (D3D11CreateDeviceFunType)::GetProcAddress(m_DllHandleD3D11, "D3D11CreateDevice");

		if (m_FnD3D11CreateDevice == NULL)
		{
			errorCode = GetLastError();
		}
		else
		{
			errorCode = ERROR_SUCCESS;
		}
	}
	else
	{
		errorCode = GetLastError();
	}

	return errorCode;
}


/************************************************************************************************************************************************************************************************************
*Function:  initDevice
*
*DESCRIPTION - Initializes the Graphics processor for screen capture for the given adapter
*
*Future Enhancements:
*
* Returns	   Success or Failure
*
*Date: DEC 2016
**************************************************************************************************************************************************************************************************************/

HRESULT cImageCapturingModuleId3d11Impl::initDevice(_In_opt_		IDXGIAdapter	*pAdapter)
{
	DWORD errorCode = ERROR_SUCCESS;

	if (m_FnD3D11CreateDevice == NULL)
	{
		errorCode = loadD3D11FunctionsFromDll();
	}

	if (m_Id3d11Device)
	{
		m_Id3d11Device->Release();
		m_Id3d11Device = NULL;
	}

	if (errorCode == ERROR_SUCCESS)
	{
		if (m_FnD3D11CreateDevice) {

			for (UINT driverTypeIndex = 0; driverTypeIndex < m_DriversCount; ++driverTypeIndex)
			{
				m_LastErrorCode = D3D11CreateDevice(nullptr, m_DriverTypes[driverTypeIndex], nullptr, D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
					m_FeatureLevel, m_FeatureLevelsCount, D3D11_SDK_VERSION, &m_Id3d11Device, &m_SelectedFeatureLevel, &m_Id3d11DeviceContext);

				if (SUCCEEDED(m_LastErrorCode))
				{
					break;
				}
			}
		}

		if (SUCCEEDED(m_LastErrorCode))
		{
			// Create device manager
			UINT resetToken;
			HRESULT hr = MFCreateDXGIDeviceManager(&resetToken, &deviceManager);

			hr = deviceManager->ResetDevice(m_Id3d11Device, resetToken);
		}
	}
	else
	{
		m_LastErrorCode = E_POINTER;
	}

	return m_LastErrorCode;
}

BOOL cImageCapturingModuleId3d11Impl::IsDeviceReady()
{
	return m_DuplicationManager.IsDeviceReady();
}

HRESULT cImageCapturingModuleId3d11Impl::initDuplicationManager(IDXGIAdapter *_pAdapter, IDXGIOutput *_pOutput, int outputNumber)
{
	HRESULT hrErrorCode = ERROR_INVALID_ADDRESS;

	if (m_Id3d11Device != NULL)
	{
		hrErrorCode = m_DuplicationManager.InitDupl(m_Id3d11Device, _pAdapter, _pOutput, outputNumber);

		if (SUCCEEDED(hrErrorCode))
		{
			if (!m_DuplicationManager.IsDeviceReady())
			{
				hrErrorCode = E_FAIL;
			}
		}
	}

	return hrErrorCode;
}

void cImageCapturingModuleId3d11Impl::ReleaseAdapters() {

	if (!vAdapters_map.empty())
	{
		for (std::vector <std::pair<std::pair< std::pair<IDXGIAdapter1*, IDXGIOutput*>, MONITOR*>, UINT>>::iterator i = vAdapters_map.begin(); i != vAdapters_map.end(); i++)
		{
			IDXGIAdapter1* adapter = (*i).first.first.first;
			if (adapter)
			{
				adapter->Release();
				adapter = nullptr;
			}

			IDXGIOutput* monitor = (*i).first.first.second;
			if (monitor)
			{
				monitor->Release();
				monitor = nullptr;
			}

			MONITOR *pMonitor = (*i).first.second;

			if (pMonitor) {
				delete pMonitor;
				pMonitor = nullptr;
			}
		}

		vAdapters_map.clear();
	}
}

void cImageCapturingModuleId3d11Impl::DoCleanup()
{
	try
	{
		m_DuplicationManager.Reset();

		if (_pCurrentFrameImage) { _pCurrentFrameImage->Release(); _pCurrentFrameImage = NULL; } //memory leak here, need to fix it
		if (_pPreviousFrameImage) { _pPreviousFrameImage->Release(); _pPreviousFrameImage = NULL; }

		ReleaseAdapters();
	}
	catch (std::exception &e)
	{

	}
}


/************************************************************************************************************************************************************************************************************
*Function:  getChangedRegions
*
*DESCRIPTION - Fethes the diff rects between current and previous query to this function (returns the full screen on init or reset)
*
*Future Enhancements:
*
* Returns	   Success or Failure
*
*Date: DEC 2016
**************************************************************************************************************************************************************************************************************/

INT cImageCapturingModuleId3d11Impl::getChangedRegions(int timeout, bool &isTimeOut, rectangles &dirtyRects, std::vector <DXGI_OUTDUPL_MOVE_RECT> &moveRects, UINT &rect_count, RECT ScreenRect)
{
	UINT diffArea = 0;
	FRAME_DATA currentFrameData;

	TRY
	{

		m_LastErrorCode = m_DuplicationManager.GetFrame(&currentFrameData, timeout, &isTimeOut);

		if (SUCCEEDED(m_LastErrorCode) && (!isTimeOut))
		{
			if (currentFrameData.FrameInfo.TotalMetadataBufferSize)
			{

				m_CurrentFrameTexture = currentFrameData.Frame;

				if (currentFrameData.MoveCount)
				{
					DXGI_OUTDUPL_MOVE_RECT* moveRectArray = reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*> (currentFrameData.MetaData);

					if (moveRectArray)
					{
						for (UINT index = 0; index < currentFrameData.MoveCount; index++)
						{
							//WebRTC
							// DirectX capturer API may randomly return unmoved move_rects, which should
							// be skipped to avoid unnecessary wasting of differing and encoding
							// resources.
							// By using testing application it2me_standalone_host_main, this check
							// reduces average capture time by 0.375% (4.07 -> 4.055), and average
							// encode time by 0.313% (8.042 -> 8.016) without other impacts.

							if (moveRectArray[index].SourcePoint.x != moveRectArray[index].DestinationRect.left || moveRectArray[index].SourcePoint.y != moveRectArray[index].DestinationRect.top)
							{
								moveRects.push_back(moveRectArray[index]);
								diffArea += abs((moveRectArray[index].DestinationRect.right - moveRectArray[index].DestinationRect.left) *
									(moveRectArray[index].DestinationRect.bottom - moveRectArray[index].DestinationRect.top));
							}
						}
					}
					else
					{
						return -1;
					}
				}

				if (currentFrameData.DirtyCount)
				{
					RECT* dirtyRectArray = reinterpret_cast<RECT*> (currentFrameData.MetaData + (currentFrameData.MoveCount * sizeof(DXGI_OUTDUPL_MOVE_RECT)));

					if (!dirtyRectArray)
					{
						return -1;
					}

					rect_count = currentFrameData.DirtyCount;

					for (UINT index = 0; index < rect_count; index++)
					{

						diffArea += abs((dirtyRectArray[index].right - dirtyRectArray[index].left) *
							(dirtyRectArray[index].bottom - dirtyRectArray[index].top));

						dirtyRects.push_back(dirtyRectArray[index]);
					}

				}

			}
#if defined (DEBUG) || defined (_DEBUG)
			else
			{
				LogCritical(_T("ImageCapturingModuleId3d11Impl::getChangedRegions: Total Meta data buffer size is empty."));
			}
#endif
		}
		else if (!isTimeOut)
		{
			return -1;
		}

		return diffArea;

	}

	CATCH_ALL(e)
	{
		
	}
	END_CATCH_ALL

	return -1;
}


std::vector <IDXGIOutput*> cImageCapturingModuleId3d11Impl::EnumMonitors(IDXGIAdapter1* pSelectedAdapter)
{

	std::vector <IDXGIOutput*> vOutputs;

	if (pSelectedAdapter)
	{
		IDXGIOutput* pOutput = nullptr;
		for (UINT i = 0; pSelectedAdapter->EnumOutputs(i, &pOutput) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			vOutputs.push_back(pOutput);
			pOutput = nullptr;
		}
	}

	return vOutputs;
}

std::vector <IDXGIAdapter1*> cImageCapturingModuleId3d11Impl::EnumerateAdapters(void)
{
	IDXGIAdapter1 * pAdapter = nullptr;
	std::vector <IDXGIAdapter1*> vAdapters;
	IDXGIFactory1* pFactory = NULL;

	// Create a DXGIFactory object.
	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory)))
	{
		return vAdapters;
	}


	for (UINT i = 0;
		pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND;
		++i)
	{
		vAdapters.push_back(pAdapter);
		pAdapter = nullptr;
	}


	if (pFactory)
	{
		pFactory->Release();
	}

	return vAdapters;

}

/************************************************************************************************************************************************************************************************************
*Function:  GetAllAvailableMonitors
*
*DESCRIPTION - Enumerates all the monitors present in the system by looping through all the physical and logical adapters and returns the pointer to monitor's OUTPUT struct
*
*Future Enhancements:
*
* Returns	   Success or Failure
*
*Date: DEC 2016
**************************************************************************************************************************************************************************************************************/

BOOL cImageCapturingModuleId3d11Impl::GetAllAvailableMonitors()
{

	try
	{
		ReleaseAdapters();

		std::vector <IDXGIAdapter1*> vAdapters = EnumerateAdapters();

		UINT adapterCount = 1;

		for (std::vector <IDXGIAdapter1*>::iterator i = vAdapters.begin(); i != vAdapters.end(); i++)
		{

			IDXGIAdapter1* _padapter = *i;

			std::vector <IDXGIOutput*> vOutputs = EnumMonitors(_padapter);

			UINT outputCount = 0;

			for (std::vector <IDXGIOutput*>::iterator j = vOutputs.begin(); j != vOutputs.end(); j++)
			{
				IDXGIOutput *out = *j;

				if (out)
				{
					DXGI_OUTPUT_DESC desktopDesc;
					out->GetDesc(&desktopDesc);

					MONITOR *s_mon = new MONITOR;
					s_mon->hMon = desktopDesc.Monitor;
					s_mon->hDC = NULL;
					s_mon->scrRect = new RECT();
					s_mon->pRect = new RECT();
					//Physical Monitor border
					s_mon->scrRect->left = s_mon->pRect->left = desktopDesc.DesktopCoordinates.left;
					s_mon->scrRect->top = s_mon->pRect->top = desktopDesc.DesktopCoordinates.top;
					s_mon->scrRect->right = s_mon->pRect->right = desktopDesc.DesktopCoordinates.right;
					s_mon->scrRect->bottom = s_mon->pRect->bottom = desktopDesc.DesktopCoordinates.bottom;
					s_mon->data = NULL;

					_padapter->AddRef();
					vAdapters_map.push_back(std::make_pair(std::make_pair(std::make_pair(_padapter, out), s_mon), outputCount));

					outputCount++;

					//out->Release();  // Do not release outputs, we would be using them to init the device corresponding to a particular monitor
					//out = NULL;
				}

				adapterCount++;
			}

			//(*i)->Release(); // Do not release adapters, we would be using them to init the device corresponding to a particular monitor
		}

		UINT num_of_monitors = vAdapters_map.size();

		return (num_of_monitors > 0);
	}
	catch (...)
	{
		
	}

	return FALSE;
}

BOOL cImageCapturingModuleId3d11Impl::populateMonitorDetails()
{
	BOOL result = TRUE;

	result = GetAllAvailableMonitors();

	/*if( SUCCEEDED( initDevice() ) )
	{
		if(m_Id3d11Device != NULL)
		{
			IDXGIDevice* dxgiDevice = nullptr;
			m_LastErrorCode = m_Id3d11Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));

			if(SUCCEEDED(m_LastErrorCode))
			{
				IDXGIAdapter* dxgiAdapter = nullptr;
				m_LastErrorCode = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&dxgiAdapter));
				dxgiDevice->Release();
				dxgiDevice = nullptr;

				if(SUCCEEDED(m_LastErrorCode))
				{
					IDXGIOutput* dxgiOutput = nullptr;
					UINT outputCount = 0;
					for(outputCount = 0; SUCCEEDED(m_LastErrorCode); ++outputCount)
					{
						if(dxgiOutput)
						{
							dxgiOutput->Release();
							dxgiOutput = nullptr;
						}

						m_LastErrorCode = dxgiAdapter->EnumOutputs(outputCount, &dxgiOutput);

						if(dxgiOutput && (m_LastErrorCode != DXGI_ERROR_NOT_FOUND))
						{
							DXGI_OUTPUT_DESC desktopDesc;
							dxgiOutput->GetDesc(&desktopDesc);

							MONITOR * s_mon = new MONITOR ( ) ;
							s_mon.hMon = desktopDesc.Monitor;
							s_mon.hDC = NULL;
							s_mon.scrRect = new RECT ();
							s_mon.pRect = new RECT ();

							//Physical Monitor border
							s_mon.pRect->left   =  desktopDesc.DesktopCoordinates.left;
							s_mon.pRect->top    =  desktopDesc.DesktopCoordinates.top;
							s_mon.pRect->right  =  desktopDesc.DesktopCoordinates.right;
							s_mon.pRect->bottom =  desktopDesc.DesktopCoordinates.bottom;

							s_mon.data = NULL;
							LogNormal(_T("DXGI Monitor Info: Monitor Number: %d L:%d T:%d R:%d B:%d"),
							outputCount + 1, desktopDesc.DesktopCoordinates.left, desktopDesc.DesktopCoordinates.top,
							desktopDesc.DesktopCoordinates.right, desktopDesc.DesktopCoordinates.bottom);

							monitors_info.AddMonitor(s_mon);
						}
					}

					dxgiAdapter->Release();
					dxgiAdapter = nullptr;
					--outputCount;

					monitors_info.monCount = outputCount;

					LogCritical(_T("cImageCapturingModuleId3d11Impl::populateMonitorVariables - Total monitors - %d"), outputCount);
				}
				else
				{
					result = FALSE;
					LogCritical(_T("cImageCapturingModuleId3d11Impl::populateMonitorVariables - Unable to get IDXGIAdapter - %ld"), m_LastErrorCode);
				}
			}
			else
			{
				result = FALSE;
				LogCritical(_T("cImageCapturingModuleId3d11Impl::populateMonitorVariables - Unable to get IDXGIDevice - %ld"), m_LastErrorCode);
			}
		}
		else
		{
			result = FALSE;
			LogCritical(_T("cImageCapturingModuleId3d11Impl::populateMonitorVariables - D3D11Device not initialized."));
		}
	}
	else
	{
		result = FALSE;
	}*/

	return result;

}

bool cImageCapturingModuleId3d11Impl::getBitmapFromTextureId3d11(ID3D11Texture2D* texture, HBITMAP* outputBitmap)
{
	bool result = false;
	if ((texture != NULL) && (m_Id3d11Device != NULL) && (m_Id3d11DeviceContext != NULL))
	{
		D3D11_TEXTURE2D_DESC textureDesc, requiredTextureDesc;
		texture->GetDesc(&textureDesc);
		texture->GetDesc(&requiredTextureDesc);

		ID3D11Texture2D* pNewTexture = NULL;
		requiredTextureDesc.BindFlags = 0;
		requiredTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		requiredTextureDesc.Usage = D3D11_USAGE_STAGING;
		requiredTextureDesc.MiscFlags = 0;

		m_LastErrorCode = m_Id3d11Device->CreateTexture2D(&requiredTextureDesc, NULL, &pNewTexture);

		if (pNewTexture != NULL)
		{
			m_Id3d11DeviceContext->CopyResource(pNewTexture, texture);

			D3D11_MAPPED_SUBRESOURCE mappedSubresource;
			UINT subresource = D3D11CalcSubresource(0, 0, 0);
			m_LastErrorCode = m_Id3d11DeviceContext->Map(pNewTexture, subresource, D3D11_MAP_READ, 0, &mappedSubresource);

			if (SUCCEEDED(m_LastErrorCode))
			{
				// COPY from texture to bitmap buffer
				uint8_t* sptr = reinterpret_cast<uint8_t*>(mappedSubresource.pData);
				int destBufferSize = textureDesc.Width*textureDesc.Height * 2;
				uint8_t* dptr = new uint8_t[destBufferSize];

				// TODO: This approach wont work in all cases.
				// This will fail when Img stride is lesser than width.
				// Proper way is to loop through height considering min(stride, width)
				memcpy_s(dptr, destBufferSize, sptr, destBufferSize);

				BITMAPINFOHEADER bmih;
				bmih.biSize = sizeof(BITMAPINFOHEADER);
				bmih.biWidth = textureDesc.Width;
				bmih.biHeight = -textureDesc.Height;
				bmih.biPlanes = 1;
				bmih.biBitCount = 32;
				bmih.biCompression = BI_RGB;
				bmih.biSizeImage = 0;
				bmih.biXPelsPerMeter = 0;
				bmih.biYPelsPerMeter = 0;
				bmih.biClrUsed = 0;
				bmih.biClrImportant = 0;

				BITMAPINFO dbmi;
				ZeroMemory(&dbmi, sizeof(dbmi));
				dbmi.bmiHeader = bmih;
				dbmi.bmiColors->rgbBlue = 0;
				dbmi.bmiColors->rgbGreen = 0;
				dbmi.bmiColors->rgbRed = 0;
				dbmi.bmiColors->rgbReserved = 0;
				void* bits = (void*)&(dptr[0]);

				// ALTER: CreateCompatibleBitmap with SetDibBits can also be used if needed.
				*outputBitmap = CreateDIBSection(::GetDC(NULL), &dbmi, DIB_RGB_COLORS, &bits, NULL, 0);
				if (*outputBitmap != NULL)
				{
					memcpy(bits, dptr, destBufferSize);
					result = true;
				}

				m_Id3d11DeviceContext->Unmap(pNewTexture, subresource);
				//m_Id3d11DeviceContext->Release();
				delete[] dptr;
				pNewTexture->Release();
				pNewTexture = NULL;
			}
			else
			{

			}
		}
		else
		{
			m_LastErrorCode = m_Id3d11Device->GetDeviceRemovedReason();
		}
	}
	else
	{

	}

	return result;
}

bool cImageCapturingModuleId3d11Impl::GetCurrentFrameAsVideoSample(void **videoSample, void **pMediaBuffer, bool &isTimeout, CRect &deviceRect, int surfaceWidth, int surfaceHeight) {

	ID3D11Texture2D *pNewTexture = NULL;
	D3D11_TEXTURE2D_DESC desc = { 0 };

	desc.Format = DXGI_FORMAT_NV12;
	desc.Width = surfaceWidth;
	desc.Height = surfaceHeight;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	//desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	//desc.Usage = D3D11_USAGE_STAGING;

	m_LastErrorCode = m_Id3d11Device->CreateTexture2D(&desc, NULL, &pNewTexture);

	if (pNewTexture) {

		MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), pNewTexture, 0, FALSE, (IMFMediaBuffer**)pMediaBuffer);

		CComPtr<IMF2DBuffer> p2DBuffer;
		DWORD length;
		(*((IMFMediaBuffer**)pMediaBuffer))->QueryInterface(__uuidof(IMF2DBuffer), reinterpret_cast<void **>(&p2DBuffer));
		p2DBuffer->GetContiguousLength(&length);
		(*((IMFMediaBuffer**)pMediaBuffer))->SetCurrentLength(length);

		//MFCreateVideoSampleFromSurface(NULL, (IMFSample**)videoSample);
		MFCreateSample((IMFSample**)videoSample);
		(*((IMFSample**)videoSample))->AddBuffer((*((IMFMediaBuffer**)pMediaBuffer)));

		pNewTexture->Release();
		return true;
}

	return false;
}

bool cImageCapturingModuleId3d11Impl::GetCurrentFrameAsVideoSampleExp(void **videoSample, void **pMediaBuffer, bool &isTimeout, CRect &deviceRect, int surfaceWidth, int surfaceHeight) {

	FRAME_DATA currentFrameData;
	ID3D11Texture2D *pNewTexture = NULL;

#if 0
	m_LastErrorCode = m_DuplicationManager.GetFrame(&currentFrameData, 0, &isTimeout);

	if (SUCCEEDED(m_LastErrorCode) && (!isTimeout)) {

		m_CurrentFrameTexture = currentFrameData.Frame;

		D3D11_TEXTURE2D_DESC requiredTextureDesc;
		m_CurrentFrameTexture->GetDesc(&requiredTextureDesc);


		requiredTextureDesc.Format = DXGI_FORMAT_NV12;
		requiredTextureDesc.BindFlags = 0;
		requiredTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;// | D3D11_CPU_ACCESS_WRITE;
		requiredTextureDesc.Usage = D3D11_USAGE_STAGING;
		requiredTextureDesc.MiscFlags = 0;

		requiredTextureDesc.Width = surfaceWidth;
		requiredTextureDesc.Height = surfaceHeight;

		m_LastErrorCode = m_Id3d11Device->CreateTexture2D(&requiredTextureDesc, NULL, &pNewTexture);

	}
#endif

	m_LastErrorCode = m_DuplicationManager.GetFrame(&currentFrameData, 100, &isTimeout);

	if (SUCCEEDED(m_LastErrorCode) && (!isTimeout)) {

		m_CurrentFrameTexture = currentFrameData.Frame;

		D3D11_TEXTURE2D_DESC desc = { 0 };
		m_CurrentFrameTexture->GetDesc(&desc);

		desc.Format = DXGI_FORMAT_NV12;
		desc.Width = surfaceWidth;
		desc.Height = surfaceHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		//desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		//desc.Usage = D3D11_USAGE_STAGING;

		m_LastErrorCode = m_Id3d11Device->CreateTexture2D(&desc, NULL, &pNewTexture);

		if (pNewTexture) {

			D3D11_BOX box;
			box.left = deviceRect.left;
			box.top = deviceRect.top;
			box.right = deviceRect.right;
			box.bottom = deviceRect.bottom;
			box.front = 0;
			box.back = 1;

			// Copy diff area texels to new temp texture
			m_Id3d11DeviceContext->CopySubresourceRegion(pNewTexture, 0, 0, 0, 0, m_CurrentFrameTexture, 0, &box);

			MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), pNewTexture, 0, FALSE, (IMFMediaBuffer**)pMediaBuffer);

			CComPtr<IMF2DBuffer> p2DBuffer;
			DWORD length;
			(*((IMFMediaBuffer**)pMediaBuffer))->QueryInterface(__uuidof(IMF2DBuffer), reinterpret_cast<void **>(&p2DBuffer));
			p2DBuffer->GetContiguousLength(&length);
			(*((IMFMediaBuffer**)pMediaBuffer))->SetCurrentLength(length);

			//MFCreateVideoSampleFromSurface(NULL, (IMFSample**)videoSample);
			MFCreateSample((IMFSample**)videoSample);
			(*((IMFSample**)videoSample))->AddBuffer((*((IMFMediaBuffer**)pMediaBuffer)));

			pNewTexture->Release();
			return true;
		}
	}

	return false;
}


SmartPtr<BitmapData>* cImageCapturingModuleId3d11Impl::GetCurrentFrameImage(CRect &rect)
{

	SmartPtr<BitmapData> *currentFrameImg = NULL;

	TRY
	{
		if ((m_CurrentFrameTexture != NULL) && (m_Id3d11Device != NULL) && (m_Id3d11DeviceContext != NULL))
		{
			D3D11_TEXTURE2D_DESC requiredTextureDesc;
			m_CurrentFrameTexture->GetDesc(&requiredTextureDesc);

			ID3D11Texture2D* pNewTexture = NULL;
			requiredTextureDesc.BindFlags = 0;
			requiredTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;// | D3D11_CPU_ACCESS_WRITE;
			requiredTextureDesc.Usage = D3D11_USAGE_STAGING;
			requiredTextureDesc.MiscFlags = 0;

			requiredTextureDesc.Width = rect.Width();
			requiredTextureDesc.Height = rect.Height();

			m_LastErrorCode = m_Id3d11Device->CreateTexture2D(&requiredTextureDesc, NULL, &pNewTexture);
			if (pNewTexture != NULL)
			{

				D3D11_BOX box;
				box.left = rect.left;
				box.top = rect.top;
				box.right = rect.right;
				box.bottom = rect.bottom;
				box.front = 0;
				box.back = 1;

				// Copy diff area texels to new temp texture
				m_Id3d11DeviceContext->CopySubresourceRegion(pNewTexture, 0, 0, 0, 0, m_CurrentFrameTexture, 0, &box);

				// TODO: Analyze whether getting bytes from IDXGISurface will improve any performance.

				// Map GPU buffer for CPU access (This will give a locked buffer)
				D3D11_MAPPED_SUBRESOURCE mappedSubresource;
				UINT subresource = D3D11CalcSubresource(0, 0, 0);
				m_LastErrorCode = m_Id3d11DeviceContext->Map(pNewTexture, subresource, D3D11_MAP_READ, 0, &mappedSubresource);

				if (SUCCEEDED(m_LastErrorCode))
				{
					// Get pointer of buffer locked from GPU
					uint8_t* sptr = reinterpret_cast<uint8_t*>(mappedSubresource.pData);

					if (!_pCurrentFrameImage)
					{
						_pCurrentFrameImage = (new BitmapData())->template DetachObject<BitmapData>();
						_pCurrentFrameImage->bmpSize = mappedSubresource.RowPitch * requiredTextureDesc.Height; //  Crash occurs when we use this _pCurrentFrameImage->height = textureDesc.Height;, as the device gives actual resolution instead of curren RES
						_pCurrentFrameImage->lpbitmap = new byte[_pCurrentFrameImage->bmpSize];
					}

					if (_pCurrentFrameImage && _pCurrentFrameImage->lpbitmap)
					{
						_pCurrentFrameImage->bitsPerPixel = 32;
						_pCurrentFrameImage->bytesPerPixel = _pCurrentFrameImage->bitsPerPixel / 8;
						_pCurrentFrameImage->bytesPerRow = mappedSubresource.RowPitch;
						_pCurrentFrameImage->height = requiredTextureDesc.Height; //  Crash occurs when we use this _pCurrentFrameImage->height = textureDesc.Height;, as the device gives actual resolution instead of curren RES
						_pCurrentFrameImage->width = mappedSubresource.RowPitch / 4;
						//_pCurrentFrameImage->bmpSize = mappedSubresource.RowPitch * textureDesc.Height;
						_pCurrentFrameImage->RowPitch = mappedSubresource.RowPitch;
						memcpy(_pCurrentFrameImage->lpbitmap, sptr, _pCurrentFrameImage->bmpSize); // not using memcpy to avoid time consumption
						//_pCurrentFrameImage->lpbitmap = reinterpret_cast<uint8_t*>( mappedSubresource.pData );
					}

					m_Id3d11DeviceContext->Unmap(pNewTexture, subresource);

					currentFrameImg = &_pCurrentFrameImage;
				}

				// Release subresource region texture - New diffrect texture will be created in Next iteration
				pNewTexture->Release();
				pNewTexture = NULL;
			}
			else
			{
				HRESULT DeviceRemovedReason = m_Id3d11Device->GetDeviceRemovedReason();
				currentFrameImg = NULL;
			}
		}
	}
	CATCH_ALL(e)
	{
		
	}
	END_CATCH_ALL

	return currentFrameImg;
}


SmartPtr<BitmapData>* cImageCapturingModuleId3d11Impl::GetPreviousImage(bool bCopyCurrentIfNotAvailable /*= false*/)
{
	if (!_pPreviousFrameImage && _pCurrentFrameImage && bCopyCurrentIfNotAvailable)
	{
		_pPreviousFrameImage = (new BitmapData(_pCurrentFrameImage))->template DetachObject<BitmapData>();
	}

	return (_pPreviousFrameImage ? &(_pPreviousFrameImage) : NULL);
}

SmartPtr<BitmapData>* cImageCapturingModuleId3d11Impl::GetCurrentImage(CRect &rect, BOOL _bCaptureNew)
{
	if (_bCaptureNew)
	{
		return GetCurrentFrameImage(rect);
	}

	return (_pCurrentFrameImage ? &_pCurrentFrameImage : NULL);
}

void cImageCapturingModuleId3d11Impl::releaseBuffer()
{
	if ((m_Id3d11DeviceContext != NULL) && (m_BackUpTexture != NULL))
	{
		UINT subresource = D3D11CalcSubresource(0, 0, 0);
		m_Id3d11DeviceContext->Unmap(m_BackUpTexture, subresource);
		m_BackUpTexture->Release();
		m_BackUpTexture = NULL;
	}
}

// dummy impl

int	cImageCapturingModuleId3d11Impl::getThisRect(CRect &rect)
{
	return 0;
}

int	cImageCapturingModuleId3d11Impl::getNonLayeredScreenRect(CRect &rect)
{
	return 0;
}


//TODO: This can be also done by getting locked rect from GPU and passing it directly to CxImage (As in formImageDataToSend)
//Done like this just to maintain compatibility with bitmaps if needed.
HBITMAP cImageCapturingModuleId3d11Impl::getCurrentFrameAsBitmap()
{
	HBITMAP  hbitmapCurrentFrame = NULL;

	getBitmapFromTextureId3d11(m_CurrentFrameTexture, &hbitmapCurrentFrame);

	return hbitmapCurrentFrame;
}


/************************************************************************************************************************************************************************************************************
*Function:  InitimageCapturingModule
*
*DESCRIPTION - Resets and/or prepares the device for image capture
*
*Future Enhancements:
*
* Returns	   Success or Failure
*
*Date: DEC 2016
**************************************************************************************************************************************************************************************************************/

bool cImageCapturingModuleId3d11Impl::InitimageCapturingModule(CRect &monitorRect, int monitorNum)
{
	bool result = false;

	try
	{
		if (_pPreviousFrameImage)
		{
			_pPreviousFrameImage->Release();
			_pPreviousFrameImage = NULL;
		}

		if (_pCurrentFrameImage)
		{
			_pCurrentFrameImage->Release();
			_pCurrentFrameImage = NULL;
		}

		m_CurrentFrameTexture = NULL;

		m_DuplicationManager.Reset();

		UINT monitor = (monitorNum - 1);

		//auto it = std::find_if( vAdapters_map.begin(), vAdapters_map.end(), [monitor](const std::pair< std::pair<IDXGIAdapter*, IDXGIOutput*> , UINT>& element){ return element.second == monitor;} ); //lambda iterator

		if (vAdapters_map.size() == 0 || vAdapters_map.size() < monitorNum)
		{
			GetAllAvailableMonitors();
		}

		std::pair<std::pair<std::pair<IDXGIAdapter*, IDXGIOutput*>, MONITOR*>, UINT> monitor_map = vAdapters_map.at(monitor);
		monitorRect = monitor_map.first.second->scrRect;

		IDXGIAdapter* _pAdapter = monitor_map.first.first.first;
		IDXGIOutput *_pOutput = monitor_map.first.first.second;

		m_LastErrorCode = initDevice(_pAdapter);

		if (SUCCEEDED(m_LastErrorCode))
		{
			m_LastErrorCode = initDuplicationManager(_pAdapter, _pOutput, monitor_map.second);

			if (SUCCEEDED(m_LastErrorCode))
			{
				result = true;
			}
			else
			{
				ReleaseAdapters();
			}
		}
		else
		{
			ReleaseAdapters();
		}
	}
	catch (std::out_of_range &e)
	{
		
	}
	catch (...)
	{

	}

	return result;
}

bool cImageCapturingModuleId3d11Impl::InitimageCapturingModule(CRect &monitorRect, CComPtr<IMFDXGIDeviceManager> &pDeviceManager, int monitorNum)
{
	bool result = false;

	try
	{
		if (_pPreviousFrameImage)
		{
			_pPreviousFrameImage->Release();
			_pPreviousFrameImage = NULL;
		}

		if (_pCurrentFrameImage)
		{
			_pCurrentFrameImage->Release();
			_pCurrentFrameImage = NULL;
		}

		m_CurrentFrameTexture = NULL;

		m_DuplicationManager.Reset();

		UINT monitor = (monitorNum - 1);

		//auto it = std::find_if( vAdapters_map.begin(), vAdapters_map.end(), [monitor](const std::pair< std::pair<IDXGIAdapter*, IDXGIOutput*> , UINT>& element){ return element.second == monitor;} ); //lambda iterator

		if (vAdapters_map.size() == 0 || vAdapters_map.size() < monitorNum)
		{
			GetAllAvailableMonitors();
		}

		std::pair<std::pair<std::pair<IDXGIAdapter*, IDXGIOutput*>, MONITOR*>, UINT> monitor_map = vAdapters_map.at(monitor);
		monitorRect = monitor_map.first.second->scrRect;

		IDXGIAdapter* _pAdapter = monitor_map.first.first.first;
		IDXGIOutput *_pOutput = monitor_map.first.first.second;

		m_LastErrorCode = initDevice(_pAdapter);

		if (SUCCEEDED(m_LastErrorCode))
		{
			m_LastErrorCode = initDuplicationManager(_pAdapter, _pOutput, monitor_map.second);

			if (SUCCEEDED(m_LastErrorCode))
			{
				result = true;
			}
			else
			{
				ReleaseAdapters();
			}
		}
		else
		{
			ReleaseAdapters();
		}

		pDeviceManager = deviceManager;
	}
	catch (std::out_of_range &e)
	{

	}
	catch (...)
	{

	}

	return result;
}

bool cImageCapturingModuleId3d11Impl::handleDesktopChange()
{
	//TODO: Reset duplication manager and handle desktop change
	return true;
}

void cImageCapturingModuleId3d11Impl::cleanUpCurrentFrameObjects()
{
	m_DuplicationManager.DoneWithFrame();
}

BitmapData::BitmapData(BitmapData *bmp) :IReferenceCounter()
{
	if (NULL != bmp && bmp->lpbitmap)
	{
		this->lpbitmap = new byte[bmp->bmpSize];
		std::copy(bmp->lpbitmap, bmp->lpbitmap + bmp->bmpSize, this->lpbitmap);
		this->width = bmp->width;
		this->height = bmp->height;
		this->bmpSize = bmp->bmpSize;
		this->bitsPerPixel = bmp->bitsPerPixel;
		this->bytesPerPixel = bmp->bytesPerPixel;
		this->bytesPerRow = bmp->bytesPerRow;
		this->RowPitch = bmp->RowPitch;
	}
	else
	{
		this->lpbitmap = NULL;
		this->width = 0;
		this->height = 0;
		this->bmpSize = 0;
		this->bitsPerPixel = 0;
		this->bytesPerPixel = 0;
		this->bytesPerRow = 0;
		this->RowPitch = 0;
	}
}

BitmapData::BitmapData(const BitmapData &bmp) :IReferenceCounter()
{
	this->lpbitmap = new byte[bmp.bmpSize];
	std::copy(bmp.lpbitmap, bmp.lpbitmap + bmp.bmpSize, this->lpbitmap);
	this->width = bmp.width;
	this->height = bmp.height;
	this->bmpSize = bmp.bmpSize;
	this->bitsPerPixel = bmp.bitsPerPixel;
	this->bytesPerPixel = bmp.bytesPerPixel;
	this->bytesPerRow = bmp.bytesPerRow;
	this->RowPitch = bmp.RowPitch;
}

BitmapData::~BitmapData()
{
	if (lpbitmap)
	{
		delete[]lpbitmap;
	}
}