Local patches for third-party source used in the webOS native toolchain.

`qtbase-6.8.3-webos-qstorageinfo-linux.patch`
- Fixes `QStorageInfo` on the webOS ARM sysroot, whose glibc `struct statfs64`
  does not expose `f_flags`.
- Keeps space reporting on `statfs64`, but falls back to `statvfs64().f_flag`
  for readonly detection.

`qtbase-6.8.3-webos-qelfparser.patch`
- Fixes `qelfparser_p.cpp` against the older webOS SDK `elf.h`, which lacks
  the `EM_AARCH64` constant used in a debug switch.

`qtwayland-6.8.3-webos-wayland-version.patch`
- Lowers the Wayland package version gate from `1.15` to `1.11` so QtWayland
  can target the older Wayland stack shipped in the webOS SDK sysroot.
