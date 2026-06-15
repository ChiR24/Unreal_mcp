import assert from 'node:assert/strict';
import { runBlockedLocalBehavior } from './unreal-agent-guardrails/blocked-local-behavior.mjs';
import { runGitClassifierRegressions } from './unreal-agent-guardrails/git-classifier-regressions.mjs';
import { createHookDriver } from './unreal-agent-guardrails/hook-driver.mjs';
import { runLinkedProjectRootBehavior } from './unreal-agent-guardrails/linked-project-root.mjs';
import { runMutationLifecycle } from './unreal-agent-guardrails/mutation-lifecycle.mjs';
import { runPreflightEnvelopeFailures } from './unreal-agent-guardrails/preflight-envelope-failures.mjs';
import { runPreflightRejections } from './unreal-agent-guardrails/preflight-rejections.mjs';
import {
  createProjectFixture,
  removeProjectFixture,
} from './unreal-agent-guardrails/project-fixture.mjs';
import { runRouteCardBehavior } from './unreal-agent-guardrails/route-card-behavior.mjs';
import { runSafeLocalBehavior } from './unreal-agent-guardrails/safe-local-behavior.mjs';
import { loadGuardrailsModule } from './unreal-agent-guardrails/source-reconstruction.mjs';

async function main() {
  const fixture = await createProjectFixture();
  try {
    const guardrailsModule = await loadGuardrailsModule(
      fixture.temporaryDirectory,
    );
    runGitClassifierRegressions(guardrailsModule);
    const safeAnsiCommand = "cat Docs/Notes.$'md'";
    assert.deepEqual(
      {
        direct:
          guardrailsModule.UNREAL_BINARY_ASSET_PATTERN.test(safeAnsiCommand),
        resolved: guardrailsModule.containsResolvedBinaryAssetPath(
          safeAnsiCommand,
          true,
        ),
        normalizedDirect: guardrailsModule.UNREAL_BINARY_ASSET_PATTERN.test(
          guardrailsModule.normalizeShellForSafety(safeAnsiCommand),
        ),
        glob: guardrailsModule.containsUnrealBinaryAssetGlob(safeAnsiCommand),
      },
      {
        direct: false,
        resolved: false,
        normalizedDirect: false,
        glob: false,
      },
      'Safe ANSI-C documentation paths must not match binary-asset detectors',
    );
    const hooks = await guardrailsModule.default({
      directory: fixture.projectDirectory,
    });
    assert.equal(
      hooks['command.execute.before'],
      undefined,
      'Route-card commands must not unlock preflight before assistant output',
    );
    const driver = createHookDriver(hooks);
    const context = { ...fixture, ...driver, guardrailsModule };

    await runSafeLocalBehavior(context);
    await runBlockedLocalBehavior(context);
    await runPreflightRejections(context);
    await runPreflightEnvelopeFailures(context);
    await runRouteCardBehavior(context);
    await runMutationLifecycle(context);
    await runLinkedProjectRootBehavior(context);

    process.stdout.write(
      'Unreal Agent generated guardrails runtime tests passed.\n',
    );
  } finally {
    await removeProjectFixture(fixture.temporaryDirectory);
  }
}

await main();
