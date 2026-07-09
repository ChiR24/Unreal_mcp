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
