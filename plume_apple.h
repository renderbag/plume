//
// plume
//
// Copyright (c) 2024 renderbag and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file for details.
//

#pragma once

#include <atomic>
#include <mutex>
#include "plume_render_interface_types.h"

namespace plume {
    RenderDeviceVendor getRenderDeviceVendor(uint64_t registryID);

    struct AppleWindowAttributes {
        int x, y;
        int width, height;
    };

    class AppleWindow {
        void* windowHandle;
        AppleWindowAttributes cachedAttributes;
        std::atomic<int> cachedRefreshRate;
        mutable std::mutex attributesMutex;

        void updateWindowAttributesInternal(bool forceSync = false);
        void updateRefreshRateInternal(bool forceSync = false);
    public:
        AppleWindow(void* window);
        ~AppleWindow();

        // Get cached window attributes, may trigger async update
        void getWindowAttributes(AppleWindowAttributes* attributes) const;

        // Get cached refresh rate, may trigger async update
        int getRefreshRate() const;

        // Toggle fullscreen
        void toggleFullscreen();
    };
}
