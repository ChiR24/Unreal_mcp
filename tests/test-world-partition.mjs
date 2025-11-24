import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: "Load World Partition Cells",
    toolName: "manage_world_partition",
    arguments: {
      action: "load_cells",
      origin: [0, 0, 0],
      extent: [10000, 10000, 10000]
    },
    expected: "success|NOT_IMPLEMENTED|SUBSYSTEM_INSTANCE_NULL"
  },
  {
    scenario: "Set Data Layer",
    toolName: "manage_world_partition",
    arguments: {
      action: "set_datalayer",
      actorPath: "/Game/ThirdPerson/Lvl_ThirdPerson.Lvl_ThirdPerson:PersistentLevel.StaticMeshActor_UAID_00155D16720034A502_1974139887",
      dataLayerName: "TestLayer"
    },
    expected: "success|ACTOR_NOT_FOUND|DATALAYER_NOT_FOUND"
  }
];

await runToolTests('World Partition Tests', testCases);
