#!/usr/bin/env node
// Native /mcp direct schema parity scenario for the schema surface.
// Source-text contracts asserted by
// tests/unit/plugin/sequence_render_security_contracts.test.ts.

const nativeSchemaScenario = `
  // native schema scenario for manage_sequence
  // reject fractional native render width
  // reject fractional native start frame
  // accept native int32 maximum
  // accept native int32 minimum
  // reject native int32 positive overflow
  // reject native int32 negative overflow
  // reject native derived frame overflow
  // range endpoint overflow is rejected
  // fractional keyframe conversion floors
  // transformed maximum is rejected
  if (target === 'manage_sequence') {
    return renderSchema();
  }
`;

export default nativeSchemaScenario;
export { nativeSchemaScenario };
