/*
* This file is part of FFmpeg.
*
* FFmpeg is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* FFmpeg is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with FFmpeg; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#pragma once
#define inline __inline

#include "libavcodec/dxva2.h"
#include "libavutil/avassert.h"
#include "libavutil/buffer.h"
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "libavutil/pixfmt.h"

#include "ffmpeg_dxva2.h"

#include <d3d9.h>
#include <dxva2api.h>

typedef struct d3d_format_t{
	const char   *name;
	int   codec;
	int   format;
}d3d_format_t;

/* XXX Prefered format must come first */
static const d3d_format_t d3d_formats[] = {
	{ "YV12", (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'), AV_PIX_FMT_YUV420P },
	{ "NV12", (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2'), AV_PIX_FMT_NV12 },

	{ NULL, (D3DFORMAT)0, AV_PIX_FMT_NONE }
};

/* define all the GUIDs used directly here,
to avoid problems with inconsistent dxva2api.h versions in mingw-w64 and different MSVC version */
#include <initguid.h>
DEFINE_GUID(IID_IDirectXVideoDecoderService, 0xfc51a551, 0xd5e7, 0x11d9, 0xaf, 0x55, 0x00, 0x05, 0x4e, 0x43, 0xff, 0x02);

DEFINE_GUID(DXVA2_ModeMPEG2_VLD, 0xee27417f, 0x5e28, 0x4e65, 0xbe, 0xea, 0x1d, 0x26, 0xb5, 0x08, 0xad, 0xc9);
DEFINE_GUID(DXVA2_ModeMPEG2and1_VLD, 0x86695f12, 0x340e, 0x4f04, 0x9f, 0xd3, 0x92, 0x53, 0xdd, 0x32, 0x74, 0x60);
DEFINE_GUID(DXVA2_ModeH264_E, 0x1b81be68, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA2_ModeH264_F, 0x1b81be69, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVADDI_Intel_ModeH264_E, 0x604F8E68, 0x4951, 0x4C54, 0x88, 0xFE, 0xAB, 0xD2, 0x5C, 0x15, 0xB3, 0xD6);
DEFINE_GUID(DXVA2_ModeVC1_D, 0x1b81beA3, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA2_ModeVC1_D2010, 0x1b81beA4, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA2_ModeHEVC_VLD_Main, 0x5b11d51b, 0x2f4c, 0x4452, 0xbc, 0xc3, 0x09, 0xf2, 0xa1, 0x16, 0x0c, 0xc0);
DEFINE_GUID(DXVA2_ModeHEVC_VLD_Main10, 0x107af0e0, 0xef1a, 0x4d19, 0xab, 0xa8, 0x67, 0xa1, 0x63, 0x07, 0x3d, 0x13);
DEFINE_GUID(DXVA2_ModeVP9_VLD_Profile0, 0x463707f8, 0xa1d0, 0x4585, 0x87, 0x6d, 0x83, 0xaa, 0x6d, 0x60, 0xb8, 0x9e);
DEFINE_GUID(DXVA2_NoEncrypt, 0x1b81beD0, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(GUID_NULL, 0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

typedef IDirect3D9* WINAPI pDirect3DCreate9(UINT);
typedef HRESULT WINAPI pCreateDeviceManager9(UINT *, IDirect3DDeviceManager9 **);

CRITICAL_SECTION cs;

typedef struct dxva2_mode {
    const GUID     *guid;
    enum AVCodecID codec;
} dxva2_mode;

//供选择的解码配置，高配在前
static const dxva2_mode dxva2_modes[] = {
	/* MPEG-2 */
	{ &DXVA2_ModeMPEG2_VLD, AV_CODEC_ID_MPEG2VIDEO },
	{ &DXVA2_ModeMPEG2and1_VLD, AV_CODEC_ID_MPEG2VIDEO },

	/* H.264 */
	{ &DXVA2_ModeH264_F, AV_CODEC_ID_H264 },
	{ &DXVA2_ModeH264_E, AV_CODEC_ID_H264 },
	/* Intel specific H.264 mode */
	{ &DXVADDI_Intel_ModeH264_E, AV_CODEC_ID_H264 },

	/* VC-1 / WMV3 */
	{ &DXVA2_ModeVC1_D2010, AV_CODEC_ID_VC1 },
	{ &DXVA2_ModeVC1_D2010, AV_CODEC_ID_WMV3 },
	{ &DXVA2_ModeVC1_D, AV_CODEC_ID_VC1 },
	{ &DXVA2_ModeVC1_D, AV_CODEC_ID_WMV3 },

	/* HEVC/H.265 */
	{ &DXVA2_ModeHEVC_VLD_Main, AV_CODEC_ID_HEVC },
	{ &DXVA2_ModeHEVC_VLD_Main10, AV_CODEC_ID_HEVC },

	/* VP8/9 */
	{ &DXVA2_ModeVP9_VLD_Profile0, AV_CODEC_ID_VP9 },

	{ NULL, AV_CODEC_ID_NONE },
};

typedef struct surface_info {
    int used;
    uint64_t age;
} surface_info;

typedef struct DXVA2Context {
    HMODULE d3dlib;
    HMODULE dxva2lib;

    HANDLE  deviceHandle;

    IDirect3D9                  *d3d9;
    IDirect3DDevice9            *d3d9device;
    IDirect3DDeviceManager9     *d3d9devmgr;
    IDirectXVideoDecoderService *decoder_service;
    IDirectXVideoDecoder        *decoder;

    GUID                        decoder_guid;
    DXVA2_ConfigPictureDecode   decoder_config;

    LPDIRECT3DSURFACE9          *surfaces;
    surface_info                *surface_infos;
    uint32_t                    num_surfaces;
    uint64_t                    surface_age;

    AVFrame                     *tmp_frame;
} DXVA2Context;

typedef struct DXVA2SurfaceWrapper {
    DXVA2Context         *ctx;
    LPDIRECT3DSURFACE9   surface;
    IDirectXVideoDecoder *decoder;
} DXVA2SurfaceWrapper;

static void dxva2_destroy_decoder(AVCodecContext *s)
{
    InputStream  *ist = (InputStream *)s->opaque;
    DXVA2Context *ctx = (DXVA2Context *)ist->hwaccel_ctx;
    int i;

    if (ctx->surfaces) {
        for (i = 0; i < ctx->num_surfaces; i++) {
            if (ctx->surfaces[i])
                IDirect3DSurface9_Release(ctx->surfaces[i]);
        }
    }
    av_freep(&ctx->surfaces);
    av_freep(&ctx->surface_infos);
    ctx->num_surfaces = 0;
    ctx->surface_age = 0;

    if (ctx->decoder) {
        IDirectXVideoDecoder_Release(ctx->decoder);
        //ctx->decoder->Release();
        ctx->decoder = NULL;
    }
}

static void dxva2_uninit(AVCodecContext *s)
{
    InputStream  *ist = (InputStream  *)s->opaque;
    DXVA2Context *ctx = (DXVA2Context *)ist->hwaccel_ctx;

    ist->hwaccel_uninit = NULL;
    ist->hwaccel_get_buffer = NULL;
    ist->hwaccel_retrieve_data = NULL;

    if (ctx->decoder)
        dxva2_destroy_decoder(s);

    if (ctx->decoder_service)
        IDirectXVideoDecoderService_Release(ctx->decoder_service);
        //ctx->decoder_service->Release();

    if (ctx->d3d9devmgr && ctx->deviceHandle != INVALID_HANDLE_VALUE)
        IDirect3DDeviceManager9_CloseDeviceHandle(ctx->d3d9devmgr, ctx->deviceHandle);
        //ctx->d3d9devmgr->CloseDeviceHandle(ctx->deviceHandle);

    if (ctx->d3d9devmgr)
        IDirect3DDeviceManager9_Release(ctx->d3d9devmgr);
        //ctx->d3d9devmgr->Release();

    if (ctx->d3d9device)
        IDirect3DDevice9_Release(ctx->d3d9device);

    if (ctx->d3d9)
        IDirect3D9_Release(ctx->d3d9);

    if (ctx->d3dlib)
        FreeLibrary(ctx->d3dlib);

    if (ctx->dxva2lib)
        FreeLibrary(ctx->dxva2lib);

    av_frame_free(&ctx->tmp_frame);

    av_freep(&ist->hwaccel_ctx);
    av_freep(&s->hwaccel_context);

	DeleteCriticalSection(&cs);

}

static void dxva2_release_buffer(void *opaque, uint8_t *data)
{
    DXVA2SurfaceWrapper *w = (DXVA2SurfaceWrapper *)opaque;
    DXVA2Context        *ctx = w->ctx;
    int i;

    for (i = 0; i < ctx->num_surfaces; i++) {
        if (ctx->surfaces[i] == w->surface) {
            ctx->surface_infos[i].used = 0;
            break;
        }
    }
    IDirect3DSurface9_Release(w->surface);
    IDirectXVideoDecoder_Release(w->decoder);
    //w->decoder->Release();
    av_free(w);
}

static int dxva2_get_buffer(AVCodecContext *s, AVFrame *frame, int flags)
{
    InputStream  *ist = (InputStream  *)s->opaque;
    DXVA2Context *ctx = (DXVA2Context *)ist->hwaccel_ctx;
    int i, old_unused = -1;
    LPDIRECT3DSURFACE9 surface;
    DXVA2SurfaceWrapper *w = NULL;

    av_assert0(frame->format == AV_PIX_FMT_DXVA2_VLD);

    for (i = 0; i < ctx->num_surfaces; i++) {
        surface_info *info = &ctx->surface_infos[i];
        if (!info->used && (old_unused == -1 || info->age < ctx->surface_infos[old_unused].age))
            old_unused = i;
    }
    if (old_unused == -1) {
        av_log(NULL, AV_LOG_ERROR, "No free DXVA2 surface!\n");
        return AVERROR(ENOMEM);
    }
    i = old_unused;

    surface = ctx->surfaces[i];

    w = (DXVA2SurfaceWrapper *)av_mallocz(sizeof(*w));
    if (!w)
        return AVERROR(ENOMEM);

    frame->buf[0] = av_buffer_create((uint8_t*)surface, 0,
        dxva2_release_buffer, w,
        AV_BUFFER_FLAG_READONLY);
    if (!frame->buf[0]) {
        av_free(w);
        return AVERROR(ENOMEM);
    }

    w->ctx = ctx;
    w->surface = surface;
    IDirect3DSurface9_AddRef(w->surface);
    w->decoder = ctx->decoder;
    IDirectXVideoDecoder_AddRef(w->decoder);
    //w->decoder->AddRef();

    ctx->surface_infos[i].used = 1;
    ctx->surface_infos[i].age = ctx->surface_age++;

    frame->data[3] = (uint8_t *)surface;

    return 0;
}

D3DPRESENT_PARAMETERS d3dpp = { 0 };
RECT m_rtViewport;
IDirect3DSurface9 * m_pDirect3DSurfaceRender = NULL;
IDirect3DSurface9 * m_pBackBuffer = NULL;
static int dxva2_retrieve_data(AVCodecContext *s, AVFrame *frame)
{
 //   LPDIRECT3DSURFACE9 surface = (LPDIRECT3DSURFACE9)frame->data[3];
 //   InputStream        *ist = (InputStream *)s->opaque;
 //   DXVA2Context       *ctx = (DXVA2Context *)ist->hwaccel_ctx;

	//EnterCriticalSection(&cs);

	//ctx->d3d9device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
	//ctx->d3d9device->BeginScene();
	//if (m_pBackBuffer)
	//{
	//	m_pBackBuffer->Release();
	//	m_pBackBuffer = NULL;
	//}
	//ctx->d3d9device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &m_pBackBuffer);
	//GetClientRect(d3dpp.hDeviceWindow, &m_rtViewport);
	//ctx->d3d9device->StretchRect(surface, NULL, m_pBackBuffer, &m_rtViewport, D3DTEXF_LINEAR);
	//ctx->d3d9device->EndScene();
	//ctx->d3d9device->Present(NULL, NULL, NULL, NULL);

	//LeaveCriticalSection(&cs);

    return 0;
}

static int dxva2_alloc(AVCodecContext *s)
{
    InputStream *ist = (InputStream *)s->opaque;
    int loglevel = (ist->hwaccel_id == HWACCEL_AUTO) ? AV_LOG_VERBOSE : AV_LOG_ERROR;
    DXVA2Context *ctx;
    pDirect3DCreate9      *createD3D = NULL;
    pCreateDeviceManager9 *createDeviceManager = NULL;
    HRESULT hr;
    D3DDISPLAYMODE        d3ddm;
    unsigned resetToken = 0;
    UINT adapter = D3DADAPTER_DEFAULT;

    ctx = (DXVA2Context *)av_mallocz(sizeof(*ctx));
    if (!ctx)
        return AVERROR(ENOMEM);

    ctx->deviceHandle = INVALID_HANDLE_VALUE;

    ist->hwaccel_ctx = ctx;
    ist->hwaccel_uninit = dxva2_uninit;
    ist->hwaccel_get_buffer = dxva2_get_buffer;
    ist->hwaccel_retrieve_data = dxva2_retrieve_data;

    ctx->d3dlib = LoadLibrary(L"d3d9.dll");
    if (!ctx->d3dlib) {
        av_log(NULL, loglevel, "Failed to load D3D9 library\n");
        goto fail;
    }
    ctx->dxva2lib = LoadLibrary(L"dxva2.dll");
    if (!ctx->dxva2lib) {
        av_log(NULL, loglevel, "Failed to load DXVA2 library\n");
        goto fail;
    }

    createD3D = (pDirect3DCreate9 *)GetProcAddress(ctx->d3dlib, "Direct3DCreate9");
    if (!createD3D) {
        av_log(NULL, loglevel, "Failed to locate Direct3DCreate9\n");
        goto fail;
    }
    createDeviceManager = (pCreateDeviceManager9 *)GetProcAddress(ctx->dxva2lib, "DXVA2CreateDirect3DDeviceManager9");
    if (!createDeviceManager) {
        av_log(NULL, loglevel, "Failed to locate DXVA2CreateDirect3DDeviceManager9\n");
        goto fail;
    }

    ctx->d3d9 = createD3D(D3D_SDK_VERSION);
    if (!ctx->d3d9) {
        av_log(NULL, loglevel, "Failed to create IDirect3D object\n");
        goto fail;
    }

    if (ist->hwaccel_device) {
        adapter = atoi(ist->hwaccel_device);
        printf("Using HWAccel device %d\n", adapter);
    }

    IDirect3D9_GetAdapterDisplayMode(ctx->d3d9, adapter, &d3ddm);
	d3dpp.Windowed = TRUE;
	d3dpp.BackBufferCount = 0;
	d3dpp.hDeviceWindow = NULL;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferFormat = d3ddm.Format;
	d3dpp.EnableAutoDepthStencil = FALSE;
	d3dpp.Flags = D3DPRESENTFLAG_VIDEO;
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

	D3DCAPS9 caps;
	DWORD BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED;
	hr = IDirect3D9_GetDeviceCaps(ctx->d3d9, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);
	//hr = ctx->d3d9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);
	if (SUCCEEDED(hr))
	{
		if (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
		{
			BehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE;
		}
		else
		{
			BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE;
		}
	}

	//creat device
	if (ctx->d3d9device)
	{
		IDirect3DDevice9_Release(ctx->d3d9device);
		//ctx->d3d9device->Release();
		ctx->d3d9device = NULL;
	}

	hr = IDirect3D9_CreateDevice(ctx->d3d9, adapter, D3DDEVTYPE_HAL, GetDesktopWindow(), BehaviorFlags, &d3dpp, &ctx->d3d9device);
	if (FAILED(hr)) {
		av_log(NULL, loglevel, "Failed to create Direct3D device\n");
		goto fail;
	}

	//creat devide manager
    hr = createDeviceManager(&resetToken, &ctx->d3d9devmgr);
    if (FAILED(hr)) {
        av_log(NULL, loglevel, "Failed to create Direct3D device manager\n");
        goto fail;
    }

    hr = IDirect3DDeviceManager9_ResetDevice(ctx->d3d9devmgr, ctx->d3d9device, resetToken);
    //hr = ctx->d3d9devmgr->ResetDevice(ctx->d3d9device, resetToken);
    if (FAILED(hr)) {
        av_log(NULL, loglevel, "Failed to bind Direct3D device to device manager\n");
        goto fail;
    }

	//creat video service
    hr = IDirect3DDeviceManager9_OpenDeviceHandle(ctx->d3d9devmgr, &ctx->deviceHandle);
    //hr = ctx->d3d9devmgr->OpenDeviceHandle(&ctx->deviceHandle);
    if (FAILED(hr)) {
        av_log(NULL, loglevel, "Failed to open device handle\n");
        goto fail;
    }

    hr = IDirect3DDeviceManager9_GetVideoService(ctx->d3d9devmgr, ctx->deviceHandle, &IID_IDirectXVideoDecoderService, (void **)&ctx->decoder_service);
    //hr = ctx->d3d9devmgr->GetVideoService(ctx->deviceHandle, IID_IDirectXVideoDecoderService, (void **)&ctx->decoder_service);
    if (FAILED(hr)) {
        av_log(NULL, loglevel, "Failed to create IDirectXVideoDecoderService\n");
        goto fail;
    }

    ctx->tmp_frame = av_frame_alloc();
    if (!ctx->tmp_frame)
        goto fail;

    s->hwaccel_context = av_mallocz(sizeof(struct dxva_context));
    if (!s->hwaccel_context)
        goto fail;

    return 0;
fail:
    dxva2_uninit(s);
    return AVERROR(EINVAL);
}

static int dxva2_get_decoder_configuration(AVCodecContext *s, const GUID *device_guid,
    const DXVA2_VideoDesc *desc,
    DXVA2_ConfigPictureDecode *config)
{
    InputStream  *ist = (InputStream *)s->opaque;
    int loglevel = (ist->hwaccel_id == HWACCEL_AUTO) ? AV_LOG_VERBOSE : AV_LOG_ERROR;
    DXVA2Context *ctx = (DXVA2Context *)ist->hwaccel_ctx;
    unsigned cfg_count = 0, best_score = 0;
    DXVA2_ConfigPictureDecode *cfg_list = NULL;
    DXVA2_ConfigPictureDecode best_cfg = { { 0 } };
    HRESULT hr;
    int i;

    hr = IDirectXVideoDecoderService_GetDecoderConfigurations(ctx->decoder_service, device_guid, desc, NULL, &cfg_count, &cfg_list);
    //hr = ctx->decoder_service->GetDecoderConfigurations(*device_guid, desc, NULL, &cfg_count, &cfg_list);
    if (FAILED(hr)) {
        av_log(NULL, loglevel, "Unable to retrieve decoder configurations\n");
        return AVERROR(EINVAL);
    }

    for (i = 0; i < cfg_count; i++) {
        DXVA2_ConfigPictureDecode *cfg = &cfg_list[i];

        unsigned score;
        if (cfg->ConfigBitstreamRaw == 1)
            score = 1;
        else if (s->codec_id == AV_CODEC_ID_H264 && cfg->ConfigBitstreamRaw == 2)
            score = 2;
        else
            continue;
        if (IsEqualGUID(&cfg->guidConfigBitstreamEncryption, &DXVA2_NoEncrypt))
            score += 16;
        if (score > best_score) {
            best_score = score;
            best_cfg = *cfg;
        }
    }
    CoTaskMemFree(cfg_list);

    if (!best_score) {
        av_log(NULL, loglevel, "No valid decoder configuration available\n");
        return AVERROR(EINVAL);
    }

    *config = best_cfg;
    return 0;
}

static const d3d_format_t *D3dFindFormat(D3DFORMAT format)
{
	for (unsigned i = 0; d3d_formats[i].name; i++) {
		if (d3d_formats[i].format == format)
			return &d3d_formats[i];
	}
	return NULL;
}

static char* get_guid_name(int index)
{
	switch (index)
	{
	case 2:
		return "DXVA2_ModeH264_F(1b81be69-a0c7-11d3-b984-00c04f2e73c5)";
	case 3:
		return "DXVA2_ModeH264_E(1b81be68-a0c7-11d3-b984-00c04f2ex73c5)";
	case 4:
		return "DXVADDI_Intel_ModeH264_E(604F8E68-4951-4C54-88FEABD25C15B3D6)";
	default:
		return "not support now";
	}
}

static int dxva2_create_decoder(AVCodecContext *s)
{
    InputStream  *ist = (InputStream *)s->opaque;
    int loglevel = (ist->hwaccel_id == HWACCEL_AUTO) ? AV_LOG_VERBOSE : AV_LOG_ERROR;
    DXVA2Context *ctx = (DXVA2Context *)ist->hwaccel_ctx;
	struct dxva_context *dxva_ctx = (struct dxva_context *)s->hwaccel_context;
    GUID *guid_list = NULL;
    unsigned guid_count = 0, i, j;
    GUID device_guid = GUID_NULL;
    D3DFORMAT target_format = D3DFMT_UNKNOWN;
    DXVA2_VideoDesc desc = { 0 };
    DXVA2_ConfigPictureDecode config;
    HRESULT hr;
    int surface_alignment;
    int ret;

	//find the best suited decoder mode GUID and render format
    hr = IDirectXVideoDecoderService_GetDecoderDeviceGuids(ctx->decoder_service, &guid_count, &guid_list);
    //hr = ctx->decoder_service->GetDecoderDeviceGuids(&guid_count, &guid_list);
    if (FAILED(hr)) {
        av_log(NULL, loglevel, "Failed to retrieve decoder device GUIDs\n");
        goto fail;
    }

    for (i = 0; dxva2_modes[i].guid; i++) {
        D3DFORMAT *target_list = NULL;
        unsigned target_count = 0;
        const dxva2_mode *mode = &dxva2_modes[i];
        if (mode->codec != s->codec_id)	//编码匹配
            continue;

		//匹配guid，设备支持的与我们需要的做匹配(能力强的，在前面)
        for (j = 0; j < guid_count; j++) {
			if (IsEqualGUID(&(*mode->guid), &guid_list[j])){
				printf("devices support decode, guid name: %s", get_guid_name(i));
				break;
			}
        }
        if (j == guid_count)
            continue;

        hr = IDirectXVideoDecoderService_GetDecoderRenderTargets(ctx->decoder_service, mode->guid, &target_count, &target_list);
        //hr = ctx->decoder_service->GetDecoderRenderTargets(*mode->guid, &target_count, &target_list);
        if (FAILED(hr)) {
			printf("get devices target list error, devide guid name: %s", get_guid_name(i));
            continue;
        }

		for (unsigned j = 0; j < target_count; j++)
		{
			const D3DFORMAT f = target_list[j];
			const d3d_format_t *format = D3dFindFormat(f);
			if (format) {
				printf("%s is supported for output ,devide guid name: %s", format->name, get_guid_name(i));
			}
		}

		//选中的设备支持的输出格式匹配
		for (unsigned j = 0; j < target_count; j++) {
            const D3DFORMAT format = target_list[j];
			if (format == MKTAG('N', 'V', '1', '2')) {
                target_format = format;
                break;
            }
        }
        CoTaskMemFree(target_list);
        if (target_format) {
            device_guid = *mode->guid;
			printf("final device guid:%s is chose for decode", get_guid_name(i));
            break;
        }
    }
    CoTaskMemFree(guid_list);

    if (IsEqualGUID(&device_guid, &GUID_NULL)) {
        av_log(NULL, loglevel, "No decoder device for codec found\n");
		printf("No decoder device for codec found\n");
        goto fail;
    }

    desc.SampleWidth = s->coded_width;
    desc.SampleHeight = s->coded_height;
    desc.Format = target_format;

	//设备解码
    ret = dxva2_get_decoder_configuration(s, &device_guid, &desc, &config);
    if (ret < 0) {
        goto fail;
    }

    /* decoding MPEG-2 requires additional alignment on some Intel GPUs,
    but it causes issues for H.264 on certain AMD GPUs..... */
    if (s->codec_id == AV_CODEC_ID_MPEG2VIDEO)
        surface_alignment = 32;
    /* the HEVC DXVA2 spec asks for 128 pixel aligned surfaces to ensure
    all coding features have enough room to work with */
    else if (s->codec_id == AV_CODEC_ID_HEVC)
        surface_alignment = 128;
    else
        surface_alignment = 16;

    /* 4 base work surfaces */
    ctx->num_surfaces = 4;

    /* add surfaces based on number of possible refs */
    if (s->codec_id == AV_CODEC_ID_H264 || s->codec_id == AV_CODEC_ID_HEVC)
        ctx->num_surfaces += 16;
    else if (s->codec_id == AV_CODEC_ID_VP9)
        ctx->num_surfaces += 8;
    else
        ctx->num_surfaces += 2;

    /* add extra surfaces for frame threading */
    if (s->active_thread_type & FF_THREAD_FRAME)
        ctx->num_surfaces += s->thread_count;

    ctx->surfaces = (LPDIRECT3DSURFACE9 *)av_mallocz(ctx->num_surfaces * sizeof(*ctx->surfaces));
    ctx->surface_infos = (surface_info *)av_mallocz(ctx->num_surfaces * sizeof(*ctx->surface_infos));

    if (!ctx->surfaces || !ctx->surface_infos) {
        av_log(NULL, loglevel, "Unable to allocate surface arrays\n");
        goto fail;
    }

    hr = IDirectXVideoDecoderService_CreateSurface(ctx->decoder_service,
        FFALIGN(s->coded_width, surface_alignment),
        FFALIGN(s->coded_height, surface_alignment),
        ctx->num_surfaces - 1,
        target_format, D3DPOOL_DEFAULT, 0,
        DXVA2_VideoDecoderRenderTarget,
        ctx->surfaces, NULL);
	/*hr = ctx->decoder_service->CreateSurface(FFALIGN(s->coded_width, surface_alignment),
        FFALIGN(s->coded_height, surface_alignment),
        ctx->num_surfaces - 1,
        target_format, D3DPOOL_DEFAULT, 0,
        DXVA2_VideoDecoderRenderTarget,
        ctx->surfaces, NULL);*/

    if (FAILED(hr)) {
        av_log(NULL, loglevel, "Failed to create %d video surfaces\n", ctx->num_surfaces);
        goto fail;
    }

    hr = IDirectXVideoDecoderService_CreateVideoDecoder(ctx->decoder_service, &device_guid,
        &desc, &config, ctx->surfaces,
        ctx->num_surfaces, &ctx->decoder);
	/*hr = ctx->decoder_service->CreateVideoDecoder(device_guid,
        &desc, &config, ctx->surfaces,
        ctx->num_surfaces, &ctx->decoder);*/
    if (FAILED(hr)) {
        av_log(NULL, loglevel, "Failed to create DXVA2 video decoder\n");
        goto fail;
    }

    ctx->decoder_guid = device_guid;
    ctx->decoder_config = config;

    dxva_ctx->cfg = &ctx->decoder_config;
    dxva_ctx->decoder = ctx->decoder;
    dxva_ctx->surface = ctx->surfaces;
    dxva_ctx->surface_count = ctx->num_surfaces;

    if (IsEqualGUID(&ctx->decoder_guid, &DXVADDI_Intel_ModeH264_E))
        dxva_ctx->workaround |= FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO;

    return 0;
fail:
    dxva2_destroy_decoder(s);
    return AVERROR(EINVAL);
}

int dxva2_init(AVCodecContext *s)
{
	InitializeCriticalSection(&cs);

    InputStream *ist = (InputStream *)s->opaque;
    int loglevel = (ist->hwaccel_id == HWACCEL_AUTO) ? AV_LOG_VERBOSE : AV_LOG_ERROR;
    DXVA2Context *ctx;
    int ret;

    if (!ist->hwaccel_ctx) {
        ret = dxva2_alloc(s);
        if (ret < 0)
            return ret;
    }
    ctx = (DXVA2Context *)ist->hwaccel_ctx;

    if (s->codec_id == AV_CODEC_ID_H264 &&
        (s->profile & ~FF_PROFILE_H264_CONSTRAINED) > FF_PROFILE_H264_HIGH) {
        av_log(NULL, loglevel, "Unsupported H.264 profile for DXVA2 HWAccel: %d\n", s->profile);
        return AVERROR(EINVAL);
    }

    if (ctx->decoder)
        dxva2_destroy_decoder(s);

    ret = dxva2_create_decoder(s);
    if (ret < 0) {
        av_log(NULL, loglevel, "Error creating the DXVA2 decoder\n");
        return ret;
    }

    return 0;
}

int dxva2_retrieve_data_call(AVCodecContext *s, AVFrame *frame)
{
    return dxva2_retrieve_data(s, frame);
}

static void copy_plane(uint8_t *dst, size_t dst_pitch, const uint8_t *src, size_t src_pitch,
						unsigned width, unsigned height)
{
	for (unsigned y = 0; y < height; y++) {
		memcpy(dst, src, width);
		src += src_pitch;
		dst += dst_pitch;
	}
}

static void split_planes(uint8_t *dstu, size_t dstu_pitch,
							uint8_t *dstv, size_t dstv_pitch,
							const uint8_t *src, size_t src_pitch,
							unsigned width, unsigned height)
{
	for (unsigned y = 0; y < height; y++) {
		for (unsigned x = 0; x < width; x++) {
			dstu[x] = src[2 * x + 0];
			dstv[x] = src[2 * x + 1];
		}
		src += src_pitch;
		dstu += dstu_pitch;
		dstv += dstv_pitch;
	}
}

static void copy_from_nv12(AVFrame *dst, uint8_t *src[2], size_t src_pitch[2],
							unsigned width, unsigned height)
{
	copy_plane(dst->data[0], dst->linesize[0], src[0], src_pitch[0], width, height);
	split_planes(dst->data[1], dst->linesize[1], dst->data[2], dst->linesize[2], src[1], src_pitch[1], width / 2, height / 2);
}

int dxva2_get_nv12_data(AVCodecContext *s, AVFrame *frame)
{
	if (!frame->data[3]){
		return -1;
	}

	IDirect3DSurface9* pSurface = (IDirect3DSurface9 *)frame->data[3];
	D3DLOCKED_RECT lRect;
	D3DSURFACE_DESC SurfaceDesc;
	IDirect3DSurface9_GetDesc(pSurface, &SurfaceDesc);
	//pSurface->GetDesc(&SurfaceDesc);
	HRESULT hr = IDirect3DSurface9_LockRect(pSurface, &lRect, NULL, D3DLOCK_READONLY);
	//HRESULT hr = pSurface->LockRect(&lRect, nullptr, D3DLOCK_READONLY);
	av_image_fill_arrays(frame->data, frame->linesize, (uint8_t*)lRect.pBits, AV_PIX_FMT_NV12, s->width, s->height, 1);
	frame->format = AV_PIX_FMT_NV12;
	IDirect3DSurface9_UnlockRect(pSurface);
	//pSurface->UnlockRect();

	//static FILE* fp_yuv = fopen("nv12.yuv", "wb+");
	//fwrite(frame->data[0], 1, lRect.Pitch*SurfaceDesc.Height, fp_yuv);
	//fwrite(frame->data[1], 1, lRect.Pitch*SurfaceDesc.Height / 2, fp_yuv);
	//fflush(fp_yuv);

	return 0;
}

int dxva2_get_yuv420_data(AVCodecContext *s, AVFrame *src, AVFrame *dst)
{
	LPDIRECT3DSURFACE9 d3d = (LPDIRECT3DSURFACE9)(uintptr_t)src->data[3];
	D3DLOCKED_RECT lock;
	if (FAILED(IDirect3DSurface9_LockRect(d3d, &lock, NULL, D3DLOCK_READONLY))) {
		av_log(NULL, AV_LOG_ERROR, "Failed to lock surface");
		return -1;
	}

	uint8_t *plane[2] = {
		(uint8_t*)lock.pBits,
		(uint8_t*)lock.pBits + lock.Pitch * s->coded_height
	};
	size_t  pitch[2] = {
		lock.Pitch,
		lock.Pitch,
	};

	copy_from_nv12(dst, plane, pitch, s->width, s->height);
	dst->width  = s->width;
	dst->height = s->height;
	dst->format = AV_PIX_FMT_YUV420P;
	dst->key_frame = src->key_frame;
	dst->pts = src->pts;
	dst->pkt_dts = src->pkt_dts;
	dst->pkt_pts = src->pkt_pts;
	IDirect3DSurface9_UnlockRect(d3d);

	//static FILE* fp_yuv = fopen("yuv420.yuv", "wb+");
	//int y_size = s->width * s->height;
	//fwrite(dst->data[0], 1, y_size, fp_yuv);		//Y 
	//fwrite(dst->data[1], 1, y_size / 4, fp_yuv);	//U
	//fwrite(dst->data[2], 1, y_size / 4, fp_yuv);	//V
	//fflush(fp_yuv);

	return 0;
}