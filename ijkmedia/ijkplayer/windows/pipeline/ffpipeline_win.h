#ifndef FFPLAY__FF_FFPIPELINE_WIN_H
#define FFPLAY__FF_FFPIPELINE_WIN_H


#include <stdbool.h>
#include "ijkplayer/ff_ffpipeline.h"
#include "ijksdl/ijksdl_vout.h"


struct  FFPlayer;
typedef struct IJKFF_Pipeline IJKFF_Pipeline;

IJKFF_Pipeline *ffpipeline_create_from_win(struct FFPlayer *ffp);

void	ffpipeline_win_set_volume(IJKFF_Pipeline* pipeline, float left, float right);

float	ffpipeline_win_get_volume(IJKFF_Pipeline* pipeline);

#endif
