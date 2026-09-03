# Spool for Jellyfin

- libmpv: We've forked this and made it compatible in the directory above. keep libmpv behind a thin PlayerController / PlaybackController facade and do not let Jellyfin/network/UI code know about mpv internals.
- Qt6.11
  HTTP / REST: QNetworkAccessManager as the base, QRestAccessManager on top, plus QNetworkRequestFactory for shared base URL / headers / auth boilerplate. Use Qt’s JSON types (QJsonDocument, QJsonObject, QJsonArray) end to end. In Qt 6.11, QRestAccessManager is the REST-focused wrapper over QNetworkAccessManager, and Qt OpenAPI generates Qt HTTP clients using Qt Network APIs such as QRestAccessManager.
  API generation: Qt OpenAPI, not generic OpenAPI Generator, since you want the result to stay natively Qt-shaped. I would use it to generate the baseline Jellyfin client, then put a thin handwritten JellyfinApi facade over it for auth, session reporting, and whatever server-version weirdness you hit. Jellyfin does expose OpenAPI/Swagger docs, but there are recent reports of schema issues, so a wrapper layer is still wise.
  Database: QSqlDatabase with QSQLITE. That is the right choice for local cache/state in a client like this. Qt’s SQLite driver is in-process and single-file, and QSqlDatabase connections must only be used from the thread they were created in, so do DB work on a dedicated DB thread or keep strict per-thread connections.
  Async layer: upstream QCoro 0.12 on top of the Qt async APIs, so network/database orchestration stays coroutine-based. Native builds use the nixpkgs package, while webOS builds a pinned ARM package against the target Qt prefix.
  UI stack: Qt Quick + Qt Quick Controls with the Basic style as the base. Qt documents Basic as simple, lightweight, and giving maximum performance, which is exactly what you want for a TV UI that you will heavily customize anyway.
  Models: C++ QAbstractListModel exposed to QML, not ad hoc QML data blobs. QAbstractListModel is the standard one-dimensional model base, and both GridView and ListView are designed to consume C++ models like that efficiently.
  10-foot / remote navigation: build around GridView / ListView + FocusScope + KeyNavigation + Keys. KeyNavigation is specifically for arrow/tab-based focus jumps, and FocusScope exists to keep reusable focus regions sane, which is exactly the problem space for D-pad TV UIs.
  HTTP asset caching: QNetworkDiskCache for posters, backdrops, and image responses. It is basic, but it plugs directly into QNetworkAccessManager; just remember it is basic by design and defaults to a 50 MB limit, so you will probably want to raise that.

## Android development

The Android toolchain is pinned to SDK 36, Build Tools 36.0.0 and NDK
27.2.12479018 by the flake, and to the Qt and FFmpeg versions in
`tools/manifests/toolchain.json`, which every platform reads. `nixpkgs` tracks `nixos-unstable`; the
headless emulator and its Google APIs x86_64 system image come from that
channel rather than nixpkgs master.

Build the emulator ABI locally, in one command:

```sh
nix develop .#android -c bash tools/android/build.sh
```

That runs the three cached stages -- `build-dependencies.sh`, `build-qt6.sh`,
`build-apks.sh` -- which can also be invoked on their own.

It builds both `spool-phone-x86_64.apk` and `spool-tv-x86_64.apk` under
`dist/android`, signed with a debug keystore generated on first use at
`build/android/debug.keystore` so they install. Build release-device APKs by
setting `ANDROID_ABI=arm64-v8a`. Launch-test both variants in the pinned headless emulator with:

```sh
nix develop .#android -c bash tools/android/emulator-launch-test.sh
```

# Name

## Fast Jellyfin client for LG TVs and desktop

// Download link box

// quick headline Features dot points

// links to below sections

// screenshots

// full feature list

// usage information

// technical information

// thank yous (kodi)
