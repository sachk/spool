# Codebase Audit — Refactors & Major Tasks

Central, de-duplicated list of major refactoring opportunities and outstanding
work across the jellyfin-webOS native app. Produced 2026-05-28 by a four-agent
sweep of the project's own code (C++ `src/`, `qml/`, the custom `mpv/` Starfish
backend, and build/packaging/CI). Vendored upstreams (jellyfin-web, full mpv,
ffmpeg, etc.) were out of scope.

All `file:line` references are relative to the repo root
`/home/sachk/Documents/jellyfin-webos/jellyfin-webos` unless prefixed with `../`
(workspace root). Effort tags: **S** ≈ hours, **M** ≈ 1–3 days, **L** ≈ a week+.

This file supersedes the scattered notes as the single index. The originals it
folds in: `TODO.md`, `../human_notes.md`, `docs/player-overlay-refresh-plan.md`,
`../STARFISH_DEBUG_LOG.md`, `../STARFISH_PCM_AUDIO.md`, `../starfish_v2_proposal/`.

---

## Top priorities (the short list)

If only a handful of things get done, do these — they are the highest
impact-per-effort or unblock everything else:

1. **OSD/subtitle overlay on the Starfish video plane** — the headline product
   blocker; ~30 documented attempts, still unresolved. (Starfish §T1)
2. **Verify the PCM audio path on-TV, then delete the AAC encoder path** —
   unblocks ~350 LOC of deletion in `ao_starfish.c`. (Starfish §T2 / §R5)
3. **Split `PlayerController` (1379 LOC) and `starfish_ctx.cpp` (~2100 LOC)** —
   the two god-objects that dominate maintenance cost. (C++ §R1, Starfish §R1)
4. **Centralized HTTP retry + 401 re-auth; stop swallowing report failures** —
   resume position silently never persists today. (C++ §T1)
5. **CI build-check for the IPK + a single source of truth for the app version** —
   nothing compiles the webOS target on PRs. (Build §T1, §R4)
6. **Delete the stale `starfish_v2_proposal/`** (its core idea already shipped),
   but first salvage `docs/starfish/clocking.md` into the repo. (Starfish §V)

---

## C++ Application Core (`src/`)

~33 files reviewed in full. Only two literal TODO markers exist; the debt is
structural.

### Major Refactors

- **`PlayerController` is a god-object mixing 5 concerns** — `src/player/PlayerController.cpp:1` (1379 lines); header exposes 27 `Q_PROPERTY` + ~25 `Q_INVOKABLE` (`src/player/PlayerController.h:23`). Owns mpv lifecycle, the raw event-loop thread, seek/position math, track-list parsing, trickplay sprite math, skip-intro segment logic, and progress reporting. Split into e.g. `MpvEngine` / `PlaybackPositionTracker` / `TrackListParser` / `TrickplayModel`. Single biggest maintainability risk. **L**
- **Fragile positional aggregate-init of `MovieItem`** — `src/app/AppController.cpp:585` and `:947`; same pattern in `src/api/JellyfinApiFacade.cpp:102` and `:355`. Brace-lists pass 14 positional values into a 16-field struct (`src/common/JellyfinTypes.h:26`); any field reorder silently misassigns with no compiler error. Use designated initializers or a named factory. **M**
- **Two divergent `NativeAppWindow` implementations behind one header** — `src/app/NativeAppWindow.cpp` (webOS, 486 LOC) vs `src/app/HostNativeAppWindow.cpp` (desktop, 120 LOC), `#ifdef`-gated in `NativeAppWindow.h:50`. The `*OverlayImageProvider` inner classes are near-duplicates. Extract a shared base + platform backend. **M**
- **`AppController` conflates navigation, session, settings, prefetch, and quick-connect polling** — `src/app/AppController.cpp:1` (1223 lines). Hand-rolled nav stack (`m_currentViewKind`/`m_currentSeriesId`/…) and a generation-counter pattern duplicated across ~6 sites. Extract a router + a prefetch coordinator. **L**
- **Audio-output-mode normalization duplicated 3×** — identical `starfish`→`starfish-pcm` else `alsa` mapping at `AppController.cpp:210`, `:670`, and `PlayerController.cpp:780`. Centralize. **S**
- **Two near-identical secret-redaction regexes** — `JellyfinApiFacade.cpp:53` (`diagnosticUrl`) and `Diagnostics.cpp:139` (`sanitizedUrl`). Consolidate. **S**
- **Repeated fetch→cache→prefetch boilerplate in API callers** — `fetchMovies/Series/Seasons/Episodes` bodies (`JellyfinApiFacade.cpp:323`) are structurally identical; callers wrap each in the same generation-guard closure. A templated helper removes ~150 LOC. **M**

