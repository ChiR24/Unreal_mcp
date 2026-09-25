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

const consoleCommand = readFileSync(
  resolve(
    process.cwd(),
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ConsoleCommand/McpAutomationBridge_ConsoleCommandHandlers.cpp',
  ),
  'utf8',
);

describe('console_command output redaction contracts', () => {
  it('runs captured output and log text through the per-line engine sanitizer', () => {
    expect(consoleCommand).toContain('#include "Core/Subsystem/McpAutomationBridgeSubsystemResponseSanitization.h"');
    expect(consoleCommand).toContain('Line = McpAutomationBridgeSubsystemResponse::SanitizeEngineErrorForResponse(Line);');
    expect(consoleCommand).toContain('const FString BoundedOutput = SanitizeLines(BoundText(');
    expect(consoleCommand).toMatch(/const FString BoundedLog =\s+SanitizeLines\(BoundText\(/);
  });
});

const launchBuild = readFileSync(
  resolve(
    process.cwd(),
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/SystemControl/McpAutomationBridge_SystemControlHandlersLaunchBuild.cpp',
  ),
  'utf8',
);

describe('launch_build log tail redaction contracts', () => {
  it('runs the packaged game log tail through the same sanitizer read_log uses', () => {
    expect(launchBuild).toContain('#include "Core/Subsystem/McpAutomationBridgeSubsystemResponseSanitization.h"');
    expect(launchBuild).toMatch(/Tail\.Add\(MakeShared<FJsonValueString>\(McpAutomationBridgeSubsystemResponse::SanitizeEngineErrorForResponse\(\s*FMcpLogHistory::KeepDiagnosticFileName\(Lines\[Index\]\)\)\)\);/);
  });
});
