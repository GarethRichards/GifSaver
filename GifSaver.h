#pragma once

#include "Common\DirectXHelper.h"
#include "DirectXTex.h"

namespace SIRDS {


	class GifSaver
	{
	public:
		GifSaver(IWICImagingFactory2* imagingFactory, const std::wstring &FileName,
			int width, int height, int delay, int nFrames);
		~GifSaver();
		void AddBitmap(std::shared_ptr<DirectX::ScratchImage> image);
		void Commit();
	private:
		void Initialize(IWICImagingFactory2* imagingFactory);

		int m_delay;
		int m_width;
		int m_height;
		int m_currentFrame;
		int m_nFrames;
		IWICImagingFactory2 *m_imagingFactory;
		Microsoft::WRL::ComPtr<IWICBitmapEncoder> m_wicBitmapEncoder;
		Microsoft::WRL::ComPtr<IWICStream> m_stream;
		std::wstring m_FileName;
	};


}
