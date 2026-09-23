// src/tools/catalog/capabilities/records/utility/helpers.ts
// Compatibility facade for the utility record builder. The implementation and
// the `UtilityRecordSpec` shape live in `utility-record-builders.ts`; input
// field type/description pins live in `utility-schema-pins.ts` and the
// Draft-2020-12 schema envelope in `utility-output-schema.ts`. Existing
// imports keep working through this re-export.
export {
  utilityRecord,
  withTopics,
  withAliases,
  withInputProps,
  asPostMigration,
  type UtilityRecordSpec
} from './utility-record-builders.js';
