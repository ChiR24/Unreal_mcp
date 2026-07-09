import { Logger } from '../utils/logging/logger.js';

export const REDACTED_AUTOMATION_CREDENTIAL = '[REDACTED]';

const SENSITIVE_KEY_NAMES = new Set(['sessionid', 'capabilitytoken', 'xmcpcapability', 'xmcpcapabilitytoken']);
const SENSITIVE_TEXT_PATTERN =
    /((?:"|')?(?:session[\s_-]*id|capability[\s_-]*token|x-mcp-capability(?:-token)?)(?:"|')?\s*[:=]\s*)(?:"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[^\s,}\]]+)/gi;
const SENSITIVE_TEXT_VALUE_PATTERN =
    /(?:"|')?(?:session[\s_-]*id|capability[\s_-]*token|x-mcp-capability(?:-token)?)(?:"|')?\s*[:=]\s*(?:"((?:\\.|[^"\\])*)"|'((?:\\.|[^'\\])*)'|([^\s,}\]]+))/gi;
const MAX_REDACTION_DEPTH = 12;

function isSensitiveKey(key: string): boolean {
    const normalized = key.toLowerCase().replace(/[^a-z0-9]/g, '');
    return SENSITIVE_KEY_NAMES.has(normalized);
}

function addSensitiveValue(values: Set<string>, value: unknown): void {
    if (typeof value === 'string' && value.length > 0 && value !== REDACTED_AUTOMATION_CREDENTIAL) {
        values.add(value);
    }
}

function collectSensitiveTextValues(text: string, values: Set<string>): void {
    for (const match of text.matchAll(SENSITIVE_TEXT_VALUE_PATTERN)) {
        addSensitiveValue(values, match[1] ?? match[2] ?? match[3]);
    }
}

function collectSensitiveValuesInternal(value: unknown, values: Set<string>, seen: WeakSet<object>, depth: number): void {
    if (typeof value === 'string') {
        collectSensitiveTextValues(value, values);
        return;
    }
    if (value === null || typeof value !== 'object' || depth >= MAX_REDACTION_DEPTH || seen.has(value)) {
        return;
    }

    seen.add(value);
    if (value instanceof Error) {
        collectSensitiveTextValues(value.message, values);
        if (value.stack) {
            collectSensitiveTextValues(value.stack, values);
        }
        collectSensitiveValuesInternal(value.cause, values, seen, depth + 1);
        seen.delete(value);
        return;
    }
    if (Array.isArray(value)) {
        for (const item of value) {
            collectSensitiveValuesInternal(item, values, seen, depth + 1);
        }
        seen.delete(value);
        return;
    }

    for (const [key, descriptor] of Object.entries(Object.getOwnPropertyDescriptors(value))) {
        if (!('value' in descriptor)) {
            continue;
        }
        if (isSensitiveKey(key)) {
            addSensitiveValue(values, descriptor.value);
        }
        collectSensitiveValuesInternal(descriptor.value, values, seen, depth + 1);
    }
    seen.delete(value);
}

function collectSensitiveValues(values: readonly unknown[], knownCredentials: readonly string[] = []): readonly string[] {
    const sensitiveValues = new Set<string>();
    for (const credential of knownCredentials) {
        addSensitiveValue(sensitiveValues, credential);
    }
    for (const value of values) {
        collectSensitiveValuesInternal(value, sensitiveValues, new WeakSet<object>(), 0);
    }
    return [...sensitiveValues].sort((left, right) => right.length - left.length);
}

function redactSensitiveText(text: string, sensitiveValues: readonly string[]): string {
    let redacted = text.replace(
        SENSITIVE_TEXT_PATTERN,
        (_match: string, prefix: string) => `${prefix}"${REDACTED_AUTOMATION_CREDENTIAL}"`
    );
    for (const sensitiveValue of sensitiveValues) {
        // F6 fix: also redact case-insensitive matches. Hex/base64 secrets
        // are case-sensitive by design, but logs may use different case
        // (e.g. tool output may uppercase or title-case the value). The
        // substring split-join handles exact case; the case-insensitive
        // regex covers the rest. We skip empty strings to avoid matching
        // every character position.
        if (sensitiveValue.length > 0) {
            const escaped = sensitiveValue.replace(/[.*+?^${}()|[\]\\]/gu, '\\$&');
            const caseInsensitivePattern = new RegExp(escaped, 'giu');
            redacted = redacted.replace(caseInsensitivePattern, REDACTED_AUTOMATION_CREDENTIAL);
        }
        redacted = redacted.split(sensitiveValue).join(REDACTED_AUTOMATION_CREDENTIAL);
    }
    return redacted;
}

function redactObject(
    value: object,
    seen: WeakSet<object>,
    depth: number,
    sensitiveValues: readonly string[],
    redactSensitiveKeys: boolean
): Record<string, unknown> | string {
    if (seen.has(value)) {
        return '[Circular]';
    }
    if (depth >= MAX_REDACTION_DEPTH) {
        return '[Object]';
    }

    seen.add(value);
    const redacted: Record<string, unknown> = {};
    const descriptors = Object.getOwnPropertyDescriptors(value);
    for (const [key, descriptor] of Object.entries(descriptors)) {
        if (!('value' in descriptor)) {
            continue;
        }
        redacted[key] = redactSensitiveKeys && isSensitiveKey(key)
            ? REDACTED_AUTOMATION_CREDENTIAL
            : redactAutomationLogValueInternal(descriptor.value, seen, depth + 1, sensitiveValues, redactSensitiveKeys);
    }
    seen.delete(value);
    return redacted;
}

function redactAutomationLogValueInternal(
    value: unknown,
    seen: WeakSet<object>,
    depth: number,
    sensitiveValues: readonly string[],
    redactSensitiveKeys: boolean
): unknown {
    if (typeof value === 'string') {
        return redactSensitiveText(value, sensitiveValues);
    }
    if (
        value === null ||
        typeof value === 'number' ||
        typeof value === 'boolean' ||
        typeof value === 'bigint' ||
        typeof value === 'undefined'
    ) {
        return value;
    }
    if (typeof value === 'symbol' || typeof value === 'function') {
        return String(value);
    }
    if (value instanceof Date) {
        return value.toISOString();
    }
    if (Buffer.isBuffer(value)) {
        return `<binary; ${value.byteLength} bytes>`;
    }
    if (value instanceof Error) {
        return {
            name: value.name,
            message: redactSensitiveText(value.message, sensitiveValues),
            stack: value.stack ? redactSensitiveText(value.stack, sensitiveValues) : undefined,
            cause: redactAutomationLogValueInternal(value.cause, seen, depth + 1, sensitiveValues, redactSensitiveKeys)
        };
    }
    if (Array.isArray(value)) {
        return value.map((item) => redactAutomationLogValueInternal(item, seen, depth + 1, sensitiveValues, redactSensitiveKeys));
    }
    return redactObject(value, seen, depth, sensitiveValues, redactSensitiveKeys);
}

export function redactAutomationLogValue(value: unknown): unknown {
    const sensitiveValues = collectSensitiveValues([value]);
    return redactAutomationLogValueInternal(value, new WeakSet<object>(), 0, sensitiveValues, true);
}

export function redactKnownAutomationCredentials(value: unknown, knownCredentials: readonly string[]): unknown {
    const sensitiveValues = collectSensitiveValues([value], knownCredentials);
    return redactAutomationLogValueInternal(value, new WeakSet<object>(), 0, sensitiveValues, false);
}

export function redactAutomationLogRecord(value: Readonly<Record<string, unknown>>): Record<string, unknown> {
    const sensitiveValues = collectSensitiveValues([value]);
    const redacted: Record<string, unknown> = {};
    for (const [key, child] of Object.entries(value)) {
        redacted[key] = isSensitiveKey(key)
            ? REDACTED_AUTOMATION_CREDENTIAL
            : redactAutomationLogValueInternal(child, new WeakSet<object>(), 1, sensitiveValues, true);
    }
    return redacted;
}

function redactAutomationLogArguments(args: readonly unknown[]): unknown[] {
    const sensitiveValues = collectSensitiveValues(args);
    return args.map((argument) => redactAutomationLogValueInternal(argument, new WeakSet<object>(), 0, sensitiveValues, true));
}

export class AutomationLogger extends Logger {
    override debug(...args: unknown[]): void {
        super.debug(...redactAutomationLogArguments(args));
    }

    override info(...args: unknown[]): void {
        super.info(...redactAutomationLogArguments(args));
    }

    override warn(...args: unknown[]): void {
        super.warn(...redactAutomationLogArguments(args));
    }

    override error(...args: unknown[]): void {
        super.error(...redactAutomationLogArguments(args));
    }
}
