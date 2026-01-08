/* eslint-disable no-console */
import fs from 'fs/promises';
import path from 'path';
import ts from 'typescript';

type ToolActionMap = Map<string, Set<string>>;

type Violation = {
  tool: string;
  kind: 'missing_cpp_registration' | 'missing_cpp_actions' | 'extra_cpp_actions' | 'unreadable';
  message: string;
  details?: Record<string, unknown>;
};

function toPosixPath(p: string): string {
  return p.replace(/\\/g, '/');
}

async function fileExists(filePath: string): Promise<boolean> {
  try {
    await fs.stat(filePath);
    return true;
  } catch {
    return false;
  }
}

function usageAndExit(message?: string): never {
  if (message) {
    // scripts are allowed to print to stdout
    console.error(message);
  }
  console.error(
    [
      'Usage:',
      '  node --loader ts-node/esm scripts/verify-handler-sync.ts [--tool manage_water] [--format text|json]',
      '',
      'Checks that TS tool action enums match C++ handler sub-actions and tool registration.',
    ].join('\n')
  );
  process.exit(2);
}

function parseArgs(argv: string[]): { toolFilter?: string; format: 'text' | 'json' } {
  const out: { toolFilter?: string; format: 'text' | 'json' } = { format: 'text' };
  for (let i = 2; i < argv.length; i += 1) {
    const token = argv[i];
    if (token === '--tool') {
      const value = argv[i + 1];
      if (!value) usageAndExit('Missing value after --tool');
      out.toolFilter = value;
      i += 1;
      continue;
    }
    if (token === '--format') {
      const value = argv[i + 1];
      if (value !== 'text' && value !== 'json') usageAndExit('Invalid --format; expected text|json');
      out.format = value;
      i += 1;
      continue;
    }
    if (token === '-h' || token === '--help') usageAndExit();
    usageAndExit(`Unknown argument: ${token}`);
  }
  return out;
}

function getRepoRootFromImportMeta(importMetaUrl: string): string {
  const filePath = fileURLToPath(importMetaUrl);
  return path.resolve(path.dirname(filePath), '..');
}

// Node ESM compat
import { fileURLToPath } from 'url';

async function readUtf8(filePath: string): Promise<string> {
  return await fs.readFile(filePath, 'utf8');
}

function extractConsolidatedToolDefinitions(fileText: string, fileNameForDiagnostics: string): ToolActionMap {
  const sourceFile = ts.createSourceFile(fileNameForDiagnostics, fileText, ts.ScriptTarget.ES2022, true, ts.ScriptKind.TS);

  const toolActions: ToolActionMap = new Map();

  const groupedDiagnostics: string[] = [];

  function report(node: ts.Node, msg: string): void {
    const { line, character } = sourceFile.getLineAndCharacterOfPosition(node.getStart(sourceFile));
    groupedDiagnostics.push(`${fileNameForDiagnostics}:${line + 1}:${character + 1} ${msg}`);
  }

  function isIdentifierNamed(node: ts.Node, name: string): node is ts.Identifier {
    return ts.isIdentifier(node) && node.text === name;
  }

  function getProperty(objectLiteral: ts.ObjectLiteralExpression, name: string): ts.PropertyAssignment | undefined {
    for (const prop of objectLiteral.properties) {
      if (!ts.isPropertyAssignment(prop)) continue;
      const propName = prop.name;
      if (ts.isIdentifier(propName) && propName.text === name) return prop;
      if (ts.isStringLiteral(propName) && propName.text === name) return prop;
    }
    return undefined;
  }

  function getStringLiteralValue(expr: ts.Expression | undefined): string | undefined {
    if (!expr) return undefined;
    if (ts.isStringLiteral(expr) || ts.isNoSubstitutionTemplateLiteral(expr)) return expr.text;
    return undefined;
  }

  function getStringArrayLiteral(expr: ts.Expression | undefined): string[] | undefined {
    if (!expr) return undefined;
    if (!ts.isArrayLiteralExpression(expr)) return undefined;
    const values: string[] = [];
    for (const el of expr.elements) {
      if (ts.isStringLiteral(el) || ts.isNoSubstitutionTemplateLiteral(el)) {
        values.push(el.text);
      } else {
        return undefined;
      }
    }
    return values;
  }

  function tryExtractToolFromObjectLiteral(obj: ts.ObjectLiteralExpression): void {
    const nameProp = getProperty(obj, 'name');
    if (!nameProp) return;
    const toolName = getStringLiteralValue(nameProp.initializer);
    if (!toolName) {
      report(nameProp, 'Tool definition has non-literal name; skipping');
      return;
    }

    const inputSchemaProp = getProperty(obj, 'inputSchema');
    if (!inputSchemaProp || !ts.isObjectLiteralExpression(inputSchemaProp.initializer)) {
      report(obj, `Tool '${toolName}' missing inputSchema literal; skipping`);
      return;
    }

    const inputSchema = inputSchemaProp.initializer;
    const propertiesProp = getProperty(inputSchema, 'properties');
    if (!propertiesProp || !ts.isObjectLiteralExpression(propertiesProp.initializer)) {
      report(obj, `Tool '${toolName}' missing inputSchema.properties literal; skipping`);
      return;
    }

    const propertiesObj = propertiesProp.initializer;
    const actionProp = getProperty(propertiesObj, 'action');
    if (!actionProp || !ts.isObjectLiteralExpression(actionProp.initializer)) {
      report(obj, `Tool '${toolName}' missing inputSchema.properties.action literal; skipping`);
      return;
    }

    const actionSchema = actionProp.initializer;
    const enumProp = getProperty(actionSchema, 'enum');
    const enumValues = enumProp ? getStringArrayLiteral(enumProp.initializer) : undefined;

    if (!enumValues) {
      report(obj, `Tool '${toolName}' action enum not a string literal array; skipping`);
      return;
    }

    toolActions.set(toolName, new Set(enumValues));
  }

  function visit(node: ts.Node): void {
    // Look for: export const consolidatedToolDefinitions: ToolDefinition[] = [ ... ]
    if (ts.isVariableStatement(node) && node.declarationList.declarations.length > 0) {
      const isExported = node.modifiers?.some((m) => m.kind === ts.SyntaxKind.ExportKeyword) ?? false;
      if (!isExported) {
        ts.forEachChild(node, visit);
        return;
      }

      for (const decl of node.declarationList.declarations) {
        if (!ts.isVariableDeclaration(decl)) continue;
        if (!decl.name || !isIdentifierNamed(decl.name, 'consolidatedToolDefinitions')) continue;
        if (!decl.initializer || !ts.isArrayLiteralExpression(decl.initializer)) continue;

        for (const el of decl.initializer.elements) {
          if (ts.isObjectLiteralExpression(el)) {
            tryExtractToolFromObjectLiteral(el);
          }
        }
      }
    }

    ts.forEachChild(node, visit);
  }

  visit(sourceFile);

  if (toolActions.size === 0) {
    const hint = groupedDiagnostics.length > 0 ? groupedDiagnostics.slice(0, 15).join('\n') : '(no specific AST diagnostics)';
    throw new Error(`Failed to extract tool definitions. Diagnostics:\n${hint}`);
  }

  return toolActions;
}

