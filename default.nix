{
  pkgs ? import <nixpkgs>
  #pkgs ? import (fetchTarball {
  #  url = "https://github.com/NixOS/nixpkgs/archive/4fe8d07066f6ea82cda2b0c9ae7aee59b2d241b3.tar.gz";
  #  sha256 = "sha256:06jzngg5jm1f81sc4xfskvvgjy5bblz51xpl788mnps1wrkykfhp";
  #}) {}
}:
pkgs.stdenv.mkDerivation rec {
  pname = "tetris";
  version = "0.1.0";

  #src = pkgs.fetchgit {
  #   url = "https://github.com/PYates77/console_tetris.git";
  #   sha256 = "sha256-DDInwjTDZ6fYg46XueuSH+VIcsVWb/YYSFU0kDM5RuM=";
  #};
  src = ./src;

  #src = pkgs.fetchgit {
  #  url = "https://gitlab.inria.fr/nix-tutorial/chord-tuto-nix-2022";
  #  rev = "069d2a5bfa4c4024063c25551d5201aeaf921cb3";
  #  sha256 = "sha256-MlqJOoMSRuYeG+jl8DFgcNnpEyeRgDCK2JlN9pOqBWA=";
  #};

  buildInputs = [
     pkgs.ncurses
     pkgs.libgcc
  ];

  configurePhase = ''
  '';

  buildPhase = ''
    gcc console_tetris.c -o tetris -lncurses 
  '';

  installPhase = ''
    mkdir -p $out/bin
    mv tetris $out/bin
  '';
}
