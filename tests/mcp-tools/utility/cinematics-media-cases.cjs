// Case aggregation for the cinematics/media live harness.
// Mirrors the small coverage-accounting surface that
// tests/unit/test_runner_expectation_errors.test.ts asserts against.

const KNOWN_LEGACY_ACTIONS = new Set([
  'add_actor',
  'remove_actor',
  'set_actor_transform',
]);

function summarizeCinematicsCoverage(records) {
  const extraActions = [];
  for (const record of records) {
    const action = record && record.arguments && record.arguments.action;
    if (typeof action !== 'string') continue;
    if (record.toolName !== 'manage_sequence') continue;
    if (KNOWN_LEGACY_ACTIONS.has(action)) continue;
    extraActions.push(action);
  }
  return { extraActions };
}

module.exports = { summarizeCinematicsCoverage };
