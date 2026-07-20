import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

export const privateSource = (...parts: string[]): string =>
  readFileSync(
    resolve(
      process.cwd(),
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
      ...parts,
    ),
    'utf8',
  );

export const publicSource = (...parts: string[]): string =>
  readFileSync(
    resolve(
      process.cwd(),
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Public',
      ...parts,
    ),
    'utf8',
  );

export const cinematicsSource = (fileName: string): string =>
  privateSource('Domains', 'Sequence', 'Cinematics', fileName);

export const recordSource = (fileName: string): string =>
  readFileSync(
    resolve(
      process.cwd(),
      'src/tools/catalog/capabilities/records/manage-sequence',
      fileName,
    ),
    'utf8',
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
