import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const REPO_ROOT = path.resolve(__dirname, '..');
const TMP_DIR = path.join(REPO_ROOT, 'tmp');

function isPathWithin(candidate, directory) {
  const relative = path.relative(directory, candidate);
  return relative === '' || (relative && !relative.startsWith('..') && !path.isAbsolute(relative));
}

if (!fs.existsSync(TMP_DIR)) {
  console.log('No tmp directory present.');
  process.exit(0);
}

const tmpStat = fs.lstatSync(TMP_DIR);
if (tmpStat.isSymbolicLink()) {
  console.warn('Refusing to clean tmp because it is a symbolic link:', TMP_DIR);
  process.exit(1);
}

if (!tmpStat.isDirectory()) {
  console.warn('Refusing to clean tmp because it is not a directory:', TMP_DIR);
  process.exit(1);
}

const realRepoRoot = fs.realpathSync(REPO_ROOT);
const realTmpDir = fs.realpathSync(TMP_DIR);
if (!isPathWithin(realTmpDir, realRepoRoot)) {
  console.warn('Refusing to clean tmp outside the repository:', realTmpDir);
  process.exit(1);
}

for (const f of fs.readdirSync(TMP_DIR).sort()) {
  const p = path.join(TMP_DIR, f);
  try {
    fs.rmSync(p, { recursive: true, force: true });
    console.log('Removed', p);
  } catch (e) {
    console.warn('Failed to remove', p, e.message || e);
  }
}
console.log('tmp cleanup complete.');
