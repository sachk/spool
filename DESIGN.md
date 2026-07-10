# Design Overview

## Brief summary

Jellyfin Native is a C++/QML client for LG webOS TVs, with native Linux and
macOS development builds, that talks directly to a Jellyfin server. The C++
core owns durable state and I/O: a coroutine-based REST facade, focused
feature controllers, `QAbstractListModel` models, an SQLite worker, and the
custom libmpv/Starfish playback stack. Those objects are registered as
`JellyfinWebOS` singleton instances; QML does not depend on ambient context
properties.

The 10k-line QML layer is intentionally shallow. `AppShell` owns lazy routes
and overlays, `KeyRouter` is the only non-text key event boundary, pages expose
small zone-level input contracts, and reusable `MediaRow`, `MediaItemCard`,
`OverlayDialog`, and `MenuListView` components cover repeated UI structures.
Qt 6.11 compiles the module with qmlcachegen direct calls. The webOS build is
otherwise statically linked and packages the native app as an armv7 IPK.

## Libraries and toolchain in detail

### Qt 6.11 (core framework)

Statically linked on webOS; the module list is in `CMakeLists.txt:51-76`.

- **QtCore / QtGui / QtQuick / QtQml / QtQuickControls2 (Basic style)** — the
  UI stack. QML is compiled ahead-of-time via **qmlcachegen** with
  `QT_QMLCACHEGEN_DIRECT_CALLS`; the app loads the module through
  `loadFromModule`, so the generated compilation units are used. Controllers
  and services are registered singleton instances in the `JellyfinWebOS`
  module before the engine loads.
- **QtNetwork** — one shared `QNetworkAccessManager` with a
  `QNetworkDiskCache`. The API facade uses `QRestAccessManager`,
  `QNetworkRequestFactory`, and `QHttpHeaders`. Its request factory sets an
  explicit 15-minute connection-cache expiry so idle browse connections do not
  inherit Qt 6.11's shorter default. Retry and timeout policy remains a small
  constexpr class (`src/api/HttpRequestPolicy.h`).
- **QtWebSockets** — SyncPlay group session socket
  (`src/app/SyncPlayController.h:11`).
- **QtSql (SQLite driver)** — single `cache.sqlite` for auth session, device
  id, settings, discovered servers, home-payload cache and a generic
  namespaced byte cache (`src/cache/DatabaseManager.h`). Reads are exposed as
  `QCoro::Task<T>` methods that hop to the worker thread without blocking the
  GUI thread; startup initialization and shutdown are the only synchronous
  worker barriers.
- **webOS system keyboard** — a small statically linked platform input
  context speaks LG's compositor `text_model` protocol and lets
  `MaliitServer` render the stock TV keyboard; Qt Virtual Keyboard is not
  built or shipped.
- **QtWaylandClient + GuiPrivate** (webOS only) — wayland-egl QPA against
  LG's compositor, with a local qtwayland patch to suppress client cursor
  surfaces so the magic-remote pointer keeps working (`src/main.cpp:387-392`).
- **QtSvg, QtQuickLayouts** (webOS), **LinguistTools** (en_* translations,
  `CMakeLists.txt:287-292`).

### QCoro 6 (0.12, Core + Network)

C++20 coroutine bindings for Qt. Every API call in
`JellyfinApiFacade` is a `QCoro::Task<T>` that `co_await`s a
`QNetworkReply` (`src/api/JellyfinApiFacade.cpp:1609-1695`), including retry
loops with `QCoro::sleepFor`. Controller hot paths now use coroutines where
they need joins or database reads (`AppController::startPlayback`,
`HomeModelController::refreshAsync`, `DatabaseManager::*Async`); the
`src/common/AsyncTask.h` bridge remains for leaf fire-and-forget calls with
`RequestGeneration` latest-request-wins semantics.

### libmpv (custom fork, `mpv/`)

The playback engine. On webOS the fork adds a **Starfish** VO/VD/AO family:
video ES is fed to LG's `StarfishMediaAPIs` for hardware decode and
plane composition, audio goes through PCM-over-Starfish or ALSA, and the
Starfish clock is exported back to mpv as master. The app talks to mpv only
through the C client API (`mpv/client.h`); on webOS the library is **not**
linked but dlopen'd after first frame through a shim
(`src/player/MpvRuntime.cpp`), keeping ~40 MB of mapping/relocation off the
launch path. A version script trims its exports to `mpv_*`/`starfish_*`.

### FFmpeg (bundled shared libs)

Built demuxer/audio-decoder-focused (video decode is Starfish hardware);
consumed only by libmpv — the app links none of it. Tuned Thumb-2/cortex-a53
per the performance plan. **Lua 5.2** is bundled solely for mpv's scripting
(stats overlay).

### webOS platform libraries (system, 32-bit)

- **luna-service2 + webos-helpers** — LS2 bus calls: app lifecycle
  (`registerApp` relaunch/close), remote-keyboard registration, and
  memory-pressure subscription (`src/main.cpp:738-773`).
- **wayland-webos-client, EGL/GLESv2** — LG compositor extensions and GL.
- **glib** — required by the webOS stack (`CMakeLists.txt:86`).

