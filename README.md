# DOS demo/game development template
![screenshot.png](screenshot.png)

Want to relieve the 90ies and create little demos and games for DOS using C/C++ with "modern" tools? Then this project template is for you.

It's a turn key solution to setup a develpoment environment to create DOS demos/apps. consisting of:

* [DJGPP](https://www.delorie.com/djgpp/), the GCC-based C/C++ compiler for DOS (DPMI/protected mode).
* [GDB](https://www.sourceware.org/gdb/), the debugger used to debug the DOS demo/game.
* [DOSBox-x](https://dosbox-x.com/), the bells and whistles DOS-emulator to develop and run the DOS demo/game in.
* [CMake](https://cmake.org/), to define what needs to be build how.
* [Visual Studio Code](https://code.visualstudio.com/), to tie all the above together and provide an integrated development environment.

## Quickstart Guide

### Installation
Install:

* [Git](https://git-scm.com/): On Windows, install [Git for Windows](https://gitforwindows.org/). You will need `Git Bash` to run a shell script later.
* [CMake](https://cmake.org/): ensure `cmake` is in your `PATH`.
* [Visual Studio Code](https://code.visualstudio.com/): ensure [command line interface is installed](https://www.digitalocean.com/community/tutorials/how-to-install-and-use-the-visual-studio-code-vs-code-command-line-interface) and in your `PATH`.

Open a shell (Git Bash on Windows), then:

```
git clone https://github.com/badlogic/dos-dev-template
cd dos-dev-template
./download-tools.sh
```

The `download-tools.sh` script will download everything you need to start developing for DOS.

Once complete, open the `dos-dev-template/` folder in Visual Studio Code.

### Running and debugging
The project consists of a single source file `main.c`. To run it,



## FAQ

Here are a few questions you may have, if you want to dig deeper into the entire setup to modify it to your liking.

### What does `download-tools.sh` do?

It will download the following tools for your operating system to the `tools/` folder:

*  My fork of [GDB 7.1a](https://github.com/badlogic/gdb-7.1a-djgpp), which is specifically build to debug DOS programs compiled with DJGPP remotely, e.g. running in DOSBox-x.
* [DJGPP binaries](https://github.com/badlogic/dosbox-x) provided by [Andrew Wu](https://github.com/andrewwutw)
* My fork of [DOSBox-x](https://github.com/badlogic/dosbox-x), which fixes a few things required for debugging, such as [serial port emulation through TCP/IP on macOS](https://github.com/joncampbell123/dosbox-x/pull/3892).

After downloading the tools, it will install these Visual Studio Code extensions necessary for C/C++ development:

* [clangd](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd), the superior C/C++ code completion, navigation, and insights language service.
* []