### Major Tasks

- **No HTTP retry/timeout recovery or auth-refresh on playback/report paths** — `JellyfinApiFacade::requestBytes` (`src/api/JellyfinApiFacade.cpp:797`) throws on any error; reporting callers swallow it with empty handlers (`PlayerController.cpp:241`, `:801`, `:820`), so a dropped progress/stopped report is lost and server-side resume never persists. 401 handling exists only in `loadLibraries` (`AppController.cpp:840`). Add centralized retry + 401-triggered re-auth. **M**
- **SyncPlay realtime sync incomplete + stale header comment** — `JellyfinApiFacade.h:67` claims the WebSocket "module not yet built", but `SyncPlayController` uses `QWebSocket`; it only handles Play/Pause/Seek/Stop (`SyncPlayController.cpp:154`) with no continuous time-sync/buffering coordination, and `m_groupName` is faked on join (`:187`). **M**
- **`buildPlaybackSession` hardcodes DirectPlay + 140 Mbit ceiling, no transcode fallback** — `JellyfinApiFacade.cpp:660`. `MaxStreamingBitrate`/`MaxStaticBitrate` pinned at `140000000` (`:668`, `:853`), `EnableTranscoding:false`, `PlayMethod` always `"DirectPlay"` (`:719`, `:733`). Servers that can't direct-play fail with no fallback. Make config-driven; derive play method from the chosen source. **L**
- **No expiry/invalidation on the SQLite item cache** — `src/cache/DatabaseManager.cpp` stores JSON blobs by id with no TTL/version/eviction; `applyMoviesCache` (`AppController.cpp:811`) shows stale data indefinitely and deleted server items are never purged. Add cache versioning + staleness. **M**
- **Image-prefetch hard-capped at 24 posters with magic numbers** — `AppController.cpp:1205` truncates to 24, concurrency `6` is a literal, while lists fetch up to 500 (`JellyfinApiFacade.cpp:335`). Track the visible viewport instead; lift caps to config. **M**
- **mpv option block is a giant hardcoded literal with no config surface** — `PlayerController.cpp:416` sets ~50 mpv options inline (subtitle styling, cache sizes, night-mode AF filter at `:34`). Move user-relevant ones to a config/profile. **M**
- **`qFatal` on recoverable playback-setup failure aborts the whole app** — `PlayerController.cpp:513` (desktop) and `MpvVideoItem.cpp:94` (`mpv_render_context_create` failure). Surface a UI error; abort only playback. **S**
- **`setVideoCrop` uses `BlockingQueuedConnection` from the decode callback thread** — `NativeAppWindow.cpp:454`. Risks a stall/deadlock if the GUI thread is itself waiting on mpv (cf. the async-command warning at `PlayerController.cpp:853`). Switch to queued+coalesced. **S**
- **Quick Connect timeout/poll limits are magic numbers** — `AppController.cpp:857` (36 polls), `:902` (6 errors), interval `5000` (`:74`); the 3-minute timeout is implicit. Name them. **S**

---

## QML UI Layer (`qml/`)

~5,528 LOC across 42 files. No TODO/FIXME markers — debt is structural. Note:
the `docs/player-overlay-refresh-plan.md` work is **largely done** in the working
tree (PlayerOverlayPage.qml down to ~1067 LOC; trickplay/skip/audio-sync/menu
components extracted). `SettingsPage.qml` (669 LOC) is now the largest target.

### Major Refactors

