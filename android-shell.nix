let
  pkgs = import (builtins.fetchTarball {
    url = "https://releases.nixos.org/nixos/26.05/nixos-26.05.9227.c25784012c99/nixexprs.tar.xz";
    sha256 = "1br666y8sjng35y1di6nbclnigjadnls6311w908p5km2kxmlnb9";
  }) {};
in pkgs.buildFHSEnv {
  name = "openjuice-android-shell";
  targetPkgs = p: with p; [ bash coreutils findutils gnugrep gnused gawk git curl unzip zip which file python3 jdk17 cmake ninja gnumake pkg-config gcc zlib libxml2 openssl ncurses5 patchelf android-tools libGL libglvnd vulkan-loader libxkbcommon alsa-lib libpulseaudio nss nspr dbus xorg.libX11 xorg.libXcursor xorg.libXrandr xorg.libXi xorg.libXrender xorg.libxcb ];
  runScript = "bash";
  profile = ''
    export JAVA_HOME=${pkgs.jdk17}
    export SSL_CERT_FILE=${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt
    export ANDROID_SDK_ROOT="$PWD/out/android-sdk"
    export ANDROID_HOME="$ANDROID_SDK_ROOT"
  '';
}
