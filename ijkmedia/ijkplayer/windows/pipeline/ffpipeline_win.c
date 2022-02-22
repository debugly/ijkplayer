#include "ffpipeline_win.h"
#include "ijkplayer/ff_ffplay.h"
#include "ijkplayer/pipeline/ffpipenode_ffplay_vdec.h"
#include "ijksdl/windows/ijksdl_aout_win_audio_directsound.h"

static SDL_Class g_pipeline_class = {	
	.name = "ffpipeline_win_media",
};

typedef struct IJKFF_Pipeline_Opaque {
	FFPlayer      *ffp;
	float          left_volume;
	float          right_volume;
} IJKFF_Pipeline_Opaque;

static void func_destroy(IJKFF_Pipeline *pipeline)
{
	return;
}

static IJKFF_Pipenode *func_open_video_decoder(IJKFF_Pipeline *pipeline, FFPlayer *ffp)
{
	return ffpipenode_create_video_decoder_from_ffplay(ffp);
}

static SDL_Aout *func_open_audio_output(IJKFF_Pipeline *pipeline, FFPlayer *ffp)
{
	SDL_Aout *aout = NULL;
	aout = SDL_AoutWin_CreateForAudio();
	if (aout)
		SDL_AoutSetStereoVolume(aout, pipeline->opaque->left_volume, pipeline->opaque->right_volume);
	return aout;
}

IJKFF_Pipeline *ffpipeline_create_from_win(FFPlayer *ffp)
{
	IJKFF_Pipeline_Opaque *opaque;
	ALOGD("ffpipeline_create_from_win()\n");
	IJKFF_Pipeline *pipeline = ffpipeline_alloc(&g_pipeline_class, sizeof(IJKFF_Pipeline_Opaque));
	if (!pipeline)
		return pipeline;

	opaque = pipeline->opaque;
	opaque->ffp = ffp;
	opaque->left_volume  = 50.0f;
	opaque->right_volume = 50.0f;

	pipeline->func_destroy = func_destroy;
	pipeline->func_open_video_decoder = func_open_video_decoder;
	pipeline->func_open_audio_output = func_open_audio_output;

	return pipeline;
}

void ffpipeline_win_set_volume(IJKFF_Pipeline* pipeline, float left, float right)
{
	IJKFF_Pipeline_Opaque *opaque = pipeline->opaque;
	opaque->left_volume = left;
	opaque->right_volume = right;

	if (opaque->ffp && opaque->ffp->aout) {
		SDL_AoutSetStereoVolume(opaque->ffp->aout, left, right);
	}
}

float ffpipeline_win_get_volume(IJKFF_Pipeline* pipeline)
{
	IJKFF_Pipeline_Opaque *opaque = pipeline->opaque;
	float volume = -1;

	if (opaque) {
		volume = opaque->left_volume;
	}
	return volume;
}
