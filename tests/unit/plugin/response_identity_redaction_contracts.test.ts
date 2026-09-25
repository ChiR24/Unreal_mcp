/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const sanitization = readFileSync(
  resolve(
    process.cwd(),
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Core/Subsystem/McpAutomationBridgeSubsystemResponseSanitization.h',
  ),
  'utf8',
);

describe('response identity redaction contracts', () => {
  it('redacts identity keys however the editor spaces them before a response leaves', () => {
    const sanitize = sanitization.slice(sanitization.indexOf('inline FString SanitizeEngineErrorForResponse'));

    expect(sanitize).toContain('{TEXT("userid"), TEXT("accountid"), TEXT("loginid")}');
    expect(sanitize).toContain('RedactKeyedValueForResponse(Out, Key);');
    // Spaces on either side of '=' or ':' must not let the value through.
    expect(sanitization).toMatch(/while \(Start < Text\.Len\(\) && Text\[Start\] == ' '\) \+\+Start;/);
    expect(sanitization).toContain("(Text[Start] != '=' && Text[Start] != ':')");
  });
});
