# Contributing

Jellyfin Native welcomes focused bug reports and feature proposals. Before submitting code, open or find an issue for consequential behavior changes so the user-facing contract is clear.

## Privacy

Use synthetic accounts and media. Never post access tokens, passwords, Quick Connect codes or secrets, authorization headers, cookies, raw logs, server URLs or addresses, account names, library names, media titles, or stable Jellyfin item/profile IDs. Use the in-app diagnostics export after reviewing its preview.

## Development

The supported native environment is Nix:

```sh
nix run .#build
nix develop .#native -c ctest --test-dir build/linux-release/app --output-on-failure
```

Keep changes small and cohesive. Add tests for new observable contracts. Run the relevant targeted tests, strict QML lint, and QML import scan. Do not include generated build output.

## Pull requests

Describe the problem, the chosen behavior, affected platforms, privacy/security implications, and exact verification performed. Preserve TV D-pad behavior when changing QML navigation. Platform packaging changes must retain pinned inputs, checksums, license material, and package smoke checks.

By participating, you agree to follow `CODE_OF_CONDUCT.md`.
