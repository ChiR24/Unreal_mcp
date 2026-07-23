import { existsSync, readdirSync, readFileSync, statSync } from 'node:fs';
import { join, resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const serverDir = resolve(process.cwd(), 'src/server');

function collectTsFiles(dir: string): string[] {
    const out: string[] = [];
    for (const entry of readdirSync(dir)) {
        const full = join(dir, entry);
        const stat = statSync(full);
        if (stat.isDirectory()) {
            out.push(...collectTsFiles(full));
        } else if (entry.endsWith('.ts') && !entry.endsWith('.test.ts')) {
            out.push(full);
        }
    }
    return out;
}

// Task 30 makes the single-tool surface permanent. The gateway-mode toggle, its
// selector, and the legacy registry it selected are all removed from source.
describe('permanent single-tool surface source contract (no gateway toggle)', () => {
    const files = collectTsFiles(serverDir);

    it('no source file under src/server references MCP_GATEWAY_MODE at all', () => {
        // Not just raw process.env: the toggle is gone, so neither the config
        // field read nor any branch on it may survive anywhere under src/server.
        const offenders = files.filter((f) => /MCP_GATEWAY_MODE/.test(readFileSync(f, 'utf8')));
        expect(offenders).toEqual([]);
    });

    it('no source file under src/server defines or calls isGatewayMode', () => {
        // The mode selector disappears together with the flag it read.
        const offenders = files.filter((f) => /isGatewayMode/.test(readFileSync(f, 'utf8')));
        expect(offenders).toEqual([]);
    });

    it('the legacy 23-tool registry file (tool-registry-legacy.ts) is deleted', () => {
        // tool-registry-legacy.ts only existed to back MCP_GATEWAY_MODE=false.
        expect(existsSync(resolve(serverDir, 'tool-registry-legacy.ts'))).toBe(false);
    });
});
