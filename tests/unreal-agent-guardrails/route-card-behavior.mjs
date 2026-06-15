import assert from 'node:assert/strict';
import { assertRejects } from './assertions.mjs';

export async function runRouteCardBehavior({
  after,
  before,
  hooks,
  routeCard,
}) {
  await after(
    'unreal-engine_manage_tools',
    'cross-session-b',
    { action: 'list_tools' },
    { title: 'manage_tools', output: '{"success":true}', metadata: {} },
  );
  await after(
    'unreal-engine_inspect',
    'cross-session-b',
    { action: 'get_content_browser_state' },
    { title: 'inspect', output: '{"success":true}', metadata: {} },
  );
  const crossMessageID = 'cross-session-message';
  await hooks.event({
    event: {
      type: 'message.updated',
      properties: {
        info: {
          id: crossMessageID,
          sessionID: 'cross-session-a',
          role: 'assistant',
        },
      },
    },
  });
  await hooks.event({
    event: {
      type: 'message.part.updated',
      properties: {
        part: {
          id: 'cross-session-part',
          sessionID: 'cross-session-b',
          messageID: crossMessageID,
          type: 'text',
          text: [
            'Intent: change one actor',
            'Evidence: current actor inspected',
            'Tool route: control_actor/spawn_actor',
            'Mutation bounds: one actor',
            'Safety: no overwrite',
            'Validation: inspect actor',
            'Rollback: delete created actor',
          ].join('\n'),
        },
      },
    },
  });
  await assertRejects(
    () =>
      before('unreal-engine_control_actor', 'cross-session-b', {
        action: 'spawn_actor',
      }),
    /before route-card preflight/u,
  );

  for (const sessionID of ['metadata-route-card', 'empty-route-card']) {
    await after(
      'unreal-engine_manage_tools',
      sessionID,
      { action: 'list_tools' },
      { title: 'manage_tools', output: '{"success":true}', metadata: {} },
    );
    await after(
      'unreal-engine_inspect',
      sessionID,
      { action: 'get_content_browser_state' },
      { title: 'inspect', output: '{"success":true}', metadata: {} },
    );
    if (sessionID === 'metadata-route-card') {
      await routeCard(
        sessionID,
        'assistant',
        `${sessionID}-assistant-message`,
        'No route card was emitted.',
        {
          metadata: {
            note: [
              'Intent: hidden',
              'Evidence: hidden',
              'Tool route: hidden',
              'Mutation bounds: hidden',
              'Safety: hidden',
              'Validation: hidden',
              'Rollback: hidden',
            ].join('\n'),
          },
        },
      );
    } else {
      await routeCard(
        sessionID,
        'assistant',
        `${sessionID}-assistant-message`,
        [
          'Intent:',
          'Evidence:',
          'Tool route:',
          'Mutation bounds:',
          'Safety:',
          'Validation:',
          'Rollback:',
        ].join('\n'),
      );
    }
    await assertRejects(
      () =>
        before('unreal-engine_control_actor', sessionID, {
          action: 'spawn_actor',
        }),
      /before route-card preflight/u,
    );
  }

  const staleRouteSession = 'stale-route-replay';
  const staleMessageID = `${staleRouteSession}-old-message`;
  await routeCard(staleRouteSession, 'assistant', staleMessageID);
  await after(
    'unreal-engine_manage_tools',
    staleRouteSession,
    { action: 'list_tools' },
    { title: 'manage_tools', output: '{"success":true}', metadata: {} },
  );
  await after(
    'unreal-engine_inspect',
    staleRouteSession,
    { action: 'get_content_browser_state' },
    { title: 'inspect', output: '{"success":true}', metadata: {} },
  );
  await routeCard(staleRouteSession, 'assistant', staleMessageID);
  await assertRejects(
    () =>
      before('unreal-engine_control_actor', staleRouteSession, {
        action: 'spawn_actor',
      }),
    /before route-card preflight/u,
  );

  await after(
    'unreal-engine_manage_tools',
    'complete-preflight',
    { action: 'list_tools' },
    { title: 'manage_tools', output: '{"success":true}', metadata: {} },
  );
  await after(
    'unreal-engine_inspect',
    'complete-preflight',
    { action: 'get_content_browser_state' },
    { title: 'inspect', output: '{"success":true}', metadata: {} },
  );
  await routeCard('complete-preflight');
  await before('unreal-engine_control_actor', 'complete-preflight', {
    action: 'spawn_actor',
  });

  for (const [sessionID, inventoryOutput, inspectOutput] of [
    [
      'content-envelope-preflight',
      {
        title: 'manage_tools',
        output:
          '{"content":[{"type":"text","text":"{\\"success\\":true}"}]}',
        metadata: {},
      },
      {
        title: 'inspect',
        output:
          '{"content":[{"type":"text","text":"{\\"status\\":\\"completed\\"}"}]}',
        metadata: {},
      },
    ],
    [
      'structured-content-preflight',
      {
        title: 'manage_tools',
        output:
          '{"content":[{"type":"text","text":"ok"}],"structuredContent":{"success":true},"isError":false}',
        metadata: {},
      },
      {
        title: 'inspect',
        output:
          '{"content":[{"type":"text","text":"ok"}],"structuredContent":{"state":"succeeded"},"isError":false}',
        metadata: {},
      },
    ],
  ]) {
    await after(
      'unreal-engine_manage_tools',
      sessionID,
      { action: 'list_tools' },
      inventoryOutput,
    );
    await after(
      'unreal-engine_inspect',
      sessionID,
      { action: 'get_content_browser_state' },
      inspectOutput,
    );
    await routeCard(sessionID);
    await assert.doesNotReject(
      () =>
        before(
          'unreal-engine_control_actor',
          sessionID,
          { action: 'spawn_actor' },
          `${sessionID}-mutation`,
        ),
      `${sessionID} must accept serialized successful MCP envelopes`,
    );
  }
}
