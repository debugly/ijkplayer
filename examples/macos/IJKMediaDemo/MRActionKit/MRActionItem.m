//
//  MRActionItem.m
//  MRFoundation
//
//  Created by Matt Reach on 2019/8/5.
//  Copyright © 2022 IJK Mac. All rights reserved.
//

#import "MRActionItem.h"

@interface MRActionItem ()

@property (nonatomic, copy, readwrite) NSString *scheme;
@property (nonatomic, copy, readwrite) NSString *host;
@property (nonatomic, copy, readwrite) NSNumber *port;
@property (nonatomic, copy, readwrite) NSString *path;
@property (nonatomic, copy, readwrite) NSString *query;
@property (nonatomic, strong, readwrite) NSDictionary *queryMap;

@end

@implementation MRActionItem

- (instancetype)initWithURLString:(NSString *)string
{
    if (!string || string.length == 0) {
        return nil;
    }
    //配置action的时候可能前后有空格，去除掉空格
    string = [string stringByTrimmingCharactersInSet:[NSMutableCharacterSet whitespaceAndNewlineCharacterSet]];
    //解决中文导致的，构造URL失败，这里可能将 action 里的 url 二次编码，如果 url 是编过码的！
    string = [string stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLQueryAllowedCharacterSet]];
    
    NSURL *url = [NSURL URLWithString:string];
    if (!url) {
        return nil;
    }
    
    return [self initWithURL:url];
}

- (instancetype)initWithURL:(NSURL *)url
{
    self = [super init];
    if (self) {
        self.scheme = url.scheme;
        self.host = url.host;
        self.port = url.port;
        self.path = url.path;
        self.query = url.query;
    }
    return self;
}

+ (instancetype)actionItemWithString:(NSString *)string
{
    return [[self alloc]initWithURLString:string];
}

- (NSDictionary *)queryMap
{
    if (!_queryMap) {
        NSMutableDictionary *map = [[NSMutableDictionary alloc]init];
        if (self.query && self.query.length > 1) {
            NSArray *items = [self.query componentsSeparatedByString:@"&"];
            for (NSString *item in items) {
                NSArray *keyValue = [item componentsSeparatedByString:@"="];
                
                NSString *key = [keyValue firstObject];
                NSString *value = [keyValue lastObject];
                //URL解码
                NSString *v = [value stringByRemovingPercentEncoding];
                while ([v rangeOfString:@"%"].location != NSNotFound) {
                    v = [v stringByRemovingPercentEncoding];
                }
                [map setObject:v forKey:key];
            }
        }
        _queryMap = [map copy];
    }
    return _queryMap;
}
@end
