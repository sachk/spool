{
  description = "Jellyfin webOS native build environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/15f4ee454b1dce334612fa6843b3e05cf546efab";
    libplacebo-src = {
      url = "git+https://github.com/haasn/libplacebo?submodules=1&rev=27aa71a97f4daed84916936572fa6a2e1c3eedb7";
      flake = false;
    };
    mpv-src = {
      url = "github:sachk/mpv/1313bdd9aba8eb7014cb02688d45a7b7adc9b3e7";
      flake = false;
    };
    mpv-webos-src = {
      url = "github:sachk/mpv/4528e21ae48575d572dda5b7b952eba2ad0c47d9";
      flake = false;
    };
  };

  outputs = { nixpkgs, libplacebo-src, mpv-src, mpv-webos-src, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-darwin" "x86_64-darwin" ];

      libplaceboOverlay = final: prev: {
        libplacebo = prev.libplacebo.overrideAttrs (_: {
          version = "master-27aa71a";
          src = libplacebo-src;
          patches = [];
        });
      };

      # Slim ffmpeg-full: keep what libmpv needs for playback, but drop bloat
      # that otherwise gets pulled into the Linux/AppImage closure.
      ffmpegSlimOverlay = final: prev: {
        ffmpeg-full = prev.ffmpeg-full.override {
          buildFfplay = false;
          withSdl2 = false;

          # Encoders not needed by a playback-only client.
          withX264 = false;
          withX265 = false;
          withAom = false;
          withSvtav1 = false;
          withVvenc = false;
          withRav1e = false;
          withVpx = false;
          withXavs = false;
          withXavs2 = false;
          withXeve = false;
          withXevd = false;
          withKvazaar = false;
          withFdkAac = false;
          withOpenh264 = false;
          withMp3lame = false;
          withVoAmrwbenc = false;
          withTwolame = false;
          withShine = false;
          withTheora = false;

          # Niche audio codecs.
          withOpenmpt = false;
          withGme = false;
          withModplug = false;
          withCodec2 = false;
          withCelt = false;
          withGsm = false;
          withIlbc = false;
          withLc3 = false;
          withSpeex = false;
          withOpencoreAmrnb = false;
          withOpencoreAmrwb = false;
          withMysofa = false;

          # Niche video codecs / protocols / subsystems.
          withDavs2 = false;
          withUavs3d = false;
          withAribb24 = false;
          withAribcaption = false;
          withZvbi = false;
          withSrt = false;
          withRist = false;
          withSsh = false;
          withRtmp = false;

          # Removable bloat.
          withSamba = false;
          withFlite = false;
          withChromaprint = false;
          withTensorflow = false;
          withWhisper = false;
          withVmaf = false;
          withZmq = false;
          withJxl = false;
          withSvg = false;
          withLcevcdec = false;
          withFrei0r = false;
          withQrencode = false;
          withQuirc = false;
          withOpenjpeg = false;
          withXvid = false;

          # Capture / disc inputs we never use.
          withV4l2 = false;
          withV4l2M2m = false;
          withDvdnav = false;
          withDvdread = false;
          withDc1394 = false;
          withCdio = false;
          withCaca = false;

          # GPU/vendor encode paths.
          withAmf = false;
          withCuda = false;
          withCudaLLVM = false;
          withCudaNVCC = false;
          withNpp = false;
          withNvcodec = false;
          withNvdec = false;
          withNvenc = false;
          withCuvid = false;
          withMfx = false;
          withVpl = false;
          withOpencl = false;
          withOpenal = false;
          withJack = false;
          withLadspa = false;
          withBs2b = false;
        };
      };

      forAllSystems = f:
        nixpkgs.lib.genAttrs systems (system:
          f (import nixpkgs {
            inherit system;
            config.allowUnfree = true;
            overlays = [ libplaceboOverlay ffmpegSlimOverlay ];
          }));

      # Shared build/media dependencies. Intentionally contains no qt6.* packages.
      # The Qt source build must not see nixpkgs Qt through CMAKE_PREFIX_PATH.
      basePackages = pkgs: with pkgs; [
        autoconf
        automake
        bashInteractive
        bison
        cacert
        ccache
        cmake
        curl
        ffmpeg-full
        file
        findutils
        flex
        fontconfig
        freetype
        git
        gnumake
        jq
        lcms2
        libarchive
        libass
        libbluray
        libffi
        libplacebo
        libtool
        libuchardet
        libxkbcommon
        lua5_2
        luajit
        meson
        mujs
        ninja
        nodejs_22
        openapi-generator-cli
        jdk17_headless
        patchelf
        pcre2
        perl
        pkg-config
        python3
        rubberband
        rustup
        unzip
        which
        zlib
        zip
      ];

      sourceLinuxPackages = pkgs: with pkgs; [
        alsa-lib
        appimage-run
        expat
        libICE
        libdrm
        libpulseaudio
        libSM
        libva
        libvdpau
        mesa
        pipewire
        shaderc
        squashfsTools
        spirv-cross
        wayland
        wayland-scanner
        wayland-protocols
        zimg
      ];

      darwinPackages = pkgs: with pkgs; [
        apple-sdk_15
        create-dmg
      ];

      qmlToolWrappers = pkgs:
        pkgs.runCommand "qt-qml-tool-wrappers" { nativeBuildInputs = [ pkgs.makeWrapper ]; } ''
          mkdir -p "$out/bin"
          qml_import_path="${pkgs.qt6.qtdeclarative}/lib/qt-6/qml"
          makeWrapper ${pkgs.qt6.qtdeclarative}/bin/qmllint "$out/bin/qmllint" \
            --prefix QML2_IMPORT_PATH : "$qml_import_path"
          makeWrapper ${pkgs.qt6.qtdeclarative}/bin/qmlformat "$out/bin/qmlformat" \
            --prefix QML2_IMPORT_PATH : "$qml_import_path"
          makeWrapper ${pkgs.qt6.qtdeclarative}/libexec/qmlcachegen "$out/bin/qmlcachegen" \
            --prefix QML2_IMPORT_PATH : "$qml_import_path"
        '';

      # Shell used by tools/webos-native/build-qt6-611.sh. No nixpkgs Qt here.
      sourceBuildPackages = pkgs:
        basePackages pkgs
        ++ pkgs.lib.optionals pkgs.stdenv.isLinux (sourceLinuxPackages pkgs)
        ++ pkgs.lib.optionals pkgs.stdenv.isDarwin (darwinPackages pkgs);

      # Shell used by local native Linux app builds / nix run. This may use
      # nixpkgs Qt, but the Qt source-build script should not be run from it.
      nativePackages = pkgs:
        sourceBuildPackages pkgs
        ++ (with pkgs; [
          qt6.qtbase
          qt6.qtdeclarative
          qt6.qtimageformats
          qt6Packages.qcoro
          qt6.qttools
          qt6.qtvirtualkeyboard
          qt6.qtwebsockets
          gammaray
          (qmlToolWrappers pkgs)
        ])
        ++ pkgs.lib.optionals pkgs.stdenv.isLinux (with pkgs; [
          qt6.qtwayland
        ]);

      nativeQtPackages = pkgs:
        (with pkgs; [
          qt6.qtbase
          qt6.qtdeclarative
          qt6.qtimageformats
          qt6.qtvirtualkeyboard
          qt6.qtwebsockets
        ])
        ++ pkgs.lib.optionals pkgs.stdenv.isLinux (with pkgs; [
          qt6.qtwayland
        ]);

      nativeRuntimePackages = pkgs:
        nativeQtPackages pkgs
        ++ pkgs.lib.optionals pkgs.stdenv.isLinux (sourceLinuxPackages pkgs);

      commonShellHook = pkgs: ''
        export SSL_CERT_FILE=${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt
        export CURL_CA_BUNDLE="$SSL_CERT_FILE"
        export NIX_ENFORCE_PURITY=0

        export WEBOS_SDK_ROOT="''${WEBOS_SDK_ROOT:-$PWD/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot}"

        ${pkgs.lib.optionalString pkgs.stdenv.isLinux ''
          export WAYLAND_PROTOCOLS_DIR="${pkgs.wayland-protocols}/share/wayland-protocols"
        ''}
      '';

      sourceShellHook = pkgs: commonShellHook pkgs + ''
        # Keep the Qt source build hermetic with respect to Qt. mkShell's setup
        # hooks may set broad CMake/QML paths; the build script strips these too,
        # but clearing them here makes interactive diagnostics less confusing.
        unset Qt6_DIR Qt6Core_DIR Qt6Gui_DIR Qt6Widgets_DIR Qt6Qml_DIR Qt6Quick_DIR
        unset Qt6CoreTools_DIR Qt6GuiTools_DIR Qt6WidgetsTools_DIR Qt6QmlTools_DIR
        unset Qt6ShaderTools_DIR Qt6WaylandClient_DIR Qt6WaylandScannerTools_DIR
        unset QT_PLUGIN_PATH QML_IMPORT_PATH QML2_IMPORT_PATH QT_SELECT
        export QT_BUILD_CLEAN_POISONED=1

        if [ -z "''${OPENAPI_GENERATOR_CLI_JAR:-}" ]; then
          OPENAPI_GENERATOR_CLI_JAR="$(${pkgs.findutils}/bin/find ${pkgs.openapi-generator-cli} -type f -name 'openapi-generator-cli*.jar' -print -quit 2>/dev/null || true)"
          export OPENAPI_GENERATOR_CLI_JAR
        fi
      '';

      nativeShellHook = pkgs: commonShellHook pkgs + ''
        # This shell intentionally includes nixpkgs Qt for native Linux/macOS
        # development. Do not use it for tools/webos-native/build-qt6-611.sh.
      '';
    in
    {
      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          packages = sourceBuildPackages pkgs ++ [ (qmlToolWrappers pkgs) ];
          shellHook = sourceShellHook pkgs;
        };

        qt-source = pkgs.mkShell {
          packages = sourceBuildPackages pkgs;
          shellHook = sourceShellHook pkgs;
        };

        native = pkgs.mkShell {
          packages = nativePackages pkgs;
          shellHook = nativeShellHook pkgs;
        };
      });

      apps = forAllSystems (pkgs:
        let
          qtPluginPath =
            pkgs.lib.makeSearchPath pkgs.qt6.qtbase.qtPluginPrefix
              (nativeQtPackages pkgs);
          qmlImportPath =
            pkgs.lib.makeSearchPath pkgs.qt6.qtbase.qtQmlPrefix
              (nativeQtPackages pkgs);
          nativeRuntimeLibPath = pkgs.lib.makeLibraryPath (nativeRuntimePackages pkgs);
          runner = pkgs.writeShellScriptBin "jellyfin-native-run" ''
            export PATH="${pkgs.lib.makeBinPath [ pkgs.nix pkgs.bashInteractive pkgs.coreutils pkgs.gnugrep pkgs.gnused ]}:$PATH"
            set -euo pipefail

            REPO_ROOT="''${JELLYFIN_REPO:-$PWD}"
            if [ ! -f "$REPO_ROOT/CMakeLists.txt" ] || [ ! -d "$REPO_ROOT/mpv" ]; then
              echo "error: run from the jellyfin-webos repo root, or set JELLYFIN_REPO" >&2
              exit 1
            fi
            cd "$REPO_ROOT"

            BIN="$REPO_ROOT/build/linux-release/install/bin/jellyfin-native"

            # Strip webOS cross state so native Linux builds do not pick up the
            # old SDK wayland-scanner/cross toolchain.
            scrub='PATH=$(printf %s "$PATH" | tr ":" "\n" | grep -v webos-sdk | paste -sd:); export PATH; unset WEBOS_SDK_ROOT QT_PLUGIN_PATH QML2_IMPORT_PATH QML_IMPORT_PATH'

            if [ -n "''${JELLYFIN_NO_REBUILD:-}" ] && [ -x "$BIN" ]; then
              :
            else
              nix develop "$REPO_ROOT#native" -c bash -c "$scrub; exec bash tools/build-linux-release.sh"
            fi

            export MPV_LIB="$REPO_ROOT/build/linux-release/mpv-prefix/lib"
            runtime_env='export LD_LIBRARY_PATH="$MPV_LIB:${nativeRuntimeLibPath}''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"; export QT_PLUGIN_PATH="${qtPluginPath}"; export QML2_IMPORT_PATH="${qmlImportPath}"; export QML_IMPORT_PATH="$QML2_IMPORT_PATH"'
            export LC_NUMERIC=C
            exec nix develop "$REPO_ROOT#native" -c bash -c "$scrub; $runtime_env"'; exec "$@"' _ "$BIN" "$@"
          '';
        in {
          default = {
            type = "app";
            program = "${runner}/bin/jellyfin-native-run";
          };
        });
    };
}
