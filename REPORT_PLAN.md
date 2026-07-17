# Evaluation follow-through plan

Status: implementation contract, 2026-07-17
Source: `EVAL_REPORT.md`, verified against the current working tree rather than
accepted as a stale snapshot.

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

## Corrections to the evaluation snapshot

### Already implemented; verify and retain

- **Session WebSocket and remote control:** `SyncPlayController` now opens an
  authenticated `QWebSocket`, reconnects, receives `Play`, `Playstate`, and
  `GeneralCommand`, and sends those commands through `AppController`. It also
  handles server push invalidation. Work remaining: focused protocol tests,
  capability truthfulness, and profile-switch teardown—not a second socket.
- **Thumb-2 rebuild:** `webos_tune_cflags()` supplies
  `-mthumb -mcpu=cortex-a53 -mfpu=neon-fp-armv8`; target Qt base/modules,
  FFmpeg, curl, Lua, and app/dependency builds consume it. Work remaining is an
  assertion/audit so future cache restores cannot regress silently.
- **Staged library stripping:** `build-ipk.sh` already strips the app and all
  staged `.so*` files with `--strip-unneeded`.
- **Subtitle appearance:** the schema and mpv option policy already cover mode,
  language, size, weight, font, colour, shadow, background, position, global
  scale, bitmap smoothing, and HDR brightness. This phase is a semantic/UI
  cleanup plus preview, not a wholesale rewrite.
- **Server discovery/manual probe:** typed addresses now perform a real probe
  and remember successful servers. Preserve that flow while attaching the
  selected server record to the saved account.
- **Input architecture:** `KeyRouter` is the single event boundary. Scroll
  acceleration must extend that protocol or the shared navigation primitive;
  do not add page-local `Keys.onPressed` handlers.

### Confirmed remaining

- SQLite currently stores one `login/*` session; the launch picker renders at
  most that one pseudo-profile.
- the top bar has Settings but no adjacent Switch user action;
- Burn/PGS controls are exposed despite mpv support;
- Linux’s resolved `ffmpeg-full 8.0.1` derivation uses `--enable-gpl` and
  `--enable-version3`; webOS has a manual allowlist but not
  `--disable-everything`; Windows fallback FFmpeg has no explicit common
  licensing contract;
- Qt’s webOS target is currently configured with `INPUT_openssl=no`, so native
  HTTPS trust UX cannot be complete until Qt Network is linked to the pinned
  OpenSSL 3 toolchain libraries;
- future SQLite schema versions fail boot instead of recreating the cache;
- playback URLs still contain `api_key` and tests currently bless the leak;
- direct-play errors do not trigger a one-shot renegotiation;
- no webOS screensaver service subscription exists;
- Linux/macOS artifact jobs smoke-test but do not run `ctest`; there is no
  qmllint gate.

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

## Phase 1 — Account/server profile model and persistence

### Data contract

Add a compact value type (for example `AccountProfile`) containing:

- `profileId`: stable hash/UUID, never a list index;
- `serverId`, `serverName`, canonical `serverUrl`;
- `userId`, `userName`, `accessToken`;
- `avatarTag` or deterministic colour seed;
- `lastUsedAt` and `needsAuthentication`.

Persist a versioned JSON array in the existing SQLite worker or a dedicated
small `profiles` table. Prefer the table if it makes atomic upsert/delete/select
clearer; avoid a new repository/service layer. Because the app is unreleased,
bump/reset schema data rather than introduce migration/fallback readers.
Settings remain device-wide unless a setting is explicitly Jellyfin user
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

Replace the single wide pseudo-profile button with a responsive horizontal
tile shelf:

- heading: **Who’s watching?**; subtitle explains that each tile is a Jellyfin
  account on a specific server;
- tile: large deterministic initial/avatar disc, user name, server name, and a
  smaller elided server host/address; offline/auth-required badge where needed;
- trailing Add account tile with the same geometry;
- focused tile gains a crisp accent outline, subtle raised fill/scale, and a
  high-contrast bottom marker; unfocused tiles never rely on faint outlines;
