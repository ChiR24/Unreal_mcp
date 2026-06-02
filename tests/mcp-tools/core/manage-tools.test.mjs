#!/usr/bin/env node
/**
 * manage_tools Tool Integration Tests
 * Covers all 8 actions with proper setup/teardown sequencing.
 */

import { runToolTests } from '../../test-runner.mjs';

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: reset dynamic tool state', toolName: 'manage_tools', arguments: { action: 'reset' }, expected: 'success' },

  // === INFO ===
  { scenario: 'INFO: list_tools', toolName: 'manage_tools', arguments: { action: 'list_tools' }, expected: 'success', assertions: [{ path: 'structuredContent.tools', includesObject: { name: 'manage_tools', enabled: true, category: 'core' }, label: 'manage_tools remains visible and protected' }] },
  { scenario: 'INFO: list_categories', toolName: 'manage_tools', arguments: { action: 'list_categories' }, expected: 'success', assertions: [{ path: 'structuredContent.categories', includesObject: { name: 'gameplay' }, label: 'gameplay category is registered' }] },
  // === TOGGLE ===
  { scenario: 'TOGGLE: enable_tools', toolName: 'manage_tools', arguments: { action: 'enable_tools', tools: ['system_control'] }, expected: 'success', assertions: [{ path: 'structuredContent.enabled', length: 1, label: 'enable_tools reports the requested tool' }] },
  { scenario: 'TOGGLE: disable_tools', toolName: 'manage_tools', arguments: { action: 'disable_tools', tools: ['system_control'] }, expected: 'success', assertions: [{ path: 'structuredContent.disabled', length: 1, label: 'disable_tools reports the requested tool' }] },
  { scenario: 'TOGGLE: enable_category', toolName: 'manage_tools', arguments: { action: 'enable_category', category: 'gameplay' }, expected: 'success', assertions: [{ path: 'structuredContent.category', equals: 'gameplay', label: 'enable_category echoes category' }] },
  { scenario: 'TOGGLE: disable_category', toolName: 'manage_tools', arguments: { action: 'disable_category', category: 'gameplay' }, expected: 'success', assertions: [{ path: 'structuredContent.category', equals: 'gameplay', label: 'disable_category echoes category' }] },
  // === INFO ===
  { scenario: 'INFO: get_status', toolName: 'manage_tools', arguments: { action: 'get_status' }, expected: 'success', assertions: [{ path: 'structuredContent.categories', includesObject: { name: 'gameplay', enabled: false }, label: 'get_status reflects disabled gameplay category' }] },
  // === ACTION ===
  { scenario: 'ACTION: reset', toolName: 'manage_tools', arguments: { action: 'reset' }, expected: 'success' },

  // === CLEANUP ===
  { scenario: 'Cleanup: reset dynamic tool state', toolName: 'manage_tools', arguments: { action: 'reset' }, expected: 'success' },
];

runToolTests('manage-tools', testCases);
