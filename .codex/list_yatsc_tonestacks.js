const fs = require('fs');
const path = require('path');

const root = path.join(process.cwd(), '.codex/yatc/src/models/tonestacks');

function walk(dir) {
  let out = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out = out.concat(walk(full));
    else if (entry.name.endsWith('.js') && !entry.name.includes('.test') && !entry.name.startsWith('_') && entry.name !== 'BaseTonestack.js') {
      out.push(full);
    }
  }
  return out;
}

const roughMap = {
  'BigMuff/BigMuff': 'Big Muff',
  'Dumble/DumbleJazz': 'Dmbl Jazz',
  'Dumble/DumbleRock': 'Dmbl Rock',
  'Fender/Bassman5F6A': 'Fndr Bassman 5F6-A',
  'Fender/Brownface': 'Fndr Brownface',
  'Fender/Deluxe5E3Normal': 'Fndr Deluxe 5E3',
  'Fender/ESeries': 'Fndr E-series',
  'Fender/FenderTMB': 'Fndr TMB',
  'Fender/Princeton5E2': 'Fndr Princeton 5E2',
  'Fender/Princeton5F2A': 'Fndr Princeton 5F2A',
  'Fender/ProJunior': 'Fndr Pro Jr',
  'misc/Bench': 'Bench',
  'misc/Crate': 'Crate',
  'misc/HiwattDR': 'Hiwatt',
  'misc/Marshall': 'Marshall',
  'misc/NeveShelvingHiLo': 'Neve',
  'misc/Vox': 'Vox',
};

const ignoredPrefixes = ['Basic/', 'SWTC/'];
const ignoredModels = new Set(['misc/FramusMidControl', 'misc/Tilt', 'misc/Wah']);

const models = walk(root)
  .map((file) => path.relative(root, file).replaceAll(path.sep, '/').replace(/\.js$/, ''))
  .filter((model) => !ignoredPrefixes.some((prefix) => model.startsWith(prefix)))
  .filter((model) => !ignoredModels.has(model))
  .sort();

console.log('Present-ish:');
for (const [yatc, nam] of Object.entries(roughMap).sort()) console.log(`${yatc} -> ${nam}`);
console.log('\nMissing candidates:');
for (const model of models.filter((model) => !roughMap[model])) console.log(model);
