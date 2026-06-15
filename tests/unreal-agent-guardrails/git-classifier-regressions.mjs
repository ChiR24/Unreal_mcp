import assert from 'node:assert/strict';

export function runGitClassifierRegressions(guardrailsModule) {
  const separatedAlias = "git -c alias.wipe='!rm -rf .' wipe";
  assert.deepEqual(
    guardrailsModule.tokenizeShellCommand(separatedAlias),
    ['git', '-c', 'alias.wipe=!rm -rf .', 'wipe'],
    'Quoted Git alias values must remain one shell token',
  );
  assert.equal(
    guardrailsModule.containsDestructiveGitCommand(separatedAlias),
    true,
    'Shell-backed Git aliases passed with a separate -c value must be destructive',
  );

  const inlineAlias = "git -c=alias.wipe='!rm -rf .' wipe";
  assert.deepEqual(
    guardrailsModule.tokenizeShellCommand(inlineAlias),
    ['git', '-c=alias.wipe=!rm -rf .', 'wipe'],
    'Quoted inline Git alias values must remain one shell token',
  );
  assert.equal(
    guardrailsModule.containsDestructiveGitCommand(inlineAlias),
    true,
    'Shell-backed Git aliases passed with inline -c must be destructive',
  );

  const ansiAlias = "git -c $'alias.wipe=!rm -rf .' wipe";
  assert.deepEqual(
    guardrailsModule.tokenizeShellCommand(ansiAlias),
    ['git', '-c', 'alias.wipe=!rm -rf .', 'wipe'],
    'ANSI-C quoted Git alias values must normalize to one shell token',
  );
  assert.equal(
    guardrailsModule.containsDestructiveGitCommand(ansiAlias),
    true,
    'ANSI-C quoted shell-backed Git aliases must be destructive',
  );

  const ansiEscapedAlias = String.raw`git -c $'alias.wipe=\x21rm -rf .' wipe`;
  assert.equal(
    guardrailsModule.containsDestructiveGitCommand(ansiEscapedAlias),
    true,
    'ANSI-C escaped shell-backed Git aliases must be destructive',
  );

  const configEnvironmentAlias =
    "UA_ALIAS='!rm -rf .' git --config-env=alias.wipe=UA_ALIAS wipe";
  assert.equal(
    guardrailsModule.containsDestructiveGitCommand(configEnvironmentAlias),
    true,
    'Git aliases sourced from environment variables must fail closed',
  );

  const ansiEscapedExecutable = String.raw`g$'\x69t' -C . reset --hard`;
  assert.deepEqual(
    guardrailsModule.tokenizeShellCommand(ansiEscapedExecutable),
    ['git', '-C', '.', 'reset', '--hard'],
    'ANSI-C escapes in executable names must be decoded',
  );
  assert.equal(
    guardrailsModule.containsDestructiveGitCommand(ansiEscapedExecutable),
    true,
    'ANSI-C escaped Git executable names must remain destructive',
  );

  const escapedGit = String.raw`g\it -C . reset --hard`;
  assert.deepEqual(
    guardrailsModule.tokenizeShellCommand(escapedGit),
    ['git', '-C', '.', 'reset', '--hard'],
    'POSIX escapes in Git executable names must be normalized',
  );
  assert.equal(
    guardrailsModule.containsDestructiveGitCommand(escapedGit),
    true,
    'Escaped Git executable names must remain destructive',
  );

  const escapedApply = String.raw`git ap\ply /tmp/policy.patch`;
  assert.deepEqual(
    guardrailsModule.tokenizeShellCommand(escapedApply),
    ['git', 'apply', '/tmp/policy.patch'],
    'POSIX escapes in Git subcommand names must be normalized',
  );
  assert.equal(
    guardrailsModule.containsDestructiveGitCommand(escapedApply),
    true,
    'Escaped Git apply commands must remain destructive',
  );

  for (const command of ['git restore .', 'git checkout .']) {
    assert.equal(
      guardrailsModule.containsDestructiveGitCommand(command),
      true,
      `${command} must remain destructive`,
    );
  }
}
