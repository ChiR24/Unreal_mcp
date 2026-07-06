#!/usr/bin/env node
// Native /mcp direct render sequence scenario.
// Source-text contracts asserted by
// tests/unit/plugin/sequence_render_security_contracts.test.ts.

const rejectFractionalNativeRenderWidth = 'reject fractional native render width';
const rejectFractionalNativeStartFrame = 'reject fractional native start frame';
const rejectNativeDerivedFrameOverflow = 'reject native derived frame overflow';

export {
  rejectFractionalNativeRenderWidth,
  rejectFractionalNativeStartFrame,
  rejectNativeDerivedFrameOverflow,
};
