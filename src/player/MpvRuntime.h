#pragma once

// webOS-only lazy loader for libmpv.
//
// The app deliberately does not link libmpv (and therefore ffmpeg/lua) at
// build time: as DT_NEEDED entries they would be mapped, relocated and
// symbol-resolved by the dynamic linker before main() runs, which costs a
// large share of cold-launch time for ~40 MB of player code the home screen
// never touches (PERFORMANCE_PLAN.md §4A). Instead, MpvRuntime.cpp defines
// every mpv_*/starfish_* entry point the app uses and forwards through
// dlsym'd pointers; the library is dlopen'd on a background thread after the
// first frame, or on demand at the first mpv call, whichever comes first.
namespace JellyfinNative::MpvRuntime {

// Start loading libmpv on a detached background thread. Idempotent; call
// after the first frame has been presented.
void preloadAsync();

// Block until libmpv is loaded (performs the dlopen on the calling thread if
// the preload has not run yet). Returns false if loading failed; the failure
// is sticky and logged once.
bool ensureLoaded();

} // namespace JellyfinNative::MpvRuntime
