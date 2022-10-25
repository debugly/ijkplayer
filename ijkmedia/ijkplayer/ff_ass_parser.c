//
//  ff_ass_parser.c
//  IJKMediaPlayerKit
//
//  Created by Reach Matt on 2022/5/17.
//

#include "ff_ass_parser.h"
#include <libavutil/mem.h>

static const char * remove_ass_line_header(const char *ass)
{
    const char *tok = NULL;
    tok = strchr(ass, ':'); if (tok) tok += 1; // skip event
    tok = strchr(tok, ','); if (tok) tok += 1; // skip layer
    tok = strchr(tok, ','); if (tok) tok += 1; // skip start_time
    tok = strchr(tok, ','); if (tok) tok += 1; // skip end_time
    tok = strchr(tok, ','); if (tok) tok += 1; // skip style
    tok = strchr(tok, ','); if (tok) tok += 1; // skip name
    tok = strchr(tok, ','); if (tok) tok += 1; // skip margin_l
    tok = strchr(tok, ','); if (tok) tok += 1; // skip margin_r
    tok = strchr(tok, ','); if (tok) tok += 1; // skip margin_v
    tok = strchr(tok, ','); if (tok) tok += 1; // skip effect
    return tok;
}

static void replace_N_to_n(char *buffer)
{
    int len = (int)strlen(buffer);
    if (len > 0) {
        do {
            char *found = strstr(buffer, "\\N");
            if (found) {
                *(found) = '\n';
                memmove(found + 1, found + 2, strlen(found + 2));
                *(buffer+len - 1) = '\0';
            } else {
                break;
            }
        } while(1);
    }
}

static void remove_last_rn(char *buffer)
{
    int len = (int)strlen(buffer);
    if (len > 0) {
        char *found = strstr(buffer, "\r\n");
        if (found) {
            if (found + 2 == buffer + len) {
                *(found) = '\0';
            }
        }
    }
}

static void remove_last_n(char *buffer)
{
    int len = (int)strlen(buffer);
    if (len > 0) {
        char *found = strstr(buffer, "\n");
        if (found) {
            if (found + 1 == buffer + len) {
                *(found) = '\0';
            }
        }
    }
}

static char * remove_ass_line_effect(const char *ass)
{
    while (ass && strlen(ass) > 2) {
        //移除 { 开头并且 } 结尾的特效内容
        if (ass[0] == '{') {
            char* end = strchr(ass, '}');
            if (end) {
                ass = end + 1;
            } else {
                break;
            }
        } else {
            break;
        }
    }
    
    char *buffer = av_malloc(strlen(ass) + 1);
    bzero(buffer, strlen(ass) + 1);
    memcpy(buffer, ass, strlen(ass));
    
    while (1) {
        char *left  = strstr(buffer, "{");
        char *right = strstr(buffer, "}");
        if (left && right > left) {
            if (right - buffer == strlen(buffer)) {
                bzero(left, right - left);
            } else {
                int count = (int)strlen(buffer) - (int)(right - buffer);
                memmove(left, right + 1, count);
                bzero(left + count + 1, 1);
            }
        } else {
            break;
        }
    }
    return buffer;
}

//need free!
char * parse_ass_subtitle(const char *ass)
{
//    ass = "Dialogue: 0,0:00:00.00,0:00:00.00,Default,,0,0,0,,这里是自由城\\N{\\fn微软雅黑\\fs52.5\\b0\\bord0}{\\i1}This is Free City.{\\i}{\\fad(150,150)}在不断衍生的黑暗之中  相互交换了\r\n{\\i1}";
//    ass = "Dialogue: 0,0:00:00.00,0:00:00.00,Default,,0,0,0,,{\an3\fnDFKai-SB\c&HFEFDFD&\3c&H070101&\pos(260,422)}{\fad(300,150)}字幕来源 X2&CASO";
    
    const char *text = remove_ass_line_header(ass);
    if (text && strlen(text) > 0) {
        char *buffer = remove_ass_line_effect(text);
        replace_N_to_n(buffer);
        remove_last_rn(buffer);
        remove_last_n(buffer);
        return buffer;
    }
    return NULL;
}
