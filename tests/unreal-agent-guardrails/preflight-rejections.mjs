import { assertRejects } from './assertions.mjs';

export async function runPreflightRejections({
  after,
  before,
  routeCard,
}) {
  await before('unreal-engine_manage_asset', 'readonly', {
    action: 'get_material_details',
  });
  await assertRejects(
    () =>
      before('unreal-engine_manage_tools', 'mutation-without-preflight', {
        action: 'enable_tools',
      }),
    /before completed preflight/u,
  );
  await assertRejects(
    () =>
      before('unreal-engine_custom_mutator', 'unknown-mutation-without-preflight', {
        action: 'delete_all',
      }),
    /before completed preflight/u,
  );
  for (const [tool, action] of [
    ['unreal-engine_custom_mutator', 'get_and_delete_all'],
    ['unreal-engine_control_actor', 'get_and_delete_all'],
  ]) {
    await assertRejects(
      () => before(tool, `unknown-getter-${tool}`, { action }),
      /before completed preflight/u,
    );
  }

  await after(
    'evil_manage_tools',
    'spoofed-preflight',
    { action: 'list_tools' },
    { title: 'manage_tools', output: '{"success":true}', metadata: {} },
  );
  await after(
    'evil_inspect',
    'spoofed-preflight',
    { action: 'get_content_browser_state' },
    { title: 'inspect', output: '{"success":true}', metadata: {} },
  );
  await routeCard('spoofed-preflight');
  await assertRejects(
    () =>
      before('unreal-engine_control_actor', 'spoofed-preflight', {
        action: 'spawn_actor',
      }),
    /before completed preflight/u,
  );

  for (const [sessionID, output] of [
    [
      'plain-failure',
      { title: 'manage_tools', output: 'Bridge unavailable', metadata: {} },
    ],
    [
      'mcp-failure',
      { title: 'manage_tools', output: 'MCP error -32000: offline', metadata: {} },
    ],
    ['empty-failure', { title: 'manage_tools', output: '', metadata: {} }],
  ]) {
    await after(
      'unreal-engine_manage_tools',
      sessionID,
      { action: 'list_tools' },
      output,
    );
    await after(
      'unreal-engine_inspect',
      sessionID,
      { action: 'get_content_browser_state' },
      { ...output, title: 'inspect' },
    );
    await routeCard(sessionID);
    await assertRejects(
      () =>
        before('unreal-engine_control_actor', sessionID, {
          action: 'spawn_actor',
        }),
      /before completed preflight/u,
    );
  }

  await after(
    'unreal-engine_manage_tools',
    'failed-preflight',
    { action: 'list_tools' },
    {
      title: 'manage_tools',
      output: '{"success":false}',
      metadata: { isError: true },
    },
  );
  await after(
    'unreal-engine_inspect',
    'failed-preflight',
    { action: 'get_content_browser_state' },
    { title: 'inspect', output: 'Error: bridge unavailable', metadata: {} },
  );
  await routeCard('failed-preflight');
  await assertRejects(
    () =>
      before('unreal-engine_control_actor', 'failed-preflight', {
        action: 'spawn_actor',
      }),
    /before completed preflight/u,
  );

  for (const [sessionID, failedOutput] of [
    [
      'metadata-error-spoof',
      {
        title: 'manage_tools',
        output: '{"success":true}',
        metadata: { isError: true },
      },
    ],
    [
      'outer-error-spoof',
      {
        title: 'manage_tools',
        output: '{"success":true}',
        isError: true,
        metadata: {},
      },
    ],
  ]) {
    await after(
      'unreal-engine_manage_tools',
      sessionID,
      { action: 'list_tools' },
      failedOutput,
    );
    await after(
      'unreal-engine_inspect',
      sessionID,
      { action: 'get_content_browser_state' },
      { ...failedOutput, title: 'inspect' },
    );
    await routeCard(sessionID);
    await assertRejects(
      () =>
        before('unreal-engine_control_actor', sessionID, {
          action: 'spawn_actor',
        }),
      /before completed preflight/u,
    );
  }
}
