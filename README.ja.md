# wasmrun

シンプルなwasmランタイム in C

[English](README.md)

## 制限

現状は学習用の小さな整数限定ランタイムです。

- Wasm binary v1 のみ
- 対応 section は type / function / export / code のみ
- 値型は `i32` のみ
- 関数の戻り値は 0 個または `i32` 1 個のみ
- 対応命令は `i32.const`、local get/set/tee、i32 算術、比較、ビット演算、shift、`call`、`return`、`drop`、`select`、`block`、`loop`、戻り値なしの `if`/`else`、`br`、`br_if`
- import / memory / global / table / float / SIMD / multi-value / host function は未対応
- 厳密な検証器ではないため、不正な wasm の安全な実行は目的外

## build

```sh
make
```

## usage

```sh
./wasmrun file.wasm [export] [i32 args...]
```

`export` 省略時は `main`、なければ `_start` を呼びます。

## examples

`examples` には変換済みの `.wasm` も置いてあるので、そのまま実行できます。

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

`.wat` を編集した後は `wat2wasm` で `.wasm` を再生成できます。

`wat2wasm` は WABT (The WebAssembly Binary Toolkit) に含まれています。未インストールの場合:

```sh
# macOS / Homebrew
brew install wabt

# Debian / Ubuntu
sudo apt install wabt
```

ソースからビルドする場合は WABT のリポジトリを使います。

```sh
git clone --recursive https://github.com/WebAssembly/wabt
cd wabt
mkdir build
cd build
cmake ..
cmake --build .
```

```sh
wat2wasm examples/add.wat -o examples/add.wasm
wat2wasm examples/fact.wat -o examples/fact.wasm
wat2wasm examples/max.wat -o examples/max.wasm
```

まとめて再生成する場合:

```sh
for f in examples/*.wat; do wat2wasm "$f" -o "${f%.wat}.wasm"; done
```

## embed

他の C プログラムから使う場合は `wasmrun.h` を include するだけです。実装もヘッダ内に入っています。

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

コンパイル例:

```sh
cc -std=c99 -I. your_app.c -o your_app
```

同梱の C サンプル:

```sh
cc -std=c99 -Wall -Wextra -O2 examples/simple.c -o simple
./simple
# 42
```
