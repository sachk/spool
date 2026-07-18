# AGENTS.md

## Project

- Jellyfin Native client for webOS and desktop.
- Prefer a smaller, faster, easier-to-extend codebase over compatibility scaffolding or speculative abstractions.
- The app is unreleased: bump/reset caches on schema changes instead of adding migrations or fallback readers.
- `mpv/` is a submodule. Commit mpv changes inside `mpv/`, then commit its pointer here.
- Make small conventional commits as coherent work finishes. Never push unless the user explicitly asks.

## Local Development

- Use `nix develop .#native -c ...` from the repository root (e.g. `cmake --preset linux-dev`, then `cmake --build build/linux-dev/app --target jellyfin-native`).
- Foreground-test the real app with `timeout 10s nix run` — a visible launch catches GUI and runtime failures.
- Batch coherent edits, then run one build and one `qmlformat`/`clang-format` invocation over all touched files; don't build or format file-by-file.

## webOS

- Do not build, install, launch, deploy, or test on a TV unless the user explicitly requests it. "Deploy" means install-only; never launch unless separately asked.
- For a requested deployment, use this exact build-and-install command from the repository root:
  `./build-ipk.sh && nix develop -c bash tools/webos/verify-device.sh --no-launch --host root@192.168.0.200 ./build/com.sachk.tern_0.2.1_arm.ipk`
  `build-ipk.sh` defaults to the complete fresh `all` pipeline. Never invoke the `app`, `stage`, or `package` phases separately.
- The packaged IPK is the only supported way to update the TV app. Never copy binaries or libraries to the TV manually, mutate the installed application tree, or substitute hand-written CMake/staging/install steps.
- If webOS diagnosis is requested, read current logs first and make one targeted change — no speculative deploy loops.
- Never write to TV partitions, patch package metadata, restart system services, or wipe app/user data.
