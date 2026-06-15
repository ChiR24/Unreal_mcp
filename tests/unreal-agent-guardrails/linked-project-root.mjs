import { join } from 'node:path';
import { assertRejects } from './assertions.mjs';

export async function runLinkedProjectRootBehavior({
  createDirectoryLink,
  guardrailsModule,
  projectDirectory,
  temporaryDirectory,
}) {
  const linkedProjectDirectory = join(temporaryDirectory, 'LinkedProject');
  await createDirectoryLink(projectDirectory, linkedProjectDirectory);
  const linkedHooks = await guardrailsModule.default({
    directory: linkedProjectDirectory,
  });
  await assertRejects(
    () =>
      linkedHooks['tool.execute.before'](
        {
          tool: 'write',
          sessionID: 'linked-root',
          callID: 'linked-root-write',
        },
        { args: { filePath: 'Docs/My Alias/Bypass.txt', content: 'x' } },
      ),
    /blocked direct Unreal (?:project-state file write|content\/package mutation)/u,
  );
}
