//
//  LeftCategoryController.h
//  IJKMediaMacDemo
//
//  Created by Reach Matt on 2022/2/24.
//  Copyright © 2022 IJK Mac. All rights reserved.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface LeftCategoryController : NSViewController

- (void)updateDataSource:(NSArray <NSDictionary *>*)arr;
- (void)onSelectItem:(void(^)(NSDictionary *))handler;

@end

NS_ASSUME_NONNULL_END
