import assert from 'node:assert/strict';
import { assertRejects } from './assertions.mjs';

export async function runMutationLifecycle({
  after,
  before,
  hooks,
  routeCard,
}) {
  const concurrentSession = 'concurrent-mutations';
  await after(
    'unreal-engine_manage_tools',
    concurrentSession,
    { action: 'list_tools' },
    { title: 'manage_tools', output: '{"success":true}', metadata: {} },
  );
  await after(
    'unreal-engine_inspect',
    concurrentSession,
    { action: 'get_content_browser_state' },
    { title: 'inspect', output: '{"success":true}', metadata: {} },
  );
  await routeCard(concurrentSession);
  for (const callID of ['concurrent-1', 'concurrent-2', 'concurrent-3']) {
    await before(
      'unreal-engine_control_actor',
      concurrentSession,
      { action: 'spawn_actor' },
      callID,
    );
  }
  await assertRejects(
    () =>
      before(
        'unreal-engine_control_actor',
        concurrentSession,
        { action: 'spawn_actor' },
        'concurrent-4',
      ),
    /after stale (?:inspection|route card)/u,
  );
  await after(
    'unreal-engine_control_actor',
    concurrentSession,
    { action: 'spawn_actor' },
    { title: 'control_actor', output: '{"success":false}', metadata: {} },
    'concurrent-1',
  );
  await before(
    'unreal-engine_control_actor',
    concurrentSession,
    { action: 'spawn_actor' },
    'concurrent-after-failure',
  );

  const abandonedSession = 'abandoned-mutations';
  await after(
    'unreal-engine_manage_tools',
    abandonedSession,
    { action: 'list_tools' },
    { title: 'manage_tools', output: '{"success":true}', metadata: {} },
  );
  await after(
    'unreal-engine_inspect',
    abandonedSession,
    { action: 'get_content_browser_state' },
    { title: 'inspect', output: '{"success":true}', metadata: {} },
  );
  await routeCard(abandonedSession);
  for (const callID of ['abandoned-1', 'abandoned-2', 'abandoned-3']) {
    await before(
      'unreal-engine_control_actor',
      abandonedSession,
      { action: 'spawn_actor' },
      callID,
    );
  }
  await assertRejects(
    () =>
      before(
        'unreal-engine_control_actor',
        abandonedSession,
        { action: 'spawn_actor' },
        'abandoned-4',
      ),
    /after stale (?:inspection|route card)/u,
  );
  await after(
    'unreal-engine_inspect',
    abandonedSession,
    { action: 'get_content_browser_state' },
    { title: 'inspect', output: '{"success":true}', metadata: {} },
  );
  await routeCard(
    abandonedSession,
    'assistant',
    `${abandonedSession}-recovery-message`,
  );
  await assert.doesNotReject(
    () =>
      before(
        'unreal-engine_control_actor',
        abandonedSession,
        { action: 'spawn_actor' },
        'abandoned-recovered',
      ),
    'A successful fresh inspect must release abandoned mutation reservations',
  );
  await after(
    'unreal-engine_control_actor',
    abandonedSession,
    { action: 'spawn_actor' },
    { title: 'control_actor', output: '{"success":false}', metadata: {} },
    'abandoned-recovered',
  );
  await after(
    'unreal-engine_control_actor',
    abandonedSession,
    { action: 'spawn_actor' },
    { title: 'control_actor', output: '{"success":true}', metadata: {} },
    'abandoned-1',
  );
  await assertRejects(
    () =>
      before(
        'unreal-engine_control_actor',
        abandonedSession,
        { action: 'spawn_actor' },
        'abandoned-after-late-success',
      ),
    /before completed preflight/u,
  );

  const deletedSession = 'deleted-session';
  await after(
    'unreal-engine_manage_tools',
    deletedSession,
    { action: 'list_tools' },
    { title: 'manage_tools', output: '{"success":true}', metadata: {} },
  );
  await after(
    'unreal-engine_inspect',
    deletedSession,
    { action: 'get_content_browser_state' },
    { title: 'inspect', output: '{"success":true}', metadata: {} },
  );
  await routeCard(deletedSession);
  await hooks.event({
    event: {
      type: 'session.deleted',
      properties: {
        info: { id: deletedSession },
      },
    },
  });
  await assertRejects(
    () =>
      before(
        'unreal-engine_control_actor',
        deletedSession,
        { action: 'spawn_actor' },
        'deleted-session-reused',
      ),
    /before completed preflight/u,
  );

  await after(
    'unreal-engine_manage_tools',
    'route-mismatch',
    { action: 'list_tools' },
    { title: 'manage_tools', output: '{"success":true}', metadata: {} },
  );
  await after(
    'unreal-engine_inspect',
    'route-mismatch',
    { action: 'get_content_browser_state' },
    { title: 'inspect', output: '{"success":true}', metadata: {} },
  );
  await routeCard('route-mismatch');
  await assertRejects(
    () =>
      before('unreal-engine_control_actor', 'route-mismatch', {
        action: 'delete_actor',
      }),
    /does not match the exact parent tool\/action/u,
  );

  for (const flag of ['ignored', 'synthetic']) {
    const sessionID = `${flag}-route-card`;
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
    await routeCard(
      sessionID,
      'assistant',
      `${sessionID}-message`,
      undefined,
      { [flag]: true },
    );
    await assertRejects(
      () =>
        before('unreal-engine_control_actor', sessionID, {
          action: 'spawn_actor',
        }),
      /before route-card preflight/u,
    );
  }

  await after(
    'unreal-engine_manage_tools',
    'user-route-card',
    { action: 'list_tools' },
    { title: 'manage_tools', output: '{"success":true}', metadata: {} },
  );
  await after(
    'unreal-engine_inspect',
    'user-route-card',
    { action: 'get_content_browser_state' },
    { title: 'inspect', output: '{"success":true}', metadata: {} },
  );
  await routeCard('user-route-card', 'user');
  await assertRejects(
    () =>
      before('unreal-engine_control_actor', 'user-route-card', {
        action: 'spawn_actor',
      }),
    /before route-card preflight/u,
  );
}
