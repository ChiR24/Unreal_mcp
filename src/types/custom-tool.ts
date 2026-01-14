import { z } from 'zod';

/**
 * Schema for custom tool input validation
 */
export const CustomToolSchema = z.object({
  name: z.string().min(1).max(64).regex(/^[a-z][a-z0-9_]*$/, 'Tool name must be lowercase alphanumeric with underscores'),
  description: z.string().min(10).max(500),
  inputSchema: z.record(z.string(), z.unknown()),
  category: z.enum(['core', 'world', 'authoring', 'gameplay', 'utility']).optional(),
});

export type CustomToolDefinition = z.infer<typeof CustomToolSchema>;

/**
 * Security constraints for custom tools
 */
export const CUSTOM_TOOL_CONSTRAINTS = {
  maxTools: 50,
  forbiddenPatterns: [
    /eval\s*\(/,
    /Function\s*\(/,
    /new\s+Function/,
    /__proto__/,
  ],
  reservedNames: ['manage_', 'control_', 'build_', 'animation_'],
} as const;
