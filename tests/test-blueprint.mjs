import { runToolTests } from './test-runner.mjs';

// Use a unique blueprint name per run to avoid collisions
const ts = new Date().toISOString().replace(/[^0-9]/g, '').slice(0, 12);
const BP_NAME = `BP_Auto_${ts}`;
const BP_PATH = `/Game/Blueprints/${BP_NAME}`;

const testCases = [
  {
    scenario: "Create Actor blueprint",
    toolName: "manage_blueprint",
    arguments: {
      action: "create",
      name: BP_NAME,
      blueprintType: "Actor",
      savePath: "/Game/Blueprints",
      waitForCompletion: true
    },
    expected: "success"
  },
  {
    scenario: "Ensure blueprint exists (probe)",
    toolName: "manage_blueprint",
    arguments: {
      action: "ensure_exists",
      name: BP_PATH,
      timeoutMs: 30000
    },
    expected: "success"
  },
  {
    scenario: "Add Float var",
    toolName: "manage_blueprint",
    arguments: {
      action: "add_variable",
      name: BP_PATH,
      variableName: "MyVar",
      variableType: "Float",
      defaultValue: 0.0,
      waitForCompletion: true
    },
    expected: "success"
  },
  {
    scenario: "Add Int var",
    toolName: "manage_blueprint",
    arguments: {
      action: "add_variable",
      name: BP_PATH,
      variableName: "VarInt",
      variableType: "Int",
      defaultValue: 7,
      waitForCompletion: true
    },
    expected: "success"
  },
  {
    scenario: "Add Bool var",
    toolName: "manage_blueprint",
    arguments: {
      action: "add_variable",
      name: BP_PATH,
      variableName: "VarBool",
      variableType: "Bool",
      defaultValue: true,
      waitForCompletion: true
    },
    expected: "success"
  },
  {
    scenario: "Set variable metadata",
    toolName: "manage_blueprint",
    arguments: {
      action: "set_variable_metadata",
      name: BP_PATH,
      variableName: "MyVar",
      metadata: {
        displayName: "Health",
        tooltip: "Health of the actor"
      }
    },
    expected: "success"
  },
  {
    scenario: "Add custom event",
    toolName: "manage_blueprint",
    arguments: {
      action: "add_event",
      name: BP_PATH,
      eventType: "Custom",
      customEventName: "OnCustom",
      waitForCompletion: true
    },
    expected: "success"
  },
  {
    scenario: "Add function with input",
    toolName: "manage_blueprint",
    arguments: {
      action: "add_function",
      name: BP_PATH,
      functionName: "DoSomething",
      inputs: [
        {
          name: "Value",
          type: "Float"
        }
      ],
      waitForCompletion: true
    },
    expected: "success"
  },
  {
    scenario: "Add SMC via add_component",
    toolName: "manage_blueprint",
    arguments: {
      action: "add_component",
      name: BP_PATH,
      componentType: "StaticMeshComponent",
      componentName: "TestMesh",
      attachTo: "RootComponent",
      applyAndSave: true
    },
    expected: "success"
  },
  {
    scenario: "Modify SCS (add component operation)",
    toolName: "manage_blueprint",
    arguments: {
      action: "modify_scs",
      name: BP_PATH,
      operations: [
        {
          type: "add_component",
          componentName: "RuntimeAddedMesh",
          componentClass: "StaticMeshComponent",
          attachTo: "RootComponent"
        }
      ],
      applyAndSave: true
    },
    expected: "success"
  },
  {
    scenario: "Set SCS transform",
    toolName: "manage_blueprint",
    arguments: {
      action: "set_scs_transform",
      name: BP_PATH,
      componentName: "RuntimeAddedMesh",
      location: [0, 0, 50],
      rotation: [0, 0, 0],
      scale: [1, 1, 1]
    },
    expected: "success"
  },
  {
    scenario: "Add construction script entry",
    toolName: "manage_blueprint",
    arguments: {
      action: "add_construction_script",
      name: BP_PATH,
      scriptName: "InitScript"
    },
    expected: "success"
  },
  {
    scenario: "Add VariableGet node",
    toolName: "manage_blueprint",
    arguments: {
      action: "add_node",
      name: BP_PATH,
      nodeType: "variableget",
      variableName: "MyVar",
      graphName: "EventGraph",
      posX: 0,
      posY: 0
    },
    expected: "success or requires editor build or not_available"
  },
  {
    scenario: "Add VariableSet node",
    toolName: "manage_blueprint",
    arguments: {
      action: "add_node",
      name: BP_PATH,
      nodeType: "variableset",
      variableName: "MyVar",
      graphName: "EventGraph",
      posX: 200,
      posY: 0
    },
    expected: "success or requires editor build or not_available"
  },
  {
    scenario: "Add second custom event",
    toolName: "manage_blueprint",
    arguments: {
      action: "add_event",
      name: BP_PATH,
      eventType: "Custom",
      customEventName: "OnCustom2",
      waitForCompletion: true
    },
    expected: "success"
  },
  {
    scenario: "Remove second custom event",
    toolName: "manage_blueprint",
    arguments: {
      action: "remove_event",
      name: BP_PATH,
      eventName: "OnCustom2",
      waitForCompletion: true
    },
    expected: "success"
  },
  {
    scenario: "Add SCS component via API",
    toolName: "manage_blueprint",
    arguments: {
      action: "add_scs_component",
      name: BP_PATH,
      componentType: "StaticMeshComponent",
      componentName: "APINode"
    },
    expected: "success"
  },
  {
    scenario: "Reparent SCS component",
    toolName: "manage_blueprint",
    arguments: {
      action: "reparent_scs_component",
      name: BP_PATH,
      componentName: "APINode",
      newParent: "RootComponent"
    },
    expected: "success"
  },
  {
    scenario: "Set SCS property",
    toolName: "manage_blueprint",
    arguments: {
      action: "set_scs_property",
      name: BP_PATH,
      componentName: "APINode",
      propertyName: "bVisible",
      propertyValue: true
    },
    expected: "success"
  },
  {
    scenario: "Remove SCS component",
    toolName: "manage_blueprint",
    arguments: {
      action: "remove_scs_component",
      name: BP_PATH,
      componentName: "APINode"
    },
    expected: "success"
  },
  {
    scenario: "Get SCS hierarchy",
    toolName: "manage_blueprint",
    arguments: {
      action: "get_scs",
      name: BP_PATH
    },
    expected: "success"
  },
  {
    scenario: "Set blueprint default property",
    toolName: "manage_blueprint",
    arguments: {
      action: "set_default",
      name: BP_PATH,
      propertyName: "MyVar",
      value: 100.0
    },
    expected: "success"
  },
  {
    scenario: "Compile blueprint",
    toolName: "manage_blueprint",
    arguments: {
      action: "compile",
      name: BP_PATH,
      saveAfterCompile: true
    },
    expected: "success"
  },
  {
    scenario: "Light Switch - Create BP",
    toolName: "manage_blueprint",
    arguments: {
      action: "create",
      name: "BP_LightSwitch",
      savePath: "/Game/Blueprints",
      parentClass: "Actor"
    },
    expected: "success"
  },
  {
    scenario: "Light Switch - Add Trigger Box",
    toolName: "manage_blueprint",
    arguments: {
      action: "add_scs_component",
      name: "/Game/Blueprints/BP_LightSwitch",
      componentType: "BoxComponent",
      componentName: "TriggerBox",
      parentName: "DefaultSceneRoot"
    },
    expected: "success"
  },
  {
    scenario: "Light Switch - Add State Variable",
    toolName: "manage_blueprint",
    arguments: {
      action: "add_variable",
      name: "/Game/Blueprints/BP_LightSwitch",
      variableName: "IsLightOn",
      variableType: "bool",
      defaultValue: false
    },
    expected: "success"
  },
  {
    scenario: "Light Switch - Add Toggle Function",
    toolName: "manage_blueprint",
    arguments: {
      action: "add_function",
      name: "/Game/Blueprints/BP_LightSwitch",
      functionName: "ToggleLight"
    },
    expected: "success"
  },
  {
    scenario: "Light Switch - Compile",
    toolName: "manage_blueprint",
    arguments: {
      action: "compile",
      name: "/Game/Blueprints/BP_LightSwitch",
      saveAfterCompile: true
    },
    expected: "success"
  },
  {
    scenario: "Probe SubobjectData handle",
    toolName: "manage_blueprint",
    arguments: {
      action: "probe_handle",
      name: BP_PATH,
      componentClass: "StaticMeshComponent"
    },
    expected: "success"
  },
  {
    scenario: "Fetch blueprint and verify registry entries",
    toolName: "manage_blueprint",
    arguments: {
      action: "get",
      name: BP_PATH
    },
    expected: "success",
    verify: {
      blueprintHasVariable: ["MyVar", "VarInt", "VarBool"],
      blueprintHasFunction: ["DoSomething"],
      blueprintHasEvent: ["OnCustom"]
    }
  },
  {
    scenario: "Retrieve blueprint details via blueprint_get",
    toolName: "blueprint_get",
    arguments: {
      action: "get",
      blueprintPath: BP_PATH
    },
    expected: "success"
  },
  {
    scenario: "Remove custom event",
    toolName: "manage_blueprint",
    arguments: {
      action: "remove_event",
      name: BP_PATH,
      eventName: "OnCustom",
      waitForCompletion: true
    },
    expected: "success"
  },
  {
    scenario: "Fetch unknown blueprint should fail",
    toolName: "manage_blueprint",
    arguments: {
      action: "get",
      name: "/Game/Blueprints/DOES_NOT_EXIST_123"
    },
    expected: "not found"
  },
  {
    scenario: "Idempotent add variable check",
    toolName: "manage_blueprint",
    arguments: {
      action: "add_variable",
      name: BP_PATH,
      variableName: "MyVar",
      variableType: "Float"
    },
    expected: "success"
  },
  {
    scenario: "Ensure blueprint still exists",
    toolName: "manage_blueprint",
    arguments: {
      action: "ensure_exists",
      name: BP_PATH,
      timeoutMs: 30000
    },
    expected: "success"
  },
  {
    scenario: "Error: Invalid graph name",
    toolName: "manage_blueprint",
    arguments: { action: "add_node", name: "/ValidBP", graphName: "InvalidGraph", nodeType: "variableget" },
    expected: "error|graph_not_found"
  },
  {
    scenario: "Edge: Scale 0 transform",
    toolName: "manage_blueprint",
    arguments: { action: "set_scs_transform", name: "/ValidBP", componentName: "TestComp", scale: [0,0,0] },
    expected: "success"
  },
  {
    scenario: "Border: Empty operations array",
    toolName: "manage_blueprint",
    arguments: { action: "modify_scs", name: "/ValidBP", operations: [] },
    expected: "success|no_op"
  },
  {
    scenario: "Error: Connect invalid pins",
    toolName: "manage_blueprint",
    arguments: { action: "connect_pins", name: "/ValidBP", fromPin: "InvalidPin", toPin: "InvalidPin" },
    expected: "error"
  }
];

await runToolTests('Blueprint', testCases);
