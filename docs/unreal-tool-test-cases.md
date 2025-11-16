# Unreal Tool Automated Test Cases

These scenarios feed the automated harness in `tests/run-unreal-tool-tests.mjs`. Every row uses a JSON payload the runner can execute directly without manual edits.

**Placeholders**

- `{{FBX_TEST_MODEL}}` resolves to a writable FBX file path. By default it maps to `C:\\Users\\micro\\Downloads\\Compressed\\fbx\\test_model.fbx`; override with the `UNREAL_MCP_FBX_DIR` or `UNREAL_MCP_FBX_FILE` environment variables if needed.

---

## Lighting Tools (`src/tools/lighting.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Directional light spawn uses defaults | `{"action":"create_light","lightType":"Directional","name":"MCP_Test_Directional"}` | Success response reports the directional light spawn. |
| 2 | Point light spawn at custom location | `{"action":"create_light","lightType":"Point","name":"MCP_Test_Point","location":{"x":0,"y":200,"z":400}}` | Success response confirms the point light creation. |
| 3 | Directional light rejects negative intensity | `{"action":"create_light","lightType":"Directional","name":"MCP_Test_Directional_Bad","intensity":-1}` | Error message explains the invalid intensity value. |
| 4 | Directional light custom intensity and color | `{"name":"Dir_Custom","intensity":50,"color":[0.2,0.4,0.8]}` | Success response notes the directional light intensity and color update. |
| 5 | Directional light with custom rotation | `{"name":"Dir_Rot","rotation":[45,90,0]}` | Success response reports the directional light spawned with the requested rotation. |
| 6 | Point light custom location and radius | `{"name":"Point_Custom","location":[100,0,400],"radius":500}` | Success response confirms the point light radius change. |
| 7 | Point light invalid radius is rejected | `{"name":"Point_Bad","radius":-1}` | Error message highlights that the radius must be positive. |
| 8 | Point light custom falloff exponent | `{"name":"Point_Falloff","falloffExponent":4}` | Success response notes the falloff exponent update. |
| 9 | Spot light with cone overrides | `{"name":"Spot_Cone","location":[0,0,0],"rotation":[0,0,0],"innerCone":10,"outerCone":25}` | Success response confirms the spot light cone angles. |
| 10 | Spot light rejects invalid cone order | `{"name":"Spot_ConeSwap","innerCone":45,"outerCone":30}` | Error message states inner cone must be less than outer cone. |
| 11 | Spot light rejects outer cone above 180 | `{"name":"Spot_BadCone","outerCone":200}` | Error response cites the outer cone limit of 180 degrees. |
| 12 | Rect light width and height overrides | `{"name":"Rect_Custom","width":200,"height":50}` | Success response reports the rect light dimensions. |
| 13 | Rect light invalid width fails validation | `{"name":"Rect_Bad","width":0}` | Error message explains the width must be greater than zero. |
| 14 | Sky light default spawn with recapture | `{"name":"Sky_Default","recapture":true}` | Success response confirms the sky light setup and recapture trigger. |
| 15 | Sky light missing cubemap asset fails without cubemap | `{"name":"Sky_MissingCubemap","sourceType":"SpecifiedCubemap"}` | Error message states that a cubemap path is required when `sourceType` is `SpecifiedCubemap`. |

---

## Actor Tools (`src/tools/actors.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Spawn cube actor for MCP smoke test | `{"action":"spawn","classPath":"/Engine/BasicShapes/Cube","name":"MCP_Cube_A"}` | Success response indicates the actor spawned. |
| 2 | Delete previously spawned actor | `{"action":"delete","actorName":"MCP_Cube_A"}` | Success response reports the actor delete. |
| 3 | Spawn without class path returns validation error | `{"action":"spawn"}` | Error message states that `classPath` is required. |
| 4 | Spawn cube actor with transform override | `{"action":"spawn","classPath":"/Engine/BasicShapes/Cube","name":"MCP_Cube_B","location":[0,100,300],"rotation":[0,45,0]}` | Success response confirms the actor spawn using the provided transform. |
| 5 | Delete missing actor returns error | `{"action":"delete","actorName":"MCP_Cube_Missing"}` | Error message notes that the actor could not be found. |

---

## Asset Tools (`src/tools/assets.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | List root content directory | `{"action":"list","directory":"/Game"}` | Success response enumerates `/Game` children. |
| 2 | Create material asset in MCP folder | `{"action":"create_material","name":"MCP_TestMaterial","path":"/Game/MCP/Materials"}` | Success response states that the material was created. |
| 3 | Import FBX with missing source file | `{"action":"import","sourcePath":"C:/Invalid/path/to/model.fbx","destinationPath":"/Game/MCP/Imports"}` | Error message explains that the source file could not be imported. |
| 4 | Create material in read-only engine folder | `{"action":"create_material","name":"MCP_ReadOnly","path":"/Engine/Restricted"}` | Failure response notes the destination path is read-only and nothing is written. |

---

## Animation Tools (`src/tools/animation.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Create animation blueprint with missing skeleton fails | `{"action":"create_animation_bp","name":"ABP_MCP_Test","skeletonPath":"/Game/MCP/NotReal.SK_NotReal"}` | Error message explains that the skeleton asset is missing. |
| 2 | Play montage on missing actor fails gracefully | `{"action":"play_montage","actorName":"MissingActor","montagePath":"/Game/MCP/Anims/MT_Missing"}` | Error response notes that the actor was not found. |
| 3 | Setup ragdoll on missing actor fails | `{"action":"setup_ragdoll","actorName":"MissingActor","physicsAssetName":"/Game/MCP/Physics/PH_Missing"}` | Error response states that the actor could not be located for ragdoll setup. |
| 4 | Create animation blueprint using engine skeletal cube | `{"action":"create_animation_bp","name":"ABP_MCP_EngineCube","skeletonPath":"/Engine/EngineMeshes/SkeletalCube.SkeletalCube","savePath":"/Game/MCP/Anims"}` | Failure response explains the referenced skeleton asset is invalid. |

