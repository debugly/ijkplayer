#ifndef WIN_TEXT_TO_BITMAP_H
#define WIN_TEXT_TO_BITMAP_H

typedef struct IJK_EGL_Opaque  IJK_EGL_Opaque;

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Subtitle_Overlay {
	unsigned int	w;
	unsigned int	h;
	int				stride;
	void*			pixels;
	void*			bitmap;
	void*			bitmap_data;

	float			font_size;
	char*			font_name;
} Subtitle_Overlay;

Subtitle_Overlay* Create_Subitle_Overlay();

Subtitle_Overlay* Create_Bitmap(const char* text, Subtitle_Overlay** ptr);

void Release_Bitmap(Subtitle_Overlay* overlay);

#ifdef __cplusplus
}
#endif

#endif

