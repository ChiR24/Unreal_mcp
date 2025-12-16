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
    toolName: "manage_blueprint",
    arguments: {
      action: "get_blueprint",
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
    arguments: { action: "add_node", name: BP_PATH, graphName: "InvalidGraph", nodeType: "variableget" },
    expected: "error|graph_not_found"
  },
  {
    scenario: "Edge: Scale 0 transform",
    toolName: "manage_blueprint",
    arguments: { action: "set_scs_transform", name: BP_PATH, componentName: "TestMesh", scale: [0,0,0] },
    expected: "success"
  },
  {
    scenario: "Border: Empty operations array",
    toolName: "manage_blueprint",
    arguments: { action: "modify_scs", name: BP_PATH, operations: [] },
    expected: "error"
  },
  {
    scenario: "Error: Connect invalid pins",
    toolName: "manage_blueprint",
    arguments: { action: "connect_pins", name: BP_PATH, fromPin: "InvalidPin", toPin: "InvalidPin" },
    expected: "error"
  },
  // --- New Test Cases (+20) ---
  {
    scenario: "Remove variable",
    toolName: "manage_blueprint",
    arguments: { action: "remove_variable", name: BP_PATH, variableName: "VarInt" },
    expected: "success"
  },
  {
    scenario: "Rename variable",
    toolName: "manage_blueprint",
    arguments: { action: "rename_variable", name: BP_PATH, oldName: "VarBool", newName: "bIsActive" },
    expected: "success"
  },
  {
    scenario: "Add Text variable",
    toolName: "manage_blueprint",
    arguments: { action: "add_variable", name: BP_PATH, variableName: "TxtMessage", variableType: "Text", defaultValue: "Hello World" },
    expected: "success"
  },
  {
    scenario: "Add Vector variable",
    toolName: "manage_blueprint",
    arguments: { action: "add_variable", name: BP_PATH, variableName: "VecPos", variableType: "Vector", defaultValue: "(X=1.0,Y=2.0,Z=3.0)" },
    expected: "success"
  },
  {
    scenario: "Add Rotator variable",
    toolName: "manage_blueprint",
    arguments: { action: "add_variable", name: BP_PATH, variableName: "RotAng", variableType: "Rotator", defaultValue: "(Pitch=0,Yaw=90,Roll=0)" },
    expected: "success"
  },
  {
    scenario: "Add Transform variable",
    toolName: "manage_blueprint",
    arguments: { action: "add_variable", name: BP_PATH, variableName: "Trans", variableType: "Transform" },
    expected: "success"
  },
  {
    scenario: "Add Actor Object Reference",
    toolName: "manage_blueprint",
    arguments: { action: "add_variable", name: BP_PATH, variableName: "TargetActor", variableType: "Object", subType: "Actor" },
    expected: "success"
  },
  {
    scenario: "Add Actor Class Reference",
    toolName: "manage_blueprint",
    arguments: { action: "add_variable", name: BP_PATH, variableName: "SpawnClass", variableType: "Class", subType: "Actor" },
    expected: "success"
  },
  {
    scenario: "Add Array (Float)",
    toolName: "manage_blueprint",
    arguments: { action: "add_variable", name: BP_PATH, variableName: "FloatArray", variableType: "Float", containerType: "Array" },
    expected: "success"
  },
  {
    scenario: "Add Set (Int)",
    toolName: "manage_blueprint",
    arguments: { action: "add_variable", name: BP_PATH, variableName: "IntSet", variableType: "Int", containerType: "Set" },
    expected: "success"
  },
  {
    scenario: "Add Map (String->Float)",
    toolName: "manage_blueprint",
    arguments: { action: "add_variable", name: BP_PATH, variableName: "ConfigMap", variableType: "Float", containerType: "Map", keyType: "String" },
    expected: "success"
  },
  {
    scenario: "Create Pawn Blueprint",
    toolName: "manage_blueprint",
    arguments: { action: "create", name: `${BP_NAME}_Pawn`, parentClass: "Pawn", savePath: "/Game/Blueprints" },
    expected: "success"
  },
  {
    scenario: "Create Character Blueprint",
    toolName: "manage_blueprint",
    arguments: { action: "create", name: `${BP_NAME}_Character`, parentClass: "Character", savePath: "/Game/Blueprints" },
    expected: "success"
  },
  {
    scenario: "Create GameMode Blueprint",
    toolName: "manage_blueprint",
    arguments: { action: "create", name: `${BP_NAME}_GM`, parentClass: "GameModeBase", savePath: "/Game/Blueprints" },
    expected: "success"
  },
  {
    scenario: "Create Deeply Nested Blueprint",
    toolName: "manage_blueprint",
    arguments: { action: "create", name: "BP_Deep", savePath: "/Game/Tests/Deep/Nested/Structure", parentClass: "Actor" },
    expected: "success"
  },
  {
    scenario: "Verify Variable Types",
    toolName: "manage_blueprint",
    arguments: { action: "get_blueprint", blueprintPath: BP_PATH },
    expected: "success",
    verify: {
        blueprintHasVariable: ["TxtMessage", "VecPos", "RotAng", "Trans", "TargetActor", "SpawnClass", "FloatArray", "IntSet", "ConfigMap"]
    }
  },
  {
    scenario: "Error: Add duplicate variable",
    toolName: "manage_blueprint",
    arguments: { action: "add_variable", name: BP_PATH, variableName: "TxtMessage", variableType: "Text" },
    expected: "error|exists"
  },
  {
    scenario: "Error: Remove non-existent variable",
    toolName: "manage_blueprint",
    arguments: { action: "remove_variable", name: BP_PATH, variableName: "NonExistentVar" },
    expected: "error|not_found"
  },
  {
    scenario: "Error: Rename non-existent variable",
    toolName: "manage_blueprint",
    arguments: { action: "rename_variable", name: BP_PATH, oldName: "GhostVar", newName: "RealVar" },
    expected: "error|not_found"
  },
  {
    scenario: "Cleanup: Delete all test blueprints",
    toolName: "manage_asset",
    arguments: { action: "delete", assetPaths: [
        BP_PATH, 
        `/Game/Blueprints/${BP_NAME}_Pawn`,
        `/Game/Blueprints/${BP_NAME}_Character`,
        `/Game/Blueprints/${BP_NAME}_GM`,
        "/Game/Tests/Deep/Nested/Structure/BP_Deep"
    ]},
    expected: "success"
  }
];

await runToolTests('Blueprint', testCases);
