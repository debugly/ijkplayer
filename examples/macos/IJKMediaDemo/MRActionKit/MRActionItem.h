//
//  MRActionItem.h
//  MRFoundation
//
//  Created by Matt Reach on 2019/8/5.
//  Copyright © 2022 IJK Mac. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface MRActionItem : NSObject

@property (nonatomic, copy, readonly) NSString *scheme;
@property (nonatomic, copy, readonly) NSString *host;
@property (nonatomic, copy, readonly) NSNumber *port;
@property (nonatomic, copy, readonly) NSString *path;
@property (nonatomic, copy, readonly) NSString *query;
@property (nonatomic, strong, readonly) NSDictionary *queryMap;

- (instancetype)initWithURLString:(NSString *)string;
+ (instancetype)actionItemWithString:(NSString *)string;

@end
