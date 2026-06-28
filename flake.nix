{
  description = "FEMU - Fast EMUlator";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs {
          inherit system;
          crossSystem = null;
        };

        # Python with required packages
        pythonEnv = pkgs.python3.withPackages (
          ps: with ps; [
            # Runtime dependencies
            cffi
            click
            rich
            pyyaml
            pyelftools

            # Testing
            pytest
            pytest-cov

            # Documentation
            sphinx
            sphinx-autodoc-typehints
            myst-parser
            furo
            breathe
          ]
        );

      in
      {
        devShells.default = pkgs.mkShell {
          name = "femu";

          buildInputs = with pkgs; [
            # C/C++ compilers
            gcc
            clang

            # ARM cross-compiler
            pkgsCross.arm-embedded.stdenv.cc

            # Build tools
            cmake
            gnumake
            ninja

            # Python environment
            pythonEnv

            # Python tooling (Astral): package manager, linter/formatter, type checker
            uv
            ruff
            ty

            # Debugging and analysis
            gdb
            valgrind

            # Static analysis tools
            cppcheck
            clang-tools # includes clang-tidy, clang-format

            # Documentation
            doxygen
            graphviz # For Doxygen diagrams

            # Utilities
            git
            which

            # CI tools
            act # GitHub Actions local runner
            pre-commit # Pre-commit hook framework
          ];

          shellHook = ''
            echo "╔══════════════════════════════════════════════════════════════╗"
            echo "║              FEMU Development Environment                    ║"
            echo "╚══════════════════════════════════════════════════════════════╝"
            echo ""
            echo "Compilers:"
            echo "  gcc               - $(gcc --version | head -1)"
            echo "  clang             - $(clang --version | head -1)"
            echo "  arm-none-eabi-gcc - $(arm-none-eabi-gcc --version | head -1)"
            echo ""
            echo "Static Analysis:"
            echo "  cppcheck          - $(cppcheck --version)"
            echo "  clang-tidy        - $(clang-tidy --version | head -1)"
            echo ""
            echo "Python: $(python3 --version)"
            echo "Python tooling (Astral):"
            echo "  uv                - $(uv --version)"
            echo "  ruff              - $(ruff --version)"
            echo "  ty                - $(ty --version)"
            echo ""

            # Set up environment for ARM cross-compilation
            export ARM_CC=arm-none-eabi-gcc
            export ARM_OBJCOPY=arm-none-eabi-objcopy
            export ARM_OBJDUMP=arm-none-eabi-objdump

            # CppUTest paths
            export CPPUTEST_HOME=$PWD/lib/cpputest

            # Add Python package to PYTHONPATH for development
            export PYTHONPATH="$PWD/python:$PYTHONPATH"

            # Create wrapper script for femu command
            mkdir -p "$PWD/.nix-shell-bin"
            cat > "$PWD/.nix-shell-bin/femu" << 'WRAPPER'
#!/usr/bin/env bash
exec python -m femu.cli "$@"
WRAPPER
            chmod +x "$PWD/.nix-shell-bin/femu"
            export PATH="$PWD/.nix-shell-bin:$PATH"

            echo "╔══════════════════════════════════════════════════════════════╗"
            echo "║  Quick Start                                                 ║"
            echo "╠══════════════════════════════════════════════════════════════╣"
            echo "║  femu build all              Build the project               ║"
            echo "║  femu test all               Run all tests                   ║"
            echo "║  femu dev status             Show module status              ║"
            echo "║  femu dev context <module>   AI context for module           ║"
            echo "╠══════════════════════════════════════════════════════════════╣"
            echo "║  Build with different compilers:                             ║"
            echo "║    CC=clang CXX=clang++ femu build all                       ║"
            echo "║    CC=gcc CXX=g++ femu build all                             ║"
            echo "╠══════════════════════════════════════════════════════════════╣"
            echo "║  Static Analysis:                                            ║"
            echo "║    femu build analyze                                        ║"
            echo "╠══════════════════════════════════════════════════════════════╣"
            echo "║  Documentation:                                              ║"
            echo "║    femu docs build             Build documentation           ║"
            echo "║    femu docs serve             Serve docs locally            ║"
            echo "╚══════════════════════════════════════════════════════════════╝"
            echo ""
          '';
        };

        # Package for the emulator
        packages.default = pkgs.python3Packages.buildPythonApplication {
          pname = "femu";
          version = "0.1.0";
          src = ./.;
          format = "pyproject";

          nativeBuildInputs = with pkgs; [
            cmake
            python3Packages.setuptools
          ];

          propagatedBuildInputs = with pkgs.python3Packages; [
            cffi
            click
            rich
          ];

          # Build C components
          preBuild = ''
            mkdir -p build
            cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
            cmake --build build -j$NIX_BUILD_CORES
          '';

          meta = with pkgs.lib; {
            description = "FEMU - Fast EMUlator";
            homepage = "https://github.com/dlly11/femu";
            license = licenses.mit;
            maintainers = [ ];
          };
        };
      }
    );
}
