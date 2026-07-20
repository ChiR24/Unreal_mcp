/**
 * tests/unit/audit-fixture-workspace.ts
 *
 * Shared temp-workspace plumbing for the Task-23 audit tests. Owns the tracked
 * temp-directory lifecycle so parameter_audit_schema, native_mcp_parity_audit,
 * and native_mcp_parity_schema each stay under the project 250 pure-LOC ceiling
 * without changing a single assertion.
 */
/// <reference types="node" />

import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { afterEach } from 'vitest';

const temporaryDirectories: string[] = [];

/** Creates a temp root that {@link registerTempRootCleanup} removes after each test. */
export function createTrackedTempRoot(prefix: string): string {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), prefix));
  temporaryDirectories.push(root);
  return root;
}

export function writeFixtureFile(root: string, relativePath: string, source: string): void {
  const filePath = path.join(root, relativePath);
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(filePath, source);
}

export function writeFixtureFiles(root: string, files: ReadonlyMap<string, string>): void {
  for (const [relativePath, source] of files) {
    writeFixtureFile(root, relativePath, source);
  }
}

/** Registers the file-wide afterEach hook that removes every tracked temp root. */
export function registerTempRootCleanup(): void {
  afterEach(() => {
    for (const directory of temporaryDirectories.splice(0)) {
      fs.rmSync(directory, { recursive: true, force: true });
    }
  });
}
