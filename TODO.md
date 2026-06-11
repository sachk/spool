# TODO

Status updated: 2026-06-11.

The frontend/build-foundation branch is now reproducible on the self-hosted
NixOS runner. GitHub Actions run `27287301890` passed lint, Linux AppImage, and
webOS IPK jobs. The uploaded IPK was independently verified as a non-empty
archive containing app version `0.2.1` and a 32-bit ARM EABI5 executable.

## Completed

### Build And Dependency Foundation

- [x] Split desktop and webOS mpv sources into `mpv` and `mpv_webos`.
- [x] Select the correct mpv source tree per target.
- [x] Add submodule/flake revision checks, a tracked pre-commit hook, and CI
  enforcement.
- [x] Add a build matrix and checksum-pinned manifests for Qt, webOS
  third-party sources, and AppImage tooling.
- [x] Split the webOS build into fetch, Qt host, Qt target, dependencies, app,
  stage, and package phases.
- [x] Add a single project `VERSION` consumed by CMake, webOS metadata,
  AppImage, and DMG packaging.
- [x] Replace hardcoded webOS shared-library versions with SONAME-derived
  staging and symlinks.
- [x] Replace the local fake QCoro shim with upstream QCoro 0.12.
- [x] Cross-compile upstream QCoro Core and Network for static ARM webOS.
- [x] Pin and cross-compile libdovi with a compatible Rust toolchain.
- [x] Make the LG/buildroot SDK compiler and subordinate tools executable on
  NixOS by patching their interpreter and runtime paths.
- [x] Add memory-aware parallelism for Qt, third-party, QCoro, FFmpeg, and app
  builds.
- [x] Reduce Qt build disk usage and remove completed CI module build trees.
- [x] Derive static QML plugins with `qmlimportscanner`, including the complete
  Qt Virtual Keyboard plugin set.
- [x] Exclude nested build/source fixtures from recursive QML scanning.
- [x] Share native mpv/CMake setup through `tools/lib/build-common.sh`.
- [x] Make local scripts prefer the workspace webOS SDK when the repository
  copy is absent.

### CI And Packaging

- [x] Run cheap shell, Python, whitespace, flake, and QML import checks.
- [x] Build Linux/AppImage and webOS IPK artifacts on the self-hosted runner.
- [x] Restore a pinned webOS buildroot SDK.
- [x] Cache Qt, SDK, third-party, QCoro, ccache, and webOS mpv outputs.
- [x] Save the expensive dependency cache before app packaging.
- [x] Skip dependency builds on an exact cache hit.
- [x] Cancel superseded runs on the same branch.
- [x] Validate IPK archive structure, app metadata, executable permissions, and
  ARM ELF identity before artifact upload.
- [x] Produce and upload a valid
  `com.codex.jellyfinwebosnative_0.2.1_arm.ipk`.

### Frontend And Application Foundation

- [x] Add shared remote/key classification for accept, back, direction, media,
  color, and webOS scan-code events.
- [x] Convert the main custom controls to Qt Quick Templates while retaining
  explicit TV focus and D-pad behavior.
- [x] Make TV Select activate toggle rows.
- [x] Introduce stable item-id routes instead of relying on `model + index`.
- [x] Add named media-item construction instead of fragile positional
  aggregate initialization.
- [x] Add shared async-task helpers and common string/diagnostic utilities.
- [x] Remove stale settings-row index bookkeeping.
- [x] Bind persisted selector state in Settings.
- [x] Add a Libraries entry to top navigation.
- [x] Add core control accessibility metadata and build Qt accessibility
  support for webOS.
- [x] Honor reduced-motion settings for page scrolling and key panel motion.
- [x] Centralize semantic UI colors and pass responsive width into selector and
  slider metrics.
- [x] Parse track language codes through Qt locale data.
- [x] Queue/coalesce webOS video crop updates instead of blocking the decode
  callback thread.
