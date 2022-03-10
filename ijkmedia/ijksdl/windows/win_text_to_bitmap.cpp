
#include "win_text_to_bitmap.h"
#include <windows.h>
#include <gdiplus.h>

const wchar_t* UTF82WCS(const char* str_utf8)
{
	//预转换，得到所需空间的大小;
	int wcsLen = ::MultiByteToWideChar(CP_UTF8, NULL, str_utf8, strlen(str_utf8), NULL, 0);

	//分配空间要给'\0'留个空间，MultiByteToWideChar不会给'\0'空间
	wchar_t* wszString = new wchar_t[wcsLen + 1];

	//转换
	::MultiByteToWideChar(CP_UTF8, NULL, str_utf8, strlen(str_utf8), wszString, wcsLen);

	//最后加上'\0'
	wszString[wcsLen] = '\0';

	return wszString;
}

Subtitle_Overlay* Create_Bitmap(const char* text)
{
	static bool init = false;
	if (false == init) {
		init = true;
		Gdiplus::GdiplusStartupInput    gdiplusStartupInput;
		ULONG_PTR                       gdiplusToken;
		if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Gdiplus::Ok)
		{
			MessageBox(NULL, TEXT("GDI+ failed to start up!"),
				TEXT("Error!"), MB_ICONERROR);
			return NULL;
		}
	}

	Gdiplus::Bitmap* bitmap = new Gdiplus::Bitmap(500, 50, PixelFormat32bppARGB);
	Gdiplus::Graphics* g = Gdiplus::Graphics::FromImage(bitmap);

	Gdiplus::FontFamily   fontFamily(L"仿宋");
	Gdiplus::Font font(&fontFamily, 12);

	Gdiplus::SolidBrush brush(Gdiplus::Color::Black);
	const wchar_t* wcs = UTF82WCS(text);
	
	g->DrawString(wcs, wcslen(wcs), &font, Gdiplus::PointF(0.0f, 0.0f), &brush);

	Gdiplus::BitmapData*  bmpData = new Gdiplus::BitmapData();
	Gdiplus::Rect rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight());
	bitmap->LockBits(&rect, Gdiplus::ImageLockMode::ImageLockModeRead, PixelFormat32bppARGB, bmpData);
	
	{
		CLSID pngClsid;
		bool foundPngClsid = false;

		// Get png clsid
		UINT num = 0;
		UINT size = 0;
		Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;
		Gdiplus::GetImageEncodersSize(&num, &size);
		pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
		Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
		for (UINT j = 0; j < num; ++j)
		{
			if (wcscmp(pImageCodecInfo[j].MimeType, L"image/png") == 0)
			{
				pngClsid = pImageCodecInfo[j].Clsid;
				foundPngClsid = true;
				break;
			}
		}

		free(pImageCodecInfo);

		if (foundPngClsid)
			bitmap->Save(L"D:\\1.png", &pngClsid);
	}

	Subtitle_Overlay* overlay = new Subtitle_Overlay();
	overlay->w = bmpData->Width;
	overlay->h = bmpData->Height;
	overlay->stride = bmpData->Stride;
	overlay->pixels = bmpData->Scan0;
	overlay->bitmap = bitmap;
	overlay->bitmap_data = bmpData;
	return overlay;
}

void Release_Bitmap(Subtitle_Overlay* overlay)
{
	Gdiplus::Bitmap* bitmap = (Gdiplus::Bitmap*)overlay->bitmap;
	Gdiplus::BitmapData* bmpData = (Gdiplus::BitmapData*)overlay->bitmap_data;

	bitmap->UnlockBits(bmpData);
	delete bmpData;
	delete bitmap;
	delete overlay;
}