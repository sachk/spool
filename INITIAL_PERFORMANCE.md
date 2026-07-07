# Initial Performance Measurements — webOS build

Date: 2026-07-07
Target: `root@192.168.0.200` webOS TV
App id: `com.codex.jellyfinwebosnative`
Measured build: staged webOS app from `build/webos/stage/app`, version `0.2.1`

## Scope and caveats

This is the widest measurement pass possible from the current tree and the reachable TV without changing app code, rebuilding dependencies, wiping app data, or rebooting the TV.

Measured:

- Local ELF/package characteristics for the staged webOS app and bundled shared libraries.
- Temporary-copy strip savings for every real staged shared library.
- `readelf` dynamic dependencies, relocations, symbol exports, ARM attributes, and function ISA state.
- Five warm TV close→launch cycles through supported webOS `luna-send` applicationManager calls over `ssh -tt`.
- Three direct on-TV `--smoke-and-exit` runs with `LD_DEBUG=statistics`.
- Five direct on-TV `--version` runs for loader-only/trivial-main timing.

Not measured:

- True cold launch after TV reboot. Rebooting the TV is disruptive and was not required to complete this measurement artifact.
- Exec→`main()` and static-constructor timestamps. The current binary does not yet contain the `/proc/self/stat` field-22 or constructor instrumentation from `PERFORMANCE_PLAN.md §0`.
- `Diagnostics::Phase` JSON sub-timings. Release diagnostics were not active for the installed build; no diagnostics directory was present under the app data tree. Startup logs still expose only coarse phase lines.

Raw evidence saved locally:

- `build/performance/initial/warm-launch-1.log` … `warm-launch-5.log`
- `build/performance/initial/direct-smoke-1.log` … `direct-smoke-3.log`
- `build/performance/initial/direct-smoke-1.stdout` … `direct-smoke-3.stdout`
- `build/performance/initial/measurements.json`

## Executive baseline

Warm app-manager launch is now around a 1-second first-frame path on the measured TV:

- Event loop entered: median **1013 ms**, min **982 ms**, max **1404 ms**.
- First frame swapped: median **1016 ms**, min **992 ms**, max **1413 ms**.
- Warm home payload cache applied: median **1009 ms**, min **978 ms**, max **1400 ms**.
- Home network rows completed: median **1405 ms**, min **1383 ms**, max **1846 ms**.
- Lazy `libmpv` preload completed after first frame: median **1120 ms** absolute, with `dlopen` duration median **110 ms**.

The largest visible startup block remains QML/window construction, not `QGuiApplication`:

- `QGuiApplication` construction: median **10 ms**.
- `prepareForUiSurface` coarse span: median **171 ms**, but outliers at **439 ms** and **631 ms**.
- `prepareForUiSurface` → QML loaded delta: median **759 ms**.
- QML loaded → event loop entered delta: median **85 ms**.
- Event loop entered → first frame swapped delta: median **10 ms**.

The lazy-mpv change is doing what it was designed to do:

- The app binary no longer has `libmpv`, FFmpeg, or Lua in `DT_NEEDED`.
- `libmpv` loads after the first frame; it costs **104–158 ms** on these warm launches and does not block the first frame.

The easiest size win is still stripping staged shared libraries:

- Current staged app tree: **68.42 MiB** real file bytes.
- Current IPK: **30.90 MiB**.
- `--strip-unneeded` on real staged shared libraries would save **15.51 MiB**, almost entirely from `libmpv.so.2.5.0`, `libgcc_s.so.1`, and `libstdc++.so.6.0.33`.

## Warm TV launch timings

Method: each run closed the app with `luna://com.webos.applicationManager/closeByAppId`, waited briefly, launched with `luna://com.webos.applicationManager/launch`, waited ~4.2 s, then captured `/tmp/com.codex.jellyfinwebosnative.log`. The app was closed again after the measurement loop.

All values are milliseconds since the app's startup timer begins at `main()` entry, except `mpv dlopen`, which is the duration logged by `MpvRuntime`.

| Run | QGui constructed | prepareForUiSurface done | QML loaded | warm cache | event loop | first frame | mpv loaded | mpv dlopen | syncplay | home rows done |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 11 | 439 | 1261 | 1342 | 1347 | 1358 | 1517 | 158 | 1385 | 1846 |
| 2 | 12 | 171 | 904 | 981 | 986 | 996 | 1107 | 110 | 1020 | 1405 |
| 3 | 9 | 138 | 897 | 978 | 982 | 992 | 1102 | 108 | 1014 | 1383 |
| 4 | 10 | 631 | 1319 | 1400 | 1404 | 1413 | 1525 | 111 | 1438 | 1820 |
| 5 | 7 | 165 | 926 | 1009 | 1013 | 1016 | 1120 | 104 | 1037 | 1385 |

