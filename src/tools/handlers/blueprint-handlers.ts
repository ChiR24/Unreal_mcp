import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, HandlerResult, BlueprintArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

/** Response from blueprint operations */
interface BlueprintResponse {
  success?: boolean;
  error?: string;
  message?: string;
  [key: string]: unknown;
}

export async function handleBlueprintTools(action: string, args: HandlerArgs, tools: ITools): Promise<HandlerResult> {
  const argsTyped = args as BlueprintArgs;
  const argsRecord = args as Record<string, unknown>;
  
  switch (action) {
    case 'create': {
      // Support 'path' or 'blueprintPath' argument by splitting it into name and savePath if not provided
      let name = argsTyped.name;
      let savePath = argsTyped.savePath;
      const pathArg = (argsRecord.path as string | undefined) || argsTyped.blueprintPath;

      if (pathArg) {
        const parts = pathArg.split('/');
        const extractName = parts.pop(); // The last part is the name

        // If name wasn't provided, use the extracted name
        if (!name) {
          name = extractName;
        }

        // If savePath wasn't provided, use the extracted path
        if (!savePath) {
          savePath = parts.join('/');
        }
      }

      if (!savePath) savePath = '/Game';

      const res = await tools.blueprintTools.createBlueprint({
        name: name ?? 'NewBlueprint',
        blueprintType: argsTyped.blueprintType,
        savePath: savePath,
        parentClass: argsRecord.parentClass as string | undefined,
        properties: argsTyped.properties,
        timeoutMs: argsRecord.timeoutMs as number | undefined,
        waitForCompletion: argsRecord.waitForCompletion as boolean | undefined,
        waitForCompletionTimeoutMs: argsRecord.waitForCompletionTimeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'ensure_exists': {
      const res = await tools.blueprintTools.waitForBlueprint(argsTyped.name || argsTyped.blueprintPath || (argsRecord.path as string) || '', argsRecord.timeoutMs as number | undefined);
      return cleanObject(res) as HandlerResult;
    }
    case 'add_variable': {
      // Support multiple naming conventions:
      // - blueprintPath/assetPath/path for the blueprint
      // - variableName/name for the variable (when 'name' isn't used for blueprint)
      const blueprintPathArg = argsTyped.blueprintPath || (argsRecord.assetPath as string) || (argsRecord.path as string) || '';
      // If blueprintPath is provided, 'name' can be used for variableName
      const variableNameArg = argsTyped.variableName || (blueprintPathArg ? (argsRecord.name as string) : '') || '';
      // memberClass/variableType for the type
      const variableTypeArg = (argsRecord.variableType as string) || (argsRecord.memberClass as string) || 'Boolean';
      
      const res = await tools.blueprintTools.addVariable({
        blueprintName: blueprintPathArg,
        variableName: variableNameArg,
        variableType: variableTypeArg,
        defaultValue: argsRecord.defaultValue,
        category: argsRecord.category as string | undefined,
        isReplicated: argsRecord.isReplicated as boolean | undefined,
        isPublic: argsRecord.isPublic as boolean | undefined,
        variablePinType: (typeof argsRecord.variablePinType === 'object' && argsRecord.variablePinType !== null ? argsRecord.variablePinType : undefined) as Record<string, unknown> | undefined,
        timeoutMs: argsRecord.timeoutMs as number | undefined,
        waitForCompletion: argsRecord.waitForCompletion as boolean | undefined,
        waitForCompletionTimeoutMs: argsRecord.waitForCompletionTimeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'set_variable_metadata': {
      const blueprintPathArg = argsTyped.blueprintPath || (argsRecord.assetPath as string) || (argsRecord.path as string) || '';
      const variableNameArg = argsTyped.variableName || (blueprintPathArg ? (argsRecord.name as string) : '') || '';
      const res = await tools.blueprintTools.setVariableMetadata({
        blueprintName: blueprintPathArg,
        variableName: variableNameArg,
        metadata: argsTyped.metadata ?? {},
        timeoutMs: argsRecord.timeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'remove_variable': {
      const blueprintPathArg = argsTyped.blueprintPath || (argsRecord.assetPath as string) || (argsRecord.path as string) || '';
      const variableNameArg = argsTyped.variableName || (blueprintPathArg ? (argsRecord.name as string) : '') || '';
      const res = await tools.blueprintTools.removeVariable({
        blueprintName: blueprintPathArg,
        variableName: variableNameArg,
        timeoutMs: argsRecord.timeoutMs as number | undefined,
        waitForCompletion: argsRecord.waitForCompletion as boolean | undefined,
        waitForCompletionTimeoutMs: argsRecord.waitForCompletionTimeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'rename_variable': {
      const blueprintPathArg = argsTyped.blueprintPath || (argsRecord.assetPath as string) || (argsRecord.path as string) || '';
      // For rename, 'name' is the old name (the variable to rename), newName is the new name
      const oldNameArg = (argsRecord.oldName as string) || (blueprintPathArg ? (argsRecord.name as string) : '') || '';
      const res = await tools.blueprintTools.renameVariable({
        blueprintName: blueprintPathArg,
        oldName: oldNameArg,
        newName: (argsRecord.newName as string) ?? '',
        timeoutMs: argsRecord.timeoutMs as number | undefined,
        waitForCompletion: argsRecord.waitForCompletion as boolean | undefined,
        waitForCompletionTimeoutMs: argsRecord.waitForCompletionTimeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'set_metadata': {
      const assetPathRaw = typeof (argsRecord.assetPath) === 'string' ? (argsRecord.assetPath as string).trim() : '';
      const blueprintPathRaw = typeof argsTyped.blueprintPath === 'string' ? argsTyped.blueprintPath.trim() : '';
      const nameRaw = typeof argsTyped.name === 'string' ? argsTyped.name.trim() : '';
      const savePathRaw = typeof argsTyped.savePath === 'string' ? argsTyped.savePath.trim() : '';

      let assetPath = assetPathRaw;
      if (!assetPath) {
        if (blueprintPathRaw) {
          assetPath = blueprintPathRaw;
        } else if (nameRaw && savePathRaw) {
          const base = savePathRaw.replace(/\/$/, '');
          assetPath = `${base}/${nameRaw}`;
        }
      }
      if (!assetPath) {
        throw new Error('Invalid parameters: assetPath or blueprintPath or name+savePath required for set_metadata');
      }

      const metadata = (argsTyped.metadata && typeof argsTyped.metadata === 'object') ? argsTyped.metadata : {};
      const res = await executeAutomationRequest(tools, 'set_metadata', { assetPath, metadata });
      return cleanObject(res) as HandlerResult;
    }
    case 'add_event': {
      const blueprintName = argsTyped.blueprintPath || (argsRecord.path as string | undefined) || argsTyped.name || '';
      const usedNameForBlueprint = !argsTyped.blueprintPath && !(argsRecord.path as string | undefined) && argsTyped.name;

      const res = await tools.blueprintTools.addEvent({
        blueprintName: blueprintName,
        eventType: argsTyped.eventType ?? 'Custom',
        customEventName: (argsRecord.customEventName as string | undefined) || (!usedNameForBlueprint ? argsTyped.name : undefined),
        parameters: argsRecord.parameters as { name: string; type: string }[] | undefined,
        timeoutMs: argsRecord.timeoutMs as number | undefined,
        waitForCompletion: argsRecord.waitForCompletion as boolean | undefined,
        waitForCompletionTimeoutMs: argsRecord.waitForCompletionTimeoutMs as number | undefined
      }) as BlueprintResponse;

      if (res && res.success === false) {
        const msg = (res.message || '').toLowerCase();
        if (msg.includes('already exists') || msg.includes('duplicate')) {
          return cleanObject({
            success: false,
            error: 'EVENT_ALREADY_EXISTS',
            message: res.message || 'Event already exists',
            blueprintName
          });
        }
      }
      return cleanObject(res) as HandlerResult;
    }
    case 'remove_event': {
      const res = await tools.blueprintTools.removeEvent({
        blueprintName: argsTyped.name || argsTyped.blueprintPath || (argsRecord.path as string) || '',
        eventName: (argsRecord.eventName as string) ?? '',
        customEventName: argsRecord.customEventName as string | undefined,
        timeoutMs: argsRecord.timeoutMs as number | undefined,
        waitForCompletion: argsRecord.waitForCompletion as boolean | undefined,
        waitForCompletionTimeoutMs: argsRecord.waitForCompletionTimeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'add_function': {
      // Prioritize explicit path for blueprint, allowing 'name' to be function name
      const blueprintName = argsTyped.blueprintPath || (argsRecord.path as string | undefined) || argsTyped.name || '';
      const usedNameForBlueprint = !argsTyped.blueprintPath && !(argsRecord.path as string | undefined) && argsTyped.name;

      const res = await tools.blueprintTools.addFunction({
        blueprintName: blueprintName,
        functionName: (argsRecord.functionName as string | undefined) || argsTyped.memberName || (!usedNameForBlueprint ? argsTyped.name : undefined) || 'NewFunction',
        inputs: argsRecord.inputs as { name: string; type: string }[] | undefined,
        outputs: argsRecord.outputs as { name: string; type: string }[] | undefined,
        isPublic: argsRecord.isPublic as boolean | undefined,
        category: argsRecord.category as string | undefined,
        timeoutMs: argsRecord.timeoutMs as number | undefined,
        waitForCompletion: argsRecord.waitForCompletion as boolean | undefined,
        waitForCompletionTimeoutMs: argsRecord.waitForCompletionTimeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'add_component': {
      const res = await tools.blueprintTools.addComponent({
        blueprintName: argsTyped.name || argsTyped.blueprintPath || (argsRecord.path as string) || '',
        componentType: argsTyped.componentType || (argsRecord.componentClass as string) || 'SceneComponent',
        componentName: argsTyped.componentName ?? '',
        attachTo: argsTyped.attachTo,
        transform: argsRecord.transform as Record<string, unknown> | undefined,
        properties: argsTyped.properties,
        compile: argsRecord.applyAndSave as boolean | undefined,
        save: argsRecord.applyAndSave as boolean | undefined,
        timeoutMs: argsRecord.timeoutMs as number | undefined,
        waitForCompletion: argsRecord.waitForCompletion as boolean | undefined,
        waitForCompletionTimeoutMs: argsRecord.waitForCompletionTimeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'modify_scs': {
      const res = await tools.blueprintTools.modifyConstructionScript({
        blueprintPath: argsTyped.name || argsTyped.blueprintPath || (argsRecord.path as string) || '',
        operations: (argsRecord.operations as Array<Record<string, unknown>>) ?? [],
        compile: argsRecord.applyAndSave as boolean | undefined,
        save: argsRecord.applyAndSave as boolean | undefined,
        timeoutMs: argsRecord.timeoutMs as number | undefined,
        waitForCompletion: argsRecord.waitForCompletion as boolean | undefined,
        waitForCompletionTimeoutMs: argsRecord.waitForCompletionTimeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'set_scs_transform': {
      const loc = argsRecord.location as { x?: number; y?: number; z?: number } | undefined;
      const rot = argsRecord.rotation as { pitch?: number; yaw?: number; roll?: number } | undefined;
      const scl = argsRecord.scale as { x?: number; y?: number; z?: number } | undefined;
      const res = await tools.blueprintTools.setSCSComponentTransform({
        blueprintPath: argsTyped.name || argsTyped.blueprintPath || (argsRecord.path as string) || '',
        componentName: argsTyped.componentName ?? '',
        location: loc ? [loc.x ?? 0, loc.y ?? 0, loc.z ?? 0] : undefined,
        rotation: rot ? [rot.pitch ?? 0, rot.yaw ?? 0, rot.roll ?? 0] : undefined,
        scale: scl ? [scl.x ?? 1, scl.y ?? 1, scl.z ?? 1] : undefined,
        timeoutMs: argsRecord.timeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'add_construction_script': {
      const res = await tools.blueprintTools.addConstructionScript({
        blueprintName: argsTyped.name || argsTyped.blueprintPath || (argsRecord.path as string) || '',
        scriptName: (argsRecord.scriptName as string) ?? '',
        timeoutMs: argsRecord.timeoutMs as number | undefined,
        waitForCompletion: argsRecord.waitForCompletion as boolean | undefined,
        waitForCompletionTimeoutMs: argsRecord.waitForCompletionTimeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'add_node': {
      if ((argsTyped.nodeType === 'CallFunction' || argsTyped.nodeType === 'K2Node_CallFunction') && !(argsRecord.functionName as string | undefined) && !argsTyped.memberName) {
        throw new Error('CallFunction node requires functionName parameter');
      }

      // Map common node aliases to K2Node types
      const nodeAliases: Record<string, string> = {
        'CallFunction': 'K2Node_CallFunction',
        'VariableGet': 'K2Node_VariableGet',
        'VariableSet': 'K2Node_VariableSet',
        'If': 'K2Node_IfThenElse',
        'Branch': 'K2Node_IfThenElse',
        'Switch': 'K2Node_Switch',
        'Select': 'K2Node_Select',
        'Cast': 'K2Node_DynamicCast',
        'CustomEvent': 'K2Node_CustomEvent',
        'Event': 'K2Node_Event',
        'MakeArray': 'K2Node_MakeArray',
        'ForEach': 'K2Node_ForEachElementInEnum' // Note: ForEachLoop is a macro, this is different
      };

      const resolvedNodeType = (argsTyped.nodeType && nodeAliases[argsTyped.nodeType]) || argsTyped.nodeType || 'K2Node_CallFunction';

      // Validation for Event nodes
      if ((resolvedNodeType === 'K2Node_Event' || resolvedNodeType === 'K2Node_CustomEvent') && !(argsRecord.eventName as string | undefined) && !(argsRecord.customEventName as string | undefined)) {
        // Check if 'name' was used for the blueprint or if it's available as an event name
        const usedNameForBlueprint = !argsTyped.blueprintPath && !(argsRecord.path as string | undefined) && argsTyped.name;
        
        // If 'name' is present and NOT used for the blueprint path, use it as eventName fallback
        if (argsTyped.name && !usedNameForBlueprint) {
           argsRecord.eventName = argsTyped.name;
        }

        if (!(argsRecord.eventName as string | undefined)) {
          throw new Error(`${resolvedNodeType} requires eventName (or customEventName) parameter`);
        }
      }


      const res = await tools.blueprintTools.addNode({
        blueprintName: argsTyped.name || argsTyped.blueprintPath || (argsRecord.path as string) || '',
        nodeType: resolvedNodeType,
        graphName: argsTyped.graphName,
        functionName: argsRecord.functionName as string | undefined,
        variableName: argsTyped.variableName,
        nodeName: argsRecord.nodeName as string | undefined,
        eventName: (argsRecord.eventName as string | undefined) || (argsRecord.customEventName as string | undefined),
        memberClass: argsRecord.memberClass as string | undefined,
        posX: argsRecord.posX as number | undefined,
        posY: argsRecord.posY as number | undefined,
        timeoutMs: argsRecord.timeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'add_scs_component': {
      const res = await tools.blueprintTools.addSCSComponent({
        blueprintPath: argsTyped.name || argsTyped.blueprintPath || (argsRecord.path as string) || '',
        componentClass: (argsRecord.componentClass as string | undefined) || argsTyped.componentType || 'SceneComponent',
        componentName: argsTyped.componentName ?? '',
        meshPath: argsRecord.meshPath as string | undefined,
        materialPath: argsRecord.materialPath as string | undefined,
        timeoutMs: argsRecord.timeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'reparent_scs_component': {
      const res = await tools.blueprintTools.reparentSCSComponent({
        blueprintPath: argsTyped.name || argsTyped.blueprintPath || (argsRecord.path as string) || '',
        componentName: argsTyped.componentName ?? '',
        newParent: (argsRecord.newParent as string) ?? '',
        timeoutMs: argsRecord.timeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'set_scs_property': {
      const res = await tools.blueprintTools.setSCSComponentProperty({
        blueprintPath: argsTyped.name || argsTyped.blueprintPath || (argsRecord.path as string) || '',
        componentName: argsTyped.componentName ?? '',
        propertyName: argsTyped.propertyName ?? '',
        propertyValue: argsRecord.propertyValue,
        timeoutMs: argsRecord.timeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'remove_scs_component': {
      const res = await tools.blueprintTools.removeSCSComponent({
        blueprintPath: argsTyped.name || argsTyped.blueprintPath || (argsRecord.path as string) || '',
        componentName: argsTyped.componentName ?? '',
        timeoutMs: argsRecord.timeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'get_scs': {
      const res = await tools.blueprintTools.getBlueprintSCS({
        blueprintPath: argsTyped.name || argsTyped.blueprintPath || (argsRecord.path as string) || '',
        timeoutMs: argsRecord.timeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'set_default': {
      const res = await tools.blueprintTools.setBlueprintDefault({
        blueprintName: argsTyped.name || argsTyped.blueprintPath || (argsRecord.path as string) || '',
        propertyName: argsTyped.propertyName ?? '',
        value: argsTyped.value
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'compile': {
      const res = await tools.blueprintTools.compileBlueprint({
        blueprintName: argsTyped.name || argsTyped.blueprintPath || (argsRecord.path as string) || '',
        saveAfterCompile: argsRecord.saveAfterCompile as boolean | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'probe_handle': {
      const res = await tools.blueprintTools.probeSubobjectDataHandle({
        componentClass: (argsRecord.componentClass as string) ?? ''
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'get': {
      const res = await tools.blueprintTools.getBlueprintInfo({
        blueprintPath: argsTyped.name || argsTyped.blueprintPath || (argsRecord.path as string) || '',
        timeoutMs: argsRecord.timeoutMs as number | undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'connect_pins':
    case 'break_pin_links':
    case 'delete_node':
    case 'create_reroute_node':
    case 'set_node_property':
    case 'get_node_details':
    case 'get_pin_details':
    case 'get_graph_details': {
      const processedArgs = {
        ...args,
        subAction: action,
        blueprintPath: argsTyped.blueprintPath || (argsRecord.path as string | undefined) || argsTyped.name
      };
      const res = await executeAutomationRequest(tools, 'manage_blueprint_graph', processedArgs, 'Automation bridge not available for blueprint graph operations');
      return cleanObject(res) as HandlerResult;
    }
    default: {
      // Translate applyAndSave to compile/save flags for modify_scs action
      const processedArgs = { ...args } as HandlerResult;
      if ((argsRecord.action as string | undefined) === 'modify_scs' && argsRecord.applyAndSave === true) {
        processedArgs.compile = true;
        processedArgs.save = true;
      }
      const res = await executeAutomationRequest(tools, 'manage_blueprint', processedArgs, 'Automation bridge not available for blueprint operations');
      return cleanObject(res) as HandlerResult;
    }
  }
}

export async function handleBlueprintGet(args: HandlerArgs, tools: ITools): Promise<HandlerResult> {
  const argsTyped = args as BlueprintArgs;
  const argsRecord = args as Record<string, unknown>;
  
  const res = await executeAutomationRequest(tools, 'blueprint_get', args, 'Automation bridge not available for blueprint operations') as { success?: boolean; message?: string } | null;
  if (res && res.success) {
    const blueprintPath = argsTyped.blueprintPath || (argsRecord.path as string | undefined) || argsTyped.name;
    return cleanObject({
      ...res,
      blueprintPath: typeof blueprintPath === 'string' ? blueprintPath : undefined,
      message: res.message || 'Blueprint fetched'
    }) as HandlerResult;
  }
  return cleanObject(res) as HandlerResult;
}
