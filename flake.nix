{
  description = "Jellyfin webOS native build environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/15f4ee454b1dce334612fa6843b3e05cf546efab";
  };

  outputs = { nixpkgs, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-darwin" "x86_64-darwin" ];
      forAllSystems = f:
        nixpkgs.lib.genAttrs systems (system:
          f (import nixpkgs {
            inherit system;
            config.allowUnfree = true;
          }));
    in
    {
      devShells = forAllSystems (pkgs:
        let
          linuxPackages = with pkgs; [
            alsa-lib
            appimage-run
            expat
            libdrm
            libpulseaudio
            libva
            libvdpau
            libxkbcommon
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
          darwinPackages = with pkgs; [
            apple-sdk_15
            create-dmg
          ];
        in {
          default = pkgs.mkShell {
            packages = with pkgs; [
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
            ] ++ pkgs.lib.optionals pkgs.stdenv.isLinux linuxPackages
              ++ pkgs.lib.optionals pkgs.stdenv.isDarwin darwinPackages;

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
    };
}
