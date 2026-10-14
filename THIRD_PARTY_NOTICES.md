# Third-party notices

flydigi_ds4 is licensed under the GNU General Public License v3.0 (see `LICENSE`).
It builds on the following projects.

## GoldHEN Plugins SDK — MIT

Statically linked into `flydigi_ds4.prx` (hooking, patching, GoldHEN syscalls, PRX CRT).
https://github.com/GoldHEN/GoldHEN_Plugins_SDK (commit `bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7`)

```
MIT License

Copyright (c) 2022 GoldHEN

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## GoldHEN Plugins Repository (gamepad_helper by jocover) — GPL-3.0

`include/pad.h` (libScePad types) and the scePadRead/scePadReadState hooking
technique in `source/main.c` come from the `gamepad_helper` plugin.
https://github.com/GoldHEN/GoldHEN_Plugins_Repository

## OpenOrbis PS4 Toolchain — GPL-3.0

Headers, library stubs, linker script and `create-fself` used to build the PRX.
https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain (release v0.5.3)

## llvm-mingw — Apache-2.0 with LLVM exceptions / various

Compiler and linker used on Windows; not linked into the plugin.
https://github.com/mstorsjo/llvm-mingw (release 20260922)
