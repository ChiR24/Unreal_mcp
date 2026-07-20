/**
 * manage_tools mutating capability-record specs: enable_tools, disable_tools,
 * enable_category, disable_category, reset. These actions change dynamic tool
 * visibility state.
 *
 * Grounded in manageToolsToolDefinition (action enum + I/O schema), the
 * manage-tools handler (local TS dispatch), and dynamic-tool-state-operations
 * (protected tool/category guards). Protected tools (manage_tools, inspect)
 * and the core category cannot be disabled; this is surfaced truthfully in
 * discovery guidance and normalization rationale rather than advertising
 * successful configurability.
 */
import type { CoreRecordSpec } from '../core/builder.js';
import { CATEGORY_ENUM, DOMAIN, NORM_CLASS, PARENT, TOOLS_ARRAY } from './shared.js';

export const MANAGE_TOOLS_WRITE_SPECS: readonly CoreRecordSpec[] = [
  {
    parentTool: PARENT,
    action: 'enable_tools',
    dispatchMode: 'local',
    domain: DOMAIN,
    family: 'enable',
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
    costLatency: 'instant',
    costResources: 'low',
    normalizationClass: NORM_CLASS,
    normalizationRationale:
      'Distinct manage_tools enable_tools target; no shared canonical across tools.',
    exampleInput: { action: 'enable_tools', tools: ['manage_asset'] },
    exampleOutput: {
      success: true,
      message: 'Enabled 1 tools.',
      enabled: ['manage_asset'],
      notFound: [],
    },
  },
  {
    parentTool: PARENT,
    action: 'disable_tools',
    dispatchMode: 'local',
    domain: DOMAIN,
    family: 'disable',
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
    costLatency: 'instant',
    costResources: 'low',
    normalizationClass: NORM_CLASS,
    normalizationRationale:
      'Distinct manage_tools disable_tools target. Protected tools (manage_tools, inspect) cannot be disabled and are returned in the protected array; they remain enabled.',
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
    parentTool: PARENT,
    action: 'enable_category',
    dispatchMode: 'local',
    domain: DOMAIN,
    family: 'enable',
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
    costLatency: 'instant',
    costResources: 'low',
    normalizationClass: NORM_CLASS,
    normalizationRationale:
      'Distinct manage_tools enable_category target; no shared canonical across tools.',
    exampleInput: { action: 'enable_category', category: 'world' },
    exampleOutput: {
      success: true,
      message: "Enabled category 'world' (4 tools).",
      category: 'world',
      enabled: ['build_environment', 'manage_geometry', 'manage_pcg', 'manage_level_structure'],
    },
  },
  {
    parentTool: PARENT,
    action: 'disable_category',
    dispatchMode: 'local',
    domain: DOMAIN,
    family: 'disable',
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
    costLatency: 'instant',
    costResources: 'low',
    normalizationClass: NORM_CLASS,
    normalizationRationale:
      'Distinct manage_tools disable_category target. The core category is protected and cannot be disabled; protected tools (manage_tools, inspect) remain enabled and are returned in the protected array.',
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
    parentTool: PARENT,
    action: 'reset',
    dispatchMode: 'local',
    domain: DOMAIN,
    family: 'reset',
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
    costLatency: 'instant',
    costResources: 'low',
    normalizationClass: NORM_CLASS,
    normalizationRationale:
      'Distinct manage_tools reset target; no shared canonical across tools.',
    exampleInput: { action: 'reset' },
    exampleOutput: {
      success: true,
      message: 'Reset complete. 0 tools re-enabled.',
      enabled: 0,
    },
  },
];
