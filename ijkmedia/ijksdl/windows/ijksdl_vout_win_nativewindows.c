#include <windows.h>
#include "ijksdl/ijksdl_egl.h"
#include "ijksdl/ijksdl_vout.h"
#include "ijksdl/ijksdl_vout_internal.h"
#include "ijksdl/ffmpeg/ijksdl_vout_overlay_ffmpeg.h"
#include "win_text_to_bitmap.h"

typedef struct SDL_Vout_Opaque {
    HWND   native_window;
   
    int              null_native_window_warned; // reduce log for null window
    int              next_buffer_id;

	IJK_EGL         *egl;

	int(*decode_video_callback)(void *arg, SDL_VoutOverlay* overlay);
	char*			subtitle_text;
} SDL_Vout_Opaque;

static SDL_Class g_nativewindow_class = {
    .name = "ANativeWindow_Vout",
};

static SDL_VoutOverlay *func_create_windows_overlay_l(int width, int height, int frame_format, SDL_Vout *vout)
{
	SDL_VoutOverlay* t = NULL;
    t = SDL_VoutFFmpeg_CreateOverlay(width, height, frame_format, vout);
	return t;

	//SDL_VoutOverlay *overlay = SDL_VoutOverlay_CreateInternal(sizeof(SDL_VoutOverlay_Opaque));
	//if (!overlay) {
	//	ALOGE("overlay allocation failed");
	//	return NULL;
	//}

	//SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
	//opaque->mutex = SDL_CreateMutex();
	//opaque->sws_flags = SWS_BILINEAR;

	//overlay->format = frame_format;
	//overlay->pitches = opaque->pitches;
	//overlay->pixels = opaque->pixels;
	//overlay->w = width;
	//overlay->h = height;

	//return overlay;
}

static SDL_VoutOverlay *func_create_windows_overlay(int width, int height, int frame_format, SDL_Vout *vout)
{
    SDL_LockMutex(vout->mutex);
    SDL_VoutOverlay *overlay = func_create_windows_overlay_l(width, height, frame_format, vout);
    SDL_UnlockMutex(vout->mutex);
    return overlay;
}

static void func_free_windows_l(SDL_Vout *vout)
{
    if (!vout)
        return;

    SDL_Vout_Opaque *opaque = vout->opaque;
	
    if (opaque) {
		if (opaque->native_window) {
			opaque->native_window = NULL;
        }
		IJK_EGL_freep(&opaque->egl);
	}
	SDL_Vout_FreeInternal(vout);
}

static int func_display_windows_overlay_l(SDL_Vout *vout, SDL_VoutOverlay *overlay)
{
    SDL_Vout_Opaque *opaque = vout->opaque;
    HWND native_window = opaque->native_window;

    if (!native_window) {
        if (!opaque->null_native_window_warned) {
            opaque->null_native_window_warned = 1;
            ALOGW("func_display_overlay_l: NULL native_window");
        }
        return -1;
    } else {
        opaque->null_native_window_warned = 1;
    }

    if (!overlay) {
        ALOGE("func_display_overlay_l: NULL overlay");
        return -1;
    }

    if (overlay->w <= 0 || overlay->h <= 0) {
        ALOGE("func_display_overlay_l: invalid overlay dimensions(%d, %d)", overlay->w, overlay->h);
        return -1;
    }

    switch(overlay->format) {
    case SDL_FCC__AMC: {
        // only ANativeWindow support
        // IJK_EGL_terminate(opaque->egl);
        // return SDL_VoutOverlayAMediaCodec_releaseFrame_l(overlay, NULL, true);
              printf("SDL_FCC_I444P10LE\n");
    }
    case SDL_FCC_RV24:
    case SDL_FCC_I420:
	case SDL_FCC_NV12:
    case SDL_FCC_I444P10LE: {
        // only GLES support
             // printf("SDL_FCC_I444P10LE\n");
        if (opaque->egl){
			  // printf("SDL_FCC_I444P10LE000\n");
            return IJK_EGL_display(opaque->egl, native_window, overlay, opaque->subtitle_text);
        	}
        break;
    }
    case SDL_FCC_YV12:
    case SDL_FCC_RV16:
    case SDL_FCC_RV32: {
        // both GLES & ANativeWindow support
   
		if(vout->overlay_format == SDL_FCC__GLES2){
			    printf("SDL_FCC_RV32\n");
			}else{
				    printf("SDL_FCC_RV32:%d\n",vout->overlay_format);
				}
        if (vout->overlay_format == SDL_FCC__GLES2 && opaque->egl)
            return IJK_EGL_display(opaque->egl, native_window, overlay, opaque->subtitle_text);
        break;
    }
    }

    // fallback to ANativeWindow
    IJK_EGL_terminate(opaque->egl);
    //return SDL_Android_NativeWindow_display_l(native_window, overlay); 
    return 0;
}

