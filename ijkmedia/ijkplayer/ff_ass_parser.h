//
//  ff_ass_parser.h
//  IJKMediaPlayerKit
//
//  Created by Reach Matt on 2022/5/17.
//

#ifndef ff_ass_parser_h
#define ff_ass_parser_h

#include <stdio.h>

//need free the return value! see av_free();
char * parse_ass_subtitle(const char *ass);

#endif /* ff_ass_parser_h */
