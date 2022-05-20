//
//  MRActionProcessor.h
//  MRPlayer
//
//  Created by Matt Reach on 2019/8/5.
//  Copyright © 2022 IJK Mac. All rights reserved.
//

#import <Foundation/Foundation.h>

@class MRActionItem;

typedef void (^MRActionHandler)(MRActionItem *item);

@interface MRActionProcessor : NSObject

@property (nonatomic, copy, readonly) NSString *scheme;

- (instancetype)initWithScheme:(NSString *)scheme;

- (void)registerHandler:(MRActionHandler)handler forPath:(NSString *)path;

@end