- **`ItemDetailsPage.qml` ignores Theme/Metrics; hardcodes scaling + colors** — `qml/pages/ItemDetailsPage.qml:91` defines its own `scaleFactor()` (`height/1080`), 18 `Math.round(N*scaleFactor())` calls, and hardcoded ring colors (`:40`) so the accent preference never reaches it. Also embeds a `DetailIconButton` duplicating `IconButton`/the overlay's focus ring. **M**
- **Search & ItemDetails reuse `appController.movies`, producing wrong content** — `qml/pages/SearchPage.qml:78` filters the movie grid client-side by title substring (`:89`); `ItemDetailsPage.qml:384` "More Like This" re-lists the entire movies model. Both need dedicated controller models. **M**
- **Duplicated accept-key / navigation handling across nearly every page** — `HomePage.qml` (6 sites), `SettingsPage.qml` (6), plus `ItemDetailsPage`, `LibraryGridPage`, `LibrariesPage`, `SearchPage`, `SideRail`. Same `Return/Enter/Select/Space` test + `focusRail()`-on-left + `Math.floor(width/cellWidth)` reimplemented per page, often twice. A shared `isAcceptKey()` / attached nav behavior removes ~100 LOC and fixes drift. **M**
- **Two near-identical horizontal-row components in HomePage** — `qml/pages/HomePage.qml:267` (`rowComponent`) and `:404` (`libraryRowComponent`) share `ensureVisible()`/`handleNavigationKey()`/Flickable+Repeater scaffolding. Collapse into one parameterized row. **M**
- **HomePage repeats the same 6-handler Connections block 4×** — `qml/pages/HomePage.qml:21` (latest/library/resume/nextUp), each re-caching `rowCount()` on model changes. Drive off the models directly. **S**
- **Scattered hardcoded hex literals bypassing Theme** — `LibrariesPage.qml:69`, `ItemDetailsPage.qml:202`/`:345`, `AppShell.qml:342`/`:376`/`:382`, `SelectRow.qml:36`, `MediaInfoOverlay.qml:24`. Add translucent scrim/error/busy tokens to Theme. **S**
- **SettingsPage manual `settingIndex` bookkeeping is fragile** — `qml/pages/SettingsPage.qml:30` + `:238`. ~27 rows hardcode literal `settingIndex: N` and a hand-listed `rebuildSettingsRows`; inserting one row renumbers everything. Make it data-driven. **L**
- **`Metrics.metaPx` reaches through `root.Window.window.width` in a primitive** — `qml/primitives/SelectRow.qml:45` (brittle global reach-out + magic 1920 fallback). Pass width in via property. **S**

### Major Tasks

- **Dead settings UI** — `qml/pages/SettingsPage.qml:437` (max remote bitrate), `:449` (prefer remux), `:467` (shortcuts enabled) have no handler / hardcoded `checked`. Wire to `appController` or remove. **M**
- **Selectors don't reflect persisted state** — `SettingsPage.qml:340` — several `SelectRow`s set literal `currentIndex` so the shown selection desyncs from the live singleton after restart. Bind `currentIndex`. **S**
- **Side rail missing a Libraries entry; uses ASCII letters not Material icons** — `qml/shell/SideRail.qml:75`. Libraries only reachable from Home; "H"/"/"/"S" via `IconButton.iconText` clash with the Material icons used elsewhere. **S**
- **No empty-state for player track menus** — `qml/pages/PlayerOverlayMenu.qml:94`. Add a "No subtitles available" row (overlay-plan §3f). **S**
- **Accessibility: no `Accessible` properties anywhere** in `qml/`. Blocks screen-reader/voice on a TV client. **M**
- **Unbounded/visibility-filtered list models** — `SearchPage.qml:81` instantiates the whole movies model and hides delegates via `visible:`/`opacity:` (`:89`) instead of a filtered model; grid pages don't paginate. **M**
- **Reduced-motion only partially honored** — `SideRail.qml:117`/`:129`, `HomePage.qml:156`, `SettingsPage.qml:228` animate unconditionally (cf. correct gating in `ImageCard.qml:15`). **S**

---

## mpv Starfish Backend (`mpv/`)

Custom files only: `audio/out/ao_starfish.c`, `video/decode/vd_starfish.c`,
`video/out/vo_starfish.c`, `video/out/starfish/{ctx,json,backend}.{c,cpp,h}`.
Memory ownership in the feed path was reviewed and is **sound** (packet buffers
held by `shared_ptr`, copied before the unlocked feed). The LS2/Starfish JSON
boundary is well-guarded (`try_bool`/`try_string` wrappers).

### Major Refactors

