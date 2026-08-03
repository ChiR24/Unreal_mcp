// Shared gateway discovery/parity test helpers.
//
// `firstAction` was duplicated byte-for-byte across the unreal-gateway discovery,
// execute-error-parity, and parity suites. It reads the first declared action off
// a canonical parent tool's union input schema; keeping one copy here means a
// catalog shape change surfaces once instead of three times.

import { consolidatedToolDefinitions } from '../../../../src/tools/catalog/consolidated-tool-definitions.js';
import { isRecord } from '../../../../src/utils/validation/type-guards.js';

/** The first declared action of a canonical parent tool (fallback `'x'`). */
export function firstAction(toolName: string): string {
  const def = consolidatedToolDefinitions.find((tool) => tool.name === toolName);
  const props = isRecord(def?.inputSchema) && isRecord(def.inputSchema.properties) ? def.inputSchema.properties : undefined;
  const action = isRecord(props) ? props.action : undefined;
  const enumArr = isRecord(action) && Array.isArray(action.enum) ? action.enum : [];
  const first = enumArr.find((value) => typeof value === 'string');
  return typeof first === 'string' ? first : 'x';
}
