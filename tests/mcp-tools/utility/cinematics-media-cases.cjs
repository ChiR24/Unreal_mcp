// Case aggregation for the cinematics/media live harness.
// Mirrors the small coverage-accounting surface that
// tests/unit/test_runner_expectation_errors.test.ts asserts against.
//
// The eight Media actions below are exercised by
// tests/mcp-tools/utility/cinematics-media.test.mjs (T5 lane L3). They are
// treated as known/coverage actions so the accounting helper no longer flags
// them as unknown extras once the live harness owns them.

const KNOWN_LEGACY_ACTIONS = new Set([
  'add_actor',
  'remove_actor',
  'set_actor_transform',
]);

const COVERED_MEDIA_ACTIONS = new Set([
  'create_media_player',
  'create_media_source',
  'create_media_texture',
  'create_media_sound_component',
  'create_media_playlist',
  'play_media',
  'pause_media',
  'seek_media',
]);

// Everything the cinematics/media live harness legitimately covers. Anything
// outside this set is an unknown/extra action worth surfacing.
const KNOWN_CINEMATICS_ACTIONS = new Set([
  ...KNOWN_LEGACY_ACTIONS,
  ...COVERED_MEDIA_ACTIONS,
]);

function summarizeCinematicsCoverage(records) {
  const extraActions = [];
  for (const record of records) {
    const action = record && record.arguments && record.arguments.action;
    if (typeof action !== 'string') continue;
    if (record.toolName !== 'manage_sequence') continue;
    if (KNOWN_CINEMATICS_ACTIONS.has(action)) continue;
    extraActions.push(action);
  }
  return { extraActions };
}

module.exports = {
  summarizeCinematicsCoverage,
  KNOWN_LEGACY_ACTIONS,
  COVERED_MEDIA_ACTIONS,
};
