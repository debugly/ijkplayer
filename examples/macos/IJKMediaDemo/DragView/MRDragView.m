//
//  MRDragView.m
//  FFmpegTutorial-macOS
//
//  Created by Matt Reach on 2020/12/2.
//

#import "MRDragView.h"

@implementation MRDragView

- (void)registerDragTypes
{
    
    if (@available(macOS 10.13, *)) {
        [self registerForDraggedTypes:[NSArray arrayWithObjects:NSPasteboardTypeFileURL, nil]];
    } else if (@available(macOS 10.0, *)){
#pragma clang diagnostic push
#pragma clang diagnostic ignored"-Wdeprecated-declarations"
        [self registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType, nil]];
#pragma clang diagnostic pop
    }
}

- (id)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        //注册文件拖动事件
        [self registerDragTypes];
    }
    return self;
}

- (void)awakeFromNib
{
    [super awakeFromNib];
    [self registerDragTypes];
}

- (void)dealloc
{
    [self unregisterDraggedTypes];
}

- (NSArray *)draggedFileList:(id<NSDraggingInfo> _Nonnull)sender
{
    NSPasteboard *pboard = [sender draggingPasteboard];
    NSArray *list = nil;
    if (@available(macOS 10.13, *)) {
        if ([[pboard types] containsObject:NSPasteboardTypeFileURL]) {
            list = [pboard readObjectsForClasses:@[[NSURL class]] options:nil];
        }
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored"-Wdeprecated-declarations"
        if ([[pboard types] containsObject:NSFilenamesPboardType]) {
            list = [pboard propertyListForType:NSFilenamesPboardType];
        }
#pragma clang diagnostic pop
    }
    
    NSMutableArray *result = [NSMutableArray arrayWithArray:list];
    for (int i = 0; i < [result count]; i ++) {
        id obj = result[i];
        if ([obj isKindOfClass:[NSString class]]) {
            obj = [NSURL fileURLWithPath:(NSString *)obj];
            result[i] = obj;
        }
    }
    return [result copy];
}

//当文件被拖动到界面触发
- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    NSArray * list = [self draggedFileList:sender];
    if (self.delegate) {
        return [self.delegate acceptDragOperation:list];
    }
    return NSDragOperationNone;
}

//当文件在界面中放手
- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender
{
    NSArray * list = [self draggedFileList:sender];
    if (list.count && self.delegate) {
        [self.delegate handleDragFileList:list];
    }
    return YES;
}

@end
