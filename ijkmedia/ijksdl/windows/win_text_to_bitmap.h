#ifndef WIN_TEXT_TO_BITMAP_H
#define WIN_TEXT_TO_BITMAP_H


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
} Subtitle_Overlay;

Subtitle_Overlay* Create_Bitmap(const char* text);

void Release_Bitmap(Subtitle_Overlay* overlay);

#ifdef __cplusplus
}
#endif

#endif

