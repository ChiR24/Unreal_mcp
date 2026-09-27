/**
 * manage_character records — part 2 of 2 (advanced locomotion, footstep system,
 * crouch/sprint, info). Grounded in manage-character-tool.ts enum and native
 * Character domain. Authoring actions mutate the Character Blueprint asset.
 *
 * As in part 1, each record's optional set is exactly the fields the matching
 * native handler reads; action-specific parameters live in ./character.props.ts.
 */
import type { CapabilityRecordSource } from '../../../model.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { CHARACTER_P as C } from './character.props.js';

const T = 'manage_character';
const F = 'character';

export const CHARACTER_2: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.get_character_info`, action: 'get_character_info', family: F,
    summary: 'Read character Blueprint metadata.', whenToUse: ['Inspect a character.'], whenNotToUse: ['Mutate the character.'],
    inputProps: { blueprintPath: P.blueprintPath }, required: ['blueprintPath'],
    effect: 'read', latency: 'instant', resources: 'low',
    outputProps: {
      blueprintPath: P.blueprintPath,
      assetName: P.string_,
      capsuleRadius: P.num_,
      capsuleHalfHeight: P.num_,
      walkSpeed: P.num_,
      jumpZVelocity: P.num_,
      airControl: P.num_,
      orientToMovement: P.bool_,
      gravityScale: P.num_,
      maxJumpCount: P.num_,
      useControllerRotationYaw: P.bool_,
      hasSpringArm: P.bool_,
      hasCamera: P.bool_,
      playerViewState: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Player camera/view state.' },
      // Additional fields the handler
      // (McpAutomationBridge_CharacterHandlersInfo.cpp) already emits. Anything
      // undeclared is discarded by output projection before the caller sees it.
      customMovementSpeed: P.num_,
      targetArmLength: P.num_,
      usePawnControlRotation: P.bool_,
      enableCameraLag: P.bool_,
      cameraLagSpeed: P.num_,
      fieldOfView: P.num_,
      springArmTemplates: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Spring-arm component templates found on the Blueprint.' },
      cameraTemplates: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Camera component templates found on the Blueprint.' },
      movementVariables: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'CharacterMovement values read from the Blueprint CDO.' },
    }, outputRequired: [],
    exampleInput: { action: 'get_character_info', blueprintPath: '/Game/BP_Char' },
    exampleOutput: { success: true, message: 'Character info', blueprintPath: '/Game/BP_Char', capsuleRadius: 42, hasSpringArm: true } }),
  buildRecord({ parentTool: T, id: `${T}.configure_crouch`, action: 'configure_crouch', family: F,
    summary: 'Configure crouch height/speed.', whenToUse: ['Crouch needed.'], whenNotToUse: ['Use configure_sprint.'],
    inputProps: { blueprintPath: P.blueprintPath, canCrouch: C.canCrouch, crouchSpeed: C.crouchSpeed, crouchedHalfHeight: C.crouchedHalfHeight }, required: ['blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_crouch', blueprintPath: '/Game/BP_Char', canCrouch: true, crouchSpeed: 300 } }),
];
