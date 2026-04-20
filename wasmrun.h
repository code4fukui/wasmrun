#ifndef WASMRUN_H
#define WASMRUN_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WASMRUN_MAX_TYPES 128
#define WASMRUN_MAX_FUNCS 128
#define WASMRUN_MAX_EXPORTS 128
#define WASMRUN_MAX_PARAMS 16
#define WASMRUN_MAX_LOCALS 256

typedef struct {
  uint32_t nparams;
  int32_t result_type;
} WasmrunSig;

typedef struct {
  uint32_t type;
  const uint8_t *code;
  const uint8_t *end;
  uint32_t nlocals;
  uint8_t locals[WASMRUN_MAX_LOCALS];
} WasmrunFunc;

typedef struct {
  char *name;
  uint32_t func;
} WasmrunExport;

typedef struct {
  WasmrunSig types[WASMRUN_MAX_TYPES];
  uint32_t ntypes;
  WasmrunFunc funcs[WASMRUN_MAX_FUNCS];
  uint32_t nfuncs;
  WasmrunExport exports[WASMRUN_MAX_EXPORTS];
  uint32_t nexports;
  const char *error;
} Wasmrun;

typedef struct { const uint8_t *p, *end; } WasmrunR;
typedef struct { uint8_t op; const uint8_t *start, *end; int height; } WasmrunCtrl;

static inline void wasmrun_init(Wasmrun *m);
static inline void wasmrun_free(Wasmrun *m);
static inline int wasmrun_load(Wasmrun *m, const uint8_t *data, size_t len);
static inline int wasmrun_find_export(Wasmrun *m, const char *name, uint32_t *func);
static inline int wasmrun_call(Wasmrun *m, uint32_t func, const int32_t *args, int32_t *result, int *has_result);
static inline int wasmrun_call_export(Wasmrun *m, const char *name, const int32_t *args, int32_t *result, int *has_result);
static inline uint32_t wasmrun_param_count(Wasmrun *m, uint32_t func);
static inline const char *wasmrun_error(Wasmrun *m);

static void wasmrun_seterr_(Wasmrun *m, const char *s) { if (!m->error) m->error = s; }
static int wasmrun_fail_(Wasmrun *m, const char *s) { wasmrun_seterr_(m, s); return 0; }
static int wasmrun_u8_(Wasmrun *m, WasmrunR *r, uint8_t *v) { if (r->p >= r->end) return wasmrun_fail_(m, "unexpected eof"); *v = *r->p++; return 1; }
static int wasmrun_leb_(Wasmrun *m, WasmrunR *r, uint32_t *v) {
  uint32_t x = 0, s = 0;
  for (;;) {
    uint8_t b; if (!wasmrun_u8_(m, r, &b)) return 0;
    x |= (uint32_t)(b & 127) << s;
    if (!(b & 128)) { *v = x; return 1; }
    s += 7; if (s >= 35) return wasmrun_fail_(m, "bad leb128");
  }
}
static int wasmrun_sleb_(Wasmrun *m, WasmrunR *r, int32_t *v) {
  int32_t x = 0, s = 0; uint8_t b;
  do {
    if (!wasmrun_u8_(m, r, &b)) return 0;
    x |= (int32_t)(b & 127) << s; s += 7;
  } while (b & 128);
  if (s < 32 && (b & 64)) x |= -(1 << s);
  *v = x; return 1;
}
static int wasmrun_bytes_(Wasmrun *m, WasmrunR *r, const uint8_t **p, uint32_t n) {
  if ((uint32_t)(r->end - r->p) < n) return wasmrun_fail_(m, "short section");
  *p = r->p; r->p += n; return 1;
}
static int wasmrun_read32_(Wasmrun *m, WasmrunR *r, uint32_t *v) {
  uint8_t a, b, c, d;
  if (!wasmrun_u8_(m, r, &a) || !wasmrun_u8_(m, r, &b) || !wasmrun_u8_(m, r, &c) || !wasmrun_u8_(m, r, &d)) return 0;
  *v = a | (uint32_t)b << 8 | (uint32_t)c << 16 | (uint32_t)d << 24; return 1;
}