---

## Blueprint Tools (`src/tools/blueprint.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Create actor blueprint | `{"action":"create","name":"BP_Test","blueprintType":"Actor","savePath":"/Game/Blueprints"}` | Asset created at path. |
| 2 | Create blueprint invalid parent | `{"action":"create","name":"BP_InvalidParent","blueprintType":"Actor","savePath":"/Game/Blueprints","parentClass":"/Script/NonExisting"}` | Failure message. |
| 3 | Create blueprint whitespace name | `{"action":"create","name":"   BP Hello   ","blueprintType":"Actor","savePath":"/Game/Blueprints"}` | Name sanitized/trimmed before creation. |
| 4 | Create blueprint missing path | `{"action":"create","name":"BP_DefaultPath","blueprintType":"Actor"}` | Tool defaults to `/Game/Blueprints`; success. |
| 5 | Create blueprint reserved keyword | `{"action":"create","name":"class","blueprintType":"Actor","savePath":"/Game/Blueprints"}` | Name adjusted to `class_Asset`. |
| 6 | Create blueprint path with invalid characters | `{"action":"create","name":"BP_BadPath","blueprintType":"Actor","savePath":"/Game/Bad:Path"}` | Path sanitized before asset creation. |
| 7 | Create blueprint when folder locked | `{"action":"create","name":"BP_ReadOnly","blueprintType":"Actor","savePath":"/Engine/Blueprints"}` | Failure due to write protection. |
| 8 | Add component to blueprint | `{"action":"add_component","name":"BP_Test","componentType":"StaticMeshComponent","componentName":"MeshComp"}` | Component added; compile success. |
| 9 | Add component to missing blueprint fails | `{"action":"add_component","name":"BP_Missing","componentType":"StaticMeshComponent","componentName":"MeshComp"}` | Error message indicates that the blueprint asset was not found. |
| 10 | Add component with invalid component type fails | `{"action":"add_component","name":"BP_Test","componentType":"InvalidComponent","componentName":"BadComp"}` | Error message references the missing component class. |

Additional advanced blueprint scenarios remain documented in the legacy matrix for manual execution.

---

## Material Tools (`src/tools/materials.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Create material asset at custom path | `{"name":"MCP_TestMaterial","path":"/Game/MCP/Materials"}` | Success response states that the material was created. |
| 2 | Create material with invalid characters fails | `{"name":"MCP Material","path":"/Game/MCP/Materials"}` | Error message highlights the invalid material name. |
| 3 | Create material in read-only engine folder fails | `{"name":"MCP_ReadOnly","path":"/Engine/Restricted"}` | Error explains that the destination path is read-only. |
| 4 | Create material with empty name fails validation | `{"name":"   ","path":"/Game/MCP/Materials"}` | Error response states that the material name cannot be empty. |
| 5 | Create material outside /Game or /Engine is rejected | `{"name":"MCP_InvalidPath","path":"/Projects/Test"}` | Error explains that the path must start with `/Game` or `/Engine`. |
| 6 | Material name exceeding 100 characters fails | `{"name":"MCP_ThisNameIsWayTooLongForMaterialCreationBecauseItExceedsTheAllowedCharacterLimitWhichIs100Characters_Total","path":"/Game/MCP/Materials"}` | Error message notes that the material name length is above the 100 character limit. |

---

## Niagara Tools (`src/tools/niagara.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Spawn debug sphere via Niagara tool | `{"action":"debug_shape","shape":"Sphere","location":{"x":0,"y":0,"z":200},"size":100,"color":[0,0.5,1,1],"duration":2}` | Success response confirms the debug shape draw. |
| 2 | Spawn Niagara with missing asset fails | `{"action":"niagara","systemPath":"/Game/MCP/FX/NS_Missing"}` | Error response reports that the Niagara system could not be found. |
| 3 | Spawn Niagara particle preset | `{"action":"particle","effectType":"Fire","name":"MCP_Fire","location":{"x":0,"y":0,"z":0},"scale":1.0}` | Success response notes the particle preset launch or simulated playback. |
| 4 | Niagara particle invalid effect type fails | `{"action":"particle","effectType":"NotReal","name":"MCP_Bad"}` | Error message explains the requested preset or system is unavailable. |
| 5 | Niagara particle rejects non-string effect type | `{"action":"particle","effectType":123,"name":"MCP_InvalidType_Number","location":{"x":0,"y":0,"z":0}}` | Error explicitly states the effect type is invalid. |

---

## Level Tools (`src/tools/level.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Save current level to ensure baseline | `{"action":"save"}` | Success response confirms the level save. |
| 2 | Stream missing level returns error | `{"action":"stream","levelName":"NotARealSubLevel","shouldBeLoaded":true,"shouldBeVisible":true}` | Error message explains that the sub-level was not found. |
| 3 | Load level missing path parameter fails | `{"action":"load"}` | Error message states that `levelPath` is required. |
| 4 | Load non-existent level returns error | `{"action":"load","levelPath":"/Game/Maps/NotReal"}` | Error response reports that the level could not be loaded. |
| 5 | Stream level unload invalid name | `{"action":"stream","levelName":"AnotherMissingSubLevel","shouldBeLoaded":false,"shouldBeVisible":false}` | Error response notes that the streaming level name could not be resolved. |

---

