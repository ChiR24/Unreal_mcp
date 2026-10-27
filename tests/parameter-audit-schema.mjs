import {
  readFoldedActionsByTool,
  readRuntimeFacadeToolDefinitions
} from './parameter-audit-context.mjs';
import { compareAscii } from './ordering.mjs';

function sortedStrings(values) {
  return Array.isArray(values)
    ? [...new Set(values.filter((value) => typeof value === 'string'))].sort()
    : [];
}

// The advertised enum stays exact; a folded family also lists the old names it
// still serves, so a static case that exercises one covers a declared action.
function schemaFromRuntimeFacade(definition, foldedByTool = new Map()) {
  const properties = definition.inputSchema?.properties ?? {};
  const folded = foldedByTool.get(definition.name) ?? new Set();
  return {
    name: definition.name,
    actions: sortedStrings(properties['action']?.enum),
    foldedActions: sortedStrings([...folded]),
    properties: Object.keys(properties).sort(),
    required: sortedStrings(definition.inputSchema?.required)
  };
}

/**
 * The generated runtime facade: the surface the server actually exposes. The
 * facade loader fails closed on an empty artifact, so a missing or emptied
 * generated surface raises instead of silently reporting zero coverage.
 */
export function extractToolSchemas() {
  const foldedByTool = readFoldedActionsByTool();
  return readRuntimeFacadeToolDefinitions()
    .map((definition) => schemaFromRuntimeFacade(definition, foldedByTool))
    .sort((left, right) => compareAscii(left.name, right.name));
}
