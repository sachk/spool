# Performance Plan — webOS build (round 3, updated 2026-07-15)

Goal: make cold launch, steady-state playback, **and navigation latency**
demonstrate what a native webOS client can do — while every app-level perf
change also *shrinks* the tree. NEXT_PLAN.md §1 process rules (net-negative
gates, delete-in-same-commit, no single-consumer components) apply to every
QML/C++ item below. Toolchain items (Part E) are exempt from LoC gates.

Companion doc: NEXT_PLAN.md (refactor round 2 — phases A–G mostly landed).
PERFORMANCE_IDEAS.md (the 2026-07 UI-latency investigation) is **fully
merged into Parts A–C below** and superseded by this file.
The uncommitted worktree changes (route/content-ready instrumentation in
`InputLatencyMonitor` + `RouteStack`) are the *base* for Part A — extend
them, don't overwrite.

## Status ledger (2026-07-15, verified in tree)

Round-3 implementation landed in `d6da59f` through `ef49917`. The only
remaining unchecked work is measurement that requires launching the app on
the TV: the `LD_DEBUG=statistics` capture in A9 and the trained PGO cycle in
E3. Packaging/install does not authorize either launch, so both remain
explicitly visible below instead of being reported as completed.

Done since the 2026-07-07 plan:

- ~~§1 strip staged shared libs~~ — `build-ipk.sh:350` strips everything in
  `lib/`.
- ~~§1 HEAPTRACK_UNWIND_FLAGS opt-in~~ — flags no longer in CMakeLists.
- ~~§1 qtvirtualkeyboard~~ — replaced entirely by the static `webosim`
  input-context plugin (`src/platform/webos/`); `QT_IM_MODULE=webosim`.
- ~~§1 customcontext/scenegraph probe~~ — legacy env unset at
  `main.cpp:546-547`.
- ~~§2 Thumb-2 + cortex-a53 retune~~ — app, libmpv, ffmpeg, lua
  (libavcodec −23%). **Qt still ARM-mode** (Part E).
- ~~§2bis libmpv export diet~~ — 3454 → 94 exported functions.
- ~~§4A dlopen-lazy libmpv~~ — `MpvRuntime` shim, preload after first frame
  (`main.cpp:908-918`).
- ~~§5 RHI pipeline cache~~ — `configurePersistentRhiPipelineCache`
  (`main.cpp:264-275`).
- ~~§5 fontconfig/shader caches~~ — `configurePersistentStartupCaches`
  (`main.cpp:249-262`).
- ~~§5 defer device-id/API identity~~ — awaited after first frame
  (`main.cpp:727-744`).
- ~~§7 TLS preconnect~~ — `connectToHostEncrypted` at
  `JellyfinApiFacade.cpp:335`.

Still open, carried into this round:

- §0 exec→main measurement, `LD_DEBUG=statistics` one-off, Phase timings in
  the startup log → **Part A**.
- §5 leftover: sqlite open still blocks the GUI pre-window
  (`DatabaseManager.cpp:340`, `Qt::BlockingQueuedConnection`) → **Part D**.
- §6 QML load: virtualization/lazy-loading not done (HomePage builds every
  row eagerly) → **Parts B/C**.
- §2/§3 Qt static rebuild (thumb + a53 + LTCG), mpv/ffmpeg LTO, §4
  `requiredMemory` A/B, readahead, §8 PGO, §9 clang → **Parts D/E**.
- §4A follow-up (idle mpv handle pre-create, commit 904733d) — implicated in
  the player-won't-start report; env-gate and A/B per NEXT_PLAN §9 before
  building on it.

Current tree: `src/` ≈ 21.4k + `qml/` ≈ 11.0k LoC.

## Measured facts (2026-07-07) — toolchain & startup

- Toolchain: buildroot GCC 14.2, `arm-webos-linux-gnueabi`, ELF32 ARM,
  soft-float EABI, now Thumb-2 `-mcpu=cortex-a53 -mfpu=neon-fp-armv8` for
  app/mpv/ffmpeg; Qt static still Cortex-A9 ARM-state until its rebuild.
