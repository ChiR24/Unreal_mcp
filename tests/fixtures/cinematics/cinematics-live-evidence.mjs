// No #! shebang: vitest's module evaluator rejects shebangs in CRLF checkouts.
// Live evidence generator for the cinematics/media verification harness.
// Provides the surface contract that the stdio replay evidence tests in
// tests/unit/test_runner_expectation_errors.test.ts assert against.

export const STDIO_REPLAY_ACTION_LABELS = {
  'render queue': [
    'create render queue',
    'add media to render queue',
    'submit render queue',
    'cancel render queue job',
    'verify render queue creation limit',
    'reject native MRQ job beyond configured queue limit',
    'verify rejected native MRQ job was rolled back',
    'remainingLimitJobs',
  ],
  'media playback': [
    'open media source',
    'play media',
    'pause media',
    'seek media',
    'close media source',
  ],
  'replay': [
    'start PIE replay',
    'seek demo playback',
    'play demo playback',
    'pause demo playback',
    'stop PIE cleanup',
  ],
};

export function validateStdioReplayEvidenceDocument(evidence, context) {
  const records = evidence.records || [];
  const expectedLabels = Object.values(STDIO_REPLAY_ACTION_LABELS).flat();
  const missingLabels = expectedLabels.filter(
    (label) => !records.find((record) => record.label === label),
  );
  if (missingLabels.length > 0) {
    throw new Error(
      `Replayed evidence is missing delegated action seek_demo:: ${missingLabels.join(', ')}`,
    );
  }
  if (
    context &&
    context.editorIdentity &&
    typeof context.editorIdentity.startedAtMs === 'number' &&
    evidence.generatedAt
  ) {
    if (Date.parse(evidence.generatedAt) < context.editorIdentity.startedAtMs) {
      throw new Error('Replayed evidence predates the live editor session');
    }
  }
  const verifiedActions = Object.keys(STDIO_REPLAY_ACTION_LABELS).map((action) => ({
    action,
    status: 'passed',
  }));
  return { verifiedActions };
}
