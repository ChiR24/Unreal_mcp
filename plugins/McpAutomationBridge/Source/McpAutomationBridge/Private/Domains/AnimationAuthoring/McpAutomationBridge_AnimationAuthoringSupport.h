#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"  // MUST be first
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Misc/EngineVersionComparison.h"

// UE 5.0 deprecation warning suppression - BlendSpaceBase.h is deprecated but transitively included by engine headers
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/Skeleton.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
#pragma warning(pop)
#endif
#include "Engine/SkeletalMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/AnimSequenceFactory.h"
#include "Factories/AnimMontageFactory.h"
#include "Factories/AnimBlueprintFactory.h"
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"

// Blend Space factories
#include "Factories/BlendSpaceFactoryNew.h"
#include "Factories/BlendSpaceFactory1D.h"

// Control Rig support (optional module)
#if __has_include("ControlRig.h")
#include "ControlRig.h"
#endif

// Control Rig Blueprint - header location changed in UE 5.5+
// UE 5.5+: ControlRigDeveloper/Public/ControlRigBlueprintLegacy.h
// UE 5.0-5.4: ControlRigBlueprint.h (various locations)
#if __has_include("ControlRigBlueprintLegacy.h")
#include "ControlRigBlueprintLegacy.h"
#define MCP_HAS_CONTROLRIG_BLUEPRINT 1
#elif __has_include("ControlRigBlueprint.h")
#include "ControlRigBlueprint.h"
#define MCP_HAS_CONTROLRIG_BLUEPRINT 1
#else
#define MCP_HAS_CONTROLRIG_BLUEPRINT 0
#endif

// RigVM Blueprint Generated Class (needed for ControlRig creation fallback in UE 5.1-5.4)
#if __has_include("RigVMBlueprintGeneratedClass.h")
#include "RigVMBlueprintGeneratedClass.h"
#endif

// UE 5.0 uses UControlRigBlueprintGeneratedClass (different name from UE 5.1+)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
#include "ControlRigBlueprintGeneratedClass.h"
#endif

// Control Rig Factory (for creating Control Rig assets)
// Note: ControlRigBlueprintFactory header is Public only in UE 5.5+
// For UE 5.1-5.4 we use a fallback implementation
#if MCP_HAS_CONTROLRIG_FACTORY && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
  #include "ControlRigBlueprintFactory.h"
#endif

// IK Rig support (UE 5.0+)
// Header path: Engine/Plugins/Animation/IKRig/Source/IKRig/Public/Rig/IKRigDefinition.h
#if __has_include("Rig/IKRigDefinition.h")
#include "Rig/IKRigDefinition.h"
#define MCP_HAS_IKRIG 1
#elif __has_include("IKRigDefinition.h")
#include "IKRigDefinition.h"
#define MCP_HAS_IKRIG 1
#else
#define MCP_HAS_IKRIG 0
#endif

// IK Rig Factory (for creating IK Rig assets)
#if __has_include("RigEditor/IKRigDefinitionFactory.h")
#include "RigEditor/IKRigDefinitionFactory.h"
#define MCP_HAS_IKRIG_FACTORY 1
#elif __has_include("IKRigDefinitionFactory.h")
#include "IKRigDefinitionFactory.h"
#define MCP_HAS_IKRIG_FACTORY 1
#else
#define MCP_HAS_IKRIG_FACTORY 0
#endif

// IK Retarget Factory
#if __has_include("RetargetEditor/IKRetargetFactory.h")
#include "RetargetEditor/IKRetargetFactory.h"
#define MCP_HAS_IKRETARGET_FACTORY 1
#elif __has_include("IKRetargetFactory.h")
#include "IKRetargetFactory.h"
#define MCP_HAS_IKRETARGET_FACTORY 1
#else
#define MCP_HAS_IKRETARGET_FACTORY 0
#endif

#if __has_include("Retargeter/IKRetargeter.h")
#include "Retargeter/IKRetargeter.h"
#define MCP_HAS_IKRETARGETER 1
#elif __has_include("IKRetargeter.h")
#include "IKRetargeter.h"
#define MCP_HAS_IKRETARGETER 1
#else
#define MCP_HAS_IKRETARGETER 0
#endif

// IK Retargeter Controller (for setting IK Rigs on retargeter)
#if __has_include("RetargetEditor/IKRetargeterController.h")
#include "RetargetEditor/IKRetargeterController.h"
#define MCP_HAS_IKRETARGETER_CONTROLLER 1
#else
#define MCP_HAS_IKRETARGETER_CONTROLLER 0
#endif

// Animation Blueprint Graph
#include "AnimationGraph.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimStateNode.h"

// Additional AnimGraph node types for state machine implementation
#include "AnimStateTransitionNode.h"

#include "AnimStateEntryNode.h"

#include "AnimationStateMachineGraph.h"

#include "AnimationStateMachineSchema.h"

// Animation State Graph (for creating individual states with BoundGraph)
#include "AnimationStateGraph.h"

