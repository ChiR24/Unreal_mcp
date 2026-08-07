import { execFileSync } from 'node:child_process';
import { existsSync, readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';

// ============================================================================
// Task 7 — ACP panel plugin removal contract (focused, Given/When/Then)
// ----------------------------------------------------------------------------
// This contract is written FIRST and is intentionally RED while the
// experimental editor ACP panel plugin subtree still exists. After the subtree
// is deleted and every repository-owned touchpoint is reconciled, the same
// contract must go GREEN.
//
// Given  : the repository after Task 7 reconciliation
// When   : the ACP panel plugin subtree and its references are removed
// Then   : (1) the plugin directory is absent,
//          (2) its .uplugin manifest is absent,
//          (3) an `rg` scan over repository-owned files (excluding `.omo`)
//              returns exactly ONE line — the single intentional
//              external-consumer migration/deprecation statement.
// ============================================================================

// Assembled from fragments on purpose so this test file itself never contains
// any of the contiguous patterns the acceptance `rg` scan searches for.
const UA = 'Unreal' + 'Agent';
const SK = 'Studio' + 'Kit';
const OCA = 'opencode' + ' acp';
const OCAP = 'Open' + 'Code ACP';
const PATTERN = [UA, SK, OCA, OCAP].join('|');

const PLUGIN_DIR = `plugins/${UA}`;
const UPLUGIN = `plugins/${UA}/${UA}.uplugin`;

// The single permitted residual reference must read as a migration note.
const isIntentionalStatement = (line: string): boolean =>
  line.includes(UA) && /removed/i.test(line) && /external consumer/i.test(line);

// "Repository-owned" is exactly the set of TRACKED files, so the scan enumerates
// them with `git ls-files` and reads them in process. `.omo` is untracked, so it
// is excluded for free — no glob needed.
//
// This replaced a `rg` shell-out that could never pass in CI. ripgrep is not
// installed on GitHub runners and nothing in the workflow installs it, so
// `execFileSync` threw ENOENT; the catch treated that exactly like ripgrep's
// "no matches" exit code and returned an empty string. A MISSING TOOL therefore
// reported as a MISSING MIGRATION STATEMENT, and the contract only passed on
// machines that happened to have rg on PATH. Reading the files here removes the
// external dependency, so the contract means the same thing everywhere.
const isBinary = (contents: Buffer): boolean => contents.subarray(0, 8192).includes(0);

const runRepoOwnedScan = (): string => {
  const tracked = execFileSync('git', ['ls-files', '-z'], {
    encoding: 'utf8',
    maxBuffer: 64 * 1024 * 1024,
  })
    .split('\0')
    .filter(Boolean);

  const matcher = new RegExp(PATTERN);
  const hits: string[] = [];
  for (const file of tracked) {
    let contents: Buffer;
    try {
      contents = readFileSync(file);
    } catch {
      // Listed but unreadable: a submodule entry, or deleted mid-scan.
      continue;
    }
    if (isBinary(contents)) continue;
    contents
      .toString('utf8')
      .split(/\r?\n/u)
      .forEach((line, index) => {
        if (matcher.test(line)) hits.push(`${file}:${index + 1}:${line}`);
      });
  }
  return hits.join('\n');
};

describe('Task 7 — ACP panel plugin removal contract', () => {
  it('Given the plugin subtree, When removed, the directory must be absent', () => {
    expect(
      existsSync(PLUGIN_DIR),
      `${PLUGIN_DIR} must be deleted by Task 7`,
    ).toBe(false);
  });

  it('the plugin .uplugin manifest must be absent', () => {
    expect(
      existsSync(UPLUGIN),
      `${UPLUGIN} must be deleted by Task 7`,
    ).toBe(false);
  });

  it('rg over repo-owned files returns only the intentional migration statement', () => {
    const raw = runRepoOwnedScan();
    const lines = raw.split('\n').filter(Boolean);
    expect(
      lines.length,
      `expected exactly one residual match (the intentional migration statement), found ${lines.length}:\n${raw}`,
    ).toBe(1);
    expect(
      isIntentionalStatement(lines[0]),
      'the single residual match must be the intentional migration/deprecation statement',
    ).toBe(true);
  });
});
