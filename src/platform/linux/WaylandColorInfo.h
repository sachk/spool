#pragma once

class QWindow;

namespace JellyfinNative {

// What the compositor says the output can actually show.
//
// Nothing else on Linux knows. Qt's swapchain reports a fixed 1000 nits and
// 200 nits SDR white on every backend but Windows, and the desktops do not
// read panel luminance out of EDID either -- on KDE the number is one the
// viewer typed into the display settings. The colour-management protocol is
// where that number comes back out, so this asks for it rather than leaving
// tone mapping to a constant nobody chose.
struct WaylandColorInfo {
    bool valid = false;
    float minLuminanceNits = 0.0f;
    float maxLuminanceNits = 0.0f;
    // The luminance the compositor puts diffuse white at, which is what SDR
    // content and the interface are drawn against.
    float referenceLuminanceNits = 0.0f;
};

// Empty unless this is a Wayland session whose compositor implements
// wp_color_manager_v1 and whose output has an image description to describe.
WaylandColorInfo waylandColorInfo(QWindow *window);

} // namespace JellyfinNative
