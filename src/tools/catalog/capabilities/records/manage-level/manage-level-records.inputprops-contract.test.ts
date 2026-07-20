/**
 * Source-contract test: Task 15 manage_level record `inputProps` must not
 * redundantly declare `action: P.action`, because `buildCoreRecord` already
 * injects the canonical `action` property into every input schema
 * (builder.ts ACTION_PROP).
 *
 * Redundant declaration is dead weight: the builder spreads `inputProps`
 * after `action: ACTION_PROP`, so a caller-supplied `action: P.action` either
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

import { MANAGE_LEVEL_RECORDS, MANAGE_LEVEL_SOURCES } from './index.js';

const here = dirname(fileURLToPath(import.meta.url));

const DATA_FILES = ['operations.data.ts', 'lifecycle.data.ts'] as const;

const ACTION_PROP_DESCRIPTION = 'The action to execute on the parent tool.';

describe('manage_level Task 15 inputProps do not redundantly declare action', () => {
  it('no data file passes `action: P.action` inside inputProps', () => {
    for (const file of DATA_FILES) {
      const path = resolve(here, file);
      const source = readFileSync(path, 'utf8');
      const lines = source.split('\n');
      let inInputProps = false;
      const offenders: string[] = [];
      for (const line of lines) {
        if (line.includes('inputProps')) inInputProps = true;
        if (inInputProps && line.includes('action: P.action')) {
          offenders.push(`${file}: ${line.trim()}`);
        }
        if (inInputProps && /^\s*\}\s*,?\s*$/.test(line) && !line.includes('inputProps')) {
          inInputProps = false;
        }
      }
      expect(offenders, `redundant action: P.action in ${file}`).toEqual([]);
    }
  });

  it('every generated manage_level schema still contains the builder-injected action property', () => {
    for (const record of MANAGE_LEVEL_RECORDS) {
      const props = record.schemas.input.properties as Record<string, unknown>;
      expect(props).toHaveProperty('action');
      const actionProp = props.action as { type?: string; description?: string };
      expect(actionProp.type).toBe('string');
      expect(actionProp.description).toBe(ACTION_PROP_DESCRIPTION);
      expect(record.schemas.input.required).toContain('action');
    }
  });

  it('every manage_level source schema input is closed (additionalProperties false)', () => {
    for (const source of MANAGE_LEVEL_SOURCES) {
      expect(source.schemas.input.additionalProperties).toBe(false);
    }
  });
});
