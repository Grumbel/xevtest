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
          default = xevtest;

          xevtest = pkgs.stdenv.mkDerivation rec {
            pname = "xevtest";
            version = "0.0.0";

            src = ./.;

            buildInputs = with pkgs; [
              xorg.libxcb
              extra-cmake-modules
              fmt

              # unused, but necessary to fix warnings issued by cmake
              xorg.libXau
              xorg.libXdmcp
            ];

            nativeBuildInputs = [
              pkgs.cmake
            ];
          };
        };
      }
    );
}