- **`starfish_ctx` is a god-object/file** — `video/out/starfish/starfish_ctx.cpp` (2,106 LOC); one struct (`:175`) holds 60+ fields across session lifecycle, HDR/DOVI resolution, packet queues, segment-timeline anchoring, the clock sampler, and the pending-segment-ready state machine. Extract subsystems into separate TUs. **L**
- **HDR/DOVI color-mapping helpers don't belong in the session file** — `starfish_ctx.cpp:345` (~150 LOC of `pl_color_*`→`AVCol*` tables + `apply_hdr_info`). Move to `starfish_hdr.cpp` (or fold into `starfish_json`); makes them unit-testable. **M**
- **Pending-segment-ready state machine duplicated 3 ways** — `starfish_ctx.cpp:1300` vs `:760` vs `:688`; the same 7-field reset-and-queue sequence is hand-inlined in the FRAMEREADY handler, the PLAYING handler (twice), and the watchdog. Extract `release_pending_segment_ready_locked(...)`. High bug-density area. **M**
- **Dead JSON builders** — `video/out/starfish/starfish_json.cpp:204` (`_build_feed`, `_build_seek`, `_build_play_rate`) are unused; `starfish_backend.cpp:303`/`:231`/`:282` build the same JSON inline. Delete or route the backend through them. **S**
- **Two parallel "project the clock forward" implementations** — `starfish_ctx.cpp:1024` (`project_fresh_clock_locked`) vs `:1889`/`:2058`; `pop_video_frame` and `get_current_pts` re-derive projected PTS with subtly different EOS clamping. Consolidate. **S**
- **AAC encode path is large dead-weight in `ao_starfish.c`** — `audio/out/ao_starfish.c:139`, `:480`, `:659` (`reopen_encoder_locked`, `encode_pending_audio`, `encode_silence_frame`). The file's own comments (`:80`, `:660`) say it's removable once PCM is the only mode — ~350 LOC + per-function branching. (Blocked on §T2.) **M**
- **`render_osd_surface` mixes three output paths plus logging** — `video/out/vo_starfish.c:411` (110 LOC: launcher-callback, Wayland-shm, early-skip, interleaved with alpha-scan diagnostics). Split per path. **S**

### Major Tasks

- **[BLOCKER] OSD/subtitle overlay on the Starfish video plane is unresolved** — `video/out/vo_starfish.c:411`; `../STARFISH_DEBUG_LOG.md` Attempts 19–47. mpv produces correct OSD pixels but webOS composition makes the OSD an opaque black layer over punch-through video, or the imported-surface path crops/black-screens video. Work has moved into the Qt launcher (Attempts 40–47), still crashing around `StarfishMediaAPIs::Load`. Largest outstanding task; spans beyond these VO files. **L**
- **PCM audio path unverified on-TV** — `video/out/starfish/starfish_json.cpp:117` (`pcmInfo`); `../STARFISH_PCM_AUDIO.md` §5 flags the exact `pcmInfo` key name + field set as needing on-TV confirmation. Until verified, the AAC path can't be deleted. **M**
- **Feed `BufferFull`/`Retry` uses a fixed 25 ms busy-wait, not event-driven backpressure** — `starfish_ctx.cpp:1202`, const at `:71`. `SF_EVENT_BUFFER_LOW` wakes it but the sleep dominates, adding latency/jitter. Wait on the CV until BUFFER_LOW or timeout. **M**
- **Clock-export readiness gating can strand audio sync after seeks** — `starfish_ctx.cpp:2084`, `:935`. `get_video_clock` returns false until a ≥250 ms stable window (`:66`); `SEEK_DONE` resets readiness (`:1408`), so every seek re-incurs the gap during which mpv audio falls back to the synthetic dummy-frame PTS. Validate thresholds across rapid seeks. **M**
- **`ao_starfish` pending list has no backpressure cap** — `audio/out/ao_starfish.c:256` (`feed_pending_packets`) re-queues to an unbounded `pending_head` on `STARFISH_FEED_AGAIN`; only the ctx-side `AUDIO_QUEUE_LIMIT` caps it. Add a ceiling + drop policy. **S**
- **`unload` timeout silently forces IDLE** — `starfish_ctx.cpp:1561`; if `UNLOADCOMPLETED` doesn't arrive in 2 s, state flips to IDLE while the backend may still be unloading, racing a subsequent Load. Define a real recovery (destroy/recreate backend). **M**
- **Soft spot: feed-result classification is substring matching** — `starfish_backend.cpp:315`; an unexpected SDK string falls through to `SF_BACKEND_FEED_RETRY` and can spin (mitigated by `MAX_FEED_AHEAD`). Add an explicit unknown-result log. **S**

### v2 Proposal Assessment — recommendation: **SKIP as a replacement; SALVAGE its docs**

`../starfish_v2_proposal/` is a stale, never-integrated scaffold. The current
tree has already adopted its core idea independently:

