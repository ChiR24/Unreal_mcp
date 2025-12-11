import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

export async function handleBlueprintTools(action: string, args: any, tools: ITools) {
  switch (action) {
    case 'create': {
      // Support 'path' or 'blueprintPath' argument by splitting it into name and savePath if not provided
      let name = args.name;
      let savePath = args.savePath;
      const pathArg = args.path || args.blueprintPath;
      if (!name && pathArg) {
        const parts = pathArg.split('/');
        name = parts.pop();
        savePath = parts.join('/');
        if (!savePath) savePath = '/Game';
      }

      const res = await tools.blueprintTools.createBlueprint({
        name: name,
        blueprintType: args.blueprintType,
        savePath: savePath,
        parentClass: args.parentClass,
        properties: args.properties,
        timeoutMs: args.timeoutMs,
        waitForCompletion: args.waitForCompletion,
        waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
      });
      return cleanObject(res);
    }
    case 'ensure_exists': {
      const res = await tools.blueprintTools.waitForBlueprint(args.name || args.blueprintPath || args.path, args.timeoutMs);
      return cleanObject(res);
    }
    case 'add_variable': {
      const res = await tools.blueprintTools.addVariable({
        blueprintName: args.name || args.blueprintPath || args.path,
        variableName: args.variableName,
        variableType: args.variableType,
        defaultValue: args.defaultValue,
        category: args.category,
        isReplicated: args.isReplicated,
        isPublic: args.isPublic,
        variablePinType: args.variablePinType,
        timeoutMs: args.timeoutMs,
        waitForCompletion: args.waitForCompletion,
        waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
      });
      return cleanObject(res);
    }
    case 'set_variable_metadata': {
      const res = await tools.blueprintTools.setVariableMetadata({
        blueprintName: args.name || args.blueprintPath || args.path,
        variableName: args.variableName,
        metadata: args.metadata,
        timeoutMs: args.timeoutMs
      });
      return cleanObject(res);
    }
    case 'remove_variable': {
      const res = await tools.blueprintTools.removeVariable({
        blueprintName: args.name || args.blueprintPath || args.path,
        variableName: args.variableName,
        timeoutMs: args.timeoutMs,
        waitForCompletion: args.waitForCompletion,
        waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
      });
      return cleanObject(res);
    }
    case 'rename_variable': {
      const res = await tools.blueprintTools.renameVariable({
        blueprintName: args.name || args.blueprintPath || args.path,
        oldName: args.oldName,
        newName: args.newName,
        timeoutMs: args.timeoutMs,
        waitForCompletion: args.waitForCompletion,
        waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
      });
      return cleanObject(res);
    }
    case 'set_metadata': {
      const assetPathRaw = typeof args.assetPath === 'string' ? args.assetPath.trim() : '';
      const blueprintPathRaw = typeof args.blueprintPath === 'string' ? args.blueprintPath.trim() : '';
      const nameRaw = typeof args.name === 'string' ? args.name.trim() : '';
      const savePathRaw = typeof args.savePath === 'string' ? args.savePath.trim() : '';

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

      const metadata = (args.metadata && typeof args.metadata === 'object') ? args.metadata : {};
      const res = await executeAutomationRequest(tools, 'set_metadata', { assetPath, metadata });
      return cleanObject(res);
    }
    case 'add_event': {
      const res = await tools.blueprintTools.addEvent({
        blueprintName: args.name || args.blueprintPath || args.path,
        eventType: args.eventType,
        customEventName: args.customEventName,
        parameters: args.parameters,
        timeoutMs: args.timeoutMs,
        waitForCompletion: args.waitForCompletion,
        waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
      });
      return cleanObject(res);
    }
    case 'remove_event': {
      const res = await tools.blueprintTools.removeEvent({
        blueprintName: args.name || args.blueprintPath || args.path,
        eventName: args.eventName,
        timeoutMs: args.timeoutMs,
        waitForCompletion: args.waitForCompletion,
        waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
      });
      return cleanObject(res);
    }
    case 'add_function': {
      const res = await tools.blueprintTools.addFunction({
        blueprintName: args.name || args.blueprintPath || args.path,
        functionName: args.functionName || args.memberName,
        inputs: args.inputs,
        outputs: args.outputs,
        isPublic: args.isPublic,
        category: args.category,
        timeoutMs: args.timeoutMs,
        waitForCompletion: args.waitForCompletion,
        waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
      });
      return cleanObject(res);
    }
    case 'add_component': {
      const res = await tools.blueprintTools.addComponent({
        blueprintName: args.name || args.blueprintPath || args.path,
        componentType: args.componentType || args.componentClass,
        componentName: args.componentName,
        attachTo: args.attachTo,
        transform: args.transform,
        properties: args.properties,
        compile: args.applyAndSave,
        save: args.applyAndSave,
        timeoutMs: args.timeoutMs,
        waitForCompletion: args.waitForCompletion,
        waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
      });
      return cleanObject(res);
    }
    case 'modify_scs': {
      const res = await tools.blueprintTools.modifyConstructionScript({
        blueprintPath: args.name || args.blueprintPath || args.path,
        operations: args.operations,
        compile: args.applyAndSave,
        save: args.applyAndSave,
        timeoutMs: args.timeoutMs,
        waitForCompletion: args.waitForCompletion,
        waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
      });
      return cleanObject(res);
    }
    case 'set_scs_transform': {
      const res = await tools.blueprintTools.setSCSComponentTransform({
        blueprintPath: args.name || args.blueprintPath || args.path,
        componentName: args.componentName,
        location: args.location,
        rotation: args.rotation,
        scale: args.scale,
        timeoutMs: args.timeoutMs
      });
      return cleanObject(res);
    }
    case 'add_construction_script': {
      const res = await tools.blueprintTools.addConstructionScript({
        blueprintName: args.name || args.blueprintPath || args.path,
        scriptName: args.scriptName,
        timeoutMs: args.timeoutMs,
        waitForCompletion: args.waitForCompletion,
        waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
      });
      return cleanObject(res);
    }
    case 'add_node': {
      const res = await tools.blueprintTools.addNode({
        blueprintName: args.name || args.blueprintPath || args.path,
        nodeType: args.nodeType,
        graphName: args.graphName,
        functionName: args.functionName,
        variableName: args.variableName,
        nodeName: args.nodeName,
        eventName: args.eventName,
        memberClass: args.memberClass,
        posX: args.posX,
        posY: args.posY,
        timeoutMs: args.timeoutMs
      });
      return cleanObject(res);
    }
    case 'add_scs_component': {
      const res = await tools.blueprintTools.addSCSComponent({
        blueprintPath: args.name || args.blueprintPath || args.path,
        componentClass: args.componentClass || args.componentType,
        componentName: args.componentName,
        meshPath: args.meshPath,
        materialPath: args.materialPath,
        timeoutMs: args.timeoutMs
      });
      return cleanObject(res);
    }
    case 'reparent_scs_component': {
      const res = await tools.blueprintTools.reparentSCSComponent({
        blueprintPath: args.name || args.blueprintPath || args.path,
        componentName: args.componentName,
        newParent: args.newParent,
        timeoutMs: args.timeoutMs
      });
      return cleanObject(res);
    }
    case 'set_scs_property': {
      const res = await tools.blueprintTools.setSCSComponentProperty({
        blueprintPath: args.name || args.blueprintPath || args.path,
        componentName: args.componentName,
        propertyName: args.propertyName,
        propertyValue: args.propertyValue,
        timeoutMs: args.timeoutMs
      });
      return cleanObject(res);
    }
    case 'remove_scs_component': {
      const res = await tools.blueprintTools.removeSCSComponent({
        blueprintPath: args.name || args.blueprintPath || args.path,
        componentName: args.componentName,
        timeoutMs: args.timeoutMs
      });
      return cleanObject(res);
    }
    case 'get_scs': {
      const res = await tools.blueprintTools.getBlueprintSCS({
        blueprintPath: args.name || args.blueprintPath || args.path,
        timeoutMs: args.timeoutMs
      });
      return cleanObject(res);
    }
    case 'set_default': {
      const res = await tools.blueprintTools.setBlueprintDefault({
        blueprintName: args.name || args.blueprintPath || args.path,
        propertyName: args.propertyName,
        value: args.value
      });
      return cleanObject(res);
    }
    case 'compile': {
      const res = await tools.blueprintTools.compileBlueprint({
        blueprintName: args.name || args.blueprintPath || args.path,
        saveAfterCompile: args.saveAfterCompile
      });
      return cleanObject(res);
    }
    case 'probe_handle': {
      const res = await tools.blueprintTools.probeSubobjectDataHandle({
        componentClass: args.componentClass
      });
      return cleanObject(res);
    }
    case 'get': {
      const res = await tools.blueprintTools.getBlueprintInfo({
        blueprintPath: args.name || args.blueprintPath || args.path,
        timeoutMs: args.timeoutMs
      });
      return cleanObject(res);
    }
    default: {
      // Translate applyAndSave to compile/save flags for modify_scs action
      const processedArgs = { ...args };
      if (args.action === 'modify_scs' && args.applyAndSave === true) {
        processedArgs.compile = true;
        processedArgs.save = true;
      }
      const res = await executeAutomationRequest(tools, 'manage_blueprint', processedArgs, 'Automation bridge not available for blueprint operations');
      return cleanObject(res);
    }
  }
}

export async function handleBlueprintGet(args: any, tools: ITools) {
  const res = await executeAutomationRequest(tools, 'blueprint_get', args, 'Automation bridge not available for blueprint operations');
  return cleanObject(res);
}
