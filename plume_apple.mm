//
// plume
//
// Copyright (c) 2024 renderbag and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file for details.
//

#include "plume_apple.h"
#include <TargetConditionals.h>
#if TARGET_OS_OSX 
#import <AppKit/AppKit.h>
#import <IOKit/IOKitLib.h>
#else
#import <UIKit/UIKit.h>
#endif
#import <Foundation/Foundation.h>

#if TARGET_OS_OSX
static uint32_t plumeGetEntryProperty(io_registry_entry_t entry, CFStringRef propertyName) {
    uint32_t value = 0;
    CFTypeRef cfProp = IORegistryEntrySearchCFProperty(entry, kIOServicePlane, propertyName, kCFAllocatorDefault, kIORegistryIterateRecursively | kIORegistryIterateParents);

    if (cfProp) {
        if (CFGetTypeID(cfProp) == CFDataGetTypeID()) {
            const uint32_t* pValue = reinterpret_cast<const uint32_t*>(CFDataGetBytePtr((CFDataRef)cfProp));
            if (pValue) {
                value = *pValue;
            }
        }
        CFRelease(cfProp);
    }

    return value;
}
#endif
namespace plume {
    RenderDeviceVendor getRenderDeviceVendor(uint64_t registryID) {
    #if TARGET_OS_OSX
        io_service_t entry = IOServiceGetMatchingService(MACH_PORT_NULL, IORegistryEntryIDMatching(registryID));

        if (entry) {
            io_registry_entry_t parent;
            if (IORegistryEntryGetParentEntry(entry, kIOServicePlane, &parent) == kIOReturnSuccess) {
                uint32_t vendorId = plumeGetEntryProperty(parent, CFSTR("vendor-id"));
                IOObjectRelease(parent); // Release the parent
                IOObjectRelease(entry); // Release the entry
                return RenderDeviceVendor(vendorId);
            }
            IOObjectRelease(entry); // Release the entry if we couldn't get parent
        }

        return RenderDeviceVendor::UNKNOWN;
    #else
        return RenderDeviceVendor::APPLE;
    #endif
    }

#if TARGET_OS_OSX
    CGFloat getScaleFactor(NSWindow *nsWindow) {
#ifdef PLUME_APPLE_RETINA_ENABLED
        return [nsWindow backingScaleFactor];
#else
        return 1.0f;
#endif
    }
#else
    CGFloat getScaleFactor(UIWindow *uiWindow) {
#ifdef PLUME_APPLE_RETINA_ENABLED
        return [uiWindow screen].scale;
#else
        return 1.0f;
#endif
    }
#endif

    // MARK: - CocoaWindow

    CocoaWindow::CocoaWindow(void* window)
        : windowHandle(window), cachedRefreshRate(0) {
        cachedAttributes = {0, 0, 0, 0};

        if ([NSThread isMainThread]) {
        #if TARGET_OS_OSX
            NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
            NSRect contentFrame = [[nsWindow contentView] frame];
            CGFloat scaleFactor = getScaleFactor(nsWindow);
            NSScreen *screen = [nsWindow screen];
        #else
            UIWindow *uiWindow = (__bridge UIWindow *)windowHandle;
            CGRect contentFrame = uiWindow.bounds;
            CGFloat scaleFactor = getScaleFactor(uiWindow);
            UIScreen *screen = [uiWindow screen];
        #endif
            cachedAttributes.x = (int)round(contentFrame.origin.x);
            cachedAttributes.y = (int)round(contentFrame.origin.y);
            cachedAttributes.width = (int)round(contentFrame.size.width * scaleFactor);
            cachedAttributes.height = (int)round(contentFrame.size.height * scaleFactor);
            
            if (@available(macOS 12.0, iOS 10.3, *)) {
                cachedRefreshRate.store((int)[screen maximumFramesPerSecond]);
            }
        } else {
            updateWindowAttributesInternal(true);
            updateRefreshRateInternal(true);
        }
    }

    CocoaWindow::~CocoaWindow() {}

