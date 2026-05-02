import { Wasmrun } from "../Wasmrun.js";

if (Deno.args.length !== 1) {
  console.error(`usage: ${Deno.mainModule} file.wasm`);
  Deno.exit(2);
}

const path = Deno.args[0];
let data;
try {
  data = await Deno.readFile(path);
} catch {
  console.error(`error: cannot read ${path}`);
  Deno.exit(1);
}

const m = new Wasmrun();
if (!m.load(data)) {
  console.error(`load error: ${m.error()}`);
  Deno.exit(1);
}

const hostPutchar = (_m, _user, args) => {
  Deno.stdout.writeSync(new Uint8Array([args[0] & 255]));
  return 1;
};

if (!m.setImportFunc("env", "putchar", hostPutchar, null)) {
  console.error(`import error: ${m.error()}`);
  Deno.exit(1);
}

const result = m.callExport("main");
if (!result) {
  console.error(`run error: ${m.error()}`);
  Deno.exit(1);
}
