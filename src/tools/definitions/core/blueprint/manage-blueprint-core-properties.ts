import { commonSchemas } from '../../../catalog/tool-definition-utils.js';
import { WIDGET_AUTHORING_ACTIONS } from '../../shared/action-sets.js';

export const manageBlueprintCoreProperties = {
action: {
          type: 'string',
          enum: [
            'create', 'create_blueprint', 'get_blueprint', 'get', 'compile',
            'add_component', 'set_default', 'modify_scs', 'get_scs', 'add_scs_component', 'remove_scs_component', 'reparent_scs_component', 'set_scs_transform', 'set_scs_property',
            'ensure_exists', 'probe_handle', 'add_variable', 'remove_variable', 'rename_variable', 'add_function', 'remove_function', 'add_event', 'remove_event', 'add_construction_script', 'set_variable_metadata', 'set_metadata',
            'create_node', 'add_node', 'delete_node', 'connect_pins', 'break_pin_links', 'set_node_property', 'create_reroute_node', 'get_node_details', 'get_graph_details', 'get_pin_details',
            'list_node_types', 'set_pin_default_value'
          ,
            ...WIDGET_AUTHORING_ACTIONS],
          description: 'Blueprint action'
        },
name: commonSchemas.name,
blueprintPath: commonSchemas.blueprintPath,
blueprintType: commonSchemas.parentClass,
savePath: commonSchemas.savePath,
componentType: commonSchemas.stringProp,
componentName: commonSchemas.componentName,
componentClass: commonSchemas.stringProp,
attachTo: commonSchemas.stringProp,
newParent: commonSchemas.stringProp,
propertyName: commonSchemas.propertyName,
variableName: commonSchemas.variableName,
oldName: commonSchemas.stringProp,
newName: commonSchemas.newName,
value: commonSchemas.value,
metadata: commonSchemas.objectProp,
properties: commonSchemas.objectProp,
graphName: commonSchemas.graphName,
includePins: { type: 'boolean', description: 'get_graph_details: when true, each node also carries its pins and their linkedTo connections, so a graph\'s exec/data flow is readable in one call (no per-node get_node_details loop). Default false keeps the lightweight nodeId/nodeName/nodeTitle list.' },
nodeType: commonSchemas.stringProp,
nodeId: commonSchemas.nodeId,
pinName: commonSchemas.pinName,
linkedTo: commonSchemas.stringProp,
memberName: commonSchemas.stringProp,
x: commonSchemas.numberProp,
y: commonSchemas.numberProp,
location: commonSchemas.arrayOfNumbers,
rotation: commonSchemas.arrayOfNumbers,
scale: commonSchemas.arrayOfNumbers,
operations: commonSchemas.arrayOfObjects,
eventType: commonSchemas.stringProp,
customEventName: commonSchemas.eventName,
parameters: commonSchemas.arrayOfObjects,
// Variable configuration (C++ TryGetStringField/BoolField)
        variableType: { type: 'string', description: 'Variable type (e.g., Boolean, Float, Integer, Vector, String, Object)' },
defaultValue: commonSchemas.value,
category: commonSchemas.stringProp,
isReplicated: commonSchemas.booleanProp,
isPublic: commonSchemas.booleanProp,
// Function configuration
        functionName: commonSchemas.functionName,
inputs: commonSchemas.arrayOfObjects,
outputs: commonSchemas.arrayOfObjects,
// Node positioning (C++ TryGetNumberField)
        posX: commonSchemas.numberProp,
posY: commonSchemas.numberProp,
// Event configuration
        eventName: commonSchemas.eventName,
// Component/SCS configuration
        parentComponent: commonSchemas.stringProp,
meshPath: commonSchemas.meshPath,
materialPath: commonSchemas.materialPath,
applyAndSave: commonSchemas.booleanProp,
// Graph operations
        memberClass: commonSchemas.stringProp,
targetClass: commonSchemas.stringProp,
inputAxisName: commonSchemas.stringProp,
actionPath: commonSchemas.assetPath,
inputActionPath: commonSchemas.assetPath,
inputActionAssetPath: commonSchemas.assetPath,
// Compilation options
        saveAfterCompile: commonSchemas.booleanProp,
// Timing/async options
        timeoutMs: commonSchemas.numberProp,
// Parent class for blueprint creation
        parentClass: commonSchemas.parentClass,
// Graph pin connections
        fromNodeId: commonSchemas.sourceNodeId,
fromPinName: commonSchemas.sourcePin,
toNodeId: commonSchemas.targetNodeId,
toPinName: commonSchemas.targetPin
};
