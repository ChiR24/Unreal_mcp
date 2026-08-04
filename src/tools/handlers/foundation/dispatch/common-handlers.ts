export { executeAutomationRequest, createSubActionDispatcher, executeBatchConsoleCommands } from './automation-request-dispatch.js';
export type { SubActionDispatcher } from './automation-request-dispatch.js';
export { withHandlerContext, createUnknownActionResponse } from './handler-error-context.js';
export { ensureArgsPresent, validateSecurityPatterns, validateArgsSecurity, requireAction, requireNonEmptyString, requireAssetName, validateExpectedParams, validateRequiredParams } from '../arguments/handler-argument-validation.js';
export { getTimeoutMs, resolveActionTimeoutMs, resolveCostTimeoutMs } from './handler-timeout.js';
export { normalizePathFields } from '../normalization/ue-path-normalization.js';
export { promoteScalarResultFields } from '../responses/scalar-result-promotion.js';
export { normalizeLocation, normalizeRotation } from '../normalization/transform-normalization.js';