## Sequence Tools (`src/tools/sequence.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Create sequence asset for MCP automation | `{"action":"create","name":"Seq_MCP_Test","path":"/Game/MCP/Sequences"}` | Success response reports the sequence creation. |
| 2 | Add spawnable camera to MCP sequence | `{"action":"add_camera","path":"/Game/MCP/Sequences/Seq_MCP_Test","spawnable":true}` | Success response notes the new camera binding. |
| 3 | Open missing sequence returns error | `{"action":"open","path":"/Game/MCP/Sequences/Missing"}` | Error message indicates the sequence asset was not found. |
| 4 | Add actor binding with Sequencer disabled | `{"action":"add_actor","path":"/Game/MCP/Sequences/Seq_MCP_Test","actorName":"Cube_1"}` | Failure response warns that required Sequencer plugins are disabled. |
| 5 | Play sequence with loop mode pingpong | `{"action":"play","loopMode":"pingpong"}` | Success response includes `loopMode` confirmation. |
| 6 | Pause sequence when Sequencer disabled | `{"action":"pause"}` | Failure response notes missing Sequencer plugins. |
| 7 | Set sequence properties frame rate and length | `{"action":"set_properties","path":"/Game/MCP/Sequences/Seq_MCP_Test","frameRate":30,"lengthInFrames":480}` | Success response reports updated properties. |
| 8 | Get sequence bindings without focused sequence | `{"action":"get_bindings"}` | Failure response states no sequence is currently focused. |

---

## UI Tools (`src/tools/ui.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Create HUD widget through system control | `{"action":"create_widget","widgetName":"MCP_Widget","widgetType":"HUD"}` | Success response confirms widget creation. |
| 2 | Show widget that does not exist fails | `{"action":"show_widget","widgetName":"Widget_Does_Not_Exist","visible":true}` | Error message states that the widget was not found. |
| 3 | Create widget invalid type fails validation | `{"action":"create_widget","widgetName":"Bad_Widget","widgetType":42}` | Error message highlights that the widget type must be a string. |
| 4 | Show widget without providing a name fails | `{"action":"show_widget","visible":true}` | Error response indicates `widgetName` is required. |

---

## Physics Tools (`src/tools/physics.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Apply force to missing actor returns error | `{"action":"apply_force","actorName":"PhysicsBox","force":{"x":0,"y":0,"z":5000}}` | Error response indicates the actor was not found. |
| 2 | Apply force with invalid vector fails validation | `{"action":"apply_force","actorName":"PhysicsBox","force":{"x":"invalid"}}` | Error message reports the invalid force vector. |
| 3 | Apply force with missing actor name fails validation | `{"action":"apply_force","force":{"x":0,"y":0,"z":2000}}` | Error message highlights that the actor name parameter is required. |

---

## Landscape Tools (`src/tools/landscape.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Create landscape actor for MCP tests | `{"action":"create_landscape","name":"Landscape_MCP","sizeX":256,"sizeY":256}` | Success response confirms landscape creation. |
| 2 | Create landscape with negative size fails | `{"action":"create_landscape","name":"Landscape_Invalid","sizeX":-1,"sizeY":256}` | Error message highlights the invalid landscape size. |
| 3 | Create small landscape with defaults | `{"action":"create_landscape","name":"Landscape_Small"}` | Success response reports that a landscape actor was created using default size. |
| 4 | Landscape sculpt reports editor-only limitation | `{"action":"sculpt","name":"Landscape_MCP","tool":"Sculpt"}` | Error message states sculpting is not implemented via Remote Control. |

---

## Build Environment Tools (`src/tools/build_environment_advanced.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Add foliage type with invalid mesh path fails | `{"action":"add_foliage","name":"InvalidFoliage","meshPath":"undefined"}` | Error message reports that the foliage mesh path is invalid. |
| 2 | Create procedural foliage without bounds fails | `{"action":"create_procedural_foliage","name":"ProceduralTest"}` | Error response states that `bounds.location` and `bounds.size` must be provided. |
| 3 | Add foliage instances with missing asset fails | `{"action":"add_foliage_instances","foliageType":"/Game/Foliage/Types/FT_Missing","transforms":[{"location":[0,0,0]}]}` | Error message notes that the foliage asset could not be loaded. |

---

## Performance Tools (`src/tools/performance.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Enable FPS overlay | `{"action":"show_fps","enabled":true}` | Success response confirms the FPS overlay toggle. |
| 2 | Set shadow quality to medium | `{"action":"set_quality","category":"Shadows","level":2}` | Success response reports the updated shadow quality. |
| 3 | Set shadow quality with invalid level fails | `{"action":"set_quality","category":"Shadows","level":7}` | Error message explains that the level is out of range. |
| 4 | Start CPU profiling session | `{"action":"profile","profileType":"CPU"}` | Success response states that CPU profiling started. |

---

## System Control Tools (`src/tools/consolidated-tool-handlers.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Trigger screenshot capture with defaults | `{"action":"screenshot"}` | Success response notes that a screenshot command was issued. |
| 2 | Play sound with missing asset fails | `{"action":"play_sound","soundPath":"/Game/MCP/Audio/Missing"}` | Error message reports that the sound asset could not be found. |
| 3 | Play sound at location missing asset fails | `{"action":"play_sound","soundPath":"/Game/MCP/Audio/Missing","location":{"x":0,"y":0,"z":0}}` | Error message reports that the sound asset could not be found for the world location playback. |
| 4 | Play sound with empty path fails validation | `{"action":"play_sound","soundPath":"","volume":1.0}` | Error message explains that the sound asset path could not be resolved. |
| 5 | Read Output Log tail with defaults | `{"action":"read_log","lines":50,"filter_level":"All"}` | Success response returns recent log entries and the logPath used. |
| 6 | Read Output Log filtered by custom categories | `{"action":"read_log","filter_category":"LogMyCategory,LogOtherLog","filter_level":"Error","lines":100}` | Success response returns only matching categories at Error level. |

---

## Debug Tools (`src/tools/debug.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Draw debug sphere in front of camera | `{"action":"debug_shape","shape":"Sphere","location":{"x":0,"y":0,"z":100},"size":50,"color":[1,0,0,1],"duration":3}` | Success response notes the debug sphere render. |
| 2 | Debug shape rejects unknown primitive | `{"action":"debug_shape","shape":"Hexagon","location":{"x":0,"y":0,"z":0},"size":50}` | Error message states that the shape is unsupported. |

