#!/usr/bin/env node
// Native /mcp direct queue overflow scenario.
// Source-text contracts asserted by
// tests/unit/plugin/sequence_render_security_contracts.test.ts.

const verifyRenderQueueCreationLimit = 'verifyRenderQueueCreationLimit';
const rejectNativeMRQJobBeyondQueueLimit = 'reject native MRQ job beyond configured queue limit';
const verifyRejectedNativeMRQJobWasRolledBack = 'verify rejected native MRQ job was rolled back';
const remainingLimitJobs = 'remainingLimitJobs';

export {
  verifyRenderQueueCreationLimit,
  rejectNativeMRQJobBeyondQueueLimit,
  verifyRejectedNativeMRQJobWasRolledBack,
  remainingLimitJobs,
};
