# NEMU

NEMU(NJU Emulator) is a simple but complete full-system emulator designed for teaching purpose.
Currently it supports x86, mips32, and riscv32.
To build programs run above NEMU, refer to the [AM project](https://github.com/NJU-ProjectN/nexus-am.git).

The main features of NEMU include
* a small monitor with a simple debugger
  * single step
  * register/memory examination
  * expression evaluation without the support of symbols
  * watch point
  * differential testing with reference design (e.g. QEMU)
  * snapshot
* CPU core with support of most common used instructions
  * x86
    * real mode is not supported
    * x87 floating point instructions are not supported
  * mips32
    * CP1 floating point instructions are not supported
  * riscv32
    * only RV32IM
* memory
* paging
  * TLB is optional (but necessary for mips32)
  * protection is not supported
* interrupt and exception
  * protection is not supported
* 5 devices
  * Args Rom, serial, timer, keyboard, VGA
  * most of them are simplified and unprogrammable
* 2 types of I/O
  * port-mapped I/O and memory-mapped I/O

## Howto

### Run OpenSBI and Linux

**All steps below use source code in RISCVERS**

1. Compile a Linux kernel, with proper SD card driver integrated if you want to run Debian or Fedora. Currently Kernel v4.18 is verified to work.
2. Convert ```vmlinux``` to binary format using ```objcopy```.
3. Compile OpenSBI using ```build_linux.sh``` where vmlinux path may need a modification.
4. Compile NEMU intepreter. You may want to change default sdcard image path in ```src/devices/sdcard.c``` to boot Debian or Fedora.
5. launch NEMU intepreter and load ```fw_payload.bin``` generated by OpenSBI.
6. If you are using a ```vmlinux``` with initramfs, you will likely be greeted with a ```Hello```, otherwise you may see startup logs and finally a login prompt from Debian or Fedora if SD card is configured properly.