Summary:

| Metric | Min | Median | Mean | Max |
|---|---:|---:|---:|---:|
| QGui constructed | 7 | 10 | 9.8 | 12 |
| prepareForUiSurface done | 138 | 171 | 308.8 | 631 |
| QML loaded | 897 | 926 | 1061.4 | 1319 |
| warm cache applied | 978 | 1009 | 1142.0 | 1400 |
| event loop entered | 982 | 1013 | 1146.4 | 1404 |
| first frame swapped | 992 | 1016 | 1155.0 | 1413 |
| mpv loaded absolute | 1102 | 1120 | 1274.2 | 1525 |
| mpv dlopen duration | 104 | 110 | 118.2 | 158 |
| syncplay websocket connected | 1014 | 1037 | 1178.8 | 1438 |
| latest Movies row | 1358 | 1389 | 1545.6 | 1825 |
| latest Shows row | 1349 | 1365 | 1534.4 | 1818 |
| next-up row | 1376 | 1395 | 1557.0 | 1835 |
| resume row | 1376 | 1405 | 1566.4 | 1846 |

Derived deltas:

| Delta | Min | Median | Mean | Max |
|---|---:|---:|---:|---:|
| prepareForUiSurface done → QML loaded | 688 | 759 | 752.6 | 822 |
| QML loaded → event loop entered | 82 | 85 | 85.0 | 87 |
| event loop entered → first frame | 3 | 10 | 8.6 | 11 |
| first frame → libmpv loaded | 104 | 111 | 119.2 | 159 |
| event loop entered → resume row | 372 | 416 | 420.0 | 499 |

Observed every app-manager launch:

- `QT_IM_MODULE=qtvirtualkeyboard` still logs: `qtvirtualkeyboard currently is not supported at client-side`.
- The scenegraph still logs `Could not create scene graph context for backend 'customcontext'` on the app-manager launch path.
- `requiredMemory` remains `300` in `appinfo.json`.

## Direct smoke startup

Method: direct SSH execution of the installed binary with `LD_DEBUG=statistics` and `--smoke-and-exit`. This exercises `QGuiApplication`, cache/database setup, webOS UI surface preparation, and QML load, then exits before `showFullScreen()`, app-manager registration, event-loop entry, and first-frame/libmpv preload.

| Run | Remote wall time | prepareForUiSurface done | QML loaded | Loader relocations |
|---:|---:|---:|---:|---|
| 1 | 900 ms | 344 ms | 799 ms | 5560 ordinary, 2014 cache, 97099 relative; final 6075 ordinary |
| 2 | 510 ms | 121 ms | 423 ms | 5560 ordinary, 2014 cache, 97099 relative; final 6073 ordinary |
| 3 | 530 ms | 132 ms | 422 ms | 5560 ordinary, 2014 cache, 97099 relative; final 6073 ordinary |

Summary:

- Remote wall time: median **530 ms**, min **510 ms**, max **900 ms**.
- Direct-smoke `prepareForUiSurface`: median **132 ms**, min **121 ms**, max **344 ms**.
- Direct-smoke QML loaded: median **423 ms**, min **422 ms**, max **799 ms**.
- Direct smoke did not show the `customcontext` scenegraph warning. That warning appeared in all app-manager launches, so it is likely tied to the show/render path rather than QML-only loading.

## Direct loader-only timing

Method: direct SSH execution of the installed binary with `--version`, timing inside the TV via `/proc/uptime` before/after the process. This measures warm dynamic loader + trivial `main()` work, not full app startup. `/proc/uptime` provides centisecond precision on this TV, so values are coarse.

| Mode | Runs | Min | Median | Mean | Max |
|---|---:|---:|---:|---:|---:|
| lazy binding | 5 | 30 ms | 30 ms | 34 ms | 40 ms |
| `LD_BIND_NOW=1` | 5 | 30 ms | 40 ms | 40 ms | 50 ms |
| `LD_DEBUG=statistics` | 5 | 30 ms | 40 ms | 36 ms | 40 ms |

`LD_DEBUG=statistics` for `--version` reported:

- ordinary relocations before main: **5560**
- relocations from cache: **2014**
- relative relocations: **97099**
- final ordinary relocations: **5728**
- final relocations from cache: **2014**

Interpretation: with `libmpv` removed from app `DT_NEEDED`, warm dynamic-loader/trivial-main time is small on this TV. The remaining 97k relative relocations are across the direct dependency closure, especially static-Qt-era system/userland libraries pulled in before `main()`.

