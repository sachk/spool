#!/usr/bin/env bash

qt_deploy_query_path() {
  local key="$1"

  if command -v qtpaths6 >/dev/null 2>&1; then
    qtpaths6 -query "$key" 2>/dev/null || true
  elif command -v qtpaths >/dev/null 2>&1; then
    qtpaths -query "$key" 2>/dev/null || true
  fi
}

qt_deploy_host_library_dir() {
  local qmake_bin
  qmake_bin="$(command -v qmake || true)"
  [[ -n "$qmake_bin" ]] || return 0
  "$qmake_bin" -query QT_INSTALL_LIBS 2>/dev/null || true
}

qt_deploy_linuxdeploy_qt_shadow() {
  local build_ninja="$1"
  local shadow_bin="$2"

  local qmlscanner
  qmlscanner="$(resolve_qmlimportscanner "$build_ninja")"
  [[ -n "$qmlscanner" ]] || return 0

  local qml_import_dir
  qml_import_dir="$(cd "$(dirname "$qmlscanner")/.." && pwd)/lib/qt-6/qml"
  if [[ ! -d "$qml_import_dir" ]]; then
    printf '%s\n' "$(dirname "$qmlscanner")"
    return 0
  fi

  local qtpaths6_bin qtpaths_bin qmake_bin qt_host_lib_dir
  qtpaths6_bin="$(command -v qtpaths6 || true)"
  qtpaths_bin="$(command -v qtpaths || true)"
  qmake_bin="$(command -v qmake || true)"
  qt_host_lib_dir="$(qt_deploy_host_library_dir)"

  rm -rf "$shadow_bin"
  mkdir -p "$shadow_bin"

  cat >"$shadow_bin/qmlimportscanner" <<EOF
#!/usr/bin/env bash
args=()
for arg in "\$@"; do
  if [[ "\$arg" == *-qtbase-*/lib/qt-6/qml && ! -d "\$arg" ]]; then
    args+=("$qml_import_dir")
  else
    args+=("\$arg")
  fi
done
exec "$qmlscanner" "\${args[@]}"
EOF
  chmod +x "$shadow_bin/qmlimportscanner"

  if [[ -n "$qmake_bin" ]]; then
    cat >"$shadow_bin/qmake" <<EOF
#!/usr/bin/env bash
export LD_LIBRARY_PATH="$qt_host_lib_dir\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
exec "$qmake_bin" "\$@"
EOF
    chmod +x "$shadow_bin/qmake"
    export QMAKE="$shadow_bin/qmake"
  fi

  if [[ -n "$qtpaths6_bin" ]]; then
    cat >"$shadow_bin/qtpaths6" <<EOF
#!/usr/bin/env bash
if [[ "\$1" == "-query" && "\$2" == "QT_INSTALL_QML" ]]; then
  printf '%s\n' "$qml_import_dir"
else
  export LD_LIBRARY_PATH="$qt_host_lib_dir\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
  exec "$qtpaths6_bin" "\$@"
fi
EOF
    chmod +x "$shadow_bin/qtpaths6"
  fi

  if [[ -n "$qtpaths_bin" ]]; then
    cat >"$shadow_bin/qtpaths" <<EOF
#!/usr/bin/env bash
if [[ "\$1" == "-query" && "\$2" == "QT_INSTALL_QML" ]]; then
  printf '%s\n' "$qml_import_dir"
else
  export LD_LIBRARY_PATH="$qt_host_lib_dir\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
  exec "$qtpaths_bin" "\$@"
fi
EOF
    chmod +x "$shadow_bin/qtpaths"
  elif [[ -n "$qtpaths6_bin" ]]; then
    ln -sf qtpaths6 "$shadow_bin/qtpaths"
  fi

  append_colon_path QML_IMPORT_PATH "$qml_import_dir"
  append_colon_path QML2_IMPORT_PATH "$qml_import_dir"
  printf '%s\n' "$shadow_bin"
}

qt_deploy_macdeployqt_shadow() {
  local build_ninja="$1"
  local shadow_root="$2"
  local macdeployqt_bin="$3"

  local qmlscanner
  qmlscanner="$(resolve_qmlimportscanner "$build_ninja")"
  if [[ -z "$qmlscanner" ]]; then
    printf 'error: qmlimportscanner is required for macdeployqt QML deployment\n' >&2
    return 1
  fi
  local qml_import_dir
  qml_import_dir="$(cd "$(dirname "$qmlscanner")/.." && pwd)/lib/qt-6/qml"
  if [[ ! -d "$qml_import_dir" ]]; then
    qml_import_dir="$(qt_deploy_query_path QT_INSTALL_QML)"
  fi

  local shadow_bin="$shadow_root/bin"
  local shadow_libexec="$shadow_root/libexec"
  rm -rf "$shadow_root"
  mkdir -p "$shadow_bin" "$shadow_libexec"
  cp "$macdeployqt_bin" "$shadow_bin/macdeployqt"
  chmod +x "$shadow_bin/macdeployqt"
  cat >"$shadow_libexec/qmlimportscanner" <<EOF
#!/usr/bin/env bash
args=()
while [[ \$# -gt 0 ]]; do
  case "\$1" in
    -importPath)
      if [[ \$# -gt 1 && -n "\$2" ]]; then
        args+=("\$1" "\$2")
      fi
      shift 2
      ;;
    *)
      args+=("\$1")
      shift
      ;;
  esac
done
exec "$qmlscanner" "\${args[@]}"
EOF
  chmod +x "$shadow_libexec/qmlimportscanner"

  local qt_prefix qt_plugins qt_qml
  qt_prefix="$(qt_deploy_query_path QT_INSTALL_PREFIX)"
  qt_plugins="$(qt_deploy_query_path QT_INSTALL_PLUGINS)"
  qt_qml="$qml_import_dir"

  cat >"$shadow_bin/qt.conf" <<EOF
[Paths]
Prefix=${qt_prefix:-$shadow_root}
HostPrefix=$shadow_root
HostLibraryExecutables=$shadow_libexec
LibraryExecutables=$shadow_libexec
Plugins=${qt_plugins:-}
QmlImports=${qt_qml:-}
EOF

  printf '%s\n' "$shadow_bin/macdeployqt"
}
