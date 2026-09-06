import { z } from 'zod';

/**
 * Any JSON-safe value: the boundary every receipt payload, error detail and
 * canonical output is checked against before it crosses a transport.
 */
export const JsonValueSchema = z.json();
