import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const TMP_DIR = path.resolve(__dirname, '..', 'tmp');
if (!fs.existsSync(TMP_DIR)) {
  console.log('No tmp directory present.');
  process.exit(0);
}

for (const f of fs.readdirSync(TMP_DIR)) {
  const p = path.join(TMP_DIR, f);
  try {
    fs.unlinkSync(p);
    console.log('Removed', p);
  } catch (e) {
    console.warn('Failed to remove', p, e.message || e);
  }
}
console.log('tmp cleanup complete.');
