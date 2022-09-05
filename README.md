# sver
SystemVerilog Language Server, based on slang

## Requirements
- CMake >= 3.10
- C++17 compiler
- [Boost](https://www.boost.org/) Library >= 1.66

## Compilation

Clone the repo and run
```bash
git submodule update --init --recursive
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```

## Usage

Sver can be used with any editor that supports the Language Server Protocol.
All the provided examples assume the `sver` executable is in your `PATH`.

### Vim (using vim-lsp)
```vim
if executable('sver')
    au User lsp_setup call lsp#register_server({
        \ 'name': 'sver',
        \ 'whitelist': ['verilog_systemverilog','verilog','systemverilog'],
        \ 'cmd': {server_info->['sver']},
        \ })
endif
```

### Kate
Use this in `Settings -> Configure Kate -> LSP Client -> User Server Settings`:
```json
{
  "servers": {
    "SystemVerilog": {
      "command": ["sver"]
    }
  }
}
```
