# Evaluation follow-through plan

Status: **completed**, verified 2026-07-18
Source: `EVAL_REPORT.md`, continuously reconciled with the current working tree.
Completed work is recorded here so remaining phases do not repeat or undo it.

## Outcome

Ship one polished account/server-aware Jellyfin client across webOS and native
desktop with:

- an explicit account-and-server picker on every launch;
- a fast switch-user path beside Settings;
- a readable, progressively disclosed settings surface rather than a thicket
  of submenus;
- Jellyfin-compatible subtitle language/mode preferences plus mpv-native
  appearance controls, without server-burn-in controls that are counterproductive
  for this client;
- Kodi-style held-key acceleration and an alphabet indicator in large sorted
  libraries;
- remembered TLS trust decisions, recoverable caches, one safe codec fallback,
  secret-free playback URLs, correct screensaver behaviour, and CI that runs
  the tests it builds;
- explicitly LGPL-compliant, allowlist-only FFmpeg builds on every platform.

This plan is deliberately the only root-level implementation plan. Historical
plans and audit handoffs remain available in Git history.

## Executive decisions

1. **Do not upstream mpv work in this effort.** The gpu-next/libmpv work already
   has another upstream path. The threaded-curl change is too policy-specific,
   and the C++ Starfish backend is not a realistic short-term upstream unit.
   Keep the fork additive and test it, but do not spend this cycle preparing
   upstream patches.
2. **Profiles are account/server pairs, never free-floating users.** A profile
   identity is `(serverId or canonical server URL, userId)`. Its visible record
   contains user name, server name, canonical URL, stable tint/avatar seed, and
   token. Selecting a profile activates both halves atomically.
3. **Always show profile selection at launch when profiles exist.** Do not skip
   it because one token is valid, and do not revive the old “default profile”
   auto-login behaviour. A single saved profile still appears as one tile next
   to Add account. This is predictable on shared TVs and exactly matches the
   requested launch behaviour.
4. **Switch user is not logout.** Switching stops playback, disconnects the
   current session socket, clears user-scoped in-memory models, and returns to
   the profile picker without deleting any saved pair. “Remove account” is a
   separate confirmed action. A 401 marks only that profile as needing sign-in.
5. **Keep one Settings route.** Use a sticky three-state disclosure control:
   **Essential**, **More**, **All**. “More” contains Essential + Advanced;
   “All” adds Expert. There is one ordered schema and one list, not three copies
   and not a submenu per category. D-pad Up at the first row reaches the
   disclosure control; Down returns to the list. Persist the chosen level.
6. **Common settings lead; danger/debug settings trail.** Within the visible
   set: UI scale, subtitle mode/language, audio/video sync, streaming quality,
   and appearance come first. Diagnostics, renderer internals, caches, and
   button remapping are Expert. WebOS-only choices do not appear on desktop;
   desktop-only choices use native-feeling mouse/keyboard affordances while
   retaining the same visual language.
7. **mpv owns subtitle rendering.** Retain Jellyfin’s user-level language and
   mode semantics, but remove/hide Burn subtitles, Render PGS, and Always burn
   in. Advertise the formats mpv/libass can render and ask the server to burn
   only when playback negotiation proves no local path exists. No normal UI
   should encourage server rasterisation.
8. **TLS trust is per server and certificate, not a global “ignore TLS” switch.**
   The user sees host, error, issuer, and SHA-256 fingerprint and can Cancel,
   Trust once, or Remember for this server. A remembered exception matches the
   canonical authority plus certificate fingerprint; a changed certificate
   prompts again. Hostname mismatch is shown explicitly and never silently
   broadened to another host.
9. **Codec retry is once, early, and visible.** Retry only a failed direct-play
   start before meaningful playback progress, renegotiate the same item with
   `EnableDirectPlay=false`, preserve resume position/queue/selected tracks
   where valid, and show “Trying a compatible stream…”. Never loop and never
   hide failures after the fallback also fails.
10. **Screensavers are inhibited only while media is actively advancing.** A
    playing video, music item, or slideshow vetoes the webOS saver. Paused or
    stopped playback releases the veto, so a paused screen can time out
    normally. Resume reacquires it. Desktop uses the platform’s supported
    idle-inhibit mechanism when available.
11. **LGPL is a hard build gate.** Every FFmpeg configuration must state
    `--disable-everything --disable-gpl --disable-version3 --disable-nonfree`
    before an explicit allowlist. If verification shows that a critical client
    feature requires GPL/nonfree FFmpeg code, stop rather than silently ship a
    degraded or non-compliant build.
12. **Do not deploy to or launch on a TV in this effort.** Validate webOS by
    configuration/build metadata and targeted tests only. End with one minimal
    local foreground launch (`timeout 10s nix run`) and inspect its output to
    confirm the profile/home route renders without QML/runtime failure.
13. **Platform code is selected at build time, not branched through the shared
    application core.** Platform APIs belong under `src/platform/<platform>/`;
    shared code consumes narrow compile-selected functions or components. Do not
    add a runtime service locator, inheritance hierarchy, or duplicate QML app.
    A platform folder is for OS integration, not every policy with a platform
    parameter.

## Corrections to the evaluation snapshot

### Implemented; verify and retain

- **Account pairs and switching:** `SessionController` persists an
  account/server-pair array, `LoginPage.qml` renders profile tiles, and TopBar
  exposes Switch user. Remaining profile work is listed below; do not recreate
  a second profile store.
- **Settings interaction and visual system:** the Essential/Advanced/Expert
  disclosure flow, compact schema rows, `SelectRow`, sliders, toggles, anchored
  option picker, focus repair, and popup scroll accommodation are present.
  Remaining work is schema ownership and semantic cleanup, not another visual
  rewrite.
- **Large-library navigation:** held-key acceleration, release handling, the
  visible/drag-capable scrollbar, and transient current-letter feedback exist
  with focused QML tests. Retain the shared navigation path.
- **Session WebSocket and remote control:** `SyncPlayController` owns the
  authenticated socket and routes commands through `AppController`. Remaining
  work is focused protocol/profile-switch coverage, not a second socket.
- **TLS-enabled target Qt:** the webOS Qt build now links the pinned OpenSSL
  backend, `TlsTrust.h` enforces `QT_CONFIG(ssl)`, and discovery/API/WebSocket
  traffic reuses remembered certificate fingerprints. The trust decision UX is
  still incomplete.
- **Playback privacy and fallback:** playback URLs no longer append `api_key`;
  authorization is carried in headers and redacted from diagnostics. A failed
  early direct-play start has a one-shot server-stream fallback.
