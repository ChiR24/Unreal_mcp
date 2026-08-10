/// <reference types="node" />

import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const pluginRoot = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge',
);
const repoRoot = resolve(process.cwd());

const readFilterRules = (): readonly string[] =>
  readFileSync(resolve(pluginRoot, 'Config/FilterPlugin.ini'), 'utf8')
    .split(/\r?\n/u)
    .map((line) => line.trim())
    .filter(
      (line) =>
        line.length > 0 &&
        !line.startsWith(';') &&
        line !== '[FilterPlugin]',
    );

describe('plugin packaging contracts', () => {
  it('packages the public plugin documentation and license', () => {
    // Given
    const rules = readFilterRules();

    // When
    const documentationRules = ['/README.md', '/CHANGELOG.md', '/LICENSE'];

    // Then
    expect(rules).toEqual(expect.arrayContaining(documentationRules));
  });

  it('excludes source-scoped agent instructions with FileFilter syntax', () => {
    // Given
    const rules = readFilterRules();

    // When
    const sourceAgentRule = rules.find((rule) => rule.includes('AGENTS.md'));

    // Then
    expect(sourceAgentRule).toBe('-/Source/.../AGENTS.md');
  });

  it('retains an empty Content directory in packaged submissions', () => {
    // Given
    const rules = readFilterRules();
    const placeholder = resolve(pluginRoot, 'Content/.keep');

    // When
    const contentRule = rules.find((rule) => rule === '/Content/...');

    // Then
    expect(contentRule).toBe('/Content/...');
    expect(existsSync(placeholder)).toBe(true);
  });

  it('builds versioned packages from a staged descriptor with PCG enabled', () => {
    const shellScript = readFileSync(
      resolve(repoRoot, 'scripts/package-plugin.sh'),
      'utf8',
    );
    const batchScript = readFileSync(
      resolve(repoRoot, 'scripts/package-plugin.bat'),
      'utf8',
    );

    expect(shellScript).toContain('SOURCE_PLUGIN_FILE=');
    expect(shellScript).toContain("dependency.pop('Optional', None)");
    expect(shellScript).toContain('-Plugin="$SOURCE_PLUGIN_FILE"');
    expect(batchScript).toContain('SOURCE_PLUGIN_FILE=');
    expect(batchScript).toContain("Properties.Remove('Optional')");
    expect(batchScript).toContain('-Plugin="%SOURCE_PLUGIN_FILE%"');
  });

  it('excludes platform debug symbols from distribution archives', () => {
    const shellScript = readFileSync(
      resolve(repoRoot, 'scripts/package-plugin.sh'),
      'utf8',
    );
    const batchScript = readFileSync(
      resolve(repoRoot, 'scripts/package-plugin.bat'),
      'utf8',
    );

    for (const extension of ['.pdb', '.debug', '.sym']) {
      expect(shellScript).toContain(`-x "*${extension}"`);
      expect(batchScript).toContain(`'${extension}'`);
    }
    expect(shellScript).toContain('-iname "*.dSYM"');
    expect(shellScript).toContain('-x "*.dSYM/*"');
    expect(shellScript).toContain("part.endswith('.dsym')");
    expect(batchScript).toContain("-Filter '*.dSYM'");
    expect(batchScript).toContain(".Contains('.dsym/')");
    expect(shellScript).toContain('rm -f "$ZIP_PATH"');
    expect(shellScript).toContain('find "$OUTPUT_PLUGIN_DIR" -type f');
    expect(shellScript).toContain('zipfile.ZipFile(archive_path)');
    expect(shellScript).toContain(
      'name.lower().endswith(forbidden_extensions)',
    );
    expect(batchScript).toContain(
      '[System.IO.Compression.ZipFile]::OpenRead($env:MCP_PKG_ZIP)',
    );
    // `powershell -Command` appends trailing tokens to the command TEXT instead
    // of binding them to $args, so the earlier `OpenRead($args[0]) "%ZIP_PATH%"`
    // form passed $null and threw "Empty path name is not legal" on every run --
    // the verification step failed for a reason unrelated to what it verifies.
    // Every helper reads its path from the environment instead, so no argument
    // indexing may return (the prose REM explaining this is expected to stay).
    expect(batchScript).not.toContain('$args[');
    expect(batchScript).toContain('Archive verification failed.');
  });

  it('links PCG only when the project or staged descriptor enables it', () => {
    const buildRules = readFileSync(
      resolve(
        pluginRoot,
        'Source/McpAutomationBridge/McpAutomationBridge.Build.cs',
      ),
      'utf8',
    );

    expect(buildRules).toContain('ProjectDescriptor.FromFile(Target.ProjectFile)');
    expect(buildRules).toContain('PluginDescriptor.FromFile');
    expect(buildRules).toContain('Reference.bEnabled && !Reference.bOptional');
    expect(buildRules).toContain('&& AddOptionalDynamicModule(Target, EngineDir, "PCG", "PCG")');
  });
});
