#!/usr/bin/env bash

webos_sdk_compat_die() {
  printf 'error: %s\n' "$*" >&2
  return 1
}

webos_sdk_nix_dynamic_linker() {
  if [[ -n "${NIX_CC:-}" && -r "$NIX_CC/nix-support/dynamic-linker" ]]; then
    cat "$NIX_CC/nix-support/dynamic-linker"
    return 0
  fi

  if [[ -e /lib64/ld-linux-x86-64.so.2 ]]; then
    printf '%s\n' /lib64/ld-linux-x86-64.so.2
    return 0
  fi

  return 1
}

webos_sdk_compiler_works() {
  local cxx="$1"
  local object
  object="$(mktemp)"
  rm -f "$object"

  if printf 'int main() { return 0; }\n' \
      | "$cxx" -x c++ -c -o "$object" - >/dev/null 2>&1; then
    rm -f "$object"
    return 0
  fi

  rm -f "$object"
  return 1
}

ensure_webos_sdk_host_tools() {
  local sdk_root="$1"
  local sdk_bin="$sdk_root/bin"
  local cxx="$sdk_bin/arm-webos-linux-gnueabi-g++"

  [[ -x "$cxx" ]] || webos_sdk_compat_die "Missing webOS SDK compiler under $sdk_bin"

  if webos_sdk_compiler_works "$cxx"; then
    return 0
  fi

  command -v patchelf >/dev/null 2>&1 || \
    webos_sdk_compat_die "webOS SDK compiler cannot run and patchelf is not in PATH"

  local dynamic_linker
  dynamic_linker="$(webos_sdk_nix_dynamic_linker)" || \
    webos_sdk_compat_die "webOS SDK compiler cannot run; enter nix develop so NIX_CC exposes a dynamic linker"

  local rpath=""
  add_rpath_dir() {
    local dir="$1"
    [[ -n "$dir" && -d "$dir" ]] || return 0
    case ":$rpath:" in
      *":$dir:"*) ;;
      *) rpath="${rpath:+$rpath:}$dir" ;;
    esac
  }

  add_rpath_dir "$(dirname "$dynamic_linker")"
  add_rpath_dir "$sdk_root/lib"
  add_rpath_dir "$sdk_root/lib64"

  local lib
  if command -v cc >/dev/null 2>&1; then
    lib="$(cc -print-libgcc-file-name 2>/dev/null || true)"
    [[ "$lib" = /* ]] && add_rpath_dir "$(dirname "$lib")"
  fi
  if command -v c++ >/dev/null 2>&1; then
    lib="$(c++ -print-file-name=libstdc++.so 2>/dev/null || true)"
    [[ "$lib" = /* ]] && add_rpath_dir "$(dirname "$lib")"
  fi
  if [[ -n "${NIX_CC:-}" && -r "$NIX_CC/nix-support/orig-libc" ]]; then
    lib="$(cat "$NIX_CC/nix-support/orig-libc")"
    add_rpath_dir "$lib/lib"
  fi

  local expect_rpath=0 token
  for token in ${NIX_LDFLAGS:-}; do
    if [[ "$expect_rpath" == "1" ]]; then
      add_rpath_dir "$token"
      expect_rpath=0
      continue
    fi
    case "$token" in
      -L/*) add_rpath_dir "${token#-L}" ;;
      -Wl,-rpath,/*) add_rpath_dir "${token#-Wl,-rpath,}" ;;
      -rpath) expect_rpath=1 ;;
    esac
  done

  while IFS= read -r lib; do
    add_rpath_dir "$(dirname "$lib")"
  done < <(
    find "$sdk_root" \
      -path "$sdk_root/arm-webos-linux-gnueabi" -prune -o \
      -type f \( -name 'libstdc++.so*' -o -name 'libgcc_s.so*' -o -name 'libz.so*' -o -name 'libgmp.so*' -o -name 'libmpfr.so*' -o -name 'libmpc.so*' \) \
      -print 2>/dev/null
  )

  [[ -n "$rpath" ]] || webos_sdk_compat_die "could not build an RPATH for webOS SDK host tools"

  printf 'Patching webOS SDK host tools for NixOS dynamic linking...\n' >&2
  local tool real_tool interp patched=0
  while IFS= read -r tool; do
    real_tool="$(readlink -f "$tool" 2>/dev/null || printf '%s\n' "$tool")"
    [[ -f "$real_tool" ]] || continue
    case "$real_tool" in
      "$sdk_root"/*) ;;
      *) continue ;;
    esac
    interp="$(patchelf --print-interpreter "$real_tool" 2>/dev/null || true)"
    [[ -n "$interp" ]] || continue
    chmod u+w "$real_tool"
    patchelf --set-interpreter "$dynamic_linker" --set-rpath "$rpath" "$real_tool"
    patched=$((patched + 1))
  done < <(
    find \
      "$sdk_root/bin" \
      "$sdk_root/libexec" \
      "$sdk_root/lib/gcc" \
      "$sdk_root/arm-webos-linux-gnueabi/bin" \
      \( -type f -o -type l \) -perm /111 -print 2>/dev/null
  )

  [[ "$patched" -gt 0 ]] || webos_sdk_compat_die "found no dynamically linked webOS SDK host tools to patch"

  webos_sdk_compiler_works "$cxx" || \
    webos_sdk_compat_die "webOS SDK compiler still cannot compile after NixOS compatibility patching"
}
