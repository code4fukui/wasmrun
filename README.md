# wasmrun

A small WebAssembly runtime in C.

[日本語版](README.ja.md)

## Limitations

This is a small integer-only runtime intended for learning and experiments.

- WebAssembly binary format v1 only
- Supported sections: type / function / global / export / code
- Only the `i32` value type is supported
- Function results are limited to no result or one `i32` result
- Supported instructions: `i32.const`, local get/set/tee, global get/set, i32 arithmetic, comparisons, bit operations, shifts, `call`, `return`, `drop`, `select`, `block`, `loop`, resultless `if`/`else`, `br`, `br_if`
- Unsupported: import / memory instructions / global exports / table / float / SIMD / multi-value / host functions
- This is not a strict validator; safely running malformed wasm is out of scope

## build

```sh
make
```

## usage

```sh
./wasmrun file.wasm [export] [i32 args...]
```

When `export` is omitted, `main` is called. If `main` is not found, `_start` is tried.

## examples

Prebuilt `.wasm` files are included in `examples`, so you can run them directly.

```sh
./wasmrun examples/add.wasm add 40 2
# 42

./wasmrun examples/fact.wasm fact 5
# 120

./wasmrun examples/max.wasm max 3 -5
# 3

./wasmrun examples/max.wasm max -2 7
# 7
```

`examples/addc.c` can be compiled with the Yasa-C CLI `yasac`.

```sh
deno install -f --allow-import --allow-read --allow-write --global --name yasac https://raw.githubusercontent.com/code4fukui/yasa-c/main/yasac.js
yasac examples/addc.c
./wasmrun examples/addc.wasm add 40 2
# 42
```

After editing a `.wat` file, regenerate the matching `.wasm` with `wat2wasm`.

`wat2wasm` is part of WABT (The WebAssembly Binary Toolkit). Install it if needed:

```sh
# macOS / Homebrew
brew install wabt

# Debian / Ubuntu
sudo apt install wabt
```

To build WABT from source:

```sh
git clone --recursive https://github.com/WebAssembly/wabt
cd wabt
mkdir build
cd build
cmake ..
cmake --build .
```

Regenerate individual examples:

```sh
wat2wasm examples/add.wat -o examples/add.wasm
wat2wasm examples/fact.wat -o examples/fact.wasm
wat2wasm examples/max.wat -o examples/max.wasm
```

Regenerate all examples:

```sh
for f in examples/*.wat; do wat2wasm "$f" -o "${f%.wat}.wasm"; done
```

## embed

Include `wasmrun.h` from another C program. The implementation is header-only.

```c
#include "wasmrun.h"

Wasmrun m;
wasmrun_init(&m);
wasmrun_load(&m, wasm_bytes, wasm_size);

int32_t args[] = { 40, 2 };
int32_t result;
int has_result;
wasmrun_call_export(&m, "add", args, &result, &has_result);

wasmrun_free(&m);
```

Compile:

```sh
cc -std=c99 -I. your_app.c -o your_app
```

Included C sample:

```sh
cc -std=c99 -Wall -Wextra -O2 examples/simple.c -o simple
./simple
# 42
```
