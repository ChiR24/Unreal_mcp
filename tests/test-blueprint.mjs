#!/usr/bin/env node
/**
 * Blueprint Test Suite - Expanded
 * Tool: manage_blueprint
 * Actions: create, add_component, set_default, modify_scs
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: 'Create Actor blueprint',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'create',
      name: 'BP_TestActor',
      blueprintType: 'Actor',
      savePath: '/Game/Blueprints'
    },
    expected: 'success - blueprint created'
  },
  {
    scenario: 'Create Pawn blueprint',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'create',
      name: 'BP_TestPawn',
      blueprintType: 'Pawn',
      savePath: '/Game/Blueprints'
    },
    expected: 'success - pawn blueprint created'
  },
  {
    scenario: 'Create Character blueprint',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'create',
      name: 'BP_TestCharacter',
      blueprintType: 'Character',
      savePath: '/Game/Blueprints'
    },
    expected: 'success - character blueprint created'
  },
  {
    scenario: 'Create GameMode blueprint',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'create',
      name: 'BP_GameMode',
      blueprintType: 'GameModeBase',
      savePath: '/Game/Blueprints'
    },
    expected: 'success - gamemode blueprint created'
  },
  {
    scenario: 'Create PlayerController blueprint',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'create',
      name: 'BP_PlayerController',
      blueprintType: 'PlayerController',
      savePath: '/Game/Blueprints'
    },
    expected: 'success - controller blueprint created'
  },
  {
    scenario: 'Create Widget blueprint',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'create',
      name: 'WBP_MainMenu',
      blueprintType: 'UserWidget',
      savePath: '/Game/UI'
    },
    expected: 'success - widget blueprint created'
  },
  {
    scenario: 'Add StaticMesh component',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'add_component',
      name: 'BP_TestActor',
      componentType: 'StaticMeshComponent',
      componentName: 'MeshComp'
    },
    expected: 'success - mesh component added'
  },
  {
    scenario: 'Add PointLight component',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'add_component',
      name: 'BP_TestActor',
      componentType: 'PointLightComponent',
      componentName: 'LightComp'
    },
    expected: 'success - light component added'
  },
  {
    scenario: 'Add BoxCollision component',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'add_component',
      name: 'BP_TestActor',
      componentType: 'BoxComponent',
      componentName: 'CollisionBox'
    },
    expected: 'success - collision component added'
  },
  {
    scenario: 'Add Camera component',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'add_component',
      name: 'BP_TestPawn',
      componentType: 'CameraComponent',
      componentName: 'CameraComp'
    },
    expected: 'success - camera component added'
  },
  {
    scenario: 'Add SpringArm component',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'add_component',
      name: 'BP_TestPawn',
      componentType: 'SpringArmComponent',
      componentName: 'SpringArm'
    },
    expected: 'success - spring arm component added'
  },
  {
    scenario: 'Add AudioComponent',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'add_component',
      name: 'BP_TestActor',
      componentType: 'AudioComponent',
      componentName: 'AudioComp'
    },
    expected: 'success - audio component added'
  },
  {
    scenario: 'Set replication property',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'set_default',
      name: 'BP_TestActor',
      propertyName: 'bReplicates',
      propertyValue: true
    },
    expected: 'success - replication enabled'
  },
  {
    scenario: 'Set health property default',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'set_default',
      name: 'BP_TestCharacter',
      propertyName: 'MaxHealth',
      propertyValue: 100.0
    },
    expected: 'success - health set'
  },
  {
    scenario: 'Modify mesh component in SCS',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'modify_scs',
      blueprintPath: '/Game/Blueprints/BP_TestActor',
      operations: [
        { 
          type: 'modify_component', 
          componentName: 'MeshComp',
          properties: {
            RelativeLocation: { x: 0, y: 0, z: 100 },
            RelativeScale3D: { x: 2, y: 2, z: 2 }
          }
        }
      ]
    },
    expected: 'success - mesh component modified'
  }
];

await runToolTests('Blueprint', testCases);
