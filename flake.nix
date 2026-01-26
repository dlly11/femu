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

            # Testing
            pytest
            pytest-cov

            # Development tools
            black
            mypy
            ruff
            pip
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

            # Debugging and analysis
            gdb
            valgrind

            # Static analysis tools
            cppcheck
            clang-tools  # includes clang-tidy, clang-format

            # Documentation
            doxygen

            # Utilities
            git
            which
          ];

          shellHook = ''
            echo "╔══════════════════════════════════════════════════════════════╗"
            echo "║              FEMU Development Environment                     ║"
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
            echo ""

            # Set up environment for ARM cross-compilation
            export ARM_CC=arm-none-eabi-gcc
            export ARM_OBJCOPY=arm-none-eabi-objcopy
            export ARM_OBJDUMP=arm-none-eabi-objdump

            # CppUTest paths
            export CPPUTEST_HOME=$PWD/lib/cpputest

            # Install Python package in development mode
            if [ ! -f "$PWD/.venv-installed" ]; then
              echo "Installing Python package in development mode..."
              pip install -e "$PWD" --quiet 2>/dev/null || true
              touch "$PWD/.venv-installed"
            fi

            echo "╔══════════════════════════════════════════════════════════════╗"
            echo "║  Quick Start                                                  ║"
            echo "╠══════════════════════════════════════════════════════════════╣"
            echo "║  femu build all              Build the project                ║"
            echo "║  femu test all               Run all tests                    ║"
            echo "║  femu dev status             Show module status               ║"
            echo "║  femu dev context <module>   AI context for module            ║"
            echo "╠══════════════════════════════════════════════════════════════╣"
            echo "║  Build with different compilers:                              ║"
            echo "║    CC=clang CXX=clang++ femu build all                        ║"
            echo "║    CC=gcc CXX=g++ femu build all                              ║"
            echo "╠══════════════════════════════════════════════════════════════╣"
            echo "║  Static Analysis:                                             ║"
            echo "║    cmake --build build --target cppcheck                      ║"
            echo "║    cmake --build build --target clang-tidy                    ║"
            echo "║    cmake --build build --target analyze                       ║"
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
