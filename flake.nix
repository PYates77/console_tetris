{
  description = "Console Tetris Flake";

  # These must be other flakes?
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    #console_tetris.url = "github:pyates77/console_tetris?ref=master";
  };

  outputs = { self, nixpkgs, ... }:
      let 
          pkgs = import nixpkgs { system = "x86_64-linux"; };
      in
      {
          devShells.x86_64-linux.default = pkgs.mkShell {
              buildInputs = [
                pkgs.ncurses
                pkgs.libgcc
              ];
          };

          packages.x86_64-linux.tetris = pkgs.callPackage ./default.nix {};
          #packages.x86_64-linux.tetris = pkgs.stdenv.mkDerivation {
          #    pname = "tetris";
          #    version = "0.1.0";
          #    src = ./src;
          #    buildInputs = [
          #      #pkgs.gnumake
          #      pkgs.ncurses
          #      #pkgs.libgcc
          #    ];
          #    #buildPhase = "gcc console_tetris.c -o tetris -lncurses";
          #    installPhase = ''
          #      mkdir -p $out/bin
          #      mv tetris $out/bin
          #    '';
          #};
          
# define the default target, like a makefile
          packages.x86_64-linux.default = self.packages.x86_64-linux.tetris;
      };
}
