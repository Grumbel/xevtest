{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs?ref=nixos-unstable";
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
            version = "0.1.0";

            src = ./.;

            buildInputs = with pkgs; [
              libxcb

              # unused, but necessary to fix warnings issued by cmake
              libxau
              libxdmcp
            ];

            nativeBuildInputs = with pkgs; [
              pkg-config
              cmake
            ];
          };
        };
      }
    );
}
