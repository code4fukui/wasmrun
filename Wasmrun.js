const WASMRUN_MAX_TYPES = 128;
const WASMRUN_MAX_FUNCS = 128;
const WASMRUN_MAX_EXPORTS = 128;
const WASMRUN_MAX_PARAMS = 16;
const WASMRUN_MAX_LOCALS = 256;
const WASMRUN_MAX_GLOBALS = 64;

class Reader {
  constructor(data, p = 0, end = data.length) {
    this.data = data;
    this.p = p;
    this.end = end;
  }
}

export class Wasmrun {
  constructor() {
    this.init();
  }

  init() {
    this.types = [];
    this.funcs = [];
    this.nimports = 0;
    this.globals = [];
    this.exports = [];
    this._error = "";
  }

  free() {
    this.init();
  }

  error() {
    return this._error || "";
  }

  load(data) {
    this.free();
    const bytes = data instanceof Uint8Array ? data : new Uint8Array(data);
    this._bytes = bytes;
    const r = new Reader(bytes);
    const magic = this.#read32(r);
    const version = this.#read32(r);
    if (magic == null || version == null) return false;
    if (magic !== 0x6d736100 || version !== 1) {
      return this.#fail("not wasm32 v1");
    }

    while (r.p < r.end) {
      const id = this.#u8(r);
      const size = this.#leb(r);
      if (id == null || size == null) return false;
      const s = new Reader(bytes, r.p, r.p + size);
      if (s.end > r.end) return this.#fail("bad section size");

      if (id === 1) {
        const ntypes = this.#leb(s);
        if (ntypes == null) return false;
        if (ntypes > WASMRUN_MAX_TYPES) return this.#fail("too many types");
        this.types = [];
        for (let i = 0; i < ntypes; i++) {
          const form = this.#u8(s);
          if (form == null) return false;
          if (form !== 0x60) return this.#fail("bad type");
          const nparams = this.#leb(s);
          if (nparams == null) return false;
          if (nparams > WASMRUN_MAX_PARAMS) {
            return this.#fail("too many params");
          }
          for (let j = 0; j < nparams; j++) {
            const vt = this.#u8(s);
            if (vt == null) return false;
            if (vt !== 0x7f) return this.#fail("only i32 params");
          }
          const nr = this.#leb(s);
          if (nr == null) return false;
          if (nr > 1) return this.#fail("too many results");
          let resultType = 0;
          if (nr) {
            const vt = this.#u8(s);
            if (vt == null) return false;
            if (vt !== 0x7f) return this.#fail("only i32 result");
            resultType = 0x7f;
          }
          this.types.push({ nparams, resultType });
        }
      } else if (id === 2) {
        const ni = this.#leb(s);
        if (ni == null) return false;
        for (let i = 0; i < ni; i++) {
          const module = this.#name(s);
          const name = this.#name(s);
          const kind = this.#u8(s);
          if (module == null || name == null || kind == null) return false;
          if (kind !== 0) return this.#fail("only function imports");
          if (this.funcs.length >= WASMRUN_MAX_FUNCS) {
            return this.#fail("too many funcs");
          }
          const type = this.#leb(s);
          if (type == null) return false;
          if (type >= this.types.length) return this.#fail("bad import type");
          this.funcs.push({
            type,
            codeStart: 0,
            codeEnd: 0,
            locals: [],
            imported: true,
            importModule: module,
            importName: name,
            host: null,
            hostUser: undefined,
          });
          this.nimports++;
        }
      } else if (id === 3) {
        const nf = this.#leb(s);
        if (nf == null) return false;
        if (this.funcs.length + nf > WASMRUN_MAX_FUNCS) {
          return this.#fail("too many funcs");
        }
        for (let i = 0; i < nf; i++) {
          const type = this.#leb(s);
          if (type == null) return false;
          this.funcs[this.nimports + i] = {
            type,
            codeStart: 0,
            codeEnd: 0,
            locals: [],
            imported: false,
            importModule: "",
            importName: "",
            host: null,
            hostUser: undefined,
          };
        }
      } else if (id === 7) {
        const nexports = this.#leb(s);
        if (nexports == null) return false;
        if (nexports > WASMRUN_MAX_EXPORTS) {
          return this.#fail("too many exports");
        }
        this.exports = [];
        for (let i = 0; i < nexports; i++) {
          const name = this.#name(s);
          if (name == null) return false;
          const kind = this.#u8(s);
          if (kind == null) return false;
          if (kind !== 0) return this.#fail("only function exports");
          const func = this.#leb(s);
          if (func == null) return false;
          this.exports.push({ name, func });
        }
      } else if (id === 6) {
        const nglobals = this.#leb(s);
        if (nglobals == null) return false;
        if (nglobals > WASMRUN_MAX_GLOBALS) {
          return this.#fail("too many globals");
        }
        this.globals = [];
        for (let i = 0; i < nglobals; i++) {
          const vt = this.#u8(s);
          const mut = this.#u8(s);
          if (vt == null || mut == null) return false;
          if (vt !== 0x7f) return this.#fail("only i32 globals");
          if (mut > 1) return this.#fail("bad global mutability");
          const op = this.#u8(s);
          if (op == null) return false;
          if (op !== 0x41) return this.#fail("only i32.const global init");
          const value = this.#sleb(s);
          const endop = this.#u8(s);
          if (value == null || endop == null) return false;
          if (endop !== 0x0b) return this.#fail("bad global init");
          this.globals.push({ mut, value: value | 0 });
        }
      } else if (id === 10) {
        const nf = this.#leb(s);
        const first = this.nimports;
        if (nf == null) return false;
        if (nf !== this.funcs.length - this.nimports) {
          return this.#fail("function/code count mismatch");
        }
        for (let i = 0; i < nf; i++) {
          const n = this.#leb(s);
          if (n == null) return false;
          const b = new Reader(bytes, s.p, s.p + n);
          if (b.end > s.end) return this.#fail("bad body");
          const groups = this.#leb(b);
          if (groups == null) return false;
          const locals = [];
          for (let g = 0; g < groups; g++) {
            let c = this.#leb(b);
            const vt = this.#u8(b);
            if (c == null || vt == null) return false;
            if (vt !== 0x7f) return this.#fail("only i32 locals");
            if (locals.length + c > WASMRUN_MAX_LOCALS) {
              return this.#fail("too many locals");
            }
            while (c--) locals.push(vt);
          }
          const f = this.funcs[first + i];
          f.locals = locals;
          f.codeStart = b.p;
          f.codeEnd = b.end;
          s.p += n;
        }
      }
      r.p += size;
    }
    return true;
  }

