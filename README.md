
<div align="center">
# VeroCC — Vero Compiler Collection
 
**A multi-language, multi-architecture compiler collection built on LLVM**
 
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Version](https://img.shields.io/badge/version-0.1.0-green.svg)](https://github.com/VeroTRUS/VeroCC/releases/tag/v0.1.0)
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
  - [Linux From Scratch (LFS)](#linux-from-scratch-lfs)
  - [macOS](#macos)
  - [Windows (WSL2)](#windows-wsl2)
- [Building VeroCC](#building-verocc)
- [Usage](#usage)
  - [Basic Compilation](#basic-compilation)
  - [Cross-Compilation](#cross-compilation)
  - [All Flags Reference](#all-flags-reference)
- [Testing with QEMU](#testing-with-qemu)
- [eBPF Support](#ebpf-support)
- [License](#license)
---
 
## What is VeroCC?
 
VeroCC is the compiler frontend of the **VUASM (Vero Universal Assembler)** family,
a set of tools developed by VTRUS for universal code compilation across every major
CPU architecture and operating system.
 
VeroCC accepts source code in **C, C++, Pascal, Go, Rust, and Assembly** and drives
it through a clean pipeline down to a native binary — or a cross-compiled binary for
any architecture LLVM supports.
 
The command to invoke the compiler is **`vcc`**.
 
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
│   VUC/A — Vero Universal C-to-Assembly                   │
│   Translates source → LLVM IR  (.ll)                     │
│                                                          │
│   • C        → native lexer + parser + LLVM codegen      │
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
| C             | `.c`                 | VUC/A native + clang       |
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
 
VeroCC requires the following tools. Language-specific tools (fpc, go, rustc) are
optional — you only need them if you want to compile that language.
 
| Tool              | Required | Purpose                          |
|-------------------|----------|----------------------------------|
| LLVM ≥ 16         | ✅ Yes   | IR generation and codegen        |
| clang             | ✅ Yes   | C/C++ frontend and linker driver |
| lld               | ✅ Yes   | Fast linker backend              |
| cmake ≥ 3.16      | ✅ Yes   | Build system                     |
| ninja             | ✅ Yes   | Fast build tool                  |
| gcc               | ✅ Yes   | Bootstrap C compiler             |
| fpc               | ⚡ Optional | Pascal/FreePascal support     |
| go                | ⚡ Optional | Go support                    |
| rust / cargo      | ⚡ Optional | Rust support                  |
| bpftool / libbpf  | ⚡ Optional | eBPF support                  |
| qemu-user         | ⚡ Optional | Cross-arch testing            |
 
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
 
# Ubuntu 20.04 — LLVM may be too old, use the LLVM apt repo:
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 17
sudo apt install -y clang-17 lld-17
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-17 100
sudo update-alternatives --install /usr/bin/llc llc /usr/bin/llc-17 100
```
 
---
 
### Arch Linux / Manjaro
 
```bash
sudo pacman -S --needed \
  llvm clang lld \
  cmake ninja \
  gcc \
  fpc go rust \
  bpf libbpf \
  qemu-user
 
# AUR helpers (optional, for tinygo etc.)
# yay -S tinygo
```
 
---
 
### Gentoo
 
```bash
# Add USE flags
echo "sys-devel/llvm targets: x86 arm aarch64 riscv wasm" >> /etc/portage/package.use/llvm
 
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
# openSUSE Tumbleweed
sudo zypper install -y \
  llvm clang lld \
  cmake ninja \
  gcc gcc-c++ \
  fpc go rust \
  libbpf-devel bpftool \
  qemu-linux-user
 
# openSUSE Leap 15.5+ — use devel:tools repo for newer LLVM
sudo zypper addrepo https://download.opensuse.org/repositories/devel:tools/openSUSE_Leap_15.5/devel:tools.repo
sudo zypper refresh
sudo zypper install -y llvm17 clang17 lld17
```
 
---
 
### Solus
 
```bash
sudo eopkg install -y \
  llvm clang lld \
  cmake ninja \
  gcc \
  golang rust \
  linux-headers
 
# fpc is not in Solus repos — build from source:
wget https://sourceforge.net/projects/freepascal/files/Linux/3.2.2/fpc-3.2.2.x86_64-linux.tar
tar xf fpc-3.2.2.x86_64-linux.tar
cd fpc-3.2.2.x86_64-linux
sudo ./install.sh
```
 
---
 
### Slackware
 
```bash
# Slackware 15.0
# Install from SlackBuilds.org
 
# 1. LLVM
sbopkg -i llvm
 
# 2. CMake (if not present)
sbopkg -i cmake
 
# 3. Ninja
sbopkg -i ninja
 
# 4. Go
sbopkg -i go
 
# 5. Rust — use rustup
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source "$HOME/.cargo/env"
 
# 6. FreePascal
wget https://sourceforge.net/projects/freepascal/files/Linux/3.2.2/fpc-3.2.2.x86_64-linux.tar
tar xf fpc-3.2.2.x86_64-linux.tar && cd fpc-3.2.2.x86_64-linux
sudo ./install.sh
```
 
---
 
### Linux From Scratch (LFS)
 
LFS requires building everything from source. Follow these steps after your base LFS system is up.
 
```bash
# 1. Build LLVM + Clang + LLD (BLFS chapter)
# Download LLVM source
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
ninja -j$(nproc)
sudo ninja install
 
# 2. Build cmake if not present
wget https://cmake.org/files/v3.27/cmake-3.27.7.tar.gz
tar xf cmake-3.27.7.tar.gz && cd cmake-3.27.7
./bootstrap --prefix=/usr -- -DCMAKE_BUILD_TYPE=Release
make -j$(nproc) && sudo make install
 
# 3. Build ninja
wget https://github.com/ninja-build/ninja/releases/download/v1.11.1/ninja-linux.zip
unzip ninja-linux.zip -d /usr/local/bin
 
# Then proceed to Building VeroCC below
```
 
---
 
### macOS
 
> Requires Xcode Command Line Tools and Homebrew.
 
```bash
# 1. Install Xcode CLI tools
xcode-select --install
 
# 2. Install Homebrew (if not present)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
 
# 3. Install dependencies
brew install llvm cmake ninja go rust fpc
 
# 4. Add LLVM to PATH (Homebrew installs it keg-only)
echo 'export PATH="/opt/homebrew/opt/llvm/bin:$PATH"' >> ~/.zshrc
echo 'export LDFLAGS="-L/opt/homebrew/opt/llvm/lib"' >> ~/.zshrc
echo 'export CPPFLAGS="-I/opt/homebrew/opt/llvm/include"' >> ~/.zshrc
source ~/.zshrc
 
# Verify
llvm-config --version   # should print 17.x or newer
clang --version
```
 
---
 
### Windows (WSL2)
 
```bash
# 1. Install WSL2 from PowerShell (run as Administrator):
#    wsl --install
#    Then open Ubuntu from the Start menu and set up your user.
 
# 2. Inside WSL2 Ubuntu terminal:
sudo apt update
sudo apt install -y \
  llvm clang lld \
  cmake ninja-build \
  gcc g++ \
  fp-compiler \
  golang-go \
  rustc cargo \
  bpftool libbpf-dev
 
# Then proceed to Building VeroCC below
```
 
---
 
## Building VeroCC
 
These steps are the same on **every supported platform** once dependencies are installed.
 
```bash
# 1. Clone the repository
git clone https://github.com/VeroTRUS/VeroCC.git
cd VeroCC
 
# 2. Create build directory and configure
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
 
# 3. Compile
ninja -j$(nproc)
 
# 4. Install system-wide (adds 'vcc' to /usr/local/bin)
sudo ninja install
 
# 5. Verify
vcc --version
```
 
Expected output:
```
VeroCC (Vero Compiler Collection) v0.1.0
  VUC/A  (Vero Universal C-to-Assembly)
  VUA/MC (Vero Universal Assembly-to-Machine-Code)
  VUMC   (Vero Universal Machine Code)
```
 
---
 
## Usage
 
### Basic Compilation
 
```bash
# Compile a C program
vcc hello.c -o hello
./hello
 
# Compile a C++ program
vcc hello.cpp -o hello
./hello
 
# Compile Pascal
vcc hello.pas -o hello
./hello
 
# Compile Go
vcc hello.go -o hello
./hello
 
# Compile Rust
vcc hello.rs -o hello
./hello
 
# Compile Assembly
vcc hello.s -o hello
./hello
```
 
---
 
### Inspect the Pipeline
 
```bash
# Stop after preprocessing (C only)
vcc hello.c -E -o hello_preprocessed.c
 
# Emit LLVM IR (see what VUC/A produces)
vcc hello.c --emit-ir -o hello.ll
cat hello.ll
 
# Emit native assembly (see what VUA/MC produces)
vcc hello.c -S -o hello.s
cat hello.s
 
# Compile to object file only (no linking)
vcc hello.c -c -o hello.o
file hello.o
```
 
---
 
### Optimization
 
```bash
vcc program.c -O0 -o program   # No optimization (debug-friendly, default)
vcc program.c -O1 -o program   # Light optimization
vcc program.c -O2 -o program   # Recommended for release builds
vcc program.c -O3 -o program   # Aggressive optimization
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
# Define macros
vcc program.c -DDEBUG -DVERSION=2 -o program
 
# Add include directories
vcc program.c -I/usr/local/include -I./include -o program
 
# Combined
vcc program.c -I./include -DNDEBUG -O2 -o program
```
 
---
 
### Libraries
 
```bash
# Link a library (e.g. libm, libssl)
vcc program.c -lm -o program
vcc program.c -lssl -lcrypto -o program
 
# Add a library search path
vcc program.c -L/usr/local/lib -lmylib -o program
```
 
---
 
### Special Output Types
 
```bash
# Build a shared library (.so)
vcc mylib.c -shared -fPIC -o libmylib.so
 
# Build a static-linked binary (no runtime dependencies)
vcc program.c -static -o program
 
# Build a position-independent executable
vcc program.c -fPIE -o program
```
 
---
 
### Cross-Compilation
 
```bash
# Compile for AArch64 (ARM 64-bit)
vcc hello.c -target aarch64 -o hello_arm64
 
# Compile for RISC-V 64-bit
vcc hello.c -target riscv64 -o hello_riscv64
 
# Compile for IBM Z (mainframe)
vcc hello.c -target s390x -o hello_s390x
 
# Compile for MIPS
vcc hello.c -target mips -o hello_mips
 
# Compile for WebAssembly
vcc hello.c -target wasm32 -o hello.wasm
 
# Compile for eBPF (Linux kernel)
vcc hello_ebpf.c --ebpf -o hello_ebpf.o
```
 
---
 
### All Flags Reference
 
| Flag                  | Description                                          |
|-----------------------|------------------------------------------------------|
| `-o <file>`           | Output file name                                     |
| `-c`                  | Compile to object file only, do not link             |
| `-S`                  | Emit native assembly, do not assemble                |
| `--emit-ir`           | Emit LLVM IR (.ll), do not compile further           |
| `-E`                  | Preprocess only                                      |
| `-O0` / `-O1` / `-O2` / `-O3` | Optimization level (default: 0)            |
| `-g`                  | Include debug information                            |
| `-v`                  | Verbose: show every pipeline command being run       |
| `-target <arch>`      | Cross-compile for target architecture                |
| `--ebpf`              | Target eBPF (Linux kernel programs)                  |
| `-I<dir>`             | Add directory to include search path                 |
| `-L<dir>`             | Add directory to library search path                 |
| `-l<lib>`             | Link with library                                    |
| `-D<macro>`           | Define preprocessor macro                            |
| `-fPIC`               | Generate position-independent code                   |
| `-fPIE`               | Generate position-independent executable             |
| `-shared`             | Build a shared library (.so)                         |
| `-static`             | Link statically (no shared library dependencies)     |
| `--version`           | Print version information and exit                   |
| `-h` / `--help`       | Print usage information and exit                     |
 
---
 
## Testing with QEMU
 
After cross-compiling, use QEMU user-mode emulation to run the binary on your x86_64 machine.
 
First install the cross-compilation sysroots:
 
```bash
# Fedora
sudo dnf install -y \
  gcc-aarch64-linux-gnu \
  gcc-riscv64-linux-gnu \
  gcc-mips-linux-gnu \
  gcc-s390x-linux-gnu \
  qemu-user
 
# Debian / Ubuntu
sudo apt install -y \
  gcc-aarch64-linux-gnu \
  gcc-riscv64-linux-gnu \
  gcc-mips-linux-gnu \
  gcc-s390x-linux-gnu \
  qemu-user
```
 
Then compile and run:
 
```bash
# AArch64
vcc hello.c -target aarch64 -o hello_aarch64
qemu-aarch64 -L /usr/aarch64-linux-gnu ./hello_aarch64
 
# RISC-V 64
vcc hello.c -target riscv64 -o hello_riscv64
qemu-riscv64 -L /usr/riscv64-linux-gnu ./hello_riscv64
 
# MIPS
vcc hello.c -target mips -o hello_mips
qemu-mips -L /usr/mips-linux-gnu ./hello_mips
 
# IBM Z / S390x
vcc hello.c -target s390x -o hello_s390x
qemu-s390x -L /usr/s390x-linux-gnu ./hello_s390x
 
# PowerPC 64 LE
vcc hello.c -target ppc64le -o hello_ppc64le
qemu-ppc64le -L /usr/powerpc64le-linux-gnu ./hello_ppc64le
 
# WebAssembly (needs wasmtime)
vcc hello.c -target wasm32 -o hello.wasm
wasmtime hello.wasm
```
 
---
 
## eBPF Support
 
VeroCC can compile eBPF programs for the Linux kernel.
 
```bash
# Compile an eBPF program
vcc --ebpf my_prog.c -o my_prog.o
 
# Load it with bpftool
sudo bpftool prog load my_prog.o /sys/fs/bpf/my_prog type xdp
 
# Inspect it
sudo bpftool prog show
sudo bpftool map show
```
 
See `tests/ebpf/hello_ebpf.c` for a working example using the `vcc_ebpf.h` helper header.
 
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
<sub>Built with ❤️  by <a href="https://github.com/VeroTRUS">VTRUS — Vero Technical Research & Universal Security</a></sub>
</div>