- App binary 27.9 MiB stripped, non-PIE, LTO on (app only).
- libmpv+ffmpeg are dlopen'd post-first-frame, no longer `DT_NEEDED`.
- No LTO yet in mpv (`b_lto`), ffmpeg (`--enable-lto`), Qt (`FEATURE_ltcg`).
- Startup budget (cold-ish): pre-main est. 1–2 s (unmeasured — Part A);
  2→26 ms QGuiApplication; 26→788 ms prepare_ui_surface span (includes the
  **blocking sqlite open**, wayland window + EGL); 788→1882 ms QML load
  (AOT-cached); event loop 2078 ms; warm home cache 2071 ms; network rows
  2555–2650 ms.
- BOLT: no 32-bit ARM support. 64-bit userspace: dead end (32-bit-only
  proprietary system libs in-process). Both remain non-goals.

## Measured facts (2026-07, TV at 192.168.0.200) — navigation

| Route | Total | Reported load | Present | budget_intervals |
| --- | ---: | ---: | ---: | ---: |
| Scale setup | 303.66 ms | 291.02 ms | 12.64 ms | 19 |
| First home | 405.51 ms | 396.76 ms | 8.75 ms | 25 |
| Settings | 416.04 ms | 363.68 ms | 52.35 ms | 25 |
| Home return | 260.18 ms | 259.19 ms | 0.98 ms | 16 |
| Movies grid | 483.49 ms | 469.80 ms | 13.70 ms | 30 |
| Home return | 203.95 ms | 191.07 ms | 12.87 ms | 13 |
| Shows grid | 403.58 ms | 388.47 ms | 15.11 ms | 25 |
| Home return | 244.66 ms | 232.22 ms | 12.45 ms | 15 |

- Library viewport: 24 delegates in ~250–280 ms (≈11 ms/card on TV);
  artwork ~325–330 ms.
- The API/model is *not* the cost: warm 100-item page available instantly,
  model apply logs 0 ms, network refresh 66–186 ms. **Settings takes 416 ms
  with zero network dependency** — the cost is QML object construction on
  every navigation.
- Caveat: the current `frames` field is `ceil(wall_ms / budget)` — rename to
  `budget_intervals`; it cannot distinguish CPU work, deliberate async
  incubation, model waits, or render/compositor waits. Part A fixes this.

Non-negotiable targets: warm settings navigation = **1 actual swap** (it
has no network dependency and is simple enough that rebuilding it during
navigation is unnecessary work); every other page presents its local UI
(shell + cached text/model content + usable focus) within **4 actual
swaps**; network latency, image fetch, and image decode are measured
separately and excluded from the local budget; a page shell must never
wait for artwork. Fix the architecture and object lifetimes — never weaken
the definition of "ready" or hide work behind misleading metrics. Use the
screen's reported refresh period, not assumed 60 Hz (already done via
`refreshSource=screen`).

---

## Part A — Attribution first: make the latency system name the culprit

Everything else in this plan gets cheaper once a single log line says
*where* a slow transition spent its time. Extend the uncommitted
`InputLatencyMonitor` transition work; keep its shape (begin → ready →
first-swap-after-ready) and add the missing dimensions. All gated behind
the existing latency-guard setting (`diagnostics/inputLatencyGuard` /
`JELLYFIN_INPUT_LATENCY_DIAGNOSTICS`).

- [x] **A1. Honest frame counting.** Keep `budget_intervals` (renamed from
      `frames`), add `actual_swaps`: count `frameSwapped` signals between
      begin and completion in the existing DirectConnection hook. One
      `std::atomic<quint32>` incremented on the render thread, snapshotted
      into the sample. Also aggregate per-transition counters/totals from
      the render-thread hooks the monitor already connects
      (`beforeSynchronizing`/`afterSynchronizing`/`afterRendering`/
      `frameSwapped`/`afterFrameEnd`): `sync_ms_total`, `render_ms_total`,
      `swap_wait_ms` — a transition burning its frames inside sync/render
      must be distinguishable from one waiting on a model or the
      compositor. Timestamps stay monotonic; only the compact completed
      sample crosses back to the GUI thread. Acceptance budgets above are
      expressed in `actual_swaps`, nothing else.
