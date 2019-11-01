#pragma once
#include "CommonTypes.h"
#include "d3d11.h"
#include "DuplicationManager.h"
#include <vector>
#include "ReferenceCounter.h"
#include "SmartPtr.h"
#include <mfobjects.h>

using namespace std;

# define D3_D11_DLL _T("d3d11.dll")

typedef HRESULT(WINAPI *D3D11CreateDeviceFunType)(
	_In_opt_ IDXGIAdapter* pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	_In_reads_opt_(FeatureLevels) CONST D3D_FEATURE_LEVEL* pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	_Out_opt_ ID3D11Device** ppDevice,
	_Out_opt_ D3D_FEATURE_LEVEL* pFeatureLevel,
	_Out_opt_ ID3D11DeviceContext** ppImmediateContext);

class IImageCapturingModule;

class cImageCapturingModuleId3d11Impl : public IReferenceCounter
{

private:
	virtual ~cImageCapturingModuleId3d11Impl();

	std::vector<std::pair<std::pair<std::pair<IDXGIAdapter1*, IDXGIOutput*>, MONITOR*>, UINT>> vAdapters_map;

protected:

	ID3D11DeviceContext*						m_Id3d11DeviceContext;
	ID3D11Device*								m_Id3d11Device;
	CComPtr<IMFDXGIDeviceManager>				deviceManager;
	CComPtr<ID3D11Texture2D> 					m_CurrentFrameTexture;
	ID3D11Texture2D*							m_BackUpTexture;
	HRESULT										m_LastErrorCode;
	cDuplicationManager							m_DuplicationManager;
	D3D_FEATURE_LEVEL							m_SelectedFeatureLevel;
	D3D11CreateDeviceFunType					m_FnD3D11CreateDevice;
	HMODULE										m_DllHandleD3D11;

protected:

	HRESULT					initDuplicationManager(IDXGIAdapter *, IDXGIOutput *, int outputNumber);

	DWORD					loadD3D11FunctionsFromDll();

public:

	SmartPtr<BitmapData>	_pPreviousFrameImage;
	SmartPtr<BitmapData>	_pCurrentFrameImage;

public:

	cImageCapturingModuleId3d11Impl();

	std::vector <IDXGIOutput*>	EnumMonitors(IDXGIAdapter1* pSelectedAdapter);

	std::vector <IDXGIAdapter1*> EnumerateAdapters(void);

	BOOL GetAllAvailableMonitors();

	bool					getBitmapFromTextureId3d11(ID3D11Texture2D* texture, HBITMAP* outputBitmap);

	HRESULT					initDevice(_In_opt_		IDXGIAdapter	*pAdapter = nullptr);

	HBITMAP					getCurrentFrameAsBitmap();

	void					releaseBuffer();

	virtual int				getThisRect(CRect &rect);

	virtual int				getNonLayeredScreenRect(CRect &rect);

	INT						getChangedRegions(int timeout, bool &isTimeOut, rectangles &dirtyRects, std::vector <DXGI_OUTDUPL_MOVE_RECT> &moveRects, UINT &rect_count, RECT ScreenRect);

	bool					InitimageCapturingModule(CRect &monitorRect, int monitorNum = 1);

	bool					InitimageCapturingModule(CRect &monitorRect, CComPtr<IMFDXGIDeviceManager> &pDeviceManager, int monitorNum = 1);

	bool					handleDesktopChange();

	void					cleanUpCurrentFrameObjects();

	virtual void			DoCleanup();

	virtual void			ReleaseAdapters();

	virtual SmartPtr<BitmapData>*	GetCurrentFrameImage(CRect &rect);
	virtual SmartPtr<BitmapData>*	GetPreviousImage(bool bCopyCurrentIfNotAvailable = false);
	virtual SmartPtr<BitmapData>*	GetCurrentImage(CRect &rect, BOOL _bCaptureNew = false);

	bool GetCurrentFrameAsVideoSample(void **, void **, bool &isTimeout, CRect &deviceRect, int width, int height);
	bool GetCurrentFrameAsVideoSampleExp(void **, void **, bool &isTimeout, CRect &deviceRect, int width, int height);

	virtual inline void ResetPreviousImage()
	{
		_pPreviousFrameImage = NULL;
	}

	BOOL virtual			populateMonitorDetails();

	BOOL					GetChangedRegion(CRect& optimizedRegion,
							rectangles& dirtyRects,
							std::vector <DXGI_OUTDUPL_MOVE_RECT>& moveRects,
							_In_ RECT* adjusted_rect,
							rectangles& rectsToSend,
							UINT& total_area,
							UINT& numberofrects,
							CString& latestWindowStr,
							CString& prevWindowStr,
							CStringA& cacheStr);


	BOOL					IsDeviceReady();

	HRESULT virtual			GetLastError()
	{
		return m_LastErrorCode;
	}
};