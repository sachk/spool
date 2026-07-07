# Performance Plan — webOS build

Goal: make cold launch and steady-state performance demonstrate what a native
webOS client can do. Current cold launch: ~1–2 s pre-main (binary + library
load, not visible in logs) + ~2.1 s to event loop = ~3–4 s perceived.

## Measured facts (2026-07-07)

- Toolchain: buildroot GCC 14.2, `arm-webos-linux-gnueabi`, ELF32 ARM,
  soft-float EABI (`-mfloat-abi=softfp`), tuned `-mcpu=cortex-a9`,
  `-mfpu=neon-fp16`, **ARM state (no Thumb-2)** — verified zero Thumb
  functions in `jellyfin-native` and `libmpv`.
- App binary: 27.9 MiB stripped, **non-PIE EXEC**, LTO on
  (`CMAKE_INTERPROCEDURAL_OPTIMIZATION=ON`).
- Staged shared libs are **not stripped**: `libmpv.so` 19 MiB with full
  `.debug_info` (built with `-g` via `HEAPTRACK_UNWIND_FLAGS`),
  `libgcc_s.so.1` 2.7 MiB with debug info. `HEAPTRACK_UNWIND_FLAGS`
  (`-g -fno-omit-frame-pointer -f(asynchronous-)unwind-tables`) applies to app
  + libmpv in every release build.
- libmpv + all ffmpeg libs are `DT_NEEDED` of the app → loaded/relocated
  before `main()`. The app itself calls no ffmpeg API — those links exist only
  in the CMake webOS block.
- `libmpv.so` exports **3454** dynamic functions, of which ~3360 are leakage
  from statically-linked deps (libass `ass_*`, harfbuzz `hb_*`, Rust libdovi +
  Rust stdlib `_ZN…`): meson's `gnu_symbol_visibility: hidden` covers only
  mpv's own sources, not static archives linked in. Bloats `.dynsym`/`.dynstr`
  and slows load/dlopen.
- No LTO in mpv (`b_lto` unset), ffmpeg (no `--enable-lto`), or Qt static
  (no `FEATURE_ltcg`).
- ffmpeg build is already demuxer/audio-decoder-focused (video is Starfish HW),
  libavcodec still 15 MiB.
- Startup log budget (cold-ish launch):
  - pre-main: unmeasured, est. 1–2 s (paging + dynamic linking + ctors)
  - 2→26 ms QGuiApplication
  - 26→788 ms "prepareForUiSurface" span — actually includes sqlite
    `database.initialize`, device id, Discovery/Api construction, then
    QQuickView `show()` (wayland window + EGL init)
  - 788→1882 ms QML load (AOT-cached via qmlcachegen already)
  - 2078 ms event loop entered; warm home cache applied 2071 ms
  - network: syncplay ws 2129 ms, home rows 2555–2650 ms
- BOLT does **not** support 32-bit ARM → PGO + section ordering is the layout
  ceiling.
- 64-bit userspace: kernel can exec aarch64 with a bundled 64-bit userland,
  but Starfish/ACB/luna/wayland-webos/GLES system libs are 32-bit only and
  in-process → 64-bit build is a dead end for this app. Stay armv7.

## 0. Measure first (blocking everything else)

- [ ] Log exec→main latency: at main entry read `/proc/self/stat` field 22
      (starttime) vs `/proc/uptime`; log the delta. Also add an
      `__attribute__((constructor))` timestamp to bracket static-init time.
- [ ] Surface the existing `Diagnostics::Phase` sub-timings
      (database_initialize, prepare_ui_surface, load_qml) into the startup log
      so the 26→788 ms span is attributable.
- [ ] One-off: run the binary over SSH with `LD_DEBUG=statistics` to quantify
      relocation/symbol-lookup cost.
- [ ] Repeat cold vs warm launches (reboot vs relaunch) to split page-cache
      effects from CPU work.

## 1. Free wins (no rebuild of deps, no risk)

- [ ] Strip staged shared libs in `build-ipk.sh` (currently only the app
      binary is stripped): `--strip-unneeded` on everything in `lib/`.
      Expected: −17+ MiB package/install, faster install, slightly faster
      library open.
- [ ] Make `HEAPTRACK_UNWIND_FLAGS` opt-in (empty unless `BUNDLE_HEAPTRACK=1`
      or explicit env): removes `-g` and frame-pointer/unwind overhead from
      release app + libmpv. Expected: 1–3% CPU on armv7, smaller text.
