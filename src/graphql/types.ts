import type { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import type { GraphQLLoaders } from './loaders.js';

export interface GraphQLContext {
    bridge: UnrealBridge;
    automationBridge: AutomationBridge;
    /** DataLoaders for batching and caching - solves N+1 query problem */
    loaders?: GraphQLLoaders;
}
