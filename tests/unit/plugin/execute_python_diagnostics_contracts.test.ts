import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const pluginSourceRoot = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge',
);
const pythonHandlerPath = resolve(
  pluginSourceRoot,
  'Private/Domains/SystemControl/McpAutomationBridge_SystemControlHandlersPython.cpp',
);
const messagesPath = resolve(
  pluginSourceRoot,
  'Private/Transport/Connection/McpConnectionManagerMessages.cpp',
);
const connectionPrivatePath = resolve(
  pluginSourceRoot,
  'Private/Transport/Connection/McpConnectionManagerPrivate.h',
);

describe('execute_python diagnostics contracts (issue #525)', () => {
  it('logs execution metadata before ExecPythonCommandEx without raw source', () => {
    // Given
    const source = readFileSync(pythonHandlerPath, 'utf8');

    // When
    const logIdx = source.indexOf('execute_python begin:');
    const execIdx = source.indexOf('ExecPythonCommandEx(');
    const logRegion = source.slice(logIdx, execIdx);

    // Then - the begin log must precede native execution.
    expect(logIdx).toBeGreaterThan(-1);
    expect(execIdx).toBeGreaterThan(-1);
    expect(logIdx).toBeLessThan(execIdx);

    // Then - all required diagnostic fields are present in the log region.
    expect(logRegion).toContain('executionId=');
    expect(logRegion).toContain('requestId=');
    expect(logRegion).toContain('origin=');
    expect(logRegion).toContain('codeSha256=');
    expect(logRegion).toContain('codePath=');
    expect(logRegion).toContain('wrapperPath=');

    // Then - raw Python source variables are never passed as log arguments.
    expect(logRegion).not.toContain('*Code,');
    expect(logRegion).not.toContain('*File');
  });

  it('computes SHA-256 of the code bytes through the shared OpenSSL helper', () => {
    // Given
    const source = readFileSync(pythonHandlerPath, 'utf8');

    // Then - the code BYTES are hashed (not the raw string), via McpSha256Hex.
    expect(source).toContain('McpSha256Hex(CodeBytes.GetData(), CodeBytes.Num())');
  });

  it('surfaces executionId and codeSha256 in the structured response', () => {
    // Given
    const source = readFileSync(pythonHandlerPath, 'utf8');

    // Then - both fields are written to the response JSON object.
    expect(source).toContain('SetStringField(TEXT("executionId"), SafeId)');
    expect(source).toContain('SetStringField(TEXT("codeSha256"), CodeSha256)');
  });

  it('redacts the code field in the generic request log', () => {
    // Given
    const source = readFileSync(messagesPath, 'utf8');
    const preview = readFileSync(connectionPrivatePath, 'utf8');

    // Then - the request log previews the payload in request mode, where the
    // `code` field is replaced with <redacted> so raw Python source never
    // enters the bridge request log.
    expect(source).toMatch(/PreviewJsonFields\(Payload, \d+, true\)/);
    expect(preview).toContain('bRequest && Key == TEXT("code")');
    expect(preview).toContain('TEXT("<redacted>")');
  });

  it('preserves the temp-file cleanup RAII and MCP_Python temp directory', () => {
    // Given
    const source = readFileSync(pythonHandlerPath, 'utf8');

    // Then - the RAII cleanup and temp directory path are unchanged.
    expect(source).toContain('FPythonTempFileCleanup Cleanup');
    expect(source).toContain('Temp/MCP_Python');
    expect(source).toContain('Cleanup.Paths.Add(ScriptPath)');
  });
});