- [ ] Investigate dropping the dead `qtvirtualkeyboard` import
      (`QT_IM_MODULE=qtvirtualkeyboard` while Qt logs it is unsupported
      client-side on this compositor) — likely pure dead weight in binary size
      and QML import time.
- [ ] Remove the failing `customcontext` scenegraph probe (env or config) —
      cosmetic, avoids a plugin-dir scan + warning.

## 2. Codegen flags (one rebuild of everything, low risk)

- [ ] Thumb-2: add `-mthumb` globally (app, mpv, ffmpeg, Qt, third-party).
      Expected: −25–30% text size, equal or better speed (I-cache), faster
      cold paging. Verify ffmpeg NEON `.S` files still assemble (they select
      ARM/Thumb per-file; expected fine).
- [ ] Tune for real cores: `-mcpu=cortex-a53 -mfpu=neon-fp-armv8`
      (keep `-mfloat-abi=softfp` — ABI must not change). 202x TVs are
      A53/A55/A73-class in AArch32 mode; current tuning is Cortex-A9 (2011).
- [ ] `-ffunction-sections -fdata-sections -Wl,--gc-sections` where not
      already on.
- [ ] Consider `-fvisibility=hidden` + version script for libmpv exporting
      only `mpv_*`: shrinks 13k dynamic symbols to ~hundreds → faster dynamic
      linking, better intra-lib optimization. (Moot if §4 dlopen/static path
      is taken for ffmpeg too.)

## 3. LTO across the stack (medium effort)

- [ ] mpv: `-Db_lto=true` in meson setup.
- [ ] ffmpeg: `--enable-lto`.
- [ ] Qt static: rebuild with `-DFEATURE_ltcg=ON`; ensure app link uses
      `gcc-ar`/`gcc-ranlib` and `-flto=<jobs>` so static-archive LTO bytecode
      participates in the final link (this is where static linking really
      pays: cross-Qt dead-code elimination into the 28 MiB binary).
- [ ] Watch link memory/time; use `-flto=N` partitioning.

## 4. Kill the pre-main second (highest launch impact)

Option A (preferred): **dlopen libmpv lazily**.
- [ ] Wrap the small mpv C API surface in a dlopen/dlsym shim (we control
      PlayerController); drop libmpv+ffmpeg from `DT_NEEDED`.
- [ ] Load it on a worker thread right after first frame (or at first
      playback). Removes ~40 MiB (pre-strip) of load/reloc/symbol work from
      exec time. Playback start cost is hidden in UI idle.

Option B (alternative): static-link mpv+ffmpeg into the app with LTO +
gc-sections (best total size/locality, but grows the exec that must page in
before the UI — worse for time-to-first-frame than A).

Supporting items:
- [ ] Experiment with lowering `requiredMemory` (300 → e.g. 150): SAM may
      reclaim/kill other apps before exec when the ask is high; measure
      launch-latency difference. Keep runtime memory-pressure handling as the
      safety net.
- [ ] Optional: background readahead thread at startup issuing
      `posix_fadvise(WILLNEED)` sequentially over the app binary (and libs) to
      convert random demand paging into sequential eMMC reads; measure.

## 5. 26→788 ms span (UI surface + pre-window work)

- [ ] Move sqlite open + device-id load off the critical path (thread it;
      nothing before window show needs it synchronously except device id for
      API identity — defer API identity set).
- [ ] Qt RHI pipeline cache: set `QQuickGraphicsConfiguration::
      setPipelineCacheSaveFile/LoadFile` to a persistent app-cache path so
      GLES program binaries survive restarts; warms shader compilation.
- [ ] Fontconfig: point `FONTCONFIG_CACHE`/`XDG_CACHE_HOME` at a persistent
      app dir so the system-font scan caches across launches (system
      libfontconfig is linked); or bundle a minimal font + `QT_QPA_FONTDIR`.

## 6. QML load 788→1882 ms

- Already: qmlcachegen AOT, lazy loading of heavy views.
- [ ] Async `Loader`/incubation for everything not needed for the first
      frame; render a shell skeleton first.
- [ ] Audit import list of the startup path; each module import initializes a
      plugin even when static. Remove qtvirtualkeyboard (see §1).
- [ ] Evaluate `qmltc` for the shell components (compiled C++ QML,
      faster instantiation than cached bytecode) — Qt 6.11 coverage permitting.

## 7. Network content phase

- [ ] `QNetworkAccessManager::connectToHostEncrypted()` to the server at
      t≈0 (preconnect TLS during QML load); enable HTTP/2; persist TLS
      session tickets if the server honors them.
