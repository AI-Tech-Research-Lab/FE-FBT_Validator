// Compares the page's level model (levelRows in index.html) with a levelprobe grid.
// usage: node tools/levelprobe/check.js [grid.json]   (default: results/grid-main.json, the committed run)
const fs = require('fs');
const path = require('path');
const root = path.join(__dirname, '..', '..');
const html = fs.readFileSync(path.join(root, 'index.html'), 'utf8');
const start = html.indexOf('// GetDepthByDegree (ckksrns-utils.cpp)');
const end = html.indexOf('</script>', start);
const { psDepth, levelRows } = new Function(html.slice(start, end) + '\nreturn { psDepth, levelRows };')();

const file = process.argv[2] || path.join(__dirname, 'results', 'grid-main.json');
const grid = JSON.parse(fs.readFileSync(file, 'utf8'));
let bad = 0, n = 0, skipped = 0;
for (const r of grid) {
  if (r.info.startsWith('ERROR')) { // OpenFHE refused the parameters (e.g. q0 > 60 bits on the 64-bit build)
    skipped++;
    if (process.env.VERBOSE) console.log('skip ' + r.cfg.join(' ') + ' | ' + r.info);
    continue;
  }
  const [mode, st, skd, D, , logS, cts, stc, fdeg, , fms] = r.cfg;
  const b = [cts, stc];
  const res = levelRows({ D, logSlots: logS, st, skd, q0big: fms > 60, fe: b, bs: b, cur: null }, fdeg || 27);
  const row = res.rows.find(x => x.op === mode);
  const got = [row.minIn, row.last1, row.last2, row.out, row.out];
  const s1 = r.s1 || {}, s2 = r.s2 || {};
  const want = [s1.minEff, s1.last, s2.last, s1.effOut, s2.effOut];
  const ok = got.every((v, i) => v === want[i]);
  n++;
  if (!ok) bad++;
  if (!ok || process.env.VERBOSE)
    console.log((ok ? 'ok  ' : 'DIFF') + ' ' + r.cfg.join(' ') + ' | model ' + got.join(' ') + ' | probe ' + want.join(' ') +
      (r.wrong ? ' | wrong ' + JSON.stringify(r.wrong) : '') + ' | ' + r.info);
}
console.log(n - bad + '/' + n + ' configurations match' + (skipped ? ', ' + skipped + ' skipped (refused by OpenFHE)' : ''));
process.exit(bad ? 1 : 0);