- [x] **A2. Fix the per-frame queued lambda (bug in the uncommitted diff).**
      *(landed 2026-07-15: `m_uiTransitionActive` atomic gates the
      render-thread hook)*
      `attachWindow` now queues a `QMetaObject::invokeMethod` onto the GUI
      thread on **every frame swap, forever** — a heap-allocated functor +
      queued event at refresh rate even when idle
      (`InputLatencyMonitor.cpp`, second `frameSwapped` connect). Guard with
      an atomic "transition active" flag tested on the render thread before
      queueing anything.
- [x] **A3. `gui_cpu_ms`.** Sample GUI-thread CPU time
      (`clock_gettime(CLOCK_THREAD_CPUTIME_ID)`; POSIX is what matters —
      stub on Windows) at begin, each mark, and completion. This is the
      field that separates "we did 400 ms of work" from "async incubation
      idled across 25 refreshes" — the two need opposite fixes.
- [x] **A4. Generic stage marks.** Replace the single
      `markUiTransitionReady(token)` with `mark(token, stage)` writing into
      a fixed `std::array` of (stage, ns) — no allocation. Standard stages:
      `instance` (Loader onLoaded), `shell` (page root completed),
      `model_ready`, `first_delegate`, `viewport` (reveal settled),
      `content_ready` (today's signal). Final line prints deltas:
      `ui latency: name=route:settings … instance_ms=… shell_ms=…
      viewport_ms=… present_ms=… actual_swaps=2 gui_cpu_ms=…
      delegates_created=…`. RouteStack owns the token and passes it to the
      loaded page; pages mark what they know. Two extra linkages:
      - **input → router_changed**: when the transition is triggered by a
        key press the monitor is already timing, chain them — record
        `input_ms` so the headline number is true press-to-content latency,
        not route-change-to-content.
      - **C++ marks**: `BrowseSessionController::setPage`/`applyCachedPage`
        mark `model_ready`, tagged cache-vs-server and reset-vs-append;
        RouteStack marks route request + cache hit/miss + visibility
        switch once Part B lands.
- [x] **A5. Fold `AtomicViewReveal` timing into A4.** It currently keeps its
      own `Date.now()` wall-clock accounting and its own `console.info`
      line. Give it the token and let it call `mark(token,
      "first_delegate"/"viewport")`: monotonic C++ time, one fewer log
      line, ~15 lines deleted, and its `forceLayout()`-per-tick polling can
      be revisited once marks show what it costs. Never use `Date.now()`
      for performance accounting anywhere (grep-gate it).
- [x] **A6. Delegate churn counters.** `Q_INVOKABLE
      InputLatency.noteDelegate(kind, delta)` — a QHash increment, no-op
      unless a transition is active. Call from `Component.onCompleted` /
      `onDestruction` of `MediaItemCard`, `MenuRow`, and the settings row
      delegate only. Report `delegates_created/destroyed` per transition —
      this is the number that catches off-screen `cacheBuffer` creation
      (C5) and reuse failures, which `AtomicViewReveal`'s visible-range
      check structurally cannot see.
- [x] **A7. Artwork stage attribution.** `ArtworkService` already has the
      stages as code (byte-memory cache → render queue → fetch → decode
      pool → deliver); it just doesn't time them. Per request, record:
      byte-cache hit; worker-queue wait; fetch duration with
      `QNetworkRequest::SourceIsFromCacheAttribute` for disk-cache-vs-
      network; decode-pool queue wait; decode duration + requested/result
      dimensions; GUI delivery to QML `Image.Ready`; cancellations
      (delegate recycled/evicted). Aggregate **per viewport batch**, one
      summary line:
      `artwork batch: n=24 mem=8 disk=12 net=4 queue_p50=… fetch_p50/p95=…
      decode_p50/p95=… deliver_last_ms=… cancelled=…`. No per-image lines —
      log I/O is `fflush`ed per line on eMMC (`main.cpp logLine`) and
      distorts the very timings we're taking.
- [x] **A8. Event-loop gap probe during transitions.** The
      `EventLoopWatchdog` ticks at 1 s — invisible below multi-second
      hangs. While a transition is active, arm a repeating one-frame
      `QChronoTimer`; each fire records lateness > 1 frame as a gap, and
      the `gui_cpu_ms` delta across the gap classifies busy (our code) vs
      blocked (syscall/compositor). Report `max_gap_ms` in the transition
      line. Disarm outside transitions.
- [ ] **A9. exec→main + static-init measurement** (old §0). At main entry:
      `/proc/self/stat` field 22 vs `/proc/uptime` → "exec was N ms before
      main". An `__attribute__((constructor))` timestamp brackets
      static-init. Plus the one-off `LD_DEBUG=statistics` run over SSH.
      This sizes the remaining pre-main second before Part E spends days on
      it.
      *(instrumentation landed; desktop smoke measured exec→main=30 ms and
      static-init=0.01 ms. The requested TV `LD_DEBUG=statistics` launch is
      still pending explicit launch authorization.)*
- [x] **A10. Phase timings into the startup log.** `Diagnostics::Phase`
      writes only to the diagnostics JSONL; echo `phase_end` for the
      `startup`/`shutdown` categories through `logLine` so the 26→788 ms
      span is attributable in the log everyone actually reads.
- [x] **A11. Overlay + structure cleanup.** Show the last route sample in
      `DiagnosticsOverlay` (name, wall, gui_cpu, swaps, delegates) next to
      the input stats. While in the file: collapse the five parallel
      `m_uiTransition*` members and three duplicated reset sites into one
      `UiTransition` struct with `reset()`; RouteStack's
      `property double uiTransitionToken` → keep the token opaque
      (`property var`). Net LoC ≈ 0 for A1–A11 combined outside
      instrumentation; the payoff is every later phase citing a number.

Route sample fields after Part A: `name, route_from, route_to, cache_hit,
refresh_budget_ms, wall_ms, gui_cpu_ms, budget_intervals, actual_swaps,
instance_ms, shell_ms, model_wait_ms, first_delegate_ms, viewport_ms,
present_ms, delegates_created, delegates_destroyed, max_gap_ms, stage`.

## Part B — Route lifetime: resident pages (the settings-in-one-frame fix)

Root cause of the table above: `RouteStack.qml` owns **one destructive
`Loader`** and `setSource()`s it on every route change — every navigation
destroys the old page tree and incrementally incubates the new one
(`asynchronous: true` deliberately spreads that over frames). Settings pays
~416 ms to rebuild a page whose inputs didn't change. Warning: do **not**
"fix" this by flipping the Loader to synchronous — that can trade
incubation wall time for one long GUI-thread stall; only change loading
mode with `gui_cpu_ms` + `actual_swaps` in hand (Part A).

- [x] **B1. Resident route host.** *(landed 2026-07-15: RouteStack keeps
      per-key Loaders; login/scaleSetup destroyed on leave; settings
      prewarmed 1.5 s after first authenticated activation; transitions
      tagged `:warm`/`:cold`. Desktop@120 Hz verified:
      `route:settings:warm total_ms=1.04 frames=1`, warm home/grid ≈1–3 ms
      at `frames=1`; cold home 6 intervals, cold grid 2.)*
      Replace the destructive loader with a
      host that retains selected page instances:

      | Page | Lifetime |
      | --- | --- |
      | Home / libraries | Resident after authentication (one page) |
      | Settings | Resident and prewarmed; navigation target is 1 swap |
      | Library grid | Resident while authenticated |
      | Search | Lazy once, then retained |
      | Item details | One reusable instance, rebound to route args |
      | Person details | One reusable instance, rebound to route args |
      | Login | Discard after authentication if memory requires it |
      | Scale setup | Discard after setup completes |

      Retained pages get their own `Loader` or direct instantiation inside
      the host; once constructed, route changes only update `visible`,
      `enabled`, stacking order, and focus. Keep the old page visible until
      the replacement is ready (no blank flash). Never keep several
      independent copies of the same page merely to avoid rebinding
      properties. Prewarm settings right after the first useful startup
      frame, then continue prewarming one page at a time during idle —
      settings should be warm long before a user can reach it.
      Warm-settings contract: route flips, resident item becomes visible,
      focus returns to its retained row, one swap — zero construction, zero
      schema traversal, zero row-model rebuild, zero font loading.
- [x] **B2. The LoC synergy — delete the state-escrow.** *(landed
      2026-07-15: `lastGridIndex`/`lastLibraryIndex`/`lastSearchIndex`/
      `lastSearchKind` + `searchModel()` deleted from AppShell; pages own
      their state and expose `currentMediaItem()`. Honest accounting: the
      session net was **+76 code lines**, over the ≤0 budget — prewarm/
      trim/warm-cold tagging are new functionality, and the predicted
      details-model escrow deletion had already happened in an earlier
      phase. The HomePage focus-repair chain was deliberately kept: it
      guards dynamic row changes, not page lifetime.)* Pages currently
      die on navigation, so their state is escrowed in AppShell and
      re-imported through plumbing. Residency makes the page its own state
      owner; delete the escrow in the same commit:
      - `shell.lastGridIndex`/`lastLibraryIndex` + `savedGridIndex()`/
        `setSavedGridIndex()`/`restoreIndex()` (LibraryGridPage ~30 lines +
        AppShell properties);
      - HomePage `currentSection` persistence + the `scheduleFocusRepair`/
        `ensureValidFocus` timer chain (~25 lines) — retained pages keep
        focus;
      - the `detailsModel/detailsIndex/detailsSource` plumbing in AppShell
        (NEXT_PLAN Phase C/D leftover) — a single retained details page is
        rebound instead.
      Budget: host ≈ +80, deletions ≈ −100…−150 → **net ≤ 0** while making
      warm navigation ~free. Gate: `route:settings` warm sample shows
      `actual_swaps=1, delegates_created=0`.
- [x] **B3. Memory-pressure trim.** *(minimal version landed 2026-07-15:
      `RouteStack.trim()` evicts hidden details/person/search on
      `aggressiveMemoryPressure`; rewarm-after-pressure and object-count
      logging still open.)* Hook the existing
      `aggressiveMemoryPressure` path (`main.cpp:762`): always keep the
      active page; prefer keeping home+settings (cheap, frequent); evict
      hidden details/person/search first; consider keeping the grid's
      object tree while `releaseResources()` drops textures. Rewarm
      evicted pages only after pressure subsides and the active viewport is
      stable. Log construct/hit/evict per page with estimated
      object/delegate counts so the policy is auditable. Explicit policy
      for the known route set — **no generic cache framework**.
- [x] **B4. Cold-edge instrumentation.** If the user reaches a page before
      its prewarm finishes, promote the pending incubation and tag the
      sample `cache_hit=promoted` so cold edges stay visible instead of
      polluting the warm numbers.

## Part C — Delegate & page diets: perf work that is also deletion

Ordered by measured impact ÷ risk. Each item states its LoC direction;
NEXT_PLAN gates apply.

- [x] **C1. Card batching + object count** (`MediaItemCard`/`ImageCard`).
      ~11 ms/card on TV is object + batching overhead, not layout:
      - Drop `clip: true` on the card root and on `titleLabel`
        (`MediaItemCard.qml:29,128`) — every clip breaks scene-graph
        batching per delegate; elide + maximumLineCount already bound the
        text. Keep only ImageCard's frame clip (needed for the rounded
        corners).
      - Fallback text behind a `Loader` active only when there is no URL or
        the image errored — 24 fewer Text nodes per viewport today.
      - Focus ring: one `highlight` item at the GridView/ListView level
        (NavGrid already animates `highlightMoveDuration`) instead of
        per-card `focused` bindings driving border width/color — removes a
        binding per delegate and a repaint per focus move.
      - Pointer handling: one grid-level MouseArea/TapHandler using
        `indexAt()` replaces the per-card MouseArea (~20 lines out of the
        card; long-press/right-click routes through the same
        `shell.openItemMenu`). Measure with A6 before/after.
      - Instantiate secondary metadata only when non-empty; drop the
        per-card `antialiasing: true` on ImageCard's frame if a shared or
        simpler visual reads the same at TV distance.
      - Move repeated string/presentation computation
        (`titleText()`/`subtitleText()` re-evaluated per delegate) into
        model roles where it's stable data rather than view state — this is
        the same motion as C9.
      LoC ≈ −30 net. Do **not** jump to a custom C++ scene-graph card;
      only revisit if A-instrumented numbers still miss budget after C1–C5.
