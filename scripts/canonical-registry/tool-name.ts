// scripts/canonical-registry/tool-name.ts
//
// Shared legacy tool-name validation for the Task-23 generator.
//
// Mirrors the `LegacyToolNameSchema` regex in
// src/tools/catalog/capabilities/identifiers.ts (lower-snake-case legacy tool
// name) without pulling the Zod runtime into the pure generator path. Used by
// parent-derivation.ts to reject unknown/malformed parents BEFORE any file is
// written.

const LEGACY_TOOL_NAME_SOURCE = '^[a-z][a-z0-9_]*$';

export const LEGACY_TOOL_NAME_PATTERN = new RegExp(LEGACY_TOOL_NAME_SOURCE);
