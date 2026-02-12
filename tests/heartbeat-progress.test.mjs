#!/usr/bin/env node
/**
 * Heartbeat/Progress Protocol Integration Tests
 * 
 * Tests the progress_update message protocol that keeps long-running
 * operations alive by extending their timeout on the server side.
 * 
 * Prerequisites:
 * - UE Editor must be running with the McpAutomationBridge plugin loaded
 * - MCP server must be built (npm run build:core)
 * 
 * Usage:
 *   node tests/heartbeat-progress.test.mjs
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  // Test 1: Basic progress protocol test - short operation with progress
  {
    scenario: 'Progress: Short operation with progress updates (5 steps, 200ms each)',
    toolName: 'system_control',
    arguments: {
      action: 'test_progress_protocol',
      steps: 5,
      stepDurationMs: 200,
      sendProgress: true
    },
    expected: 'success'
  },

  // Test 2: Progress protocol disabled - should still work but no progress updates
  {
    scenario: 'Progress: Operation without progress updates',
    toolName: 'system_control',
    arguments: {
      action: 'test_progress_protocol',
      steps: 3,
      stepDurationMs: 100,
      sendProgress: false
    },
    expected: 'success'
  },

  // Test 3: Longer operation - tests max extension limit
  // 10 progress updates hits MAX_PROGRESS_EXTENSIONS=10 limit and gets rejected
  // This is CORRECT behavior - safety mechanism prevents deadlock
  {
    scenario: 'Progress: Longer operation hits max extensions limit (10 steps = 11 updates)',
    toolName: 'system_control',
    arguments: {
      action: 'test_progress_protocol',
      steps: 10,
      stepDurationMs: 500,
      sendProgress: true
    },
    expected: { condition: 'error', errorPattern: 'max progress extensions' }
  },

  // Test 4: Edge case - minimum steps
  {
    scenario: 'Progress: Minimum steps (1 step)',
    toolName: 'system_control',
    arguments: {
      action: 'test_progress_protocol',
      steps: 1,
      stepDurationMs: 100,
      sendProgress: true
    },
    expected: 'success'
  },

  // Test 5: Within max extensions limit (9 steps = 10 updates, just under limit)
  {
    scenario: 'Progress: Within max extensions limit (9 steps)',
    toolName: 'system_control',
    arguments: {
      action: 'test_progress_protocol',
      steps: 9,
      stepDurationMs: 100,
      sendProgress: true
    },
    expected: 'success'
  },

  // Test 6: Stale progress test - sends same percent repeatedly
  // This SHOULD FAIL because stale detection triggers (correct safety behavior)
  {
    scenario: 'Progress: Stale detection triggers correctly (same percent 5 times)',
    toolName: 'system_control',
    arguments: {
      action: 'test_stale_progress',
      staleCount: 5
    },
    expected: { condition: 'error', errorPattern: 'stalled' }
  },

  // Test 7: Verify progress fields in response
  {
    scenario: 'Progress: Verify response contains progress test results',
    toolName: 'system_control',
    arguments: {
      action: 'test_progress_protocol',
      steps: 3,
      stepDurationMs: 100,
      sendProgress: true
    },
    expected: { condition: 'success', successPattern: 'progressSent' }
  },

  // Test 8: Very short operation - should complete within base timeout even without progress
  {
    scenario: 'Progress: Quick operation completes fast',
    toolName: 'system_control',
    arguments: {
      action: 'test_progress_protocol',
      steps: 2,
      stepDurationMs: 50,
      sendProgress: true
    },
    expected: 'success'
  }
];

console.log('='.repeat(60));
console.log('Heartbeat/Progress Protocol Integration Tests');
console.log('='.repeat(60));
console.log('');
console.log('Testing:');
console.log('  - progress_update message handling');
console.log('  - Timeout extension during long operations');
console.log('  - Stale progress detection');
console.log('  - Deadlock prevention mechanisms');
console.log('');
console.log('Configuration:');
console.log('  - DEFAULT_OPERATION_TIMEOUT_MS = 30000 (30s)');
console.log('  - PROGRESS_EXTENSION_MS = 30000 (30s per update)');
console.log('  - MAX_PROGRESS_EXTENSIONS = 10');
console.log('  - PROGRESS_STALE_THRESHOLD = 3');
console.log('  - ABSOLUTE_MAX_TIMEOUT_MS = 300000 (5 min)');
console.log('');

runToolTests('heartbeat-progress', testCases);
