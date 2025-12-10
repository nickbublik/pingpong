{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = [
    pkgs.cmake
    pkgs.boost189
    pkgs.openssl
    pkgs.gcc        # or clang, if you use clang
  ];
}

# Usage:
# $ nix-shell
# $ mkdir -p build
# $ cd build
# $ cmake ..
# $ make -j