- context/long-press menu: Sign in again, Edit server label/address, Remove from
  this device; removal requires confirmation;
- left/right navigation, Home/End and mouse click on desktop, no wrapping that
  makes Add unexpectedly jump to the first tile;
- if many pairs exist, horizontal scroll keeps the focused tile visible and
  exposes a compact position indicator.

Retain the existing two-step Add account screen: server first, credentials or
Quick Connect second. Improve it by carrying the selected server name/address
visibly through sign-in and by returning to the pair picker on Back.

### Top-bar action

Add a `person`/`switch_account` icon immediately before the Settings cog, not
buried inside Settings. Its accessible label is **Switch user** and tooltip on
desktop is `Switch user — <user · server>`. Update TopBar’s explicit focus
indexing so D-pad traversal remains deterministic with SyncPlay at the far
right. Keep the Settings action inside Settings as a secondary path.

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
tone-map visualisation, button remapping, and destructive cache actions.
Platform-invalid settings disappear entirely rather than displaying disabled
noise.

### Directional disclosure control

Place **Essential · More · All** in a sticky compact segmented control under
the page title. It is a focusable row before the settings list:

- Left/Right changes level; Down focuses the first/current valid setting;
- Up from the first setting returns to it; Up again returns to TopBar;
- when narrowing the level hides the current row, move focus to the nearest
  preceding visible row and announce the new result count;
- persist level, but default new installs to Essential;
- no category sidebar, duplicated “all settings” route, or nested submenu.

### Rows and popups

Create a cohesive card-list treatment shared by settings and desktop menus:

- 60–72 px scaled rows with a stable icon column, title, wrapping two-line help
  when useful, and a right-side value/control column;
- group cards use a slightly raised surface and spacing instead of a forest of
  borders; focus uses an inside accent bar + raised fill so it cannot clip;
- toggles use an actual pill switch plus On/Off text; select rows show current
  value and chevron; sliders show numeric value, minus/plus buttons, and a
  clearly visible track;
- option pickers anchor on desktop, centre as TV dialogs on webOS, share row
  styling, and retain deterministic Up/Down/Back behaviour;
- hover is quieter than keyboard focus; mouse wheel and keyboard never steal
  active focus unpredictably;
- destructive actions are red-toned and separated from ordinary options.

Use scaled metrics throughout; eliminate hard-coded 42/44 px controls in the
settings and library toolbars where they conflict with UI scale.

Tests include schema filtering, focus preservation while levels change,
conditional settings, select/toggle/slider operation, popup dismissal, and
desktop/webOS visibility.

## Phase 3 — Subtitle semantics, appearance, and preview

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

### Acceleration policy

Adapt the constants and curve from the local Kodi checkout at commit
`1669a7e58d`, `xbmc/guilib/GUIBaseContainer.cpp`:

- acceleration starts after 100 ms of held directional input;
- linearly reaches full rate at 3000 ms;
- minimum rate is 10 items/s;
- maximum is at least 30 items/s, or `itemCount / 7` items/s so very large lists
  can be traversed in about seven seconds;
- accumulate fractional items using elapsed time and cap per-frame elapsed time
  at 50 ms to avoid giant jumps after stalls;
- reset accumulation on release, direction change, focus-zone change, modal
  open, or model reset.

Implement this once in a reusable navigation helper/primitive used by
`NavGrid`, `MenuListView`, and long horizontal media rows where appropriate.
The first press still moves exactly one logical row/card; ordinary taps remain
precise. Reduced motion removes animation, not navigation speed. The central
KeyRouter may add held duration to its target protocol, but no page gets direct
key handlers.

### Scrollbar and alphabet overlay

Add a shared `LibraryScrollIndicator` to the large library grid:

- slim right-edge track with a minimum-size thumb derived from visible/total
  rows and content position;
- thumb becomes brighter/thicker while scrolling or focused; mouse drag works
  on desktop without becoming a required TV focus stop;
- a floating bubble beside the thumb shows the current section: `A`–`Z`, `#`,
  or a numeric/year bucket when the active sort is not name;
