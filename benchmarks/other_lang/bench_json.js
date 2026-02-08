/* eslint-disable no-console */
const { performance } = require("perf_hooks");

let SnowflakeUtil = null;
try {
  ({ SnowflakeUtil } = require("discord.js"));
} catch (err) {
  console.error("discord.js not available. Install dependencies first:");
  console.error("  npm install");
  console.error("  # or");
  console.error("  bun install");
  process.exit(1);
}

const SAMPLE_JSON = '{"id":"123456789012345678","name":"test","value":42}';
const SAMPLE_BUF = Buffer.from(SAMPLE_JSON, "utf8");
const MIN_WALL_MS = 500;

function cpuUsageMs(start) {
  const usage = process.cpuUsage(start);
  return (usage.user + usage.system) / 1000;
}

function bench(name, fn) {
  let iters = 1;
  // warmup
  for (let i = 0; i < 1000; i += 1) fn();
  // calibrate
  while (true) {
    const start = performance.now();
    for (let i = 0; i < iters; i += 1) fn();
    const wall = performance.now() - start;
    if (wall >= MIN_WALL_MS) break;
    iters *= 2;
  }

  const wallStart = performance.now();
  const cpuStart = process.cpuUsage();
  for (let i = 0; i < iters; i += 1) fn();
  const wall = performance.now() - wallStart;
  const cpu = cpuUsageMs(cpuStart);
  const nsWall = (wall * 1e6) / iters;
  const nsCpu = (cpu * 1e6) / iters;
  console.log(
    `${name.padEnd(30)} ${nsWall.toFixed(1).padStart(10)} ns ` +
      `${nsCpu.toFixed(1).padStart(10)} ns ${String(iters).padStart(11)}`
  );
}

console.log("----------------------------------------------------------------");
console.log("Benchmark                      Time             CPU   Iterations");
console.log("----------------------------------------------------------------");
bench("BM_JSON_Parse", () => JSON.parse(SAMPLE_JSON));
bench("BM_JSON_Parse_Buffer", () => JSON.parse(SAMPLE_BUF.toString("utf8")));

const parsed = JSON.parse(SAMPLE_JSON);
const snowflake = parsed.id;

bench("BM_JSON_Get_Snowflake", () => {
  BigInt(snowflake);
});

bench("BM_JSON_Mut_Serialize", () => {
  const payload = { id: snowflake, name: "test", value: 42 };
  JSON.stringify(payload);
});
