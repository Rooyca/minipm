{
  description = "minipm - a minimal power-manager";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };
  outputs =
    { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in
    {
      packages.${system}.default = pkgs.stdenv.mkDerivation {
        pname = "minipm";
        version = "0.1.0";
        src = ./.;
        buildInputs = [ pkgs.xorg.libX11 ];
        buildPhase = ''
          mkdir -p build
          $CC main.c -o build/minipm $LDFLAGS $CPPFLAGS -lX11
        '';
        installPhase = ''
          mkdir -p $out/bin
          cp build/minipm $out/bin/minipm
        '';
      };
      devShells.${system}.default = pkgs.mkShell {
        buildInputs = [
          pkgs.gcc
          pkgs.xorg.libX11
        ];
      };
    };
}
