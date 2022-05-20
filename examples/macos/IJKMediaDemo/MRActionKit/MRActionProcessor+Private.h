//
//  MRActionProcessor+Private.h
//  MRPlayer
//
//  Created by Matt Reach on 2019/8/5.
//  Copyright © 2022 IJK Mac. All rights reserved.
//

#import "MRActionProcessor.h"

@interface MRActionProcessor (Private)

- (MRActionHandler)handlerForPath:(NSString *)path;

@end
