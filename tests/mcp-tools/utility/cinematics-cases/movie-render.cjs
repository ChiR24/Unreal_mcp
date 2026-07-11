// Case/contract aggregation for the Movie Render Queue (MRQ) live + static
// coverage lane (T5 L2). Provides the source-text contracts that
// tests/unit/plugin/sequence_render_security_contracts.test.ts asserts, and
// the parameter values referenced by
// tests/mcp-tools/utility/movie-render.test.mjs.
//
// The allowlisted class paths below mirror the C++ defaults in
// McpAutomationBridgeSettings.cpp (MovieRenderExecutorClassAllowlist and
// MovieRenderBurnInClassAllowlist) so the security-contract unit test and the
// integration cases stay aligned with the plugin's enforcement surface.

module.exports = {
  DefaultBurnIn_DefaultBurnIn_C: 'DefaultBurnIn.DefaultBurnIn_C',
  BurnInAllowlisted: '/MovieRenderPipeline/Blueprints/DefaultBurnIn.DefaultBurnIn_C',
  includes: 'BurnInOverlay',
  CustomStencil: '/Engine/BufferVisualization/CustomStencil.CustomStencil',
  ExecutorInProcess: '/Script/MovieRenderPipelineCore.MoviePipelineInProcessExecutor',
  ExecutorEditor: '/Script/MovieRenderPipelineEditor.MoviePipelinePIEExecutor',
};
