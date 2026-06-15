import { assertRejects } from './assertions.mjs';
import { createBlockedLocalMutationCases } from './blocked-local-mutation-cases.mjs';

export async function runBlockedLocalBehavior({
  before,
  hasBinaryFileLink,
  hasSafeFileLink,
  projectDirectory,
}) {
  const blockedCases = createBlockedLocalMutationCases(projectDirectory);
  if (hasSafeFileLink) {
    blockedCases.push([
      'write',
      { filePath: 'Docs/safe-link.md', content: 'must not follow links' },
    ]);
  }
  for (const [tool, args] of blockedCases) {
    await assertRejects(
      () => before(tool, `blocked-${tool}-${JSON.stringify(args)}`, args),
      /blocked (?:direct (?:Unreal (?:project-state file write|content\/package mutation)|local Unreal editor-state access|local mutation through a symbolic link)|indirect Unreal project mutation|destructive local shell command)/u,
      `${tool}: ${JSON.stringify(args)}`,
    );
  }

  const blockedBinaryReads = [
    'cat Content/Danger.uasse?',
    'cat Content/Danger.uasse[t]',
    'cat Content/Danger.uasse{t,p}',
    'cat Docs/{Danger.uasse,Other.tx}t',
    'cat Docs/Danger.uasse{,t}t',
    'cat Docs/Danger.uasse{t..t}',
    String.raw`cat Docs/Danger.uasse\t`,
    "cat Docs/Danger.uasse$'t'",
    String.raw`cat Docs/Danger.uasse$'\x74'`,
    String.raw`rg -n "\.uasset" Content/Danger.uasset`,
    "cat Docs/Danger.uasse't'",
    'cat Docs/Danger.uasse$"t"',
    'cat Docs/Danger.uasse"t"',
    `cat Content/Danger.${'*'.repeat(600)}uasset`,
    "python -c \"print(open('Content/Danger.uasset','rb').read())\"",
    "node -e \"console.log(require('fs').readFileSync('Content/Danger.uasset'))\"",
  ];
  if (hasBinaryFileLink) {
    blockedBinaryReads.push(
      "node -e \"console.log(require('fs').readFileSync('Docs/cache.bin'))\"",
    );
  }
  for (const command of blockedBinaryReads) {
    await assertRejects(
      () => before('execute_command', `binary-${command}`, command),
      /blocked direct \.uasset\/\.umap filesystem access/u,
      command,
    );
  }
  await assertRejects(
    () =>
      before('custom_reader', 'binary-custom-reader', {
        filePath: 'Content/Danger.uasset',
      }),
    /blocked direct \.uasset\/\.umap filesystem access/u,
    'custom_reader binary asset path',
  );
  await assertRejects(
    () =>
      before('custom_reader', 'binary-custom-reader-hidden', {
        payload: { blob: 'Content/Danger.uasset' },
      }),
    /blocked direct \.uasset\/\.umap filesystem access/u,
    'custom_reader hidden binary asset path',
  );
}
