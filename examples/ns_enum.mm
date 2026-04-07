// Real-world NS_ENUM patterns for testing
#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, UIViewAnimationCurve) {
    UIViewAnimationCurveEaseInOut,
    UIViewAnimationCurveEaseIn,
    UIViewAnimationCurveEaseOut,
    UIViewAnimationCurveLinear
};

typedef NS_OPTIONS(NSUInteger, UIViewAutoresizing) {
    UIViewAutoresizingNone                 = 0,
    UIViewAutoresizingFlexibleLeftMargin   = 1 << 0,
    UIViewAutoresizingFlexibleWidth        = 1 << 1,
    UIViewAutoresizingFlexibleRightMargin  = 1 << 2,
    UIViewAutoresizingFlexibleTopMargin    = 1 << 3,
    UIViewAutoresizingFlexibleHeight       = 1 << 4,
    UIViewAutoresizingFlexibleBottomMargin = 1 << 5
};

typedef NS_CLOSED_ENUM(NSInteger, NSComparisonResult) {
    NSOrderedAscending = -1,
    NSOrderedSame,
    NSOrderedDescending
};

typedef NS_ERROR_ENUM(NSInteger, NSCocoaError) {
    NSFileNoSuchFileError = 4,
    NSFileLockingError = 255,
    NSFileReadUnknownError = 256
};

@interface MyManager : NSObject
@property (nonatomic, strong, nonnull) NSString *name;
- (nonnull instancetype)init;
- (void)processWithCompletion:(void (^)(BOOL success))completion;
@end

@implementation MyManager
- (nonnull instancetype)init {
    self = [[super alloc] init];
    if (self) {
        _name = [[NSString alloc] initWithString:@"default"];
    }
    return self;
}

- (void)processWithCompletion:(void (^)(BOOL success))completion {
    @autoreleasepool {
        NSMutableArray *items = [[NSMutableArray alloc] initWithCapacity:10];
        for (id item in [[self dataSource] allItems]) {
            [items addObject:[[item description] uppercaseString]];
        }
        completion(YES);
    }
}
@end
