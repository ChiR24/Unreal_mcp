#pragma once

#include "McpVersionCompatibility.h"  // MUST be first
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
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
#if __has_include("Factories/BlendSpaceFactoryNew.h") && __has_include("Factories/BlendSpaceFactory1D.h")
#include "Factories/BlendSpaceFactoryNew.h"
#include "Factories/BlendSpaceFactory1D.h"
#define MCP_HAS_BLENDSPACE_FACTORY 1
#else
#define MCP_HAS_BLENDSPACE_FACTORY 0
#endif

// Control Rig support (optional module)
#if __has_include("ControlRig.h")
#include "ControlRig.h"
#define MCP_HAS_CONTROLRIG 1
#else
#define MCP_HAS_CONTROLRIG 0
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

// Pose Asset
#if __has_include("Animation/PoseAsset.h")
#include "Animation/PoseAsset.h"
#define MCP_HAS_POSEASSET 1
#else
#define MCP_HAS_POSEASSET 0
#endif

// Animation Blueprint Graph
#if __has_include("AnimationGraph.h")
#include "AnimationGraph.h"
#endif
#if __has_include("AnimGraphNode_StateMachine.h")
#include "AnimGraphNode_StateMachine.h"
#endif
#if __has_include("AnimGraphNode_TransitionResult.h")
#include "AnimGraphNode_TransitionResult.h"
#endif
#if __has_include("AnimStateNode.h")
#include "AnimStateNode.h"
#endif

// Additional AnimGraph node types for state machine implementation
#if __has_include("AnimStateTransitionNode.h")
#include "AnimStateTransitionNode.h"
#define MCP_HAS_ANIM_STATE_TRANSITION 1
#else
#define MCP_HAS_ANIM_STATE_TRANSITION 0
#endif

#if __has_include("AnimStateEntryNode.h")
#include "AnimStateEntryNode.h"
#endif

#if __has_include("AnimationStateMachineGraph.h")
#include "AnimationStateMachineGraph.h"
#define MCP_HAS_ANIM_STATE_MACHINE_GRAPH 1
#else
#define MCP_HAS_ANIM_STATE_MACHINE_GRAPH 0
#endif

#if __has_include("AnimationStateMachineSchema.h")
#include "AnimationStateMachineSchema.h"
#define MCP_HAS_ANIM_STATE_MACHINE_SCHEMA 1
#else
#define MCP_HAS_ANIM_STATE_MACHINE_SCHEMA 0
#endif

// Animation State Graph (for creating individual states with BoundGraph)
#if __has_include("AnimationStateGraph.h")
#include "AnimationStateGraph.h"
#define MCP_HAS_ANIMATION_STATE_GRAPH 1
#else
#define MCP_HAS_ANIMATION_STATE_GRAPH 0
#endif

#if __has_include("AnimationStateGraphSchema.h")
#include "AnimationStateGraphSchema.h"
#define MCP_HAS_ANIMATION_STATE_GRAPH_SCHEMA 1
#else
#define MCP_HAS_ANIMATION_STATE_GRAPH_SCHEMA 0
#endif

// Blend node types
#if __has_include("AnimGraphNode_TwoWayBlend.h")
#include "AnimGraphNode_TwoWayBlend.h"
#define MCP_HAS_TWO_WAY_BLEND 1
#else
#define MCP_HAS_TWO_WAY_BLEND 0
#endif

#if __has_include("AnimGraphNode_LayeredBoneBlend.h")
#include "AnimGraphNode_LayeredBoneBlend.h"
#define MCP_HAS_LAYERED_BLEND 1
#else
#define MCP_HAS_LAYERED_BLEND 0
#endif

#if __has_include("AnimGraphNode_SaveCachedPose.h")
#include "AnimGraphNode_SaveCachedPose.h"
#define MCP_HAS_CACHED_POSE 1
#else
#define MCP_HAS_CACHED_POSE 0
#endif

#if __has_include("AnimGraphNode_Slot.h")
#include "AnimGraphNode_Slot.h"
#define MCP_HAS_SLOT_NODE 1
#else
#define MCP_HAS_SLOT_NODE 0
#endif

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

inline FString GetStringFieldAnimAuth(const TSharedPtr<FJsonObject>& Obj, const FString& Field,
    const FString& Default = TEXT(""))
{
    return GetJsonStringField(Obj, Field, Default);
}

inline double GetNumberFieldAnimAuth(const TSharedPtr<FJsonObject>& Obj, const FString& Field,
    double Default = 0.0)
{
    return GetJsonNumberField(Obj, Field, Default);
}

inline bool GetBoolFieldAnimAuth(const TSharedPtr<FJsonObject>& Obj, const FString& Field,
    bool Default = false)
{
    return GetJsonBoolField(Obj, Field, Default);
}

FString NormalizeAnimPath(const FString& Path);
USkeleton* LoadSkeletonFromPathAnim(const FString& SkeletonPath);
USkeletalMesh* LoadSkeletalMeshFromPathAnim(const FString& MeshPath);
UAnimSequence* LoadAnimSequenceFromPath(const FString& AnimPath);
bool SaveAnimAsset(UObject* Asset, bool bShouldSave);
FVector GetVectorFromJsonAnim(const TSharedPtr<FJsonObject>& Obj);
FRotator GetRotatorFromJsonAnim(const TSharedPtr<FJsonObject>& Obj);

#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA
UEdGraph* GetAnimGraphFromBlueprint(UAnimBlueprint* AnimBP);
UAnimGraphNode_StateMachine* FindStateMachineNode(UEdGraph* Graph, const FString& Name);
TArray<UAnimGraphNode_StateMachine*> FindStateMachineNodes(UEdGraph* Graph, const FString& Name);
UAnimStateNode* FindStateNode(UAnimationStateMachineGraph* SMGraph, const FString& Name);
UAnimStateTransitionNode* FindTransitionNode(UAnimationStateMachineGraph* SMGraph, const FString& FromState, const FString& ToState);
#endif

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
TSharedPtr<FJsonObject> HandleRigUtilityActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleIKRigActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleIKRetargetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
TSharedPtr<FJsonObject> HandleAnimationInfoActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);

} // namespace McpAnimationAuthoring

#endif // WITH_EDITOR