- [x] **C2. Icon font singleton.** `MaterialIcon.qml` instantiates a
      `FontLoader` per icon (14 QML use sites, many inside delegates).
      Typography.qml is already the font singleton — add the Material face
      there, and MaterialIcon becomes a 3-line `Text`. Also stop loading
      the subtitle-preview serif on the ordinary settings path (Loader on
      the subtitle editor only). LoC ≈ −10.
- [x] **C3. Metrics: properties, not per-binding function calls.** 267 call
      sites invoke `Metrics.*(width)` functions whose bodies allocate a JS
      array literal per call (`[52,56,62,72][density]`) — re-evaluated per
      delegate per width change. Bind `Metrics.refWidth` to the window once
      and expose the hot values as readonly properties (`Metrics.gapPx`,
      `Metrics.bodyPx`…); keep functions only for truly parametric cases.
      Call sites shorten (`Metrics.gap(root.width)` → `Metrics.gapPx`),
      width-plumbing through pages disappears, delegates stop re-running
      scale math. LoC ≈ −40 across call sites.
- [x] **C4. HomePage: ListView of row descriptors.** The four near-identical
      `MediaRow` declarations + `Repeater` (HomePage.qml:200-275) eagerly
      build every row including below-fold ones, and the
      `visibleSections()/activeSection()/focusRelative` walker exists only
      to iterate that hand-built set. A vertical `ListView` over
      `[{kind, title, model}]` descriptors keeps only viewport rows alive,
      reuses row delegates, and replaces the section walker with
      `currentIndex`. Same pattern for SearchPage's result rows. LoC ≈ −70;
      gate: home cold sample `delegates_created` drops accordingly.