async function listCppHandlerFiles(repoRoot: string): Promise<string[]> {
  const handlersRoot = path.join(repoRoot, 'plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private');
  const entries = await fs.readdir(handlersRoot);
  const candidates: string[] = [];
  for (const e of entries) {
    if (!e.endsWith('.cpp')) continue;
    if (!e.includes('Handlers')) continue;
    candidates.push(path.join(handlersRoot, e));
  }
  return candidates;
}

function extractRegisteredToolsFromSubsystemCpp(subsystemCppText: string): Set<string> {
  const toolNames = new Set<string>();
  const re = /RegisterHandler\(TEXT\("([a-z0-9_-]+)"\)/g;
  for (let m = re.exec(subsystemCppText); m; m = re.exec(subsystemCppText)) {
    toolNames.add(m[1]);
  }
  return toolNames;
}

function extractCppSubActions(cppText: string): Set<string> {
  const actions = new Set<string>();

  // Common patterns in this repo:
  // - if (LowerSub == TEXT("create_water_body_ocean"))
  // - else if (LowerSub == TEXT("..."))
  // - Resp->SetStringField(TEXT("action"), LowerSub);
  const reLowerSubCompare = /LowerSub\s*==\s*TEXT\("([^"]+)"\)/g;
  for (let m = reLowerSubCompare.exec(cppText); m; m = reLowerSubCompare.exec(cppText)) {
    actions.add(m[1].toLowerCase());
  }

  // Fallback: switch-case patterns if any exist
  const reCase = /case\s+TEXT\("([^"]+)"\)\s*:/g;
  for (let m = reCase.exec(cppText); m; m = reCase.exec(cppText)) {
    actions.add(m[1].toLowerCase());
  }

  return actions;
}

function guessCppFileForTool(tool: string, cppFiles: string[], fileContents: Map<string, string>): string[] {
  const suffix = tool.replace(/^manage_/, '');
  const directName = `McpAutomationBridge_${suffix.charAt(0).toUpperCase()}${suffix.slice(1)}Handlers.cpp`;
  const direct = cppFiles.find((p) => path.basename(p) === directName);
  if (direct) return [direct];

  const matches: string[] = [];
  const toolRe = new RegExp(`\\bHandle\\w*\\(.*\\b${tool}\\b`, 'i');
  for (const p of cppFiles) {
    const txt = fileContents.get(p);
    if (!txt) continue;
    if (txt.includes(tool) || toolRe.test(txt)) matches.push(p);
  }
  return matches;
}

