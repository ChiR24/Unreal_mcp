const assert = require('node:assert/strict');
const test = require('node:test');
const { applyAdapterExit, applyDapEvent } = require('../dist/dap-state.js');

test('fake adapter events drive the documented session state machine', () => {
  const record = { state: 'starting' };

  applyDapEvent(record, 'process', { systemProcessId: 4242 });
  assert.equal(record.targetPid, 4242);

  applyDapEvent(record, 'stopped', { reason: 'breakpoint' });
  assert.deepEqual(record, {
    state: 'stopped',
    targetPid: 4242,
    stoppedReason: 'breakpoint'
  });

  applyDapEvent(record, 'continued');
  assert.equal(record.state, 'running');
  assert.equal(record.stoppedReason, undefined);

  applyDapEvent(record, 'terminated');
  assert.equal(record.state, 'terminated');
});

test('a fake adapter crash changes the session to error', () => {
  const record = { state: 'running' };
  applyAdapterExit(record, 1, undefined);
  assert.equal(record.state, 'error');
});

test('a pause acknowledgement alone does not synthesize a stopped event', () => {
  const record = { state: 'running' };
  assert.equal(record.state, 'running');
  applyDapEvent(record, 'stopped', { reason: 'pause' });
  assert.equal(record.state, 'stopped');
});