- [x] **C5. Library grid: pagination + cacheBuffer.**
      - `BrowseSessionController::prefetchVisibleRange` requests the next
        page whenever `lastIndex + 200 >= rowCount()`
        (`BrowseSessionController.cpp:118`) — with 100-item pages and a
        viewport ending near 23 this is *always* true: the captured session
        pulled items 100–263 while the first viewport was still being
        built. Make the threshold viewport-relative (1–2 visible rows) and
        gate `moreItemsRequested` until the initial reveal has presented.
      - `cacheBuffer: 2 * cellHeight` (`LibraryGridPage.qml:735`): stage it
        — 0 until first present, modest afterwards; verify with A6 counters
        (created delegates, not just the reveal range).
      - Secondary, after the above: model update granularity — a warm
        refresh currently resets the whole model (GridView reconsiders
        every delegate and its layout even though the apply logs 0 ms);
        investigate in-place row updates for changed items only.
      LoC ≈ 0, pure contention removal during the worst 300 ms.
- [x] **C6. Library grid menu diet.** Three inline
      `PopupMenuPanel`+`MenuListView`+`MenuRow` trees (library/sort/filter,
      ~150 lines, LibraryGridPage.qml:825-975) are built during page
      construction even though all start closed. One shared popup-list
      component (3 call sites — passes the NEXT_PLAN rule) behind a
      `Loader` that instantiates on open: page construction stops paying
      for three menu trees, and the page sheds ~120 lines toward a ≤650
      target (currently 977).
