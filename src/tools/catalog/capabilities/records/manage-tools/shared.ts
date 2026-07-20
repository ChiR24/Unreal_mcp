/**
 * Shared constants for manage_tools capability-record specs.
 *
 * Extracted to avoid duplication across the read/write record shards. All 8
 * actions share the same parent tool, domain, and normalization class.
 */
import type { JsonObject } from '../../index.js';

export const PARENT = 'manage_tools';
export const DOMAIN = 'tools';
export const NORM_CLASS = 'C_SAME_VERB_DIFFERENT_TARGET' as const;

export const CATEGORY_ENUM: JsonObject = {
  type: 'string',
  enum: ['core', 'world', 'gameplay', 'utility', 'all'],
  description: 'Category name to enable/disable.',
};

export const TOOLS_ARRAY: JsonObject = {
  type: 'array',
  items: { type: 'string' },
  description: 'Tool names to enable or disable.',
};
