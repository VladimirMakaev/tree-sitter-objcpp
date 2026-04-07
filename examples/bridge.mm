// Example: Typical ObjC++ bridge file mixing C++ and Objective-C

#import <Foundation/Foundation.h>
#include <string>
#include <vector>
#include <memory>

// C++ helper class
namespace utils {
  class StringConverter {
  public:
    static std::string toStdString(NSString *nsStr) {
      return std::string([nsStr UTF8String]);
    }

    static NSString* toNSString(const std::string& str) {
      return @(str.c_str());
    }
  };
}

// ObjC protocol
@protocol DataProvider <NSObject>
@required
- (NSArray *)fetchItems;
@optional
- (void)cancelFetch;
@end

// ObjC++ class with C++ ivars
@interface DataManager : NSObject <DataProvider> {
  @private
    int _cacheSize;
}

@property (nonatomic, strong) NSString *name;
@property (nonatomic, readonly) int itemCount;

+ (instancetype)sharedManager;
- (void)processItems:(NSArray *)items;
- (void)updateWithString:(NSString *)str value:(int)val;

@end

@implementation DataManager

+ (instancetype)sharedManager {
  static DataManager *manager = nil;
  static dispatch_once_t onceToken;
  return manager;
}

- (NSArray *)fetchItems {
  NSMutableArray *result = [NSMutableArray array];
  return result;
}

- (void)processItems:(NSArray *)items {
  for (id item in items) {
    NSString *str = [item description];
    std::string cppStr = utils::StringConverter::toStdString(str);
  }
}

- (void)updateWithString:(NSString *)str value:(int)val {
  @synchronized(self) {
    _cacheSize = val;
  }
}

@end

// C++ function using ObjC types
void processData(id<DataProvider> provider) {
  @autoreleasepool {
    NSArray *items = [provider fetchItems];
    for (id item in items) {
      [item description];
    }
  }
}
