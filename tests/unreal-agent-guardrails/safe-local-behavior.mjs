import assert from 'node:assert/strict';

export async function runSafeLocalBehavior({ after, before, hasSafeFileLink }) {
  await before('read', 'safe', { filePath: 'Config/DefaultEngine.ini' });
  await before('custom_reader', 'safe-extension-read', {
    filePath: 'Content/Readable.txt',
  });
  await before('execute_command', 'safe', "rg -n '\\.uasset' Docs");
  await before('execute_command', 'safe', 'rg -n "create actor" Docs');
  await before('execute_command', 'safe', 'tar -tf archive.tar --exclude=*.tmp');
  await before('execute_command', 'safe', 'git -C . clean -ndx');
  if (hasSafeFileLink) {
    await before('read', 'safe-symlink', { filePath: 'Docs/safe-link.md' });
  }

  const secretArgs = {
    filePath: 'Docs/Notes.md',
    authorization: 'Bearer live-secret',
    password: 'live-password',
  };
  const originalSecretArgs = structuredClone(secretArgs);
  await before('read', 'secrets', secretArgs);
  assert.deepEqual(
    secretArgs,
    originalSecretArgs,
    'Before hook must not mutate live tool args',
  );

  const secretOutput = {
    title: 'read',
    output: 'Authorization: Bearer response-secret',
    metadata: { password: 'response-password' },
  };
  await after('read', 'secrets', { filePath: 'Docs/Notes.md' }, secretOutput);
  assert.equal(secretOutput.output, '[REDACTED]');
  assert.equal(secretOutput.metadata.password, '[REDACTED]');

  const multilineSecretOutput = {
    title: 'read',
    output:
      'X-MCP-Capability-Token:\nlive-multiline-secret-part-1\nlive-multiline-secret-part-2\nsafe: value',
    metadata: {},
  };
  await after(
    'read',
    'multiline-secrets',
    { filePath: 'Docs/Notes.md' },
    multilineSecretOutput,
  );
  assert.equal(multilineSecretOutput.output, '[REDACTED]');

  const camelCaseSecretOutput = {
    title: 'read',
    output: 'accessToken: camel-response-secret',
    metadata: { refreshToken: 'camel-refresh-secret' },
  };
  await after(
    'read',
    'camel-case-secrets',
    { filePath: 'Docs/Notes.md' },
    camelCaseSecretOutput,
  );
  assert.equal(camelCaseSecretOutput.output, '[REDACTED]');
  assert.equal(camelCaseSecretOutput.metadata.refreshToken, '[REDACTED]');

  const capabilityTokenOutput = {
    title: 'read',
    output: 'safe',
    metadata: { capabilityToken: 'generic-capability-secret' },
  };
  await after(
    'read',
    'generic-capability-token',
    { filePath: 'Docs/Notes.md' },
    capabilityTokenOutput,
  );
  assert.equal(capabilityTokenOutput.metadata.capabilityToken, '[REDACTED]');

  for (const [sessionID, command] of [
    ['safe-python-docs-read', "python -c \"print(open('Docs/Notes.md').read())\""],
    ['safe-python-unreal-text', "python -c \"print('Unreal documentation')\""],
    ['safe-node-analysis', "node -e \"console.log('analysis')\""],
    [
      'safe-node-function',
      'node -e "function add(a,b){return a+b}; console.log(add(1,2))"',
    ],
    ['safe-ansi-doc-read', "cat Docs/Notes.$'md'"],
    ['safe-locale-doc-read', 'cat Docs/Notes.$"md"'],
    ['safe-binary-suffix-doc-read', 'cat Docs/guide.uasset.md'],
    ['safe-shell-info', 'bash --version'],
    ['safe-python-extension-mention', "python -c \"print('.uasset')\""],
  ]) {
    await assert.doesNotReject(
      () => before('execute_command', sessionID, command),
      `${sessionID} must remain reviewable local support work`,
    );
  }
  await assert.doesNotReject(
    () =>
      before('write', 'safe-nested-content-doc-write', {
        filePath: 'Docs/Content/guide.md',
        content: 'documentation',
      }),
    'A nested documentation folder named Content must not be treated as project Content',
  );
  await assert.doesNotReject(
    () =>
      before('write', 'safe-nested-config-doc-write', {
        filePath: 'Docs/Config/guide.ini',
        content: 'documentation',
      }),
    'A nested documentation folder named Config must not be treated as project Config',
  );
}
