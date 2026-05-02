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

const putchar = (ch) => {
  Deno.stdout.writeSync(new Uint8Array([ch & 255]));
};

let instance;
try {
  const wasm = await WebAssembly.instantiate(data, { env: { putchar } });
  instance = wasm.instance;
} catch (e) {
  console.error(`load error: ${e.message}`);
  Deno.exit(1);
}

const entry = instance.exports.main ?? instance.exports._start;
if (typeof entry !== "function") {
  console.error("run error: missing main or _start export");
  Deno.exit(1);
}

try {
  entry();
} catch (e) {
  console.error(`run error: ${e.message}`);
  Deno.exit(1);
}