1. **The headline change already shipped.** v2's pitch ("replace `VOCTRL_SET_EXTERNAL_AUDIO_CLOCK` with `GET_EXTERNAL_VIDEO_CLOCK`, Starfish video as master") is already in tree: `vo_starfish.c:705` implements `VOCTRL_GET_EXTERNAL_VIDEO_CLOCK`, the worker samples `getCurrentPlaytime()` off-thread (`starfish_ctx.cpp:935`), and `SET_EXTERNAL_AUDIO_CLOCK` has zero references. Both proposal patches would fail to apply (their "before" context is gone).
2. **Stale on audio architecture.** v2 assumes `--ao=alsa` + mpv resampling as default; the project moved the other way — PCM-over-Starfish to drop ALSA (`../STARFISH_PCM_AUDIO.md`).
3. **Misleading LOC claim.** Its ~30% savings partly come from omitting shipped features — the HDR10+→DoVi 8.1 path (`vd_starfish.c:489`, `dovi_generate_from_json`) is unaccounted for.
4. **Its one good idea already landed.** The narrow C ABI over the LG C++ SDK exists today (`starfish_backend.cpp/.h`).

**Action:** copy `starfish_v2_proposal/docs/starfish/clocking.md` into the repo
(e.g. `docs/starfish/clocking.md` and/or `AGENTS.md`) as the canonical clocking
contract — nothing in-tree documents that invariant as crisply — then delete the
`starfish_v2_proposal/` directory to avoid confusion with live code.

---

## Build / Packaging / CI / Tooling

All shell scripts use `set -euo pipefail` and pass `bash -n`; the gaps below are
logic/version-pinning/duplication, not missing `set -e`.

### Major Refactors

- **Duplicated qmlimportscanner/qt-shadow resolution across packagers** — `tools/package-appimage.sh:297` and `tools/build-macos.sh:70` independently reimplement the same fragile dance (locate `qmlimportscanner`, fall back to scraping `build.ninja` with `awk`, build a "qt-shadow" wrapper to work around Nix splitting Qt across outputs). Factor into one `tools/lib/qt-deploy.sh`. **M**
- **Three near-identical mpv `meson setup` + cmake pipelines** — `tools/build-linux-release.sh:28`, `tools/build-macos.sh:16`, `tools/build-linux-dev.sh:25`. A shared library function taking per-platform flag arrays removes ~150 LOC and keeps platforms in sync. **M**
- **Hardcoded soname/version strings in the IPK bundling loop** — `build-ipk.sh:224` pins exact patch versions (`libmpv.so.2.5.0`, `libavcodec.so.62`, `libstdc++.so.6.0.33`, `libpcre2-16.so.0.13.0`, …). Any bump breaks the `cp` or ships a stale symlink. Glob (`libavcodec.so.*`) and derive the symlink chain. **M**
- **App version `0.2.1` duplicated and unsynced** — `CMakeLists.txt:359` (macOS bundle) vs `app/appinfo.json:3` (webOS); no Linux/AppImage version at all. Single `VERSION` file / CMake `project(... VERSION ...)` consumed by all packagers. **S**
- **Static-vs-shared Qt branching scattered across two layers** — `build-ipk.sh:32`/`:109`/`:199`/`:257` + `CMakeLists.txt:226` (incl. the hand-maintained `JELLYFIN_STATIC_QML_PLUGINS` list at `:302`). High maintenance; consider committing to one Qt linkage for the shippable IPK and deleting the unused branch. **L**
- **Slim-ffmpeg overlay is a ~100-line manual deny-list** — `flake.nix:26` (90+ `withX = false` toggles, re-audited every nixpkgs bump, invalidates the binary cache). `tools/package-appimage.sh:159` (`audit_unexpected_bloat`) is a second hand-maintained copy of the same intent. Derive both from one declaration, or reframe as an allow-list. **M**

### Major Tasks

