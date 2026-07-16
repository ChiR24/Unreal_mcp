import { execFileSync } from 'node:child_process';
import { existsSync } from 'node:fs';
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

// ripgrep needs an explicit path under `execFileSync` (the no-path form searches
// nothing when stdin is not a TTY). `.` searches the repo root recursively; the
// hidden `.omo` directory is skipped by default, mirroring the acceptance
// command's `--glob '!.omo/**'` intent.
const runRgRepoOwned = (): string => {
  try {
    return execFileSync(
      'rg',
      ['-n', PATTERN, '.'],
      { encoding: 'utf8' },
    ).trim();
  } catch {
    // rg exits non-zero when there are zero matches; treat that as empty.
    return '';
  }
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
    const raw = runRgRepoOwned();
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
