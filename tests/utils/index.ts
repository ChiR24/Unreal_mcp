/**
 * Test Utilities - Public Exports
 * 
 * Usage:
 * ```typescript
 * import { createMockTools, createMockAutomationBridge, expectCleanObject } from '../../tests/utils/index.js';
 * ```
 */

export {
    createMockAutomationBridge,
    createMockTools,
    expectCleanObject,
    wasCalledWith,
    mockResponses,
    type MockAutomationBridge,
} from './test-helpers.js';