### Assets & tooling

- **Material Icons font** (`qml/fonts/MaterialIcons-Regular.ttf`) rendered via
  a tiny `MaterialIcon` primitive — no icon-image assets.
- **Build**: CMake ≥3.22 + Ninja; meson for libmpv; buildroot GCC 14.2
  cross-toolchain (armv7, softfp); Nix flake for reproducible dev shells;
  `tools/` shell scripts for IPK/AppImage/DMG packaging.
- **Tests**: 21 plain-`main`/Qt test executables plus three qmltestrunner
  contracts (`RoutePolicy`, `KeyRouter`, and `PlayerOverlayInput`) are wired
  through CTest.

No embedded browser/jellyfin-web, no JS runtime beyond QML, no
openapi-generator client (a past attempt hit `cpp-qt6-client` generator bugs,
see `docs/codebase-audit.md:181`), no third-party JSON/serialization library.
DTO cache/API mapping uses a small in-tree Qt meta-object mapper
(`src/common/MetaJson.h`) over hand-written `Q_GADGET` value types. Bespoke
mapping remains only where Jellyfin's wire shape needs interpretation.
`MovieItem` stores raw ids, image tags, and media data; playability, labels,
and `image://artwork` URLs are derived at the use site rather than retained per
item.

## Refactored runtime boundaries

### Input and focus

- `qml/shell/KeyRouter.qml` owns press/release normalization, auto-repeat,
  accept long-press, back handling, and ignored player-key noise.
- `AppShell` selects exactly one route target. Pages navigate named focus
  zones; lists expose `moveSelection()`/`activate()` instead of installing
  competing `Keys` handlers.
- Text entry is the sole exception: `TextFieldRow` consumes editing keys and
  delegates keyboard display to LG's system IME.
- Flickable visibility uses Qt 6.11's `positionViewAtChild()` through
  `InputKeys.positionChild()`. No page carries its own scrolling algorithm.

### Reusable presentation

- `MediaRow` represents poster, landscape, library, and person rows.
  `MediaItemCard` handles the corresponding card layouts without forwarding
  copies of model properties.
- Player subtitles, audio tracks, queue entries, and playback settings share
  `OverlayDialog` + `MenuListView`; the complete player overlay is 1,433 QML
  lines.
- Item context, media information, management, and sync-play overlays use the
  same dialog/list primitives. `AppShell` owns their precedence and routes
  input only to the topmost visible layer.
- Search makes one mixed Movie/Series/Episode request and partitions the
  response into three local `MovieGridModel` sections. Episode cards reuse the
  series primary image while retaining the episode title and Sxx:Exx label.
- Home starts Continue Watching, Next Up, and every supported library's Latest
  request together. Completed library responses become data-driven `MediaRow`
  sections, including generic content types rather than a movie/TV-only set.
- `SettingsController` persists one integer UI-scale percentage. `Metrics`
  derives typography, controls, spacing, cards, grids, navigation, and overlay
  geometry from it; the first authenticated route is a three-preset showcase
  until its completion marker is stored.

### C++ ownership

- `AppController` is the 668-line application coordinator. Browse state,
  content/detail models, home data, settings, item-state mutation, library
  management, play queue, and sync play live in focused controllers.
- `MovieItem` is a raw value type. `ArtworkService` composes image requests on
  demand; `MetaJson` handles writable gadget properties and nested string
  lists; the API facade keeps only wire-format fixups.
- Database reads resume through Qt continuations on the GUI thread. Diagnostics
  use typed phase/task objects, and CPU topology comes from sysfs rather than
  subprocess parsing.

## Refactor measurements

Tracked C++ (`src/**/*.cpp`, `src/**/*.h`) changed from 20,038 to 19,430 lines.
Tracked QML/JS changed from 12,466 to 9,997 lines. Combined: 32,504 to 29,427,
a net deletion of 3,077 lines against the plan's 2,700-line floor.

The phase commits netted A −415, B −395, C −874, D −593, E −352, F −10,
G −193, and H −150 lines in their target trees. The per-phase estimates were
not forced when code moved to its correct owner: notably, F moved URL
composition into `ArtworkService` instead of deleting required behavior.
The aggregate floor still clears by 377 lines, while the explicit acceptance
ceilings hold (`AppController.cpp` 668; player QML 1,433).

The Qt 6.11 qmltc leaf experiment was rejected. Compiling only `AppText` while
continuing to instantiate the root with `loadFromModule` changed median
isolated login-page load from 93.5 ms to 95 ms (12 interleaved samples), added
528,248 bytes to the stripped executable, and added 46,781 bytes of runtime
sections. qmltc only avoids `QQmlComponent` construction when C++ directly
instantiates its generated class, so leaf-only compilation added cost without
serving the application's construction path. qmlcachegen direct mode remains
the measured choice.

The M2 post-diet playback log snapshot recorded 277 MiB RSS, 195 MiB anonymous,
and 580 MiB VmData on the target TV. Treat it as a point measurement, not an
attribution of the full process delta to `MovieItem`.
