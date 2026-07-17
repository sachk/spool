# EVAL_REPORT — codebase evaluation (2026-07-17)

Independent pass over the whole tree (no subagents), evaluated against the
project's own stated goal: **feature/settings parity with jellyfin-web, but
native, fast, and mpv-based**. Kodi's webOS port (sparse-cloned
`xbmc/platform/linux`, `xbmc/windowing/wayland`, `MediaPipelineWebOS`) was
used as a reference for platform-integration completeness.

## Snapshot

| Metric | Value |
|---|---|
| `src/` C++ | 25,676 LoC (97+ files) |
| `qml/` | 12,612 LoC (62 files) |
| `tests/` | 4,739 LoC, **34 tests, all passing in 4.8 s** |
| mpv fork divergence | **111 commits, +9,447 / −198** (Starfish VO/VD/AO, webOS foreign-surface, OSD-over-shm) |
| Root-level planning docs | ~20 files, 5,650 lines of markdown |
| TODO/FIXME/HACK markers in code | 0 |
| NEXT_PLAN acceptance greps (input router) | **all pass** (0 stray `Keys.on`, 0 `handleKey`, `isAutoRepeat` only in KeyRouter) |
| Size vs own target | 38.3k vs 26–26.5k target (NEXT_PLAN §3); +7.3k since 2026-07-09, mostly feature commits |

## Grades

| # | Attribute | Grade /100 |
|---|---|---|
| 1 | Architecture & separation of concerns | **84** |
| 2 | Code quality & idiom | **86** |
| 3 | Bloat control / size discipline | **58** |
| 4 | Feature completeness vs jellyfin-web | **66** |
| 5 | Performance engineering | **90** |
| 6 | Robustness & error handling | **71** |
| 7 | Testing & CI | **68** |
| 8 | Security & privacy | **60** |
| 9 | webOS platform integration | **83** |
| 10 | Documentation & process | **80** |
| | **Overall (unweighted mean)** | **75** |

---

## 1. Architecture & separation of concerns — 84

**Strong.** The layering is real, not aspirational:

- **Input**: `qml/shell/KeyRouter.qml` (164 lines) is the single dispatcher
  for every key event — normalization (webOS scancodes, back-key synonyms,
  player noise), accept press/long-press/release arming, type-ahead, and a
  single explicit `activeTarget` resolution in AppShell. The four racing
  dispatch paths documented in NEXT_PLAN §4 are actually gone; the
  acceptance greps confirm it. This is a textbook fix of a structural bug
  class, and pages adopted the protocol (`routeKey`/`focusZones` tables in
  ItemDetailsPage).
- **Navigation**: `RouterController` is the single route owner; QML
  singletons (`App`, `Router`, `Player`, `Browse`, …) replaced context
  properties, so qmlcachegen direct calls and qmllint are live options.
- **Player**: decomposed into right-sized policy units with tests
  (`PlaybackPositionTracker`, `PlaybackTrackParser/State`,
  `PlaybackTimeline`, `PlaybackReporter`, `AudioSyncPolicy`,
  `MpvOptionProfile`, `PlayQueueController`, `MpvRuntime` dlopen shim).
- **API**: `JellyfinApiFacade` + extracted `PlaybackNegotiation` /
  `PlaybackBandwidthPolicy`, modern Qt HTTP stack
  (`QRestAccessManager`/`QNetworkRequestFactory`), QCoro tasks end-to-end
  with `RequestGeneration` latest-wins tokens.

**Deductions:**

- `PlayerController.cpp` is still a **1,955-line, ~107-method god object**
  — mpv lifecycle + idle-handle adoption + seek command building + segment
  logic + trickplay + delay/volume/speed state in one class. The extracted
  helpers orbit it rather than shrink it.
- `main.cpp` is 1,293 lines: LS2 callbacks, ALSA latency reads, Wayland env
  setup, log plumbing, signal handling, RHI cache config. It's coherent
  glue, but it's a `src/platform/webos/Bootstrap.{h,cpp}` waiting to exist
  — and FINDINGS 1.2/1.3 (PlatformIntegration extraction) already agrees.
- `MetaJson.h`, the "generic" mapper, hardcodes per-type key aliases
  (`Title→Name`, `MovieId→Id`, `ItemType→Type`…) in the shared lookup path
  — type-specific knowledge in the generic layer, exactly the drift the
  mapper was meant to end.

## 2. Code quality & idiom — 86

Modern, consistent, disciplined. QCoro coroutines with generation-token
guards read linearly (`HomeModelController::refreshAsync`); Qt 6.7+ HTTP
idioms used correctly; QML primitives are small and composable
(`MediaItemCard` 143 lines, `ImageCard` 65, post-diet); zero TODO/FIXME in
code because debt is promoted into tracked planning docs — a genuinely
better system than comment rot. Naming is uniform, formatting is enforced
(`qmlformat`/`clang-format` in dedicated commits).

