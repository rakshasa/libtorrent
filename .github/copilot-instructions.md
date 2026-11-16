# Copilot Behavior Guidelines
- The environment is strictly Ubuntu 22.04.
- All compilation tools (automake, aclocal, libtoolize, autoconf) are fully pre-installed.
- Never suggest commands that probe the system architecture, OS release version, or package manager availability.
- Always output direct compilation commands without environment-checking wrappers.

# Project Architecture & Bootstrapping Strategy
- **This is a barebones Autotools repository.** It contains a `configure.ac` and `Makefile.am`.
- **There is no `autogen.sh`, `bootstrap`, or pre-generated `configure` script.** Do not look for, check for, or wrap execution around their existence.
- When tasked with generating, building, or configuring the project, skip all environment verification and immediately execute the Autotools generation sequence.
- **Ensure Autotools binaries are in the PATH.** If a tool like `aclocal` is not found by `autoreconf`, invoke the sequence using explicit system paths or direct binary execution.
- Proceed directly to these sequential execution steps without exception:
  1. `autoreconf -ivf`
  2. `./configure`
  3. `make`
