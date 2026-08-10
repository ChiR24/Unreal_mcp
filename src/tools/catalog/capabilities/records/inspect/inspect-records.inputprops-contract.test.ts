/**
 * Source-contract test: Task 15 record `inputProps` must not redundantly
 * declare `action: P.action`, because `buildCoreRecord` already injects the
 * canonical `action` property into every input schema (builder.ts ACTION_PROP).
 *
 * Redundant declaration is dead weight: the builder spreads `inputProps` after
 * `action: ACTION_PROP`, so a caller-supplied `action: P.action` either
 * overrides the canonical property with an identical value (no-op) or, if the
 * caller ever drifts, silently diverges from the builder contract. The single
 * source of truth is the builder.
 *
 * This test fails before cleanup (the redundant entries exist) and passes after
 * cleanup, while the behavioral assertions below prove the builder-injected
 * `action` schema remains present and required in every generated record.
 */
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';

import { INSPECT_RECORDS, INSPECT_SOURCES } from './index.js';

const here = dirname(fileURLToPath(import.meta.url));

const DATA_FILES = [
  'object-property.data.ts',
  'component-actor.data.ts',
  'global-runtime.data.ts',
] as const;

const ACTION_PROP_DESCRIPTION = 'The action to execute on the parent tool.';

describe('inspect Task 15 inputProps do not redundantly declare action', () => {
  it('no data file passes `action: P.action` inside inputProps', () => {
    for (const file of DATA_FILES) {
      const path = resolve(here, file);
      const source = readFileSync(path, 'utf8');
      // Match `action: P.action` only within inputProps blocks. The builder
      // injects `action` itself, so any caller-supplied `action: P.action`
      // inside inputProps is redundant.
      const lines = source.split('\n');
      let inInputProps = false;
      const offenders: string[] = [];
      for (const line of lines) {
        if (line.includes('inputProps')) inInputProps = true;
        if (inInputProps && line.includes('action: P.action')) {
          offenders.push(`${file}: ${line.trim()}`);
        }
        // An inputProps block ends when the closing brace of the object is
        // reached on its own line (the `},` or `}` after the props).
        if (inInputProps && /^\s*\}\s*,?\s*$/.test(line) && !line.includes('inputProps')) {
          inInputProps = false;
        }
      }
      expect(offenders, `redundant action: P.action in ${file}`).toEqual([]);
    }
  });

  it('every generated inspect schema still contains the builder-injected action property', () => {
    for (const record of INSPECT_RECORDS) {
      const props = record.schemas.input.properties as Record<string, unknown>;
      expect(props).toHaveProperty('action');
      const actionProp = props.action as { type?: string; description?: string };
      expect(actionProp.type).toBe('string');
      expect(actionProp.description).toBe(ACTION_PROP_DESCRIPTION);
      expect(record.schemas.input.required).toContain('action');
    }
  });

  it('every inspect source schema input is closed (additionalProperties false)', () => {
    for (const source of INSPECT_SOURCES) {
      expect(source.schemas.input.additionalProperties).toBe(false);
    }
  });
});
