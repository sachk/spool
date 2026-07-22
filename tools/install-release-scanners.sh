#!/usr/bin/env bash
set -euo pipefail

bin_dir="${1:?usage: install-release-scanners.sh BIN_DIR}"
mkdir -p "$bin_dir"

install_tool() {
  local name="$1" version="$2" sha256="$3"
  local archive="${name}_${version}_linux_amd64.tar.gz"
  local url="https://github.com/anchore/${name}/releases/download/v${version}/${archive}"
  local work
  work="$(mktemp -d)"
  curl --fail --location --proto '=https' --tlsv1.2 "$url" -o "$work/$archive"
  printf '%s  %s\n' "$sha256" "$work/$archive" | sha256sum --check --strict
  tar -xzf "$work/$archive" -C "$work" "$name"
  install -m 0755 "$work/$name" "$bin_dir/$name"
  rm -rf "$work"
}

install_tool syft 1.36.0 0d196c884396f17f9627611f96d02d2b30f184e7ba6db244fcf02fc9446b4424
install_tool grype 0.104.0 862816d0addab60968f9401fe5dbaeaf244bf86a3f759003103c11efe151b31d
