import fs from 'node:fs';
import path from 'node:path';
import ts from 'typescript';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';

const auditFile = fileURLToPath(import.meta.url);
const testsDir = path.dirname(auditFile);

export const repoRoot = path.resolve(testsDir, '..');
export const definitionsRoot = path.join(repoRoot, 'src/tools/definitions');
export const testsRoot = path.join(repoRoot, 'tests/mcp-tools');
export const integrationSuitePath = path.join(repoRoot, 'tests/integration.mjs');
export const reportsDir = path.join(repoRoot, 'tests/reports');
export const requireFromAudit = createRequire(import.meta.url);
export const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor;

// Parent tool definitions are no longer hand-written under
// `definitions/<category>/*-tool.ts`; they are derived from the capability
// records and emitted into two generated artifacts. The audits read those
// artifacts instead of scanning the definitions directory.
//
// The split is deliberate:
// - the generated definitions carry the neutral parent contract that the native
//   plugin also generates, so they are the correct evidence for native parity;
// - the gateway manifest is the runtime facade projection, so it additionally
//   carries the TypeScript-only `params` passthrough appended by
//   `addActionParamsSchema`, making it the correct evidence for the parameter
//   audit, which measures the surface the server actually exposes.
export const generatedParentDefinitionsPath = path.join(
  repoRoot,
  'src/tools/catalog/capabilities/generated/parent-tool-definitions.generated.ts'
);
export const gatewayManifestPath = path.join(
  repoRoot,
  'src/gateway/gateway-manifest.generated.json'
);

/**
 * A root is "generated" when the caller did not pin one, or pinned the real
 * repository definitions directory. Explicit fixture roots stay on the
 * TypeScript compiler AST path so fixtures keep exercising that parser.
 */
export function isGeneratedDefinitionsRoot(candidate) {
  return candidate === undefined || path.resolve(candidate) === definitionsRoot;
}

function literalPropertyName(property) {
  return ts.isIdentifier(property.name) || ts.isStringLiteral(property.name)
    ? property.name.text
    : undefined;
}

function literalValue(node) {
  if (ts.isStringLiteral(node) || ts.isNoSubstitutionTemplateLiteral(node)) return node.text;
  if (ts.isNumericLiteral(node)) return Number(node.text);
  if (node.kind === ts.SyntaxKind.TrueKeyword) return true;
  if (node.kind === ts.SyntaxKind.FalseKeyword) return false;
  if (node.kind === ts.SyntaxKind.NullKeyword) return null;
  if (
    ts.isPrefixUnaryExpression(node)
    && node.operator === ts.SyntaxKind.MinusToken
    && ts.isNumericLiteral(node.operand)
  ) {
    return -Number(node.operand.text);
  }
  if (ts.isArrayLiteralExpression(node)) return node.elements.map(literalValue);
  if (ts.isObjectLiteralExpression(node)) {
    const value = {};
    for (const property of node.properties) {
      if (!ts.isPropertyAssignment(property)) continue;
      const name = literalPropertyName(property);
      if (name !== undefined) value[name] = literalValue(property.initializer);
    }
    return value;
  }
  throw new Error(
    `Unsupported syntax ${ts.SyntaxKind[node.kind]} in generated artifact; `
    + 'the emitter must keep generated definitions JSON-literal only.'
  );
}

function exportedArrayLiteral(sourcePath, exportName) {
  const source = ts.createSourceFile(
    sourcePath,
    fs.readFileSync(sourcePath, 'utf8'),
    ts.ScriptTarget.ES2022,
    true
  );

  for (const statement of source.statements) {
    if (!ts.isVariableStatement(statement)) continue;
    for (const declaration of statement.declarationList.declarations) {
      if (
        ts.isIdentifier(declaration.name)
        && declaration.name.text === exportName
        && declaration.initializer
        && ts.isArrayLiteralExpression(declaration.initializer)
      ) {
        return declaration.initializer.elements.map(literalValue);
      }
    }
  }

  throw new Error(`${exportName} array literal not found in ${sourcePath}`);
}

function requireNonEmptyDefinitions(definitions, sourcePath) {
  if (definitions.length === 0) {
    throw new Error(
      `${sourcePath} declares zero parent tool definitions; `
      + 'refusing to run a vacuous audit against an empty generated surface.'
    );
  }
  return definitions;
}

/**
 * Neutral generated parent contract, shared with the native plugin. Carries no
 * `params` passthrough, matching the generated native parent definitions.
 */
export function readGeneratedParentToolDefinitions(sourcePath = generatedParentDefinitionsPath) {
  return requireNonEmptyDefinitions(
    exportedArrayLiteral(sourcePath, 'generatedParentToolDefinitions'),
    sourcePath
  );
}

/**
 * Runtime facade projection: the gateway manifest is emitted from
 * `consolidatedToolDefinitions`, so it reflects the tool surface the server
 * actually serves, including the `params` passthrough.
 */
export function readRuntimeFacadeToolDefinitions(manifestPath = gatewayManifestPath) {
  const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  if (!Array.isArray(manifest.tools)) {
    throw new Error(`${manifestPath} declares no tools array`);
  }
  return requireNonEmptyDefinitions(manifest.tools, manifestPath);
}
