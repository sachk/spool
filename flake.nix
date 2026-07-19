{
  description = "Jellyfin webOS native build environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/15f4ee454b1dce334612fa6843b3e05cf546efab";
    libplacebo-src = {
      url = "github:haasn/libplacebo/27aa71a97f4daed84916936572fa6a2e1c3eedb7?submodules=1";
      flake = false;
    };
    mpv-src = {
      url = "path:./mpv";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, libplacebo-src, mpv-src, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-darwin" "x86_64-darwin" ];

      libplaceboOverlay = final: prev: {
        libplacebo = prev.libplacebo.overrideAttrs (_: {
          version = "master-27aa71a";
          src = libplacebo-src;
          patches = [];
        });
      };

      ffmpegCapabilities =
        builtins.fromJSON (builtins.readFile ./tools/manifests/ffmpeg-capabilities.json);
      ffmpegConfigureFlags = platform:
        let
          enableEach = kind: values: map (value: "--enable-${kind}=${value}") values;
          platformConfig = ffmpegCapabilities.platforms.${platform};
          protocols = ffmpegCapabilities.protocols ++ platformConfig.protocols;
        in
        ffmpegCapabilities.requiredDisableFlags
        ++ ffmpegCapabilities.commonConfigureFlags
        ++ platformConfig.configureFlags
        ++ map (library: "--enable-${library}") ffmpegCapabilities.libraries
        ++ enableEach "protocol" protocols
        ++ enableEach "demuxer" ffmpegCapabilities.demuxers
        ++ enableEach "parser" ffmpegCapabilities.parsers
        ++ enableEach "decoder" ffmpegCapabilities.decoders
        ++ enableEach "encoder" ffmpegCapabilities.encoders
        ++ enableEach "filter" ffmpegCapabilities.filters
        ++ enableEach "muxer" ffmpegCapabilities.muxers
        ++ enableEach "bsf" ffmpegCapabilities.bitstreamFilters
        ++ enableEach "hwaccel" platformConfig.hardwareAccelerators;
      ffmpegSlimOverlay = final: prev:
        let
          platform = if final.stdenv.isDarwin then "macos" else "linux";
          structuralEnableFlags = [
            "--enable-asm"
            "--enable-fast-unaligned"
            "--enable-hardcoded-tables"
            "--enable-inline-asm"
            "--enable-optimizations"
            "--enable-pic"
            "--enable-pthreads"
            "--enable-rpath"
            "--enable-runtime-cpudetect"
            "--enable-safe-bitstream-reader"
            "--enable-shared"
            "--enable-stripping"
            "--enable-swscale-alpha"
            "--enable-x86asm"
          ];
          keepInheritedFlag = flag:
            !(nixpkgs.lib.hasPrefix "--enable-" flag)
            || builtins.elem flag structuralEnableFlags;
        in {
        ffmpeg-full = (prev.ffmpeg-full.override
          (nixpkgs.lib.genAttrs ffmpegCapabilities.disabledNixFeatures (_: false))).overrideAttrs (old: {
            doCheck = false;
            configureFlags =
              builtins.filter keepInheritedFlag old.configureFlags
              ++ ffmpegConfigureFlags platform;
            postInstall = (old.postInstall or "") + ''
              mkdir -p "$bin/bin" "$data/share/ffmpeg" "$doc/share/doc/ffmpeg" "$man/share/man"
            '';
          });
      };

      qcoroDarwinOverlay = final: prev: {
        qt6Packages = prev.qt6Packages.overrideScope (_qtFinal: qtPrev: {
          qcoro = qtPrev.qcoro.overrideAttrs (old: {
            meta = old.meta // {
              platforms = old.meta.platforms ++ final.lib.platforms.darwin;
            };
          });
        });
      };

      forAllSystems = f:
        nixpkgs.lib.genAttrs systems (system:
          f (import nixpkgs {
            inherit system;
            config.allowUnfree = true;
            overlays = [ libplaceboOverlay ffmpegSlimOverlay qcoroDarwinOverlay ];
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
        expat
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
          qt6.qtwebsockets
          (qmlToolWrappers pkgs)
        ])
        ++ pkgs.lib.optionals pkgs.stdenv.isLinux (with pkgs; [
          gammaray
          qt6.qtwayland
        ]);

      nativeQtPackages = pkgs:
        (with pkgs; [
          qt6.qtbase
          qt6.qtdeclarative
          qt6.qtimageformats
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

        if [ -z "''${WEBOS_SDK_ROOT:-}" ]; then
          repo_sdk="$PWD/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot"
          workspace_sdk="$PWD/../build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot"
          if [ -x "$repo_sdk/bin/arm-webos-linux-gnueabi-gcc" ]; then
            export WEBOS_SDK_ROOT="$repo_sdk"
          elif [ -x "$workspace_sdk/bin/arm-webos-linux-gnueabi-gcc" ]; then
            export WEBOS_SDK_ROOT="$workspace_sdk"
          else
            export WEBOS_SDK_ROOT="$repo_sdk"
          fi
          unset repo_sdk workspace_sdk
        fi

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
        export JELLYFIN_NATIVE_SHELL=1
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
          stagedSourceId = builtins.substring 0 12
            (builtins.hashString "sha256" "${self}-${mpv-src}");
          buildScript =
            if pkgs.stdenv.isDarwin
            then "tools/build-macos.sh"
            else "tools/build-linux-release.sh";
          buildCommand =
            if pkgs.stdenv.isDarwin
            then ''APP_INSTALL="$REPO_ROOT/build/macos/run-install" DEPLOY_APP=0 exec bash ${buildScript}''
            else "exec bash ${buildScript}";
          binaryPath =
            if pkgs.stdenv.isDarwin
            then "build/macos/run-install/jellyfin-native.app/Contents/MacOS/jellyfin-native"
            else "build/linux-release/install/bin/jellyfin-native";
          mpvLibraryPath =
            if pkgs.stdenv.isDarwin
            then "build/macos/mpv-prefix/lib"
            else "build/linux-release/mpv-prefix/lib";
          libraryPathVariable =
            if pkgs.stdenv.isDarwin
            then "DYLD_LIBRARY_PATH"
            else "LD_LIBRARY_PATH";
          qtPluginPath =
            pkgs.lib.makeSearchPath pkgs.qt6.qtbase.qtPluginPrefix
              (nativeQtPackages pkgs);
          qmlImportPath =
            pkgs.lib.makeSearchPath pkgs.qt6.qtbase.qtQmlPrefix
              (nativeQtPackages pkgs);
          nativeRuntimeLibPath = pkgs.lib.makeLibraryPath (nativeRuntimePackages pkgs);
          # Resolves the checkout, optionally builds the selected native
          # variant, then either exits or launches it inside the #native shell.
          # Launch apps never rebuild; build apps are explicit flake targets.
          makeRunner = {
            name,
            launchPrefix ? "",
            cmakeExtraArgs ? "",
            buildRoot ? "",
            buildBeforeRun ? false,
            buildOnly ? false,
          }:
            let
              runnerBinaryPath =
                if buildRoot == "" then binaryPath
                else if pkgs.stdenv.isDarwin
                then "${buildRoot}/run-install/jellyfin-native.app/Contents/MacOS/jellyfin-native"
                else "${buildRoot}/install/bin/jellyfin-native";
              runnerMpvLibraryPath =
                if buildRoot == "" then mpvLibraryPath
                else "${buildRoot}/mpv-prefix/lib";
              buildRootExport = pkgs.lib.optionalString (buildRoot != "")
                ''export BUILD_ROOT="$REPO_ROOT/${buildRoot}"; '';
              runnerBuildCommand =
                if pkgs.stdenv.isDarwin && buildRoot != ""
                then ''APP_INSTALL="$REPO_ROOT/${buildRoot}/run-install" DEPLOY_APP=0 exec bash ${buildScript}''
                else buildCommand;
            in pkgs.writeShellScriptBin name ''
            export PATH="${pkgs.lib.makeBinPath [ pkgs.nix pkgs.bashInteractive pkgs.coreutils pkgs.gnugrep pkgs.gnused ]}:$PATH"
            set -euo pipefail

            FLAKE_SOURCE="${self}"
            MPV_SOURCE="${mpv-src}"

            is_repo_root() {
              [ -f "$1/CMakeLists.txt" ] && [ -f "$1/tools/build-macos.sh" ] && [ -f "$1/mpv/meson.build" ]
            }

            stage_flake_source() {
              cache_base="''${XDG_CACHE_HOME:-$HOME/.cache}/jellyfin-native/nix-run"
              staged="$cache_base/${stagedSourceId}"
              marker="$staged/.jellyfin-staged-source"
              if [ ! -f "$marker" ]; then
                tmp="$cache_base/.${stagedSourceId}.$$"
                rm -rf "$tmp"
                mkdir -p "$cache_base"
                cp -R "$FLAKE_SOURCE/." "$tmp"
                chmod -R u+w "$tmp"
                rm -rf "$tmp/mpv"
                ln -s "$MPV_SOURCE" "$tmp/mpv"
                printf '%s\n' "${stagedSourceId}" > "$tmp/.jellyfin-staged-source"
                rm -rf "$staged"
                mv "$tmp" "$staged"
              fi
              printf '%s\n' "$staged"
            }

            if [ -n "''${JELLYFIN_REPO:-}" ]; then
              REPO_ROOT="$JELLYFIN_REPO"
              if ! is_repo_root "$REPO_ROOT"; then
                echo "error: JELLYFIN_REPO does not point to a usable jellyfin-webos checkout: $REPO_ROOT" >&2
                exit 1
              fi
            elif is_repo_root "$PWD"; then
              REPO_ROOT="$PWD"
            else
              REPO_ROOT="$(stage_flake_source)"
            fi
            cd "$REPO_ROOT"

            BIN="$REPO_ROOT/${runnerBinaryPath}"

            # Strip webOS cross state so native Linux builds do not pick up the
            # old SDK wayland-scanner/cross toolchain.
            scrub='PATH=$(printf %s "$PATH" | tr ":" "\n" | grep -v webos-sdk | paste -sd:); export PATH; unset WEBOS_SDK_ROOT QT_PLUGIN_PATH QML2_IMPORT_PATH QML_IMPORT_PATH'

            if ${if buildBeforeRun then "true" else "false"}; then
              nix develop "$REPO_ROOT#native" -c bash -c "$scrub; ${buildRootExport}export JELLYFIN_CMAKE_EXTRA_ARGS='${cmakeExtraArgs}'; ${runnerBuildCommand}"
            elif [ ! -x "$BIN" ]; then
              echo "error: native app is not built: $BIN" >&2
              echo "build it first with: nix run .#${if buildRoot == "" then "build" else "image-debug-build"}" >&2
              exit 1
            fi

            if ${if buildOnly then "true" else "false"}; then
              exit 0
            fi

            export MPV_LIB="$REPO_ROOT/${runnerMpvLibraryPath}"
            runtime_env='eval "current_lib_path=\"''${${libraryPathVariable}:-}\""; export ${libraryPathVariable}="$MPV_LIB:${nativeRuntimeLibPath}''${current_lib_path:+:$current_lib_path}"; export QT_PLUGIN_PATH="${qtPluginPath}"; export QML2_IMPORT_PATH="${qmlImportPath}"; export QML_IMPORT_PATH="$QML2_IMPORT_PATH"'
            export LC_NUMERIC=C
            exec nix develop "$REPO_ROOT#native" -c bash -c "$scrub; $runtime_env"'; exec ${launchPrefix}"$@"' _ "$BIN" "$@"
          '';

          builder = makeRunner {
            name = "jellyfin-native-build";
            buildBeforeRun = true;
            buildOnly = true;
          };

          runner = makeRunner { name = "jellyfin-native-run"; };
          imageDebugRunner = makeRunner {
            name = "jellyfin-native-image-debug";
            cmakeExtraArgs = "-DJELLYFIN_ARTWORK_ASPECT_DIAGNOSTICS=ON";
            buildRoot = if pkgs.stdenv.isDarwin
              then "build/macos-image-debug"
              else "build/linux-release-image-debug";
          };
          imageDebugBuilder = makeRunner {
            name = "jellyfin-native-image-debug-build";
            cmakeExtraArgs = "-DJELLYFIN_ARTWORK_ASPECT_DIAGNOSTICS=ON";
            buildRoot = if pkgs.stdenv.isDarwin
              then "build/macos-image-debug"
              else "build/linux-release-image-debug";
            buildBeforeRun = true;
            buildOnly = true;
          };


          # GammaRay launches the target and opens its introspection GUI. The
          # nixpkgs `gammaray` probe must match the app's Qt; the #native shell
          # builds the app against nixpkgs Qt, so they line up.
          gammarayRunner = makeRunner {
            name = "jellyfin-native-gammaray";
            # QuickInspector updates its scene-graph model on every render and
            # crashes GammaRay 3.4 during the mpv overlay transition. Keep the
            # default profiling runner stable; use .#gammaray-full for Quick Scenes.
            launchPrefix = "env GAMMARAY_DisabledPlugins=gammaray_quickinspector gammaray ";
          };

          gammarayFullRunner = makeRunner {
            name = "jellyfin-native-gammaray-full";
            launchPrefix = "gammaray ";
          };
        in {
          build = {
            type = "app";
            program = "${builder}/bin/jellyfin-native-build";
          };

          default = {
            type = "app";
            program = "${runner}/bin/jellyfin-native-run";
          };

          image-debug = {
            type = "app";
            program = "${imageDebugRunner}/bin/jellyfin-native-image-debug";
          };

          image-debug-build = {
            type = "app";
            program = "${imageDebugBuilder}/bin/jellyfin-native-image-debug-build";
          };
        } // pkgs.lib.optionalAttrs pkgs.stdenv.isLinux {
          gammaray = {
            type = "app";
            program = "${gammarayRunner}/bin/jellyfin-native-gammaray";
          };

          gammaray-full = {
            type = "app";
            program = "${gammarayFullRunner}/bin/jellyfin-native-gammaray-full";
          };
        });
    };
}