Deductions: `ItemDetailsPage.qml` (1,518) and `LibraryGridPage.qml` (1,092)
remain tangles of ~40 functions each mixing focus math, data refresh, and
layout; a handful of C++ methods exceed 100 lines; `DiscoveryController`
mixes SSDP-style discovery with manual-probe state machines in one class.

## 3. Bloat control / size discipline — 58

The weakest axis **by the project's own standard** (NEXT_PLAN's net-negative
gates and the 26k target). Judged against jellyfin-web (~150k+ LoC of
TS/JS), the tree is admirably lean — but the local goal is the honest ruler.

- 38.3k src+qml vs the 26–26.5k target; +7.3k in the last 10 days. Most of
  it is features (negotiation, mixed libraries, person pages, diagnostics),
  but Phases C/D/E page diets have not landed: Player QML is 1,837
  (target ≤1,500), AppController 966 (target ≤700), ItemDetailsPage grew
  again.
- **Diagnostics is 2,374 lines of shipped dev tooling**: `InputLatencyMonitor`
  alone is 1,458 lines (+499 test) of frame-stage latency profiling. It
  demonstrably drove real fixes, but it compiles into the release binary a
  user never benefits from at runtime. Same for parts of
  `SystemPerformanceMonitor` and the `Diagnostics` event/phase/task tracer.
- **Doc sprawl**: ~20 root-level markdown files (5.6k lines), several stale
  or superseded (`PLAN.md`, `REFACTOR_PLAN.md`, `redesign.md`,
  `SEARCH_PLAN.md`, `CARD_DESIGN.md`, `DYNSTREAM.txt`, `search-bug.png`),
  some untracked. The refactor report marks items "done" that FINDINGS
  reopens — two sources of truth.
- The **111-commit mpv fork** (+9.4k lines) is the single largest "code you
  must carry" item. It's the price of the core design decision (below) and
  is well-organized (`video/out/starfish/` is cleanly additive), but it is
  maintenance surface nothing upstream will absorb unless pushed.

What is *not* bloat, despite its size: the webOS input context (661 lines —
the OSK/text_model dance is irreducibly fiddly), the settings schema, the
bandwidth/negotiation policies (all tested), and the build system (731-line
CMakeLists + 405-line flake pinning a full static-Qt/ffmpeg/mpv toolchain is
reasonable for what it does).

## 4. Feature completeness vs jellyfin-web — 66

**Present and solid** (video-first parity is close): resume/next-up/latest
rows, mixed libraries, series/season/episode drill-down, person pages with
grouped credits, search with type-ahead, playlists, collections/box sets,
genres/studios, favorites/played toggles, Quick Connect, server discovery +
real reachability probe, SyncPlay (with clock tests), trickplay previews,
media-segment skip (intro/credits), chapters, subtitle selection +
preferences + preload, audio track selection, audio delay/sync panel,
playback speed, transcode negotiation with codec/bandwidth capabilities,
fMP4 HLS, music/audiobook/photo item types in browse and negotiation
profiles.

