export function runParityCli(audit, config = {}) {
  const result = audit(config);
  console.log('Native MCP Action Parity Audit');
  console.log(`TypeScript tools: ${result.counts.typeScriptDefinitions}`);
  console.log(`Native canonical tools: ${result.counts.nativeRegistryEntries}`);
  console.log(`Native canonical definitions: ${result.counts.nativeDefinitions}`);
  console.log(`Tools with action mismatches: ${result.actionGaps.length}`);
  console.log(`Schema parity tools: ${result.schemaParityTools.join(', ')}`);
  console.log(`Tools with schema property mismatches: ${result.schemaPropertyGaps.length}`);

  if (result.hasMismatches) {
    console.error(JSON.stringify({
      counts: result.counts,
      duplicateNames: result.duplicateNames,
      toolNameGaps: result.toolNameGaps,
      actionGaps: result.actionGaps,
      schemaPropertyGaps: result.schemaPropertyGaps
    }, null, 2));
    process.exitCode = 1;
  }
  return result;
}