---

## Remote Control Preset Tools (`src/tools/rc.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Create RC preset | `{"action":"create_preset","name":"RCP_Test","path":"/Game/RCPresets"}` | Preset asset created. |
| 2 | Create preset invalid path | `{"action":"create_preset","name":"RCP_Restricted","path":"/Engine/Restricted"}` | Failure message. |
| 3 | Expose actor | `{"action":"expose_actor","presetPath":"/Game/RCPresets/RCP_Test","actorName":"MCP_Cube_B"}` | Actor exposed; success true. |
| 4 | Expose missing actor | `{"action":"expose_actor","presetPath":"/Game/RCPresets/RCP_Test","actorName":"MissingActor"}` | Error message. |
| 5 | List fields empty preset | `{"action":"list_fields","presetPath":"/Game/RCPresets/RCP_Test"}` | Empty array returned. |
| 6 | Delete preset success | `{"action":"delete_preset","presetPath":"/Game/RCPresets/RCP_Test"}` | Asset removed. |

More advanced preset workflows (batch exposure, property mutation, etc.) continue to live in the legacy manual section.

---

## Legacy Comprehensive Matrix (manual + advanced scenarios)

The following tables mirror the full scenario list that previously lived in this file. They remain valuable for manual validation and for expanding automated coverage over time.

## Lighting Tools (`src/tools/lighting.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Spawn directional light with defaults | `{"name":"Dir_Default"}` | Actor spawns at default location/rotation; log contains "Directional light 'Dir_Default' spawned". |
| 2 | Directional light custom intensity/color | `{"name":"Dir_Custom","intensity":50,"color":[0.2,0.4,0.8]}` | Light component intensity = 50 and color matches payload. |
| 3 | Reject negative intensity | `{"name":"Dir_Bad","intensity":-10}` | API throws validation error before Python execution. |
| 4 | Reject non-numeric color | `{"name":"Dir_Color","color":["a",0,0]}` | Validation error citing invalid color component. |
| 5 | Apply custom rotation | `{"name":"Dir_Rot","rotation":[45,90,0]}` | Actor rotation updated; log still reports success. |
| 6 | Spawn point light defaults | `{"name":"Point_Default"}` | Point light at origin with default radius/intensity. |
| 7 | Point light custom location & radius | `{"name":"Point_Custom","location":[100,0,400],"radius":500}` | Light positioned as requested; attenuation radius = 500. |
| 8 | Point light invalid radius | `{"name":"Point_Bad","radius":-1}` | Validation error referencing radius. |
| 9 | Point light custom falloff exponent | `{"name":"Point_Falloff","falloffExponent":4}` | Component falloff exponent set to 4. |
| 10 | Spot light with cone values | `{"name":"Spot_Cone","location":[0,0,0],"rotation":[0,0,0],"innerCone":10,"outerCone":25}` | Spot light spawns; component cones reflect payload. |
| 11 | Spot light inner cone > outer cone | `{"name":"Spot_ConeSwap","location":[0,0,0],"rotation":[0,0,0],"innerCone":45,"outerCone":30}` | Validation error or engine warning (record behavior). |
| 12 | Spot light outer cone >180 | `{"name":"Spot_BadCone","location":[0,0,0],"rotation":[0,0,0],"outerCone":200}` | Validation error (must be ≤180). |
| 13 | Rect light width/height | `{"name":"Rect_Custom","location":[0,0,0],"rotation":[0,0,0],"width":200,"height":50}` | Rect light component width=200 height=50. |
| 14 | Rect light invalid width | `{"name":"Rect_Bad","location":[0,0,0],"rotation":[0,0,0],"width":0}` | Validation error for positive width. |
| 15 | Sky light reuse existing | Call twice `{"name":"Sky_Unique"}` | First call spawns/labels actor; second ensures reuse (no duplicates, log indicates success). |
| 16 | Sky light with cubemap path | `{"name":"Sky_Cubemap","sourceType":"SpecifiedCubemap","cubemapPath":"/Game/Textures/T_Sky"}` | Component cubemap set if asset exists; recapture executed. |
| 17 | Sky light missing cubemap asset | Same as above with invalid path | Tool returns failure with error referencing asset lookup. |
| 18 | Ensure single sky light default | `ensureSingleSkyLight()` | Only one SkyLight remains, renamed `MCP_Test_Sky`; logs removed count. |
| 19 | Ensure single sky light custom trimmed name | `{"name":"  CustomSky  "}` | Result renames to `CustomSky`; trimmed input accepted. |
| 20 | Create lightmass volume valid | `{"name":"LM_Volume","location":[0,0,0],"size":[1000,800,600]}` | Actor spawns; scale approx size; log success. |
| 21 | Create lightmass volume blank name | `{"name":"   ","location":[0,0,0],"size":[100,100,100]}` | Normalization throws "Invalid name" error. |
| 22 | Create lightmass volume negative size | `{"name":"LM_Bad","location":[0,0,0],"size":[-10,100,100]}` | Python logs failure; tool surfaces error message. |

---

## Actor Tools (`src/tools/actors.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Spawn cube actor for MCP smoke test | `{"action":"spawn","classPath":"/Engine/BasicShapes/Cube","name":"MCP_Cube_A"}` | Success response indicates the actor spawned. |
| 2 | Delete previously spawned actor | `{"action":"delete","actorName":"MCP_Cube_A"}` | Success response reports the actor delete. |
| 3 | Spawn without class path returns validation error | `{"action":"spawn"}` | Error message states that `classPath` is required. |
| 4 | Apply force to missing actor returns error | `{"action":"apply_force","actorName":"PhysicsBox","force":{"x":0,"y":0,"z":5000}}` | Error response indicates the actor was not found. |
| 5 | Apply force with invalid vector fails validation | `{"action":"apply_force","actorName":"PhysicsBox","force":{"x":"invalid"}}` | Error message reports the invalid force vector. |

