import { readdirSync, readFileSync, statSync } from 'node:fs';
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

describe('MCP_GATEWAY_MODE source contract (server registry)', () => {
    const files = collectTsFiles(serverDir);

    it('exposes no source file under src/server that reads process.env.MCP_GATEWAY_MODE directly', () => {
        const directReadPattern = /process\.env\.MCP_GATEWAY_MODE/;
        const offenders = files.filter((f) => directReadPattern.test(readFileSync(f, 'utf8')));
        expect(offenders).toEqual([]);
    });

    it('tool-registry.ts reads the validated config.MCP_GATEWAY_MODE instead of raw env', () => {
        const source = readFileSync(resolve(serverDir, 'tool-registry.ts'), 'utf8');
        expect(source).toContain("import { config } from '../config.js'");
        expect(source).toContain('return config.MCP_GATEWAY_MODE;');
        expect(source).not.toMatch(/process\.env\.MCP_GATEWAY_MODE/);
    });
});