- **Screensaver integration:** active playback inhibition exists for webOS,
  Windows, and freedesktop desktops, and paused playback releases it. It is
  currently embedded in `main.cpp` and must move behind platform ownership.
- **Build hardening:** Thumb-2 target flags, staged shared-library stripping,
  slim LGPL FFmpeg configuration checks, native `ctest`, strict compiled-QML
  lint, and host startup smoke steps are present.
- **Platform-correct audio output choices:** Linux exposes Automatic/PipeWire/
  PulseAudio/ALSA, Windows Automatic/WASAPI, macOS Automatic/CoreAudio, and
  webOS only ALSA/Starfish. Desktop Automatic leaves mpv output probing unset.
- **Server discovery/manual probe and input routing:** retain the real server
  probe and `KeyRouter` as the single QML input boundary.

### Confirmed remaining

- One valid saved profile is still auto-activated in
  `SessionController::initializeAsync()`, contrary to the always-show-picker
  product decision. Profiles still share the legacy key/value store and lack
  the complete remove/re-authenticate lifecycle and coverage.
- `SettingSpec` still has no disclosure-level, platform, icon, or dependency
  metadata. `SettingsPage.qml` manually classifies rows and contains platform
  checks that belong in the schema/capability layer.
- Platform integration remains concentrated in the 1,384-line `src/main.cpp`,
  `src/app/NativeAppWindow*`, `PlayerController`, `SettingsController`,
  `SettingsSchema`, and `JellyfinTypes`. The next structural phase is the
  explicit source split below.
- Subtitle burn/PGS/transcoding-policy settings are hidden but their storage,
  controller, defaults, and tests remain. Remove those paths cleanly.
- The certificate prompt offers one remember-and-retry action. It still needs
  error/issuer/host detail, Cancel, Trust once, certificate-change handling,
  and an explicit management/removal surface.
- A future SQLite `user_version` still fails initialization. Durable
  profiles/preferences and disposable cache recovery are not separated.
- FFmpeg compliance is enforced for the built native configuration, but the
  webOS/Linux/macOS/Windows build inputs still need one reviewed capability
  manifest rather than parallel allowlists and package-specific switches.
- Platform-specific behavior lacks dedicated tests for LS2 lifecycle,
  screensaver subscription/acknowledgement, webOS audio-route updates, and
  compile-selected desktop implementations.

## Phase 0 — Baseline, ownership, and safety

1. Record the existing dirty worktree and touch only files required by this
   plan. Stage explicit paths for commits so unrelated in-progress work is not
   swept in.
2. Run the existing fast unit/QML tests before structural edits. If the current
   dirty tree does not build, identify whether the failure predates this work
   and fix only an in-scope dependency.
3. Keep `EVAL_REPORT.md` as the immutable source evaluation and this file as the
   live checklist. Delete root historical plans/refactor notes; do not archive
   copies elsewhere.
4. Use small conventional commits at coherent boundaries. Never push.

Acceptance:

- root planning/document inventory is `AGENTS.md`, `DESIGN.md`,
  `EVAL_REPORT.md`, `README.md`, and `REPORT_PLAN.md` only;
- the baseline command/result is recorded in the final handoff;
- no TV command is run.

## Phase 0A — Platform source separation

Status: **implemented and verified**.

### Boundary rules

1. `src/main.cpp` is composition only: parse process-wide arguments, create the
   compile-selected platform objects, construct shared controllers, register
   QML types, load the root component, and run the event loop.
2. Headers included by shared code must not expose LS2, ALSA, Wayland-webOS,
   Win32, DBus, or Starfish types. Platform handles stay private to their
   implementation file or platform-owned PIMPL.
3. CMake selects exactly one implementation for each platform contract. Do not
   compile every backend and select one with runtime `if` statements.
4. Prefer matching free functions or concrete classes with one implementation
   per target. Do not introduce factories, service locators, abstract base
   classes, or virtual calls where link-time selection is sufficient.
5. Keep product policy shared when it is independent of OS APIs and can be
   tested as pure data. Move device APIs, paths, environment setup, lifecycle,
   window/surface ownership, and backend availability.
6. `JELLYFIN_NATIVE_WEBOS` and `Q_OS_*` are allowed in platform CMake/source
   selection and narrowly in platform tests. They should disappear from
   `main.cpp`, app/session/settings controllers, and common value utilities.

### Target layout

```text
src/platform/
  PlatformCapabilities.h
  PlatformPaths.h
  PlatformStartup.h
  ScreenSaverInhibitor.h
  MpvConfigPolicy.h
  NativeAppWindow.h
  common/
    NativeAppWindowCommon.cpp
  desktop/
    DesktopNativeAppWindow.cpp
    DesktopPlaybackSurface.cpp
    DesktopMpvConfigPolicy.cpp
  linux/
    LinuxPlatformCapabilities.cpp
    LinuxPlatformPaths.cpp
    LinuxPlatformStartup.cpp
    LinuxScreenSaverInhibitor.cpp
  macos/
    MacOSPlatformCapabilities.cpp
    MacOSPlatformPaths.cpp
    MacOSPlatformStartup.cpp
    MacOSScreenSaverInhibitor.cpp
  windows/
    WindowsPlatformCapabilities.cpp
    WindowsPlatformPaths.cpp
    WindowsPlatformStartup.cpp
    WindowsScreenSaverInhibitor.cpp
  webos/
    WebOSPlatformCapabilities.cpp
    WebOSPlatformPaths.cpp
    WebOSPlatformStartup.cpp
    WebOSNativeAppWindow.cpp
    WebOSApplicationServices.cpp
    WebOSApplicationServices.h
    WebOSScreenSaverInhibitor.cpp
    WebOSAudioRoute.cpp
    WebOSAudioRoute.h
    WebOSAudioSyncPolicy.cpp
    WebOSAudioSyncPolicy.h
    WebOSMpvRuntime.cpp
    WebOSMpvRuntime.h
    WebOSPlaybackSurface.cpp
    input/
    protocol/
```

The existing webOS input-context files and protocol tables move only one level
deeper to `src/platform/webos/input/`; their firmware-derived wire definitions
remain unchanged. Names may be shortened during implementation, but ownership
and dependencies must match this tree.

### Required moves