## Package and file sizes

| Artifact | Size / count |
|---|---:|
| `build/com.codex.jellyfinwebosnative_0.2.1_arm.ipk` | 30.90 MiB |
| `build/webos/stage/app` real file bytes | 68.42 MiB |
| `build/webos/stage/app` real file count | 18 |
| `build/webos/stage/app` symlink count | 27 |
| Staged app binary, stripped | 27.65 MiB |
| App binary before staging strip | 137.83 MiB |
| Unstripped app debug sections | 96.19 MiB |
| Unstripped app `.symtab` | 3.46 MiB |

Largest staged ELF payloads:

| Artifact | Size | Notes |
|---|---:|---|
| `bin/jellyfin-native` | 27.65 MiB | stripped, non-PIE `EXEC`, no `.debug_*` |
| `lib/libmpv.so.2.5.0` | 17.81 MiB | unstripped, 10.85 MiB debug sections |
| `lib/libavcodec.so.62.28.100` | 10.86 MiB | already stripped |
| `lib/libavfilter.so.11.14.100` | 2.86 MiB | already stripped |
| `lib/libgcc_s.so.1` | 2.63 MiB | unstripped, 2.42 MiB debug sections |
| `lib/libstdc++.so.6.0.33` | 2.33 MiB | not fully stripped, `.symtab` present |
| `lib/libavformat.so.62.12.100` | 1.87 MiB | already stripped |

## Strip-savings dry run

Method: copied each real file in `build/webos/stage/app/lib` to a temporary directory and ran the webOS cross `strip --strip-unneeded`; staged files were not modified.

| Library | Current | After strip | Saved | Saved % |
|---|---:|---:|---:|---:|
| `libmpv.so.2.5.0` | 17.81 MiB | 5.72 MiB | 12.09 MiB | 67.9% |
| `libgcc_s.so.1` | 2.63 MiB | 121.57 KiB | 2.52 MiB | 95.5% |
| `libstdc++.so.6.0.33` | 2.33 MiB | 1.55 MiB | 796.62 KiB | 33.4% |
| `liblua5.2.so.0.0.0` | 154.73 KiB | 114.16 KiB | 40.58 KiB | 26.2% |
| `libpng16.so.16.46.0` | 185.92 KiB | 153.95 KiB | 31.96 KiB | 17.2% |
| `libjpeg.so.8.2.2` | 404.54 KiB | 374.06 KiB | 30.48 KiB | 7.5% |
| `libpcre2-16.so.0.13.0` | 308.25 KiB | 293.50 KiB | 14.75 KiB | 4.8% |
| `libAcbAPI.so.1.0.0` | 7.97 KiB | 5.25 KiB | 2.71 KiB | 34.1% |
| FFmpeg real libs | unchanged | unchanged | 0 B | 0% |

Total measured shared-library saving: **15.51 MiB**.

## Dynamic dependencies and relocation shape

### App binary

`build/webos/stage/app/bin/jellyfin-native`:

