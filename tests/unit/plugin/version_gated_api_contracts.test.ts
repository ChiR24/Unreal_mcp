// Guards against the two ways a version-gated engine API silently stops
// compiling on the versions the plugin claims to support:
//
//   1. Calling FProperty::ExportTextItem_Direct directly. It exists from 5.1;
//      5.0 only has ExportText_Direct. Every call must go through the
//      MCP_PROPERTY_EXPORT_TEXT macro, which picks the right spelling.
//   2. Touching UWidgetBlueprint::WidgetVariableNameToGuidMap outside the
//      MCP_HAS_WIDGET_VARIABLE_GUID_MAP guard. The member does not exist before
//      5.6, so an unguarded reference fails to compile there.
//
// Both regressed exactly this way: the macro existed but three call sites
// bypassed it, and one file referenced the member with no guard at all.
import {
  readFileSync,
  readdirSync,
} from 'node:fs';
import { resolve } from 'node:path';

import {
  describe,
  expect,
  it,
} from 'vitest';

const privateRoot = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
);
const compatibilityHeader = 'Core/Compatibility/McpVersionCompatibility.h';

const listSourceFiles = (directory: string): readonly string[] => {
  const files: string[] = [];

  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    const entryPath = resolve(directory, entry.name);
    if (entry.isDirectory()) {
      files.push(...listSourceFiles(entryPath));
    } else if (/\.(?:cpp|h)$/u.test(entry.name)) {
      files.push(entryPath);
    }
  }

  return files;
};

const relativeToPrivate = (file: string): string =>
  file.slice(privateRoot.length + 1).replaceAll('\\', '/');

const codeLines = (source: string): readonly string[] =>
  source
    .split(/\r?\n/u)
    .map((line) => line.trim())
    .filter((line) => !line.startsWith('//') && !line.startsWith('*'));

describe('version-gated engine API contracts', () => {
  it('routes every ExportTextItem_Direct call through MCP_PROPERTY_EXPORT_TEXT', () => {
    // Given
    const files = listSourceFiles(privateRoot);

    // When
    const directCallers = files
      .filter((file) => relativeToPrivate(file) !== compatibilityHeader)
      .filter((file) => codeLines(readFileSync(file, 'utf8')).some((line) => line.includes('ExportTextItem_Direct')))
      .map(relativeToPrivate)
      .sort();

    // Then
    expect(directCallers).toEqual([]);
  });

  it('guards every WidgetVariableNameToGuidMap use behind MCP_HAS_WIDGET_VARIABLE_GUID_MAP', () => {
    // Given
    const files = listSourceFiles(privateRoot);

    // When
    const unguarded = files
      .filter((file) => relativeToPrivate(file) !== compatibilityHeader)
      .filter((file) => {
        const source = readFileSync(file, 'utf8');
        const usesMember = codeLines(source).some((line) => line.includes('WidgetVariableNameToGuidMap'));
        return usesMember && !source.includes('MCP_HAS_WIDGET_VARIABLE_GUID_MAP');
      })
      .map(relativeToPrivate)
      .sort();

    // Then
    expect(unguarded).toEqual([]);
  });

  // Each of these headers was confirmed absent from UE 5.0's engine source (the
  // engine install used for the audit): the declarations live in an older
  // header there, so an unconditional include fails to compile on 5.0. Guard
  // with __has_include or the appropriate version macro.
  const HEADERS_ABSENT_IN_5_0 = [
    'MaterialDomain.h',
    'Misc/StringOutputDevice.h',
    'UObject/StrProperty.h',
    'GeometryScript/MeshSelectionFunctions.h',
    'Containers/AllowShrinking.h',
  ] as const;

  it('never includes a header that does not exist in UE 5.0 without a guard', () => {
    // Given
    const files = listSourceFiles(privateRoot);

    // When
    const unguarded = files.flatMap((file) => {
      const lines = readFileSync(file, 'utf8').split(/\r?\n/u);
      const guardStack: string[] = [];
      const found: string[] = [];

      lines.forEach((raw, index) => {
        const line = raw.trim();
        if (/^#\s*ifn?def\b/u.test(line) || /^#\s*if\b/u.test(line)) {
          guardStack.push(line);
          return;
        }
        if (/^#\s*endif\b/u.test(line)) {
          guardStack.pop();
          return;
        }
        const include = line.match(/^#\s*include\s+"([^"]+)"/u)?.[1];
        if (include === undefined) return;
        if (!HEADERS_ABSENT_IN_5_0.some((header) => include.endsWith(header))) return;
        const guarded = guardStack.some((condition) =>
          /MCP_HAS_|ENGINE_MINOR_VERSION|ENGINE_MAJOR_VERSION|__has_include/u.test(condition));
        if (!guarded) found.push(`${relativeToPrivate(file)}:${index + 1} -> ${include}`);
      });

      return found;
    }).sort();

    // Then
    expect(unguarded).toEqual([]);
  });
});
