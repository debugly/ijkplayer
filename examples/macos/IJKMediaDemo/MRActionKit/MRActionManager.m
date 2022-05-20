//
//  MRActionManager.m
//  MRPlayer
//
//  Created by Matt Reach on 2019/8/2.
//  Copyright © 2022 IJK Mac. All rights reserved.
//

#import "MRActionManager.h"
#import "MRActionItem.h"
#import "MRActionProcessor.h"
#import "MRActionProcessor+Private.h"

#define MakeActionError(_code,_desc) \
if (error) {    \
*error = [NSError errorWithDomain:@"com.sohu.action" code: _code userInfo:@{NSLocalizedDescriptionKey: _desc}]; \
}

@implementation MRActionManager

+ (BOOL)handleActionWithURL:(NSString *)url error:(NSError *__autoreleasing *)error
{
    MRActionItem *item = [MRActionItem actionItemWithString:url];
    if (!item) {
        MakeActionError(-100, @"URL 不合法！");
        return NO;
    }
    return [self handleActionWithItem:item error:error];
}

+ (BOOL)handleActionWithItem:(MRActionItem *)item error:(NSError *__autoreleasing *)error
{
    if (!item) {
        MakeActionError(-101, @"item 不能为空！");
        return NO;
    }
    
    NSString *scheme = item.scheme;
    
    NSArray<MRActionProcessor *>*processArr = [_s_processor_arr copy];
    
    MRActionProcessor *target = nil;
    for (MRActionProcessor *processor in processArr) {
        if ([processor.scheme isEqualToString:scheme]) {
            target = processor;
            break;
        }
    }
    
    if (target) {
        MRActionHandler handler = [target handlerForPath:item.path];
        if (handler) {
            handler(item);
            return YES;
        } else {
            NSString *desc = [NSString stringWithFormat:@"can't find match Handler for [%@://action.cmd%@]！", scheme,item.path];
            MakeActionError(-100, desc);
            return NO;
        }
    } else {
        NSString *desc = [NSString stringWithFormat:@"can't find match Processor for [%@]！", scheme];
        MakeActionError(-100, desc);
        return NO;
    }
}

static NSArray <MRActionProcessor *>*_s_processor_arr;

+ (void)registerProcessor:(MRActionProcessor *)processor
{
    [self registerProcessor:processor forScheme:processor.scheme];
}

+ (void)registerProcessor:(MRActionProcessor *)processor forScheme:(NSString *)scheme
{
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:1];
    [array addObject:processor];
    if (_s_processor_arr) {
        [array addObjectsFromArray:_s_processor_arr];
    }
    _s_processor_arr = [array copy];
}

@end
