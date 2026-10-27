// scripts/canonical-registry/types.ts
//
// Shared types and low-level helpers for the Task-23 canonical registry
// generator. No side effects; pure functions only.

import { createHash } from 'node:crypto';

export type JsonSchemaNode = Record<string, unknown>;

export const sha256Hex = (text: string): string =>
  createHash('sha256').update(text, 'utf8').digest('hex');

// Escape a value for a C++ double-quoted string literal (TEXT("...")).
export const cppStringLiteral = (value: string): string =>
  value
    .replace(/\\/g, '\\\\')
    .replace(/"/g, '\\"')
    .replace(/\n/g, '\\n')
    .replace(/\r/g, '\\r')
    .replace(/\t/g, '\\t');

// Convert a tool name such as "manage_level_structure" to "ManageLevelStructure".
export const pascalCase = (name: string): string =>
  name
    .split(/[_-]/)
    .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
    .join('');