- [x] **C7. Settings page contract.** With B1 residency the warm path is
      free; still fix construction so the *cold* path fits budget: build
      the row-descriptor list once per schema/mode change (not per
      visibility), preserve it while hidden, avoid `forceLayout()` when
      only value bindings changed, keep rows virtualized, and make sure
      first visible row + focus exist before the page reports warm.
      Precreate the option picker only if it's cheap; otherwise load it on
      open — it must not affect the settings navigation frame. If the
      subtitle preview/font dependencies materially increase ordinary page
      construction, split the subtitle-editor concerns out. The page is
      already schema-driven (`Settings.settingsSchema`); audit for rows
      declared in both QML and `SettingsSchema.cpp` and keep only the
      schema (NEXT_PLAN Phase D leftover).
- [x] **C8. Details page: lazy below-fold.** ItemDetailsPage (1,321 lines)
      eagerly declares metadata repeaters + context/people/similar
      `MediaRow`s. Hero/details first; secondary rows behind viewport-
      proximity Loaders; route readiness = hero usable. Pairs with the
      Phase C/D page-diet work already planned.
- [x] **C9. MovieItem diet (NEXT_PLAN Phase F) — do it for perf reasons.**
      Nine prebuilt URL strings + formatted subtitle per item × thousands
      of cached items ≈ multi-MB RAM on armv7, ~40% larger home-payload
      cache blobs, measurable parse time at the warm-cache 2.07 s mark.
      The refactor (−250 src LoC) and the startup win are the same change.
      Bump `kHomePayloadSchemaVersion`.