---

## Editor Tools (`src/tools/editor.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Set viewport to wireframe mode | `{"action":"set_view_mode","viewMode":"Wireframe"}` | Success response confirms the view mode change. |
| 2 | Unsafe view mode is rejected | `{"action":"set_view_mode","viewMode":"BaseColor"}` | Error message explains the requested view mode is blocked for safety. |
| 3 | Position viewport camera above origin | `{"action":"set_camera","location":{"x":0,"y":0,"z":1000},"rotation":{"pitch":-45,"yaw":0,"roll":0}}` | Success response notes that the camera was updated. |
| 4 | Reject non-positive editor game speed | `{"action":"set_game_speed","speed":0}` | Error message states that the speed must be a positive number. |

---

## Asset Tools (`src/tools/assets.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | List root content directory | `{"action":"list","directory":"/Game"}` | Success response enumerates `/Game` children. |
| 2 | Create material asset in MCP folder | `{"action":"create_material","name":"MCP_TestMaterial","path":"/Game/MCP/Materials"}` | Success response states that the material was created. |
| 3 | Import FBX with missing source file | `{"action":"import","sourcePath":"C:/Invalid/path/to/model.fbx","destinationPath":"/Game/MCP/Imports"}` | Error message explains that the source file could not be imported. |
| 4 | Create material in read-only engine folder | `{"action":"create_material","name":"MCP_ReadOnly","path":"/Engine/Restricted"}` | Error explains the destination path is read-only and the asset is not created. |

---

## Animation Tools (`src/tools/animation.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Create animation blueprint with missing skeleton fails | `{"action":"create_animation_bp","name":"ABP_MCP_Test","skeletonPath":"/Game/MCP/NotReal.SK_NotReal"}` | Error message explains that the skeleton asset is missing. |
| 2 | Play montage on missing actor fails gracefully | `{"action":"play_montage","actorName":"MissingActor","montagePath":"/Game/MCP/Anims/MT_Missing"}` | Error response notes that the actor was not found. |
| 3 | Setup ragdoll on missing actor returns error | `{"action":"setup_ragdoll","actorName":"MissingActor","physicsAssetName":"/Game/MCP/Physics/PH_Missing"}` | Error response indicates that the actor could not be located. |
| 4 | Create animation blueprint using engine skeletal cube | `{"action":"create_animation_bp","name":"ABP_MCP_EngineCube","skeletonPath":"/Engine/EngineMeshes/SkeletalCube.SkeletalCube","savePath":"/Game/MCP/Anims"}` | Success response reports the created animation blueprint path. |

---

## Blueprint Tools (`src/tools/blueprint.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Create actor blueprint | `{"name":"BP_Test","parentClass":"Actor","path":"/Game/Blueprints"}` | Asset created at path. |
| 2 | Create blueprint invalid parent | `parentClass":"/Script/NonExisting"` | Failure message. |
| 3 | Create blueprint whitespace name | `"   BP Hello   "` | Name sanitized/trimmed before creation. |
| 4 | Create blueprint missing path | Path omitted | Tool defaults to `/Game/Blueprints`; success. |
| 5 | Create blueprint reserved keyword | `name":"class"` | Name adjusted to `class_Asset`. |
| 6 | Create blueprint path with invalid characters | `path":"/Game/Bad:Path"` | Path sanitized before asset creation. |
| 7 | Create blueprint when folder locked | e.g., `/Engine/Blueprints` | Failure due to write protection. |
| 8 | Add component to blueprint | `{"blueprintPath":"/Game/Blueprints/BP_Test","componentType":"StaticMeshComponent","componentName":"MeshComp"}` | Component added; compile success. |
| 9 | Add component duplicate name | Same payload twice | Name deduped (MeshComp_1) or error; document behavior. |
| 10 | Add component invalid blueprint path | Non-existent asset | Operation fails with error. |
| 11 | Add component invalid type | `componentType":"InvalidComponent"` | Error referencing missing class. |
| 12 | Add component empty name | `componentName":"   "` | Tool sanitizes or auto-generates name. |
| 13 | Create blueprint with interfaces | Provide interface list in payload | Interfaces implemented; blueprint compiles. |
| 14 | Create blueprint while asset exists (overwrite) | Provide same name & path | Tool either reuses or fails; note result. |
| 15 | Delete blueprint after test (via asset tool) | Confirm removal. |
| 16 | Create blueprint with metadata tags | `metadata":{"Category":"Test"}` | Tags applied (verify in asset). |
| 17 | Add component when blueprint already open (locked) | Operation fails gracefully. |
| 18 | Create blueprint in nested folders | `path":"/Game/Blueprints/SubFolder"` | Folders auto-created; asset saved. |
| 19 | Create blueprint extremely long name | >64 characters | Name truncated; creation success. |
| 20 | Create blueprint with unicode characters | Name includes Unicode | Ensure sanitized but retains valid segments. |
| 21 | Verify logs contain `RESULT` JSON for automation parsing. |

---

## Material Tools (`src/tools/materials.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Create material asset at custom path | `{"name":"MCP_TestMaterial","path":"/Game/MCP/Materials"}` | Success response states that the material was created. |
| 2 | Create material with invalid characters fails | `{"name":"MCP Material","path":"/Game/MCP/Materials"}` | Error message highlights the invalid material name. |
| 3 | Create material in read-only engine folder fails | `{"name":"MCP_ReadOnly","path":"/Engine/Restricted"}` | Error explains that the destination path is read-only. |

---

## Niagara Tools (`src/tools/niagara.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Spawn debug sphere via Niagara tool | `{"action":"debug_shape","shape":"Sphere","location":{"x":0,"y":0,"z":200},"size":100,"color":[0,0.5,1,1],"duration":2}` | Success response confirms the debug shape draw. |
| 2 | Spawn Niagara with missing asset fails | `{"action":"niagara","systemPath":"/Game/MCP/FX/NS_Missing"}` | Error response reports that the Niagara system could not be found. |
| 3 | Create particle preset effect | `{"action":"particle","effectType":"Fire","name":"MCP_Fire","location":{"x":0,"y":0,"z":0},"scale":1.0}` | Success response notes that the effect asset was created. |

