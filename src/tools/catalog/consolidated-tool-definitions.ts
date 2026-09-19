// src/tools/catalog/consolidated-tool-definitions.ts
//
// Runtime facade for the 23 canonical parent tool definitions.
//
// The generator derives the parent surface EXCLUSIVELY from the strict (folded)
// CapabilityRecords (name/category/description from record parent metadata;
// action enum from record legacyIds; input/output schemas as permissive unions
// of exact per-action record properties). It emits the generated
// parent-tool-definitions artifact, which this facade clones and augments with
// the `params` passthrough expected by the TS gateway. Generated native parent
// definitions intentionally do not carry `params`.
//
// Do NOT hand-author schema/action tables here -- they live in the generated
// artifact and are emitted deterministically by the generator from records.

import { generatedParentToolDefinitions } from './capabilities/generated/parent-tool-definitions.generated.js';
import { addActionParamsSchema } from './tool-definition-utility-schema-helpers.js';
import type { ToolDefinition } from '../definitions/shared/tool-definition.js';

export type { ToolDefinition } from '../definitions/shared/tool-definition.js';

export const consolidatedToolDefinitions: ToolDefinition[] = (() => {
  const cloned: ToolDefinition[] = generatedParentToolDefinitions.map((definition) =>
    structuredClone(definition),
  );
  addActionParamsSchema(cloned);
  return cloned;
})();