static int func_display_windows_overlay(SDL_Vout *vout, SDL_VoutOverlay *overlay)
{
    SDL_LockMutex(vout->mutex);
    int retval = func_display_windows_overlay_l(vout, overlay);
    SDL_UnlockMutex(vout->mutex);
    return retval;
}

static void func_update_subtitle(SDL_Vout *vout, const char *text)
{
	SDL_Vout_Opaque* opaque = vout->opaque;

	if (!opaque) {
		return;
	}

	if (opaque->subtitle_text) {
		av_freep((void*)&opaque->subtitle_text);
	}

	if (!text || strlen(text) == 0) {
		return;
	}

	opaque->subtitle_text = av_strdup(text);
}

static void SDL_VoutWindows_SetNativeWindow_l(SDL_Vout *vout, HWND native_window)
{
   // AMCTRACE("%s(%p, %p)\n", __func__, vout, native_window);
    SDL_Vout_Opaque *opaque = vout->opaque;

    if (opaque->native_window == native_window) {
        // if (native_window == NULL) {
        //     // always invalidate buffers, if native_window is changed
        //     SDL_VoutAndroid_invalidateAllBuffers_l(vout);
        // }
        return;
    }

    IJK_EGL_terminate(opaque->egl);

	opaque->native_window = native_window;
    opaque->null_native_window_warned = 0;
}

void SDL_VoutWindows_SetNativeWindow(SDL_Vout *vout, HWND hwnd)
{
    SDL_LockMutex(vout->mutex);
    SDL_VoutWindows_SetNativeWindow_l(vout, hwnd);
    SDL_UnlockMutex(vout->mutex);
}

static void func_free_l(SDL_Vout *vout)
{
    if (!vout)
        return;

    SDL_Vout_Opaque *opaque = vout->opaque;
    if (opaque) {

		if (opaque->native_window) {
           // ANativeWindow_release(opaque->native_window);
            opaque->native_window = NULL;
        }

        IJK_EGL_freep(&opaque->egl);

        
    }

    SDL_Vout_FreeInternal(vout);
}

SDL_Vout *SDL_VoutWindows_CreateForANativeWindow()
{
    SDL_Vout *vout = SDL_Vout_CreateInternal(sizeof(SDL_Vout_Opaque));
    if (!vout)
        return NULL;

    SDL_Vout_Opaque *opaque = vout->opaque;
    opaque->native_window = NULL;

    opaque->egl = IJK_EGL_create();
    if (!opaque->egl)
        goto fail;

    vout->opaque_class    = &g_nativewindow_class;
    vout->create_overlay  = func_create_windows_overlay;
    vout->free_l          = func_free_windows_l;
    vout->display_overlay = func_display_windows_overlay;
	vout->update_subtitle = func_update_subtitle;
    return vout;
fail:
    func_free_l(vout);
    return NULL;
}

void SDL_VoutWinNative_SetVideoDataCallback(void *arg, SDL_Vout *vout, int(*video_callback)(void *arg, SDL_VoutOverlay* overlay))
{
	SDL_Vout_Opaque *opaque = vout->opaque;
	SDL_LockMutex(vout->mutex);
	opaque->decode_video_callback = video_callback;
	SDL_UnlockMutex(vout->mutex);
}