---

## Level Tools (`src/tools/level.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Load level valid | `{"action":"load","levelPath":"/Game/Maps/Test"}` | Level loads without errors. |
| 2 | Load level missing | Path invalid | Failure message. |
| 3 | Save current level | `{"action":"save"}` | Save completes; log success. |
| 4 | Save dirty level locked | Level read-only | Failure message referencing inability to save. |
| 5 | Stream level in visible | `{"action":"stream","levelName":"SubLevel","shouldBeLoaded":true,"shouldBeVisible":true}` | Sublevel loads and becomes visible. |
| 6 | Stream level out | Same but `shouldBeLoaded:false` | Sublevel unloaded; duplicates removed. |
| 7 | Stream invalid level name | Error. |
| 8 | Stream visible false but loaded true | `shouldBeVisible:false` | Level stays loaded but hidden. |
| 9 | Load level while PIE active | Ensure command succeeds or warns. |
| 10 | Load same level twice | Idempotent; no reload error. |
| 11 | Save with custom path (if parameter) | Provide `targetPath` | Save as new level; confirm asset. |
| 12 | Build lighting via tool | `{"action":"build_lighting","quality":"High"}` | Build triggered; log indicates start. |
| 13 | Build lighting invalid quality | `quality":"Ultra"` | Validation error. |
| 14 | Load level with relative path | `"Maps/Test"` | Auto-prepends `/Game/`. |
| 15 | Stream level while already loaded | Should handle gracefully; no duplicates. |
| 16 | Save level with no changes | Operation success; message indicates saved or no action. |
| 17 | Load level with uppercase path | Works (case-insensitive). |
| 18 | Attempt to save when editor lacks permissions | Error from UE log. |
| 19 | Stream level during asset load | Check concurrency; expect success. |
| 20 | Save multiple times sequentially | All succeed without stale state. |
| 21 | Validate response includes sanitized message for automation. |

---

## Sequence Tools (`src/tools/sequence.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Create new sequence asset | `{"action":"create","name":"Seq_Test","path":"/Game/Sequences"}` | Sequence created; file path returned. |
| 2 | Create sequence duplicate name | Same payload twice | Second call handles collision (suffix or error). |
| 3 | Open existing sequence | `{"action":"open","path":"/Game/Sequences/Seq_Test"}` | Sequence opened successfully. |
| 4 | Open missing sequence | Invalid path | Failure message. |
| 5 | Add camera spawnable | `{"action":"add_camera","path":"/Game/Sequences/Seq_Test","spawnable":true}` | Camera binding added; returns binding id. |
| 6 | Add actor possessable | `{"action":"add_actor","path":"/Game/Sequences/Seq_Test","actorName":"Cube_1"}` | Actor bound; success message. |
| 7 | Add multiple actors | `add_actors` list | All actors bound; result enumerates ids. |
| 8 | Remove actor binding | `{"action":"remove_actors","path":"...","actorNames":["Cube_1"]}` | Binding removed. |
| 9 | Set sequence properties | `{"action":"set_properties","path":"...","frameRate":24,"lengthInFrames":240}` | Properties updated. |
| 10 | Play sequence once | `{"action":"play","path":"...","loopMode":"once"}` | Sequence plays; log indicates playback start. |
| 11 | Play loop mode | `loopMode":"loop"` | Sequence loops; confirm log. |
| 12 | Pause sequence | `{"action":"pause","path":"..."}` | Playback paused; state recorded. |
| 13 | Stop sequence | `{"action":"stop","path":"..."}` | Playback stops; time resets. |
| 14 | Set playback speed >1 | `{"action":"set_playback_speed","path":"...","speed":2}` | Plays faster; log success. |
| 15 | Set playback speed invalid | Speed ≤0 | Validation error. |
| 16 | Play sequence missing path | Path omitted | Validation error. |
| 17 | Add camera when path invalid | Failure message. |
| 18 | Pause sequence not playing | Should handle gracefully (no crash). |
| 19 | Get properties | `{"action":"get_properties","path":"..."}` | Returns frame rate, length, playback window. |
| 20 | Set playback range beyond length | e.g., `playbackEnd":500` when length 240 | Tool accepts or warns; record behavior. |
| 21 | Delete sequence asset after tests | Use asset tool; ensure no residuals. |

---

## UI Tools (`src/tools/ui.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Create 3D widget default class | `{"action":"create_widget","widgetType":"HUD","name":"Widget_Test"}` | Widget spawned; handle returned. |
| 2 | Create widget invalid class | Non-existent widget blueprint path | Error message. |
| 3 | Show widget | `{"action":"show_widget","widgetName":"Widget_Test","visible":true}` | Widget becomes visible in viewport. |
| 4 | Hide widget | Same with `visible:false` | Widget hidden. |
| 5 | Create widget with location/rotation | Provide transform fields | Widget placed accordingly. |
| 6 | Create widget whitespace name | Name sanitized; unique identifier returned. |
| 7 | Create multiple widgets same base name | Ensure unique suffix appended. |
| 8 | Create widget invalid parameters (non-string) | e.g., `widgetType:42` | Validation error. |
| 9 | Show non-existent widget | Failure message. |
| 10 | Destroy widget (if action) | `{"action":"destroy_widget","widgetName":"Widget_Test"}` | Widget removed. |
| 11 | Attach widget to actor | Provide `attachToActor` field | Widget follows actor transform. |
| 12 | Update widget content (if action) | e.g., set text | Change reflected in UI. |
| 13 | Create widget while Slate disabled | Error referencing subsystem. |
| 14 | Create widget with long name | Name truncated but functional. |
| 15 | Batch create 20 widgets | Ensure runtime handles count; check for perf issues. |
| 16 | Create widget during PIE | Works in game viewport; record. |
| 17 | Show widget already visible | Idempotent; no error. |
| 18 | Hide widget already hidden | Idempotent. |
| 19 | Create widget with custom blueprint requiring constructor args | Tool passes defaults or errors; record behavior. |
| 20 | Clean up all widgets (loop destroy) | Environment returns to baseline. |
| 21 | Validate log output uses consistent `RESULT` structure. |

