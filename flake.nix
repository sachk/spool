{
  description = "Jellyfin webOS native build environment";
  nixConfig = {
    extra-substituters = [ "https://spool.cachix.org" ];
    extra-trusted-public-keys = [ "spool.cachix.org-1:yx+E3raAGXiyUNoMEscSu9c85b+WbgoV7swqyY8oL6s=" ];
  };


  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    # Nixpkgs 26.11 dropped x86_64-darwin, so Intel macOS builds track the
    # 26.05 darwin branch, which is maintained to the end of 2026.
    nixpkgs-x86-darwin.url = "github:NixOS/nixpkgs/nixpkgs-26.05-darwin";
    libplacebo-src = {
      url = "github:haasn/libplacebo/a7a18af88ff0a17c04840dcb3246047bb6b46df3?submodules=1";
      flake = false;
    };
    qcoro-src = {
      url = "github:danvratil/qcoro/d1b52b5db2ff9560185c39a4ed9f944dc610c235";
      flake = false;
    };
    mpv-src = {
      url = "git+file:./mpv";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, nixpkgs-x86-darwin, libplacebo-src, qcoro-src, mpv-src, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-darwin" "x86_64-darwin" ];

      # Only Intel macOS needs the older branch; every other system stays on
      # the pin the rest of the project is built and tested against.
      nixpkgsFor = system:
        if system == "x86_64-darwin" then nixpkgs-x86-darwin else nixpkgs;

      libplaceboOverlay = final: prev: {
        spoolLibplacebo = prev.libplacebo.overrideAttrs (_: {
          version = "master-a7a18af";
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
        ++ nixpkgs.lib.optional platformConfig.gpl "--enable-gpl"
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
        spoolFfmpeg = (prev.ffmpeg_8-full.override
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
      tailoredQtOverlay = final: prev: {
        spoolQt6 = (prev.qt6.overrideScope (_qtFinal: qtPrev: {
          qtbase = (qtPrev.qtbase.override {
            systemdSupport = false;
            withGtk3 = false;
          }).overrideAttrs (old: {
            propagatedBuildInputs = builtins.filter (input:
              input != final.glib
              && input != final.icu
              && input != final.unixodbc
              && input != final.unixodbcDrivers.mariadb
              && input != final.unixodbcDrivers.psql
              && input != final.unixodbcDrivers.sqlite
              && (!final.stdenv.isLinux || input != final.systemd)
              && input != final.vulkan-headers
              && input != final.vulkan-loader)
              old.propagatedBuildInputs;
            buildInputs = builtins.filter (input:
              input != final.libmysqlclient
              && input != final.libpq
              && (!final.stdenv.isDarwin || input != final.moltenvk))
              old.buildInputs;
            cmakeFlags =
              builtins.filter (flag: flag != "-DQT_FEATURE_vulkan=ON") old.cmakeFlags
              ++ [
                "-DQT_FEATURE_glib=OFF"
                "-DQT_FEATURE_icu=OFF"
                "-DQT_FEATURE_sql_mysql=OFF"
                "-DQT_FEATURE_sql_odbc=OFF"
                "-DQT_FEATURE_sql_psql=OFF"
                "-DQT_FEATURE_vulkan=OFF"
              ];
            postFixup = builtins.replaceStrings [
              ''patchelf --add-rpath "${final.libmysqlclient}/lib/mariadb" $out/lib/qt-6/plugins/sqldrivers/libqsqlmysql.so''
              ''patchelf --add-rpath "${final.vulkan-loader}/lib" --add-needed "libvulkan.so" $out/lib/libQt6Gui.so''
            ] [ "" "" ] (old.postFixup or "");
          });
        })) // {
          # pythonPackages.qt6 expects this secondary package scope.
          override = prev.qt6.override;
        };
      };


      qcoroOverlay = final: prev: {
        spoolQcoro = (prev.qt6Packages.qcoro.override {
          qtbase = final.spoolQt6.qtbase;
          qtwebsockets = final.spoolQt6.qtwebsockets;
          wrapQtAppsHook = final.spoolQt6.wrapQtAppsHook;
        }).overrideAttrs (old: {
          version = "0.13.0";
          src = qcoro-src;

          meta = old.meta // {
            platforms = old.meta.platforms ++ final.lib.platforms.darwin;
          };
        });
      };
      cacheDependencyOverlay = final: prev: {
        # Cached `nix run` packages use the widely substituted nixpkgs build.
        spoolFfmpeg = prev.ffmpeg;
      };

      forAllSystems = f:
        nixpkgs.lib.genAttrs systems (system:
          f (import (nixpkgsFor system) {
            inherit system;
            config.allowUnfree = true;
            overlays = [ libplaceboOverlay ffmpegSlimOverlay tailoredQtOverlay qcoroOverlay ];
          }));
      # Native artifacts use a tailored Qt without ICU, Vulkan, foreign SQL
      # drivers or GTK. Release jobs retain the full build closure in GitHub
      # Actions; Cachix receives only the runtime and development outputs that
      # keep `nix run` from compiling Qt.
      cachePkgsFor = system:
        import (nixpkgsFor system) {
          inherit system;
          config.allowUnfree = true;
          overlays = [ libplaceboOverlay tailoredQtOverlay qcoroOverlay cacheDependencyOverlay ];
        };

      forAllCacheSystems = f:
        nixpkgs.lib.genAttrs systems (system: f (cachePkgsFor system));


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
        spoolFfmpeg
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
        spoolLibplacebo
        libtool
        libuchardet
        libxkbcommon
        lua5_2
        luajit
        meson
        mujs
        ninja
        nodejs_22
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

      dwarfsTools = pkgs: pkgs.stdenvNoCC.mkDerivation {
        pname = "dwarfs-tools";
        version = "0.14.0";
        src = pkgs.fetchurl {
          url = "https://github.com/mhx/dwarfs/releases/download/v0.14.0/dwarfs-0.14.0-Linux-x86_64.tar.xz";
          hash = "sha256-KyU67IIkNDenkT6Kh4lE2Wp6swijhh1ZzkGqoV4UCa0=";
        };
        dontBuild = true;
        installPhase = ''
          runHook preInstall
          mkdir -p "$out"
          cp -R bin share sbin "$out/"
          runHook postInstall
        '';
      };

      sourceLinuxPackages = pkgs: with pkgs; [
        alsa-lib
        appimage-run
        (dwarfsTools pkgs)
        expat
        libICE
        libdrm
        libpulseaudio
        libsecret
        libGL
        libSM
        libva
        libvdpau
        libx11
        libxext
        libxpresent
        libxrandr
        libxscrnsaver
        mesa
        nv-codec-headers-11
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
        libiconvReal
      ];
      # Native release builds do not need the webOS Qt toolchain, JavaScript
      # interpreter, Rust, AppImage emulation or debugger stack. libmpv still
      # enables Lua scripting on Linux and macOS.
      nativeBasePackages = pkgs: with pkgs; [
        bashInteractive
        binutils
        cacert
        ccache
        cmake
        curl
        spoolFfmpeg
        file
        findutils
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
        spoolLibplacebo
        libuchardet
        libxkbcommon
        lua5_2
        meson
        ninja
        patchelf
        pcre2
        perl
        pkg-config
        python3
        rubberband
        unzip
        which
        zlib
        zip
      ];

      nativeLinuxPackages = pkgs:
        (builtins.filter (package: package != pkgs.appimage-run)
          (sourceLinuxPackages pkgs))
        ++ [ pkgs.elfutils pkgs.vulkan-loader ];


      qmlToolWrappers = pkgs: qt:
        pkgs.runCommand "qt-qml-tool-wrappers" { nativeBuildInputs = [ pkgs.makeWrapper ]; } ''
          mkdir -p "$out/bin"
          qml_import_path="${qt.qtdeclarative}/lib/qt-6/qml"
          makeWrapper ${qt.qtdeclarative}/bin/qmllint "$out/bin/qmllint" \
            --prefix QML2_IMPORT_PATH : "$qml_import_path"
          makeWrapper ${qt.qtdeclarative}/bin/qmlformat "$out/bin/qmlformat" \
            --prefix QML2_IMPORT_PATH : "$qml_import_path"
          makeWrapper ${qt.qtdeclarative}/libexec/qmlcachegen "$out/bin/qmlcachegen" \
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
        nativeBasePackages pkgs
        ++ pkgs.lib.optionals pkgs.stdenv.isLinux (nativeLinuxPackages pkgs)
        ++ pkgs.lib.optionals pkgs.stdenv.isDarwin (darwinPackages pkgs)
        ++ (with pkgs; [
          spoolQt6.qtbase
          spoolQt6.qtdeclarative
          spoolQt6.qtimageformats
          spoolQt6.qtsvg
          spoolQcoro
          spoolQt6.qttools
          spoolQt6.qtwebsockets
          (qmlToolWrappers pkgs pkgs.spoolQt6)
        ])
        ++ pkgs.lib.optionals pkgs.stdenv.isLinux (with pkgs; [
          spoolQt6.qtwayland
        ]);

      nativeQtPackages = pkgs:
        (with pkgs; [
          spoolQt6.qtbase
          spoolQt6.qtdeclarative
          spoolQt6.qtimageformats
          spoolQt6.qtsvg
          spoolQt6.qtwebsockets
        ])
        ++ pkgs.lib.optionals pkgs.stdenv.isLinux (with pkgs; [
          spoolQt6.qtwayland
        ]);

      nativeRuntimePackages = pkgs:
        nativeQtPackages pkgs
        ++ pkgs.lib.optionals pkgs.stdenv.isLinux (nativeLinuxPackages pkgs);

      gitHooksShellHook = ''
        repo_root="$(git rev-parse --show-toplevel 2>/dev/null || true)"
        if [ -n "$repo_root" ] && [ -x "$repo_root/tools/install-git-hooks.sh" ]; then
          "$repo_root/tools/install-git-hooks.sh"
        fi
        unset repo_root
      '';

      commonShellHook = pkgs: gitHooksShellHook + ''
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

        ${pkgs.lib.optionalString pkgs.stdenv.isDarwin ''
          export GNU_ICONV_DYLIB="${pkgs.libiconvReal}/lib/libiconv.2.dylib"
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

      '';

      nativeShellHook = pkgs: commonShellHook pkgs + ''
        # This shell intentionally includes nixpkgs Qt for native Linux/macOS
        # development. Do not use it for tools/webos-native/build-qt6-611.sh.
        export SPOOL_QT_CMAKE_DIR="${pkgs.spoolQt6.qtbase}/lib/cmake/Qt6"
        native_qt_cmake_path="${pkgs.spoolQt6.qtbase}:${pkgs.spoolQt6.qtdeclarative}:${pkgs.spoolQt6.qtsvg}:${pkgs.spoolQt6.qttools}:${pkgs.spoolQt6.qtwebsockets}"
        export CMAKE_PREFIX_PATH="$native_qt_cmake_path''${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
        unset native_qt_cmake_path
        export JELLYFIN_NATIVE_SHELL=1

        # macdeployqt and linuxdeploy-plugin-qt both discover plugins under one
        # Qt prefix, and every Qt module is a separate store path here. That
        # leaves qtimageformats invisible to packaging, so the bundles shipped
        # only qtbase's gif/ico/jpeg readers while ArtworkService asks Jellyfin
        # for webp. tools/lib/qt-deploy.sh and tools/package-appimage.sh take
        # qwebp from this prefix; both then assert it landed.
        export SPOOL_QT_EXTRA_PLUGIN_DIRS="${pkgs.spoolQt6.qtimageformats}/lib/qt-6/plugins"
      '';
      cachedNativeQtPackage = pkgs:
        let
          packages = nativeQtPackages pkgs ++ [
            pkgs.spoolQt6.qttools
            pkgs.spoolQcoro
          ];
        in
        pkgs.symlinkJoin {
          name = "spool-native-qt-${pkgs.spoolQt6.qtbase.version}";
          paths = pkgs.lib.unique (packages ++ map pkgs.lib.getDev packages);
        };

      cachedNativePackage = pkgs:
        pkgs.stdenv.mkDerivation {
          pname = "spool";
          version = pkgs.lib.removeSuffix "\n" (builtins.readFile ./VERSION);
          src = builtins.path {
            path = self;
            name = "spool-source";
            filter = path: _: baseNameOf path != "mpv";
          };

          nativeBuildInputs = nativePackages pkgs ++ [ pkgs.spoolQt6.wrapQtAppsHook ];
          dontWrapQtApps = pkgs.stdenv.isDarwin;
          dontConfigure = true;
          dontInstall = true;
          qtWrapperArgs = pkgs.lib.optionals pkgs.stdenv.isLinux [
            "--prefix"
            "PATH"
            ":"
            (pkgs.lib.makeBinPath [ pkgs.libsecret ])
          ];

          postUnpack = ''
            rm -rf "$sourceRoot/mpv"
            cp -R ${mpv-src} "$sourceRoot/mpv"
            chmod -R u+w "$sourceRoot/mpv"
          '';

          buildPhase = ''
            patchShebangs mpv/TOOLS
            runHook preBuild
            export HOME="$TMPDIR/home"
            export XDG_CACHE_HOME="$TMPDIR/cache"
            export JELLYFIN_NATIVE_SHELL=1
            export SPOOL_QT_CMAKE_DIR="${pkgs.spoolQt6.qtbase}/lib/cmake/Qt6"
            export CMAKE_PREFIX_PATH="${pkgs.spoolQt6.qtbase}:${pkgs.spoolQt6.qtdeclarative}:${pkgs.spoolQt6.qtsvg}:${pkgs.spoolQt6.qttools}:${pkgs.spoolQt6.qtwebsockets}''${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
            mkdir -p "$HOME" "$XDG_CACHE_HOME"

            ${if pkgs.stdenv.isDarwin then ''
              export PATH="$PATH:/usr/bin:/bin"
              BUILD_ROOT="$TMPDIR/spool-build" \
              MPV_PREFIX="$out" \
              APP_INSTALL="$out/Applications" \
              DEPLOY_APP=0 \
              SPOOL_MACOS_CREDENTIAL_SERVICE=com.sachk.spool \
                bash tools/build-macos.sh
            '' else ''
              BUILD_ROOT="$TMPDIR/spool-build" \
              APP_INSTALL="$out" \
                bash tools/build-linux-release.sh
            ''}
            runHook postBuild
          '';

          meta = {
            description = "Spool for Jellyfin";
            license = pkgs.lib.licenses.gpl3Plus;
            platforms = systems;
          };
        };

    in
    {
      packages = forAllCacheSystems (pkgs: {
        default = cachedNativePackage pkgs;
        native-cache = cachedNativePackage pkgs;
        native-qt-cache = cachedNativeQtPackage pkgs;
      });

      devShells = forAllSystems (pkgs:
        let
          system = pkgs.stdenv.hostPlatform.system;
          lintPkgs = import (nixpkgsFor system) { inherit system; };
        in {
        default = pkgs.mkShell {
          packages = sourceBuildPackages pkgs;
          shellHook = sourceShellHook pkgs;
        };

        lint = lintPkgs.mkShell {
          packages = with lintPkgs; [
            python3
            qt6.qtdeclarative
            (qmlToolWrappers lintPkgs lintPkgs.qt6)
          ];
          shellHook = gitHooksShellHook;
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
          system = pkgs.stdenv.hostPlatform.system;
          cachedPkgs = cachePkgsFor system;
          cachedPackage = cachedNativePackage cachedPkgs;
          cachedQtPluginPath =
            cachedPkgs.lib.makeSearchPath cachedPkgs.spoolQt6.qtbase.qtPluginPrefix
              (nativeQtPackages cachedPkgs);
          cachedQmlImportPath =
            cachedPkgs.lib.makeSearchPath cachedPkgs.spoolQt6.qtbase.qtQmlPrefix
              (nativeQtPackages cachedPkgs);
          cachedRuntimeLibPath =
            cachedPkgs.lib.makeLibraryPath (nativeRuntimePackages cachedPkgs);
          cachedMacosRunner = pkgs.writeShellScript "jellyfin-native-cached-run" ''
            export DYLD_LIBRARY_PATH="${cachedPackage}/lib:${cachedRuntimeLibPath}''${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
            export QT_PLUGIN_PATH="${cachedQtPluginPath}"
            export QML2_IMPORT_PATH="${cachedQmlImportPath}"
            export QML_IMPORT_PATH="$QML2_IMPORT_PATH"
            export LC_NUMERIC=C
            exec "${cachedPackage}/Applications/Spool.app/Contents/MacOS/Spool" "$@"
          '';
          cachedProgram =
            if pkgs.stdenv.isDarwin
            then cachedMacosRunner
            else "${cachedPackage}/bin/jellyfin-native";
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
            then "build/macos/run-install/Spool.app/Contents/MacOS/Spool"
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
            pkgs.lib.makeSearchPath pkgs.spoolQt6.qtbase.qtPluginPrefix
              (nativeQtPackages pkgs);
          qmlImportPath =
            pkgs.lib.makeSearchPath pkgs.spoolQt6.qtbase.qtQmlPrefix
              (nativeQtPackages pkgs);
          nativeRuntimeLibPath = pkgs.lib.makeLibraryPath (nativeRuntimePackages pkgs);
          # Development apps resolve the checkout, optionally build the
          # selected native variant, then launch it inside the #native shell.
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
                then "${buildRoot}/run-install/Spool.app/Contents/MacOS/Spool"
                else "${buildRoot}/install/bin/jellyfin-native";
              runnerMpvLibraryPath =
                if buildRoot == "" then mpvLibraryPath
                else "${buildRoot}/mpv-prefix/lib";
              runnerBuildStamp =
                if buildRoot != "" then "${buildRoot}/.jellyfin-nix-source-id"
                else if pkgs.stdenv.isDarwin then "build/macos/.jellyfin-nix-source-id"
                else "build/linux-release/.jellyfin-nix-source-id";
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
            BUILD_STAMP="$REPO_ROOT/${runnerBuildStamp}"

            # Strip webOS cross state so native Linux builds do not pick up the
            # old SDK wayland-scanner/cross toolchain.
            scrub='PATH=$(printf %s "$PATH" | tr ":" "\n" | grep -v webos-sdk | paste -sd:); export PATH; unset WEBOS_SDK_ROOT QT_PLUGIN_PATH QML2_IMPORT_PATH QML_IMPORT_PATH'

            if ${if buildBeforeRun then "true" else "false"}; then
              if [ -x "$BIN" ] && [ -f "$BUILD_STAMP" ] && [ "$(cat "$BUILD_STAMP")" = "${stagedSourceId}" ]; then
                echo "native build is current (${stagedSourceId}); skipping rebuild"
              else
                nix develop "$REPO_ROOT#native" -c bash -c "$scrub; ${buildRootExport}export JELLYFIN_CMAKE_EXTRA_ARGS='${cmakeExtraArgs}'; ${runnerBuildCommand}"
                mkdir -p "$(dirname "$BUILD_STAMP")"
                printf '%s\n' "${stagedSourceId}" > "$BUILD_STAMP"
              fi
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

          runner = makeRunner {
            name = "jellyfin-native-run";
            buildBeforeRun = true;
          };
          noBuildRunner = makeRunner { name = "jellyfin-native-run-no-build"; };
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
            launchPrefix = "env GAMMARAY_DisabledPlugins=gammaray_quickinspector ${pkgs.gammaray}/bin/gammaray ";
          };

          gammarayFullRunner = makeRunner {
            name = "jellyfin-native-gammaray-full";
            launchPrefix = "${pkgs.gammaray}/bin/gammaray ";
          };
        in {
          build = {
            type = "app";
            program = "${builder}/bin/jellyfin-native-build";
          };

          # A dirty Git flake keeps the incremental checkout build. Clean
          # revisions execute the immutable package that release CI publishes
          # to Cachix, so `nix run` only substitutes and launches it.
          default = {
            type = "app";
            program =
              if self ? dirtyRev
              then "${runner}/bin/jellyfin-native-run"
              else cachedProgram;
          };

          run = {
            type = "app";
            program = "${noBuildRunner}/bin/jellyfin-native-run-no-build";
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
