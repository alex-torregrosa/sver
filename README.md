# sver
SystemVerilog Language Server, based on slang

## Requirements
- CMake >= 3.10
- C++17 compiler
- [Boost](https://www.boost.org/) Library

## Compilation

Clone the repo and run
```bash
git submodule update --init --recursive
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```
