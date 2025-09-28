import JSON5 from 'json5';

export interface PythonOutput {
  raw: unknown;
  text: string;
}

export interface TaggedJsonResult<T> extends PythonOutput {
  data: T | null;
}

export interface StandardResultPayload {
  success?: boolean;
  message?: string;
  error?: string;
  [key: string]: unknown;
}

function escapeRegExp(value: string): string {
  return value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function stringifyAny(value: unknown): string {
  if (typeof value === 'string') {
    return value;
  }
  try {
    return JSON.stringify(value ?? '');
  } catch {
    return String(value);
  }
}

function isTaggedResultLine(line: unknown, tag = 'RESULT:'): boolean {
  if (typeof line !== 'string') {
    return false;
  }
  return line.includes(tag);
}

function collectLogOutput(logs: unknown): string | undefined {
  if (!Array.isArray(logs)) return undefined;
  let buffer = '';
  for (const entry of logs) {
    if (!entry) continue;
    if (typeof entry === 'string') {
      buffer += entry.endsWith('\n') ? entry : `${entry}\n`;
      continue;
    }
    const output = (entry as any).Output ?? (entry as any).output ?? (entry as any).Message;
    if (typeof output === 'string') {
      buffer += output.endsWith('\n') ? output : `${output}\n`;
    }
  }
  return buffer || undefined;
}

export function toPythonOutput(response: unknown): PythonOutput {
  if (typeof response === 'string') {
    return { raw: response, text: response };
  }

  if (response == null) {
    return { raw: response, text: '' };
  }

  if (Array.isArray(response)) {
    const lines = response.map(item => stringifyAny(item));
    return { raw: response, text: lines.join('\n') };
  }

  if (typeof response === 'object') {
    const obj = response as Record<string, unknown>;

    const logOutput = collectLogOutput(obj.LogOutput ?? obj.logOutput ?? obj.logs);
    if (logOutput) {
      return { raw: response, text: logOutput };
    }

    if (typeof obj.result === 'string') {
      return { raw: response, text: obj.result };
    }

    if (typeof obj.ReturnValue === 'string') {
      return { raw: response, text: obj.ReturnValue };
    }

    if (obj.result !== undefined) {
      return { raw: response, text: stringifyAny(obj.result) };
    }

    if (obj.ReturnValue !== undefined) {
      return { raw: response, text: stringifyAny(obj.ReturnValue) };
    }

    return { raw: response, text: stringifyAny(obj) };
  }

  return { raw: response, text: String(response) };
}

function tryParseJson<T>(candidate: string): T | null {
  try {
    return JSON.parse(candidate) as T;
  } catch {
    try {
      return JSON5.parse(candidate) as T;
    } catch {
      try {
        const normalized = normalizeLegacyJson(candidate);
        return JSON5.parse(normalized) as T;
      } catch {
        return null;
      }
    }
  }
}

function normalizeLegacyJson(candidate: string): string {
  return candidate
    .replace(/\bTrue\b/g, 'true')
    .replace(/\bFalse\b/g, 'false')
    .replace(/\bNone\b/g, 'null');
}

function findJsonBlock(text: string, startIndex: number): string | null {
  const length = text.length;
  let index = startIndex;

  while (index < length) {
    const currentChar = text[index];
    if (!currentChar || !/\s/.test(currentChar)) {
      break;
    }
    index += 1;
  }

  if (index >= length) {
    return null;
  }

  const opening = text[index];
  if (!opening) {
    return null;
  }
  let closing: string | null = null;
  if (opening === '{') {
    closing = '}';
  } else if (opening === '[') {
    closing = ']';
  }

  if (!closing) {
    return null;
  }

  let depth = 0;
  let inString = false;
  let stringQuote: string | null = null;
  let escaped = false;

  for (let i = index; i < length; i++) {
    const char = text[i];
    if (!char) {
      break;
    }

    if (inString) {
      if (escaped) {
        escaped = false;
        continue;
      }

      if (char === '\\') {
        escaped = true;
        continue;
      }

      if (char === stringQuote) {
        inString = false;
        stringQuote = null;
      }
      continue;
    }

    if (char === '"' || char === '\'') {
      inString = true;
      stringQuote = char;
      escaped = false;
      continue;
    }

    if (char === opening) {
      depth += 1;
    } else if (char === closing) {
      depth -= 1;
      if (depth === 0) {
        return text.slice(index, i + 1);
      }
    }
  }

  return null;
}

function extractTaggedJson<T = unknown>(response: unknown, tag = 'RESULT:'): TaggedJsonResult<T> {
  const output = toPythonOutput(response);
  const pattern = escapeRegExp(tag);

  let data: T | null = null;

  const directMatch = output.text.match(new RegExp(`${pattern}\\s*(.+)$`, 'm'));
  if (directMatch) {
    const parsed = tryParseJson<T>(directMatch[1]);
    if (parsed !== null) {
      data = parsed;
    }
  }

  if (data === null) {
    const tagIndex = output.text.indexOf(tag);
    if (tagIndex >= 0) {
      const jsonBlock = findJsonBlock(output.text, tagIndex + tag.length);
      if (jsonBlock) {
        const parsed = tryParseJson<T>(jsonBlock);
        if (parsed !== null) {
          data = parsed;
        }
      }
    }
  }

  const sanitizedText = stripTaggedResultLines(output.text, tag);
  const sanitizedRaw = sanitizePythonRaw(output.raw, tag);
  return { raw: sanitizedRaw, text: sanitizedText, data };
}

export function parseStandardResult(response: unknown, tag = 'RESULT:'): TaggedJsonResult<StandardResultPayload> {
  return extractTaggedJson<StandardResultPayload>(response, tag);
}

export function stripTaggedResultLines(text: string, tag = 'RESULT:'): string {
  if (!text || !text.includes(tag)) {
    return text;
  }

  const lines = text.split(/\r?\n/);
  const cleaned = lines.filter(line => !isTaggedResultLine(line, tag));
  const collapsed = cleaned.join('\n');
  return collapsed.replace(/\n{3,}/g, '\n\n');
}

export function extractTaggedLine(output: string | PythonOutput, prefix: string): string | null {
  const text = typeof output === 'string' ? output : output.text;
  const pattern = new RegExp(`^${escapeRegExp(prefix)}\\s*(.+)$`, 'm');
  const match = text.match(pattern);
  return match ? match[1].trim() : null;
}

function sanitizePythonRaw(raw: unknown, tag = 'RESULT:'): unknown {
  if (raw == null) {
    return raw;
  }

  if (typeof raw === 'string') {
    return stripTaggedResultLines(raw, tag);
  }

  if (Array.isArray(raw)) {
    const sanitized = raw
      .map(item => sanitizePythonRaw(item, tag))
      .filter(item => {
        if (typeof item === 'string') {
          return item.length > 0 && !isTaggedResultLine(item, tag);
        }
        return item !== undefined;
      });
    return sanitized;
  }

  if (typeof raw === 'object') {
    const obj = raw as Record<string, unknown>;
    let mutated = false;
    const clone: Record<string, unknown> = { ...obj };

    const sanitizeLogArray = (value: unknown[]): unknown[] => {
      const filtered = value.filter(entry => {
        if (entry == null) {
          return false;
        }
        if (typeof entry === 'string') {
          return !isTaggedResultLine(entry, tag);
        }
        const output = (entry as any).Output ?? (entry as any).output ?? (entry as any).Message;
        if (typeof output === 'string' && isTaggedResultLine(output, tag)) {
          return false;
        }
        return true;
      });
      return filtered;
    };

    if (Array.isArray(obj.LogOutput)) {
      const originalArray = obj.LogOutput as unknown[];
      const sanitizedLog = sanitizeLogArray(originalArray);
      if (
        sanitizedLog.length !== originalArray.length ||
        sanitizedLog.some((entry: unknown, idx: number) => entry !== originalArray[idx])
      ) {
        clone.LogOutput = sanitizedLog;
        mutated = true;
      }
    }

    if (Array.isArray(obj.logs)) {
      const originalLogs = obj.logs as unknown[];
      const sanitizedLogs = sanitizeLogArray(originalLogs);
      if (
        sanitizedLogs.length !== originalLogs.length ||
        sanitizedLogs.some((entry: unknown, idx: number) => entry !== originalLogs[idx])
      ) {
        clone.logs = sanitizedLogs;
        mutated = true;
      }
    }

    if (typeof obj.Output === 'string') {
      const stripped = stripTaggedResultLines(obj.Output, tag);
      if (stripped !== obj.Output) {
        clone.Output = stripped;
        mutated = true;
      }
    }

    if (typeof obj.result === 'string') {
      const strippedResult = stripTaggedResultLines(obj.result, tag);
      if (strippedResult !== obj.result) {
        clone.result = strippedResult;
        mutated = true;
      }
    }

    return mutated ? clone : raw;
  }

  return raw;
}