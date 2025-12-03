import type { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';

export interface GraphQLContext {
    bridge: UnrealBridge;
    automationBridge: AutomationBridge;
}
