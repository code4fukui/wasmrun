#include "wasmrun.h"

#include <stdio.h>
#include <stdlib.h>

static uint8_t *readfile(const char *path, size_t *len) {
  FILE *fp = fopen(path, "rb");
  if (!fp) return NULL;
  fseek(fp, 0, SEEK_END);
  long n = ftell(fp);
  rewind(fp);
  uint8_t *p = malloc(n);
  if (!p) { fclose(fp); return NULL; }
  if (fread(p, 1, n, fp) != (size_t)n) { free(p); fclose(fp); return NULL; }
  fclose(fp);
  *len = n;
  return p;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s file.wasm [export] [i32 args...]\n", argv[0]);
    return 2;
  }

  size_t len;
  uint8_t *data = readfile(argv[1], &len);
  if (!data) {
    fprintf(stderr, "error: cannot read %s\n", argv[1]);
    return 1;
  }

  Wasmrun m;
  wasmrun_init(&m);
  if (!wasmrun_load(&m, data, len)) {
    fprintf(stderr, "error: %s\n", wasmrun_error(&m));
    free(data);
    return 1;
  }

  const char *name = argc > 2 ? argv[2] : "main";
  uint32_t func;
  if (!wasmrun_find_export(&m, name, &func) && (argc > 2 || !wasmrun_find_export(&m, "_start", &func))) {
    fprintf(stderr, "error: %s\n", wasmrun_error(&m));
    wasmrun_free(&m);
    free(data);
    return 1;
  }

  uint32_t nparams = wasmrun_param_count(&m, func);
  if (argc - 3 < (int)nparams) {
    fprintf(stderr, "error: missing i32 args\n");
    wasmrun_free(&m);
    free(data);
    return 1;
  }

  int32_t args[WASMRUN_MAX_PARAMS];
  for (uint32_t i = 0; i < nparams; i++) args[i] = (int32_t)strtol(argv[3 + i], NULL, 0);

  int has_result = 0;
  int32_t result = 0;
  if (!wasmrun_call(&m, func, args, &result, &has_result)) {
    fprintf(stderr, "error: %s\n", wasmrun_error(&m));
    wasmrun_free(&m);
    free(data);
    return 1;
  }
  if (has_result) printf("%d\n", result);

  wasmrun_free(&m);
  free(data);
  return 0;
}