| Current location | Target owner | Required result |
| --- | --- | --- |
| `src/app/NativeAppWindow.h` | `src/platform/NativeAppWindow.h` | Shared Qt-facing window API contains no Wayland-webOS declarations or members. |
| `src/app/NativeAppWindowCommon.cpp` | `src/platform/common/NativeAppWindowCommon.cpp` | Input-latency wrapping, close handling, and platform-neutral overlay image publication remain common. Platform-surface and webOS key diagnostics leave this file. |
| `src/app/HostNativeAppWindow.cpp` | `src/platform/desktop/DesktopNativeAppWindow.cpp` | Shared desktop window behavior remains one implementation for Linux/macOS/Windows. |
| `src/app/NativeAppWindow.cpp` | `src/platform/webos/WebOSNativeAppWindow.cpp` | All Wayland-webOS shell/foreign/exported-surface, crop, overlay, fullscreen, key-mask, and exposure code is physically webOS-only. |
| `src/player/MpvRuntime.*` | `src/platform/webos/WebOSMpvRuntime.*` | The `dlopen` table, Starfish codec probe, overlay callback replay, and webOS-only CPU-time bridge no longer appear to be generic player runtime. |
| hard-coded `config=no` startup in `MpvOptionProfile.cpp` | shared `MpvConfigPolicy` value plus `DesktopMpvConfigPolicy.cpp` | Desktop Expert settings select disabled, native mpv config discovery, or an explicit directory. webOS remains fixed disabled. The shared player applies the selected pre-initialization policy without learning desktop path conventions. |
| `src/player/AudioSyncPolicy.*` | `src/platform/webos/WebOSAudioSyncPolicy.*` | LG output names, ALSA control-derived offsets, and `settings/webosAudioDelayMs/*` keys are explicitly webOS policy. |
| `ScreenSaverInhibitor` inside `src/main.cpp` | `src/platform/ScreenSaverInhibitor.h` plus per-platform implementations | webOS LS2 subscription/acknowledgement, Linux freedesktop DBus cookies, Windows execution state, and macOS capability/no-op behavior compile independently and have focused tests. |
| webOS LS2 callbacks/globals in `src/main.cpp` | `src/platform/webos/WebOSApplicationServices.*` | One RAII owner holds registerApp, remote keyboard, memory-status, and sound-output subscriptions; it marshals typed signals to the GUI thread and unregisters on destruction. No global controller/window pointers remain. |
| ALSA control reads and sound-output polling in `src/main.cpp` | `src/platform/webos/WebOSAudioRoute.*` | Route name and settled latency are emitted as one typed snapshot; polling/generation/mutex state is private and `SettingsController` receives a platform-neutral update. |
| webOS environment/plugin/surface-format setup in `src/main.cpp` | `src/platform/webos/WebOSPlatformStartup.cpp` | `APPID`, Wayland/QPA/QML paths, input module, render loop, cursor policy, static plugin imports, and GLES format are configured before `QGuiApplication`. |
| Linux Wayland default and OS-specific executable/data/cache/log paths in `src/main.cpp` | `PlatformStartup`/`PlatformPaths` implementations | `resolveAppRoot`, cache/data/log roots, platform environment, and executable lookup use one compile-selected source per OS; shared startup cache creation remains common. |
| Linux/Windows/macOS screensaver, signal, and path headers in `src/main.cpp` | respective platform files | `main.cpp` no longer includes DBus, Win32, LS2, ALSA, Wayland, Mach-O, or POSIX process headers. A small shared Unix signal helper may remain under `src/platform/common/`. |
| hard-coded device identity in `JellyfinApiFacade.h` and `main.cpp` | `PlatformCapabilities` | Device name and flags come from one immutable C++ capability record; desktop is not hard-coded as “Linux Wayland” on macOS/Windows. |
| audio choices in `SettingsSchema.cpp` and normalization in `JellyfinTypes.cpp` | compile-selected platform audio-output policy | Linux PipeWire/PulseAudio/ALSA, Windows WASAPI, macOS CoreAudio, and webOS ALSA/Starfish lists/defaults are not selected in shared files with preprocessor branches. |
| webOS audio-delay methods/state in `SettingsController.*` | `WebOSAudioRoute`/`WebOSAudioSyncPolicy` integration | Shared settings code stores/applies a supplied effective correction; it does not know LS2 output names, display latency, or webOS storage keys. |
| platform blocks in `PlayerController.cpp` | platform playback helpers or target-specific `PlayerController` translation units | Starfish window properties, lazy preparation/background behavior, and subtitle preload live under webOS; `MpvVideoItem` render-context attachment/release lives under desktop. Shared queue/session/event logic stays in `PlayerController`. |
| OS probes inside `CpuTopology.cpp`, `MemoryBudget.cpp`, and `SystemPerformanceMonitor.cpp` | small per-OS probe files | Shared budgeting/performance math consumes probe results. `/proc`, `sysctl`, and Win32 API reads do not share one conditional-heavy translation unit. This is lower priority than extracting `main.cpp` and playback. |

Do **not** move `PlaybackNegotiation`, `MpvLifecycle`, `PlaybackReporter`,
`PlayQueueController`, session/profile logic, API request logic, or general
settings semantics merely because they consume capabilities. `MpvOptionProfile`
also remains a shared pure policy surface; platform-specific option appenders
may live below `src/platform/` only if they remain deterministic and directly
unit-testable.

### CMake ownership

Split the monolithic platform blocks out of `CMakeLists.txt`:

```text
cmake/platform/Common.cmake
cmake/platform/Desktop.cmake
cmake/platform/Linux.cmake
cmake/platform/MacOS.cmake
cmake/platform/Windows.cmake
cmake/platform/WebOS.cmake
```

Top-level CMake retains project options, shared dependencies/targets, QML
module declaration, tests, and install rules. The platform modules own source
selection, OS packages/libraries, compile definitions, static Qt plugin imports,
and platform link/rpath settings. `WebOS.cmake` must keep the current explicit
sysroot lookup behavior and input-context protocol warning. Existing supported
build entrypoints (`build-ipk.sh`, `tools/build-linux-release.sh`,
`tools/build-macos.sh`, and `tools/windows/*.ps1`) remain stable.

### QML boundary

- Keep one shared `qml/pages`, `qml/shell`, and `qml/primitives` tree. Do not
  clone Settings, Player, or Login pages per platform.
- Replace the duplicated `NativeWindow.smartTvPlatform` and
  `Platform.isWebOS` decisions with one immutable capability singleton
  (`isTV`, `isWebOS`, `hasSystemFonts`, `hasDesktopPointer`, renderer/audio
  capabilities).
- Platform-invalid settings are removed by C++ schema/capability filtering
  before QML receives rows. `SettingsPage.qml` must not know PipeWire, WASAPI,
  CoreAudio, Starfish, or webOS audio-delay storage.
