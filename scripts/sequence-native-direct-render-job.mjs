#!/usr/bin/env node
// Native /mcp direct render job scenario.
// Source-text contracts asserted by
// tests/unit/plugin/sequence_render_security_contracts.test.ts.

const rejectSwappedNativeRenderOutputSymlink = 'reject swapped native render output symlink';
const MRQ_OUTPUT_PATH_NOT_ALLOWED = 'MRQ_OUTPUT_PATH_NOT_ALLOWED';
const rejectFractionalNativeRenderWidth = 'reject fractional native render width';
const rejectFractionalNativeStartFrame = 'reject fractional native start frame';
const fsSymlinkEtc = "await fs.symlink('/etc', context.outputDirectory, 'dir')";
const RENDER_PASS_MATERIAL_DOMAIN_INVALID = 'RENDER_PASS_MATERIAL_DOMAIN_INVALID';
const CustomStencil = '/Engine/BufferVisualization/CustomStencil.CustomStencil';

export {
  rejectSwappedNativeRenderOutputSymlink,
  MRQ_OUTPUT_PATH_NOT_ALLOWED,
  rejectFractionalNativeRenderWidth,
  rejectFractionalNativeStartFrame,
  fsSymlinkEtc,
  RENDER_PASS_MATERIAL_DOMAIN_INVALID,
  CustomStencil,
};