static int wasmrun_find_end_(Wasmrun *m, const uint8_t *p, const uint8_t *end, const uint8_t **endp) {
  int depth = 1; WasmrunR r = { p, end };
  while (r.p < r.end) {
    uint8_t op; if (!wasmrun_u8_(m, &r, &op)) return 0;
    if (op == 0x02 || op == 0x03 || op == 0x04) { int32_t x; if (!wasmrun_sleb_(m, &r, &x)) return 0; depth++; }
    else if (op == 0x0b && --depth == 0) { *endp = r.p - 1; return 1; }
    else if (op == 0x0c || op == 0x0d || op == 0x10 || op == 0x20 || op == 0x21 || op == 0x22) { uint32_t x; if (!wasmrun_leb_(m, &r, &x)) return 0; }
    else if (op == 0x41) { int32_t x; if (!wasmrun_sleb_(m, &r, &x)) return 0; }
  }
  return wasmrun_fail_(m, "missing end");
}

static int wasmrun_find_else_end_(Wasmrun *m, const uint8_t *p, const uint8_t *end, const uint8_t **elsep, const uint8_t **endp) {
  int depth = 1; WasmrunR r = { p, end }; *elsep = NULL;
  while (r.p < r.end) {
    uint8_t op; if (!wasmrun_u8_(m, &r, &op)) return 0;
    if (op == 0x02 || op == 0x03 || op == 0x04) { int32_t x; if (!wasmrun_sleb_(m, &r, &x)) return 0; depth++; }
    else if (op == 0x05 && depth == 1) *elsep = r.p - 1;
    else if (op == 0x0b && --depth == 0) { *endp = r.p - 1; return 1; }
    else if (op == 0x0c || op == 0x0d || op == 0x10 || op == 0x20 || op == 0x21 || op == 0x22) { uint32_t x; if (!wasmrun_leb_(m, &r, &x)) return 0; }
    else if (op == 0x41) { int32_t x; if (!wasmrun_sleb_(m, &r, &x)) return 0; }
  }
  return wasmrun_fail_(m, "missing if end");
}

static inline void wasmrun_init(Wasmrun *m) { memset(m, 0, sizeof(*m)); }

static inline void wasmrun_free(Wasmrun *m) {
  for (uint32_t i = 0; i < m->nexports; i++) free(m->exports[i].name);
  wasmrun_init(m);
}

