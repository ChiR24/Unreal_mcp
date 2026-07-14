import { runToolTests } from './tests/test-runner.mjs';

const testCases = [
  { scenario: 'Test dynamic handler toggle_fps', toolName: 'toggle_fps', arguments: {}, expected: 'success' },
  { scenario: 'Test dynamic handler clear_logs', toolName: 'clear_logs', arguments: {}, expected: 'success' }
];

runToolTests('DynamicHandlers', testCases).catch(console.error);
