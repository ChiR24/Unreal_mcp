#!/usr/bin/env node
/**
 * Automation Timeout Behaviour Test Suite
 *
 * Validates that long-running automation bridge actions honour timeout
 * overrides, surface automation_event completions, and emit structured
 * error payloads when exceeding configured limits.
 */

import { runToolTests } from './test-runner.mjs';

const LONG_TIMEOUT_MS = 60000;
const SHORT_TIMEOUT_MS = 2500;

const testCases = [
  {
    scenario: 'Blueprint add_variable completes within custom timeout',
    groupName: 'Automation Timeout',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'add_variable',
      name: '/Game/Blueprints/BP_TimeTest',
      variableName: 'TempVar',
      variableType: 'Float',
      waitForCompletion: true,
      waitForCompletionTimeoutMs: LONG_TIMEOUT_MS
    },
    expected: 'success'
  },
  {
    scenario: 'Blueprint add_variable short waitForCompletion timeout should fail',
    groupName: 'Automation Timeout',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'add_variable',
      name: '/Game/Blueprints/BP_TimeTest',
      variableName: 'ShortTimeoutVar',
      variableType: 'Float',
      waitForCompletion: true,
      waitForCompletionTimeoutMs: SHORT_TIMEOUT_MS
    },
    expected: 'fail or timeout'
  },
  {
    scenario: 'Sequence create honours waitForCompletion timeout override',
    groupName: 'Automation Timeout',
    toolName: 'manage_sequence',
    arguments: {
      action: 'create',
      sequenceName: 'Seq_TempAutomation',
      path: '/Game/Sequences',
      waitForCompletion: true,
      waitForCompletionTimeoutMs: LONG_TIMEOUT_MS
    },
    expected: 'success'
  },
  {
    scenario: 'Sequence create fails when waitForCompletion timeout too small',
    groupName: 'Automation Timeout',
    toolName: 'manage_sequence',
    arguments: {
      action: 'create',
      sequenceName: 'Seq_TimeoutAutomation',
      path: '/Game/Sequences',
      waitForCompletion: true,
      waitForCompletionTimeoutMs: SHORT_TIMEOUT_MS
    },
    expected: 'fail or timeout'
  },
  {
    scenario: 'Set object property with explicit timeout succeeds',
    groupName: 'Automation Timeout',
    toolName: 'inspect',
    arguments: {
      action: 'set_property',
      objectPath: '/Game/Blueprints/BP_TimeTest',
      propertyName: 'bAllowTickBeforeBeginPlay',
      value: true,
      timeoutMs: LONG_TIMEOUT_MS
    },
    expected: 'success'
  },
  {
    scenario: 'Set object property fails with too-small timeout override',
    groupName: 'Automation Timeout',
    toolName: 'inspect',
    arguments: {
      action: 'set_property',
      objectPath: '/Game/Blueprints/BP_TimeTest',
      propertyName: 'bAllowTickBeforeBeginPlay',
      value: false,
      timeoutMs: SHORT_TIMEOUT_MS
    },
    expected: 'fail or timeout'
  }
];

await runToolTests('Automation Timeout', testCases);