async function main(): Promise<void> {
  const { toolFilter, format } = parseArgs(process.argv);
  const repoRoot = getRepoRootFromImportMeta(import.meta.url);

  const defsPath = path.join(repoRoot, 'src', 'tools', 'consolidated-tool-definitions.ts');
  const subsystemCppPath = path.join(
    repoRoot,
    'plugins',
    'McpAutomationBridge',
    'Source',
    'McpAutomationBridge',
    'Private',
    'McpAutomationBridgeSubsystem.cpp'
  );

  if (!(await fileExists(defsPath))) throw new Error(`Missing file: ${toPosixPath(defsPath)}`);
  if (!(await fileExists(subsystemCppPath))) throw new Error(`Missing file: ${toPosixPath(subsystemCppPath)}`);

  const [defsText, subsystemCppText] = await Promise.all([readUtf8(defsPath), readUtf8(subsystemCppPath)]);
  const tsToolActions = extractConsolidatedToolDefinitions(defsText, defsPath);

  const registeredTools = extractRegisteredToolsFromSubsystemCpp(subsystemCppText);

  const cppFiles = await listCppHandlerFiles(repoRoot);
  const fileContents = new Map<string, string>();
  await Promise.all(
    cppFiles.map(async (p) => {
      const txt = await readUtf8(p);
      fileContents.set(p, txt);
    })
  );

  const violations: Violation[] = [];
  const checkedTools: string[] = [];

  for (const [tool, actions] of tsToolActions.entries()) {
    if (toolFilter && tool !== toolFilter) continue;

    checkedTools.push(tool);

    if (!registeredTools.has(tool)) {
      violations.push({
        tool,
        kind: 'missing_cpp_registration',
        message: `Tool '${tool}' is present in TS definitions but not registered via RegisterHandler(TEXT("${tool}"), ...) in McpAutomationBridgeSubsystem.cpp`,
      });
      continue;
    }

    const guessedFiles = guessCppFileForTool(tool, cppFiles, fileContents);
    if (guessedFiles.length === 0) {
      violations.push({
        tool,
        kind: 'unreadable',
        message: `Could not locate a C++ *Handlers.cpp file likely implementing '${tool}'.`,
      });
      continue;
    }

    const cppActions = new Set<string>();
    for (const fp of guessedFiles) {
      const t = fileContents.get(fp);
      if (!t) continue;
      for (const a of extractCppSubActions(t)) cppActions.add(a);
    }

    // Compare (TS actions are typically lower_snake; normalize TS to lower)
    const tsLower = new Set<string>(Array.from(actions).map((a) => a.toLowerCase()));

    const missing: string[] = [];
    for (const a of tsLower) {
      if (!cppActions.has(a)) missing.push(a);
    }

    // Extra is informational (can be aliases or dead code, but still helpful)
    const extra: string[] = [];
    for (const a of cppActions) {
      if (!tsLower.has(a)) extra.push(a);
    }

    if (missing.length > 0) {
      violations.push({
        tool,
        kind: 'missing_cpp_actions',
        message: `Tool '${tool}' has TS actions missing in C++ handler(s).`,
        details: {
          missing,
          searchedFiles: guessedFiles.map((p) => toPosixPath(path.relative(repoRoot, p))),
        },
      });
    }

    if (extra.length > 0) {
      violations.push({
        tool,
        kind: 'extra_cpp_actions',
        message: `Tool '${tool}' has C++ handler actions not present in TS enum (may be aliases/dead code).`,
        details: {
          extra,
          searchedFiles: guessedFiles.map((p) => toPosixPath(path.relative(repoRoot, p))),
        },
      });
    }
  }

  const result = {
    checkedTools,
    violationCount: violations.length,
    violations,
  };

  if (format === 'json') {
    console.log(JSON.stringify(result, null, 2));
  } else {
    console.log(`Checked ${checkedTools.length} tool(s).`);
    if (violations.length === 0) {
      console.log('OK: No mismatches found.');
    } else {
      for (const v of violations) {
        console.log(`\n[${v.kind}] ${v.tool}: ${v.message}`);
        if (v.details) {
          console.log(JSON.stringify(v.details, null, 2));
        }
      }
      console.log(`\nFound ${violations.length} issue(s).`);
    }
  }

  process.exit(violations.some((v) => v.kind === 'missing_cpp_registration' || v.kind === 'missing_cpp_actions' || v.kind === 'unreadable') ? 1 : 0);
}

main().catch((err: unknown) => {
  const message = err instanceof Error ? err.message : String(err);
  console.error(`verify-handler-sync failed: ${message}`);
  process.exit(1);
});
