import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const wasmPkgPath = path.resolve(__dirname, '../src/wasm/pkg/unreal_mcp_wasm.js');

try {
    // Use a file descriptor to atomically read-check-write (avoids TOCTOU race condition)
    let fd;
    try {
        fd = fs.openSync(wasmPkgPath, 'r+');
    } catch (openErr) {
        if (openErr.code === 'ENOENT') {
            console.warn('WASM binding file not found at:', wasmPkgPath);
            process.exit(0);
        }
        throw openErr;
    }

    try {
        const content = fs.readFileSync(fd, 'utf8');
        if (content.includes('console.log(getObject(arg0));')) {
            const patched = content.replace('console.log(getObject(arg0));', 'console.error(getObject(arg0));');
            fs.ftruncateSync(fd, 0);
            fs.writeSync(fd, patched, 0, 'utf8');
            console.log('Successfully patched console.log to console.error in WASM bindings.');
        } else {
            console.log('WASM bindings already patched or console.log not found.');
        }
    } finally {
        fs.closeSync(fd);
    }
} catch (error) {
    console.error('Error patching WASM bindings:', error);
    process.exit(1);
}
