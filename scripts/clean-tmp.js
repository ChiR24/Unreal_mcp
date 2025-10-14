const fs = require('fs');
const path = require('path');

const TMP_DIR = path.resolve(__dirname, '..', 'tmp');
if (!fs.existsSync(TMP_DIR)) {
  console.log('No tmp directory present.');
  process.exit(0);
}

fs.readdirSync(TMP_DIR).forEach((f) => {
  const p = path.join(TMP_DIR, f);
  try {
    fs.unlinkSync(p);
    console.log('Removed', p);
  } catch (e) {
    console.warn('Failed to remove', p, e.message || e);
  }
});
console.log('tmp cleanup complete.');