- Create `qml/platform/<platform>/` only for a genuinely different visual or
  input implementation. Small spacing, visibility, and focus-policy differences
  remain capability-driven shared components.

### Cross-platform pointer navigation

Handle physical mouse side buttons once in the common Qt window/input boundary,
not in Linux/Windows-only code:

1. `NativeAppWindowCommon::event()` recognizes `QMouseEvent` press/release for
   `Qt::BackButton` and `Qt::ForwardButton` on every target, including webOS USB
   mice. Ignore synthesized touch events, perform one action on press, consume
   the matching release, and do not let an unhandled side button close the app.
2. Emit a platform-neutral pointer-navigation signal into `KeyRouter`.
   Mouse Back follows exactly the existing modal/text-input/player/page/shell
   Back precedence; do not call `Router.pop()` underneath an open dialog.
3. Add a session-local forward stack to `RouterController` with `canForward`
   and `forward()`. A successful Back/pop pushes the current frame forward;
   Forward restores it and pushes the current frame onto the back stack. Any
   new push/replace/reset, account switch, or recovery reset clears forward
   history.
4. `AppShell.forward()` is inert while a modal, text editor, or player teardown
   owns navigation; otherwise it restores the next router frame and focus.
   Remote-control Back behavior remains unchanged.

Tests inject physical and synthesized `QMouseEvent`s through the common window,
verify one action per click on Linux/macOS/Windows/webOS source selections, and
cover Back → Forward round trips, modal precedence, forward-history invalidation,
empty history, and route-argument/focus restoration.

### Migration order

1. Add compile-selected `PlatformCapabilities`, `PlatformPaths`, and
   `PlatformStartup`; move path/environment/device-name code without changing
   behavior.
2. Move the window implementations and existing webOS input context; update
   includes/CMake, then build desktop and configure webOS.
3. Extract `ScreenSaverInhibitor` and `WebOSApplicationServices`; remove LS2,
   DBus, Win32, ALSA, and Wayland headers/globals from `main.cpp`.
4. Move `WebOSMpvRuntime`, audio route/sync policy, and platform playback
   branches. Keep `PlayerController` public behavior unchanged.
5. Move audio-output availability/defaults and settings capability filtering
   out of shared schema/value utilities.
6. Split lower-risk CPU/memory/performance probes and the platform CMake files.
7. Only after all targets build, remove obsolete macros, source entries, and
   old paths in one clean cutover; no forwarding headers or aliases.

### Platform-separation tests and acceptance

- each platform source set has a compile/link smoke target that catches missing
  implementations without requiring that OS at runtime where cross-compilation
  is available;
- pure tests cover capability values, path policy, audio-output lists/defaults,
  and mpv option output for Linux/macOS/Windows/webOS;
- mocked platform-bound tests cover screensaver state transitions and webOS LS2
  lifecycle/audio payload handling without contacting a TV;
- desktop build, focused tests, strict QML lint, and visible `nix run` pass;
- webOS fresh build/configuration passes only when explicitly performed under
  the existing no-deploy rule;
- `src/main.cpp` contains no `JELLYFIN_NATIVE_WEBOS`, `Q_OS_*`, LS2, ALSA,
  DBus, Win32, Wayland, Starfish, or platform path branches;
- `SettingsController`, `SettingsSchema`, `JellyfinTypes`, and shared QML contain
  no platform-specific backend names;
- `src/platform/webos/` is the only app source directory that includes LG
  service, Wayland-webOS, Starfish, or direct ALSA-control headers;
- no behavior change is accepted solely because files moved: startup, playback,
  foreground/background, input, TLS, audio sync, and screensaver behavior retain
  focused verification.

## Phase 1 — Account/server profile model and persistence
Status: **implemented and verified**.

### Data contract

Promote the current private `SessionController::AccountProfile` into a compact
value type with:

- `profileId`: stable hash/UUID, never a list index;
- `serverId`, `serverName`, canonical `serverUrl`;
- `userId`, `userName`, `accessToken`;
- `avatarTag` or deterministic colour seed;
- `lastUsedAt` and `needsAuthentication`.

The current `profiles/accounts-v1` JSON array is a working intermediate store.
Move it into a dedicated small `profiles` table if that makes atomic
upsert/delete/select clearer; avoid a repository/service layer. Remove the
one-time legacy `login/*` adoption path after the clean cutover. Because the app
is unreleased, bump/reset schema data rather than introduce migration/fallback
readers. Settings remain device-wide unless explicitly Jellyfin user
configuration. Home/artwork caches remain keyed by both server and user.

Required database operations:

- load all profiles ordered by most recently used, then insertion order;
- upsert after password or Quick Connect authentication using pair identity;
- activate by stable `profileId` in one worker transaction/read;
- mark one profile expired without clearing siblings;
- remove one pair with confirmation;
- clear all only from an explicit destructive action.

Never log or expose tokens through QML. The QML model gets display fields and
state only; activation passes `profileId` back to C++.

### Session lifecycle

1. `SessionController` owns the active pair and exposes the active profile ID,
   server name, and display label (`user · server`).
2. Startup loads profile summaries but does not activate a token. Initial route
   is Scale setup on a fresh install, then Profile picker if profiles exist,
   otherwise Add account.
3. Selecting a valid profile atomically sets API server URL + auth session,
   reconnects WebSocket, updates artwork headers, loads libraries, and routes
   Home. A failed/401 token keeps the tile and opens sign-in for that exact
   server/user with an “Authentication required” status.
4. Successful sign-in/Quick Connect upserts the pair using the server’s real
   name and ID from discovery/public info, then activates it.
5. Switch user disconnects the socket before clearing models so push events
   cannot race into the next profile.
6. Logout means “sign out of this account”: clear that pair’s token and mark it
   as requiring authentication; “Remove from this device” deletes the tile.

### Profile picker UI

Retain the existing responsive horizontal `ProfileTile` shelf in
`LoginPage.qml` and finish its pair lifecycle:

- heading: **Who’s watching?**; subtitle explains that each tile is a Jellyfin
  account on a specific server;
- tile: large deterministic initial/avatar disc, user name, server name, and a
  smaller elided server host/address; offline/auth-required badge where needed;
- trailing Add account tile with the same geometry;
- focused tile gains a crisp accent outline, subtle raised fill/scale, and a
  high-contrast bottom marker; unfocused tiles never rely on faint outlines;
- add the remaining context/long-press actions: Sign in again, Edit server
  label/address, and Remove from this device with confirmation;
- retain deterministic left/right movement, desktop click, focused-tile
  visibility, and the trailing Add account tile;
