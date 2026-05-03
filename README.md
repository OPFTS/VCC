<div align="center">
# VeroCC — Vero Compiler Collection
 
**A multi-language, multi-architecture compiler collection built on LLVM**
 
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Version](https://img.shields.io/badge/version-0.1.5-green.svg)](https://github.com/VeroTRUS/VeroCC/releases/tag/v0.1.5)
[![LLVM](https://img.shields.io/badge/LLVM-16%2B-orange.svg)](https://llvm.org/)
 
*Developed by [VTRUS — Vero Technical Research & Universal Security](https://github.com/VeroTRUS)*
 
</div>
---
 
## Table of Contents
 
- [What is VeroCC?](#what-is-verocc)
- [Architecture & Pipeline](#architecture--pipeline)
- [Supported Languages](#supported-languages)
- [Supported Targets](#supported-targets)
- [Installation](#installation)
  - [Dependencies](#dependencies)
  - [Fedora / RHEL / CentOS](#fedora--rhel--centos)
  - [Debian / Ubuntu / Pop!_OS](#debian--ubuntu--popos)
  - [Arch Linux / Manjaro](#arch-linux--manjaro)
  - [Gentoo](#gentoo)
  - [openSUSE](#opensuse)
  - [Solus](#solus)
  - [Slackware](#slackware)
  - [Alpine Linux](#alpine-linux)
  - [NixOS](#nixos)
  - [FreeBSD](#freebsd)
  - [OpenBSD](#openbsd)
  - [NetBSD](#netbsd)
  - [Linux From Scratch (LFS)](#linux-from-scratch-lfs)
  - [macOS](#macos)
  - [Windows (WSL2)](#windows-wsl2)
- [Building VeroCC](#building-verocc)
- [Usage](#usage)
  - [Basic Compilation](#basic-compilation)
  - [Inspect the Pipeline](#inspect-the-pipeline)
  - [VCP — Vero C Preprocessor](#vcp--vero-c-preprocessor)
  - [Optimization](#optimization)
  - [Debug Info](#debug-info)
  - [Macros and Include Paths](#preprocessor-macros-and-include-paths)
  - [Libraries](#libraries)
  - [Special Output Types](#special-output-types)
  - [Cross-Compilation](#cross-compilation)
  - [All Flags Reference](#all-flags-reference)
- [Testing with QEMU](#testing-with-qemu)
- [eBPF Support](#ebpf-support)
- [verolibc](#verolibc)
- [Changelog](#changelog)
- [License](#license)
---
 
## What is VeroCC?
 
VeroCC is the compiler frontend of the **VUASM (Vero Universal Assembler)** family,
a set of tools developed by VTRUS for universal code compilation across every major
CPU architecture and operating system.
 
VeroCC accepts source code in **C, C++, Pascal, Go, Rust, and Assembly** and drives
it through a clean pipeline down to a native binary — or a cross-compiled binary for
any architecture LLVM supports.
 
The command to invoke the compiler is **`vcc`**. The standalone preprocessor is **`vcp`**.
 
---
 
## Architecture & Pipeline
 
```
┌─────────────────────────────────────────────────────────┐
│                Source Code                               │
│   .c  .cpp  .pas  .go  .rs  .s  .asm  .ll               │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────┐
│            VeroCC  (vcc)  — Frontend Driver              │
│  Detects language, parses flags, drives the pipeline     │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────┐
│   VCP — Vero C Preprocessor          [NEW in v0.1.5]    │
│   Our own #define / #ifdef / #if / #include engine       │
│   System headers passed through to clang untouched       │
│   User headers expanded natively by VCP                  │
└──────────────────────┬──────────────────────────────────┘
                       │  Preprocessed source
                       ▼
┌─────────────────────────────────────────────────────────┐
│   VUC/A — Vero Universal C-to-Assembly                   │
│   Translates source → LLVM IR  (.ll)                     │
│                                                          │
│   • C        → VCP + clang --emit-llvm                   │
│   • C++      → clang++ --emit-llvm                       │
│   • Pascal   → fpc → object (direct to VUMC)             │
│   • Go       → go tool compile                           │
│   • Rust     → rustc --emit=llvm-ir                      │
│   • Assembly → llvm-mc                                   │
└──────────────────────┬──────────────────────────────────┘
                       │  LLVM IR (.ll)
                       ▼
┌─────────────────────────────────────────────────────────┐
│   VUA/MC — Vero Universal Assembly-to-Machine-Code       │
│   LLVM IR → native object file  (.o)  via llc            │
│   Supports all LLVM target triples                       │
└──────────────────────┬──────────────────────────────────┘
                       │  Object file (.o)
                       ▼
┌─────────────────────────────────────────────────────────┐
│   VUMC — Vero Universal Machine Code  (Linker)           │
│   Object files → final executable  via clang + lld       │
│   Automatically resolves CRT paths on any distro         │
└─────────────────────────────────────────────────────────┘
```
 
---
 
## Supported Languages
 
| Language      | Extension(s)         | Frontend Used              |
|---------------|----------------------|----------------------------|
| C             | `.c`                 | VCP + clang (LLVM IR)      |
| C++           | `.cpp` `.cxx` `.cc`  | clang++                    |
| Pascal        | `.pas` `.pp`         | FreePascal (fpc)           |
| Go            | `.go`                | go tool compile            |
| Rust          | `.rs`                | rustc                      |
| Assembly      | `.s` `.asm`          | llvm-mc                    |
| Assembly+CPP  | `.S`                 | clang (preprocessed asm)   |
| LLVM IR       | `.ll`                | passthrough to VUA/MC      |
 
---
 
## Supported Targets
 
| Flag          | Architecture                        | Notes                          |
|---------------|-------------------------------------|--------------------------------|
| `x86_64`      | Intel / AMD 64-bit                  | Default on most desktops       |
| `x86`         | Intel / AMD 32-bit                  |                                |
| `aarch64`     | ARM 64-bit                          | Apple Silicon, RPi 4, phones   |
| `arm`         | ARM 32-bit                          | Raspberry Pi 2/3, embedded     |
| `riscv64`     | RISC-V 64-bit                       | SiFive, VisionFive             |
| `riscv32`     | RISC-V 32-bit                       | Embedded RISC-V                |
| `mips`        | MIPS big-endian                     | Routers, embedded              |
| `mipsel`      | MIPS little-endian                  |                                |
| `mips64`      | MIPS64 big-endian                   |                                |
| `mips64el`    | MIPS64 little-endian                |                                |
| `ppc`         | PowerPC 32-bit                      | Old Macs, embedded             |
| `ppc64`       | PowerPC 64-bit                      | IBM POWER                      |
| `ppc64le`     | PowerPC 64-bit little-endian        | Modern IBM POWER               |
| `s390x`       | IBM Z (mainframes)                  |                                |
| `hppa`        | HP PA-RISC                          |                                |
| `or1k`        | OpenRISC 1000                       |                                |
| `wasm32`      | WebAssembly 32-bit                  | Run with wasmtime / wasmer     |
| `wasm64`      | WebAssembly 64-bit                  |                                |
| `bpf`         | eBPF (Linux kernel)                 | Use `--ebpf` flag              |
 
---
 
## Installation
 
### Dependencies
 
| Tool              | Required    | Purpose                          |
|-------------------|-------------|----------------------------------|
| LLVM >= 16        | Yes         | IR generation and codegen        |
| clang             | Yes         | C/C++ frontend and linker driver |
| lld               | Yes         | Fast linker backend              |
| cmake >= 3.16     | Yes         | Build system                     |
| ninja             | Yes         | Fast build tool                  |
| gcc               | Yes         | Bootstrap C compiler             |
| fpc               | Optional    | Pascal/FreePascal support        |
| go                | Optional    | Go support                       |
| rust / cargo      | Optional    | Rust support                     |
| bpftool / libbpf  | Optional    | eBPF support                     |
| qemu-user         | Optional    | Cross-arch testing               |
 
---
 
### Fedora / RHEL / CentOS
 
```bash
# Fedora 38+
sudo dnf install -y \
  llvm llvm-devel clang lld \
  cmake ninja-build \
  gcc gcc-c++ \
  fpc golang rust cargo \
  bpftool libbpf-devel \
  kernel-headers \
  qemu-user
 
# RHEL 9 / CentOS Stream 9 — enable CRB repo first
sudo dnf config-manager --set-enabled crb
sudo dnf install -y epel-release
sudo dnf install -y \
  llvm llvm-devel clang lld \
  cmake ninja-build gcc gcc-c++ \
  golang rust cargo
```
 
---
 
### Debian / Ubuntu / Pop!_OS
 
```bash
# Ubuntu 22.04+ / Debian 12+ / Pop!_OS 22.04+
sudo apt update
sudo apt install -y \
  llvm clang lld \
  cmake ninja-build \
  gcc g++ \
  fp-compiler \
  golang-go \
  rustc cargo \
  linux-headers-$(uname -r) \
  bpftool libbpf-dev \
  qemu-user
 
# Ubuntu 20.04 — LLVM may be too old, use the official LLVM apt repo:
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 17
sudo apt install -y clang-17 lld-17
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-17 100
sudo update-alternatives --install /usr/bin/llc   llc   /usr/bin/llc-17   100
```
 
---
 
### Arch Linux / Manjaro
 
```bash
sudo pacman -S --needed \
  llvm clang lld \
  cmake ninja \
  gcc \
  fpc go rust \
  libbpf \
  qemu-user
```
 
---
 
### Gentoo
 
```bash
echo "sys-devel/llvm targets: x86 arm aarch64 riscv wasm" \
  >> /etc/portage/package.use/llvm
 
sudo emerge --ask \
  sys-devel/llvm \
  sys-devel/clang \
  sys-devel/lld \
  dev-util/cmake \
  dev-util/ninja \
  sys-devel/gcc \
  dev-lang/fpc \
  dev-lang/go \
  dev-lang/rust \
  dev-util/bpftool
```
 
---
 
### openSUSE
 
```bash
# Tumbleweed
sudo zypper install -y \
  llvm clang lld cmake ninja gcc gcc-c++ \
  fpc go rust libbpf-devel bpftool qemu-linux-user
 
# Leap 15.5+ — newer LLVM via devel:tools repo
sudo zypper addrepo \
  https://download.opensuse.org/repositories/devel:tools/openSUSE_Leap_15.5/devel:tools.repo
sudo zypper refresh
sudo zypper install -y llvm17 clang17 lld17
```
 
---
 
### Solus
 
```bash
sudo eopkg install -y llvm clang lld cmake ninja gcc golang rust linux-headers
 
# FreePascal is not in Solus repos — build from source:
wget https://sourceforge.net/projects/freepascal/files/Linux/3.2.2/fpc-3.2.2.x86_64-linux.tar
tar xf fpc-3.2.2.x86_64-linux.tar
cd fpc-3.2.2.x86_64-linux && sudo ./install.sh
```
 
---
 
### Slackware
 
```bash
# Slackware 15.0 — use SlackBuilds.org
sbopkg -i llvm
sbopkg -i cmake
sbopkg -i ninja
 
# Rust via rustup
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source "$HOME/.cargo/env"
 
# FreePascal
wget https://sourceforge.net/projects/freepascal/files/Linux/3.2.2/fpc-3.2.2.x86_64-linux.tar
tar xf fpc-3.2.2.x86_64-linux.tar
cd fpc-3.2.2.x86_64-linux && sudo ./install.sh
```
 
---
 
### Alpine Linux
 
Alpine uses musl libc and apk. VeroCC builds and runs cleanly on Alpine.
 
```bash
# Enable community repo if not already enabled
echo "https://dl-cdn.alpinelinux.org/alpine/edge/community" \
  >> /etc/apk/repositories
apk update
 
apk add --no-cache \
  llvm llvm-dev clang lld \
  cmake ninja \
  gcc g++ musl-dev \
  go rust cargo \
  linux-headers \
  qemu-x86_64
 
# FreePascal — build from source on Alpine (musl static)
wget https://sourceforge.net/projects/freepascal/files/Linux/3.2.2/fpc-3.2.2.x86_64-linux.tar
tar xf fpc-3.2.2.x86_64-linux.tar
cd fpc-3.2.2.x86_64-linux && sudo ./install.sh
```
 
> **Note:** When cross-compiling on Alpine, use `-static` since Alpine's shared
> libraries use musl paths that differ from glibc-based distros.
 
---
 
### NixOS
 
NixOS uses declarative configuration. Add VeroCC dependencies to your system or
use a dev shell.
 
```bash
# Option 1: nix-shell for a quick build environment
nix-shell -p llvm clang lld cmake ninja gcc go rustup fpc
 
# Option 2: add to configuration.nix
# environment.systemPackages = with pkgs; [
#   llvm clang lld cmake ninja gcc go rustup fpc
# ];
# Then: sudo nixos-rebuild switch
 
# Option 3: flake dev shell — create flake.nix in the repo root:
cat > flake.nix << 'FLAKE'
{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  outputs = { self, nixpkgs }: {
    devShells.x86_64-linux.default = let
      pkgs = nixpkgs.legacyPackages.x86_64-linux;
    in pkgs.mkShell {
      buildInputs = with pkgs; [
        llvm clang lld cmake ninja gcc go rustup fpc
        llvmPackages.libllvm
      ];
    };
  };
}
FLAKE
nix develop
```
 
> **Note:** On NixOS, LLVM libraries are in non-standard paths.
> Set `LLVM_DIR` before cmake:
> ```bash
> export LLVM_DIR=$(llvm-config --cmakedir)
> cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
> ```
 
---
 
### FreeBSD
 
```bash
# FreeBSD 13+ / 14+
pkg install -y \
  llvm clang lld \
  cmake ninja \
  gcc \
  go rust \
  fpc
 
# Rust via rustup if the pkg version is too old
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source "$HOME/.cargo/env"
 
# qemu-user for cross-arch testing
pkg install -y qemu-user-static
```
 
> **Note:** On FreeBSD, `gmake` and `gninja` may be needed. The build command
> becomes `cmake .. -G Ninja && ninja`. If clang is the system compiler (it is
> by default on FreeBSD 10+), everything works as-is.
 
---
 
### OpenBSD
 
```bash
# OpenBSD 7.4+
pkg_add llvm cmake ninja go rust fpc
 
# lld is bundled with LLVM on OpenBSD
# Rust may need rustup for a recent enough version:
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```
 
> **Note:** OpenBSD's default stack protector and W^X may require
> `-DCMAKE_EXE_LINKER_FLAGS="-Wl,-z,wxneeded"` when building.
 
---
 
### NetBSD
 
```bash
# NetBSD 10+ via pkgsrc
pkgin install llvm clang cmake ninja-build gcc12 go rust fpc
 
# Or build from pkgsrc directly:
cd /usr/pkgsrc/devel/llvm && make install
cd /usr/pkgsrc/devel/cmake && make install
```
 
---
 
### Linux From Scratch (LFS)
 
```bash
# Build LLVM + Clang + LLD from source
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.6/llvm-project-17.0.6.src.tar.xz
tar xf llvm-project-17.0.6.src.tar.xz
cd llvm-project-17.0.6.src
mkdir build && cd build
cmake ../llvm \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DLLVM_TARGETS_TO_BUILD="X86;AArch64;RISCV;ARM;Mips;PowerPC;SystemZ;WebAssembly;BPF" \
  -DLLVM_BUILD_LLVM_DYLIB=ON \
  -DLLVM_LINK_LLVM_DYLIB=ON \
  -G Ninja
ninja -j$(nproc) && sudo ninja install
 
# Build CMake
wget https://cmake.org/files/v3.27/cmake-3.27.7.tar.gz
tar xf cmake-3.27.7.tar.gz && cd cmake-3.27.7
./bootstrap --prefix=/usr -- -DCMAKE_BUILD_TYPE=Release
make -j$(nproc) && sudo make install
 
# Install Ninja
wget https://github.com/ninja-build/ninja/releases/download/v1.11.1/ninja-linux.zip
unzip ninja-linux.zip -d /usr/local/bin
```
 
---
 
### macOS
 
```bash
# Install Xcode CLI tools
xcode-select --install
 
# Install Homebrew
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
 
# Install dependencies
brew install llvm cmake ninja go rust fpc
 
# Add LLVM to PATH (Homebrew installs it keg-only)
echo 'export PATH="/opt/homebrew/opt/llvm/bin:$PATH"' >> ~/.zshrc
echo 'export LDFLAGS="-L/opt/homebrew/opt/llvm/lib"'  >> ~/.zshrc
echo 'export CPPFLAGS="-I/opt/homebrew/opt/llvm/include"' >> ~/.zshrc
source ~/.zshrc
 
# Verify
llvm-config --version
clang --version
```
 
---
 
### Windows (WSL2)
 
```powershell
# In PowerShell as Administrator:
wsl --install
# Restart, open Ubuntu from Start menu, set your username/password.
```
 
```bash
# Inside WSL2 Ubuntu:
sudo apt update
sudo apt install -y \
  llvm clang lld cmake ninja-build \
  gcc g++ fp-compiler golang-go rustc cargo \
  bpftool libbpf-dev
```
 
---
 
## Building VeroCC
 
These steps are identical on every supported platform.
 
```bash
# 1. Clone
git clone https://github.com/VeroTRUS/VeroCC.git
cd VeroCC
 
# 2. Configure
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
 
# 3. Compile
ninja -j$(nproc)
 
# 4. Install  (adds vcc and vcp to /usr/local/bin)
sudo ninja install
 
# 5. Verify
vcc --version
```
 
Expected output:
```
VeroCC (Vero Compiler Collection) v0.1.5
  VUC/A  (Vero Universal C-to-Assembly)
  VUA/MC (Vero Universal Assembly-to-Machine-Code)
  VUMC   (Vero Universal Machine Code)
```
 
---
 
## Usage
 
### Basic Compilation
 
```bash
vcc hello.c   -o hello && ./hello   # C
vcc hello.cpp -o hello && ./hello   # C++
vcc hello.pas -o hello && ./hello   # Pascal
vcc hello.go  -o hello && ./hello   # Go
vcc hello.rs  -o hello && ./hello   # Rust
vcc hello.s   -o hello && ./hello   # Assembly
```
 
---
 
### Inspect the Pipeline
 
```bash
vcc hello.c -E          -o hello_pp.c  # Preprocess only (uses VCP)
vcc hello.c --emit-ir   -o hello.ll    # Emit LLVM IR
vcc hello.c -S          -o hello.s     # Emit native assembly
vcc hello.c -c          -o hello.o     # Compile to object only
```
 
---
 
### VCP — Vero C Preprocessor
 
VCP is VeroCC's own standalone C preprocessor. It handles `#define`, `#undef`,
`#ifdef`, `#ifndef`, `#if`, `#elif`, `#else`, `#endif`, `#include`, `#error`,
`#warning`, and `#pragma` — without depending on the system `cpp`.
 
System headers (`#include <...>`) are passed through untouched so that clang
handles their complex feature-detection guards. User headers (`#include "..."`)
are expanded natively by VCP.
 
```bash
# Preprocess a file with VCP standalone
vcp hello.c -o hello_preprocessed.c
 
# Define macros
vcp hello.c -DDEBUG -DVERSION=2 -o out.c
 
# Add include directories
vcp hello.c -I./include -o out.c
 
# VCP is also invoked automatically by vcc for all C files
vcc hello.c -E -o hello_preprocessed.c
```
 
---
 
### Optimization
 
```bash
vcc program.c -O0 -o program   # No optimization (default)
vcc program.c -O1 -o program   # Light
vcc program.c -O2 -o program   # Recommended for releases
vcc program.c -O3 -o program   # Aggressive
```
 
---
 
### Debug Info
 
```bash
vcc program.c -g -o program
gdb ./program
```
 
---
 
### Preprocessor Macros and Include Paths
 
```bash
vcc program.c -DDEBUG -DVERSION=2 -I./include -o program
```
 
---
 
### Libraries
 
```bash
vcc program.c -lm -o program
vcc program.c -lssl -lcrypto -o program
vcc program.c -L/usr/local/lib -lmylib -o program
```
 
---
 
### Special Output Types
 
```bash
vcc mylib.c -shared -fPIC -o libmylib.so   # Shared library
vcc program.c -static -o program            # Fully static binary
vcc program.c -fPIE   -o program            # PIE executable
```
 
---
 
### Cross-Compilation
 
```bash
vcc hello.c -target aarch64  -o hello_arm64
vcc hello.c -target riscv64  -o hello_riscv64
vcc hello.c -target s390x    -o hello_s390x
vcc hello.c -target mips     -o hello_mips
vcc hello.c -target ppc64le  -o hello_ppc64le
vcc hello.c -target wasm32   -o hello.wasm
vcc hello_ebpf.c --ebpf      -o hello_ebpf.o
```
 
---
 
### All Flags Reference
 
| Flag                          | Description                                    |
|-------------------------------|------------------------------------------------|
| `-o <file>`                   | Output file name                               |
| `-c`                          | Compile to object file only, do not link       |
| `-S`                          | Emit native assembly                           |
| `--emit-ir`                   | Emit LLVM IR (.ll)                             |
| `-E`                          | Preprocess only (uses VCP)                     |
| `-O0` `-O1` `-O2` `-O3`       | Optimization level (default: 0)                |
| `-g`                          | Include debug information                      |
| `-v`                          | Verbose: print every pipeline command          |
| `-target <arch>`              | Cross-compile target (see Supported Targets)   |
| `--ebpf`                      | Target Linux eBPF                              |
| `-I<dir>`                     | Add include search directory                   |
| `-L<dir>`                     | Add library search directory                   |
| `-l<lib>`                     | Link with library                              |
| `-D<macro>`                   | Define preprocessor macro                      |
| `-fPIC`                       | Position-independent code                      |
| `-fPIE`                       | Position-independent executable                |
| `-shared`                     | Build shared library                           |
| `-static`                     | Link statically                                |
| `--version`                   | Print version and exit                         |
| `-h` / `--help`               | Print usage and exit                           |
 
---
 
## Testing with QEMU
 
Install cross sysroots first:
 
```bash
# Fedora
sudo dnf install -y gcc-aarch64-linux-gnu gcc-riscv64-linux-gnu \
                    gcc-mips-linux-gnu gcc-s390x-linux-gnu qemu-user
 
# Debian / Ubuntu
sudo apt install -y gcc-aarch64-linux-gnu gcc-riscv64-linux-gnu \
                    gcc-mips-linux-gnu gcc-s390x-linux-gnu qemu-user
```
 
Then:
 
```bash
vcc hello.c -target aarch64  -o hello_aarch64
qemu-aarch64 -L /usr/aarch64-linux-gnu ./hello_aarch64
 
vcc hello.c -target riscv64  -o hello_riscv64
qemu-riscv64 -L /usr/riscv64-linux-gnu ./hello_riscv64
 
vcc hello.c -target mips     -o hello_mips
qemu-mips    -L /usr/mips-linux-gnu    ./hello_mips
 
vcc hello.c -target s390x    -o hello_s390x
qemu-s390x   -L /usr/s390x-linux-gnu   ./hello_s390x
 
vcc hello.c -target ppc64le  -o hello_ppc64le
qemu-ppc64le -L /usr/powerpc64le-linux-gnu ./hello_ppc64le
 
vcc hello.c -target wasm32   -o hello.wasm
wasmtime hello.wasm
```
 
---
 
## eBPF Support
 
```bash
vcc --ebpf my_prog.c -o my_prog.o
sudo bpftool prog load my_prog.o /sys/fs/bpf/my_prog type xdp
sudo bpftool prog show
```
 
See `tests/ebpf/hello_ebpf.c` for a full working example.
 
---
 
## verolibc
 
verolibc is VeroCC's own C runtime library, developed as part of the VTRUS project.
It is designed to eventually replace glibc and musl as a lightweight, secure, and
fully independent alternative.
 
### What's implemented in v0.1.5
 
| Module | Functions |
|---|---|
| **syscall** | Direct Linux x86_64 syscall wrappers — zero glibc dependency |
| **malloc** | `vero_malloc`, `vero_calloc`, `vero_realloc`, `vero_free` — mmap-based with coalescing |
| **string** | `vero_memcpy`, `vero_memset`, `vero_memmove`, `vero_strlen`, `vero_strcmp`, `vero_strchr`, `vero_strstr`, `vero_strlcpy`, `vero_strlcat`, and more |
| **stdio** | `vero_printf`, `vero_snprintf`, `vero_vsnprintf`, `vero_puts`, `vero_putchar`, `vero_getchar` — all via direct `write` syscall |
| **stdlib** | `vero_exit`, `vero_abort`, `vero_qsort`, `vero_bsearch`, `vero_atoi`, `vero_rand`, `vero_abs` |
| **math** | `vero_sqrt`, `vero_pow`, `vero_exp`, `vero_log`, `vero_sin`, `vero_cos`, `vero_tan`, `vero_floor`, `vero_ceil`, `vero_round` — SSE2 + Taylor series |
 
### Using verolibc in your code
 
```c
#include <vcc/verolibc.h>
 
int main(void) {
    vero_printf("Hello from verolibc!\n");
 
    void *p = vero_malloc(1024);
    vero_memset(p, 0, 1024);
    vero_free(p);
 
    double x = vero_sqrt(2.0);
    vero_printf("sqrt(2) = %.6f\n", x);
 
    vero_exit(0);
}
```
 
Compile with:
```bash
vcc myprogram.c -I/usr/local/include/vcc -L/usr/local/lib -lverolibc -o myprogram
```
 
### Roadmap toward v1.0.0
 
- File I/O (`vero_fopen`, `vero_fread`, `vero_fwrite`, `vero_fclose`)
- Threading (`vero_pthread_create`, futex wrappers)
- Locale and unicode basics
- Dynamic linker support (`ld-vero.so`)
- Complete POSIX compliance
- Replace all remaining clang/glibc dependencies in vcc itself
---
 
## Changelog
 
### v0.1.5 — Current
- **VCP (Vero C Preprocessor)**: own `#define`/`#ifdef`/`#if`/`#include` engine, system headers passed through to clang untouched
- **verolibc overhaul**: real syscall wrappers (zero glibc dependency), mmap-based malloc, word-aligned string ops, full printf via direct syscall, qsort/bsearch, SSE2 math
- **README**: added Alpine Linux, NixOS, FreeBSD, OpenBSD, NetBSD install guides
- **verolibc** section added to documentation
### v0.1.0 — Initial release
- VUC/A multi-language frontend (C, C++, Pascal, Go, Rust, Assembly)
- VUA/MC: LLVM IR to object code via llc
- VUMC: portable linker driver via clang + lld
- Native C lexer, parser, AST and LLVM codegen
- 19 supported target architectures
- eBPF support via `--ebpf`
- verolibc initial wrapper library
---
 
## License
 
VeroCC is licensed under the **GNU General Public License v3.0**.
 
```
Copyright (C) 2025 VTRUS — Vero Technical Research & Universal Security
 
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
 
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.
 
You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.
```
 
---
 
<div align="center">
<sub>Built with love by <a href="https://github.com/VeroTRUS">VTRUS — Vero Technical Research & Universal Security</a></sub>
</div>
 
