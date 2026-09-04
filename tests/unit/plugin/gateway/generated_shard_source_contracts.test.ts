/// <reference types="node" />

import { readFileSync, readdirSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// Task 25: the generated capability shards become part of a real translation
// unit, so their C++ framing must survive MSVC. Two hard compiler limits and one
// correctness hazard are pinned here:
//
//   * C2026 - a single string literal may not exceed 65,535 bytes. The Task-23
//     framing emitted ONE raw literal per shard, up to 416,805 characters.
//   * C1064 - a source line may not overflow the compiler's token buffer. Raw
//     literals carried the whole shard on one logical line.
//   * R"MCPCS( ... )MCPCS" terminates early if the payload ever contains the
//     closing delimiter. Escaped narrow literals remove the hazard entirely
//     rather than relying on the corpus never producing that byte sequence.
//
// RED before the generator change: shards are orphan `.h` files holding one
// oversized raw literal that no `.cpp` includes.

const generatedDir = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Generated',
);

// MSVC caps a string literal at 65,535 bytes and a source line at 16,384
// tokens' worth of text. 4,000 payload characters cannot exceed either bound
// even if every character needs a two-character escape.
const MAX_CHUNK_CHARS = 4000;

const INDEX_HEADER = 'McpGeneratedCapabilityShards.h';

const shardSources = (): readonly string[] =>
  readdirSync(generatedDir)
    .filter((name) => name.startsWith('McpGeneratedCapabilityShards_') && name.endsWith('.cpp'))
    .sort();

const readGenerated = (name: string): string =>
  readFileSync(resolve(generatedDir, name), 'utf8');

/** Reverse the generator's C++ escaping for a single TEXT("...") literal body. */
const unescapeLiteral = (body: string): string =>
  body.replace(/\\(["\\])/gu, '$1');

/** Concatenated payload of one shard translation unit, in emission order. */
const shardPayload = (source: string): string =>
  [...source.matchAll(/^\tTEXT\("((?:[^"\\]|\\.)*)"\),?$/gmu)]
    .map((match) => unescapeLiteral(match[1] ?? ''))
    .join('');

describe('Task 25: generated capability shards are MSVC-safe translation units', () => {
  it('emits one compiled .cpp per canonical parent instead of orphan headers', () => {
    expect(shardSources()).toHaveLength(23);
    const orphanHeaders = readdirSync(generatedDir).filter(
      (name) => name.startsWith('McpGeneratedCapabilityShards_') && name.endsWith('.h'),
    );
    expect(orphanHeaders).toEqual([]);
  });

  it('keeps every string literal under the MSVC 65,535-byte ceiling', () => {
    expect(shardSources().length).toBeGreaterThan(0);
    const oversized: string[] = [];
    for (const name of shardSources()) {
      for (const match of readGenerated(name).matchAll(/^\tTEXT\("((?:[^"\\]|\\.)*)"\),?$/gmu)) {
        const payload = unescapeLiteral(match[1] ?? '');
        if (payload.length > MAX_CHUNK_CHARS) {
          oversized.push(`${name}: ${payload.length}`);
        }
      }
    }
    expect(oversized).toEqual([]);
  });

  // A payload byte 'R' landing just before a literal's closing quote produces the
  // characters R" without opening a raw string, so the contract is that no raw
  // literal is ever OPENED and the Task-23 delimiter is gone.
  it('never opens a raw string literal, so no delimiter can close one early', () => {
    expect(shardSources().length).toBeGreaterThan(0);
    for (const name of [...shardSources(), INDEX_HEADER]) {
      const source = readGenerated(name);
      expect(source).not.toContain('TEXT(R"');
      expect(source).not.toContain('MCPCS');
    }
  });

  it('emits pure ASCII so no universal-character-name escape is ever required', () => {
    expect(shardSources().length).toBeGreaterThan(0);
    const nonAscii: string[] = [];
    for (const name of [...shardSources(), INDEX_HEADER]) {
      const bytes = readFileSync(resolve(generatedDir, name));
      if (bytes.some((byte) => byte > 0x7e)) nonAscii.push(name);
    }
    expect(nonAscii).toEqual([]);
  });

  it('round-trips every chunked shard back to its canonical record array', () => {
    let total = 0;
    for (const name of shardSources()) {
      const payload = shardPayload(readGenerated(name));
      expect(payload.length, `${name} produced no payload`).toBeGreaterThan(0);
      const parsed: unknown = JSON.parse(payload);
      expect(Array.isArray(parsed), `${name} is not a record array`).toBe(true);
      total += (parsed as readonly unknown[]).length;
    }
    expect(total).toBe(1401);
  });

  it('publishes a shard table the native loader can enumerate without the payloads', () => {
    const index = readGenerated(INDEX_HEADER);
    expect(index).toContain('struct FMcpCapabilityShard');
    expect(index).toContain('inline int32 Num()');
    expect(index).toContain('inline const FMcpCapabilityShard& At(int32 Index)');
    expect(index).toContain('inline int32 TotalRecordCount()');
    expect(index).toContain('inline const TCHAR* CatalogRevision()');
    expect(index).not.toContain('extern const TCHAR* MCP_CAP_SHARD_');
  });

  it('links each shard translation unit against the shared index header', () => {
    expect(shardSources().length).toBeGreaterThan(0);
    for (const name of shardSources()) {
      expect(readGenerated(name)).toContain('#include "MCP/Generated/McpGeneratedCapabilityShards.h"');
    }
  });
});
