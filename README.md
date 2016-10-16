# xbash

A convenience tool to launch an X server like Xming before calling the "Bash on
Ubuntu on Windows" (WSL) bash.

All it does is lookup the Xming installation path (via its association to
.xlaunch files).

## Arguments

All arguments except those starting with "--xbash" are passed straight to the
bash executable.

* You can pass an "--xbash-xserver [cmdline for x server]" argument to launch a
  different X server or pass custom commandline arguments.
* You can pass an "--xbash-launcher [path to bash executable]" argument to
  launch bash from a non-standard path. You can also use this to e.g. chain
  xbash with [outbash](https://github.com/xilun/cbwin/releases) from
  [xilun/cbwin](https://github.com/xilun/cbwin). In fact, xbash was derived from
  outbash and half of the code in xbash is basically still the same. Its just
  less, because xbash doesn't do much :)

## Installation

[Binary releases](https://github.com/timfel/xbash/releases) are available. To
build `xbash.exe` from source, you'll need Visual C++ 2015 and CMake. However
you retrieve it, just from then on use `xbash.exe` instead of `bash.exe`.
