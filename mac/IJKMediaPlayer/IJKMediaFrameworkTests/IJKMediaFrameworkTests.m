//
//  IJKMediaFrameworkTests.m
//  IJKMediaFrameworkTests
//
//  Created by Zhang Rui on 15/7/31.
//  Copyright (c) 2015年 bilibili. All rights reserved.
//

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif
#import <XCTest/XCTest.h>

@interface IJKMediaFrameworkTests : XCTestCase

@end

@implementation IJKMediaFrameworkTests

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)testExample {
    // This is an example of a functional test case.
    XCTAssert(YES, @"Pass");
}

- (void)testPerformanceExample {
    // This is an example of a performance test case.
    [self measureBlock:^{
        // Put the code you want to measure the time of here.
    }];
}

@end