- add offline/auth-required status treatment and a compact position indicator
  when enough pairs require horizontal scrolling.

Retain the existing two-step Add account screen: server first, credentials or
Quick Connect second. Improve it by carrying the selected server name/address
visibly through sign-in and by returning to the pair picker on Back.

### Top-bar action

Retain the existing `person` Switch user icon immediately before the Settings
cog. Add the desktop tooltip `Switch user — <user · server>` and keep TopBar’s
explicit focus indexing deterministic with SyncPlay at the far right. Keep the
Settings action inside Settings as a secondary path.

Tests:

- profile serialization/upsert pair identity/removal/order;
- two users on one server, one user on two servers, same server under canonical
  URL variants;
- expired profile does not erase siblings;
- activation swaps URL and token atomically;
- startup route with zero/one/many profiles;
- switch-user disconnect/reset ordering;
- QML focus across profile tiles, Add, and top-bar Switch user/Settings.

## Phase 2 — Settings information architecture and visual system
Status: **implemented and verified**.

### Schema changes

Extend each `SettingSpec` with:

- `level`: Essential, Advanced, Expert;
- `platforms`: all, webOS, desktop (or a small flags field);
- `icon` and optional short value summary;
- optional `dependsOn` predicate for hiding irrelevant subordinate controls;
- richer help text that states effect and restart/next-playback scope.

Do not create a parallel QML list. C++ emits one ordered schema; QML filters it
by platform, current disclosure level, dependencies, and subtitle-editor mode.
Schema tests require unique keys, monotonic levels within groups, meaningful
descriptions for non-obvious controls, valid defaults, and no unreachable
dependent setting.

### Order

Default Essential view:

1. Account: active `user · server`, Switch user, Sign out;
2. Appearance: UI scale, accent, reduced motion;
3. Subtitles: preferred language, subtitle mode, open appearance editor;
4. Playback: streaming quality policy, night mode, A/V sync, player volume on
   desktop;
5. About.

Advanced adds remux/local bitrate/cache/audio output/subtitle fine controls and
desktop text/render options. Expert adds diagnostics, latency instrumentation,
tone-map visualisation, button remapping, desktop mpv configuration, and
destructive cache actions.
Platform-invalid settings disappear entirely rather than displaying disabled
noise.

### Directional disclosure control

Retain the implemented **Essential · More · All** `ChoiceStrip` under the page
title and its single filtered list:

- preserve Left/Right level changes and Up/Down transitions between the strip,
  current valid row, and TopBar;
- move level/platform/dependency metadata into `SettingSpec`, then remove the
  manual QML level/key maps without changing row identity or focus recovery;
- persist level, default new installs to Essential, and keep the no-sidebar,
  no-duplicate-route contract.

### Rows and popups

Retain the shared settings card-list and repaired `SelectRow`/`ChoiceStrip`
input path:

- preserve scaled 60–72 px rows, stable icon/value columns, wrapping help text,
  raised group surfaces, and unclipped inside focus treatment;
- preserve pill toggles, current-value select rows, visible sliders, and
  deterministic keyboard/mouse activation;
- keep option pickers anchor-aware on desktop and centred on webOS, including
  temporary bottom scroll space when a popup would otherwise overflow;
- preserve quieter hover, deterministic D-pad focus, working mouse wheel, and
  red separation for destructive actions;
- finish replacing hard-coded 42/44 px toolbar controls only where they still
  conflict with UI scale.

### Desktop Expert mpv configuration

Yes, libmpv can load ordinary mpv configuration: its client API requires
`config` and optional `config-dir` to be set before `mpv_initialize()`. The
current app deliberately sets `config=no`; replace that desktop behavior with
two device-wide Expert settings:

- `playback/mpvConfigMode`: **Disabled** (default), **Standard mpv directory**,
  or **Custom directory**;
- `playback/mpvConfigDirectory`: visible only for Custom, with direct
  edit/paste and a desktop folder chooser.

Standard mode sets `config=yes` and leaves `config-dir` unset so libmpv uses its
normal platform search path (`~/.config/mpv` on typical Linux desktops and the
documented native equivalent elsewhere). Custom mode requires a non-empty,
absolute, readable directory, canonicalizes it in C++, sets `config-dir` to it,
then enables config. Invalid custom paths block applying the value and leave the
last valid mode active. webOS exposes neither row and always uses `config=no`.

Code ownership:

- `SettingSpec` owns the Expert/desktop/dependency metadata; QML contains no OS
  or path-selection logic.
- `MpvConfigPolicy` is a small immutable shared value (`Disabled`, `Standard`,
  `Custom` plus canonical directory). `DesktopMpvConfigPolicy.cpp` validates the
  custom directory; the standard path stays delegated to libmpv rather than
  being duplicated in Qt code.
- `SettingsController` persists the two device-wide values and sends policy
  changes to `PlayerController`. Do not interrupt active playback: destroy an
  inactive/idle handle immediately, mark an active handle stale, and recreate it
  with the new policy after stop for the next playback.
- Split `MpvOptionProfile::startupOptions()` into pre-initialize config options
  and post-config application options. `PlayerController` sets config mode/path,
  calls `mpv_initialize()`, then reapplies embedding/security invariants before
  creating the render context or loading media.

User config may control ordinary mpv playback, shader, profile, script, and
script-option behavior. Tern-owned invariants win afterward: desktop
`vo=libmpv`, QML input ownership, no terminal/OSC/standalone window, lifecycle,
selected app audio output, per-session authorization headers, and token/log
redaction. The Expert help text must state that CLI `input.conf` bindings may
not receive keys because `KeyRouter` owns application input, and that arbitrary
mpv scripts/config can change or break playback.

Tests cover Disabled/Standard/Custom option ordering, native mode leaving
`config-dir` unset, custom canonicalization/rejection, config values taking
effect, app invariants overriding conflicting config, next-playback handle
recreation, desktop-only schema visibility, webOS forced-disabled behavior, and
paths/config contents remaining absent from privacy-sensitive diagnostics.

Tests include schema filtering, focus preservation while levels change,
conditional settings, select/toggle/slider operation, popup dismissal, and
desktop/webOS visibility.

## Phase 3 — Subtitle semantics, appearance, and preview
Status: **implemented and verified**.

### Keep Jellyfin semantics, explain them plainly

Use the same user configuration values as jellyfin-web:

- **Default:** use embedded Default/Forced flags; preferred language breaks
  ties;
- **Smart:** load a subtitle matching the preferred language when the selected
  audio is in another language;
