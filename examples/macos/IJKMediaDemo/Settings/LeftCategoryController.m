//
//  LeftCategoryController.m
//  IJKMediaMacDemo
//
//  Created by Reach Matt on 2022/2/24.
//  Copyright © 2022 IJK Mac. All rights reserved.
//

#import "LeftCategoryController.h"

@interface LeftCategoryController () <NSTableViewDelegate,NSTableViewDataSource>

@property (nonatomic, strong) NSArray *dataArr;
@property (weak) IBOutlet NSTableView *tableView;
@property (nonatomic, copy) void (^selectHandler)(NSDictionary *);

@end

@implementation LeftCategoryController

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didSelectRow:) name:NSTableViewSelectionDidChangeNotification object:self.tableView];
}

- (void)viewWillAppear
{
    [super viewWillAppear];
    self.view.window.titlebarAppearsTransparent = YES;
    self.view.window.styleMask |= NSWindowStyleMaskMiniaturizable;
    self.view.window.styleMask |= NSWindowStyleMaskFullSizeContentView;
    self.view.window.title = @"";
}

- (void)updateDataSource:(NSArray<NSDictionary *> *)arr
{
    self.dataArr = arr;
    [self.tableView reloadData];
}

- (void)onSelectItem:(void (^)(NSDictionary * _Nonnull))handler
{
    self.selectHandler = handler;
}

- (void)didSelectRow:(id)sender
{
    NSInteger row = [self.tableView selectedRow];
    NSDictionary *dic = self.dataArr[row];
    if (self.selectHandler) {
        self.selectHandler(dic);
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return [self.dataArr count];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    return self.dataArr[row];
}

@end
