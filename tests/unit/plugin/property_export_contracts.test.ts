// A property read exports its value through MCP_PROPERTY_EXPORT_TEXT, which always writes it. ExportText_InContainer and
// ExportText_Direct skip a value identical to the delta they are given, and a null delta reads as zeroed memory: every
// 0 and false came back as an empty string from the change journal, an Animation Blueprint node's read-back, a
// Blueprint's component defaults and the combat, interaction and inventory readers. Source contracts only.

import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const SOURCE = join('plugins', 'EngineWire', 'Source');

/** Every C++ source file under the plugin, recursively. */
const sources = (dir: string): string[] =>
  readdirSync(dir).flatMap((name) => {
    const path = join(dir, name);
    return statSync(path).isDirectory() ? sources(path) : /\.(cpp|h)$/u.test(name) ? [path] : [];
  });

describe('property values export in full', () => {
  it('never exports through the calls that skip a value equal to their delta, but where the delta is the value itself', () => {
    const offenders: string[] = [];
    for (const file of sources(SOURCE)) {
      const text = readFileSync(file, 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');
      for (const match of text.matchAll(/ExportText_(?:InContainer|Direct)\(([^;]*)\);/gu)) {
        // Data == Delta always exports (a CDO read against itself, as project settings read).
        if (!/\b(\w+), \1, \1, PPF_None/u.test(match[1] ?? '')) {
          offenders.push(`${file}: ${match[0]}`);
        }
      }
    }
    expect(offenders).toEqual([]);
  });

  it('writes the value on UE 5.0 too, where ExportText_Direct is the call that skips it', () => {
    const compat = readFileSync(join(SOURCE, 'EngineWire', 'Private', 'Core', 'Compatibility', 'McpVersionCompatibility.h'), 'utf8');
    expect(compat).toContain('(Property)->ExportTextItem(OutText, ValuePtr, DefaultValuePtr, Container, Flags, nullptr)');
    expect(compat).toContain('(Property)->ExportTextItem_Direct(OutText, ValuePtr, DefaultValuePtr, Container, Flags)');
  });
});