**Missing vs jellyfin-web** (excluding admin/dashboard, which a TV client
shouldn't ship):

- **Live TV & DVR** — entirely absent (1 file mention). Legitimate scope
  cut, but it is the biggest named parity gap.
- **Remote-control target / session WebSocket** — the app *advertises*
  remote control but has no session socket to receive commands
  (FINDINGS 2.2/5.4); cast-to-TV silently fails. Worst kind of gap:
  visible to other clients.
- **Multi-user** — single saved profile; TVs are shared devices.
- **Display/subtitle appearance settings** — 12 settings total vs web's
  dozens (subtitle size/color/position, theme, home-section arrangement).
- Lesser gaps: lyrics, books reader, photo slideshow niceties, per-user
  home-section customization.

## 5. Performance engineering — 90

The standout attribute; this is a performance project that happens to be a
Jellyfin client:

- Startup: static Qt with import trimming, qmlcachegen AOT (+direct calls
  now unlocked by singleton registration), persistent RHI pipeline cache,
  persistent startup caches, boosted QML incubation controller, home
  payload cache with schema versioning for warm-boot first paint.
- Playback: **idle mpv pre-initialization** so play-press cost is paid
  early; fMP4 HLS transcodes; bandwidth policy with tests; Starfish
  hardware decode through the fork; OSD uploaded via Wayland shm with
  changed-bounds-only publishing (fork commits show iterative refinement).
- UI: delegate binding diet (MediaItemCard readonly-property cull),
  artwork prefetch + `MemoryBudget`, memory-pressure LS2 subscription
  with aggressive-pressure handling, reduced-motion support.
- Measurement culture: `InputLatencyMonitor` frame-stage attribution,
  `SystemPerformanceMonitor`, decoder CPU-time measurement (fork), latency
  budgets asserted in tests. Optimization here is evidence-driven, not
  vibes-driven.

Deductions: known unlanded wins the plans themselves rank top — ffmpeg
`--disable-everything` (~10–12 MB), stripped staged libs, `-mthumb`
Qt rebuild, and the `waitFor(loadDeviceIdAsync())` startup serialization.

## 6. Robustness & error handling — 71

Good bones: HTTP retry policy (tested), latest-wins generation tokens
everywhere, event-loop watchdog, log rotation with directory creation
fixes, memory-status subscription, queued toasts for background errors,
inline loading states replacing the old modal scrim, probe with candidate
fallback (http/https, port variants).

Deductions, all confirmed in code or FINDINGS:

- **No TLS trust path**: zero `sslErrors` handling. Self-signed certs are
  endemic in the Jellyfin homelab world; today that's a silent connection
  failure with no override UX (jellyfin-web inherits the browser's cert
  UI; a native client must provide its own).
- **DB schema mismatch can brick boot** (FINDINGS 12.1) — it's a cache;
  it must fail open by wiping.
- **Unsupported codec is a hard failure** — no error-driven
  re-negotiation retry (`EnableDirectPlay:false` fallback).
- Idle-mpv adoption / Starfish env-var config still carries the
  "prepared without window id" hazard class (FINDINGS 1.4/6.4).

## 7. Testing & CI — 68

34 tests, all green, 4.8 s total — the right shape of suite: policy and
parser units (negotiation, bandwidth, track parsing, position tracking,
timeline, option profiles, settings schema, route policy) plus QML input
protocol tests (`tst_KeyRouter`, `tst_PlayerOverlayInput`,
`tst_AtomicViewReveal`). Fast, deterministic, aimed at the logic most
likely to regress.

Deductions:

- **CI builds artifacts but never runs the tests** — no `ctest` step in
  `build-artifacts.yml`. A 5-second suite not wired into CI is a
  self-inflicted wound (FINDINGS 3.2 agrees).
- No qmllint gate despite being newly unlocked (zero context properties).
- No integration tests against a Jellyfin server fixture (the facade's
  JSON expectations are tested only via canned objects).
- Playback state machine (`PlayerController`) — the largest, riskiest
  file — has the least direct coverage.

## 8. Security & privacy — 60

- **Access token still rides in playback URLs**: `PlaybackNegotiation.cpp:92`
  appends `api_key=<token>` to stream URLs, which mpv then logs; a shared
  diagnostics bundle is a session compromise (their own P1, FINDINGS 2.4).
  Artwork was fixed (Authorization header, token-free cache keys); playback
  was not.
- Token stored plaintext in SQLite — acceptable on webOS (no keystore),
  same class as jellyfin-web's localStorage, but worth a note in docs.
- No certificate trust/pinning decision surface (see §6).
- Good: central secret-redaction regex for diagnostics, no telemetry,
  `NoLessSafeRedirectPolicy` on probes, headers preferred over URL auth
  everywhere else.

## 9. webOS platform integration — 83

Measured against Kodi's port (the only comparable open-source native webOS
media app), coverage is deep:

- LS2: `registerApp` lifecycle, `memorymanager/getMemoryStatus`,
  `audio/getSoundOutput` (with ALSA latency introspection for lip-sync),
  `ime/registerRemoteKeyboard`.
- Display/video: Wayland foreign-surface export for the Starfish video
  plane, layered mpv OSD over shm, custom text_model input context
  (`WebOSInputContext`, 661 lines) for the stock OSK, remote scancode
  normalization including color keys.
- The mpv-fork Starfish backend (`vd_starfish`, `vo_starfish`,
  `ao_starfish`, split-clock A/V sync) is a deeper decode integration than
  Kodi's `MediaPipelineWebOS` in some respects (subtitle preload across
  reloads, seek recovery before start).

