import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const wasmPkgPath = path.resolve(__dirname, '../src/wasm/pkg/unreal_mcp_wasm.js');

try {
    if (fs.existsSync(wasmPkgPath)) {
        let content = fs.readFileSync(wasmPkgPath, 'utf8');
        if (content.includes('console.log(getObject(arg0));')) {
            content = content.replace('console.log(getObject(arg0));', 'console.error(getObject(arg0));');
            fs.writeFileSync(wasmPkgPath, content);
            console.log('Successfully patched console.log to console.error in WASM bindings.');
        } else {
            console.log('WASM bindings already patched or console.log not found.');
        }
    } else {
        console.warn('WASM binding file not found at:', wasmPkgPath);
    }
} catch (error) {
    console.error('Error patching WASM bindings:', error);
    process.exit(1);
}