- **Only forced:** load only tracks marked Forced;
- **Always:** load the preferred subtitle language regardless of audio language;
- **None:** do not select subtitles automatically.

Put the applicable explanation directly beneath the mode row and in the option
picker. Persist language/mode through Jellyfin user configuration so the same
account behaves consistently on other clients; keep device-rendering appearance
local to this client.

### Remove server-centric controls

Remove from the visible schema and public Settings API:

- `subtitles/burnIn`;
- `subtitles/renderPgs`;
- `subtitles/alwaysBurnInWhenTranscoding`.

Because the app is unreleased, remove obsolete storage/default/test paths
instead of retaining hidden compatibility state. Playback negotiation should
advertise local text, ASS/SSA/libass, PGS, VobSub/DVD, and supported external
subtitle delivery. Only an unavoidable server transcode path may burn subtitles;
there is no user-facing burn-in policy.

### Appearance editor

Keep and polish the existing route rather than add submenus. At its top, add a
16:9 live preview card with bright and dark imagery bands, safe-area guides,
two lines of sample dialogue, and an HDR paper-white badge when enabled.
Updates apply immediately to the preview and active playback where mpv supports
runtime changes.

Controls:

- style policy renamed for mpv clarity: **Respect embedded styles** vs
  **Override with my style** (Auto may choose respect for ASS and override
  plain text); explain that bitmap subtitles can scale/smooth but cannot change
  font/colour;
- text size preset plus overall percent scale;
- font (bundled serif/sans choices; system font picker only on desktop);
- weight, colour swatches with names, outline/shadow style, background;
- vertical position labelled Bottom ↔ Top instead of an unexplained raw number;
- bitmap smoothing with Sharp/Smooth/Extra smooth examples;
- HDR subtitle dimming and brightness shown only when relevant, with “paper
  white” explained as a comfortable SDR-like white level inside HDR video;
- Reset appearance action with confirmation/undo toast.

Map all settings through `MpvOptionProfile` and extend tests for normal text,
ASS override/no-override, bitmap scaling, invalid colours/fonts, HDR dimming,
and live reapplication without changing selected track.

Upstream reference pinned during planning:
`jellyfin-web` commit `7b5580a09fb9e45f3188e113cf4fee7063e32e3d`,
`src/components/subtitlesettings/` and `src/scripts/settings/userSettings.js`.
Copy semantics and useful labels; do not copy browser-only Native/Custom
mechanisms that have no meaning for libmpv.

## Phase 4 — Kodi-style large-library navigation
Status: **implemented and verified**.

### Acceleration policy

Retain the Kodi-derived constants and curve already implemented in the shared
navigation primitives:

- acceleration starts after 100 ms of held directional input;
- it reaches full rate at 3000 ms, from at least 10 items/s to at least
  30 items/s or `itemCount / 7`;
- fractional movement uses elapsed time with a 50 ms per-frame cap;
- release, direction/focus-zone changes, modal open, and model reset clear the
  accumulator;
- the first press remains one logical move, taps remain precise, and reduced
  motion affects animation rather than traversal speed;
- `KeyRouter` remains the only page-level key boundary.

### Scrollbar and alphabet overlay

Retain `LibraryScrollIndicator` and the transient alphabet feedback in
`LibraryGridPage.qml`:

- keep the slim right-edge proportional thumb, brighter/thicker active state,
  and desktop drag behavior without making it a required TV focus stop;
- show the selected/first-visible item’s current `SortName` section (`A`–`Z`,
  `#`, or a sort-appropriate numeric/year value) beside the thumb while held
  navigation, wheel, or drag is active;
- keep the section visible for a short settle timeout, with locale-aware
  casing and no ad-hoc English article stripping;
- preserve pagination prefetch and loaded-row clamping during acceleration.

Optional direct alphabet jumping may use the existing `alphabet` query only as
a later enhancement; do not replace smooth traversal with 27 server reloads.

Tests use a fake clock for the acceleration curve and cover 1/10/250/5000-item
models, frame stalls, direction reversal, release reset, Unicode/# labels,
non-name sorts, pagination boundaries, and reduced motion.

## Phase 5 — Network, cache, playback, and privacy hardening
Status: **implemented and verified**.

### TLS trust

1. Retain the linked OpenSSL 3 target Qt build and the compile-time
   `QT_CONFIG(ssl)` assertion. Keep the TLS backend in the static-plugin/link
   audit.
2. Replace the current header-only `TlsTrust` storage plus discovery-owned
   pending state with a small shared controller. On `sslErrors`, pause the
   affected reply, collect authority/fingerprint/error/issuer, and expose one
   pending decision to QML.
3. Expand the current “Trust this certificate and retry” action into a blocking
   readable decision: Cancel, Trust once for only that reply, or Remember for
   this authority and leaf fingerprint. A changed certificate prompts again.
4. Store no blanket `ignoreSslErrors` boolean. Allow removing remembered
   certificates under Advanced > Connections. Discovery, API, artwork, and
   WebSocket traffic share the controller; mpv/curl receives an equivalent
   per-authority CA/pinned-certificate policy or reports that playback trust is
   unresolved rather than disabling verification.
5. Redact certificate payloads only where private; fingerprints, host, errors,
   and public issuer names remain useful diagnostics.

Tests: self-signed local fixture, Trust once, Remember/restart, changed leaf,
hostname mismatch, cancel, concurrent replies, profile on two authorities.

### Fail-open cache database

Treat SQLite as rebuildable cache plus local preferences/profile storage:

- split durable profile/settings export from disposable cache tables, or
  recreate only cache tables on cache schema mismatch;
- on a newer/invalid schema, close connections, rename the broken DB to a
  bounded diagnostic backup, create a fresh current schema, and continue boot;
- if durable data cannot be read safely, keep the backup and start at Add
  account with a clear one-time message—never exit the app over cache data;
- corrupt cache payload/JSON is a miss and is deleted;
- cap backups and test read-only/corrupt/future-version cases.

Because this app is unreleased, do not add incremental migration chains.

### Series/season play entry and episode overlay

The Series and Season detail pages need a primary Play action even though those
container items are not themselves playable. Keep one policy in C++, invoked by
both pages:

1. Fetch `/Shows/{seriesId}/Episodes` with current-user `UserData`; pass
   `seasonId` on a Season page and omit it on a Series page. Preserve Jellyfin's
   returned series order rather than re-sorting titles in QML.
2. Find the last row whose `UserData.Played` value is true, then select the
   first playable episode after it. If no row is played, select the first
   playable episode returned. Here “downloaded episode” means an episode
   actually present and playable in the Jellyfin result; this client has no
   local-download registry, so never infer offline state from `Path` or
   `MediaSources`. Skip virtual/missing placeholders, do not wrap to the
   beginning after the final played episode, and show a concise unavailable
   message when no candidate exists.