- ELF type: `EXEC (Executable file)`, non-PIE.
- Machine: ARM, ELF32.
- ARM attributes: CPU arch v8, Thumb-2 supported, FP for ARMv8. No `Tag_ABI_VFP_args` was emitted, matching the soft-float ABI requirement.
- RPATH: `$ORIGIN/../lib`.
- Direct `DT_NEEDED`: `libEGL.so.1`, `libGLESv2.so.2`, `libwayland-egl.so`, `libwayland-client.so.0`, `libwayland-webos-client.so.1`, `libhelpers.so.2`, `libluna-service2.so.3`, `libdl.so.2`, `libxkbcommon.so.0`, `libglib-2.0.so.0`, `libjpeg.so.8`, `libresolv.so.2`, `libwayland-cursor.so.0`, `libpng16.so.16`, `libfreetype.so.6`, `libfontconfig.so.1`, `libz.so.1`, `librt.so.1`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6`, `ld-linux.so.3`.
- No direct `DT_NEEDED` entry for `libmpv`, `libavcodec`, `libavformat`, `libavfilter`, `libavutil`, `libswresample`, `libswscale`, or `liblua`.
- Dynamic relocations in the app ELF itself: **698** total = **673** `R_ARM_JUMP_SLOT`, **21** `R_ARM_COPY`, **4** `R_ARM_GLOB_DAT`.
- Key sections: `.text` **14.72 MiB**, `.rodata` **11.13 MiB**, `.data` **1.44 MiB**, `.ARM.exidx` **84.81 KiB**, `.dynsym` **10.98 KiB**, `.dynstr` **11.88 KiB**.

### Lazy `libmpv`

`build/webos/stage/app/lib/libmpv.so.2.5.0`:

- Direct `DT_NEEDED`: `libfontconfig`, `libfreetype`, FFmpeg libs, `libdl`, `libunwind`, Starfish/player libs, `libcurl`, `librt`, `liblua5.2`, `libz`, `libasound`, Wayland/xkbcommon, C++ runtime, C runtime.
- Exported dynamic functions: **94**.
- Exported `mpv_*`/`starfish_*` dynamic functions are **94/94 Thumb**.
- Dynamic relocations: **15043** total = **14167** `.rel.dyn`, **876** `.rel.plt`.
- Relocation types: **14062** `R_ARM_RELATIVE`, **876** `R_ARM_JUMP_SLOT`, **52** `R_ARM_ABS32`, **45** `R_ARM_GLOB_DAT`, **8** `R_ARM_TLS_DTPMOD32`.
- Key sections: `.text` **4.06 MiB**, `.rodata` **1.14 MiB**, `.rel.dyn` **110.68 KiB**, `.rel.plt` **6.84 KiB**, `.dynsym` **15.84 KiB**, `.dynstr` **18.72 KiB**, `.symtab` **617.62 KiB**, `.strtab` **652.22 KiB**.
- Debug sections: **10.85 MiB**.

### FFmpeg libs

| Library | Size | Exported funcs | Thumb exports | Relocations | `.text` | `.rodata` |
|---|---:|---:|---:|---:|---:|---:|
| `libavcodec.so.62.28.100` | 10.86 MiB | 177 | 177 | 15706 | 8.18 MiB | 2.26 MiB |
| `libavformat.so.62.12.100` | 1.87 MiB | 155 | 155 | 8800 | 1.31 MiB | 316.70 KiB |
| `libavfilter.so.11.14.100` | 2.86 MiB | 66 | 66 | 19525 | 2.08 MiB | 270.57 KiB |
| `libavutil.so.60.26.100` | 582.19 KiB | 611 | 611 | 1956 | not tabled | not tabled |
| `libswresample.so.6.3.100` | 97.63 KiB | 22 | 22 | 277 | not tabled | not tabled |
| `libswscale.so.9.5.100` | 710.12 KiB | 40 | 40 | 1120 | not tabled | not tabled |

The FFmpeg shared libs are already stripped in the staged app; the strip dry run saved 0 B on them.

## Function ISA state

Dynamic/exported symbols confirm the public boundaries are Thumb where expected, but full unstripped symbol tables show remaining ARM-mode code from static Qt, C++ runtime, and Rust/libdovi code.

| Artifact | Defined funcs inspected | Thumb funcs | ARM funcs | Interpretation |
|---|---:|---:|---:|---|
| unstripped app `build/webos/app/jellyfin-native` | 86948 | 12982 | 73966 | App's own recent objects are Thumb; most linked static Qt/runtime code is still ARM-mode. |
| staged `libmpv.so.2.5.0` full symbols | 10014 | 3459 | 6555 | `mpv_*`/`starfish_*` exports are Thumb; ARM-mode symbols are dominated by Rust/libdovi and other linked code. |
| staged `libavcodec.so.62.28.100` full symbols | 177 | 177 | 0 | Exported FFmpeg functions are Thumb. |
| staged `libstdc++.so.6.0.33` full symbols | 10898 | 0 | 10898 | Bundled libstdc++ is ARM-mode. |
| staged `libgcc_s.so.1` full symbols | 1179 | 0 | 1179 | Bundled libgcc is ARM-mode. |
| staged `liblua5.2.so.0.0.0` full symbols | 190 | 188 | 2 | Lua is effectively Thumb except `_init`/`_fini`-style entries. |

## Build/codegen facts

From `build/webos/app/CMakeCache.txt`:

- `CMAKE_BUILD_TYPE=Release`.
- `CMAKE_INTERPROCEDURAL_OPTIMIZATION=ON` for the app.
- `CMAKE_CXX_FLAGS` includes `-mthumb -mcpu=cortex-a53 -mfpu=neon-fp-armv8`.
- `CMAKE_CXX_FLAGS` still includes heaptrack/unwind defaults: `-fasynchronous-unwind-tables -funwind-tables -fno-omit-frame-pointer -g`.

From `mpv_webos/build/webos-libmpv` metadata:

- Meson buildtype: `release`.
- Meson debug: `false`.
- Meson strip: `false`.
- `b_lto` was not enabled in the measured build.
- Compile database contains `-mthumb`, `-mcpu=cortex-a53`, `-mfpu=neon-fp-armv8`, `-g`, and `-fno-omit-frame-pointer`; no `-flto`.

From `build/ffmpeg-src/ffbuild/config.mak`:

- FFmpeg configured with `--arch=arm --cpu=cortex-a53 --enable-thumb`.
- FFmpeg configured with `--disable-debug`.
- FFmpeg extra CFLAGS still include `-g` from the shared tune/unwind flags, but staged FFmpeg `.so` files are already stripped.
- No `--enable-lto` in the measured FFmpeg configuration.

## Findings against `PERFORMANCE_PLAN.md`

### §0 Measure first

- Existing startup logs are sufficient for coarse launch timing and repeated warm launch statistics.
- Existing logs do not yet expose exec→`main()`, static-constructor time, or `Diagnostics::Phase` sub-timings.
- `LD_DEBUG=statistics` works on the TV, but this loader only emitted relocation counts, not linker wall-time. Direct process timing via `/proc/uptime` gives coarse centisecond wall time.

### §1 Free wins

- Strip staged shared libs: confirmed high-value. Expected saving is **15.51 MiB** in the staged lib directory, dominated by `libmpv` and `libgcc_s`.
- Make `HEAPTRACK_UNWIND_FLAGS` opt-in: still relevant. Release app/mpv compile flags include `-g`, frame pointers, and unwind tables.
- Drop or revisit `QT_IM_MODULE=qtvirtualkeyboard`: still relevant. The unsupported-client-side warning appears on every measured launch.
- Remove failing `customcontext` probe: still relevant. The warning appears on every measured app-manager launch and can land as late as **541 ms** in the current coarse log.

### §2 Codegen flags

- Thumb-2/cortex-a53 tuning is active for app objects, mpv, Lua, and FFmpeg public functions.
- Static Qt and bundled C++ runtime remain ARM-mode in the linked app/runtime. The unstripped app has **73966 ARM-mode functions** vs **12982 Thumb functions**.
- `libstdc++` and `libgcc_s` are entirely ARM-mode in their full symbol tables.

### §3 LTO

- App LTO is on.
- mpv LTO is not on.
- FFmpeg LTO is not on.
- Static Qt LTO/Thumb rebuild is not reflected in the measured binary.

### §4 Kill the pre-main second

- Lazy `libmpv` is active and verified by `DT_NEEDED`: app no longer directly loads `libmpv`, FFmpeg, or Lua before `main()`.
- Lazy `libmpv` cost now appears after first frame: **104–158 ms** `dlopen` duration in measured warm launches.
- Direct `--version` dynamic-loader/trivial-main time is **30–40 ms** warm with lazy binding. This is not a cold app-manager launch, but it shows the direct app dependency closure is no longer a multi-second warm cost.
- `requiredMemory` is still `300`; no A/B measurement was done yet.

### §5/§6 UI and QML path

- Current warm app-manager first frame is dominated by `prepareForUiSurface` + QML load.
- QML load after `prepareForUiSurface` is median **759 ms** on app-manager launches.
- Direct `--smoke-and-exit` QML loaded median is **423 ms**, so app-manager/show/render path and cache state materially affect the visible launch path.
- Persistent fontconfig and shader cache directories already exist under the app root after launches:
  - `.cache/fontconfig/`
  - `.cache/qtshadercache-arm-little_endian-ilp32-eabi/`
  These were observed as directories, not validated for hit rate.

### §7 Network content phase

- Warm home payload appears before event-loop entry or near it: median **1009 ms**.
- Network rows arrive about **0.37–0.50 s** after event loop entry in warm runs.
- Syncplay websocket connects at median **1037 ms**, around first-frame/event-loop time.

## Next measurement changes that would close remaining blind spots

1. Add exec→`main()` and constructor timestamp instrumentation from `PERFORMANCE_PLAN.md §0`.
2. Emit `Diagnostics::Phase` startup durations into the normal startup log even in release builds, at least for:
   - `database_initialize`
   - inner `database::initialize`
   - device id load/save
   - `prepare_ui_surface`
   - `load_qml`
3. Add an app-controlled measurement mode that launches, waits for first frame, records one structured JSON startup summary, and exits cleanly. This would avoid manual log parsing and app-manager relaunch noise.
4. After instrumentation lands, repeat:
   - 5 warm relaunches
   - 1 true cold launch after TV reboot, if acceptable at that time
   - `requiredMemory` 300 vs 150 A/B
5. Run the same measurements after each free win: shared-lib stripping, heaptrack flags opt-in, virtual keyboard env removal, and customcontext probe removal.