---

## Physics Tools (`src/tools/physics.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Apply impulse | `{"action":"applyImpulse","actorName":"PhysicsBox","vector":[0,0,5000]}` | Actor moves upward; success message. |
| 2 | Apply impulse invalid actor | Non-existent name | Failure message. |
| 3 | Apply impulse invalid vector | `[0,"a",0]` | Validation error. |
| 4 | Apply radial force | `{"action":"applyRadialForce","origin":[0,0,0],"radius":500,"strength":10000}` | Nearby actors affected. |
| 5 | Apply radial force negative radius | `radius":"-10"` | Validation error. |
| 6 | Toggle ragdoll on skeletal actor | `{"action":"enableRagdoll","actorName":"SkeletalActor"}` | Actor enters ragdoll; success log. |
| 7 | Disable ragdoll | `{"action":"disableRagdoll","actorName":"SkeletalActor"}` | Actor returns to default pose. |
| 8 | Enable ragdoll missing physics asset | Actor lacking asset | Failure message indicates requirement. |
| 9 | Set mass override | `{"action":"setMass","actorName":"PhysicsBox","mass":50}` | Mass property updated. |
| 10 | Set mass invalid value | Negative mass -> validation error. |
| 11 | Toggle gravity off | `{"action":"setGravity","actorName":"PhysicsBox","enabled":false}` | Actor no longer affected by gravity. |
| 12 | Toggle gravity on | Reverse of above | Gravity restored. |
| 13 | Apply torque | `{"action":"applyTorque","actorName":"PhysicsBox","vector":[0,1000,0]}` | Actor rotates; success message. |
| 14 | Apply physics to actor without sim | Non-physics actor -> error. |
| 15 | Apply impulse during PIE | Works as expected; record logs. |
| 16 | Apply radial force to large actor count | Stress test; ensure no crash. |
| 17 | Reset physics state | `{"action":"resetPhysics","actorName":"PhysicsBox"}` | Actor returns to initial state. |
| 18 | Apply damping settings | Provide `linearDamping`, `angularDamping` | Values updated. |
| 19 | Apply force when subsystem unavailable | Force tool handles gracefully. |
| 20 | Cleanup: disable ragdoll, restore gravity | System returns to baseline. |
| 21 | Validate `warnings` field when engine emits warnings (e.g., on constraints). |

---

## Landscape Tools (`src/tools/landscape.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Create landscape basic | `{"action":"create_landscape","name":"Landscape_Test","sizeX":1024,"sizeY":1024}` | Landscape actor created; log success. |
| 2 | Create landscape invalid size | `sizeX:-1` | Validation error. |
| 3 | Sculpt raise region | `{"action":"sculpt","tool":"Sculpt","location":{"x":0,"y":0,"z":0},"strength":0.5,"brushSize":500}` | Terrain height increases. |
| 4 | Sculpt smooth | `tool":"Smooth"` | Terrain smoothed. |
| 5 | Sculpt invalid tool name | `tool":"Invalid"` | Error referencing tool. |
| 6 | Add foliage asset | `{"action":"add_foliage","name":"FT_Grass","meshPath":"/Game/Foliage/SM_Grass","density":300}` | Foliage type asset created/registered. |
| 7 | Add foliage invalid mesh path | Non-existent asset | Error. |
| 8 | Paint foliage | `{"action":"paint_foliage","foliageType":"/Game/Foliage/FT_Grass","position":{"x":0,"y":0,"z":0},"brushSize":300}` | Instances placed; visible in viewport. |
| 9 | Paint foliage invalid type | Wrong path | Failure message. |
| 10 | Create procedural terrain | `{"action":"create_procedural_terrain","name":"AutoLandscape","bounds":{"location":{"x":0,"y":0,"z":0},"size":{"x":5000,"y":5000,"z":1000}}}` | Procedural landscape generated (if supported). |
| 11 | Create procedural foliage | Provide `foliageTypes` array | Instances spawn automatically. |
| 12 | Add foliage instances direct | Provide transforms | Instances appear. |
| 13 | Add foliage invalid density | Negative value -> validation error. |
| 14 | Paint foliage outside bounds | Provide far position | Either clamps or records warning. |
| 15 | Create landscape grass type | `{"action":"create_landscape_grass_type","name":"LG_Grass","foliageTypes":[{"meshPath":"/Game/Foliage/SM_Grass","density":200}]}` | Grass type asset created. |
| 16 | Sculpt while PIE active | Confirm tool works or warns. |
| 17 | Delete or cleanup created landscapes (if action) | Ensure environment reset. |
| 18 | Performance: sculpt with large brush size repeatedly | Record frame impact. |
| 19 | Validate logs include success tags. |
| 20 | Create landscape with material assignment | Provide `materialPath` in payload | Material applied. |
| 21 | Attempt operations when landscape subsystem missing | Error captured. |

---

