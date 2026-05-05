# xv6 with Automatic File Versioning

This project is a modified version of the [xv6-riscv](https://github.com/mit-pdos/xv6-riscv) operating system, which features a custom **Automatic File Versioning System**.

## Features

- **Automatic Snapshots:** Every time a file is opened with the `O_TRUNC` flag (e.g., when a file is overwritten), a backup snapshot of its previous state is automatically created.
- **Snapshot Naming:** Snapshots are saved in the same directory using the format `filename.vN` (e.g., `a.txt.v0`, `a.txt.v1`).
- **Version Rotation (`V_MAX`):** The system enforces a maximum of 3 snapshots per file to conserve disk space. If a 4th snapshot is needed, the system automatically deletes the oldest snapshot before saving the new one.
- **List Versions:** A custom system call and user program `listversions` allows you to view all available snapshots for a given file.
- **Restore Version:** A custom system call and user program `restoreversion` allows you to roll back a file's contents to any of its saved snapshots.

---

## Setup Instructions

### Windows (using WSL)
1. Install **Windows Subsystem for Linux (WSL)** by opening PowerShell as Administrator and running:
   ```bash
   wsl --install
   ```
2. Open your WSL terminal (e.g., Ubuntu) and install the required RISC-V toolchain and QEMU emulator:
   ```bash
   sudo apt-get update
   sudo apt-get install git build-essential gdb-multiarch qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
   ```
3. Navigate to your project directory (Windows drives are automatically mounted under `/mnt/` in WSL):
   ```bash
   cd /mnt/e/study/Projects/CSE323
   ```

### Linux
Install the RISC-V toolchain and QEMU via your package manager. For Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install git build-essential gdb-multiarch qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
```

### macOS
Install the toolchain using Homebrew:
```bash
brew tap riscv/riscv
brew install riscv-tools qemu
```

---

## Compiling and Running

Once your environment is set up and you are in the project folder, build and start the xv6 operating system using:

```bash
make clean
make qemu
```

*(Note: To exit QEMU at any time, press `Ctrl+A`, let go, and then press `X`.)*

---

## Demonstration Guide

Once xv6 is booted and you see the `$` prompt, you can try the following commands to see the versioning system in action!

### 1. Run the Automated Test Suite
The easiest way to verify everything works perfectly is to run the custom test program we built:
```bash
$ versiontest
```
This program will automatically create a file, overwrite it multiple times to trigger snapshots, verify that the `V_MAX=3` limit is strictly enforced, and test both successful and invalid file restorations.

### 2. Manual Step-by-Step Demonstration

**Step A: Create and overwrite a file multiple times**
```bash
$ echo "hello" > test.txt
$ echo "world" > test.txt
$ echo "xv6" > test.txt
$ echo "os" > test.txt
$ echo "project" > test.txt
```

**Step B: List the snapshots**
```bash
$ listversions test.txt
test.txt.v2
test.txt.v3
test.txt.v4
```
*(Notice that `v0` and `v1` were automatically deleted because our system only keeps a maximum of 3 snapshots at a time!)*

**Step C: Check the current file content**
```bash
$ cat test.txt
project
```

**Step D: Restore an older version**
Let's rollback the file to version 3 ("xv6"):
```bash
$ restoreversion test.txt 3
$ cat test.txt
xv6
```

**Step E: View raw files**
You can also see the actual hidden snapshot files sitting in the directory using the standard list command:
```bash
$ ls
```