- **No CI build-check for the IPK on PRs** — `.github/workflows/build-artifacts.yml:33`; the `webos-ipk` job is `workflow_dispatch`-only and needs `WEBOS_TOOLCHAIN_ARCHIVE_URL`, so PRs never compile webOS (`TODO.md:18`). At minimum add a CMake-configure dry-run / `bash -n` lint gate on PRs. **M**
- **No on-device verification loop** — `TODO.md:19`; pieces exist (`tools/webos/diagnose.sh`, `kill-stale.sh`, `collect-diagnostics-bundle.sh`) but nothing orchestrates a "build good?" gate (install registration → launch → screenshot). **L**
- **Stale duplicate `build-qt6.sh` (Qt 6.8.3) is dead code** — `tools/webos-native/build-qt6.sh:7`, superseded by `build-qt6-611.sh`, referenced by nothing; the matching `qtbase-6.8.3-*`/`qtwayland-6.8.3-*` patches are orphaned too. Delete. **S**
- **`build-ffmpeg.sh` points at a missing `experiments/` archive** — `tools/webos-native/build-ffmpeg.sh:10` hardcodes `experiments/optionc-webos-ipk/.../ffmpeg-8.1.tar.xz` which doesn't exist; `build-lua.sh`/`build-third-party.sh` are likewise unreferenced. Confirm needed or remove/document. **S**
- **`shell.nix` hardcodes an absolute `/nix/store` path** — `../shell.nix:5` symlinks a specific openapi-generator store path (breaks on other machines / after GC). The flake (`flake.nix:166`) already provides it properly; remove or derive via `pkgs.openapi-generator-cli`. **S**
- **Network downloads with no checksum pinning** — `tools/package-appimage.sh:275` (linuxdeploy `continuous` — floating!), `build-qt6-611.sh:213` (Qt tarballs), `build-lua.sh` (lua.org). Pin versions + verify SHA256. **M**
- **No signing/notarization for DMG or AppImage** — `tools/package-macos-dmg.sh:18` (Gatekeeper will block end users), `tools/package-appimage.sh:414` (unsigned). Needed only if distributing beyond developers. **M**
- **IPK postinst ships broad LS2 permissions** — `packaging/postinst:13`/`:36` (`com.webos.vt.*`, `inbound/outbound:["*"]`, `trustLevel:"dev"`). Fine for sideload; over-broad for a signed/store path. Release-hardening item. **S**
- **macdeployqt step can silently ship an undeployed bundle** — `tools/build-macos.sh:57` guards macdeployqt behind `command -v`; if absent it exits 0 and the DMG packager ships the broken `.app`. Fail explicitly when deployment is expected. **S**

---

## Folded-in existing notes

### Planned features (from `TODO.md`)
- TV show browsing (seasons/episodes) in a shared extensible media model.
- Dedicated item info page (overview, runtime, badges, cast, play/resume) — partly present via `ItemDetailsPage.qml` (see QML §R1).
- Settings page for server/session, cache controls, playback defaults, diagnostics — present but with dead controls (QML §T1).
- mpv **night mode** setting threaded through `PlayerController` as a runtime preset (AF filter exists at `PlayerController.cpp:34` but isn't user-configurable — ties to C++ §T "mpv option block").
- Resume playback, next-up, episode autoplay for TV.
- Richer artwork: backdrops, logos, per-library hero treatments.
- Local persistence for auth/session, continue-watching, richer offline cache (ties to C++ §T "SQLite cache").
- Expand the Qt OpenAPI-generated subset once the recursive `BaseItemDto`/`context` naming issues in `cpp-qt6-client` are resolved (ties to Build tooling).

### User wishlist (from `../human_notes.md`)
- Switch to `openlgtv/buildroot-nc4` (believed done — verify).
- Proper language parsing for tracks — don't hardcode translations; libmpv likely already exposes this.
- Instant subtitle display.
- "Skip back + enable subs" button.
- UI rework (overall polish) — aligns with QML refactors above.
- Faster autoscroll up/down.
- Better audio sync on Starfish; audio/video delay option; fixed audio sync on PC.
- Fancy Kodi-style audio-sync overlay (partly addressed by `PlayerAudioSyncPanel.qml`).

### Existing plans still live
- `docs/player-overlay-refresh-plan.md` — **mostly done** in the working tree; remaining items are small (track-menu empty state, dead-code verification) and folded into QML §T above.

---

## How this was produced / coverage

- **C++ core:** all 33 files in `src/` read in full.
- **QML:** 42 files (~5,528 LOC); largest read in full, rest skimmed.
- **Starfish backend:** all custom files read; v2 proposal + `../STARFISH_DEBUG_LOG.md` + `../STARFISH_PCM_AUDIO.md` reviewed.
- **Build/CI:** `CMakeLists.txt`, `flake.nix`, `build-ipk.sh`, `../shell.nix`, all of `tools/`, `.github/workflows/`, `packaging/` reviewed; all scripts pass `bash -n`.

Out of scope (vendored upstreams): `jellyfin-web/`, the full `mpv/` tree beyond
the Starfish files, ffmpeg, `dovi_tool/`, `xbmc/`, `litefin/`, `jellyfin-mpv-shim/`.