## Performance Tools (`src/tools/performance.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Show FPS | `{"action":"show_fps","enabled":true}` | `stat fps` overlay enabled. |
| 2 | Hide FPS | Same with `enabled:false` | Overlay hidden. |
| 3 | Start CPU profile | `{"action":"profile","profileType":"CPU","enabled":true}` | Profiling begins; log message. |
| 4 | Stop CPU profile | `enabled:false` | Profiling stops. |
| 5 | Invalid profile type | `profileType":"RAM"` | Validation error. |
| 6 | Set quality shadows | `{"action":"set_quality","category":"Shadows","level":2}` | Console var `sg.ShadowQuality` set to 2. |
| 7 | Set quality invalid category | Unknown category -> error. |
| 8 | Set quality invalid level | `level":6` | Validation error. |
| 9 | Play 3D sound | `{"action":"play_sound","soundPath":"/Game/Audio/SFX/Click","location":{"x":0,"y":0,"z":0},"volume":0.5,"is3D":true}` | Sound plays; log success. |
| 10 | Play sound invalid asset | Wrong path -> error. |
| 11 | Play sound without location (2D) | Omit location; `is3D:false` | Plays as 2D sound. |
| 12 | Create HUD widget | `{"action":"create_widget","widgetName":"PerfHUD","widgetType":"HUD"}` | Widget created (integration with UI tool). |
| 13 | Show widget invalid name | `show_widget` on missing | Failure. |
| 14 | Capture screenshot | `{"action":"screenshot","resolution":"1920x1080"}` | Screenshot saved; file path logged. |
| 15 | Screenshot invalid resolution | `resolution":"abc"` | Validation error. |
| 16 | Engine start without project path | `{"action":"engine_start"}` missing path | Error requiring configuration. |
| 17 | Engine quit | `{"action":"engine_quit"}` | Unreal attempts to quit; confirm connection handling. |
| 18 | Toggle verbose stats | `{"action":"profile","profileType":"CPU","verbose":true}` | Verbosity change recorded. |
| 19 | Set quality sequentially | Issue commands level 0–4 | Each succeeds; verify logs. |
| 20 | Play sound with volume >1 | `volume":1.5` | Clamped or validation error; record. |
| 21 | Show FPS when already enabled | Idempotent success. |

---

## Debug Tools (`src/tools/debug.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Draw debug line | `{"shape":"Line","start":{"x":0,"y":0,"z":0},"end":{"x":0,"y":500,"z":0},"color":[1,0,0,1],"duration":5}` | Red line visible for 5 seconds. |
| 2 | Draw debug box | `{"shape":"Box","center":{"x":0,"y":0,"z":100},"extent":100,"color":[0,1,0,1],"duration":5}` | Green box rendered. |
| 3 | Draw debug sphere | `{"shape":"Sphere","center":{"x":0,"y":0,"z":100},"radius":100,"color":[0,0,1,1]}` | Blue sphere visible. |
| 4 | Draw debug cylinder | Provide top/bottom radius | Cylinder rendered correctly. |
| 5 | Draw debug arrow | `start` and `end` vectors | Arrow drawn pointing toward end. |
| 6 | Draw debug text | `{"shape":"Text","location":{"x":0,"y":0,"z":200},"text":"Hello Unreal","color":[1,1,0,1]}` | Text appears; properly escaped. |
| 7 | Draw debug text with quotes | Text contains `'"` | Escapes; displays correctly. |
| 8 | Draw invalid shape | `shape":"Hexagon"` | Validation error referencing allowed shapes. |
| 9 | Draw with invalid color values | `color":[2,-1,0,1]` | Values clamped or validation error. |
| 10 | Draw line zero duration | `duration":0` | Line visible for single frame; no crash. |
| 11 | Draw persistent line | `duration":-1` (if supported) | Persistent overlay until cleared. |
| 12 | Clear debug shapes | `{"action":"clear"}` | All debug drawings removed. |
| 13 | Draw while PIE active | Works both in editor and PIE. |
| 14 | Draw numerous shapes quickly | Stress test; ensure performance acceptable. |
| 15 | Draw shapes with metadata | `meta.action":"debug_draw"` | Metadata recorded; logs show correct action string. |
| 16 | Draw text invalid location | Missing fields -> validation error. |
| 17 | Draw to actor socket (if supported) | Provide `attachToActor` and `socketName` | Shape follows actor. |
| 18 | Draw concurrently from multiple requests | No conflicting state. |
| 19 | Draw using color array length <4 | Validation error. |
| 20 | Draw with negative extent | Error message. |
| 21 | Draw after clearing ensures new shapes display normally. |

---

## Remote Control Preset Tools (`src/tools/rc.ts`)

| # | Scenario | Example Input Payload | Expected Outcome |
|---|----------|-----------------------|------------------|
| 1 | Create RC preset | `{"action":"create_preset","name":"RCP_Test","path":"/Game/RCPresets"}` | Preset asset created. |
| 2 | Create preset invalid path | `/Engine/Restricted` | Failure message. |
| 3 | Expose actor | `{"action":"expose_actor","presetPath":"/Game/RCPresets/RCP_Test","actorName":"MCP_Cube_B"}` | Actor exposed; success true. |
| 4 | Expose missing actor | Actor absent | Error message. |
| 5 | Expose property valid | Provide object path + property | Property exposed. |
| 6 | Expose property invalid | Non-existent property | Error returned. |
| 7 | List fields empty preset | `list_fields` on new preset | Empty array returned. |
| 8 | List fields with entries | After expose operations | Array lists exposed entities. |
| 9 | Set property numeric | Provide float value | Property updated. |
| 10 | Set property invalid type | Send string where number expected | Error returned. |
| 11 | Get property valid | Returns current value. |
| 12 | Get property missing | Non-existent property -> error. |
| 13 | Delete preset success | `delete_preset` call | Asset removed. |
| 14 | Delete preset missing | Non-existent path -> error. |
| 15 | Create preset duplicate | Same name/path | Tool appends suffix or fails; record. |
| 16 | Expose actor while preset locked | Error referencing locked asset. |
| 17 | Set property with vector payload | Provide `{x,y,z}` | Value applied. |
| 18 | Get property while actor missing | Error message. |
| 19 | Batch expose actors | Provide list sequentially | All succeed or partial fail captured. |
| 20 | Toggle property via remote control panel (manual) | Validate UI integration. |
| 21 | Validate RC preset saved to disk | Confirm file in Content Browser. |