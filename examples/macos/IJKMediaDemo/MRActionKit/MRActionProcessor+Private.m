//
//  MRActionProcessor+Private.m
//  MRPlayer
//
//  Created by Matt Reach on 2019/8/5.
//  Copyright © 2022 IJK Mac. All rights reserved.
//

#import "MRActionProcessor+Private.h"
#import "MRActionProcessorInternal.h"

@implementation MRActionProcessor (Private)

- (MRActionHandler)handlerForPath:(NSString *)path
{
    return [self.acitonHandlerMap objectForKey:path];
}

@end
