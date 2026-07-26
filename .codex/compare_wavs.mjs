import fs from "node:fs";

function readWav(path) {
  const b = fs.readFileSync(path);
  let offset = 12;
  let fmt;
  let data;
  while (offset + 8 <= b.length) {
    const id = b.toString("ascii", offset, offset + 4);
    const size = b.readUInt32LE(offset + 4);
    if (id === "fmt ")
      fmt = { channels: b.readUInt16LE(offset + 10), sampleRate: b.readUInt32LE(offset + 12), bits: b.readUInt16LE(offset + 22) };
    if (id === "data")
      data = { offset: offset + 8, size };
    offset += 8 + size + (size & 1);
  }
  const count = Math.floor(data.size / 3);
  const samples = new Float64Array(count);
  for (let i = 0, p = data.offset; i < count; i++, p += 3) {
    let value = b[p] | (b[p + 1] << 8) | (b[p + 2] << 16);
    if (value & 0x800000)
      value -= 0x1000000;
    samples[i] = value / 8388608;
  }
  return { fmt, samples };
}

const a = readWav(process.argv[2]);
const b = readWav(process.argv[3]);
const maxLag = 2048;
const start = 3 * a.fmt.sampleRate;
const length = 20 * a.fmt.sampleRate;
let best = { lag: 0, score: -Infinity, gain: 1 };
for (let lag = -maxLag; lag <= maxLag; lag++) {
  let xy = 0;
  let xx = 0;
  let yy = 0;
  for (let i = 0; i < length; i += 8) {
    const ai = start + i;
    const bi = ai + lag;
    if (bi < 0 || bi >= b.samples.length)
      continue;
    const x = a.samples[ai];
    const y = b.samples[bi];
    xy += x * y;
    xx += x * x;
    yy += y * y;
  }
  const score = xy / Math.sqrt(xx * yy);
  if (score > best.score)
    best = { lag, score, gain: xy / xx };
}

let residualEnergy = 0;
let referenceEnergy = 0;
const jumpsByModulo = new Map();
for (let i = 0; i < length; i++) {
  const ai = start + i;
  const bi = ai + best.lag;
  if (bi < 0 || bi >= b.samples.length)
    continue;
  const residual = b.samples[bi] - best.gain * a.samples[ai];
  residualEnergy += residual * residual;
  referenceEnergy += b.samples[bi] * b.samples[bi];
  if (i > 0) {
    const previous = b.samples[bi - 1] - best.gain * a.samples[ai - 1];
    const jump = Math.abs(residual - previous);
    for (const period of [32, 64, 128, 256, 512, 1024]) {
      const key = `${period}:${i % period}`;
      const entry = jumpsByModulo.get(key) ?? { sum: 0, count: 0 };
      entry.sum += jump;
      entry.count++;
      jumpsByModulo.set(key, entry);
    }
  }
}

const periodic = [];
for (const period of [32, 64, 128, 256, 512, 1024]) {
  const values = [];
  for (let phase = 0; phase < period; phase++) {
    const entry = jumpsByModulo.get(`${period}:${phase}`);
    values.push({ phase, meanJump: entry.sum / entry.count });
  }
  values.sort((x, y) => y.meanJump - x.meanJump);
  const mean = values.reduce((sum, value) => sum + value.meanJump, 0) / values.length;
  periodic.push({
    period,
    strongestPhase: values[0].phase,
    strongestToMean: values[0].meanJump / mean,
  });
}

console.log(JSON.stringify({
  alignment: best,
  residualDbRelative: 10 * Math.log10(residualEnergy / referenceEnergy),
  periodic,
}, null, 2));
