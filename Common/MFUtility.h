#pragma once

/// Filename: MFUtility.h
///
/// Description:
/// This header file contains common macros and functions that are used in the Media Foundation
/// sample applications.
///
/// History:
/// 07 Mar 2015	Aaron Clauson (aaron@sipsorcery.com)	Created.
///
/// License: Public

#include <stdio.h>
#include <tchar.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#include <wmsdkidl.h>
#include <string>

#define CHECK_HR(hr, err) if (FAILED(hr)) { printf(err); printf("Error: %.2X.\n", hr); goto done; }
#define CHECK(x, err) if (!(x)) { printf(err); printf("Error: %.2X.\n", hr); goto done; }

#define CHECKHR_GOTO(x, y) if(FAILED(x)) goto y

#define INTERNAL_GUID_TO_STRING( _Attribute, _skip ) \
if (Attr == _Attribute) \
{ \
	pAttrStr = #_Attribute; \
	C_ASSERT((sizeof(#_Attribute) / sizeof(#_Attribute[0])) > _skip); \
	pAttrStr += _skip; \
	goto done; \
} \

LPCSTR STRING_FROM_GUID(GUID Attr);
/*
Copies a media type attribute from an input media type to an output media type. Useful when setting
up the video sink and where a number of the video sink input attributes need to be duplicated on the
video writer attributes.
*/
HRESULT CopyAttribute(IMFAttributes *pSrc, IMFAttributes *pDest, const GUID& key);

void CreateBitmapFile(LPCWSTR fileName, long width, long height, WORD bitsPerPixel, BYTE * bitmapData, DWORD bitmapDataLength);