static inline int wasmrun_load(Wasmrun *m, const uint8_t *data, size_t len) {
  wasmrun_free(m);
  WasmrunR r = { data, data + len }; uint32_t magic, version;
  if (!wasmrun_read32_(m, &r, &magic) || !wasmrun_read32_(m, &r, &version)) return 0;
  if (magic != 0x6d736100 || version != 1) return wasmrun_fail_(m, "not wasm32 v1");
  while (r.p < r.end) {
    uint8_t id; uint32_t size;
    if (!wasmrun_u8_(m, &r, &id) || !wasmrun_leb_(m, &r, &size)) return 0;
    WasmrunR s = { r.p, r.p + size }; if (s.end > r.end) return wasmrun_fail_(m, "bad section size");
    if (id == 1) {
      if (!wasmrun_leb_(m, &s, &m->ntypes)) return 0;
      if (m->ntypes > WASMRUN_MAX_TYPES) return wasmrun_fail_(m, "too many types");
      for (uint32_t i = 0; i < m->ntypes; i++) {
        uint8_t form; if (!wasmrun_u8_(m, &s, &form)) return 0; if (form != 0x60) return wasmrun_fail_(m, "bad type");
        WasmrunSig *t = &m->types[i]; if (!wasmrun_leb_(m, &s, &t->nparams)) return 0;
        if (t->nparams > WASMRUN_MAX_PARAMS) return wasmrun_fail_(m, "too many params");
        for (uint32_t j = 0; j < t->nparams; j++) { uint8_t vt; if (!wasmrun_u8_(m, &s, &vt)) return 0; if (vt != 0x7f) return wasmrun_fail_(m, "only i32 params"); }
        uint32_t nr; if (!wasmrun_leb_(m, &s, &nr)) return 0; if (nr > 1) return wasmrun_fail_(m, "too many results");
        if (nr) { uint8_t vt; if (!wasmrun_u8_(m, &s, &vt)) return 0; if (vt != 0x7f) return wasmrun_fail_(m, "only i32 result"); t->result_type = 0x7f; }
      }
    } else if (id == 3) {
      if (!wasmrun_leb_(m, &s, &m->nfuncs)) return 0;
      if (m->nfuncs > WASMRUN_MAX_FUNCS) return wasmrun_fail_(m, "too many funcs");
      for (uint32_t i = 0; i < m->nfuncs; i++) if (!wasmrun_leb_(m, &s, &m->funcs[i].type)) return 0;
    } else if (id == 7) {
      if (!wasmrun_leb_(m, &s, &m->nexports)) return 0;
      if (m->nexports > WASMRUN_MAX_EXPORTS) return wasmrun_fail_(m, "too many exports");
      for (uint32_t i = 0; i < m->nexports; i++) {
        uint32_t n; const uint8_t *p;
        if (!wasmrun_leb_(m, &s, &n) || !wasmrun_bytes_(m, &s, &p, n)) return 0;
        m->exports[i].name = malloc(n + 1); if (!m->exports[i].name) return wasmrun_fail_(m, "oom");
        memcpy(m->exports[i].name, p, n); m->exports[i].name[n] = 0;
        uint8_t kind; if (!wasmrun_u8_(m, &s, &kind)) return 0; if (kind != 0) return wasmrun_fail_(m, "only function exports");
        if (!wasmrun_leb_(m, &s, &m->exports[i].func)) return 0;
      }
    } else if (id == 10) {
      uint32_t nf; if (!wasmrun_leb_(m, &s, &nf)) return 0; if (nf != m->nfuncs) return wasmrun_fail_(m, "function/code count mismatch");
      for (uint32_t i = 0; i < nf; i++) {
        uint32_t n; if (!wasmrun_leb_(m, &s, &n)) return 0; WasmrunR b = { s.p, s.p + n }; if (b.end > s.end) return wasmrun_fail_(m, "bad body");
        uint32_t groups, nl = 0; if (!wasmrun_leb_(m, &b, &groups)) return 0;
        for (uint32_t g = 0; g < groups; g++) {
          uint32_t c; uint8_t vt; if (!wasmrun_leb_(m, &b, &c) || !wasmrun_u8_(m, &b, &vt)) return 0;
          if (vt != 0x7f) return wasmrun_fail_(m, "only i32 locals");
          if (nl + c > WASMRUN_MAX_LOCALS) return wasmrun_fail_(m, "too many locals");
          while (c--) m->funcs[i].locals[nl++] = vt;
        }
        m->funcs[i].nlocals = nl; m->funcs[i].code = b.p; m->funcs[i].end = b.end; s.p += n;
      }
    } else if (id == 2) {
      return wasmrun_fail_(m, "imports are not supported");
    }
    r.p += size;
  }
  return 1;
}

static inline int wasmrun_find_export(Wasmrun *m, const char *name, uint32_t *func) {
  for (uint32_t i = 0; i < m->nexports; i++) {
    if (!strcmp(m->exports[i].name, name)) { *func = m->exports[i].func; return 1; }
  }
  return wasmrun_fail_(m, "export not found");
}

static inline uint32_t wasmrun_param_count(Wasmrun *m, uint32_t func) {
  if (func >= m->nfuncs) return 0;
  return m->types[m->funcs[func].type].nparams;
}

