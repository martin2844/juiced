{ pkgs ? import (builtins.fetchTarball {
    url = "https://releases.nixos.org/nixos/26.05/nixos-26.05.9227.c25784012c99/nixexprs.tar.xz";
    sha256 = "1br666y8sjng35y1di6nbclnigjadnls6311w908p5km2kxmlnb9";
  }) {} }:
let
  mojo = pkgs.fetchzip {
    url = "https://github.com/icculus/mojoshader/archive/ad5dff84830c2863c841f4b1f4e3df78c705b383.tar.gz";
    sha256 = "1whcz9f4mx8psss9k0nnab5jwl4b7pyvwdbzd7fqmi376brp6vfs";
  };
in pkgs.stdenv.mkDerivation {
  pname = "openjuice";
  version = "0.3.3-linux";
  src = pkgs.lib.cleanSourceWith {
    src = ./.;
    filter = path: type:
      !(builtins.elem (baseNameOf path) [ ".git" "out" ".cache" "assets" "result" ])
      && pkgs.lib.cleanSourceFilter path type;
  };
  nativeBuildInputs = with pkgs; [ cmake ninja pkg-config makeWrapper ];
  buildInputs = with pkgs; [ sdl3 ffmpeg libglvnd ];
  preConfigure = ''
    cp -r ${mojo} mojoshader-source
    chmod -R u+w mojoshader-source
    cmakeFlagsArray+=("-DFETCHCONTENT_SOURCE_DIR_MOJOSHADER=$PWD/mojoshader-source")
  '';
  doCheck = true;
  checkPhase = "ctest --output-on-failure";
  postInstall = ''
    wrapProgram "$out/bin/juiced" --prefix LD_LIBRARY_PATH : /run/opengl-driver/lib
  '';
  meta = {
    description = "OpenJuice native Linux engine (requires original Juiced game data)";
    platforms = [ "x86_64-linux" ];
  };
}