- Warm home payload cache already renders at ~2.07 s — good; network rows
  land ~0.5 s later.

## 8. PGO (biggest compiler-side lever, most effort)

- [ ] GCC instrumented PGO: `-fprofile-generate` build → scripted
      startup+browse+playback scenario on the TV → pull `.gcda` over SSH →
      rebuild with `-fprofile-use -fprofile-partial-training`.
- [ ] Profile-driven function layout (`-freorder-functions`, hot/cold text)
      to cut cold-start page faults. (BOLT unavailable on armv7.)
- [ ] Apply to app + libmpv first; ffmpeg optional.

## 9. Clang/LLVM migration (optional, last)

- Feasible against the same buildroot sysroot (glibc + libstdc++), no
  fundamental blocker; expect low-single-digit % vs GCC 14.2 — do it for the
  tooling (lld `--icf=all`, call-graph section sort, ThinLTO memory profile),
  not raw codegen.
- [ ] If done: `-static-libstdc++ -static-libgcc` + LTO also removes the
      bundled 2.4 MiB libstdc++ / 2.7 MiB libgcc shared objects.

## Non-goals

- 64-bit userspace port (blocked by 32-bit-only proprietary system libs
  in-process: Starfish/libpf, AcbAPI, luna-service2, wayland-webos, GLES).
- BOLT (no 32-bit ARM support).

## Suggested order

1. §0 measurement, §1 free wins (same day)
2. §2 Thumb-2 + tuning rebuild, measure
3. §4A dlopen-lazy libmpv, measure launch
4. §5/§6/§7 app-level startup work
5. §3 LTO stack-wide
6. §8 PGO
7. §9 clang, only if still hungry

## Execution checklist

The three highest-difficulty items (deep API-surface/lifecycle analysis,
linker-semantics, cross-stack ABI/asm risk) were assigned to the top-tier
model session of 2026-07-07; the rest are sized for any later session.

Hardest three (this session):

- [x] §4A dlopen-lazy libmpv: dl shim (`src/player/MpvRuntime.cpp`) providing
      the `mpv_*`/`starfish_*` symbols the app uses, deferred-replay for the
      `starfish_*` callback setters called in the `NativeAppWindow`
      constructor, async preload on first `frameSwapped`, libmpv+ffmpeg+lua
      dropped from app `DT_NEEDED` (verified via readelf).
- [ ] §2 Thumb-2 + cortex-a53/armv8-fpu retune of app, libmpv, ffmpeg
      (`--enable-thumb`, `--cpu=cortex-a53`), lua. (Qt stays ARM-mode until
      its own rebuild — see below.)
- [ ] §2bis libmpv dynamic-export diet: anonymous linker version script
      exporting only `mpv_*` + `starfish_*` (hides ~3360 stray exports from
      libass/harfbuzz/Rust libdovi), verified against the dlopen shim's
      dlsym set.

Remaining (unassigned):

- [ ] §0 exec→main + static-init measurement; surface Diagnostics phase
      timings into the startup log; one-off `LD_DEBUG=statistics` run
- [ ] §1 strip staged shared libs in `build-ipk.sh`
- [ ] §1 make `HEAPTRACK_UNWIND_FLAGS` opt-in for release builds
- [ ] §1 drop the unusable `qtvirtualkeyboard` import / `QT_IM_MODULE`
- [ ] §1 remove the failing `customcontext` scenegraph probe
- [ ] §2/§3 rebuild static Qt with `-mthumb -mcpu=cortex-a53` and
      `-DFEATURE_ltcg=ON` (long build; pair with gcc-ar LTO app link)
- [ ] §3 `-Db_lto=true` for mpv, `--enable-lto` for ffmpeg
- [ ] §4 `requiredMemory` 300→150 launch-latency A/B
- [ ] §4 sequential self-readahead experiment (`posix_fadvise`)
- [ ] §5 move sqlite open off the pre-window critical path
- [ ] §5 persistent RHI pipeline cache (`QQuickGraphicsConfiguration`)
- [ ] §5 persistent fontconfig cache dir (or bundled font + `QT_QPA_FONTDIR`)
- [ ] §6 async Loaders/skeleton first frame; audit startup import list;
      evaluate qmltc for shell components
- [ ] §7 TLS preconnect (`connectToHostEncrypted`) during QML load
- [ ] §8 GCC PGO cycle (needs scripted on-TV profiling runs)
- [ ] §9 clang/lld migration evaluation (lld `--icf=all`, ThinLTO)
