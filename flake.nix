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
      forAllSystems = f:
        nixpkgs.lib.genAttrs systems (system:
          f (import nixpkgs {
            inherit system;
            config.allowUnfree = true;
            overlays = [ libplaceboOverlay ];
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
              scrub='PATH=$(printf %s "$PATH" | tr ":" "\n" | grep -v webos-sdk | paste -sd:); export PATH; unset WEBOS_SDK_ROOT'
              if [ ! -x "$BIN" ] || [ -n "''${JELLYFIN_REBUILD:-}" ]; then
                nix develop "$REPO_ROOT" -c bash -c "$scrub; exec bash tools/build-linux-release.sh"
              fi
              MPV_LIB="$REPO_ROOT/build/linux-release/mpv-prefix/lib"
              export LD_LIBRARY_PATH="$MPV_LIB''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
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
