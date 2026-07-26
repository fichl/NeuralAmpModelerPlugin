import fs from "node:fs";

const filePath = process.argv[2] ?? "D:/Documenti/REAPER Media/untitled.wav";
const b = fs.readFileSync(filePath);
const u32 = (offset) => b.readUInt32LE(offset);
let offset = 12;
let fmt;
let data;
while (offset + 8 <= b.length) {
  const id = b.toString("ascii", offset, offset + 4);
  const size = u32(offset + 4);
  if (id === "fmt ") {
    fmt = {
      format: b.readUInt16LE(offset + 8),
      channels: b.readUInt16LE(offset + 10),
      sampleRate: u32(offset + 12),
      bits: b.readUInt16LE(offset + 22),
    };
  }
  if (id === "data")
    data = { offset: offset + 8, size };
  offset += 8 + size + (size & 1);
}

const sampleCount = Math.floor(data.size / 3);
const samples = new Float64Array(sampleCount);
for (let i = 0, p = data.offset; i < sampleCount; i++, p += 3) {
  let value = b[p] | (b[p + 1] << 8) | (b[p + 2] << 16);
  if (value & 0x800000)
    value -= 0x1000000;
  samples[i] = value / 8388608;
}

function fft(input) {
  const n = input.length;
  const re = new Float64Array(n);
  const im = new Float64Array(n);
  for (let i = 0; i < n; i++)
    re[i] = input[i] * (0.5 - 0.5 * Math.cos(2 * Math.PI * i / (n - 1)));
  for (let i = 1, j = 0; i < n; i++) {
    let bit = n >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      [re[i], re[j]] = [re[j], re[i]];
  }
  for (let length = 2; length <= n; length <<= 1) {
    const angle = -2 * Math.PI / length;
    for (let start = 0; start < n; start += length) {
      for (let k = 0; k < length / 2; k++) {
        const c = Math.cos(angle * k);
        const s = Math.sin(angle * k);
        const ur = re[start + k];
        const ui = im[start + k];
        const vr = re[start + k + length / 2] * c - im[start + k + length / 2] * s;
        const vi = re[start + k + length / 2] * s + im[start + k + length / 2] * c;
        re[start + k] = ur + vr;
        im[start + k] = ui + vi;
        re[start + k + length / 2] = ur - vr;
        im[start + k + length / 2] = ui - vi;
      }
    }
  }
  return { re, im };
}

const fftSize = 65536;
const segment = samples.slice(8 * fmt.sampleRate, 8 * fmt.sampleRate + fftSize);
const spectrum = fft(segment);
const peaks = [];
for (let k = 2; k < fftSize / 2 - 1; k++) {
  const magnitude = Math.hypot(spectrum.re[k], spectrum.im[k]);
  if (magnitude > Math.hypot(spectrum.re[k - 1], spectrum.im[k - 1])
      && magnitude > Math.hypot(spectrum.re[k + 1], spectrum.im[k + 1]))
    peaks.push([magnitude, k * fmt.sampleRate / fftSize]);
}
peaks.sort((a, b) => b[0] - a[0]);

const windowStats = [];
for (let second = 0; second < sampleCount / fmt.sampleRate; second++) {
  let sumSquares = 0;
  let peak = 0;
  for (let i = second * fmt.sampleRate; i < (second + 1) * fmt.sampleRate; i++) {
    sumSquares += samples[i] * samples[i];
    peak = Math.max(peak, Math.abs(samples[i]));
  }
  windowStats.push({
    second,
    rmsDb: 20 * Math.log10(Math.sqrt(sumSquares / fmt.sampleRate) + 1e-15),
    peakDb: 20 * Math.log10(peak + 1e-15),
  });
}

console.log(JSON.stringify({
  filePath,
  fmt,
  sampleCount,
  duration: sampleCount / fmt.sampleRate,
  topPeaks: peaks.slice(0, 30).map(([magnitude, frequency]) => ({
    frequency: Number(frequency.toFixed(2)),
    db: Number((20 * Math.log10(magnitude / (fftSize / 2))).toFixed(1)),
  })),
  windowStats: windowStats.map(({ second, rmsDb, peakDb }) => ({
    second,
    rmsDb: Number(rmsDb.toFixed(1)),
    peakDb: Number(peakDb.toFixed(1)),
  })),
}, null, 2));
