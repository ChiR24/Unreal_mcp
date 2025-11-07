#!/usr/bin/env node
import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(__dirname, '..');
const srcRoot = path.resolve(repoRoot, 'src');

function isPositionInsideLiteralOrComment(content, pos) {
  let inSingle = false;
  let inDouble = false;
  let inBacktick = false;
  let inLineComment = false;
  let inBlockComment = false;

  for (let i = 0; i < pos; i += 1) {
    const ch = content[i];
    const next = content[i + 1];

    if (inLineComment) {
      if (ch === '\n') inLineComment = false;
      continue;
    }
    if (inBlockComment) {
      if (ch === '*' && next === '/') {
        inBlockComment = false; i += 1; continue;
      }
      continue;
    }
    if (inSingle) {
      if (ch === "\\") { i += 1; continue; }
      if (ch === "'") { inSingle = false; }
      continue;
    }
    if (inDouble) {
      if (ch === "\\") { i += 1; continue; }
      if (ch === '"') { inDouble = false; }
      continue;
    }
    if (inBacktick) {
      if (ch === "\\") { i += 1; continue; }
      if (ch === '`') { inBacktick = false; }
      continue;
    }

    // Not inside any literal/comment
    if (ch === '/' && next === '/') { inLineComment = true; i += 1; continue; }
    if (ch === '/' && next === '*') { inBlockComment = true; i += 1; continue; }
    if (ch === "'") { inSingle = true; continue; }
    if (ch === '"') { inDouble = true; continue; }
    if (ch === '`') { inBacktick = true; continue; }
  }

  return inSingle || inDouble || inBacktick || inLineComment || inBlockComment;
}

async function listTsFiles(dir) {
  const out = [];
  const entries = await fs.readdir(dir, { withFileTypes: true });
  for (const ent of entries) {
    if (ent.name === 'node_modules' || ent.name === 'dist' || ent.name === '.git') continue;
    const full = path.join(dir, ent.name);
    if (ent.isDirectory()) {
      out.push(...(await listTsFiles(full)));
    } else if (ent.isFile() && full.endsWith('.ts')) {
      out.push(full);
    }
  }
  return out;
}

function toLineCol(content, index) {
  const before = content.slice(0, index);
  const lines = before.split(/\r?\n/);
  const line = lines.length;
  const col = lines[lines.length - 1].length + 1;
  return { line, col };
}

async function main() {
  const pattern = /simulated\s*:/g;
  const files = await listTsFiles(srcRoot);
  const failures = [];

  for (const f of files) {
    const content = await fs.readFile(f, 'utf8');
    let m;
    while ((m = pattern.exec(content)) !== null) {
      const idx = m.index;
      if (isPositionInsideLiteralOrComment(content, idx)) continue;
      const pos = toLineCol(content, idx);
      const snippetLine = content.split(/\r?\n/)[pos.line - 1] ?? '';
      failures.push({ file: f, line: pos.line, col: pos.col, snippet: snippetLine.trim() });
    }
  }

  if (failures.length > 0) {
  console.error('\nDetected per-tool "simulated" alternative usages in TypeScript files:');
    for (const fail of failures) {
      console.error(`\n - ${path.relative(repoRoot, fail.file)}:${fail.line}:${fail.col}  -> ${fail.snippet}`);
    }
  console.error('\nPer-tool simulated alternatives are disallowed. Replace simulated behavior with explicit plugin actions or return an explicit error (e.g. MISSING_ENGINE_PLUGINS).');
    process.exitCode = 1;
    return;
  }

  console.log('No per-tool plugin "simulated" alternatives found in TypeScript source files.');
}

main().catch((err) => {
  console.error('Test failed with unexpected error:', err);
  process.exitCode = 2;
});
