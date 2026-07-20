/**
 * manage_tools read-only capability-record specs: list_tools, list_categories,
 * get_status. These actions inspect dynamic tool state without mutation.
 *
 * Grounded in manageToolsToolDefinition (action enum + I/O schema) and the
 * manage-tools handler (local TS dispatch via dynamicToolManager). All 8
 * manage_tools actions are handled locally in TypeScript with no Unreal
 * bridge round-trip, so dispatchMode is 'local'.
 */
import type { CoreRecordSpec } from '../core/builder.js';
import { DOMAIN, NORM_CLASS, PARENT } from './shared.js';

export const MANAGE_TOOLS_READ_SPECS: readonly CoreRecordSpec[] = [
  {
    parentTool: PARENT,
    action: 'list_tools',
    dispatchMode: 'local',
    domain: DOMAIN,
    family: 'list',
    summary: 'List all canonical tools with their enabled state and category.',
    whenToUse: ['Enumerate every registered tool and its current visibility.'],
    whenNotToUse: ['When only aggregate counts are needed (use get_status).'],
    inputProps: {},
    required: [],
    outputProps: {
      tools: {
        type: 'array',
        items: {
          type: 'object',
          properties: {
            name: { type: 'string' },
            enabled: { type: 'boolean' },
            category: { type: 'string' },
          },
          additionalProperties: false,
        },
      },
      totalTools: { type: 'number' },
      enabledCount: { type: 'number' },
      disabledCount: { type: 'number' },
    },
    outputRequired: ['tools', 'totalTools', 'enabledCount', 'disabledCount'],
    effect: 'read',
    costLatency: 'instant',
    costResources: 'low',
    normalizationClass: NORM_CLASS,
    normalizationRationale:
      'Distinct manage_tools list_tools target; no shared canonical across tools.',
    exampleInput: { action: 'list_tools' },
    exampleOutput: {
      success: true,
      message: 'Listed 23 tools (23 enabled, 0 disabled).',
      tools: [{ name: 'manage_asset', enabled: true, category: 'core' }],
      totalTools: 23,
      enabledCount: 23,
      disabledCount: 0,
    },
  },
  {
    parentTool: PARENT,
    action: 'list_categories',
    dispatchMode: 'local',
    domain: DOMAIN,
    family: 'list',
    summary: 'List tool categories with enabled state and tool counts.',
    whenToUse: ['Inspect the four categories (core, world, gameplay, utility).'],
    whenNotToUse: ['When individual tool states are needed (use list_tools).'],
    inputProps: {},
    required: [],
    outputProps: {
      categories: {
        type: 'array',
        items: {
          type: 'object',
          properties: {
            name: { type: 'string' },
            enabled: { type: 'boolean' },
            toolCount: { type: 'number' },
            enabledCount: { type: 'number' },
          },
          additionalProperties: false,
        },
      },
      totalCategories: { type: 'number' },
    },
    outputRequired: ['categories', 'totalCategories'],
    effect: 'read',
    costLatency: 'instant',
    costResources: 'low',
    normalizationClass: NORM_CLASS,
    normalizationRationale:
      'Distinct manage_tools list_categories target; no shared canonical across tools.',
    exampleInput: { action: 'list_categories' },
    exampleOutput: {
      success: true,
      message: 'Listed 4 categories.',
      categories: [{ name: 'core', enabled: true, toolCount: 8, enabledCount: 8 }],
      totalCategories: 4,
    },
  },
  {
    parentTool: PARENT,
    action: 'get_status',
    dispatchMode: 'local',
    domain: DOMAIN,
    family: 'status',
    summary: 'Get the current enabled/disabled tool status and category breakdown.',
    whenToUse: ['Check aggregate tool visibility and per-category counts.'],
    whenNotToUse: ['When individual tool detail is needed (use list_tools).'],
    inputProps: {},
    required: [],
    outputProps: {
      totalTools: { type: 'number' },
      enabledTools: { type: 'number' },
      disabledTools: { type: 'number' },
      categories: {
        type: 'array',
        items: {
          type: 'object',
          properties: {
            name: { type: 'string' },
            enabled: { type: 'boolean' },
            toolCount: { type: 'number' },
            enabledCount: { type: 'number' },
          },
          additionalProperties: false,
        },
      },
    },
    outputRequired: ['totalTools', 'enabledTools', 'disabledTools', 'categories'],
    effect: 'read',
    costLatency: 'instant',
    costResources: 'low',
    normalizationClass: NORM_CLASS,
    normalizationRationale:
      'Distinct manage_tools get_status target; no shared canonical across tools.',
    exampleInput: { action: 'get_status' },
    exampleOutput: {
      success: true,
      message: '23/23 tools enabled.',
      totalTools: 23,
      enabledTools: 23,
      disabledTools: 0,
      categories: [{ name: 'core', enabled: true, toolCount: 8, enabledCount: 8 }],
    },
  },
];