3. Queue the complete ordered result for the page scope and start at the chosen
   index, preserving a meaningful resume position on that target. Do not route
   the chosen row back through `playEpisodeWithContext` and perform a second
   all-series request. Use the existing episode-queue generation/busy handling
   so a stale response cannot start playback after navigation or another play
   request.
4. `ItemDetailsPage.qml` shows the same primary Play control for Series and
   Season pages, disables duplicate activation while selection is pending, and
   retains the current Movie/Episode Resume, Restart, focus, pointer, and
   overflow behavior.

The player overlay also needs episode identity independent of mpv's status
text. Extend the current `PlayQueueController` row/snapshot with `seriesName`,
`seasonNumber`, and `episodeNumber`; the queue already owns the full
`MovieItem`, so do not duplicate this metadata in `PlayerController`.
`PlayerOverlayChrome.qml` renders a smaller `Show name · S01E01` context line
above the episode title. Non-episode playback keeps the existing single title
line.

The client is **not** generating the incoming episode title:
`mediaItemFromJson()` maps Jellyfin `BaseItemDto.Name` to `MovieItem.title`, and
`buildPlaybackSession()` forwards that value unchanged. `BaseItemDto` has no
provenance flag saying that a name was auto-generated; `IsPlaceHolder` describes
item availability, not title authorship. Add one narrow shared presentation
helper that treats a trimmed, case-insensitive `Episode N`/`Episode 0N` as
generic only when `N` equals `episodeNumber`. Hide that title line, including
its layout space, but retain the show/context line. Do not hide descriptive
names such as `Episode 1: Pilot`, unrelated numeric titles, or names when the
episode index is unknown.

Focused tests cover:

- first playable selection when none are played; the first playable row after
  the last played row; sparse/non-playable placeholders; no wrap after the last
  played row; and Series versus Season queue scope;
- generic-title matching for case/whitespace/zero padding and non-matches for
  descriptive or index-less titles;
- play-queue role and `get()` snapshots retaining show/index metadata;
- player-overlay rendering for a named episode, generic `Episode 1`, and a
  non-episode item, including no blank title gap;
- native smoke playback from both a Series overview and a Season page, checking
  the selected queue index, resume behavior, overlay text, next/previous episode
  controls, and unchanged Movie playback.

### Secret-free playback URLs

1. Remove the now-unused `accessToken` argument from
   `PlaybackNegotiation::buildUrl`; retain the token-free URL assertions.
2. Retain per-load `X-Emby-Token`/MediaBrowser authorization for mpv HTTP
   requests, including HLS manifests/segments, subtitles, and trickplay where
   applicable. Clear it on stop and profile switch.
3. Keep authorization out of QML, `PlaybackSession::url`, diagnostics, and
   command logging. Preserve redaction for `X-Emby-Token`, `Authorization`, and
   mpv header-option renderings.
4. Extend tests for redirects and profile switching so an old token cannot
   survive in a retained mpv handle or option.

### One-shot codec renegotiation

- retain the existing early direct-play failure signal and
  `m_codecFallbackAttempted` one-shot guard;
- tighten decoder/demux/open classification so network/auth/TLS and
  render-surface failures do not masquerade as codec errors;
- preserve resume position, queue, and selected tracks where valid when
  renegotiating with `EnableDirectPlay=false`;
- for SyncPlay, do not independently start an unscheduled fallback—coordinate
  it or leave the group with an explanation;
- keep the replacement session annotated so a second error cannot recurse.

Tests still required: direct fail → transcode success, fail after meaningful
progress (no retry), network fail (no retry), fallback fail (one error), and
resume/queue preservation.

### Screensaver

Move the existing implementation out of `main.cpp` into the platform-owned
controller defined in Phase 0A:

- retain the webOS
  `com.webos.service.tvpower/power/registerScreenSaverRequest` subscription and
  Active-response `ack:false` behavior;
- verify pause/stop/background teardown unregisters and resume re-registers;
- include video and audio playback; inhibit a slideshow only while its timer is
  advancing;
- make subscription/cookie calls idempotent and clean them before platform/app
  teardown;
- retain Windows `SetThreadExecutionState`; on Linux retain the existing
  freedesktop ScreenSaver DBus path and prefer compositor idle-inhibit when a
  reliable Qt/Wayland integration is available; implement or explicitly report
  macOS capability.

Tests cover play/pause/resume/stop, desktop cookie/execution-state transitions,
and mocked LS2 request/response payloads. TV behavior remains a later explicit
hands-on verification because this plan does not authorize deployment.

## Phase 6 — Minimal, explicitly LGPL FFmpeg everywhere
Status: **implemented and verified**.

### Common manifest

Replace platform drift with one reviewed FFmpeg capability manifest consumed by
webOS configure, Linux/Nix, and Windows fallback builds. It lists libraries,
protocols, demuxers, parsers, decoders, filters, muxers, encoders, and bitstream
filters required by mpv and this client. Generate arguments from the manifest
where practical and add a validation script that rejects accidental GPL,
version3, nonfree, or unlisted features.

Every configure invocation begins with:

```text
--disable-everything
--disable-gpl
--disable-version3
--disable-nonfree
--disable-autodetect
```

Then re-enable the FFmpeg libraries mpv links and the smallest playback set.
The initial allowlist must cover the formats already promised by the current
client: Matroska/WebM, ISO BMFF/MP4/MOV, MPEG-TS/HLS, AVI, Ogg, WAV, FLAC,
MP3/AAC; H.264/HEVC/AV1/VP9 parsing/bitstreams as required for hardware/direct
paths; common PCM, AAC, AC-3/E-AC-3, DTS, TrueHD/MLP, FLAC, ALAC, MP2/MP3,
Opus, Vorbis, WMA audio decoding; subtitle/data plumbing mpv requires; resample,
scale, basic audio filters/night mode; AAC encoding and SPDIF only if the
current Starfish/audio paths demonstrably use them.

### Platform work

- **webOS:** retain the existing explicit disable/license flags and source
  allowlist, then generate it from the common capability manifest. Preserve
  Thumb-2/LTO/shared-library choices and record the final configuration.
- **Linux:** retain the Nix LGPL overlay and `ffmpeg-slim.json` exclusions, but
  replace duplicated option knowledge with the common manifest; ensure mpv and
  the AppImage close over that exact FFmpeg.
