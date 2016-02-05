#include "pch.h"

#include "GifSaver.h"
#include <string>
#include <FileAPI.h>
 
using namespace Microsoft::WRL;
using namespace SIRDS;
using namespace std;

GifSaver::GifSaver(IWICImagingFactory2* imagingFactory, const std::wstring &FileName,
	int width, int height, int delay, int nFrames)
	: m_delay(delay), m_imagingFactory(imagingFactory), m_width(width), m_height(height),
	  m_nFrames(nFrames),m_FileName(FileName)
{
	auto hr = imagingFactory->CreateStream(&m_stream);
	hr = m_stream->InitializeFromFilename(FileName.c_str(), GENERIC_WRITE);

	Initialize( imagingFactory );
}

void GifSaver::Initialize(IWICImagingFactory2* imagingFactory)
{
	PROPVARIANT propValue; PropVariantInit(&propValue);
	propValue.vt = VT_UI1 | VT_VECTOR;
	propValue.caub.cElems = 11;

	DX::ThrowIfFailed(m_imagingFactory->CreateEncoder(GUID_ContainerFormatGif, &GUID_VendorMicrosoft, &m_wicBitmapEncoder));
	DX::ThrowIfFailed(m_wicBitmapEncoder->Initialize(m_stream.Get(), WICBitmapEncoderNoCache));
	ComPtr<IWICMetadataQueryWriter> pEncoderMetadataQueryWriter;
	DX::ThrowIfFailed(m_wicBitmapEncoder->GetMetadataQueryWriter(&pEncoderMetadataQueryWriter));
	string elms = "NETSCAPE2.0";
	propValue.caub.pElems = const_cast<UCHAR *>(reinterpret_cast<const UCHAR *>(elms.c_str()));
	DX::ThrowIfFailed(pEncoderMetadataQueryWriter->SetMetadataByName(L"/appext/Application", &propValue));

	// Set animated GIF format 
	propValue.vt = VT_UI1 | VT_VECTOR;
	propValue.caub.cElems = 5;
	UCHAR buf[5];
	propValue.caub.pElems = &buf[0];
	*(propValue.caub.pElems) = 3; // must be > 1, 
	*(propValue.caub.pElems + 1) = 1; // defines animated GIF 
	*(propValue.caub.pElems + 2) = 0; // LSB 0 = infinite loop. 
	*(propValue.caub.pElems + 3) = 0; // MSB of iteration count value 
	*(propValue.caub.pElems + 4) = 0; // NULL == end of data 

	DX::ThrowIfFailed(pEncoderMetadataQueryWriter->SetMetadataByName(L"/appext/Data", &propValue));
	propValue.vt = VT_LPSTR;
	propValue.pszVal = "Produced by DIY 3D DOTTPIX written by Gareth Richards"; // use new-delete[] calls
	DX::ThrowIfFailed(pEncoderMetadataQueryWriter->SetMetadataByName(L"/commentext/TextEntry", &propValue));

	// Global Width and Height are written successfully.
	propValue.vt = VT_UI2;
	propValue.uiVal = (USHORT)m_width;
	DX::ThrowIfFailed(pEncoderMetadataQueryWriter->SetMetadataByName
		(L"/logscrdesc/Width", &propValue));
	propValue.vt = VT_UI2; propValue.uiVal = (USHORT)m_height;
	DX::ThrowIfFailed(pEncoderMetadataQueryWriter->SetMetadataByName
		(L"/logscrdesc/Height", &propValue));
}

GifSaver::~GifSaver()
{
}

