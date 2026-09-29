// manage_tools capability records, authored in the parent's action enum order.
import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { buildCoreRecord, type CoreRecordSpec } from '../core/builder.js';

const CATEGORY_ENUM: JsonObject = {
  type: 'string',
  enum: ['core', 'world', 'gameplay', 'utility', 'all'],
  description: 'Category name to enable/disable.',
};

const TOOLS_ARRAY: JsonObject = {
  type: 'array',
  items: { type: 'string' },
  description: 'Tool names to enable or disable.',
};

const SPECS: readonly Omit<CoreRecordSpec, 'parentTool' | 'domain'>[] = [
  {
    action: 'list_tools',
    family: 'tool-catalog',
    topics: ['list tools', 'available tools', 'all tools', 'which tools', 'tool list', 'capabilities'],
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
    action: 'list_categories',
    family: 'tool-catalog',
    topics: ['tool groups', 'core world gameplay utility'],
    summary: 'List the four tool categories (core, world, gameplay, utility), each with its enabled flag, tool count and enabled tool count.',
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
    exampleInput: { action: 'list_categories' },
    exampleOutput: {
      success: true,
      message: 'Listed 4 categories.',
      categories: [{ name: 'core', enabled: true, toolCount: 8, enabledCount: 8 }],
      totalCategories: 4,
    },
  },
  {
    action: 'enable_tools',
    family: 'tool-visibility',
    summary: 'Enable specific tools by name.',
    whenToUse: ['Re-enable tools that were previously disabled.'],
    whenNotToUse: ['When the tools are already enabled.'],
    inputProps: { tools: TOOLS_ARRAY },
    required: ['tools'],
    outputProps: {
      enabled: { type: 'array', items: { type: 'string' } },
      notFound: { type: 'array', items: { type: 'string' } },
    },
    outputRequired: ['enabled'],
    effect: 'write',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    exampleInput: { action: 'enable_tools', tools: ['manage_asset'] },
    exampleOutput: {
      success: true,
      message: 'Enabled 1 tools.',
      enabled: ['manage_asset'],
      notFound: [],
    },
  },
  {
    action: 'disable_tools',
    family: 'tool-visibility',
    summary: 'Disable specific tools by name.',
    whenToUse: ['Hide non-essential tools from the MCP tool list.'],
    whenNotToUse: [
      'To disable protected tools (manage_tools, inspect) - they are protected and always enabled.',
    ],
    inputProps: { tools: TOOLS_ARRAY },
    required: ['tools'],
    outputProps: {
      disabled: { type: 'array', items: { type: 'string' } },
      notFound: { type: 'array', items: { type: 'string' } },
      protected: { type: 'array', items: { type: 'string' } },
    },
    outputRequired: ['disabled', 'protected'],
    effect: 'write',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    exampleInput: { action: 'disable_tools', tools: ['manage_audio'] },
    exampleOutput: {
      success: true,
      message: 'Disabled 1 tools.',
      disabled: ['manage_audio'],
      notFound: [],
      protected: [],
    },
  },
  {
    action: 'enable_category',
    family: 'tool-visibility',
    summary: 'Enable all tools in a category.',
    whenToUse: ['Re-enable an entire category or all categories at once.'],
    whenNotToUse: ['When only specific tools need re-enabling (use enable_tools).'],
    inputProps: { category: CATEGORY_ENUM },
    required: ['category'],
    outputProps: {
      category: { type: 'string' },
      enabled: { type: 'array', items: { type: 'string' } },
    },
    outputRequired: ['category', 'enabled'],
    effect: 'write',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    exampleInput: { action: 'enable_category', category: 'world' },
    exampleOutput: {
      success: true,
      message: "Enabled category 'world' (4 tools).",
      category: 'world',
      enabled: ['build_environment', 'manage_geometry', 'manage_pcg', 'manage_level_structure'],
    },
  },
  {
    action: 'disable_category',
    family: 'tool-visibility',
    summary: 'Disable all tools in a category.',
    whenToUse: ['Hide an entire non-essential category from the tool list.'],
    whenNotToUse: [
      'To disable the core category - it is protected and remains enabled.',
    ],
    inputProps: { category: CATEGORY_ENUM },
    required: ['category'],
    outputProps: {
      category: { type: 'string' },
      disabled: { type: 'array', items: { type: 'string' } },
      protected: { type: 'array', items: { type: 'string' } },
    },
    outputRequired: ['category', 'disabled', 'protected'],
    effect: 'write',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    exampleInput: { action: 'disable_category', category: 'utility' },
    exampleOutput: {
      success: true,
      message: "Disabled category 'utility' (3 tools disabled).",
      category: 'utility',
      disabled: ['manage_sequence', 'manage_audio', 'manage_networking'],
      protected: [],
    },
  },
  {
    action: 'get_status',
    family: 'tool-status',
    topics: ['how many tools are enabled', 'catalog revision', 'enabled and disabled counts'],
    summary: 'Read how many tools are enabled and disabled, the per-category breakdown, and the catalog revision numbers.',
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
  {
    action: 'reset',
    family: 'tool-visibility',
    summary: 'Reset all tools and categories to their default enabled state.',
    whenToUse: ['Restore the full default tool set after disabling tools or categories.'],
    whenNotToUse: ['When only a subset needs re-enabling (use enable_tools or enable_category).'],
    inputProps: {},
    required: [],
    outputProps: {
      enabled: { type: 'number' },
    },
    outputRequired: ['enabled'],
    effect: 'write',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    exampleInput: { action: 'reset' },
    exampleOutput: {
      success: true,
      message: 'Reset complete. 0 tools re-enabled.',
      enabled: 0,
    },
  },
];

export const MANAGE_TOOLS_SOURCES: readonly CapabilityRecordSource[] =
  SPECS.map((spec) => buildCoreRecord({ parentTool: 'manage_tools', domain: 'tools', ...spec }));