- derive the letter from the selected/first visible item’s display/sort title,
  using Unicode first grapheme and locale-aware upper casing; articles should
  follow the server’s `SortName`, not ad-hoc English stripping;
- show the bubble only during accelerated navigation, wheel/drag scrolling, or
  a short settle timeout; do not permanently cover artwork;
- pagination requests continue ahead of accelerated movement and clamp focus to
  loaded rows without issuing one request per repeat event.

When Name sorting is active, optional direct alphabet jumping can use the
existing `alphabet` query support, but only after the indicator works over the
normal paged list. Do not replace smooth traversal with 27 server reloads.

Tests use a fake clock for the acceleration curve and cover 1/10/250/5000-item
models, frame stalls, direction reversal, release reset, Unicode/# labels,
non-name sorts, pagination boundaries, and reduced motion.

## Phase 5 — Network, cache, playback, and privacy hardening

### TLS trust

1. Rebuild target Qt Network with linked OpenSSL 3 from the pinned webOS
   toolchain (`INPUT_openssl=linked`), not firmware ABI discovery. Add a build
   assertion that `QT_CONFIG(ssl)` is true and stage/link only what the static
   app actually requires.
2. Add a small `TlsTrustController` on the shared network manager. On
   `sslErrors`, pause that reply, collect authority/fingerprint/error/issuer,
   and expose one pending decision to QML.
3. Display a blocking but readable trust dialog during server probe/login.
   Trust once calls `ignoreSslErrors` only for that reply. Remember stores
   `(canonical authority, leaf SHA-256 fingerprint, allowed error codes)` and
   applies it only on exact match.
4. Store no blanket `ignoreSslErrors` boolean. Allow removing remembered
   certificates under Advanced > Connections. Artwork and API traffic share
   the trust policy; mpv/curl receives an equivalent per-authority CA/pinned
   certificate policy or playback must report that the server trust is not yet
   usable rather than disable verification globally.
5. Redact certificate payloads only where private; fingerprints and public
   issuer names are safe diagnostics.

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

### Secret-free playback URLs

1. Remove `accessToken` from `PlaybackNegotiation::buildUrl` and every
   `api_key` assertion.
2. Pass `X-Emby-Token`/MediaBrowser authorization to mpv through its HTTP header
   facility for the active load, including HLS manifests, segments, subtitle,
   and trickplay requests as applicable. Clear it on stop/profile switch.
3. Never expose the header through QML, `PlaybackSession::url`, diagnostics, or
   command logging. Extend redaction for `X-Emby-Token`, `Authorization`, and
   mpv header-option renderings as defence in depth.
4. Add tests that inspect negotiated URLs, diagnostics strings, mpv option
   construction, redirects, and profile switching for old-token retention.

### One-shot codec renegotiation

- add an explicit Player failure signal containing item/session, mpv error,
  whether `FILE_LOADED` occurred, and last meaningful position;
- classify decoder/demux/open failures conservatively; network/auth/TLS and
  render-surface failures do not masquerade as codec errors;
- before meaningful progress, stop/report the failed direct-play session once,
  renegotiate with `EnableDirectPlay=false` and stream-copy/transcode enabled,
  then replay at the intended position;
- preserve queue and SyncPlay rules; for SyncPlay, do not independently start
  an unscheduled fallback—coordinate or leave the group with an explanation;
- annotate the replacement session so a second error cannot recurse.

Tests: direct fail → transcode success, fail after progress (no retry), network
fail (no retry), fallback fail (one error), resume/queue preservation.

### Screensaver

Create a small platform controller rather than expand QML:

- webOS subscribes to
  `com.webos.service.tvpower/power/registerScreenSaverRequest` only while
  actively playing; on an Active request, respond to
  `responseScreenSaverRequest` with `ack:false` and the supplied timestamp;
- pause/stop/background teardown unregisters so the normal saver works after
  the TV’s idle timeout; resume re-registers;
- include video, audio, and slideshow progression; static photo display is not
  inhibited indefinitely unless a slideshow timer is advancing;
