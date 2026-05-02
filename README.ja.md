# wasmrun

C と JavaScript で実装したシンプルなwasmランタイム

[English](README.md)

## 制限

現状は学習用の小さな整数限定ランタイムです。

- Wasm binary v1 のみ
- 対応 section は type / import / function / memory / global / export / code / data のみ
- 値型は `i32` のみ
- 関数の戻り値は 0 個または `i32` 1 個のみ
- 対応命令は `i32.const`、local get/set/tee、global get/set、`i32.load`、`i32.store`、i32 算術、比較、ビット演算、shift、`call`、`return`、`drop`、`select`、`block`、`loop`、戻り値なしの `if`/`else`、`br`、`br_if`
- function import は host callback に bind 可能
- memory growth / global export / table / float / SIMD / multi-value は未対応
- 厳密な検証器ではないため、不正な wasm の安全な実行は目的外

## build

```sh
make
```

## usage

```sh
./wasmrun file.wasm [export] [i32 args...]
```

JavaScript 実装は Deno で実行できます。

```sh
deno run --allow-read main.js file.wasm [export] [i32 args...]
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

`examples/addc.c` は Yasa-C の CLI `yasac` でコンパイルできます。

```sh
deno install -f --allow-import --allow-read --allow-write --global --name yasac https://raw.githubusercontent.com/code4fukui/yasa-c/main/yasac.js
yasac examples/addc.c
./wasmrun examples/addc.wasm add 40 2
# 42
```

function import は埋め込み側のコードで提供できます。`examples/putchar.c` は `env.putchar` を import し、`examples/wasmrun_with_putchar.c` では C の `putchar` に bind しています。

```sh
yasac examples/putchar.c
cc -std=c99 -Wall -Wextra -O2 examples/wasmrun_with_putchar.c -o wasmrun_with_putchar
./wasmrun_with_putchar examples/putchar.wasm
# ABC
```

同じ import は Deno の JavaScript からも bind できます。

```sh
deno run --allow-read examples/wasmrun_with_putchar.js examples/putchar.wasm
# ABC
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

JavaScript からは `Wasmrun.js` を import して使えます。

```js
import { Wasmrun } from "./Wasmrun.js";

const wasm = await Deno.readFile("examples/add.wasm");
const m = new Wasmrun();
m.load(wasm);

const result = m.callExport("add", [40, 2]);
if (result?.hasResult) console.log(result.result);
```

function import は export を呼ぶ前に bind します。

```c
static int host_putchar(Wasmrun *m, void *user, const int32_t *args, int32_t *result) {
  (void)m;
  (void)user;
  (void)result;
  putchar(args[0]);
  return 1;
}

wasmrun_set_import_func(&m, "env", "putchar", host_putchar, NULL);
```

JavaScript API では `setImportFunc` で同じ bind ができます。

```js
const hostPutchar = (_m, _user, args) => {
  Deno.stdout.writeSync(new Uint8Array([args[0] & 255]));
  return 1;
};

m.setImportFunc("env", "putchar", hostPutchar, null);
m.callExport("main");
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