- **macOS:** prove the Nix/package path consumes the same manifest and retain
  only compatible platform decode integrations.
- **Windows:** retain disabled Meson fallback auto-features/GPL/version3/nonfree
  options, generate the explicit feature list from the common manifest, and
  audit the effective configuration in CI.

### Expected removals

The current client should not lose a useful user-facing feature. Expected
removals are developer/unused surface: FFmpeg CLI tools/docs/tests, libvidstab
and other GPL-only filters, external GPL encoders, capture/device inputs,
streaming protocols Jellyfin never emits, and hundreds of unused codecs,
demuxers, muxers, and filters. x264/x265 and other external GPL encoders are
already disabled in the Linux slim manifest and are not client playback
features. If the audit identifies a critical playback path—normal Jellyfin
video/audio/subtitle playback, HLS transcode, night mode, passthrough, or
Starfish feeding—that truly requires GPL/nonfree FFmpeg, stop and report the
specific component instead of landing the FFmpeg phase.

Acceptance:

- effective configure output contains all four disable/license flags on every
  platform and no `enable-gpl`, `enable-version3`, or `enable-nonfree`;
- licence output says LGPL and an automated audit enforces it;
- representative direct/remux/HLS/audio/subtitle fixtures open;
- webOS staged FFmpeg library set shrinks and remains stripped;
- `readelf`/dependency audit finds no accidental GPL external codec library;
- Thumb-2 attributes/instruction sampling remain asserted for target Qt,
  FFmpeg, mpv, and app objects.

## Phase 7 — Tests, CI, formatting, and final launch
Status: **implemented and verified**.

### CI

1. Retain Linux/macOS `ctest --output-on-failure` after app builds and the
   existing Windows test step.
2. Retain strict compiled-QML lint on Linux/macOS; add equivalent coverage where
   Windows tooling supports it without false success exits.
3. Add the missing QML input tests for profile picker and Settings disclosure;
   retain KeyRouter, accelerated navigation, and dialog tests; add physical
   mouse Back/Forward routing and forward-history coverage.
4. Retain FFmpeg compliance, Qt SSL, token redaction, and startup smoke gates;
   add platform-source compile/link, Thumb-2 artifact, screensaver, LS2
   lifecycle, webOS audio-route, and desktop mpv-config policy tests.
5. Keep tests and lint before packaging so a broken artifact is never produced.

### Local verification order

Batch edits, then one formatting pass over all touched QML/C++ files:

1. focused unit/QML tests while iterating;
2. `qmlformat` and `clang-format` once per coherent batch;
3. `nix develop .#native -c cmake --preset linux-dev`;
4. `nix develop .#native -c cmake --build build/linux-dev/app --target
   jellyfin-native`;
5. `ctest --test-dir build/linux-dev/app --output-on-failure`;
6. qmllint/import scan;
7. Nix/FFmpeg configuration and licence audits;
8. webOS configure/build checks only where they do not install/launch;
9. final real local foreground check with the existing minimal command:
   `timeout 10s nix run`.

For the last step, keep the app visible for the timeout, inspect stdout/stderr
and diagnostics for QML errors, confirm the profile picker appears on a launch
with saved pairs, select the intended local profile if interaction is available,
and confirm Home populates without blank rows or route churn. Do not substitute
only `--smoke-and-exit`; the visible launch is required.

## Final acceptance checklist

- [x] `src/main.cpp` is platform-neutral composition and contains no OS service,
      window-system, audio-device, screensaver, path, or backend branches.
- [x] LG/LS2/Wayland-webOS/Starfish/direct-ALSA headers occur only below
      `src/platform/webos/` (and the `mpv/` submodule’s own Starfish backend).
- [x] Linux, macOS, Windows, and webOS compile-select only their platform source
      set, and capability/audio-output/path policy tests cover each set.
- [x] Physical mouse Back/Forward buttons follow modal-aware route history on
      Linux, macOS, Windows, and webOS; synthesized touch clicks do not.
- [x] Desktop Expert settings can load the native mpv config directory or one
      validated custom directory on next playback, while app embedding,
      authorization, input, and privacy invariants still win.
- [x] Every launch with saved pairs asks which account/server pair to use.
- [x] Add account creates a new pair without overwriting existing users.
- [x] Switch user sits beside Settings and preserves saved pairs.
- [x] Remote-control WebSocket remains connected only to the active pair and
      its commands/push invalidation are tested.
- [x] Settings opens in a polished Essential view; More/All progressively
      reveal one list with stable D-pad focus.
- [x] Rows, sliders, toggles, selectors, dialogs, outlines, and surfaces are
      readable and coherent on TV and native desktop.
- [x] Smart/Default/Forced/Always/None are explained and behave like Jellyfin;
      burn-in/PGS server policy controls are gone.
- [x] Subtitle preview accurately reflects text/bitmap/HDR limitations.
- [x] Held navigation uses the Kodi-derived curve and the library shows a
      transient alphabet/section bubble and usable scrollbar.
- [x] Series and Season Play choose the first playable episode after the last
      played row (or the first playable row when none are played), preserve the
      scoped episode queue, and never wrap after the final played episode.
- [x] Episode overlays show `Show name · S01E01` above descriptive episode
      titles and suppress only a matching generic `Episode N` title without
      leaving a blank line; non-episode overlays are unchanged.
- [x] Self-signed TLS requires a scoped, fingerprinted user decision and works
      consistently for API, artwork, WebSocket, and playback.
- [x] Invalid/future cache data cannot brick boot.
- [x] A direct codec failure retries once through server negotiation and cannot
      loop.
- [x] No access token occurs in a playback URL, QML property, log, diagnostic,
      or retained mpv option after switching profiles.
- [x] Screensaver is inhibited while actively playing and allowed while paused
      or stopped.
- [x] Qt/FFmpeg target builds retain Thumb-2; staged libraries are stripped.
- [x] Every FFmpeg build is allowlist-only and explicitly LGPL; no critical
      feature was silently dropped.
- [x] CI runs unit/QML tests and qmllint before packaging.
- [x] Formatting, desktop build, full tests, audits, and the visible timed
      `nix run` launch pass.
- [x] No TV deployment/launch and no git push occurred.

## UI latitude for implementation

The visual descriptions above are constraints, not pixel-perfect mocks. The
implementing agent may improve typography, spacing, focus motion, grouping,
icons, preview imagery, empty/error states, and desktop hover/context behaviour
when the result is simpler and measurably clearer. Preserve the product rules:
one settings route, progressive disclosure, TV-safe focus, account/server pair
identity, high-contrast focus, and no decorative work that adds latency or
duplicate components.
