import { Wasmrun } from "./Wasmrun.js";

const usage =
  "usage: deno run --allow-read main.js file.wasm [export] [i32 args...]";

if (Deno.args.length < 1) {
  console.error(usage);
  Deno.exit(2);
}

const [path, exportName, ...rawArgs] = Deno.args;
let data;
try {
  data = await Deno.readFile(path);
} catch {
  console.error(`error: cannot read ${path}`);
  Deno.exit(1);
}

const m = new Wasmrun();
if (!m.load(data)) {
  console.error(`error: ${m.error()}`);
  Deno.exit(1);
}

const name = exportName ?? "main";
let func = m.findExport(name);
if (func < 0 && exportName == null) func = m.findExport("_start");
if (func < 0) {
  console.error(`error: ${m.error()}`);
  Deno.exit(1);
}

const nparams = m.paramCount(func);
if (rawArgs.length < nparams) {
  console.error("error: missing i32 args");
  Deno.exit(1);
}

const args = rawArgs.slice(0, nparams).map((v) => Number.parseInt(v, 0) | 0);
const result = m.call(func, args);
if (!result) {
  console.error(`error: ${m.error()}`);
  Deno.exit(1);
}
if (result.hasResult) console.log(result.result);
