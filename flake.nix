{
  description = "QtFin / Jellyfin native desktop build environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/15f4ee454b1dce334612fa6843b3e05cf546efab";

    libplacebo-src = {
      url = "git+https://github.com/haasn/libplacebo?submodules=1&rev=27aa71a97f4daed84916936572fa6a2e1c3eedb7";
      flake = false;
    };

    # Starfish-enabled mpv fork. Upstream mpv does not have your -Dstarfish option.
    mpv-src = {
      url = "git+ssh://git@github.com/sachk/mpv?ref=webos&submodules=1";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, libplacebo-src, mpv-src, ... }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-darwin"
        "x86_64-darwin"
      ];

      libplaceboOverlay = final: prev: {
        libplacebo = prev.libplacebo.overrideAttrs (_: {
          version = "master-27aa71a";
          src = libplacebo-src;
          patches = [];
        });
      };

      ffmpegSlimOverlay = final: prev: {
        ffmpeg-full = prev.ffmpeg-full.override {
          buildFfplay = false;
          withSdl2 = false;

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

          withDavs2 = false;
          withUavs3d = false;

          withAribb24 = false;
          withAribcaption = false;
          withZvbi = false;

          withSrt = false;
          withRist = false;
          withSsh = false;
          withRtmp = false;

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

          withV4l2 = false;
          withV4l2M2m = false;
          withDvdnav = false;
          withDvdread = false;
          withDc1394 = false;
          withCdio = false;
          withCaca = false;

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

      pkgsFor = system:
        import nixpkgs {
          inherit system;
          config.allowUnfree = true;
          overlays = [
            libplaceboOverlay
            ffmpegSlimOverlay
          ];
        };

      forAllSystems = f:
        nixpkgs.lib.genAttrs systems (system:
          f (pkgsFor system)
        );

      platformName = pkgs:
        if pkgs.stdenv.isDarwin then "macos" else "linux";

      buildDir = pkgs:
        if pkgs.stdenv.isDarwin then "macos-release" else "linux-release";

      buildScript = pkgs:
        if pkgs.stdenv.isDarwin
        then "tools/build-macos-release.sh"
        else "tools/build-linux-release.sh";

      commonPackages = pkgs: with pkgs; [
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
        lua5_2
        luajit
        meson
        mujs
        ninja
        nodejs_22
        perl
        pkg-config
        python3
        qt6.qtbase
        qt6.qtdeclarative
        qt6.qtimageformats
        qt6.qttools
        rubberband
        rustup
        libuchardet
        unzip
        which
        zlib
        zip
      ];

      linuxPackages = pkgs: with pkgs; [
        alsa-lib
        appimage-run
        expat
        libdrm
        libpulseaudio
        libva
        libvdpau
        libxkbcommon
        mesa
        patchelf
        pipewire
        qt6.qtwayland
        shaderc
        squashfsTools
        spirv-cross
        wayland
        wayland-protocols
        wayland-scanner
        zimg
      ];

      darwinPackages = pkgs: with pkgs; [
        apple-sdk_15
        create-dmg
      ];

      nativePackages = pkgs:
        commonPackages pkgs
        ++ pkgs.lib.optionals pkgs.stdenv.isLinux (linuxPackages pkgs)
        ++ pkgs.lib.optionals pkgs.stdenv.isDarwin (darwinPackages pkgs);

      shellHookFor = pkgs: ''
        export SSL_CERT_FILE="${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt"
        export CURL_CA_BUNDLE="$SSL_CERT_FILE"
        export NIX_ENFORCE_PURITY=0

        export QTFIN_PLATFORM="${platformName pkgs}"
        export QTFIN_BUILD_DIR="${buildDir pkgs}"
        export QTFIN_BUILD_SCRIPT="${buildScript pkgs}"

        ${pkgs.lib.optionalString pkgs.stdenv.isLinux ''
          export WAYLAND_PROTOCOLS_DIR="${pkgs.wayland-protocols}/share/wayland-protocols"
          export WEBOS_SDK_ROOT="''${WEBOS_SDK_ROOT:-$PWD/../build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot}"
        ''}
      '';

      qtfinPackage = pkgs:
        pkgs.stdenv.mkDerivation {
          pname = "qtfin";
          version =
            if self ? shortRev && self.shortRev != null
            then self.shortRev
            else "dirty";

          src = self.outPath;

          nativeBuildInputs =
            nativePackages pkgs
            ++ [ pkgs.makeWrapper ]
            ++ pkgs.lib.optionals (pkgs.qt6 ? wrapQtAppsHook) [
              pkgs.qt6.wrapQtAppsHook
            ];

          buildInputs = nativePackages pkgs;

          dontConfigure = true;

          unpackPhase = ''
            runHook preUnpack

            mkdir source
            cp -R "$src"/. source/
            chmod -R u+rwX source

            rm -rf source/mpv
            mkdir source/mpv
            cp -R "${mpv-src}"/. source/mpv/
            chmod -R u+rwX source/mpv

            sourceRoot=source

            runHook postUnpack
          '';

          buildPhase = ''
            runHook preBuild

            export HOME="$TMPDIR/home"
            mkdir -p "$HOME"

            export SSL_CERT_FILE="${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt"
            export CURL_CA_BUNDLE="$SSL_CERT_FILE"
            export NIX_ENFORCE_PURITY=0

            export QTFIN_PLATFORM="${platformName pkgs}"
            export QTFIN_BUILD_DIR="${buildDir pkgs}"
            export QTFIN_BUILD_SCRIPT="${buildScript pkgs}"

            PATH="$(printf "%s" "$PATH" | tr ":" "\n" | { grep -v "webos-sdk" || true; } | paste -sd: -)"
            export PATH
            unset WEBOS_SDK_ROOT QT_PLUGIN_PATH QML2_IMPORT_PATH QML_IMPORT_PATH

            if [ ! -f "$QTFIN_BUILD_SCRIPT" ]; then
              echo "error: missing platform build script: $QTFIN_BUILD_SCRIPT" >&2
              exit 1
            fi

            if [ ! -f CMakeLists.txt ]; then
              echo "error: missing CMakeLists.txt" >&2
              exit 1
            fi

            if [ ! -d mpv ] || [ -z "$(find mpv -mindepth 1 -maxdepth 1 -print -quit)" ]; then
              echo "error: mpv input was not copied into source/mpv" >&2
              exit 1
            fi

            # Nix build sandboxes do not provide /usr/bin/env.
            # mpv executes helper scripts from source during Meson/Ninja, so
            # patch build-time shebangs before running your build script.
            patchShebangs --build .

            bash "$QTFIN_BUILD_SCRIPT"

            runHook postBuild
          '';

          installPhase = ''
            runHook preInstall

            mkdir -p "$out/bin" "$out/lib"

            if [ -d "build/${buildDir pkgs}/install" ]; then
              cp -R "build/${buildDir pkgs}/install"/. "$out"/
            fi

            if [ -d "build/${buildDir pkgs}/mpv-prefix/lib" ]; then
              mkdir -p "$out/lib/mpv-prefix"
              cp -R "build/${buildDir pkgs}/mpv-prefix/lib"/. "$out/lib/mpv-prefix"/
            fi

            find_built_binary() {
              for candidate in \
                "$out/bin/jellyfin-native" \
                "$out/bin/qtfin" \
                "$out/QtFin.app/Contents/MacOS/QtFin" \
                "$out/jellyfin-native.app/Contents/MacOS/jellyfin-native" \
                "build/${buildDir pkgs}/install/bin/jellyfin-native" \
                "build/${buildDir pkgs}/jellyfin-native" \
                "build/${buildDir pkgs}/install/bin/qtfin" \
                "build/${buildDir pkgs}/qtfin" \
                "build/${buildDir pkgs}/install/QtFin.app/Contents/MacOS/QtFin" \
                "build/${buildDir pkgs}/QtFin.app/Contents/MacOS/QtFin" \
                "build/${buildDir pkgs}/install/jellyfin-native.app/Contents/MacOS/jellyfin-native" \
                "build/${buildDir pkgs}/jellyfin-native.app/Contents/MacOS/jellyfin-native"
              do
                if [ -x "$candidate" ]; then
                  printf '%s\n' "$candidate"
                  return 0
                fi
              done

              return 1
            }

            target="$(find_built_binary || true)"

            if [ -z "$target" ]; then
              echo "error: build completed but no runnable QtFin binary was found" >&2
              echo "looked under build/${buildDir pkgs} and install output" >&2
              exit 1
            fi

            case "$target" in
              "$out"/*)
                installed_target="$target"
                ;;
              *)
                installed_target="$out/bin/$(basename "$target")"
                cp "$target" "$installed_target"
                chmod +x "$installed_target"
                ;;
            esac

            if [ "$installed_target" = "$out/bin/qtfin" ]; then
              mv "$installed_target" "$out/bin/qtfin-real"
              installed_target="$out/bin/qtfin-real"
            fi

            ${pkgs.lib.optionalString pkgs.stdenv.isLinux ''
              makeWrapper "$installed_target" "$out/bin/qtfin" \
                --set LC_NUMERIC C \
                --prefix LD_LIBRARY_PATH : "$out/lib/mpv-prefix"
            ''}

            ${pkgs.lib.optionalString pkgs.stdenv.isDarwin ''
              makeWrapper "$installed_target" "$out/bin/qtfin" \
                --set LC_NUMERIC C \
                --prefix DYLD_LIBRARY_PATH : "$out/lib/mpv-prefix"
            ''}

            runHook postInstall
          '';

          meta = with pkgs.lib; {
            description = "Native Qt Jellyfin client";
            platforms = [
              "x86_64-linux"
              "aarch64-darwin"
              "x86_64-darwin"
            ];
            mainProgram = "qtfin";
          };
        };
    in
    {
      packages = forAllSystems (pkgs: rec {
        default = qtfin;
        qtfin = qtfinPackage pkgs;
      });

      apps = forAllSystems (pkgs: {
        default = {
          type = "app";
          program = "${self.packages.${pkgs.stdenv.hostPlatform.system}.default}/bin/qtfin";
        };

        qtfin = {
          type = "app";
          program = "${self.packages.${pkgs.stdenv.hostPlatform.system}.qtfin}/bin/qtfin";
        };
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          packages = nativePackages pkgs;
          shellHook = shellHookFor pkgs;
        };

        native = pkgs.mkShell {
          packages = nativePackages pkgs;
          shellHook = shellHookFor pkgs;
        };
      });
    };
}
