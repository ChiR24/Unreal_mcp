import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

/**
 * Reads source with line endings normalized. The plugin tree is CRLF, so a
 * contract spelling a multi-line expected literal with \n would otherwise never
 * match and the case would fail for a reason that has nothing to do with the
 * invariant it guards.
 */
const readNormalized = (path: string): string =>
  readFileSync(path, 'utf8').replace(/\r\n/gu, '\n');

export const privateSource = (...parts: string[]): string =>
  readNormalized(
    resolve(
      process.cwd(),
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
      ...parts,
    ),
  );

export const publicSource = (...parts: string[]): string =>
  readNormalized(
    resolve(
      process.cwd(),
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Public',
      ...parts,
    ),
  );

export const cinematicsSource = (fileName: string): string =>
  privateSource('Domains', 'Sequence', 'Cinematics', fileName);

export const recordSource = (fileName: string): string =>
  readNormalized(
    resolve(
      process.cwd(),
      'src/tools/catalog/capabilities/records/manage-sequence',
      fileName,
    ),
  );

/** Returns the brace-balanced object literal that starts at `marker`. */
export const sliceObject = (source: string, marker: string): string => {
  const start = source.indexOf(marker);
  if (start < 0) throw new Error(`marker not found: ${marker}`);
  let depth = 0;
  for (let i = start + marker.length - 1; i < source.length; i += 1) {
    if (source[i] === '{') depth += 1;
    if (source[i] === '}') {
      depth -= 1;
      if (depth === 0) return source.slice(start, i + 1);
    }
  }
  throw new Error(`unbalanced object literal at marker: ${marker}`);
};
