{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-22.11";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in {
        packages = rec {
          default = xmousetest;

          xmousetest = pkgs.stdenv.mkDerivation rec {
            pname = "xmousetest";
            version = "0.0.0";

            src = ./.;

            buildInputs = with pkgs; [
              xorg.libxcb
              xorg.libX11
              extra-cmake-modules
              fmt
            ];

            nativeBuildInputs = [
              pkgs.cmake
            ];
          };
        };
      }
    );
}
