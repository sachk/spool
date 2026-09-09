#!/usr/bin/env bash
set -euo pipefail

# Build the Arch binary package from the portable tarball, using the same
# PKGBUILD that packaging/aur ships. makepkg is what writes a pacman package
# anyone can trust, and it only runs on Arch, so an Arch container is the whole
# of that dependency -- everything else here is copying and checking.
#
# Run tools/package-linux-bundle.sh first; its tarball is the only input.

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTIFACT_DIR="${ARTIFACT_DIR:-$APP_ROOT/dist}"
IMAGE="${SPOOL_ARCH_IMAGE:-archlinux:base-devel}"

if [[ "${SPOOL_ARCH_IN_CONTAINER:-0}" != 1 ]]; then
  runtime="${SPOOL_CONTAINER_RUNTIME:-}"
  if [[ -z "$runtime" ]]; then
    for candidate in docker podman; do
      command -v "$candidate" >/dev/null 2>&1 && { runtime="$candidate"; break; }
    done
  fi
  [[ -n "$runtime" ]] || {
    echo "error: building the Arch package needs docker or podman to run $IMAGE" >&2
    exit 1
  }
  exec "$runtime" run --rm \
    -e SPOOL_ARCH_IN_CONTAINER=1 \
    -e "SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-0}" \
    -v "$APP_ROOT:/spool" -w /spool "$IMAGE" \
    bash tools/package-arch.sh "$@"
fi

# shellcheck source=tools/lib/build-common.sh
source "$APP_ROOT/tools/lib/build-common.sh"
APP_VERSION="$(read_project_version "$APP_ROOT")"
PKGBUILD="$APP_ROOT/packaging/aur/PKGBUILD"
BUNDLE="Spool-for-Jellyfin-${APP_VERSION}-linux-x86_64"
TARBALL="$ARTIFACT_DIR/$BUNDLE.tar.zst"

[[ -f "$TARBALL" ]] || {
  echo "error: portable bundle not found at $TARBALL; run tools/package-linux-bundle.sh first" >&2
  exit 1
}
# The published PKGBUILD names the release it downloads, so a version bump that
# misses it would ship an installer for the previous release.
grep -Fqx "pkgver=$APP_VERSION" "$PKGBUILD" || {
  echo "error: $PKGBUILD does not set pkgver=$APP_VERSION" >&2
  exit 1
}

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
id -u builder >/dev/null 2>&1 || useradd -m builder
cp "$PKGBUILD" "$TARBALL" "$work/"
mkdir -p "$work/out"
chown -R builder "$work"

# The tarball is already beside the PKGBUILD, so nothing is downloaded and the
# release checksum the AUR copy carries has nothing to match yet.
runuser -u builder -- env -C "$work" \
  HOME=/home/builder PKGDEST="$work/out" SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-0}" \
  makepkg --force --nodeps --skipchecksums --noconfirm

package="$(find "$work/out" -maxdepth 1 -name '*.pkg.tar.zst' -print -quit)"
[[ -n "$package" ]] || { echo 'error: makepkg produced no package' >&2; exit 1; }

# A package that installs nothing under either prefix still builds cleanly.
contents="$(bsdtar -tf "$package")"
for entry in usr/bin/spool opt/spool/AppRun usr/share/applications/com.sachk.spool.desktop; do
  grep -Fqx "$entry" <<<"$contents" || {
    echo "error: $entry is missing from $(basename "$package")" >&2
    exit 1
  }
done
pacman --query --info --file "$package"

mkdir -p "$ARTIFACT_DIR"
install -m 0644 "$package" "$ARTIFACT_DIR/"
chown "$(stat -c '%u:%g' "$APP_ROOT")" "$ARTIFACT_DIR/$(basename "$package")"
printf '%s\n' "$ARTIFACT_DIR/$(basename "$package")"
