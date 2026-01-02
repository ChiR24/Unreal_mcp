import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { GraphArgs, HandlerArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

/** Response from automation requests */
interface AutomationResponse {
    success?: boolean;
    error?: string;
    message?: string;
    result?: Record<string, unknown>;
    [key: string]: unknown;
}

/** Extended GraphArgs for internal processing */
interface ProcessedGraphArgs extends GraphArgs {
    subAction?: string;
    nodeCategory?: string;
    [key: string]: unknown;
}

// Blueprint node type aliases: map human-friendly names to K2Node class names
// NOTE: These aliases are applied in TS before sending to C++. The C++ plugin has
// its own fallback alias map (McpAutomationBridge_BlueprintGraphHandlers.cpp) which
// handles unmapped types. Keep these in sync with the C++ map.
const BLUEPRINT_NODE_ALIASES: Record<string, string> = {
    'Branch': 'K2Node_IfThenElse',
    'IfThenElse': 'K2Node_IfThenElse',
    'Sequence': 'K2Node_ExecutionSequence',
    'ExecutionSequence': 'K2Node_ExecutionSequence',
    'ForEachLoop': 'K2Node_ForEachElementInEnum',
    'ForLoop': 'K2Node_ForLoop',  // Basic ForLoop - use ForLoopWithBreak for break support
    'ForLoopWithBreak': 'K2Node_ForLoopWithBreak',
    'WhileLoop': 'K2Node_WhileLoop',
    'Switch': 'K2Node_SwitchInteger',
    'SwitchOnInt': 'K2Node_SwitchInteger',
    'SwitchOnString': 'K2Node_SwitchString',
    'SwitchOnName': 'K2Node_SwitchName',
    'SwitchOnEnum': 'K2Node_SwitchEnum',
    'Select': 'K2Node_Select',
    'DoOnce': 'K2Node_DoOnce',
    'DoN': 'K2Node_DoN',
    'FlipFlop': 'K2Node_FlipFlop',
    'Gate': 'K2Node_Gate',
    'MultiGate': 'K2Node_MultiGate',
    // 'Delay': 'K2Node_Delay', // Removed: Handled as function call in C++
    'Timeline': 'K2Node_Timeline',
    'SpawnActorFromClass': 'K2Node_SpawnActorFromClass',
    // 'DestroyActor': 'K2Node_DestroyActor', // Removed: Handled as function call in C++
    'GetAllActorsOfClass': 'K2Node_GetAllActorsOfClass'
};

// Behavior Tree node type aliases
const BT_NODE_ALIASES: Record<string, { class: string; type: string }> = {
    'Task': { class: 'BTTask_Wait', type: 'task' },
    'Wait': { class: 'BTTask_Wait', type: 'task' },
    'MoveTo': { class: 'BTTask_MoveTo', type: 'task' },
    'PlaySound': { class: 'BTTask_PlaySound', type: 'task' },
    'PlayAnimation': { class: 'BTTask_PlayAnimation', type: 'task' },
    'RunBehavior': { class: 'BTTask_RunBehavior', type: 'task' },
    'MakeNoise': { class: 'BTTask_MakeNoise', type: 'task' },
    'RotateToFaceBBEntry': { class: 'BTTask_RotateToFaceBBEntry', type: 'task' },
    'Decorator': { class: 'BTDecorator_Blackboard', type: 'decorator' },
    'Blackboard': { class: 'BTDecorator_Blackboard', type: 'decorator' },
    'CompareBBEntries': { class: 'BTDecorator_CompareBBEntries', type: 'decorator' },
    'Cooldown': { class: 'BTDecorator_Cooldown', type: 'decorator' },
    'DoesPathExist': { class: 'BTDecorator_DoesPathExist', type: 'decorator' },
    'ForceSuccess': { class: 'BTDecorator_ForceSuccess', type: 'decorator' },
    'KeepInCone': { class: 'BTDecorator_KeepInCone', type: 'decorator' },
    'Loop': { class: 'BTDecorator_Loop', type: 'decorator' },
    'TimeLimit': { class: 'BTDecorator_TimeLimit', type: 'decorator' },
    'Service': { class: 'BTService_DefaultFocus', type: 'service' },
    'DefaultFocus': { class: 'BTService_DefaultFocus', type: 'service' },
    'RunEQS': { class: 'BTService_RunEQS', type: 'service' },
    'Selector': { class: 'BTComposite_Selector', type: 'composite' },
    'SequenceNode': { class: 'BTComposite_Sequence', type: 'composite' },
    'SimpleParallel': { class: 'BTComposite_SimpleParallel', type: 'composite' }
};

export async function handleGraphTools(toolName: string, action: string, args: GraphArgs, tools: ITools): Promise<Record<string, unknown>> {
    // Common validation
    if (!args.assetPath && !args.blueprintPath && !args.systemPath) {
        // Some actions might not need a path if they operate on "currently open" asset, 
        // but generally we want an asset path.
    }

    // Dispatch based on tool name
    switch (toolName) {
        case 'manage_blueprint_graph':
            return handleBlueprintGraph(action, args, tools);
        case 'manage_niagara_graph':
            return handleNiagaraGraph(action, args, tools);
        case 'manage_material_graph':
            return handleMaterialGraph(action, args, tools);
        case 'manage_behavior_tree':
            return handleBehaviorTree(action, args, tools);
        default:
            throw new Error(`Unknown graph tool: ${toolName}`);
    }
}

async function handleBlueprintGraph(action: string, args: GraphArgs, tools: ITools): Promise<Record<string, unknown>> {
    const processedArgs: ProcessedGraphArgs = { ...args, subAction: action };

    // Default graphName
    if (!processedArgs.graphName) {
        processedArgs.graphName = 'EventGraph';
    }

    // Map human-friendly node type names to K2Node class names
    if (processedArgs.nodeType && BLUEPRINT_NODE_ALIASES[processedArgs.nodeType]) {
        processedArgs.nodeType = BLUEPRINT_NODE_ALIASES[processedArgs.nodeType];
    }

    // Fix Issue 1: Map FunctionCall to CallFunction
    if (processedArgs.nodeType === 'FunctionCall') {
        processedArgs.nodeType = 'CallFunction';
    }

    // Fix Issue 2 & 3: Map memberName to specific names based on nodeType
    if (processedArgs.memberName) {
        if (processedArgs.nodeType === 'VariableGet' || processedArgs.nodeType === 'VariableSet') {
            if (!processedArgs.variableName) processedArgs.variableName = processedArgs.memberName;
        } else if (processedArgs.nodeType === 'Event' || processedArgs.nodeType === 'CustomEvent' || (processedArgs.nodeType && processedArgs.nodeType.startsWith('K2Node_Event'))) {
            if (!processedArgs.eventName) processedArgs.eventName = processedArgs.memberName;
            // CustomEvent uses eventName (mapped to CustomFunctionName) or customEventName in some contexts, 
            // but C++ CustomEvent handler uses 'eventName' payload field.
        } else if (processedArgs.nodeType === 'CallFunction' || processedArgs.nodeType === 'K2Node_CallFunction') {
            // C++ uses 'memberName' for CallFunction, so this is fine.
        }
    }

    // Fix Issue 5: Map memberClass/componentClass to targetClass for Cast nodes
    if ((processedArgs.memberClass || processedArgs.componentClass) &&
        (processedArgs.nodeType === 'Cast' || (processedArgs.nodeType && processedArgs.nodeType.startsWith('CastTo')))) {
        if (!processedArgs.targetClass) processedArgs.targetClass = processedArgs.memberClass || processedArgs.componentClass;
    }

    // Fix Issue 6: Support connect_pins parameter mapping
    // Input: nodeId, pinName, linkedTo (TargetNode.Pin)
    if (action === 'connect_pins') {
        // Map source
        if (!processedArgs.fromNodeId && processedArgs.nodeId) {
            processedArgs.fromNodeId = processedArgs.nodeId;
        }
        if (!processedArgs.fromPinName && processedArgs.pinName) {
            processedArgs.fromPinName = processedArgs.pinName;
        }

        // Map target from linkedTo
        if (!processedArgs.toNodeId && processedArgs.linkedTo) {
            if (processedArgs.linkedTo.includes('.')) {
                const parts = processedArgs.linkedTo.split('.');
                processedArgs.toNodeId = parts[0];
                processedArgs.toPinName = parts.slice(1).join('.');
            }
        }
    }

    // Support Node.Pin format for connect_pins (existing logic preserved/enhanced)
    if (action === 'connect_pins') {
        if (processedArgs.fromNodeId && processedArgs.fromNodeId.includes('.') && !processedArgs.fromPinName) {
            const parts = processedArgs.fromNodeId.split('.');
            processedArgs.fromNodeId = parts[0];
            processedArgs.fromPinName = parts.slice(1).join('.');
        }
        if (processedArgs.toNodeId && processedArgs.toNodeId.includes('.') && !processedArgs.toPinName) {
            const parts = processedArgs.toNodeId.split('.');
            processedArgs.toNodeId = parts[0];
            processedArgs.toPinName = parts.slice(1).join('.');
        }
    }

    const res = await executeAutomationRequest(tools, 'manage_blueprint_graph', processedArgs as HandlerArgs, 'Automation bridge not available') as AutomationResponse;
    return cleanObject({ ...res, ...(res.result || {}) }) as Record<string, unknown>;
}

async function handleNiagaraGraph(action: string, args: GraphArgs, tools: ITools): Promise<Record<string, unknown>> {
    const payload: ProcessedGraphArgs = { ...args, subAction: action };
    // Map systemPath to assetPath if missing
    if (payload.systemPath && !payload.assetPath) {
        payload.assetPath = payload.systemPath;
    }
    const res = await executeAutomationRequest(tools, 'manage_niagara_graph', payload as HandlerArgs, 'Automation bridge not available') as AutomationResponse;
    return cleanObject({ ...res, ...(res.result || {}) }) as Record<string, unknown>;
}

async function handleMaterialGraph(action: string, args: GraphArgs, tools: ITools): Promise<Record<string, unknown>> {
    const payload: ProcessedGraphArgs = { ...args, subAction: action };

    // Map blueprint-style parameters to material graph parameters
    if (action === 'connect_pins' || action === 'connect_nodes') {
        if (payload.fromNodeId && !payload.sourceNodeId) {
            payload.sourceNodeId = payload.fromNodeId;
        }
        
        if (payload.toNodeId && !payload.targetNodeId) {
            const targetId = payload.toNodeId as string;
            payload.targetNodeId = (targetId === 'Root' || targetId === 'root') ? 'Main' : targetId;
        }
        
        if (payload.toPin && !payload.inputName) {
            const pinName = payload.toPin as string;
            payload.inputName = pinName.replace(/\s+/g, '');
        }
    }

    const res = await executeAutomationRequest(tools, 'manage_material_graph', payload as HandlerArgs, 'Automation bridge not available') as AutomationResponse;
    return cleanObject({ ...res, ...(res.result || {}) }) as Record<string, unknown>;
}

async function handleBehaviorTree(action: string, args: GraphArgs, tools: ITools): Promise<Record<string, unknown>> {
    const processedArgs: ProcessedGraphArgs = { ...args, subAction: action };
    
    // Map human-friendly node type names to BT class names
    if (processedArgs.nodeType && BT_NODE_ALIASES[processedArgs.nodeType]) {
        const alias = BT_NODE_ALIASES[processedArgs.nodeType];
        processedArgs.nodeType = alias.class;
        // Set nodeCategory if not already set
        if (!processedArgs.nodeCategory) {
            processedArgs.nodeCategory = alias.type;
        }
    }
    
    const res = await executeAutomationRequest(tools, 'manage_behavior_tree', processedArgs as HandlerArgs, 'Automation bridge not available') as AutomationResponse;
    return cleanObject({ ...res, ...(res.result || {}) }) as Record<string, unknown>;
}