- [x] Encode playback URL query parameters correctly.
- [x] Name Quick Connect polling and retry limits.
- [x] Fix the webOS startup QML property collision.
- [x] Add empty subtitle/audio track menu states.

## Active Queue

### Runtime Correctness

- [ ] Centralize HTTP timeout/retry handling and 401 re-authentication.
  Playback progress/stopped reports must not be silently discarded.
- [ ] Add transcode/remux fallback and derive `PlayMethod` from the selected
  media source instead of forcing DirectPlay with a fixed 140 Mbit ceiling.
- [ ] Replace recoverable playback `qFatal` paths with a playback error surfaced
  to QML.
- [ ] Add SQLite cache schema/versioning, staleness, invalidation, and eviction.
- [ ] Complete SyncPlay time synchronization, buffering coordination, group
  metadata, and error reporting.
- [ ] Replace fixed poster-prefetch limits with viewport-aware prefetching and
  configurable concurrency.
- [ ] Move user-facing mpv options out of the large hardcoded setup block into
  settings/profile configuration.

### Frontend And Navigation

- [ ] Finish route regression coverage for details opened from library, search,
  latest, resume, person, genre, studio, and similar-item views.
- [ ] Move `ItemDetailsPage` fully onto Theme/Metrics and shared icon controls.
- [ ] Give Search and More Like This dedicated controller models; remove the
  client-side visibility-filtered movie-model workaround.
- [ ] Consolidate the duplicated Home horizontal-row implementations and model
  change handlers.
- [ ] Wire or remove remaining dead settings controls, including remote bitrate,
  remux preference, and shortcut enablement.
- [ ] Extend accessibility metadata from core controls to pages, menus, grids,
  and dynamic media content.
- [ ] Add pagination or bounded models for large library/search results.
- [ ] Continue splitting `PlayerController` and `AppController` by
  responsibility.

### Build, CI, And Release

- [ ] Exercise and fix the macOS app/DMG job on a macOS runner; add a
  deterministic headless smoke path.
- [ ] Make macOS packaging fail when deployment tooling is required but
  unavailable.
- [ ] Factor duplicated qmlimportscanner/Qt deployment setup from AppImage and
  macOS packaging into one helper.
- [ ] Decide whether the shippable webOS package supports only static Qt, then
  remove unused shared/static branching.
- [ ] Consolidate the slim-FFmpeg feature declaration and AppImage bloat audit.
- [ ] Add a supported on-device verification command covering install
  registration, launch, log capture, and screenshot without destructive TV
  operations.
- [ ] Validate and trim packaged Qt/QML/runtime modules against real webOS
  launch logs.
- [ ] Harden LS2 permissions for a signed/store release profile.
- [ ] Add AppImage signing and macOS signing/notarization when public
  distribution begins.
- [ ] Expand the generated Qt OpenAPI client after upstream recursive
  `BaseItemDto` and `context` naming issues are resolved.

### Larger Refactors

- [ ] Split `PlayerController` into mpv lifecycle, position tracking, track
  parsing, trickplay, segments, and reporting components.
- [ ] Split `AppController` navigation, session, settings, prefetch, and Quick
  Connect responsibilities.
- [ ] Extract shared behavior from the webOS and desktop
  `NativeAppWindow` implementations.
- [ ] Deduplicate repeated API fetch/cache/prefetch flows.

## Product Follow-Up

- [ ] Complete and harden TV-series browsing, resume, next-up, and episode
  autoplay workflows across all navigation sources.
- [ ] Improve item details with dedicated similar-content data and consistent
  artwork/metadata treatment.
- [ ] Add richer backdrops, logos, and per-library hero artwork.
- [ ] Expand local persistence for session state, continue watching, and
  offline metadata.

## Explicitly Excluded

Do not work on these unless the user re-enables them:

- OSD/subtitle overlay on the Starfish video plane.
- On-TV PCM audio verification and deletion of the old AAC encoder path.
- Salvaging `starfish_v2_proposal/docs/starfish/clocking.md` and deleting the
  stale proposal tree.
