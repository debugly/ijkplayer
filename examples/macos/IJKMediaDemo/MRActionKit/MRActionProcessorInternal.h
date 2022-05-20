//
//  MRActionProcessorInternal.h
//  MRPlayer
//
//  Created by Matt Reach on 2019/8/5.
//  Copyright © 2022 IJK Mac. All rights reserved.
//

#import "MRActionProcessor.h"

@interface MRActionProcessor ()

@property (nonatomic, strong) NSDictionary *acitonHandlerMap;
@property (nonatomic, copy, readwrite) NSString *scheme;

@end
