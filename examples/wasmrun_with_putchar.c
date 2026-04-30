#include "../wasmrun.h"

#include <stdio.h>
#include <stdlib.h>

static uint8_t *readfile(const char *path, size_t *len) {
  FILE *fp = fopen(path, "rb");
  if (!fp) return NULL;
  fseek(fp, 0, SEEK_END);
  long n = ftell(fp);
  rewind(fp);
  if (n < 0) { fclose(fp); return NULL; }
  uint8_t *p = malloc((size_t)n ? (size_t)n : 1);
  if (!p) { fclose(fp); return NULL; }
  if (fread(p, 1, (size_t)n, fp) != (size_t)n) { free(p); fclose(fp); return NULL; }
  fclose(fp);
  *len = (size_t)n;
  return p;
}

static int host_putchar(Wasmrun *m, void *user, const int32_t *args, int32_t *result) {
  (void)m;
  (void)user;
  (void)result;
  putchar(args[0]);
  return 1;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s file.wasm\n", argv[0]);
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
    fprintf(stderr, "load error: %s\n", wasmrun_error(&m));
    free(data);
    return 1;
  }
  if (!wasmrun_set_import_func(&m, "env", "putchar", host_putchar, NULL)) {
    fprintf(stderr, "import error: %s\n", wasmrun_error(&m));
    wasmrun_free(&m);
    free(data);
    return 1;
  }

  int has_result = 0;
  int32_t result = 0;
  if (!wasmrun_call_export(&m, "main", NULL, &result, &has_result)) {
    fprintf(stderr, "run error: %s\n", wasmrun_error(&m));
    wasmrun_free(&m);
    free(data);
    return 1;
  }

  wasmrun_free(&m);
  free(data);
  return 0;
}
