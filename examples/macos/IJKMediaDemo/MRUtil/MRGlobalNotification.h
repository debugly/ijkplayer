//
//  MRGlobalNotification.h
//  IJKMediaDemo
//
//  Created by Matt Reach on 2019/3/14.
//  Copyright © 2019 IJK Mac. All rights reserved.
//
// 全局通知定义

#import <Foundation/Foundation.h>

#ifndef __MRECS__
#define __MRECS__

#define EXPORT_CONST_STRING(key) FOUNDATION_EXPORT NSString *const key
#define STRINGME_(x)    #x
#define STRINGME(x)     STRINGME_(x)
#define STRINGME2OC(x)  @STRINGME(x)
#define DEFINE_CONST_STRING(key) NSString *const key = STRINGME2OC(key)

#endif


///播放资源管理器打开的本地视频通知，参数为 obj = [ { bookmark,url },... ]
EXPORT_CONST_STRING(kPlayExplorerMovieNotificationName_G);
///播放在线视频，参数为 links = [ url1,url2,... ]
EXPORT_CONST_STRING(kPlayNetMovieNotificationName_G);
#define POST_NOTIFICATION(_name_,_obj_,_info_) \
        [[NSNotificationCenter defaultCenter] \
        postNotificationName: _name_ \
                      object: _obj_ \
                    userInfo: _info_]

#define OBSERVER_NOTIFICATION(_observer_,_sel_,_name_,_obj_) \
        [[NSNotificationCenter defaultCenter] \
            addObserver:_observer_ \
               selector:@selector(_sel_) \
                   name:_name_ \
                 object:_obj_ ]

#define STOP_OBSERING_NOTIFICATION(_observer_,_name_,_obj_) \
        [[NSNotificationCenter defaultCenter] \
            removeObserver: _observer_ \
                      name: _name_ \
                    object: _obj_]

@interface MRGlobalNotification : NSObject

@end