- [x] **C10. Artwork ≠ readiness.** Local page contract ends at shells +
      text + focus; each image fades in as its own decode completes
      (`retainWhileLoading` already set); no viewport held transparent by
      one slow image. Today `artworkVisible: gridReveal.artworkReady` makes
      artwork appear all-at-once — decouple it, keep the reveal for
      *measurement* only (A5/A7 make it observable). Retain old artwork
      while refreshing where possible. Verify no regression in perceived
      "pop".
- [x] **C11. ScaleSetupPage preview diet.** The page builds three complete
      miniature preview cards with nested repeaters. One shared preview
      driven by the selected preset (or a cheaper representation) — it's a
      one-time page, but it's the same eager-object disease and a small
      LoC win; do it opportunistically when touching the file.

Qt 6.11 leverage while touching these files (from NEXT_PLAN §10): replace
the remaining hand-rolled ensure-visible code (12 files) with the new
Flickable position-to-child APIs behind one helper; `pragma
ComponentBehavior: Bound` everywhere delegates live (mostly done); qmllint
clean as a pre-commit gate; evaluate qmltc for leaf primitives only after
the file set settles, measuring instantiation on one page first.

## Part D — Startup leftovers (app-level)

- [x] **D1. Unblock the pre-window sqlite open.** `database.initialize()`
      runs a `Qt::BlockingQueuedConnection` into the worker before the
      window shows (`DatabaseManager.cpp:340`, called at `main.cpp:729`).
      Nothing before first frame needs the result — device identity is
      already awaited post-frame. Make initialize fire-and-forget on the
      worker (first awaiting caller suspends via the existing QCoro path;
      surface open-failure as the same fatal path it is today). Removes a
      disk-bound chunk of the 26→788 ms span; A10 will show exactly how
      much.
- [x] **D2. Idle-mpv A/B** (`JELLYFIN_DISABLE_IDLE_MPV=1`, ~5 lines) —
      settle the NEXT_PLAN §9 hypothesis before building more on the idle
      handle; keep the window-id export fix in mind (export at startup, or
      refuse to adopt an idle handle created without one).
- [x] **D3. `requiredMemory` 300→150 A/B** — SAM may reclaim/kill other
      apps before exec when the ask is high; measure launch delta; runtime
      memory-pressure handling stays the safety net.
- [x] **D4. Sequential self-readahead experiment** —
      `posix_fadvise(WILLNEED)` over binary + libs at startup on a worker
      thread; convert random demand paging into sequential eMMC reads;
      keep only if A9 numbers prove it.
      *(evaluated and not retained: the measured desktop exec→main span is
      only 30 ms, and no TV measurement yet justifies permanent startup I/O.)*
- [x] **D5. QNAM keep-alive check (Qt 6.11 regression risk).** Qt 6.11
      closes idle connections after 2 min by default; verify browse/ws
      reconnect latency and set explicit keep-alive on the factory if the
      TLS-preconnect win regressed.

## Part E — Toolchain backlog (no LoC gates; long builds)