static int wasmrun_call_func_(Wasmrun *m, uint32_t fi, const int32_t *args, int32_t *result, int *has_result) {
  if (fi >= m->nfuncs) return wasmrun_fail_(m, "bad function index");
  WasmrunFunc *f = &m->funcs[fi]; WasmrunSig *sig = &m->types[f->type];
  int32_t locals[512] = {0}, st[1024]; int sp = 0, cp = 0; WasmrunCtrl cs[128];
  for (uint32_t i = 0; i < sig->nparams; i++) locals[i] = args[i];
  WasmrunR pc = { f->code, f->end };
  for (;;) {
    uint8_t op; if (!wasmrun_u8_(m, &pc, &op)) return 0;
    switch (op) {
    case 0x0b:
      if (cp) { sp = cs[--cp].height; break; }
      *has_result = sig->result_type == 0x7f; if (*has_result) *result = st[--sp]; return 1;
    case 0x0f:
      *has_result = sig->result_type == 0x7f; if (*has_result) *result = st[--sp]; return 1;
    case 0x1a: sp--; break;
    case 0x1b: { int32_t c = st[--sp], b = st[--sp], a = st[--sp]; st[sp++] = c ? a : b; break; }
    case 0x02: case 0x03: { int32_t bt; const uint8_t *endp; if (!wasmrun_sleb_(m, &pc, &bt) || !wasmrun_find_end_(m, pc.p, pc.end, &endp)) return 0; cs[cp++] = (WasmrunCtrl){ op, pc.p, endp, sp }; break; }
    case 0x04: {
      int32_t bt, cond = st[--sp]; const uint8_t *elsep, *endp;
      if (!wasmrun_sleb_(m, &pc, &bt) || !wasmrun_find_else_end_(m, pc.p, pc.end, &elsep, &endp)) return 0;
      if (cond) cs[cp++] = (WasmrunCtrl){ op, pc.p, endp, sp };
      else if (elsep) { pc.p = elsep + 1; cs[cp++] = (WasmrunCtrl){ op, pc.p, endp, sp }; }
      else pc.p = endp + 1;
      break;
    }
    case 0x05: if (!cp || cs[cp - 1].op != 0x04) return wasmrun_fail_(m, "stray else"); sp = cs[--cp].height; pc.p = cs[cp].end + 1; break;
    case 0x0c: {
      uint32_t d; if (!wasmrun_leb_(m, &pc, &d)) return 0; if (d >= (uint32_t)cp) return wasmrun_fail_(m, "bad branch depth");
      WasmrunCtrl *c = &cs[cp - 1 - d]; sp = c->height; pc.p = c->op == 0x03 ? c->start : c->end + 1; cp -= d + (c->op == 0x03 ? 0 : 1); break;
    }
    case 0x0d: {
      uint32_t d; if (!wasmrun_leb_(m, &pc, &d)) return 0; if (st[--sp]) { if (d >= (uint32_t)cp) return wasmrun_fail_(m, "bad branch depth"); WasmrunCtrl *c = &cs[cp - 1 - d]; sp = c->height; pc.p = c->op == 0x03 ? c->start : c->end + 1; cp -= d + (c->op == 0x03 ? 0 : 1); } break;
    }
    case 0x10: {
      uint32_t callee; if (!wasmrun_leb_(m, &pc, &callee)) return 0; if (callee >= m->nfuncs) return wasmrun_fail_(m, "bad function index");
      WasmrunSig *t = &m->types[m->funcs[callee].type]; int32_t av[WASMRUN_MAX_PARAMS];
      for (int i = (int)t->nparams - 1; i >= 0; i--) av[i] = st[--sp];
      int hr = 0; int32_t rv = 0; if (!wasmrun_call_func_(m, callee, av, &rv, &hr)) return 0; if (hr) st[sp++] = rv; break;
    }
    case 0x20: { uint32_t i; if (!wasmrun_leb_(m, &pc, &i)) return 0; st[sp++] = locals[i]; break; }
    case 0x21: { uint32_t i; if (!wasmrun_leb_(m, &pc, &i)) return 0; locals[i] = st[--sp]; break; }
    case 0x22: { uint32_t i; if (!wasmrun_leb_(m, &pc, &i)) return 0; locals[i] = st[sp - 1]; break; }
    case 0x41: { int32_t v; if (!wasmrun_sleb_(m, &pc, &v)) return 0; st[sp++] = v; break; }
    case 0x45: st[sp - 1] = !st[sp - 1]; break;
    case 0x46: case 0x47: case 0x48: case 0x49: case 0x4a: case 0x4b:
    case 0x4c: case 0x4d: case 0x4e: case 0x4f: {
      int32_t b = st[--sp], a = st[--sp]; uint32_t ua = (uint32_t)a, ub = (uint32_t)b;
      st[sp++] = op == 0x46 ? a == b : op == 0x47 ? a != b : op == 0x48 ? a < b :
                 op == 0x49 ? ua < ub : op == 0x4a ? a > b : op == 0x4b ? ua > ub :
                 op == 0x4c ? a <= b : op == 0x4d ? ua <= ub : op == 0x4e ? a >= b : ua >= ub; break;
    }
    case 0x6a: st[sp - 2] += st[sp - 1]; sp--; break;
    case 0x6b: st[sp - 2] -= st[sp - 1]; sp--; break;
    case 0x6c: st[sp - 2] *= st[sp - 1]; sp--; break;
    case 0x6d: if (!st[sp - 1]) return wasmrun_fail_(m, "division by zero"); st[sp - 2] /= st[sp - 1]; sp--; break;
    case 0x6e: if (!st[sp - 1]) return wasmrun_fail_(m, "division by zero"); st[sp - 2] = (uint32_t)st[sp - 2] / (uint32_t)st[sp - 1]; sp--; break;
    case 0x6f: if (!st[sp - 1]) return wasmrun_fail_(m, "remainder by zero"); st[sp - 2] %= st[sp - 1]; sp--; break;
    case 0x70: if (!st[sp - 1]) return wasmrun_fail_(m, "remainder by zero"); st[sp - 2] = (uint32_t)st[sp - 2] % (uint32_t)st[sp - 1]; sp--; break;
    case 0x71: st[sp - 2] &= st[sp - 1]; sp--; break;
    case 0x72: st[sp - 2] |= st[sp - 1]; sp--; break;
    case 0x73: st[sp - 2] ^= st[sp - 1]; sp--; break;
    case 0x74: st[sp - 2] <<= (st[sp - 1] & 31); sp--; break;
    case 0x75: st[sp - 2] = (uint32_t)st[sp - 2] >> (st[sp - 1] & 31); sp--; break;
    case 0x76: st[sp - 2] >>= (st[sp - 1] & 31); sp--; break;
    default: {
      static char msg[64];
      snprintf(msg, sizeof(msg), "unsupported opcode 0x%02x", op);
      return wasmrun_fail_(m, msg);
    }
    }
  }
}

static inline int wasmrun_call(Wasmrun *m, uint32_t func, const int32_t *args, int32_t *result, int *has_result) {
  int hr = 0; int32_t rv = 0; m->error = NULL;
  if (!wasmrun_call_func_(m, func, args, &rv, &hr)) return 0;
  if (has_result) *has_result = hr;
  if (result) *result = rv;
  return 1;
}

static inline int wasmrun_call_export(Wasmrun *m, const char *name, const int32_t *args, int32_t *result, int *has_result) {
  uint32_t func;
  m->error = NULL;
  if (!wasmrun_find_export(m, name, &func)) return 0;
  return wasmrun_call(m, func, args, result, has_result);
}

static inline const char *wasmrun_error(Wasmrun *m) { return m->error ? m->error : ""; }

#endif
