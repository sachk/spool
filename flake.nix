{
  description = "Jellyfin webOS native build environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/15f4ee454b1dce334612fa6843b3e05cf546efab";
    libplacebo-src = {
      url = "git+https://github.com/haasn/libplacebo?submodules=1&rev=27aa71a97f4daed84916936572fa6a2e1c3eedb7";
      flake = false;
    };
  };

  outputs = { nixpkgs, libplacebo-src, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-darwin" "x86_64-darwin" ];
      libplaceboOverlay = final: prev: {
        libplacebo = prev.libplacebo.overrideAttrs (_: {
          version = "master-27aa71a";
          src = libplacebo-src;
          patches = [];
        });
      };
      # Slim ffmpeg-full: keep everything libmpv needs for decoding + rendering on
      # Linux, but drop encoders, niche protocols, TTS, fingerprinting, image
      # formats Qt already handles, and other transitive bloat that otherwise
      # gets pulled into the AppImage via libavcodec.so.62's RPATH/NEEDED list.
      ffmpegSlimOverlay = final: prev: {
        ffmpeg-full = prev.ffmpeg-full.override {
          # ffplay needs SDL2; we don't ship it.
          buildFfplay = false;
          withSdl2 = false;

          # Encoders we never use in a playback-only client.
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

          # Niche audio codecs (native ffmpeg decoders cover the few streams
          # Jellyfin actually serves).
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

          # Niche video codecs.
          withDavs2 = false;
          withUavs3d = false;

          # ARIB / DVB subtitle and teletext stacks.
          withAribb24 = false;
          withAribcaption = false;
          withZvbi = false;

          # Network protocols we don't use.
          withSrt = false;
          withRist = false;
          withSsh = false;
          withRtmp = false;

          # Removable bloat.
          withSamba = false;            # libsmbclient pulls ~30 samba libs (~19 MB)
          withFlite = false;            # TTS voice databases (~23 MB)
          withChromaprint = false;      # audio fingerprinting
          withTensorflow = false;       # huge
          withWhisper = false;          # huge
          withVmaf = false;
          withZmq = false;
          withJxl = false;              # Qt handles JPEG-XL via its own plugin if ever
          withSvg = false;              # librsvg + cairo + rust deps; Qt has QtSvg
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

          # GPU / vendor encode paths (most are off by default but be explicit).
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
        libxkbcommon
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
        patchelf
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
        mesa
        pipewire
        shaderc
        squashfsTools
        spirv-cross
        wayland
        wayland-scanner
        wayland-protocols
        qt6.qtwayland
        zimg
      ];
      darwinPackages = pkgs: with pkgs; [
        apple-sdk_15
        create-dmg
      ];
      allPackages = pkgs: commonPackages pkgs
        ++ pkgs.lib.optionals pkgs.stdenv.isLinux (linuxPackages pkgs)
        ++ pkgs.lib.optionals pkgs.stdenv.isDarwin (darwinPackages pkgs);
    in
    {
      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          packages = allPackages pkgs;
          shellHook = ''
            export SSL_CERT_FILE=${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt
            export CURL_CA_BUNDLE="$SSL_CERT_FILE"
            export NIX_ENFORCE_PURITY=0
            export WEBOS_SDK_ROOT="$PWD/../build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot"
            ${pkgs.lib.optionalString pkgs.stdenv.isLinux ''
              export WAYLAND_PROTOCOLS_DIR="${pkgs.wayland-protocols}/share/wayland-protocols"
            ''}
          '';
        };
      });

      apps = forAllSystems (pkgs:
        let
          runner = pkgs.writeShellScriptBin "jellyfin-native-run" ''
            export PATH="${pkgs.lib.makeBinPath [ pkgs.nix pkgs.bashInteractive pkgs.coreutils pkgs.gnugrep ]}:$PATH"
              set -euo pipefail
              REPO_ROOT="''${JELLYFIN_REPO:-$PWD}"
              if [ ! -f "$REPO_ROOT/CMakeLists.txt" ] || [ ! -d "$REPO_ROOT/mpv" ]; then
                echo "error: run from the jellyfin-webos repo root, or set JELLYFIN_REPO" >&2
                exit 1
              fi
              cd "$REPO_ROOT"
              BIN="$REPO_ROOT/build/linux-release/install/bin/jellyfin-native"
              # Strip the webOS buildroot SDK from PATH and unset its env so the
              # native Linux build doesn't pick up the old wayland-scanner /
              # cross toolchain.
              scrub='PATH=$(printf %s "$PATH" | tr ":" "\n" | grep -v webos-sdk | paste -sd:); export PATH; unset WEBOS_SDK_ROOT QT_PLUGIN_PATH QML2_IMPORT_PATH QML_IMPORT_PATH'
              if [ -n "''${JELLYFIN_NO_REBUILD:-}" ] && [ -x "$BIN" ]; then
                :
              else
                nix develop "$REPO_ROOT" -c bash -c "$scrub; exec bash tools/build-linux-release.sh"
              fi
              MPV_LIB="$REPO_ROOT/build/linux-release/mpv-prefix/lib"
              export LD_LIBRARY_PATH="$MPV_LIB''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
              # libmpv requires C numeric locale or it refuses to start.
              export LC_NUMERIC=C
              exec nix develop "$REPO_ROOT" -c bash -c "$scrub"'; exec "$@"' _ "$BIN" "$@"
          '';
        in {
          default = {
            type = "app";
            program = "${runner}/bin/jellyfin-native-run";
          };
        });
    };
}