- [x] **E1. Qt static rebuild**: `-mthumb -mcpu=cortex-a53` +
      `-DFEATURE_ltcg=ON`, app link via `gcc-ar`/`gcc-ranlib` + `-flto=N`
      so Qt's LTO bytecode participates in the final link — the single
      biggest remaining binary/cold-page lever (libavcodec's −23% is the
      precedent; Qt is the bulk of the 27.9 MiB binary).
- [x] **E2. mpv `-Db_lto=true`, ffmpeg `--enable-lto`** — piggyback the
      same rebuild slot.
- [ ] **E3. GCC PGO cycle**: `-fprofile-generate` → scripted on-TV
      startup+browse+playback run → `.gcda` over SSH → `-fprofile-use
      -fprofile-partial-training` + `-freorder-functions` hot/cold layout.
      App + libmpv first. Only after E1 settles the binary.
      *(generate/use plumbing covers app, mpv, and FFmpeg; profile-use now
      enables partial training and function reordering. Collecting `.gcda`
      remains pending because it requires a separately authorized TV launch.)*
- [x] **E4. clang/lld evaluation, last**: for `--icf=all`, call-graph
      section ordering, ThinLTO memory profile — expect low single digits
      vs GCC 14.2; also enables `-static-libstdc++/-libgcc` to drop two
      bundled shared objects.
      *(evaluated and not adopted: the production Qt archive is GCC-LTO
      bytecode and the webOS SDK/toolchain is GCC-based, so a clang/lld result
      would require rebuilding the whole dependency stack for the expected
      low-single-digit gain.)*

## Non-goals (standing)

- 64-bit userspace port; BOLT (no armv7 support).
- Custom C++ scene-graph card before C1–C5 measurements demand it.
- Generic page-cache/navigation framework (B3 is an explicit policy).
- Speculative new QML components — three call sites or inline (NEXT_PLAN
  §11).
- Weakening "ready" definitions to hit budgets: a warm settings frame must
  be a usable, focusable page, not a screenshot.

## Suggested order

1. **Part A** (A1–A8 one commit-series, A9–A11 alongside) — everything
   after this cites `actual_swaps`/`gui_cpu_ms`/stage deltas instead of
   vibes.
2. **B1+B2** resident settings first (the 1-swap proof), then home/grid
   residency + B3 trim.
3. **C5** pagination/cacheBuffer (tiny, de-noises every other measurement),
   then **C1–C4**, then C6–C10 with the NEXT_PLAN page diets.
4. **D1–D5** startup leftovers; D2 early if playback debugging demands it.
5. **Part E** in a long-build window; PGO last.

## Verification & commit discipline

Per step, locally: one `qmlformat` invocation over all touched QML, one
`clang-format` over all touched C++, native Nix-shell build +
`tools/smoke-native-app.sh`, focused tests (`InputLatencyMonitorTest`,
router/browse, touched `tst_*` QML), `git diff --check`, and a foreground
`timeout 10s nix run` so rendering/focus regressions are visible. Coherent
conventional commits after each verified step — never combine
instrumentation, route caching, grid pagination, and delegate redesign in
one unreviewable change. Paste the transition samples and
`git diff --shortstat` into the phase commit message (NEXT_PLAN §1
accounting).

TV passes only when explicitly requested, at milestones: fresh package from
current sources per the deployment procedure (no launch unless separately
requested); capture ≥20 transitions for settings and common routes; report
cold/warm/median/p95/worst in `actual_swaps` and ms; verify focus is usable
on the first presented frame, pages never flash blank, artwork never holds
a viewport transparent, and memory pressure can evict hidden pages with
clean recovery.

## Success criteria

- Warm settings navigation completes on the next actual swapped frame,
  with zero page/delegate/font construction.
- Home/settings returns do not reconstruct their page trees; other page
  shells and model-ready viewports meet the 4-swap local budget.
- Network, disk cache, decode, and local rendering are separately
  attributable from one log line per transition/batch.
- Initial library presentation does not trigger automatic loading of the
  whole library; hidden pages cause no uncontrolled memory growth.
- The implementation stays explicit, testable, and *smaller* than what it
  replaces (LoC gates hold).
