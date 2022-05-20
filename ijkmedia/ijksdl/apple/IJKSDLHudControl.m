/*
 * IJKSDLHudControl.h
 *
 * Copyright (c) 2013-2014 Bilibili
 * Copyright (c) 2013-2014 Zhang Rui <bbcallen@gmail.com>
 *
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#import "IJKSDLHudControl.h"
#if TARGET_OS_OSX
#import "IJKHudRowView.h"
typedef NSScrollView HudContentView;
typedef NSTableView UITableView;
#else
#import "IJKSDLHudViewCell.h"
typedef UITableView HudContentView;
#endif

@interface HudViewCellData : NSObject
@property(nonatomic) NSString *key;
@property(nonatomic) NSString *value;
@end

@implementation HudViewCellData
@end


@interface IJKSDLHudControl ()
#if TARGET_OS_OSX
<NSTableViewDelegate,NSTableViewDataSource>
#else
<UITableViewDelegate,UITableViewDataSource>
#endif

@property (nonatomic, strong) NSMutableDictionary *keyIndexes;
@property (nonatomic, strong) NSMutableArray *hudDataArray;
@property (nonatomic, strong) HudContentView *view;

@end

@implementation IJKSDLHudControl

// for debug
//- (instancetype)initWithFrame:(NSRect)frameRect
//{
//    self = [super initWithFrame:frameRect];
//    if (self) {
//        [self setWantsLayer:YES];
//        self.layer.backgroundColor = [[NSColor blueColor] CGColor];
//    }
//    return self;
//}

- (NSMutableDictionary *)keyIndexes
{
    if (!_keyIndexes) {
        _keyIndexes = [NSMutableDictionary dictionary];
    }
    return _keyIndexes;
}

- (NSMutableArray *)hudDataArray
{
    if (!_hudDataArray) {
        _hudDataArray = [NSMutableArray array];
    }
    return _hudDataArray;
}

- (UIView *)contentView
{
    if (!self.view) {
        self.view = [self prepareContentView];
    }
    return self.view;
}

- (void)destroyContentView
{
    [self.view removeFromSuperview];
    self.view = nil;
}

- (void)setHudValue:(NSString *)value forKey:(NSString *)key
{
    HudViewCellData *data = nil;
    NSNumber *index = [self.keyIndexes objectForKey:key];
    if (index == nil) {
        data = [[HudViewCellData alloc] init];
        data.key = key;
        [self.keyIndexes setObject:[NSNumber numberWithUnsignedInteger:self.hudDataArray.count]
                        forKey:key];
        [self.hudDataArray addObject:data];
    } else {
        data = [self.hudDataArray objectAtIndex:[index unsignedIntegerValue]];
    }

    data.value = value;
    [self.tableView reloadData];
}

- (NSDictionary *)allHudItem
{
    NSMutableDictionary *dic = [NSMutableDictionary dictionary];
    for (HudViewCellData *data in self.hudDataArray) {
        if (data.key && data.value) {
            [dic setValue:data.value forKey:data.key];
        }
    }
    return [dic copy];
}

#if TARGET_OS_OSX
- (NSScrollView *)prepareContentView
{
    NSScrollView * scrollView = [[NSScrollView alloc] initWithFrame:CGRectMake(0, 0, 200, 300)];
    scrollView.hasVerticalScroller = NO;
    scrollView.hasHorizontalScroller = NO;
    scrollView.drawsBackground = NO;
    
    NSTableView *tableView = [[NSTableView alloc] initWithFrame:self.view.bounds];
    tableView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    tableView.intercellSpacing = NSMakeSize(0, 0);
    tableView.delegate = self;
    tableView.dataSource = self;
    tableView.selectionHighlightStyle = NSTableViewSelectionHighlightStyleNone;
    tableView.headerView = nil;
    tableView.usesAlternatingRowBackgroundColors = NO;
    tableView.rowSizeStyle = NSTableViewRowSizeStyleCustom;
    tableView.backgroundColor = [NSColor colorWithWhite:5/255.0 alpha:0.5];
    tableView.rowHeight = 25;
    scrollView.contentView.documentView = tableView;
    return scrollView;
}

- (UITableView *)tableView
{
    return self.view.contentView.documentView;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return [self.hudDataArray count];
}

- (nullable NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(nullable NSTableColumn *)tableColumn row:(NSInteger)row
{
    return nil;
}

- (NSTableRowView *)tableView:(NSTableView *)tableView rowViewForRow:(NSInteger)row
{
    IJKHudRowView *rowView = [tableView makeViewWithIdentifier:@"Row" owner:self];
    if (rowView == nil) {
        rowView = [[IJKHudRowView alloc]init];
        rowView.identifier = @"Row";
    }
    if (row < [self.hudDataArray count]) {
        HudViewCellData *data = [self.hudDataArray objectAtIndex:row];
        [rowView updateTitle:data.key];
        [rowView updateDetail:data.value];
    }
    
    return rowView;
}

#else
- (UITableView *)tableView
{
    return self.view;
}

- (UITableView *)prepareContentView
{
    UITableView *tableView = [[UITableView alloc] initWithFrame:CGRectMake(0, 0, 200, 300) style:UITableViewStylePlain];
    tableView.dataSource = self;
    tableView.delegate = self;
    tableView.backgroundColor = [[UIColor alloc] initWithRed:.5f green:.5f blue:.5f alpha:.5f];
    tableView.separatorStyle  = UITableViewCellSeparatorStyleNone;
    
    return tableView;
}

#pragma mark UITableViewDataSource

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    assert(section == 0);
    return _hudDataArray.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    assert(indexPath.section == 0);

    IJKSDLHudViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"hud"];
    if (cell == nil) {
        cell = [[IJKSDLHudViewCell alloc] init];
    }

    HudViewCellData *data = [_hudDataArray objectAtIndex:indexPath.item];

    [cell setHudValue:data.value forKey:data.key];

    return cell;
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
{
    return 16.f;
}

#endif

@end