Gaps (Kodi has these, this app doesn't):

- **No screensaver handling** — Kodi registers
  `luna://com.webos.service.tvpower/power/registerScreenSaverRequest` and
  vetoes the saver while playing. Verify behavior for paused video, music
  playback, and photo display; if Starfish playback doesn't implicitly
  inhibit, this is a user-visible bug waiting.
- `setenv` after threads exist for Starfish config (UB, FINDINGS 1.4).
- ALSA control addressed by hardcoded numid instead of name
  (cross-model landmine, FINDINGS 2.3).

## 10. Documentation & process — 80

The self-auditing culture is exceptional and rare: NEXT_PLAN's post-mortem
does per-commit `--numstat` accounting of a failed refactor and derives
hard gates from it; acceptance greps are executable and were actually run
(they pass); FINDINGS.md is a brutal, prioritized pre-release audit;
AGENTS.md is crisp and operational. This process already caught and fixed
the input-dispatch disease and the LoC-churn failure mode.

Deductions: the root directory is an archaeology site — superseded plans,
untracked notes, stray screenshots, and status claims that disagree between
docs ("done" in refactor-report vs reopened in FINDINGS). A newcomer cannot
tell which of the 20 files is load-bearing. `README.md` is 32 lines for a
project of this depth.

---

## Design decisions — review

| Decision | Verdict |
|---|---|
| **mpv fork with in-tree Starfish backend** vs Kodi-style ES feeding into StarfishMediaAPIs from own player | **Right call** for a Jellyfin client: buys format breadth, libass subtitles, HLS handling, and a desktop-identical playback core for free. The cost is the 111-commit carry; mitigate by upstreaming what mpv might take (Wayland bits) and keeping the Starfish backend additive (it is). |
| Native Qt Quick instead of Enact/web (upstream jellyfin-webos) | Right call given the goal; the measured startup/latency work justifies it. |
| Static Qt + Nix-pinned toolchain | Right call; reproducibility for a 3-toolchain target (host, webOS ARM, Windows) is otherwise unmanageable. |
| QCoro throughout | Right call, and correctly extended from the facade into the controllers; the counter-join pattern is gone. |
| KeyRouter single-dispatch + target protocol | Right call, executed well; this is the architectural highlight of the QML layer. |
| Schema-driven settings (C++ schema → generic QML rows) | Right call; adding a setting is one schema entry. |
| MetaJson hand-rolled mapper vs codegen | Right call to reject the broken OpenAPI generator, but the alias table needs to move per-type. |
| Trusting webOS's implicit screensaver behavior | Unverified assumption — see §9. |
| Shipping the diagnostics suite in release builds | Wrong default; should be a build flavor. |

## Improvement ideas (prioritized)

1. **Wire `ctest` (and qmllint) into `build-artifacts.yml`** — 5 seconds of
   CI for the whole suite; the cheapest fix on this list.
2. **Move playback auth out of URLs** — send the token via header (or
   mpv `http-header-fields`) so `api_key` stops flowing into mpv logs and
   diagnostics bundles. Pair with FINDINGS 2.4.
3. **Session WebSocket** — receive remote-control/play commands and server
   push invalidation, or stop advertising the capability. Biggest missing
   TV-specific feature (cast-to-TV).
4. **TLS trust UX** — a "connect anyway / remember this server" decision on
   `sslErrors` for self-signed homelab certs.
5. **Fail-open cache DB** — wipe-and-continue on schema mismatch, never
   refuse to boot over a cache.
6. **Screensaver inhibition** — port Kodi's ~100-line
   `OSScreenSaverWebOS` pattern (tvpower registerScreenSaverRequest);
   verify paused/music/photo behavior on device first.
7. **Diagnostics build flavor** — compile `InputLatencyMonitor` +
   `Diagnostics` tracer out of release IPKs (`JELLYFIN_DIAGNOSTICS=OFF`);
   ~2.4k LoC and measurable binary/runtime cost returned to users.
8. **Finish NEXT_PLAN Phases C/D/E under the net-negative gates** — the
   input router (A) and cards/rows (B) prove the process works; Player
   overlay (1,837→≤1,500), ItemDetailsPage (1,518), LibraryGridPage
   (1,092), AppController (966→≤700) are the remaining tumors.
9. **Split `main.cpp`** into `platform/webos/Bootstrap` + a ~200-line
   `main` — it also directly serves the Android TV port path
   (FINDINGS 1.1–1.3).
10. **Docs hygiene** — `docs/archive/` for superseded plans; keep
    FINDINGS.md + NEXT_PLAN.md + README as the only living root docs, and
    write a real README (build matrix, architecture map, doc index).
11. **Error-driven transcode re-negotiation** — one
    `EnableDirectPlay:false` retry on decoder failure converts hard
    failures into degraded playback.
12. **Multi-user profiles** — saved credentials per user + a user picker;
    TVs are shared.
13. **ffmpeg `--disable-everything` + staged-lib stripping** — the two
    top-ranked, still-unlanded binary wins (~25+ MB combined).
14. **Per-type alias tables in MetaJson** — move the `Title→Name`-style
    special cases out of the generic lookup into the type's own mapping
    declaration.
15. **Upstream mpv work** — even landing only the generic Wayland/OSD
    pieces shrinks the 111-commit carry and de-risks every future mpv
    rebase.
