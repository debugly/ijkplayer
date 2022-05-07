
#include "win_text_to_bitmap.h"

#include <windows.h>
#include <gdiplus.h>

wchar_t* UTF82WCS(int codepage, const char* str_utf8)
{
	//预转换，得到所需空间的大小;
	int wcsLen = ::MultiByteToWideChar(codepage, NULL, str_utf8, strlen(str_utf8), NULL, 0);

	//分配空间要给'\0'留个空间，MultiByteToWideChar不会给'\0'空间
	wchar_t* wszString = new wchar_t[wcsLen + 1];

	//转换
	::MultiByteToWideChar(codepage, NULL, str_utf8, strlen(str_utf8), wszString, wcsLen);

	//最后加上'\0'
	wszString[wcsLen] = '\0';

	return wszString;
}

Subtitle_Overlay* Create_Subitle_Overlay()
{
	Subtitle_Overlay* overlay = (Subtitle_Overlay*)calloc(1, sizeof(Subtitle_Overlay));

	const char* default_font = "宋体";
	int len = strlen(default_font);

	overlay->font_size = 18;
	overlay->font_name = (char*)malloc(len + 1);
	ZeroMemory(overlay->font_name, len + 1);
	strcpy(overlay->font_name, default_font);

	return overlay;
}

Subtitle_Overlay* Create_Bitmap(const char* text, Subtitle_Overlay** ptr_overlay)
{
	static bool init = false;
	static int dpiX, dpiY;
	if (false == init) {
		init = true;
		Gdiplus::GdiplusStartupInput    gdiplusStartupInput;
		ULONG_PTR                       gdiplusToken;
		if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Gdiplus::Ok)
		{
			return NULL;
		}

		HDC screen = GetDC(0);
		dpiX = GetDeviceCaps(screen, LOGPIXELSX);
		dpiY = GetDeviceCaps(screen, LOGPIXELSX);
	}

	Subtitle_Overlay*  overlay = *ptr_overlay;

	const wchar_t* name = UTF82WCS(CP_ACP, overlay->font_name);
	Gdiplus::FontFamily   fontFamily(name);

	Gdiplus::Font font(&fontFamily, overlay->font_size);

	Gdiplus::REAL fontSize = font.GetSize();

	Gdiplus::SolidBrush brush(Gdiplus::Color::White);
	const wchar_t* wcs = UTF82WCS(CP_UTF8, text);

	int	len = wcslen(wcs);
	//assume max num of rows  is 8;
	int array_lines_len[8] = { 0 };
	int line_cnt = 0;
	const wchar_t* begin = wcs;
	for (const wchar_t* cur = wcs; cur != NULL && line_cnt < 8; line_cnt++) {
		cur = wcschr(begin, L'\n');
		if (cur != NULL) {
			array_lines_len[line_cnt] = cur - begin;
			cur += 1;
			begin = cur;
		} else {
			array_lines_len[line_cnt] = wcs + len - begin;
		}
	}
	
	int max_line_len = 0;
	for (int i = 0; i < line_cnt; i++) {
		if (max_line_len < array_lines_len[i]) {
			max_line_len = array_lines_len[i];
		}
	}

	int	bmpWidth = max_line_len * 1.4 * fontSize;
	int bmpHeight = font.GetHeight(dpiY) * (line_cnt +1);

	Gdiplus::Bitmap* bitmap = new Gdiplus::Bitmap(bmpWidth, bmpHeight, PixelFormat32bppARGB);
	Gdiplus::Graphics* g = Gdiplus::Graphics::FromImage(bitmap);

	g->DrawString(wcs, wcslen(wcs), &font, Gdiplus::PointF(0.0f, 0.0f), &brush);

	Gdiplus::BitmapData*  bmpData = new Gdiplus::BitmapData();
	Gdiplus::Rect rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight());
	bitmap->LockBits(&rect, Gdiplus::ImageLockMode::ImageLockModeRead, PixelFormat32bppARGB, bmpData);

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
}