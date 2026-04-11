# TODO

## Next Features

- Add TV show browsing with seasons and episodes, while keeping movies and shows in a shared extensible media model.
- Add a dedicated item info page for movies and shows with overview, runtime, badges, cast, and explicit play/resume actions.
- Add a settings page for server/session behavior, cache controls, playback defaults, and developer diagnostics.
- Add an `mpv` night mode setting in the settings page and thread it through `PlayerController` as a runtime-adjustable video preset.
- Add resume playback, next-up behavior, and episode autoplay for TV content.
- Add richer artwork support: backdrops, logos, and per-library hero treatments.
- Add local persistence for auth/session state, continue-watching, and richer offline cache metadata in SQLite.
- Expand the Qt OpenAPI-generated subset toward more Jellyfin APIs once the upstream generator/schema issues around recursive media models are handled cleanly.

## Build / Runtime Follow-Up

- Replace the current reduced OpenAPI spec generation with a fuller generated client when the recursive `BaseItemDto` and `context` naming issues in `cpp-qt6-client` are resolved.
- Trim and validate the packaged Qt/QML/runtime dependency set against real launch logs on webOS to remove unused modules without dropping required controls plugins.
- Add a non-deploy CI-style build check for `nix-shell --run './jellyfin-webos/build-ipk.sh'`.
- Add an on-device verification loop that confirms install registration, successful launch, and a captured screenshot before considering a build good.
- Align the deploy/install path with webOS dev-manager semantics so home-screen visibility and app-manager registration are verified before runtime debugging.
