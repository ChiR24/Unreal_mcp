import { consolidatedToolDefinitions } from '../tools/catalog/consolidated-tool-definitions.js';
import type { PrimitiveSchema } from '../utils/interaction/elicitation.js';
import type { Logger } from '../utils/logging/logger.js';
import { isRecord } from '../utils/validation/type-guards.js';

// Task 35 — safe elicitation policy. Two rules the generic prefill must obey:
// (1) NEVER solicit a secret/token/credential field. (2) NEVER solicit a
// destructive confirmation value; high-impact consent is a separate explicit
// yes/no (see elicitHighImpactConsent), never the destructive value itself.
const SECRET_FIELD = /token|secret|password|passwd|credential|api[-_]?key|private[-_]?key|bearer|authorization|access[-_]?key/i;
const DESTRUCTIVE_FIELD = /confirm|force|delete|destroy|purge|wipe|overwrite|drop/i;

// A field is safe to elicit only when its name implies neither a secret nor a
// destructive confirmation. Applied to every generic prefill candidate.
export function isSafeToElicit(fieldName: string): boolean {
    return !SECRET_FIELD.test(fieldName) && !DESTRUCTIVE_FIELD.test(fieldName);
}

type ElicitFunction = (
    message: string,
    schema: { type: 'object'; properties: Record<string, PrimitiveSchema>; required: string[] },
    options: Record<string, unknown>
) => Promise<{ ok?: boolean; value?: Record<string, unknown> } | undefined>;

export async function maybeElicitMissingArgs(
    name: string,
    args: Record<string, unknown>,
    elicitFn: unknown,
    timeoutMs: number,
    logger: Logger
): Promise<Record<string, unknown>> {
    try {
        if (typeof elicitFn !== 'function') return args;
        const toolDef = (consolidatedToolDefinitions as Array<Record<string, unknown>>).find(tool => tool.name === name) as Record<string, unknown> | undefined;
        const inputSchema = toolDef?.inputSchema as Record<string, unknown> | undefined;
        if (!inputSchema) return args;

        const primitiveProps = collectSafeElicitableProps(inputSchema, args);
        if (Object.keys(primitiveProps).length === 0) return args;

        const elicitOptions: Record<string, unknown> = { fallback: async () => ({ ok: false, error: 'missing-params' }) };
        if (Number.isFinite(timeoutMs)) {
            elicitOptions.timeoutMs = timeoutMs;
        }
        const elicitRes = await (elicitFn as ElicitFunction)(
            `Provide missing parameters for ${name}`,
            { type: 'object', properties: primitiveProps, required: Object.keys(primitiveProps) },
            elicitOptions
        );
        if (elicitRes && elicitRes.ok && elicitRes.value) {
            return { ...args, ...elicitRes.value };
        }
    } catch (e) {
        const message = e instanceof Error ? e.message : String(e);
        logger.debug('Generic elicitation prefill skipped', { err: message });
    }

    return args;
}

export function collectSafeElicitableProps(inputSchema: Record<string, unknown>, args: Record<string, unknown>): Record<string, PrimitiveSchema> {
    const props = (inputSchema.properties || {}) as Record<string, Record<string, unknown>>;
    const required: string[] = Array.isArray(inputSchema.required) ? inputSchema.required as string[] : [];
    const missing = required.filter((key: string) => {
        const value = args[key];
        if (value === undefined || value === null) return true;
        if (typeof value === 'string' && value.trim() === '') return true;
        return false;
    }).filter(isSafeToElicit);

    const primitiveProps: Record<string, PrimitiveSchema> = {};
    for (const key of missing) {
        const property = props[key];
        if (!property || typeof property !== 'object') continue;
        const type = (property.type || '').toString();
        const isEnum = Array.isArray(property.enum);
        if (type === 'string' || type === 'number' || type === 'integer' || type === 'boolean' || isEnum) {
            const schemaType = (isEnum ? 'string' : type) as 'string' | 'number' | 'integer' | 'boolean';
            primitiveProps[key] = {
                type: schemaType,
                title: typeof property.title === 'string' ? property.title : undefined,
                description: typeof property.description === 'string' ? property.description : undefined,
                enum: Array.isArray(property.enum) ? (property.enum as string[]) : undefined,
                enumNames: Array.isArray(property.enumNames) ? (property.enumNames as string[]) : undefined,
                minimum: typeof property.minimum === 'number' ? property.minimum : undefined,
                maximum: typeof property.maximum === 'number' ? property.maximum : undefined,
                minLength: typeof property.minLength === 'number' ? property.minLength : undefined,
                maxLength: typeof property.maxLength === 'number' ? property.maxLength : undefined,
                pattern: typeof property.pattern === 'string' ? property.pattern : undefined,
                format: typeof property.format === 'string' ? (property.format as 'email' | 'uri' | 'date' | 'date-time') : undefined,
                default: (typeof property.default === 'string' || typeof property.default === 'number' || typeof property.default === 'boolean') ? property.default : undefined
            } as PrimitiveSchema;
        }
    }
    return primitiveProps;
}

// The decision returned by a high-impact consent request. `unsupported` means
// the client cannot elicit at all; `declined` means it could but consent was
// not granted. Both keep the operation BLOCKED — consent is never assumed.
export type ConsentDecision = { readonly granted: boolean; readonly reason: 'granted' | 'declined' | 'unsupported' };

type ConsentLogger = { debug: (message: string, meta?: Record<string, unknown>) => void };

// Ask for explicit consent before a high-impact (destructive/irreversible)
// operation. Solicited ONLY when the client structurally supports elicitation
// (never inferred from loopback/idempotency) and asks for a single boolean —
// never a secret and never the destructive value itself. Anything but an
// explicit `consent: true` leaves the operation blocked.
export async function elicitHighImpactConsent(
    operationLabel: string,
    profile: { readonly hasElicitation: boolean },
    elicitFn: unknown,
    timeoutMs: number,
    logger: ConsentLogger
): Promise<ConsentDecision> {
    if (!profile.hasElicitation || typeof elicitFn !== 'function') {
        return { granted: false, reason: 'unsupported' };
    }
    try {
        const options: Record<string, unknown> = {};
        if (Number.isFinite(timeoutMs)) options.timeoutMs = timeoutMs;
        const res = await (elicitFn as ElicitFunction)(
            `Confirm high-impact operation: ${operationLabel}. This cannot be undone and will not proceed without your explicit consent.`,
            { type: 'object', properties: { consent: { type: 'boolean', title: 'Proceed?', description: 'Explicitly confirm this high-impact operation.' } }, required: ['consent'] },
            options
        );
        if (res && res.ok === true && isRecord(res.value) && res.value.consent === true) {
            return { granted: true, reason: 'granted' };
        }
        return { granted: false, reason: 'declined' };
    } catch (e) {
        logger.debug('High-impact consent elicitation treated as declined', { err: e instanceof Error ? e.message : String(e) });
        return { granted: false, reason: 'declined' };
    }
}
