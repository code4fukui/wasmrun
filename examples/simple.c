#include "../wasmrun.h"

#include <stdio.h>

static const uint8_t add_wasm[] = {
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01, 0x60,
  0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07, 0x07, 0x01,
  0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20,
  0x00, 0x20, 0x01, 0x6a, 0x0b
};

int main(void) {
  Wasmrun m;
  wasmrun_init(&m);
  if (!wasmrun_load(&m, add_wasm, sizeof(add_wasm))) {
    fprintf(stderr, "load error: %s\n", wasmrun_error(&m));
    return 1;
  }

  int32_t args[] = { 40, 2 };
  int32_t result = 0;
  int has_result = 0;
  if (!wasmrun_call_export(&m, "add", args, &result, &has_result)) {
    fprintf(stderr, "run error: %s\n", wasmrun_error(&m));
    wasmrun_free(&m);
    return 1;
  }

  if (has_result) printf("%d\n", result);

  wasmrun_free(&m);
  return 0;
}
