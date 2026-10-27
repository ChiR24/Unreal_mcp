import { createRequire } from 'node:module';

// package.json ships beside dist/ (and src/), so name and version come from it.
export const PACKAGE = createRequire(import.meta.url)('../package.json') as { name: string; version: string };