  setImportFunc(module, name, host, user) {
    this._error = "";
    for (let i = 0; i < this.nimports; i++) {
      const f = this.funcs[i];
      if (f.importModule === module && f.importName === name) {
        f.host = host;
        f.hostUser = user;
        return true;
      }
    }
    return this.#fail("import not found");
  }

  findExport(name) {
    for (const ex of this.exports) {
      if (ex.name === name) return ex.func;
    }
    this.#fail("export not found");
    return -1;
  }

  paramCount(func) {
    if (func < 0 || func >= this.funcs.length) return 0;
    return this.types[this.funcs[func].type].nparams;
  }

  call(func, args = []) {
    this._error = "";
    const r = this.#callFunc(func, args);
    if (!r.ok) return null;
    return r;
  }

  callExport(name, args = []) {
    this._error = "";
    const func = this.findExport(name);
    if (func < 0) return null;
    return this.call(func, args);
  }

  #callFunc(fi, args) {
    if (fi < 0 || fi >= this.funcs.length) {
      return { ok: this.#fail("bad function index") };
    }
    const f = this.funcs[fi];
    const sig = this.types[f.type];
    if (f.imported) {
      if (!f.host) return { ok: this.#fail("unbound import") };
      const rv = f.host(this, f.hostUser, args);
      if (rv === false) return { ok: false };
      const hasResult = sig.resultType === 0x7f;
      return { ok: true, hasResult, result: hasResult ? (rv || 0) | 0 : 0 };
    }

    const locals = new Int32Array(512);
    const st = new Int32Array(1024);
    const cs = [];
    let sp = 0;
    for (let i = 0; i < sig.nparams; i++) locals[i] = (args[i] || 0) | 0;
    const pc = new Reader(this._bytes, f.codeStart, f.codeEnd);

    for (;;) {
      const op = this.#u8(pc);
      if (op == null) return { ok: false };
      switch (op) {
        case 0x0b:
          if (cs.length) {
            sp = cs.pop().height;
            break;
          }
          return this.#returnResult(sig, st, sp);
        case 0x0f:
          return this.#returnResult(sig, st, sp);
        case 0x1a:
          sp--;
          break;
        case 0x1b: {
          const c = st[--sp];
          const b = st[--sp];
          const a = st[--sp];
          st[sp++] = c ? a : b;
          break;
        }
        case 0x02:
        case 0x03: {
          const bt = this.#sleb(pc);
          const endp = this.#findEnd(pc.p, pc.end);
          if (bt == null || endp == null) return { ok: false };
          cs.push({ op, start: pc.p, end: endp, height: sp });
          break;
        }
        case 0x04: {
          const bt = this.#sleb(pc);
          const cond = st[--sp];
          const found = this.#findElseEnd(pc.p, pc.end);
          if (bt == null || found == null) return { ok: false };
          if (cond) {
            cs.push({ op, start: pc.p, end: found.endp, height: sp });
          } else if (found.elsep != null) {
            pc.p = found.elsep + 1;
            cs.push({ op, start: pc.p, end: found.endp, height: sp });
          } else {
            pc.p = found.endp + 1;
          }
          break;
        }
        case 0x05: {
          if (!cs.length || cs[cs.length - 1].op !== 0x04) {
            return { ok: this.#fail("stray else") };
          }
          const c = cs.pop();
          sp = c.height;
          pc.p = c.end + 1;
          break;
        }
        case 0x0c: {
          const d = this.#leb(pc);
          if (d == null) return { ok: false };
          if (d >= cs.length) return { ok: this.#fail("bad branch depth") };
          const c = cs[cs.length - 1 - d];
          sp = c.height;
          pc.p = c.op === 0x03 ? c.start : c.end + 1;
          cs.length -= d + (c.op === 0x03 ? 0 : 1);
          break;
        }
        case 0x0d: {
          const d = this.#leb(pc);
          if (d == null) return { ok: false };
          if (st[--sp]) {
            if (d >= cs.length) return { ok: this.#fail("bad branch depth") };
            const c = cs[cs.length - 1 - d];
            sp = c.height;
            pc.p = c.op === 0x03 ? c.start : c.end + 1;
            cs.length -= d + (c.op === 0x03 ? 0 : 1);
          }
          break;
        }
        case 0x10: {
          const callee = this.#leb(pc);
          if (callee == null) return { ok: false };
          if (callee >= this.funcs.length) {
            return { ok: this.#fail("bad function index") };
          }
          const t = this.types[this.funcs[callee].type];
          const av = new Int32Array(t.nparams);
          for (let i = t.nparams - 1; i >= 0; i--) av[i] = st[--sp];
          const rv = this.#callFunc(callee, av);
          if (!rv.ok) return { ok: false };
          if (rv.hasResult) st[sp++] = rv.result;
          break;
        }
        case 0x20: {
          const i = this.#leb(pc);
          if (i == null) return { ok: false };
          st[sp++] = locals[i];
          break;
        }
        case 0x21: {
          const i = this.#leb(pc);
          if (i == null) return { ok: false };
          locals[i] = st[--sp];
          break;
        }
        case 0x22: {
          const i = this.#leb(pc);
          if (i == null) return { ok: false };
          locals[i] = st[sp - 1];
          break;
        }
        case 0x23: {
          const i = this.#leb(pc);
          if (i == null) return { ok: false };
          if (i >= this.globals.length) {
            return { ok: this.#fail("bad global index") };
          }
          st[sp++] = this.globals[i].value;
          break;
        }
        case 0x24: {
          const i = this.#leb(pc);
          if (i == null) return { ok: false };
          if (i >= this.globals.length) {
            return { ok: this.#fail("bad global index") };
          }
          if (!this.globals[i].mut) {
            return { ok: this.#fail("immutable global set") };
          }
          this.globals[i].value = st[--sp];
          break;
        }
        case 0x41: {
          const v = this.#sleb(pc);
          if (v == null) return { ok: false };
          st[sp++] = v;
          break;
        }
        case 0x45:
          st[sp - 1] = !st[sp - 1];
          break;
        case 0x46:
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4a:
        case 0x4b:
        case 0x4c:
        case 0x4d:
        case 0x4e:
        case 0x4f: {
          const b = st[--sp];
          const a = st[--sp];
          const ua = a >>> 0;
          const ub = b >>> 0;
          st[sp++] = op === 0x46
            ? a === b
            : op === 0x47
            ? a !== b
            : op === 0x48
            ? a < b
            : op === 0x49
            ? ua < ub
            : op === 0x4a
            ? a > b
            : op === 0x4b
            ? ua > ub
            : op === 0x4c
            ? a <= b
            : op === 0x4d
            ? ua <= ub
            : op === 0x4e
            ? a >= b
            : ua >= ub;
          break;
        }
        case 0x6a:
          st[sp - 2] = (st[sp - 2] + st[sp - 1]) | 0;
          sp--;
          break;
        case 0x6b:
          st[sp - 2] = (st[sp - 2] - st[sp - 1]) | 0;
          sp--;
          break;
        case 0x6c:
          st[sp - 2] = Math.imul(st[sp - 2], st[sp - 1]);
          sp--;
          break;
        case 0x6d:
          if (!st[sp - 1]) return { ok: this.#fail("division by zero") };
          st[sp - 2] = Math.trunc(st[sp - 2] / st[sp - 1]) | 0;
          sp--;
          break;
        case 0x6e:
          if (!st[sp - 1]) return { ok: this.#fail("division by zero") };
          st[sp - 2] = Math.trunc((st[sp - 2] >>> 0) / (st[sp - 1] >>> 0)) | 0;
          sp--;
          break;
        case 0x6f:
          if (!st[sp - 1]) return { ok: this.#fail("remainder by zero") };
          st[sp - 2] = (st[sp - 2] % st[sp - 1]) | 0;
          sp--;
          break;
        case 0x70:
          if (!st[sp - 1]) return { ok: this.#fail("remainder by zero") };
          st[sp - 2] = ((st[sp - 2] >>> 0) % (st[sp - 1] >>> 0)) | 0;
          sp--;
          break;
        case 0x71:
          st[sp - 2] &= st[sp - 1];
          sp--;
          break;
        case 0x72:
          st[sp - 2] |= st[sp - 1];
          sp--;
          break;
        case 0x73:
          st[sp - 2] ^= st[sp - 1];
          sp--;
          break;
        case 0x74:
          st[sp - 2] <<= st[sp - 1] & 31;
          sp--;
          break;
        case 0x75:
          st[sp - 2] = st[sp - 2] >>> (st[sp - 1] & 31);
          sp--;
          break;
        case 0x76:
          st[sp - 2] >>= st[sp - 1] & 31;
          sp--;
          break;
        default:
          return {
            ok: this.#fail(
              `unsupported opcode 0x${op.toString(16).padStart(2, "0")}`,
            ),
          };
      }
    }
  }

  #returnResult(sig, st, sp) {
    const hasResult = sig.resultType === 0x7f;
    return { ok: true, hasResult, result: hasResult ? st[sp - 1] : 0 };
  }

  #seterr(s) {
    if (!this._error) this._error = s;
  }

  #fail(s) {
    this.#seterr(s);
    return false;
  }

  #u8(r) {
    if (r.p >= r.end) {
      this.#fail("unexpected eof");
      return null;
    }
    return r.data[r.p++];
  }

  #leb(r) {
    let x = 0;
    let s = 0;
    for (;;) {
      const b = this.#u8(r);
      if (b == null) return null;
      x = (x | ((b & 127) << s)) >>> 0;
      if (!(b & 128)) return x;
      s += 7;
      if (s >= 35) {
        this.#fail("bad leb128");
        return null;
      }
    }
  }

  #sleb(r) {
    let x = 0;
    let s = 0;
    let b;
    do {
      b = this.#u8(r);
      if (b == null) return null;
      x |= (b & 127) << s;
      s += 7;
    } while (b & 128);
    if (s < 32 && (b & 64)) x |= -(1 << s);
    return x | 0;
  }

  #bytes(r, n) {
    if (r.end - r.p < n) {
      this.#fail("short section");
      return null;
    }
    const p = r.p;
    r.p += n;
    return p;
  }

  #name(r) {
    const n = this.#leb(r);
    if (n == null) return null;
    const p = this.#bytes(r, n);
    if (p == null) return null;
    return new TextDecoder().decode(r.data.subarray(p, p + n));
  }

  #read32(r) {
    const a = this.#u8(r);
    const b = this.#u8(r);
    const c = this.#u8(r);
    const d = this.#u8(r);
    if (a == null || b == null || c == null || d == null) return null;
    return (a | (b << 8) | (c << 16) | (d << 24)) >>> 0;
  }

  #skipImmediate(r, op) {
    if (
      op === 0x0c || op === 0x0d || op === 0x10 || op === 0x20 || op === 0x21 ||
      op === 0x22 || op === 0x23 || op === 0x24
    ) {
      return this.#leb(r) != null;
    }
    if (op === 0x41) return this.#sleb(r) != null;
    return true;
  }

  #findEnd(p, end) {
    let depth = 1;
    const r = new Reader(this._bytes, p, end);
    while (r.p < r.end) {
      const op = this.#u8(r);
      if (op == null) return null;
      if (op === 0x02 || op === 0x03 || op === 0x04) {
        if (this.#sleb(r) == null) return null;
        depth++;
      } else if (op === 0x0b && --depth === 0) {
        return r.p - 1;
      } else if (!this.#skipImmediate(r, op)) {
        return null;
      }
    }
    this.#fail("missing end");
    return null;
  }

  #findElseEnd(p, end) {
    let depth = 1;
    let elsep = null;
    const r = new Reader(this._bytes, p, end);
    while (r.p < r.end) {
      const op = this.#u8(r);
      if (op == null) return null;
      if (op === 0x02 || op === 0x03 || op === 0x04) {
        if (this.#sleb(r) == null) return null;
        depth++;
      } else if (op === 0x05 && depth === 1) {
        elsep = r.p - 1;
      } else if (op === 0x0b && --depth === 0) {
        return { elsep, endp: r.p - 1 };
      } else if (!this.#skipImmediate(r, op)) {
        return null;
      }
    }
    this.#fail("missing if end");
    return null;
  }
}

export default Wasmrun;