    void CocoaWindow::updateWindowAttributesInternal(bool forceSync) {
        auto updateBlock = ^{
        #if TARGET_OS_OSX   
            NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
            NSRect contentFrame = [[nsWindow contentView] frame];
            CGFloat scaleFactor = getScaleFactor(nsWindow);
        #else
            UIWindow *uiWindow = (__bridge UIWindow *)windowHandle;
            CGRect contentFrame = uiWindow.bounds;
            CGFloat scaleFactor = getScaleFactor(uiWindow);
        #endif
            
            std::lock_guard<std::mutex> lock(attributesMutex);
            cachedAttributes.x = (int)round(contentFrame.origin.x);
            cachedAttributes.y = (int)round(contentFrame.origin.y);
            cachedAttributes.width = (int)round(contentFrame.size.width * scaleFactor);
            cachedAttributes.height = (int)round(contentFrame.size.height * scaleFactor);
        };

        if (forceSync) {
            dispatch_sync(dispatch_get_main_queue(), updateBlock);
        } else {
            dispatch_async(dispatch_get_main_queue(), updateBlock);
        }
    }

    void CocoaWindow::updateRefreshRateInternal(bool forceSync) {
        auto updateBlock = ^{
        #if TARGET_OS_OSX
            NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
            NSScreen *screen = [nsWindow screen];
            if (@available(macOS 12.0, *)) {
                cachedRefreshRate.store((int)[screen maximumFramesPerSecond]);
            }
        #else
            UIWindow *uiWindow = (__bridge UIWindow *)windowHandle;
            UIScreen *screen = [uiWindow screen];
            if (@available(iOS 10.3, *)) {
            cachedRefreshRate.store((int)[screen maximumFramesPerSecond]);
            }
        #endif
        };

        if (forceSync) {
            dispatch_sync(dispatch_get_main_queue(), updateBlock);
        } else {
            dispatch_async(dispatch_get_main_queue(), updateBlock);
        }
    }

    void CocoaWindow::getWindowAttributes(CocoaWindowAttributes* attributes) const {
      if ([NSThread isMainThread]) {
        #if TARGET_OS_OSX
            NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
            NSRect contentFrame = [[nsWindow contentView] frame];
            CGFloat scaleFactor = getScaleFactor(nsWindow);
        #else
            UIWindow* nsWindow = (__bridge UIWindow *)windowHandle;
            CGRect contentFrame = nsWindow.bounds;
            CGFloat scaleFactor = getScaleFactor(nsWindow);
        #endif

            {
                std::lock_guard<std::mutex> lock(attributesMutex);
                const_cast<CocoaWindow*>(this)->cachedAttributes.x = (int)round(contentFrame.origin.x);
                const_cast<CocoaWindow*>(this)->cachedAttributes.y = (int)round(contentFrame.origin.y);
                const_cast<CocoaWindow*>(this)->cachedAttributes.width = (int)round(contentFrame.size.width * scaleFactor);
                const_cast<CocoaWindow*>(this)->cachedAttributes.height = (int)round(contentFrame.size.height * scaleFactor);

                *attributes = cachedAttributes;
            }
        } else {
            {
                std::lock_guard<std::mutex> lock(attributesMutex);
                *attributes = cachedAttributes;
            }

            const_cast<CocoaWindow*>(this)->updateWindowAttributesInternal(false);
        }
    }

    int CocoaWindow::getRefreshRate() const {
        if ([NSThread isMainThread]) {
        #if TARGET_OS_OSX
            NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
            NSScreen *screen = [nsWindow screen];

            if (@available(macOS 12.0, *)) {
                int freshRate = (int)[screen maximumFramesPerSecond];
                const_cast<CocoaWindow*>(this)->cachedRefreshRate.store(freshRate);
                return freshRate;
            }
        #else
            UIWindow *uiWindow = (__bridge UIWindow *)windowHandle;
            UIScreen *screen = [uiWindow screen];
            if (@available(iOS 10.3, *)) {
                int freshRate = (int)[screen maximumFramesPerSecond];
                const_cast<CocoaWindow*>(this)->cachedRefreshRate.store(freshRate);
                return freshRate;
            }
        #endif
            return cachedRefreshRate.load();
        } else {
            int rate = cachedRefreshRate.load();

            const_cast<CocoaWindow*>(this)->updateRefreshRateInternal(false);

            return rate;
        }
    }

    void CocoaWindow::toggleFullscreen() {
    #if TARGET_OS_OSX
        if ([NSThread isMainThread]) {
            NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
            [nsWindow toggleFullScreen:NULL];
        } else {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
                [nsWindow toggleFullScreen:NULL];
            });
        }
    #endif
    }
}