void GifSaver::AddBitmap(shared_ptr<DirectX::ScratchImage> image)
{
	PROPVARIANT propValue; PropVariantInit(&propValue);
	IWICPalette *pPalette = nullptr;

	ComPtr<IWICFormatConverter> pConverter;
	DX::ThrowIfFailed(m_imagingFactory->CreateFormatConverter(&pConverter));

	// The input bitmaps in the PiXCL list are (by definition) RGB24, and GIF
	ComPtr<IWICBitmap> wicBitmap;
	UINT uWidth = m_width;
	UINT uHeight = m_height;
	auto srcImage = image->GetImage(0, 0, 0);
	DX::ThrowIfFailed(m_imagingFactory->CreateBitmapFromMemory(static_cast<UINT>(srcImage->width), static_cast<UINT>(srcImage->height), GUID_WICPixelFormat32bppBGRA,
			static_cast<UINT>(srcImage->rowPitch), static_cast<UINT>(srcImage->slicePitch),
			srcImage->pixels, wicBitmap.GetAddressOf()));
		// frames are 8bppIndexed, so we have to convert each one.
	{
		ComPtr<IWICBitmapScaler> pScaler;
		DX::ThrowIfFailed(m_imagingFactory->CreateBitmapScaler(&pScaler));// Released later
		DX::ThrowIfFailed(pScaler->Initialize((IWICBitmapSource *)wicBitmap.Get(), uWidth, uHeight,
			WICBitmapInterpolationModeFant));
		wicBitmap->CopyPalette(pPalette); // Return NULL for RGB24
		if (nullptr == pPalette) {
			DX::ThrowIfFailed(m_imagingFactory->CreatePalette(&pPalette));
			// Released later. 
			DX::ThrowIfFailed(pPalette->InitializeFromBitmap((IWICBitmapSource *)wicBitmap.Get(), 256, TRUE));
		}
		DX::ThrowIfFailed(pConverter->Initialize(pScaler.Get(),// Input bitmap to convert 
			GUID_WICPixelFormat8bppIndexed, // Destination pixel format 
			WICBitmapDitherTypeNone, // see wincodec.h 
			pPalette, // Windows decides the palette 
			0.f, // Alpha threshold 
			WICBitmapPaletteTypeFixedWebPalette // probably more useful 
			));
		// Store the converted bitmap as ppToRenderBitmapSource
	}
	ComPtr<IWICBitmapSource> pWICBitmapSource;
	DX::ThrowIfFailed(pConverter.CopyTo(IID_PPV_ARGS(&pWICBitmapSource)));

	// Create a new default pBitmapFrameEncode object. 
	ComPtr<IWICBitmapFrameEncode> pBitmapFrameEncode;
	DX::ThrowIfFailed(m_wicBitmapEncoder->CreateNewFrame(&pBitmapFrameEncode, nullptr));
	DX::ThrowIfFailed(pBitmapFrameEncode->Initialize(nullptr)); // no options yet 
	DX::ThrowIfFailed(pBitmapFrameEncode->WriteSource(pWICBitmapSource.Get(), nullptr));

	ComPtr<IWICMetadataQueryWriter> pFrameMetadataQueryWriter;
	DX::ThrowIfFailed(pBitmapFrameEncode->GetMetadataQueryWriter(&pFrameMetadataQueryWriter));
	DX::ThrowIfFailed(pBitmapFrameEncode->Commit()); // has to be HERE !

	propValue.vt = VT_UI2;
	propValue.uiVal = (WORD)m_delay;
	DX::ThrowIfFailed(pFrameMetadataQueryWriter->SetMetadataByName(L"/grctlext/Delay", &propValue));
		//PropVariantClear(&propValue);
		// Other "/grctlext/*" values written here. Writing to the root
		// metadata region is not required.
		// Set the Frame Width and Height. WIC writes both of these.  
	propValue.vt = VT_UI2;
	propValue.uiVal = (USHORT)uWidth;
	DX::ThrowIfFailed(pFrameMetadataQueryWriter->SetMetadataByName(L"/imgdesc/Width", &propValue));
		//PropVariantClear(&propValue); 
	propValue.vt = VT_UI2;
	propValue.uiVal = (USHORT)uHeight;
	DX::ThrowIfFailed(pFrameMetadataQueryWriter->SetMetadataByName(L"/imgdesc/Height", &propValue));
		//PropVariantClear(&propValue);
		// Other "/imgdesc" values written here

		//		 // Write to the imgdesc root "directory" with everything. This works // and can be read back from the created animGIF. THIS IS NOT REQUIRED, despite whit you might read in MSDN
		//		propValue.vt = VT_UNKNOWN;
		//		propValue.punkVal = pFrameMetadataQueryWriter;
		//		propValue.punkVal->AddRef();
		//		hr = pGlobalMetadataQueryWriter->SetMetadataByName(L"/imgdesc",&propValue);
		//		pFrameMetadataQueryWriter->Release();
		//		PropVariantClear(&propValue); 
		//		auto refc = wicBitmap.Get()->Release();
	m_currentFrame++;
	if (m_currentFrame == m_nFrames) {
		propValue.vt = VT_UI2;
		propValue.uiVal = (WORD)(WORD)m_delay;;
		DX::ThrowIfFailed(pFrameMetadataQueryWriter->SetMetadataByName
			(L"/grctlext/Delay", &propValue));
		PropVariantClear(&propValue);
	}
}

void GifSaver::Commit()
{
	m_stream->Commit(STGC_DEFAULT);
	auto bd = m_wicBitmapEncoder.Detach();
	int res = 1;
	while (res != 0) res = bd->Release();

	auto p = m_stream.Detach();
    res = 1;
	while (res!=0) 	res=p->Release();

	CREATEFILE2_EXTENDED_PARAMETERS exPar;
	exPar.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
	exPar.dwFileFlags = 0;
	exPar.dwSecurityQosFlags = 0;
	exPar.hTemplateFile = nullptr;
	exPar.lpSecurityAttributes = nullptr;
    exPar.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
	HANDLE hGifFile = CreateFile2(m_FileName.c_str(),
		GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE,
		OPEN_EXISTING, &exPar);

	if (INVALID_HANDLE_VALUE != hGifFile)
	{
		DWORD dwBytesRW;
		BYTE buffer[1000];
		LPBYTE ptr = buffer;
		ReadFile(hGifFile, buffer, 1000, &dwBytesRW, NULL);
		LARGE_INTEGER DistanceToMove;
		DistanceToMove.QuadPart = 0;
		SetFilePointerEx(hGifFile, DistanceToMove, NULL, FILE_BEGIN);
		// OK, now we can fix the bug. Find 21 F9, add 2 bytes 
		// 
		while ((*ptr != 0x21) || (*(ptr + 1) != 0xF9))  ptr++;
		// Should be at first grctlext block. 
		ptr += 4; // should be the target Delay value 
		*ptr = (BYTE)(m_delay);
		WriteFile(hGifFile, buffer, 100, &dwBytesRW, NULL);
		CloseHandle(hGifFile);
	}
	
}
