let
  pkgs = import (builtins.fetchTarball {
    url = "https://releases.nixos.org/nixos/26.05/nixos-26.05.9227.c25784012c99/nixexprs.tar.xz";
    sha256 = "1br666y8sjng35y1di6nbclnigjadnls6311w908p5km2kxmlnb9";
  }) {};
in pkgs.mkShell {
  packages = with pkgs; [ cmake ninja pkg-config sdl3 ffmpeg libglvnd python3 desktop-file-utils imagemagick ];
  shellHook = ''
    export LD_LIBRARY_PATH="/run/opengl-driver/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  '';
}
