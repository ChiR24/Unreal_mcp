#!/usr/bin/env node
/**
 * manage_data Tool Integration Tests
 * Covers Phase 31 Data & Persistence actions.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const SLOT_NAME = `TestSaveSlot_${ts}`;

const testCases = [
  // === CONFIG SYSTEM ===
  {
    scenario: 'CONFIG: write_config_value',
    toolName: 'manage_data',
    arguments: { action: 'write_config_value', configFilename: 'DefaultGame.ini', configSection: 'MCPTest', configKey: 'TestKey', configValue: 'TestValue' },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.success', equals: true, label: 'config value written' }]
  },
  {
    scenario: 'CONFIG: read_config_value',
    toolName: 'manage_data',
    arguments: { action: 'read_config_value', configFilename: 'DefaultGame.ini', configSection: 'MCPTest', configKey: 'TestKey' },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.configValue', equals: 'TestValue', label: 'config value read matches written value' }]
  },
  {
    scenario: 'CONFIG: flush_config',
    toolName: 'manage_data',
    arguments: { action: 'flush_config', configFilename: 'DefaultGame.ini' },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.success', equals: true, label: 'config flushed' }]
  },

  // === SAVE SYSTEM ===
  {
    scenario: 'SAVE: check_save_slot_exists (false)',
    toolName: 'manage_data',
    arguments: { action: 'check_save_slot_exists', slotName: SLOT_NAME, userIndex: 0 },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.exists', equals: false, label: 'non-existent slot returns false' }]
  },
  {
    scenario: 'SAVE: delete_save_slot (not found)',
    toolName: 'manage_data',
    arguments: { action: 'delete_save_slot', slotName: SLOT_NAME, userIndex: 0 },
    expected: 'error'
  },

  // === GAMEPLAY TAGS ===
  {
    scenario: 'TAGS: create_gameplay_tag (not implemented)',
    toolName: 'manage_data',
    arguments: { action: 'create_gameplay_tag', tagName: `Test.Tag.${ts}`, tagComment: 'Created by MCP test' },
    expected: 'error'
  }
];

runToolTests('manage-data', testCases);
