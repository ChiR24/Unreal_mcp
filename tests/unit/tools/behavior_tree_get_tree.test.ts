import { describe, it, expect } from 'vitest';
import {
  BEHAVIOR_TREE_ACTIONS,
  consolidatedToolDefinitions,
} from '../../../src/tools/consolidated-tool-definitions.js';

describe('manage_behavior_tree get_tree action wiring (TS)', () => {
  it('includes get_tree in BEHAVIOR_TREE_ACTIONS', () => {
    expect(BEHAVIOR_TREE_ACTIONS).toContain('get_tree');
  });

  it('surfaces get_tree in the manage_ai action enum (via the spread)', () => {
    const manageAi = consolidatedToolDefinitions.find((t) => t.name === 'manage_ai');
    expect(manageAi, 'manage_ai tool definition must exist').toBeTruthy();
    const actionEnum =
      (manageAi as { inputSchema?: { properties?: { action?: { enum?: string[] } } } })
        ?.inputSchema?.properties?.action?.enum ?? [];
    expect(actionEnum).toContain('get_tree');
  });
});