#include "AnimationStateGraphSchema.h"

// Blend node types
#include "AnimGraphNode_TwoWayBlend.h"

#include "AnimGraphNode_LayeredBoneBlend.h"

#include "AnimGraphNode_SaveCachedPose.h"

#include "AnimGraphNode_Slot.h"

// Helper macros
#define ANIM_ERROR_RESPONSE(Msg, Code) \
    Response->SetBoolField(TEXT("success"), false); \
    Response->SetStringField(TEXT("error"), Msg); \
    Response->SetStringField(TEXT("errorCode"), Code); \
    return Response;

#define ANIM_SUCCESS_RESPONSE(Msg) \
    Response->SetBoolField(TEXT("success"), true); \
    Response->SetStringField(TEXT("message"), Msg);

namespace McpAnimationAuthoring {

FString NormalizeAnimPath(const FString& Path);
USkeleton* LoadSkeletonFromPathAnim(const FString& SkeletonPath);
USkeletalMesh* LoadSkeletalMeshFromPathAnim(const FString& MeshPath);
UAnimSequence* LoadAnimSequenceFromPath(const FString& AnimPath);
bool SaveAnimAsset(UObject* Asset, bool bShouldSave);

UEdGraph* GetAnimGraphFromBlueprint(UAnimBlueprint* AnimBP);
UAnimGraphNode_StateMachine* FindStateMachineNode(UEdGraph* Graph, const FString& Name);
TArray<UAnimGraphNode_StateMachine*> FindStateMachineNodes(UEdGraph* Graph, const FString& Name);
UAnimStateNode* FindStateNode(UAnimationStateMachineGraph* SMGraph, const FString& Name);
UAnimStateTransitionNode* FindTransitionNode(UAnimationStateMachineGraph* SMGraph, const FString& FromState, const FString& ToState);
// Fills a state's BoundGraph with sequence players and wires the first to the
// state Result, writing animationsApplied / animationsFailed into Response.
void ApplyStateAnimations(UAnimStateNode* StateNode, const TArray<FString>& AnimPaths, TSharedPtr<FJsonObject> Response);
// Wires the state machine's Entry node to StateNode when nothing else claims it;
// an entry-less machine compiles but never runs a single frame.
void EnsureStateMachineEntry(UAnimationStateMachineGraph* SMGraph, UAnimStateNode* StateNode, TSharedPtr<FJsonObject> Response);
// Attach what DOES exist to a response, so a name that missed is answerable.
// Nothing else can read a state machine's child graph: inspect_graph reaches
// AnimGraph and EventGraph only, so without these the sole way to discover a
// state name was to guess until a call stopped erroring.
void AddStateMachineInventory(UEdGraph* AnimGraph, TSharedPtr<FJsonObject> Response);
void AddStateInventory(UEdGraph* AnimGraph, const FString& MachineName, TSharedPtr<FJsonObject> Response);
// Applies crossfade / priority / automaticRule / bidirectional / the condition
// rule to one transition. Shared so add_transition arms what it creates instead
// of accepting those fields and dropping them. bOutChanged reports whether any
// of them was actually present: a caller probing whether a transition exists
// must not cost a recompile and a package write.
bool ApplyTransitionSettings(UAnimStateTransitionNode* TransNode, UAnimBlueprint* AnimBP,
                             const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response,
                             FString& OutError, FString& OutErrorCode, bool& bOutChanged);
// The `animations` array as add_state receives it; empty when none was sent.
TArray<FString> ReadStateAnimationPaths(const TSharedPtr<FJsonObject>& Params);

TSharedPtr<FJsonObject> HandleSequenceAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleSequenceTrackActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleSequenceEventActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleSequenceSettingsActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleMontageAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleMontageNotifyBlendActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleBlendSpaceAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleBlendSpaceSampleActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleAimOffsetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleBlueprintAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleBlueprintStateMachineActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleBlueprintStateTransitionActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleBlueprintTransitionRuleActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleBlueprintBlendNodeActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleBlueprintSlotLayerActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleBlueprintNodeValueActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleControlRigActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleIKRigActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleIKRetargetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleAnimationInfoActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);

// Notify helpers shared by add_notify / add_notify_state (SequenceEvents /
// SequenceNotifyStates). Resolves "PlaySound", "AnimNotify_PlaySound",
// "/Script/Engine.AnimNotify_PlaySound" or a Blueprint class path to a class
// deriving from BaseClass; OutTried lists the candidates looked up.
UClass* ResolveNotifyClassByName(const FString& Requested, const TCHAR* Prefix, UClass* BaseClass, TArray<FString>& OutTried);
// "AnimNotify_PlaySound" -> "PlaySound", "BP_Footstep_C" -> "BP_Footstep".
FString NotifyNameFromClass(const UClass* NotifyClass, const TCHAR* Prefix);
TSharedPtr<FJsonObject> HandleSequenceNotifyStateAction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);

} // namespace McpAnimationAuthoring

