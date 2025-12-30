/**
 * Safe parsing utilities for Zod schemas.
 * Provides runtime validation with informative error messages.
 */
import { z, ZodSchema, ZodError } from 'zod';

/**
 * Parse input with a Zod schema, throwing a descriptive error on failure.
 * 
 * @param schema - Zod schema to validate against
 * @param input - Unknown input to validate
 * @param context - Context string for error messages (e.g., 'control_actor.spawn')
 * @returns Validated and typed data
 * @throws Error with formatted validation message
 * 
 * @example
 * ```typescript
 * const params = parseOrThrow(SpawnActorParamsSchema, args, 'control_actor.spawn');
 * // params is now typed as SpawnActorParams
 * ```
 */
export function parseOrThrow<T>(schema: ZodSchema<T>, input: unknown, context: string): T {
    const result = schema.safeParse(input);
    if (result.success) {
        return result.data;
    }
    throw new Error(formatZodError(result.error, context));
}

/**
 * Parse input with a Zod schema, returning a Result-like object.
 * Does not throw - useful for conditional validation.
 * 
 * @param schema - Zod schema to validate against
 * @param input - Unknown input to validate
 * @returns Object with success flag and either data or error
 * 
 * @example
 * ```typescript
 * const result = safeParse(Vec3Schema, maybeVector);
 * if (result.success) {
 *   console.log(result.data.x);
 * } else {
 *   console.log('Invalid:', result.error);
 * }
 * ```
 */
export function safeParse<T>(schema: ZodSchema<T>, input: unknown): 
    | { success: true; data: T; error?: undefined }
    | { success: false; data?: undefined; error: string } {
    const result = schema.safeParse(input);
    if (result.success) {
        return { success: true, data: result.data };
    }
    return { success: false, error: formatZodError(result.error, 'validation') };
}

/**
 * Parse input, returning undefined on failure instead of throwing.
 * Useful for optional fields that may have invalid data.
 * 
 * @param schema - Zod schema to validate against
 * @param input - Unknown input to validate
 * @returns Validated data or undefined
 */
export function parseOrUndefined<T>(schema: ZodSchema<T>, input: unknown): T | undefined {
    const result = schema.safeParse(input);
    return result.success ? result.data : undefined;
}

/**
 * Parse input, returning a default value on failure.
 * 
 * @param schema - Zod schema to validate against
 * @param input - Unknown input to validate
 * @param defaultValue - Value to return on validation failure
 * @returns Validated data or default value
 */
export function parseOrDefault<T>(schema: ZodSchema<T>, input: unknown, defaultValue: T): T {
    const result = schema.safeParse(input);
    return result.success ? result.data : defaultValue;
}

/**
 * Format a ZodError into a human-readable string.
 * 
 * @param error - ZodError from failed parsing
 * @param context - Context string for the error message
 * @returns Formatted error message
 */
export function formatZodError(error: ZodError, context: string): string {
    const issues = error.issues.map((issue) => {
        const path = issue.path.length > 0 ? issue.path.join('.') : '(root)';
        return `  - ${path}: ${issue.message}`;
    });
    return `[${context}] Validation failed:\n${issues.join('\n')}`;
}

/**
 * Type guard to check if a value is a plain object.
 * Useful before passing to Zod object schemas.
 */
export function isPlainObject(value: unknown): value is Record<string, unknown> {
    return typeof value === 'object' && value !== null && !Array.isArray(value);
}

/**
 * Coerce a value to a plain object, returning empty object if not possible.
 * Useful for handling cases where args might be undefined/null.
 */
export function toPlainObject(value: unknown): Record<string, unknown> {
    if (isPlainObject(value)) {
        return value;
    }
    return {};
}

/**
 * Parse arguments with passthrough for extra fields.
 * This is useful when you want to validate known fields but preserve unknown ones.
 * 
 * @param schema - Zod object schema (will apply .passthrough())
 * @param input - Unknown input to validate
 * @param context - Context string for error messages
 * @returns Validated data with extra fields preserved
 */
export function parseWithPassthrough<T extends z.ZodRawShape>(
    schema: z.ZodObject<T>,
    input: unknown,
    context: string
): z.infer<z.ZodObject<T>> & Record<string, unknown> {
    const passthroughSchema = schema.passthrough();
    return parseOrThrow(passthroughSchema, input, context);
}

/**
 * Validate that a required field exists and is non-empty.
 * Throws if the field is missing, null, undefined, or empty string.
 * 
 * @param value - Value to check
 * @param fieldName - Name of the field for error message
 * @param context - Context string for error message
 * @throws Error if field is missing or empty
 */
export function requireField<T>(value: T | null | undefined, fieldName: string, context: string): T {
    if (value === null || value === undefined) {
        throw new Error(`[${context}] Missing required field: ${fieldName}`);
    }
    if (typeof value === 'string' && value.trim() === '') {
        throw new Error(`[${context}] Field cannot be empty: ${fieldName}`);
    }
    return value;
}

/**
 * Get a field with a fallback value.
 * Returns the fallback if the value is null, undefined, or empty string.
 */
export function getFieldOrDefault<T>(value: T | null | undefined, defaultValue: T): T {
    if (value === null || value === undefined) {
        return defaultValue;
    }
    if (typeof value === 'string' && value.trim() === '') {
        return defaultValue;
    }
    return value;
}
