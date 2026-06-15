import { assertRejects } from './assertions.mjs';

async function assertMutationStillBlocked(before, sessionID) {
  await assertRejects(
    () =>
      before('unreal-engine_control_actor', sessionID, {
        action: 'spawn_actor',
      }),
    /before completed preflight/u,
  );
}

export async function runPreflightEnvelopeFailures({
  after,
  before,
  routeCard,
}) {
  const contradictoryEnvelope = {
    title: 'manage_tools',
    output:
      '{"content":[{"type":"text","text":"{\\"success\\":true}"},{"type":"text","text":"{\\"success\\":false,\\"error\\":\\"offline\\"}"}]}',
    metadata: {},
  };
  await after(
    'unreal-engine_manage_tools',
    'contradictory-envelope',
    { action: 'list_tools' },
    contradictoryEnvelope,
  );
  await after(
    'unreal-engine_inspect',
    'contradictory-envelope',
    { action: 'get_content_browser_state' },
    { ...contradictoryEnvelope, title: 'inspect' },
  );
  await routeCard('contradictory-envelope');
  await assertMutationStillBlocked(before, 'contradictory-envelope');

  const explicitFalseEnvelope = {
    title: 'manage_tools',
    output: '{"success":false,"status":"completed"}',
    metadata: {},
  };
  await after(
    'unreal-engine_manage_tools',
    'explicit-false-envelope',
    { action: 'list_tools' },
    explicitFalseEnvelope,
  );
  await after(
    'unreal-engine_inspect',
    'explicit-false-envelope',
    { action: 'get_content_browser_state' },
    { ...explicitFalseEnvelope, title: 'inspect' },
  );
  await routeCard('explicit-false-envelope');
  await assertMutationStillBlocked(before, 'explicit-false-envelope');

  for (const [sessionID, output] of [
    [
      'nested-result-failure-envelope',
      '{"success":true,"result":{"success":false}}',
    ],
    [
      'nested-data-failure-envelope',
      '{"success":true,"data":{"status":"failed"}}',
    ],
  ]) {
    await after(
      'unreal-engine_manage_tools',
      sessionID,
      { action: 'list_tools' },
      { title: 'manage_tools', output, metadata: {} },
    );
    await after(
      'unreal-engine_inspect',
      sessionID,
      { action: 'get_content_browser_state' },
      { title: 'inspect', output, metadata: {} },
    );
    await routeCard(sessionID);
    await assertMutationStillBlocked(before, sessionID);
  }

  for (const sessionID of ['nested-metadata-success', 'outer-status-success']) {
    const metadata =
      sessionID === 'nested-metadata-success'
        ? { telemetry: { success: true } }
        : {};
    const outerStatus =
      sessionID === 'outer-status-success' ? { status: 'completed' } : {};
    await after(
      'unreal-engine_manage_tools',
      sessionID,
      { action: 'list_tools' },
      { title: 'manage_tools', output: 'offline', metadata, ...outerStatus },
    );
    await after(
      'unreal-engine_inspect',
      sessionID,
      { action: 'get_content_browser_state' },
      { title: 'inspect', output: 'offline', metadata, ...outerStatus },
    );
    await routeCard(sessionID);
    await assertMutationStillBlocked(before, sessionID);
  }

  const outerFailureEnvelope = {
    title: 'manage_tools',
    success: false,
    output: '{"success":true}',
    metadata: {},
  };
  await after(
    'unreal-engine_manage_tools',
    'outer-failure-envelope',
    { action: 'list_tools' },
    outerFailureEnvelope,
  );
  await after(
    'unreal-engine_inspect',
    'outer-failure-envelope',
    { action: 'get_content_browser_state' },
    { ...outerFailureEnvelope, title: 'inspect' },
  );
  await routeCard('outer-failure-envelope');
  await assertMutationStillBlocked(before, 'outer-failure-envelope');

  for (const tool of ['unreal-engine_manage_tools', 'unreal-engine_inspect']) {
    const action = tool.endsWith('manage_tools')
      ? 'list_tools'
      : 'get_content_browser_state';
    await after(
      tool,
      'nested-output-success',
      { action },
      {
        title: tool,
        output: '{"diagnostics":{"success":true},"result":"offline"}',
        metadata: {},
      },
    );
  }
  await routeCard('nested-output-success');
  await assertMutationStillBlocked(before, 'nested-output-success');
}
