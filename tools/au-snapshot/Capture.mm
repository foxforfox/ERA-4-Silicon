// Renders an NSView (and its subviews) into a PNG at a fixed pixel scale.
// Kept apart from Main.cpp so Cocoa and JUCE names never meet.
#import <Cocoa/Cocoa.h>
#include <initializer_list>

extern "C" bool captureNSViewToPNG (void* viewPtr, const char* path, double scale)
{
    @autoreleasepool
    {
        NSView* view = (__bridge NSView*) viewPtr;
        if (view == nil)
            return false;

        const NSRect r = [view bounds];
        NSBitmapImageRep* rep = [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes: nullptr
                          pixelsWide: (NSInteger) (r.size.width  * scale)
                          pixelsHigh: (NSInteger) (r.size.height * scale)
                       bitsPerSample: 8
                     samplesPerPixel: 4
                            hasAlpha: YES
                            isPlanar: NO
                      colorSpaceName: NSDeviceRGBColorSpace
                         bytesPerRow: 0
                        bitsPerPixel: 0];
        [rep setSize: r.size];
        [view cacheDisplayInRect: r toBitmapImageRep: rep];

        NSData* data = [rep representationUsingType: NSBitmapImageFileTypePNG properties: @{}];
        return [data writeToFile: [NSString stringWithUTF8String: path] atomically: YES];
    }
}

// Sends a left click (down then up) to a point given in the view's own
// top-left-origin coordinates, as if the user had clicked there.
extern "C" void clickInNSView (void* viewPtr, double x, double y)
{
    @autoreleasepool
    {
        NSView* view = (__bridge NSView*) viewPtr;
        NSWindow* window = [view window];
        if (window == nil)
            return;

        const NSPoint local = [view isFlipped] ? NSMakePoint (x, y)
                                               : NSMakePoint (x, [view bounds].size.height - y);
        const NSPoint inWindow = [view convertPoint: local toView: nil];

        for (NSEventType type : { NSEventTypeLeftMouseDown, NSEventTypeLeftMouseUp })
        {
            NSEvent* event = [NSEvent mouseEventWithType: type
                                                location: inWindow
                                           modifierFlags: 0
                                               timestamp: [[NSProcessInfo processInfo] systemUptime]
                                            windowNumber: [window windowNumber]
                                                 context: nil
                                             eventNumber: 0
                                              clickCount: 1
                                                pressure: type == NSEventTypeLeftMouseDown ? 1.0f : 0.0f];
            [window sendEvent: event];
        }
    }
}
