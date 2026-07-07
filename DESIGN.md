# Design Overview

## Brief summary

Jellyfin Native is a C++/QML client for LG webOS TVs (with Linux/macOS dev
builds) that talks directly to a Jellyfin server — no embedded browser. A
~19k-line C++ core owns all state: a coroutine-based REST facade
(`JellyfinApiFacade`, QCoro + `QRestAccessManager`), a family of feature
controllers (browse, home, search, session, settings, sync-play, play queue)
aggregated behind one `AppController` context property, list models derived
from `QAbstractListModel`, an SQLite cache on a worker thread, and a player
layer that drives a custom libmpv fork whose video decodes through LG's
Starfish hardware pipeline (dlopen'd after first frame to keep launch fast).
An ~11k-line QML layer renders a fully remote-driven TV UI: a shell that
routes D-pad input, a route stack of lazily loaded pages, and hand-rolled
focus/navigation primitives tuned for the magic remote. Everything is
statically linked against Qt 6.11 where possible, AOT-compiled with
qmlcachegen, and packaged as an IPK for armv7 webOS.

## Libraries and toolchain in detail

### Qt 6.11 (core framework)

Statically linked on webOS; the module list is in `CMakeLists.txt:51-76`.

- **QtCore / QtGui / QtQuick / QtQml / QtQuickControls2 (Basic style)** — the
  UI stack. QML is compiled ahead-of-time via **qmlcachegen** with
  `QT_QMLCACHEGEN_DIRECT_CALLS` (`CMakeLists.txt:370-372`); the app loads the
  QML module through `loadFromModule` so the AOT units are actually used
  (`src/main.cpp:699`). Controllers are exposed as **context properties**
  (`src/main.cpp:684-692`), not registered QML types — see the refactor
  report; this is the main thing blocking `qmllint`/`pragma Strict`/qmltc.
- **QtNetwork** — one shared `QNetworkAccessManager` with a
  `QNetworkDiskCache` (`src/main.cpp:535-542`). The API facade uses the
  modern Qt 6.7+ HTTP trio: **`QRestAccessManager`**,
  **`QNetworkRequestFactory`** (base URL, common headers, transfer timeout)
  and **`QHttpHeaders`** (`src/api/JellyfinApiFacade.h:151-152`,
  `src/api/JellyfinApiFacade.cpp:429-440`). Retry/timeout policy is a small
  in-house constexpr class (`src/api/HttpRequestPolicy.h`).
- **QtWebSockets** — SyncPlay group session socket
  (`src/app/SyncPlayController.h:11`).
- **QtSql (SQLite driver)** — single `cache.sqlite` for auth session, device
  id, settings, discovered servers, home-payload cache and a generic
  namespaced byte cache (`src/cache/DatabaseManager.h`). All access is
  marshalled to a worker thread via `BlockingQueuedConnection`
  (`src/cache/DatabaseManager.cpp:325-339`).
- **QtVirtualKeyboard** — in-process IME on webOS because the LSM's native
  IME is unreliable for arbitrary native apps (`src/main.cpp:376-384`);
  supplies D-pad key navigation between keys. Slated for size/startup review
  in the performance plan.
- **QtWaylandClient + GuiPrivate** (webOS only) — wayland-egl QPA against
  LG's compositor, with a local qtwayland patch to suppress client cursor
  surfaces so the magic-remote pointer keeps working (`src/main.cpp:387-392`).
- **QtSvg, QtQuickLayouts** (webOS), **LinguistTools** (en_* translations,
  `CMakeLists.txt:287-292`).

### QCoro 6 (0.12, Core + Network)

C++20 coroutine bindings for Qt. Every API call in
`JellyfinApiFacade` is a `QCoro::Task<T>` that `co_await`s a
`QNetworkReply` (`src/api/JellyfinApiFacade.cpp:1609-1695`), including retry
loops with `QCoro::sleepFor`. Above the facade, controllers currently convert
tasks back into callback style through `src/common/AsyncTask.h`
(`runScoped` / `runLatest` + `RequestGeneration` tokens for
latest-request-wins semantics).

### libmpv (custom fork, `mpv_webos/`)

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
  `tools/` shell scripts for IPK/AppImage/DMG packaging;
  `tools/reduce_openapi.py` can slice the Jellyfin OpenAPI spec (currently
  unused by the build — candidate for DTO codegen).
- **Tests**: ~18 QtTest-less plain-`main` executables under `tests/` wired via
  CTest (`CMakeLists.txt:455-736`), plus one qmltestrunner test for
  `RoutePolicy.js`.

No embedded browser/jellyfin-web, no JS runtime beyond QML, no
openapi-generator client (a past attempt hit `cpp-qt6-client` generator bugs,
see `docs/codebase-audit.md:181`), no third-party JSON/serialization library.
DTO cache/API mapping uses a small in-tree Qt meta-object mapper
(`src/common/MetaJson.h`) over hand-written `Q_GADGET` value types; bespoke
code remains only where server fields need real interpretation (image URLs,
playability, browse-descriptor cache keys).