- subscription calls are idempotent and cleaned before LS2/app teardown;
- desktop attempts Wayland idle-inhibit when available, otherwise logs one
  capability message and remains safe.

Tests cover play/pause/resume/stop and mock LS2 request/response payloads. TV
behaviour remains a later explicit hands-on verification because this plan
does not authorize deployment.

## Phase 6 — Minimal, explicitly LGPL FFmpeg everywhere

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

- **webOS:** add `--disable-everything` and explicit LGPL flags to the existing
  source build, re-enable dependencies discovered by configure, and record the
  final configuration. Preserve Thumb-2/LTO/shared-library choices.
- **Linux:** stop using the resolved `ffmpeg-full` derivation that currently
  enables GPL/version3. Provide an overlay/source derivation with the common
  allowlist and ensure mpv and the AppImage close over that exact FFmpeg.
- **macOS:** use the same LGPL package/allowlist and retain only platform decode
  integrations with compatible licences.
- **Windows:** make the Meson fallback consume explicit FFmpeg project options
  or replace it with the same pinned source/configure allowlist; CI prints and
  audits the effective configuration.

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

### CI

1. After Linux and macOS app builds, run `ctest --test-dir <app build>
   --output-on-failure`; keep Windows’ existing test step.
2. Add a Linux QML lint gate using the built module/import paths. Treat real
   errors as failures; explicitly scoped false positives may be configured,
   not ignored with a blanket success exit.
3. Run QML input tests for KeyRouter, profile picker, Settings disclosure,
   accelerated navigation, and dialogs.
4. Add build-script tests/audits for FFmpeg licence/config, Qt SSL, Thumb-2, and
   token-free artifacts.
5. Keep startup smoke tests, but run tests before packaging so broken artifacts
   are never produced.

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

- [ ] Every launch with saved pairs asks which account/server pair to use.
- [ ] Add account creates a new pair without overwriting existing users.
- [ ] Switch user sits beside Settings and preserves saved pairs.
- [ ] Remote-control WebSocket remains connected only to the active pair and
      its commands/push invalidation are tested.
- [ ] Settings opens in a polished Essential view; More/All progressively
      reveal one list with stable D-pad focus.
- [ ] Rows, sliders, toggles, selectors, dialogs, outlines, and surfaces are
      readable and coherent on TV and native desktop.
- [ ] Smart/Default/Forced/Always/None are explained and behave like Jellyfin;
      burn-in/PGS server policy controls are gone.
- [ ] Subtitle preview accurately reflects text/bitmap/HDR limitations.
- [ ] Held navigation uses the Kodi-derived curve and the library shows a
      transient alphabet/section bubble and usable scrollbar.
- [ ] Self-signed TLS requires a scoped, fingerprinted user decision and works
      consistently for API, artwork, WebSocket, and playback.
- [ ] Invalid/future cache data cannot brick boot.
- [ ] A direct codec failure retries once through server negotiation and cannot
      loop.
- [ ] No access token occurs in a playback URL, QML property, log, diagnostic,
      or retained mpv option after switching profiles.
- [ ] Screensaver is inhibited while actively playing and allowed while paused
      or stopped.
- [ ] Qt/FFmpeg target builds retain Thumb-2; staged libraries are stripped.
- [ ] Every FFmpeg build is allowlist-only and explicitly LGPL; no critical
      feature was silently dropped.
- [ ] CI runs unit/QML tests and qmllint before packaging.
- [ ] Formatting, desktop build, full tests, audits, and the visible timed
      `nix run` launch pass.
- [ ] No TV deployment/launch and no git push occurred.

## UI latitude for implementation

The visual descriptions above are constraints, not pixel-perfect mocks. The
implementing agent may improve typography, spacing, focus motion, grouping,
icons, preview imagery, empty/error states, and desktop hover/context behaviour
when the result is simpler and measurably clearer. Preserve the product rules:
one settings route, progressive disclosure, TV-safe focus, account/server pair
identity, high-contrast focus, and no decorative work that adds latency or
duplicate components.
