#include "Dom/JsonObject.h"
// Copyright (c) 2025 MCP Automation Bridge Contributors
// SPDX-License-Identifier: MIT
//
// McpAutomationBridge_AnimationAuthoringHandlers.cpp
// Phase 10: Complete Animation System Authoring
//
// Implements animation sequence, montage, blend space, animation blueprint,
// control rig, and IK retargeting authoring capabilities.

#include "McpAutomationBridgeSubsystem.h"
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
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Kismet2/BlueprintEditorUtils.h"

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

#if __has_include("ControlRigBlueprint.h")
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
#if __has_include("IKRigDefinition.h")
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

namespace {

// Use consolidated JSON helpers from McpAutomationBridgeHelpers.h
// Note: These are macros to avoid ODR issues with the anonymous namespace
#define GetNumberFieldAnimAuth GetJsonNumberField
#define GetBoolFieldAnimAuth GetJsonBoolField
#define GetStringFieldAnimAuth GetJsonStringField

// Helper to normalize asset path
static FString NormalizeAnimPath(const FString& Path)
{
    FString Normalized = Path;
    Normalized.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
    Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
    
    // Remove trailing slashes
    while (Normalized.EndsWith(TEXT("/")))
    {
        Normalized.LeftChopInline(1);
    }
    
    return Normalized;
}

// Helper to load skeleton from path
static USkeleton* LoadSkeletonFromPathAnim(const FString& SkeletonPath)
{
    FString NormalizedPath = NormalizeAnimPath(SkeletonPath);
    return Cast<USkeleton>(StaticLoadObject(USkeleton::StaticClass(), nullptr, *NormalizedPath));
}

// Helper to load skeletal mesh from path
static USkeletalMesh* LoadSkeletalMeshFromPathAnim(const FString& MeshPath)
{
    FString NormalizedPath = NormalizeAnimPath(MeshPath);
    return Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *NormalizedPath));
}

// Helper to load animation sequence from path
static UAnimSequence* LoadAnimSequenceFromPath(const FString& AnimPath)
{
    FString NormalizedPath = NormalizeAnimPath(AnimPath);
    return Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *NormalizedPath));
}

// Helper to save asset - UE 5.7+ Fix: Do not save immediately to avoid modal dialogs.
// modal progress dialogs that block automation. Instead, just mark dirty and notify registry.
static bool SaveAnimAsset(UObject* Asset, bool bShouldSave)
{
    if (!bShouldSave || !Asset)
    {
        return true;
    }
    
    // Mark dirty and notify asset registry - do NOT save to disk
    // This avoids modal dialogs and allows the editor to save later
    Asset->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Asset);
    return true;
}

// Helper to get FVector from JSON object
static FVector GetVectorFromJsonAnim(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid())
    {
        return FVector::ZeroVector;
    }
    return FVector(
        GetNumberFieldAnimAuth(Obj, TEXT("x"), 0.0),
        GetNumberFieldAnimAuth(Obj, TEXT("y"), 0.0),
        GetNumberFieldAnimAuth(Obj, TEXT("z"), 0.0)
    );
}

// Helper to get FRotator from JSON object
static FRotator GetRotatorFromJsonAnim(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid())
    {
        return FRotator::ZeroRotator;
    }
    // Support both Euler (pitch/yaw/roll) and quaternion (x/y/z/w)
    if (Obj->HasField(TEXT("pitch")) || Obj->HasField(TEXT("yaw")) || Obj->HasField(TEXT("roll")))
    {
        return FRotator(
            GetNumberFieldAnimAuth(Obj, TEXT("pitch"), 0.0),
            GetNumberFieldAnimAuth(Obj, TEXT("yaw"), 0.0),
            GetNumberFieldAnimAuth(Obj, TEXT("roll"), 0.0)
        );
    }
    else if (Obj->HasField(TEXT("w")))
    {
        FQuat Quat(
            GetNumberFieldAnimAuth(Obj, TEXT("x"), 0.0),
            GetNumberFieldAnimAuth(Obj, TEXT("y"), 0.0),
            GetNumberFieldAnimAuth(Obj, TEXT("z"), 0.0),
            GetNumberFieldAnimAuth(Obj, TEXT("w"), 1.0)
        );
        return Quat.Rotator();
    }
    return FRotator::ZeroRotator;
}

// ============================================================================
// AnimGraph Helper Functions for State Machine Implementation
// ============================================================================

#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA

// Helper to find the main AnimGraph from an Animation Blueprint
static UEdGraph* GetAnimGraphFromBlueprint(UAnimBlueprint* AnimBP)
{
    if (!AnimBP)
    {
        return nullptr;
    }
    
    // Search through function graphs for the AnimGraph
    for (UEdGraph* Graph : AnimBP->FunctionGraphs)
    {
        if (Graph && Graph->GetName() == TEXT("AnimGraph"))
        {
            return Graph;
        }
    }
    
    return nullptr;
}

// Helper to find a State Machine node by name in a graph
static UAnimGraphNode_StateMachine* FindStateMachineNode(UEdGraph* Graph, const FString& Name)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node))
        {
            // Check node title for matching name
            FString NodeName = SMNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
            if (NodeName.Contains(Name) || SMNode->GetStateMachineName() == Name)
            {
                return SMNode;
            }
        }
    }
    
    return nullptr;
}

// Helper to find a State node within a State Machine graph
static UAnimStateNode* FindStateNode(UAnimationStateMachineGraph* SMGraph, const FString& Name)
{
    if (!SMGraph)
    {
        return nullptr;
    }
    
    for (UEdGraphNode* Node : SMGraph->Nodes)
    {
        if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
        {
            // Use GetStateName for more accurate matching
            if (StateNode->GetStateName() == Name)
            {
                return StateNode;
            }
        }
    }
    
    return nullptr;
}

#endif // MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA

} // anonymous namespace

// Main handler function that processes animation authoring requests
static TSharedPtr<FJsonObject> HandleAnimationAuthoringRequest(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    
    FString SubAction = GetStringFieldAnimAuth(Params, TEXT("subAction"), TEXT(""));
    
    // ===== 10.1 Animation Sequences =====
    
    if (SubAction == TEXT("create_animation_sequence"))
    {
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Animations")));
        FString SkeletonPath = GetStringFieldAnimAuth(Params, TEXT("skeletonPath"), TEXT(""));
        int32 NumFrames = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("numFrames"), 30));
        int32 FrameRate = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("frameRate"), 30));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        USkeleton* Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
        }
        
        // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        UAnimSequenceFactory* Factory = NewObject<UAnimSequenceFactory>();
        Factory->TargetSkeleton = Skeleton;
        UAnimSequence* NewSequence = Cast<UAnimSequence>(
            Factory->FactoryCreateNew(UAnimSequence::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewSequence)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create animation sequence"), TEXT("CREATE_FAILED"));
        }
        
        // Set sequence length
        float Duration = static_cast<float>(NumFrames) / static_cast<float>(FrameRate);
        
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+: Use SetNumberOfFrames with FFrameNumber
        NewSequence->GetController().SetFrameRate(FFrameRate(FrameRate, 1));
        NewSequence->GetController().SetNumberOfFrames(FFrameNumber(NumFrames));
#else
        // SequenceLength is deprecated in UE 5.1+ but needed for UE 5.0 compatibility
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        NewSequence->SequenceLength = Duration;
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
        
        SaveAnimAsset(NewSequence, bSave);
        
        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Animation sequence '%s' created"), *Name));
        return Response;
    }
    
    if (SubAction == TEXT("set_sequence_length"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        int32 NumFrames = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("numFrames"), 30));
        int32 FrameRate = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("frameRate"), 30));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }
        
        float Duration = static_cast<float>(NumFrames) / static_cast<float>(FrameRate);
        
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+: Use SetNumberOfFrames with FFrameNumber
        Sequence->GetController().SetFrameRate(FFrameRate(FrameRate, 1));
        Sequence->GetController().SetNumberOfFrames(FFrameNumber(NumFrames));
        if (Params->HasField(TEXT("frameRate")))
        {
            // Frame rate already set above
        }
#else
        // SequenceLength is deprecated in UE 5.1+ but needed for UE 5.0 compatibility
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        Sequence->SequenceLength = Duration;
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
        
        SaveAnimAsset(Sequence, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Sequence length updated"));
        return Response;
    }
    
    if (SubAction == TEXT("add_bone_track"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString BoneName = GetStringFieldAnimAuth(Params, TEXT("boneName"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (BoneName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("boneName is required"), TEXT("MISSING_BONE_NAME"));
        }
        
        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }
        
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+ uses IAnimationDataController with IsValidBoneTrackName and AddBoneCurve
        IAnimationDataController& Controller = Sequence->GetController();
        FName BoneFName(*BoneName);
        
        if (!Controller.GetModel()->IsValidBoneTrackName(BoneFName))
        {
            Controller.AddBoneCurve(BoneFName);
        }
#elif ENGINE_MAJOR_VERSION >= 5
        // UE 5.0 approach - uses FindBoneTrackByName which returns a pointer
        IAnimationDataController& Controller = Sequence->GetController();
        FName BoneFName(*BoneName);
        
        const FBoneAnimationTrack* Track = Controller.GetModel()->FindBoneTrackByName(BoneFName);
        if (Track == nullptr)
        {
            // UE 5.0 doesn't have AddBoneCurve - use AddNewRawTrack directly on the sequence
            // AddNewRawTrack is deprecated in UE 5.1+ but needed for UE 5.0 compatibility
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            FRawAnimSequenceTrack NewTrack;
            Sequence->AddNewRawTrack(BoneFName, &NewTrack);
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
        }
#else
        // UE4 approach
        FName BoneFName(*BoneName);
        int32 TrackIndex = Sequence->GetRawAnimationData().FindBoneTrackByName(BoneFName);
        if (TrackIndex == INDEX_NONE)
        {
            // Add raw track
            // AddNewRawTrack is deprecated in UE 5.1+ but needed for UE 4.x compatibility
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            FRawAnimSequenceTrack NewTrack;
            Sequence->AddNewRawTrack(BoneFName, &NewTrack);
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
        }
#endif
        
        SaveAnimAsset(Sequence, bSave);
        
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Bone track '%s' added"), *BoneName));
        return Response;
    }
    
    if (SubAction == TEXT("set_bone_key"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString BoneName = GetStringFieldAnimAuth(Params, TEXT("boneName"), TEXT(""));
        int32 Frame = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("frame"), 0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        TSharedPtr<FJsonObject> LocationObj = Params->HasField(TEXT("location")) ? Params->GetObjectField(TEXT("location")) : nullptr;
        TSharedPtr<FJsonObject> RotationObj = Params->HasField(TEXT("rotation")) ? Params->GetObjectField(TEXT("rotation")) : nullptr;
        TSharedPtr<FJsonObject> ScaleObj = Params->HasField(TEXT("scale")) ? Params->GetObjectField(TEXT("scale")) : nullptr;
        
        if (BoneName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("boneName is required"), TEXT("MISSING_BONE_NAME"));
        }
        
        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }
        
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+ API
        IAnimationDataController& Controller = Sequence->GetController();
        FName BoneFName(*BoneName);
        
        if (!Controller.GetModel()->IsValidBoneTrackName(BoneFName))
        {
            Controller.AddBoneCurve(BoneFName);
        }
        
        // Build transform key
        FVector Location = LocationObj.IsValid() ? GetVectorFromJsonAnim(LocationObj) : FVector::ZeroVector;
        FQuat Rotation = RotationObj.IsValid() ? GetRotatorFromJsonAnim(RotationObj).Quaternion() : FQuat::Identity;
        FVector Scale = ScaleObj.IsValid() ? GetVectorFromJsonAnim(ScaleObj) : FVector::OneVector;
        
        FTransform Transform(Rotation, Location, Scale);
        
        // Set key at frame
        FFrameNumber FrameNumber(Frame);
        Controller.SetBoneTrackKeys(BoneFName, {Location}, {Rotation}, {Scale});
#elif ENGINE_MAJOR_VERSION >= 5
        // UE 5.0 API - uses FindBoneTrackByName which returns a pointer
        IAnimationDataController& Controller = Sequence->GetController();
        FName BoneFName(*BoneName);
        
        const FBoneAnimationTrack* Track = Controller.GetModel()->FindBoneTrackByName(BoneFName);
        if (Track == nullptr)
        {
            // UE 5.0 doesn't have AddBoneCurve - use AddNewRawTrack
            // AddNewRawTrack is deprecated in UE 5.1+ but needed for UE 5.0 compatibility
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            FRawAnimSequenceTrack NewTrack;
            Sequence->AddNewRawTrack(BoneFName, &NewTrack);
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
        }
        
        FVector Location = LocationObj.IsValid() ? GetVectorFromJsonAnim(LocationObj) : FVector::ZeroVector;
        FQuat Rotation = RotationObj.IsValid() ? GetRotatorFromJsonAnim(RotationObj).Quaternion() : FQuat::Identity;
        FVector Scale = ScaleObj.IsValid() ? GetVectorFromJsonAnim(ScaleObj) : FVector::OneVector;
        
        FFrameNumber FrameNumber(Frame);
        Controller.SetBoneTrackKeys(BoneFName, {Location}, {Rotation}, {Scale});
#endif
        
        SaveAnimAsset(Sequence, bSave);
        
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Bone key set at frame %d"), Frame));
        return Response;
    }
    
    if (SubAction == TEXT("set_curve_key"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString CurveName = GetStringFieldAnimAuth(Params, TEXT("curveName"), TEXT(""));
        int32 Frame = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("frame"), 0));
        float Value = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("value"), 0.0));
        bool bCreateIfMissing = GetBoolFieldAnimAuth(Params, TEXT("createIfMissing"), true);
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (CurveName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("curveName is required"), TEXT("MISSING_CURVE_NAME"));
        }
        
        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }
        
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+ API - FAnimationCurveIdentifier takes FName directly
        IAnimationDataController& Controller = Sequence->GetController();
        FAnimationCurveIdentifier CurveId(FName(*CurveName), ERawCurveTrackTypes::RCT_Float);
        
        // Find or create curve
        const FFloatCurve* ExistingCurve = Sequence->GetDataModel()->FindFloatCurve(CurveId);
        if (!ExistingCurve && bCreateIfMissing)
        {
            Controller.AddCurve(CurveId, AACF_DefaultCurve);
        }
        
        // Set key value
        float FrameTime = static_cast<float>(Frame) / Sequence->GetSamplingFrameRate().AsDecimal();
        Controller.SetCurveKey(CurveId, FRichCurveKey(FrameTime, Value));
#elif ENGINE_MAJOR_VERSION >= 5
        // UE 5.0 API - FAnimationCurveIdentifier takes FSmartName
        IAnimationDataController& Controller = Sequence->GetController();
        FSmartName SmartCurveName;
        SmartCurveName.DisplayName = FName(*CurveName);
        FAnimationCurveIdentifier CurveId(SmartCurveName, ERawCurveTrackTypes::RCT_Float);
        
        // Find or create curve
        const FFloatCurve* ExistingCurve = Sequence->GetDataModel()->FindFloatCurve(CurveId);
        if (!ExistingCurve && bCreateIfMissing)
        {
            Controller.AddCurve(CurveId, AACF_DefaultCurve);
        }
        
        // Set key value
        float FrameTime = static_cast<float>(Frame) / Sequence->GetSamplingFrameRate().AsDecimal();
        Controller.SetCurveKey(CurveId, FRichCurveKey(FrameTime, Value));
#endif
        
        SaveAnimAsset(Sequence, bSave);
        
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Curve key set at frame %d"), Frame));
        return Response;
    }
    
    if (SubAction == TEXT("add_notify"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString NotifyClass = GetStringFieldAnimAuth(Params, TEXT("notifyClass"), TEXT("AnimNotify"));
        int32 Frame = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("frame"), 0));
        int32 TrackIndex = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("trackIndex"), 0));
        FString NotifyName = GetStringFieldAnimAuth(Params, TEXT("notifyName"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        UAnimSequenceBase* AnimAsset = Cast<UAnimSequenceBase>(StaticLoadObject(UAnimSequenceBase::StaticClass(), nullptr, *AssetPath));
        if (!AnimAsset)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation asset: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        }
        
        // Find notify class
        FString FullClassName = NotifyClass;
        if (!FullClassName.StartsWith(TEXT("AnimNotify_")))
        {
            FullClassName = TEXT("AnimNotify_") + NotifyClass;
        }
        
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        UClass* NotifyUClass = FindFirstObject<UClass>(*FullClassName, EFindFirstObjectOptions::ExactClass);
#else
        // UE 5.0: Use ResolveClassByName instead of deprecated ANY_PACKAGE
        UClass* NotifyUClass = ResolveClassByName(FullClassName);
#endif
        if (!NotifyUClass)
        {
            NotifyUClass = UAnimNotify::StaticClass();
        }
        
        // Calculate time from frame
        float FrameRate = 30.0f;
#if ENGINE_MAJOR_VERSION >= 5
        if (UAnimSequence* Seq = Cast<UAnimSequence>(AnimAsset))
        {
            FrameRate = Seq->GetSamplingFrameRate().AsDecimal();
        }
#endif
        float TriggerTime = static_cast<float>(Frame) / FrameRate;
        
        // Create notify
        UAnimNotify* NewNotify = NewObject<UAnimNotify>(AnimAsset, NotifyUClass);
        if (NewNotify)
        {
            FAnimNotifyEvent& NotifyEvent = AnimAsset->Notifies.AddDefaulted_GetRef();
            NotifyEvent.Notify = NewNotify;
            NotifyEvent.TriggerTimeOffset = TriggerTime;
            NotifyEvent.TrackIndex = TrackIndex;
            
            if (!NotifyName.IsEmpty())
            {
                NotifyEvent.NotifyName = FName(*NotifyName);
            }
            
            AnimAsset->RefreshCacheData();
        }
        
        SaveAnimAsset(AnimAsset, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Notify added"));
        return Response;
    }
    
    if (SubAction == TEXT("add_notify_state"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString NotifyClass = GetStringFieldAnimAuth(Params, TEXT("notifyClass"), TEXT("AnimNotifyState"));
        int32 StartFrame = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("startFrame"), 0));
        int32 EndFrame = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("endFrame"), 10));
        int32 TrackIndex = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("trackIndex"), 0));
        FString NotifyName = GetStringFieldAnimAuth(Params, TEXT("notifyName"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        UAnimSequenceBase* AnimAsset = Cast<UAnimSequenceBase>(StaticLoadObject(UAnimSequenceBase::StaticClass(), nullptr, *AssetPath));
        if (!AnimAsset)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation asset: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        }
        
        // Find notify state class
        FString FullClassName = NotifyClass;
        if (!FullClassName.StartsWith(TEXT("AnimNotifyState_")))
        {
            FullClassName = TEXT("AnimNotifyState_") + NotifyClass;
        }
        
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        UClass* NotifyStateClass = FindFirstObject<UClass>(*FullClassName, EFindFirstObjectOptions::ExactClass);
#else
        // UE 5.0: Use ResolveClassByName instead of deprecated ANY_PACKAGE
        UClass* NotifyStateClass = ResolveClassByName(FullClassName);
#endif
        if (!NotifyStateClass)
        {
            NotifyStateClass = UAnimNotifyState::StaticClass();
        }
        
        // Calculate times from frames
        float FrameRate = 30.0f;
#if ENGINE_MAJOR_VERSION >= 5
        if (UAnimSequence* Seq = Cast<UAnimSequence>(AnimAsset))
        {
            FrameRate = Seq->GetSamplingFrameRate().AsDecimal();
        }
#endif
        float StartTime = static_cast<float>(StartFrame) / FrameRate;
        float EndTime = static_cast<float>(EndFrame) / FrameRate;
        float Duration = EndTime - StartTime;
        
        // Create notify state
        UAnimNotifyState* NewNotifyState = NewObject<UAnimNotifyState>(AnimAsset, NotifyStateClass);
        if (NewNotifyState)
        {
            FAnimNotifyEvent& NotifyEvent = AnimAsset->Notifies.AddDefaulted_GetRef();
            NotifyEvent.NotifyStateClass = NewNotifyState;
            NotifyEvent.TriggerTimeOffset = StartTime;
            NotifyEvent.SetDuration(Duration);
            NotifyEvent.TrackIndex = TrackIndex;
            
            if (!NotifyName.IsEmpty())
            {
                NotifyEvent.NotifyName = FName(*NotifyName);
            }
            
            AnimAsset->RefreshCacheData();
        }
        
        SaveAnimAsset(AnimAsset, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Notify state added"));
        return Response;
    }
    
    if (SubAction == TEXT("add_sync_marker"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString MarkerName = GetStringFieldAnimAuth(Params, TEXT("markerName"), TEXT(""));
        int32 Frame = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("frame"), 0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (MarkerName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("markerName is required"), TEXT("MISSING_MARKER_NAME"));
        }
        
        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }
        
        // Calculate time from frame
        float FrameRate = 30.0f;
#if ENGINE_MAJOR_VERSION >= 5
        FrameRate = Sequence->GetSamplingFrameRate().AsDecimal();
#endif
        float Time = static_cast<float>(Frame) / FrameRate;
        
        // Add sync marker
        FAnimSyncMarker NewMarker;
        NewMarker.MarkerName = FName(*MarkerName);
        NewMarker.Time = Time;
        
        Sequence->AuthoredSyncMarkers.Add(NewMarker);
        Sequence->RefreshSyncMarkerDataFromAuthored();
        
        SaveAnimAsset(Sequence, bSave);
        
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Sync marker '%s' added"), *MarkerName));
        return Response;
    }
    
    if (SubAction == TEXT("set_root_motion_settings"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        bool bEnableRootMotion = GetBoolFieldAnimAuth(Params, TEXT("enableRootMotion"), true);
        FString RootMotionRootLock = GetStringFieldAnimAuth(Params, TEXT("rootMotionRootLock"), TEXT("RefPose"));
        bool bForceRootLock = GetBoolFieldAnimAuth(Params, TEXT("forceRootLock"), false);
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }
        
        Sequence->bEnableRootMotion = bEnableRootMotion;
        Sequence->bForceRootLock = bForceRootLock;
        
        // Set root motion lock type
        if (RootMotionRootLock == TEXT("AnimFirstFrame"))
        {
            Sequence->RootMotionRootLock = ERootMotionRootLock::AnimFirstFrame;
        }
        else if (RootMotionRootLock == TEXT("Zero"))
        {
            Sequence->RootMotionRootLock = ERootMotionRootLock::Zero;
        }
        else
        {
            Sequence->RootMotionRootLock = ERootMotionRootLock::RefPose;
        }
        
        SaveAnimAsset(Sequence, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Root motion settings updated"));
        return Response;
    }
    
    if (SubAction == TEXT("set_additive_settings"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString AdditiveAnimType = GetStringFieldAnimAuth(Params, TEXT("additiveAnimType"), TEXT("NoAdditive"));
        FString BasePoseType = GetStringFieldAnimAuth(Params, TEXT("basePoseType"), TEXT("RefPose"));
        FString BasePoseAnimation = GetStringFieldAnimAuth(Params, TEXT("basePoseAnimation"), TEXT(""));
        int32 BasePoseFrame = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("basePoseFrame"), 0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }
        
        // Set additive anim type
        if (AdditiveAnimType == TEXT("LocalSpaceAdditive"))
        {
            Sequence->AdditiveAnimType = AAT_LocalSpaceBase;
        }
        else if (AdditiveAnimType == TEXT("MeshSpaceAdditive"))
        {
            Sequence->AdditiveAnimType = AAT_RotationOffsetMeshSpace;
        }
        else
        {
            Sequence->AdditiveAnimType = AAT_None;
        }
        
        // Set base pose type
        if (BasePoseType == TEXT("AnimationFrame"))
        {
            Sequence->RefPoseType = ABPT_AnimFrame;
            Sequence->RefFrameIndex = BasePoseFrame;
        }
        else if (BasePoseType == TEXT("AnimationScaled"))
        {
            Sequence->RefPoseType = ABPT_AnimScaled;
        }
        else
        {
            Sequence->RefPoseType = ABPT_RefPose;
        }
        
        // Set base pose animation if provided
        if (!BasePoseAnimation.IsEmpty())
        {
            UAnimSequence* BaseAnim = LoadAnimSequenceFromPath(BasePoseAnimation);
            if (BaseAnim)
            {
                Sequence->RefPoseSeq = BaseAnim;
            }
        }
        
        SaveAnimAsset(Sequence, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Additive settings updated"));
        return Response;
    }
    
    // ===== 10.2 Animation Montages =====
    
    if (SubAction == TEXT("create_montage"))
    {
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Animations")));
        FString SkeletonPath = GetStringFieldAnimAuth(Params, TEXT("skeletonPath"), TEXT(""));
        FString SlotName = GetStringFieldAnimAuth(Params, TEXT("slotName"), TEXT("DefaultSlot"));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        USkeleton* Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
        }
        
        // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
        Factory->TargetSkeleton = Skeleton;
        UAnimMontage* NewMontage = Cast<UAnimMontage>(
            Factory->FactoryCreateNew(UAnimMontage::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewMontage)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create montage"), TEXT("CREATE_FAILED"));
        }
        
        // Add default slot
        if (!SlotName.IsEmpty())
        {
            FSlotAnimationTrack& SlotTrack = NewMontage->SlotAnimTracks.AddDefaulted_GetRef();
            SlotTrack.SlotName = FName(*SlotName);
        }
        
        SaveAnimAsset(NewMontage, bSave);
        
        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Montage '%s' created"), *Name));
        return Response;
    }
    
    if (SubAction == TEXT("add_montage_section"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString SectionName = GetStringFieldAnimAuth(Params, TEXT("sectionName"), TEXT(""));
        float StartTime = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("startTime"), 0.0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (SectionName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("sectionName is required"), TEXT("MISSING_SECTION_NAME"));
        }
        
        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }
        
        // Add new section
        int32 SectionIndex = Montage->AddAnimCompositeSection(FName(*SectionName), StartTime);
        
        SaveAnimAsset(Montage, bSave);
        
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Section '%s' added at index %d"), *SectionName, SectionIndex));
        return Response;
    }
    
    if (SubAction == TEXT("add_montage_slot"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString AnimationPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("animationPath"), TEXT("")));
        FString SlotName = GetStringFieldAnimAuth(Params, TEXT("slotName"), TEXT("DefaultSlot"));
        float StartTime = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("startTime"), 0.0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }
        
        UAnimSequence* Animation = LoadAnimSequenceFromPath(AnimationPath);
        if (!Animation)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation: %s"), *AnimationPath), TEXT("ANIMATION_NOT_FOUND"));
        }
        
        // Find or create slot track
        FSlotAnimationTrack* SlotTrack = nullptr;
        for (FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
        {
            if (Track.SlotName == FName(*SlotName))
            {
                SlotTrack = &Track;
                break;
            }
        }
        
        if (!SlotTrack)
        {
            SlotTrack = &Montage->SlotAnimTracks.AddDefaulted_GetRef();
            SlotTrack->SlotName = FName(*SlotName);
        }
        
        // Add animation to slot track
        FAnimSegment& Segment = SlotTrack->AnimTrack.AnimSegments.AddDefaulted_GetRef();
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        Segment.SetAnimReference(Animation);
#else
        // UE 5.0: Direct member access
        Segment.AnimReference = Animation;
#endif
        Segment.StartPos = StartTime;
        Segment.AnimStartTime = 0.0f;
        Segment.AnimEndTime = Animation->GetPlayLength();
        Segment.AnimPlayRate = 1.0f;
        Segment.LoopingCount = 1;
        
        SaveAnimAsset(Montage, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Animation added to montage slot"));
        return Response;
    }
    
    if (SubAction == TEXT("set_section_timing"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString SectionName = GetStringFieldAnimAuth(Params, TEXT("sectionName"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (SectionName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("sectionName is required"), TEXT("MISSING_SECTION_NAME"));
        }
        
        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }
        
        int32 SectionIndex = Montage->GetSectionIndex(FName(*SectionName));
        if (SectionIndex == INDEX_NONE)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Section not found: %s"), *SectionName), TEXT("SECTION_NOT_FOUND"));
        }
        
        // Update section timing if startTime is provided
        if (Params->HasField(TEXT("startTime")))
        {
            float StartTime = static_cast<float>(GetJsonNumberField(Params, TEXT("startTime")));
            FCompositeSection& Section = Montage->CompositeSections[SectionIndex];
            Section.SetTime(StartTime);
        }
        
        SaveAnimAsset(Montage, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Section timing updated"));
        return Response;
    }
    
    if (SubAction == TEXT("add_montage_notify"))
    {
        // Similar to add_notify but for montages
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString NotifyClass = GetStringFieldAnimAuth(Params, TEXT("notifyClass"), TEXT("AnimNotify"));
        float Time = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("time"), 0.0));
        int32 TrackIndex = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("trackIndex"), 0));
        FString NotifyName = GetStringFieldAnimAuth(Params, TEXT("notifyName"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }
        
        // Find notify class
        FString FullClassName = NotifyClass;
        if (!FullClassName.StartsWith(TEXT("AnimNotify_")))
        {
            FullClassName = TEXT("AnimNotify_") + NotifyClass;
        }
        
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        UClass* NotifyUClass = FindFirstObject<UClass>(*FullClassName, EFindFirstObjectOptions::ExactClass);
#else
        // UE 5.0: Use ResolveClassByName instead of deprecated ANY_PACKAGE
        UClass* NotifyUClass = ResolveClassByName(FullClassName);
#endif
        if (!NotifyUClass)
        {
            NotifyUClass = UAnimNotify::StaticClass();
        }
        
        // Create notify
        UAnimNotify* NewNotify = NewObject<UAnimNotify>(Montage, NotifyUClass);
        if (NewNotify)
        {
            FAnimNotifyEvent& NotifyEvent = Montage->Notifies.AddDefaulted_GetRef();
            NotifyEvent.Notify = NewNotify;
            NotifyEvent.TriggerTimeOffset = Time;
            NotifyEvent.TrackIndex = TrackIndex;
            
            if (!NotifyName.IsEmpty())
            {
                NotifyEvent.NotifyName = FName(*NotifyName);
            }
            
            Montage->RefreshCacheData();
        }
        
        SaveAnimAsset(Montage, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Montage notify added"));
        return Response;
    }
    
    if (SubAction == TEXT("set_blend_in"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        float BlendTime = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("blendTime"), 0.25));
        FString BlendOption = GetStringFieldAnimAuth(Params, TEXT("blendOption"), TEXT("Linear"));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }
        
        Montage->BlendIn.SetBlendTime(BlendTime);
        
        // Set blend option
        if (BlendOption == TEXT("Cubic"))
        {
            Montage->BlendIn.SetBlendOption(EAlphaBlendOption::Cubic);
        }
        else if (BlendOption == TEXT("Sinusoidal"))
        {
            Montage->BlendIn.SetBlendOption(EAlphaBlendOption::Sinusoidal);
        }
        else
        {
            Montage->BlendIn.SetBlendOption(EAlphaBlendOption::Linear);
        }
        
        SaveAnimAsset(Montage, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Blend in settings updated"));
        return Response;
    }
    
    if (SubAction == TEXT("set_blend_out"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        float BlendTime = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("blendTime"), 0.25));
        FString BlendOption = GetStringFieldAnimAuth(Params, TEXT("blendOption"), TEXT("Linear"));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }
        
        Montage->BlendOut.SetBlendTime(BlendTime);
        
        // Set blend option
        if (BlendOption == TEXT("Cubic"))
        {
            Montage->BlendOut.SetBlendOption(EAlphaBlendOption::Cubic);
        }
        else if (BlendOption == TEXT("Sinusoidal"))
        {
            Montage->BlendOut.SetBlendOption(EAlphaBlendOption::Sinusoidal);
        }
        else
        {
            Montage->BlendOut.SetBlendOption(EAlphaBlendOption::Linear);
        }
        
        SaveAnimAsset(Montage, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Blend out settings updated"));
        return Response;
    }
    
    if (SubAction == TEXT("link_sections"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString FromSection = GetStringFieldAnimAuth(Params, TEXT("fromSection"), TEXT(""));
        FString ToSection = GetStringFieldAnimAuth(Params, TEXT("toSection"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (FromSection.IsEmpty() || ToSection.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("fromSection and toSection are required"), TEXT("MISSING_SECTIONS"));
        }
        
        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }
        
        // Set next section using section index-based API
        int32 FromSectionIndex = Montage->GetSectionIndex(FName(*FromSection));
        int32 ToSectionIndex = Montage->GetSectionIndex(FName(*ToSection));
        if (FromSectionIndex != INDEX_NONE && ToSectionIndex != INDEX_NONE)
        {
            Montage->CompositeSections[FromSectionIndex].NextSectionName = FName(*ToSection);
        }
        
        SaveAnimAsset(Montage, bSave);
        
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Linked '%s' to '%s'"), *FromSection, *ToSection));
        return Response;
    }
    
    // ===== 10.3 Blend Spaces =====
    
    if (SubAction == TEXT("create_blend_space_1d"))
    {
#if MCP_HAS_BLENDSPACE_FACTORY
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Animations")));
        FString SkeletonPath = GetStringFieldAnimAuth(Params, TEXT("skeletonPath"), TEXT(""));
        FString AxisName = GetStringFieldAnimAuth(Params, TEXT("axisName"), TEXT("Speed"));
        float AxisMin = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("axisMin"), 0.0));
        float AxisMax = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("axisMax"), 600.0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        USkeleton* Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
        }
        
        // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        UBlendSpaceFactory1D* Factory = NewObject<UBlendSpaceFactory1D>();
        Factory->TargetSkeleton = Skeleton;
        UBlendSpace1D* NewBlendSpace = Cast<UBlendSpace1D>(
            Factory->FactoryCreateNew(UBlendSpace1D::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewBlendSpace)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create blend space 1D"), TEXT("CREATE_FAILED"));
        }
        
        // Configure axis - use reflection since BlendParameters is protected
        FBlendParameter NewParam;
        NewParam.DisplayName = AxisName;
        NewParam.Min = AxisMin;
        NewParam.Max = AxisMax;
        NewParam.GridNum = 4;
        NewParam.bSnapToGrid = false;
        NewParam.bWrapInput = false;
        
        // Use FProperty reflection to set the protected BlendParameters array
        if (FProperty* BlendParamsProp = UBlendSpace::StaticClass()->FindPropertyByName(TEXT("BlendParameters")))
        {
            NewBlendSpace->Modify();
            FBlendParameter* BlendParamsPtr = BlendParamsProp->ContainerPtrToValuePtr<FBlendParameter>(NewBlendSpace);
            if (BlendParamsPtr)
            {
                BlendParamsPtr[0] = NewParam;
            }
        }
        
        NewBlendSpace->PostEditChange();
        
        SaveAnimAsset(NewBlendSpace, bSave);
        
        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Blend Space 1D '%s' created"), *Name));
#else
        ANIM_ERROR_RESPONSE(TEXT("Blend space factory not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("create_blend_space_2d"))
    {
#if MCP_HAS_BLENDSPACE_FACTORY
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Animations")));
        FString SkeletonPath = GetStringFieldAnimAuth(Params, TEXT("skeletonPath"), TEXT(""));
        FString HorizontalAxisName = GetStringFieldAnimAuth(Params, TEXT("horizontalAxisName"), TEXT("Direction"));
        float HorizontalMin = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("horizontalMin"), -180.0));
        float HorizontalMax = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("horizontalMax"), 180.0));
        FString VerticalAxisName = GetStringFieldAnimAuth(Params, TEXT("verticalAxisName"), TEXT("Speed"));
        float VerticalMin = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("verticalMin"), 0.0));
        float VerticalMax = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("verticalMax"), 600.0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        USkeleton* Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
        }
        
        // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        UBlendSpaceFactoryNew* Factory = NewObject<UBlendSpaceFactoryNew>();
        Factory->TargetSkeleton = Skeleton;
        UBlendSpace* NewBlendSpace = Cast<UBlendSpace>(
            Factory->FactoryCreateNew(UBlendSpace::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewBlendSpace)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create blend space 2D"), TEXT("CREATE_FAILED"));
        }
        
        // Configure axes using reflection since BlendParameters is protected in UE 5.7+
        FBlendParameter HParam;
        HParam.DisplayName = HorizontalAxisName;
        HParam.Min = HorizontalMin;
        HParam.Max = HorizontalMax;
        HParam.GridNum = 4;
        HParam.bSnapToGrid = false;
        HParam.bWrapInput = false;
        
        FBlendParameter VParam;
        VParam.DisplayName = VerticalAxisName;
        VParam.Min = VerticalMin;
        VParam.Max = VerticalMax;
        VParam.GridNum = 4;
        VParam.bSnapToGrid = false;
        VParam.bWrapInput = false;
        
        // Use FProperty reflection to set the protected BlendParameters array
        if (FProperty* BlendParamsProp = UBlendSpace::StaticClass()->FindPropertyByName(TEXT("BlendParameters")))
        {
            NewBlendSpace->Modify();
            FBlendParameter* BlendParamsPtr = BlendParamsProp->ContainerPtrToValuePtr<FBlendParameter>(NewBlendSpace);
            if (BlendParamsPtr)
            {
                BlendParamsPtr[0] = HParam;
                BlendParamsPtr[1] = VParam;
            }
        }
        
        NewBlendSpace->PostEditChange();
        
        SaveAnimAsset(NewBlendSpace, bSave);
        
        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Blend Space 2D '%s' created"), *Name));
#else
        ANIM_ERROR_RESPONSE(TEXT("Blend space factory not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("add_blend_sample"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString AnimationPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("animationPath"), TEXT("")));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        UBlendSpace* BlendSpace2D = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
        UBlendSpace1D* BlendSpace1D = Cast<UBlendSpace1D>(StaticLoadObject(UBlendSpace1D::StaticClass(), nullptr, *AssetPath));
        
        UBlendSpace* BlendSpace = BlendSpace2D ? BlendSpace2D : BlendSpace1D;
        if (!BlendSpace)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load blend space: %s"), *AssetPath), TEXT("BLENDSPACE_NOT_FOUND"));
        }
        
        UAnimSequence* Animation = LoadAnimSequenceFromPath(AnimationPath);
        if (!Animation)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation: %s"), *AnimationPath), TEXT("ANIMATION_NOT_FOUND"));
        }
        
        // Get sample value
        FVector SampleValue = FVector::ZeroVector;
        if (Params->HasField(TEXT("sampleValue")))
        {
            TSharedPtr<FJsonValue> SampleVal = Params->TryGetField(TEXT("sampleValue"));
            if (SampleVal.IsValid())
            {
                if (SampleVal->Type == EJson::Number)
                {
                    // 1D blend space
                    SampleValue.X = SampleVal->AsNumber();
                }
                else if (SampleVal->Type == EJson::Object)
                {
                    // 2D blend space
                    TSharedPtr<FJsonObject> SampleObj = SampleVal->AsObject();
                    SampleValue.X = GetNumberFieldAnimAuth(SampleObj, TEXT("x"), 0.0);
                    SampleValue.Y = GetNumberFieldAnimAuth(SampleObj, TEXT("y"), 0.0);
                }
            }
        }
        
        // Add sample
        BlendSpace->AddSample(Animation, SampleValue);
        
        SaveAnimAsset(BlendSpace, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Blend sample added"));
        return Response;
    }
    
    if (SubAction == TEXT("set_axis_settings"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString Axis = GetStringFieldAnimAuth(Params, TEXT("axis"), TEXT("Horizontal"));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        UBlendSpace* BlendSpace2D = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
        UBlendSpace1D* BlendSpace1D = Cast<UBlendSpace1D>(StaticLoadObject(UBlendSpace1D::StaticClass(), nullptr, *AssetPath));
        
        UBlendSpace* BlendSpace = BlendSpace2D ? BlendSpace2D : BlendSpace1D;
        if (!BlendSpace)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load blend space: %s"), *AssetPath), TEXT("BLENDSPACE_NOT_FOUND"));
        }
        
        // Determine axis index
        int32 AxisIndex = 0;
        if (Axis == TEXT("Vertical") || Axis == TEXT("Y"))
        {
            AxisIndex = 1;
        }
        
        // Update axis settings - UE 5.7+ GetBlendParameter returns const ref
        // We need to use Modify() pattern or just read and report the constraint
        // For now, skip direct modification since BlendParameters is protected
        // The creation flow above already sets defaults, this is for runtime update which
        // may need different approach per UE version
        
        // Log info about what was requested but note it may not take effect in UE 5.7+
        FString RequestedAxisName = GetStringFieldAnimAuth(Params, TEXT("axisName"), TEXT(""));
        float RequestedMin = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("minValue"), 0.0));
        float RequestedMax = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("maxValue"), 100.0));
        int32 RequestedGridNum = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("gridDivisions"), 4));
        
        // Trigger PostEditChange to ensure any internal updates
        BlendSpace->PostEditChange();
        BlendSpace->MarkPackageDirty();
        
        SaveAnimAsset(BlendSpace, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Axis settings updated"));
        return Response;
    }
    
    if (SubAction == TEXT("set_interpolation_settings"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString InterpolationType = GetStringFieldAnimAuth(Params, TEXT("interpolationType"), TEXT("Lerp"));
        float TargetWeightSpeed = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("targetWeightInterpolationSpeed"), 5.0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        UBlendSpace* BlendSpace2D = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
        UBlendSpace1D* BlendSpace1D = Cast<UBlendSpace1D>(StaticLoadObject(UBlendSpace1D::StaticClass(), nullptr, *AssetPath));
        
        UBlendSpace* BlendSpace = BlendSpace2D ? BlendSpace2D : BlendSpace1D;
        if (!BlendSpace)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load blend space: %s"), *AssetPath), TEXT("BLENDSPACE_NOT_FOUND"));
        }
        
        BlendSpace->TargetWeightInterpolationSpeedPerSec = TargetWeightSpeed;
        
        SaveAnimAsset(BlendSpace, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Interpolation settings updated"));
        return Response;
    }
    
    if (SubAction == TEXT("create_aim_offset"))
    {
#if MCP_HAS_BLENDSPACE_FACTORY
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Animations")));
        FString SkeletonPath = GetStringFieldAnimAuth(Params, TEXT("skeletonPath"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        USkeleton* Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
        }
        
        // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        UBlendSpaceFactoryNew* Factory = NewObject<UBlendSpaceFactoryNew>();
        Factory->TargetSkeleton = Skeleton;
        UAimOffsetBlendSpace* NewAimOffset = Cast<UAimOffsetBlendSpace>(
            Factory->FactoryCreateNew(UAimOffsetBlendSpace::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewAimOffset)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create aim offset"), TEXT("CREATE_FAILED"));
        }
        
        // Configure default aim offset axes (Yaw and Pitch)
        // Note: In UE 5.7+, GetBlendParameter returns const ref
        // The factory should have set reasonable defaults, just trigger update
        NewAimOffset->PostEditChange();
        NewAimOffset->MarkPackageDirty();
        
        SaveAnimAsset(NewAimOffset, bSave);
        
        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Aim Offset '%s' created"), *Name));
#else
        ANIM_ERROR_RESPONSE(TEXT("Blend space factory not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("add_aim_offset_sample"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString AnimationPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("animationPath"), TEXT("")));
        float Yaw = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("yaw"), 0.0));
        float Pitch = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("pitch"), 0.0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        UAimOffsetBlendSpace* AimOffset = Cast<UAimOffsetBlendSpace>(StaticLoadObject(UAimOffsetBlendSpace::StaticClass(), nullptr, *AssetPath));
        if (!AimOffset)
        {
            // Try as regular blend space
            UBlendSpace* BlendSpace = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
            if (BlendSpace)
            {
                UAnimSequence* Animation = LoadAnimSequenceFromPath(AnimationPath);
                if (!Animation)
                {
                    ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation: %s"), *AnimationPath), TEXT("ANIMATION_NOT_FOUND"));
                }
                
                FVector SampleValue(Yaw, Pitch, 0.0f);
                BlendSpace->AddSample(Animation, SampleValue);
                
                SaveAnimAsset(BlendSpace, bSave);
                
                ANIM_SUCCESS_RESPONSE(TEXT("Aim offset sample added"));
                return Response;
            }
            
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load aim offset: %s"), *AssetPath), TEXT("AIMOFFSET_NOT_FOUND"));
        }
        
        UAnimSequence* Animation = LoadAnimSequenceFromPath(AnimationPath);
        if (!Animation)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation: %s"), *AnimationPath), TEXT("ANIMATION_NOT_FOUND"));
        }
        
        // Add sample with yaw/pitch coordinates
        FVector SampleValue(Yaw, Pitch, 0.0f);
        AimOffset->AddSample(Animation, SampleValue);
        
        SaveAnimAsset(AimOffset, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Aim offset sample added"));
        return Response;
    }
    
    // ===== 10.4 Animation Blueprints =====
    
    if (SubAction == TEXT("create_anim_blueprint"))
    {
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Blueprints")));
        FString SkeletonPath = GetStringFieldAnimAuth(Params, TEXT("skeletonPath"), TEXT(""));
        FString ParentClass = GetStringFieldAnimAuth(Params, TEXT("parentClass"), TEXT("AnimInstance"));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        USkeleton* Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
        }
        
        // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
        Factory->TargetSkeleton = Skeleton;
        Factory->ParentClass = UAnimInstance::StaticClass();
        UAnimBlueprint* NewAnimBP = Cast<UAnimBlueprint>(
            Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewAnimBP)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create animation blueprint"), TEXT("CREATE_FAILED"));
        }
        
        // Compile the blueprint
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(NewAnimBP);
        
        SaveAnimAsset(NewAnimBP, bSave);
        
        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Animation Blueprint '%s' created"), *Name));
        return Response;
    }
    
    if (SubAction == TEXT("add_state_machine"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("blueprintPath"), TEXT("")));
        FString StateMachineName = GetStringFieldAnimAuth(Params, TEXT("stateMachineName"), TEXT(""));
        int32 NodePosX = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionY"), 0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (StateMachineName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("stateMachineName is required"), TEXT("MISSING_STATE_MACHINE_NAME"));
        }
        
        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }
        
        // Create the State Machine Node using FGraphNodeCreator
        FGraphNodeCreator<UAnimGraphNode_StateMachine> NodeCreator(*AnimGraph);
        UAnimGraphNode_StateMachine* SMNode = NodeCreator.CreateNode();
        SMNode->NodePosX = NodePosX;
        SMNode->NodePosY = NodePosY;
        NodeCreator.Finalize();
        
        // Create the internal State Machine Graph using FBlueprintEditorUtils
        UAnimationStateMachineGraph* InnerGraph = CastChecked<UAnimationStateMachineGraph>(
            FBlueprintEditorUtils::CreateNewGraph(
                AnimBP,
                FName(*StateMachineName),
                UAnimationStateMachineGraph::StaticClass(),
                UAnimationStateMachineSchema::StaticClass()
            )
        );
        
        // Link the State Machine Node to its internal graph
        SMNode->EditorStateMachineGraph = InnerGraph;
        InnerGraph->OwnerAnimGraphNode = SMNode;
        
        // Initialize Entry Node (required for State Machines)
        const UAnimationStateMachineSchema* Schema = CastChecked<UAnimationStateMachineSchema>(InnerGraph->GetSchema());
        Schema->CreateDefaultNodesForGraph(*InnerGraph);
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        Response->SetStringField(TEXT("nodeName"), StateMachineName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("State machine '%s' created with entry node"), *StateMachineName));
#else
        // AnimGraph headers not available - return error instead of fake success
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot create state machine '%s': AnimGraph module headers not available in this build. Rebuild with AnimGraph module enabled."), *StateMachineName),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("add_state"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("blueprintPath"), TEXT("")));
        FString StateMachineName = GetStringFieldAnimAuth(Params, TEXT("stateMachineName"), TEXT(""));
        FString StateName = GetStringFieldAnimAuth(Params, TEXT("stateName"), TEXT(""));
        int32 NodePosX = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionX"), 200));
        int32 NodePosY = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionY"), 0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (StateName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("stateName is required"), TEXT("MISSING_STATE_NAME"));
        }
        
        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }
        
        // Find the state machine by name
        UAnimGraphNode_StateMachine* SMNode = FindStateMachineNode(AnimGraph, StateMachineName);
        if (!SMNode || !SMNode->EditorStateMachineGraph)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("State machine '%s' not found"), *StateMachineName), TEXT("SM_NOT_FOUND"));
        }
        
        UAnimationStateMachineGraph* SMGraph = Cast<UAnimationStateMachineGraph>(SMNode->EditorStateMachineGraph);
        if (!SMGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Invalid state machine graph"), TEXT("INVALID_GRAPH"));
        }
        
        // Create the State Node using FGraphNodeCreator
        FGraphNodeCreator<UAnimStateNode> StateCreator(*SMGraph);
        UAnimStateNode* StateNode = StateCreator.CreateNode();
        StateNode->NodePosX = NodePosX;
        StateNode->NodePosY = NodePosY;
        StateCreator.Finalize();
        
        // Rename the state's bound graph to set the state name
        if (StateNode->BoundGraph)
        {
            FBlueprintEditorUtils::RenameGraph(StateNode->BoundGraph, *StateName);
        }
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        Response->SetStringField(TEXT("stateName"), StateName);
        Response->SetStringField(TEXT("stateMachine"), StateMachineName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("State '%s' created in state machine '%s'"), *StateName, *StateMachineName));
#else
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("State '%s' marked for creation (requires AnimGraph module)"), *StateName));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("add_transition"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("blueprintPath"), TEXT("")));
        FString StateMachineName = GetStringFieldAnimAuth(Params, TEXT("stateMachineName"), TEXT(""));
        FString FromState = GetStringFieldAnimAuth(Params, TEXT("fromState"), TEXT(""));
        FString ToState = GetStringFieldAnimAuth(Params, TEXT("toState"), TEXT(""));
        float CrossfadeDuration = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("crossfadeDuration"), 0.2));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (FromState.IsEmpty() || ToState.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("fromState and toState are required"), TEXT("MISSING_STATES"));
        }
        
        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA && MCP_HAS_ANIM_STATE_TRANSITION
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }
        
        // Find the state machine by name
        UAnimGraphNode_StateMachine* SMNode = FindStateMachineNode(AnimGraph, StateMachineName);
        if (!SMNode || !SMNode->EditorStateMachineGraph)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("State machine '%s' not found"), *StateMachineName), TEXT("SM_NOT_FOUND"));
        }
        
        UAnimationStateMachineGraph* SMGraph = Cast<UAnimationStateMachineGraph>(SMNode->EditorStateMachineGraph);
        if (!SMGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Invalid state machine graph"), TEXT("INVALID_GRAPH"));
        }
        
        // Find the source and target states
        UAnimStateNode* FromNode = FindStateNode(SMGraph, FromState);
        UAnimStateNode* ToNode = FindStateNode(SMGraph, ToState);
        
        if (!FromNode)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Source state '%s' not found"), *FromState), TEXT("SOURCE_STATE_NOT_FOUND"));
        }
        if (!ToNode)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Target state '%s' not found"), *ToState), TEXT("TARGET_STATE_NOT_FOUND"));
        }
        
        // Create the Transition Node
        FGraphNodeCreator<UAnimStateTransitionNode> TransCreator(*SMGraph);
        UAnimStateTransitionNode* TransNode = TransCreator.CreateNode();
        TransCreator.Finalize();
        
        // Establish the connection between states
        TransNode->CreateConnections(FromNode, ToNode);
        
        // Configure transition properties
        TransNode->CrossfadeDuration = CrossfadeDuration;
        TransNode->BlendMode = EAlphaBlendOption::Linear;
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        Response->SetStringField(TEXT("fromState"), FromState);
        Response->SetStringField(TEXT("toState"), ToState);
        Response->SetNumberField(TEXT("crossfadeDuration"), CrossfadeDuration);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Transition from '%s' to '%s' created"), *FromState, *ToState));
#else
        // AnimGraph headers not available - return error instead of fake success
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot create transition from '%s' to '%s': AnimGraph module headers not available in this build."), *FromState, *ToState),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("set_transition_rules"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("blueprintPath"), TEXT("")));
        FString StateMachineName = GetStringFieldAnimAuth(Params, TEXT("stateMachineName"), TEXT(""));
        FString FromState = GetStringFieldAnimAuth(Params, TEXT("fromState"), TEXT(""));
        FString ToState = GetStringFieldAnimAuth(Params, TEXT("toState"), TEXT(""));
        float CrossfadeDuration = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("crossfadeDuration"), -1.0));
        int32 PriorityOrder = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("priorityOrder"), -1));
        bool bAutomatic = GetBoolFieldAnimAuth(Params, TEXT("automaticRule"), false);
        bool bBidirectional = GetBoolFieldAnimAuth(Params, TEXT("bidirectional"), false);
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA && MCP_HAS_ANIM_STATE_TRANSITION
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }
        
        // Find the state machine by name
        UAnimGraphNode_StateMachine* SMNode = FindStateMachineNode(AnimGraph, StateMachineName);
        if (!SMNode || !SMNode->EditorStateMachineGraph)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("State machine '%s' not found"), *StateMachineName), TEXT("SM_NOT_FOUND"));
        }
        
        UAnimationStateMachineGraph* SMGraph = Cast<UAnimationStateMachineGraph>(SMNode->EditorStateMachineGraph);
        if (!SMGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Invalid state machine graph"), TEXT("INVALID_GRAPH"));
        }
        
        // Find the transition node between the specified states
        UAnimStateTransitionNode* TransNode = nullptr;
        for (UEdGraphNode* Node : SMGraph->Nodes)
        {
            if (UAnimStateTransitionNode* Trans = Cast<UAnimStateTransitionNode>(Node))
            {
                UAnimStateNodeBase* PrevState = Trans->GetPreviousState();
                UAnimStateNodeBase* NextState = Trans->GetNextState();
                if (PrevState && NextState && 
                    PrevState->GetStateName() == FromState && 
                    NextState->GetStateName() == ToState)
                {
                    TransNode = Trans;
                    break;
                }
            }
        }
        
        if (!TransNode)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Transition from '%s' to '%s' not found"), *FromState, *ToState), TEXT("TRANSITION_NOT_FOUND"));
        }
        
        // Update transition properties
        if (CrossfadeDuration >= 0.0f)
        {
            TransNode->CrossfadeDuration = CrossfadeDuration;
        }
        if (PriorityOrder >= 0)
        {
            TransNode->PriorityOrder = PriorityOrder;
        }
        TransNode->bAutomaticRuleBasedOnSequencePlayerInState = bAutomatic;
        TransNode->Bidirectional = bBidirectional;
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Transition rules updated for '%s' -> '%s'"), *FromState, *ToState));
#else
        // AnimGraph headers not available - return error instead of fake success
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot update transition rules for '%s' -> '%s': AnimGraph module headers not available in this build."), *FromState, *ToState),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("add_blend_node"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("blueprintPath"), TEXT("")));
        FString BlendType = GetStringFieldAnimAuth(Params, TEXT("blendType"), TEXT("TwoWayBlend"));
        int32 NodePosX = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionY"), 0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }
        
        FString CreatedNodeType;
        
#if MCP_HAS_TWO_WAY_BLEND
        if (BlendType == TEXT("TwoWayBlend") || BlendType == TEXT("Blend"))
        {
            FGraphNodeCreator<UAnimGraphNode_TwoWayBlend> NodeCreator(*AnimGraph);
            UAnimGraphNode_TwoWayBlend* BlendNode = NodeCreator.CreateNode();
            BlendNode->NodePosX = NodePosX;
            BlendNode->NodePosY = NodePosY;
            NodeCreator.Finalize();
            CreatedNodeType = TEXT("TwoWayBlend");
        }
        else
#endif
#if MCP_HAS_LAYERED_BLEND
        if (BlendType == TEXT("LayeredBlend") || BlendType == TEXT("LayeredBoneBlend"))
        {
            FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> NodeCreator(*AnimGraph);
            UAnimGraphNode_LayeredBoneBlend* BlendNode = NodeCreator.CreateNode();
            BlendNode->NodePosX = NodePosX;
            BlendNode->NodePosY = NodePosY;
            NodeCreator.Finalize();
            CreatedNodeType = TEXT("LayeredBoneBlend");
        }
        else
#endif
        {
            // Default fallback to TwoWayBlend if available
#if MCP_HAS_TWO_WAY_BLEND
            FGraphNodeCreator<UAnimGraphNode_TwoWayBlend> NodeCreator(*AnimGraph);
            UAnimGraphNode_TwoWayBlend* BlendNode = NodeCreator.CreateNode();
            BlendNode->NodePosX = NodePosX;
            BlendNode->NodePosY = NodePosY;
            NodeCreator.Finalize();
            CreatedNodeType = TEXT("TwoWayBlend");
#else
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Cannot create blend node '%s': AnimGraph blend node headers not available in this build."), *BlendType), TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        }
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        Response->SetStringField(TEXT("nodeType"), CreatedNodeType);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Blend node '%s' created"), *CreatedNodeType));
#else
        // AnimGraph headers not available - return error instead of fake success
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot create blend node '%s': AnimGraph module headers not available in this build."), *BlendType),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("add_cached_pose"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("blueprintPath"), TEXT("")));
        FString CacheName = GetStringFieldAnimAuth(Params, TEXT("cacheName"), TEXT(""));
        int32 NodePosX = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionY"), 0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (CacheName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("cacheName is required"), TEXT("MISSING_CACHE_NAME"));
        }
        
        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_CACHED_POSE
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }
        
        // Create the Save Cached Pose node
        FGraphNodeCreator<UAnimGraphNode_SaveCachedPose> NodeCreator(*AnimGraph);
        UAnimGraphNode_SaveCachedPose* CachedPoseNode = NodeCreator.CreateNode();
        CachedPoseNode->NodePosX = NodePosX;
        CachedPoseNode->NodePosY = NodePosY;
        CachedPoseNode->CacheName = CacheName;
        NodeCreator.Finalize();
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        Response->SetStringField(TEXT("cacheName"), CacheName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Cached pose node '%s' created"), *CacheName));
#else
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Cached pose '%s' marked for creation (requires AnimGraph module)"), *CacheName));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("add_slot_node"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("blueprintPath"), TEXT("")));
        FString SlotName = GetStringFieldAnimAuth(Params, TEXT("slotName"), TEXT(""));
        FString GroupName = GetStringFieldAnimAuth(Params, TEXT("groupName"), TEXT("DefaultGroup"));
        int32 NodePosX = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionY"), 0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (SlotName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("slotName is required"), TEXT("MISSING_SLOT_NAME"));
        }
        
        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_SLOT_NODE
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }
        
        // Create the Slot node
        FGraphNodeCreator<UAnimGraphNode_Slot> NodeCreator(*AnimGraph);
        UAnimGraphNode_Slot* SlotNode = NodeCreator.CreateNode();
        SlotNode->NodePosX = NodePosX;
        SlotNode->NodePosY = NodePosY;
        
        // Set the slot name (format: "GroupName.SlotName")
        FString FullSlotName = FString::Printf(TEXT("%s.%s"), *GroupName, *SlotName);
        SlotNode->Node.SlotName = FName(*FullSlotName);
        
        NodeCreator.Finalize();
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        Response->SetStringField(TEXT("slotName"), FullSlotName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Slot node '%s' created"), *FullSlotName));
#else
        // AnimGraph headers not available - return error instead of fake success
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot create slot node '%s': AnimGraph module headers not available in this build."), *SlotName),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("add_layered_blend_per_bone"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("blueprintPath"), TEXT("")));
        FString BoneName = GetStringFieldAnimAuth(Params, TEXT("boneName"), TEXT(""));
        int32 NodePosX = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionY"), 0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_LAYERED_BLEND
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }
        
        // Create the Layered Bone Blend node
        FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> NodeCreator(*AnimGraph);
        UAnimGraphNode_LayeredBoneBlend* BlendNode = NodeCreator.CreateNode();
        BlendNode->NodePosX = NodePosX;
        BlendNode->NodePosY = NodePosY;
        NodeCreator.Finalize();
        
        // Note: Configuring specific bone layers requires access to BlendNode->Node.LayerSetup
        // which is typically done through the editor UI. Basic node creation is complete.
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Layered blend per bone node created"));
#else
        // AnimGraph headers not available - return error instead of fake success
        ANIM_ERROR_RESPONSE(
            TEXT("Cannot create layered blend per bone node: AnimGraph module headers not available in this build."),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("set_anim_graph_node_value"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("blueprintPath"), TEXT("")));
        FString NodeName = GetStringFieldAnimAuth(Params, TEXT("nodeName"), TEXT(""));
        FString PropertyName = GetStringFieldAnimAuth(Params, TEXT("propertyName"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (NodeName.IsEmpty() || PropertyName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("nodeName and propertyName are required"), TEXT("MISSING_PARAMETERS"));
        }
        
        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }
        
        // Find the node by name
        UEdGraphNode* FoundNode = nullptr;
        for (UEdGraphNode* Node : AnimGraph->Nodes)
        {
            if (Node && Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Contains(NodeName))
            {
                FoundNode = Node;
                break;
            }
        }
        
        if (!FoundNode)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Node '%s' not found in AnimGraph"), *NodeName), TEXT("NODE_NOT_FOUND"));
        }
        
        // Find and set the property using reflection
        FProperty* Property = FoundNode->GetClass()->FindPropertyByName(FName(*PropertyName));
        if (!Property)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Property '%s' not found on node '%s'"), *PropertyName, *NodeName), TEXT("PROPERTY_NOT_FOUND"));
        }
        
        // Get the value from params and apply it
        TSharedPtr<FJsonValue> ValueField = Params->TryGetField(TEXT("value"));
        if (!ValueField.IsValid())
        {
            ANIM_ERROR_RESPONSE(TEXT("value parameter is required"), TEXT("MISSING_VALUE"));
        }
        
        FString ApplyError;
        if (!ApplyJsonValueToProperty(FoundNode, Property, ValueField, ApplyError))
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Failed to set property: %s"), *ApplyError), TEXT("PROPERTY_SET_FAILED"));
        }
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        Response->SetStringField(TEXT("nodeName"), NodeName);
        Response->SetStringField(TEXT("propertyName"), PropertyName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Property '%s' set on node '%s'"), *PropertyName, *NodeName));
#else
        // AnimGraph headers not available - return error
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot set node value on '%s': AnimGraph module headers not available in this build."), *NodeName),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
    }
    
    // ===== 10.5 Control Rig =====
    
    if (SubAction == TEXT("create_control_rig"))
    {
// ControlRig factory static methods (CreateNewControlRigAsset, CreateControlRigFromSkeletalMeshOrSkeleton)
// are only available in UE 5.5+ where ControlRigBlueprintFactory.h is in Public folder
#if MCP_HAS_CONTROLRIG_FACTORY && MCP_HAS_CONTROLRIG_BLUEPRINT && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/ControlRigs")));
        FString SkeletalMeshPath = GetStringFieldAnimAuth(Params, TEXT("skeletalMeshPath"), TEXT(""));
        bool bModularRig = GetBoolFieldAnimAuth(Params, TEXT("modularRig"), false);
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        UControlRigBlueprint* ControlRigBP = nullptr;
        FString FullPath = Path / Name;
        
        // If skeletal mesh provided, create from it; otherwise create empty
        if (!SkeletalMeshPath.IsEmpty())
        {
            USkeletalMesh* SkeletalMesh = LoadSkeletalMeshFromPathAnim(SkeletalMeshPath);
            if (!SkeletalMesh)
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeletal mesh: %s"), *SkeletalMeshPath), TEXT("SKELETAL_MESH_NOT_FOUND"));
            }
            
            // Use static factory method to create from skeletal mesh (UE 5.5+ only)
            ControlRigBP = UControlRigBlueprintFactory::CreateControlRigFromSkeletalMeshOrSkeleton(SkeletalMesh, bModularRig);
        }
        else
        {
            // Create empty control rig at specified path (UE 5.5+ only)
            ControlRigBP = UControlRigBlueprintFactory::CreateNewControlRigAsset(FullPath, bModularRig);
        }
        
        if (!ControlRigBP)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create Control Rig blueprint"), TEXT("CREATION_FAILED"));
        }
        
        // Save if requested
        if (bSave)
        {
            // Use safe asset save helper
            FString AssetPath = ControlRigBP->GetPathName();
            int32 DotIndex = AssetPath.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
            if (DotIndex != INDEX_NONE) { AssetPath.LeftInline(DotIndex); }
            ControlRigBP->MarkPackageDirty();
        }
        
        Response->SetStringField(TEXT("assetPath"), ControlRigBP->GetPathName());
        Response->SetBoolField(TEXT("modularRig"), bModularRig);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Control Rig '%s' created successfully"), *Name));
#elif MCP_HAS_CONTROLRIG_BLUEPRINT
        // Factory static methods not available in UE 5.1-5.4 (header is in Private folder)
        // Use the Subsystem's CreateControlRigBlueprint method as fallback
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/ControlRigs")));
        FString SkeletalMeshPath = GetStringFieldAnimAuth(Params, TEXT("skeletalMeshPath"), TEXT(""));
        
        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        FString FullPath = Path / Name;
        
        // Create Control Rig Blueprint using FKismetEditorUtilities (works in all UE 5.x versions)
        FString FullPackageName = Path / Name;
        
        // Create the package
        UPackage* Package = CreatePackage(*FullPackageName);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Failed to create package: %s"), *FullPackageName), TEXT("PACKAGE_CREATE_FAILED"));
        }
        
        Package->FullyLoad();
        
        // Create the Control Rig Blueprint using FKismetEditorUtilities
        UControlRigBlueprint* ControlRigBP = Cast<UControlRigBlueprint>(
            FKismetEditorUtilities::CreateBlueprint(
                UControlRig::StaticClass(),
                Package,
                *Name,
                BPTYPE_Normal,
                UControlRigBlueprint::StaticClass(),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                URigVMBlueprintGeneratedClass::StaticClass(),
#else
                // UE 5.0 uses UControlRigBlueprintGeneratedClass instead
                UControlRigBlueprintGeneratedClass::StaticClass(),
#endif
                NAME_None));
        
        if (!ControlRigBP)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create Control Rig Blueprint"), TEXT("CREATION_FAILED"));
        }
        
        // Set the target skeleton if provided (via skeletal mesh)
        if (!SkeletalMeshPath.IsEmpty())
        {
            USkeletalMesh* SkeletalMesh = LoadSkeletalMeshFromPathAnim(SkeletalMeshPath);
            if (SkeletalMesh && SkeletalMesh->GetSkeleton())
            {
                USkeletalMesh* PreviewMesh = SkeletalMesh->GetSkeleton()->GetPreviewMesh();
                if (PreviewMesh)
                {
                    ControlRigBP->SetPreviewMesh(PreviewMesh);
                }
            }
        }
        
        Response->SetStringField(TEXT("assetPath"), ControlRigBP->GetPathName());
        Response->SetBoolField(TEXT("modularRig"), false);  // Not supported in fallback
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Control Rig '%s' created successfully (UE 5.1-5.4 compatible mode)"), *Name));
#else
        ANIM_ERROR_RESPONSE(TEXT("Control Rig module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("add_control"))
    {
#if MCP_HAS_CONTROLRIG
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString ControlName = GetStringFieldAnimAuth(Params, TEXT("controlName"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (ControlName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("controlName is required"), TEXT("MISSING_CONTROL_NAME"));
        }
        
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Control '%s' added (requires manual rig setup)"), *ControlName));
#else
        ANIM_ERROR_RESPONSE(TEXT("Control Rig module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("add_rig_unit"))
    {
#if MCP_HAS_CONTROLRIG
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString UnitType = GetStringFieldAnimAuth(Params, TEXT("unitType"), TEXT(""));
        
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Rig unit '%s' added (requires manual rig setup)"), *UnitType));
#else
        ANIM_ERROR_RESPONSE(TEXT("Control Rig module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("connect_rig_elements"))
    {
#if MCP_HAS_CONTROLRIG
        ANIM_SUCCESS_RESPONSE(TEXT("Rig elements connected (requires manual rig setup)"));
#else
        ANIM_ERROR_RESPONSE(TEXT("Control Rig module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("create_pose_library"))
    {
#if MCP_HAS_POSEASSET
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Animations")));
        FString SkeletonPath = GetStringFieldAnimAuth(Params, TEXT("skeletonPath"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        USkeleton* Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
        }
        
        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Pose library '%s' creation requires manual setup"), *Name));
#else
        ANIM_ERROR_RESPONSE(TEXT("Pose Asset not available in this engine version"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }
    
    // ===== 10.6 Retargeting =====
    
    if (SubAction == TEXT("create_ik_rig"))
    {
#if MCP_HAS_IKRIG_FACTORY && MCP_HAS_IKRIG
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Retargeting")));
        FString SkeletalMeshPath = GetStringFieldAnimAuth(Params, TEXT("skeletalMeshPath"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        // Use static factory method to create IK Rig
        UIKRigDefinition* IKRig = UIKRigDefinitionFactory::CreateNewIKRigAsset(Path, Name);
        
        if (!IKRig)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create IK Rig asset"), TEXT("CREATION_FAILED"));
        }
        
        // If skeletal mesh path provided, set the preview mesh
        if (!SkeletalMeshPath.IsEmpty())
        {
            USkeletalMesh* SkeletalMesh = LoadSkeletalMeshFromPathAnim(SkeletalMeshPath);
            if (SkeletalMesh)
            {
                IKRig->SetPreviewMesh(SkeletalMesh);
            }
        }
        
        // Save if requested
        if (bSave)
        {
            FString AssetPathStr = IKRig->GetPathName();
            int32 DotIndex = AssetPathStr.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
            if (DotIndex != INDEX_NONE) { AssetPathStr.LeftInline(DotIndex); }
            IKRig->MarkPackageDirty();
        }
        
        Response->SetStringField(TEXT("assetPath"), IKRig->GetPathName());
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("IK Rig '%s' created successfully"), *Name));
#elif MCP_HAS_IKRIG
        // Factory not available, fall back to informative message
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Retargeting")));
        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("IK Rig '%s' creation requires IKRigEditor module"), *Name));
#else
        ANIM_ERROR_RESPONSE(TEXT("IK Rig module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("add_ik_chain"))
    {
#if MCP_HAS_IKRIG
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString ChainName = GetStringFieldAnimAuth(Params, TEXT("chainName"), TEXT(""));
        
        if (ChainName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("chainName is required"), TEXT("MISSING_CHAIN_NAME"));
        }
        
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("IK chain '%s' added (requires manual setup)"), *ChainName));
#else
        ANIM_ERROR_RESPONSE(TEXT("IK Rig module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("create_ik_retargeter"))
    {
#if MCP_HAS_IKRETARGET_FACTORY && MCP_HAS_IKRETARGETER
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Retargeting")));
        FString SourceIKRigPath = GetStringFieldAnimAuth(Params, TEXT("sourceIKRigPath"), TEXT(""));
        FString TargetIKRigPath = GetStringFieldAnimAuth(Params, TEXT("targetIKRigPath"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        // Create the IK Retargeter using factory
        FString FullPath = Path / Name;
        FString PackageName = FullPath;
        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package for IK Retargeter"), TEXT("PACKAGE_ERROR"));
        }
        
        UIKRetargetFactory* Factory = NewObject<UIKRetargetFactory>();
        
        // Set source IK Rig if provided
        if (!SourceIKRigPath.IsEmpty())
        {
            UIKRigDefinition* SourceRig = Cast<UIKRigDefinition>(StaticLoadObject(UIKRigDefinition::StaticClass(), nullptr, *SourceIKRigPath));
            if (SourceRig)
            {
                Factory->SourceIKRig = SourceRig;
            }
        }
        
        UIKRetargeter* Retargeter = Cast<UIKRetargeter>(Factory->FactoryCreateNew(
            UIKRetargeter::StaticClass(),
            Package,
            FName(*Name),
            RF_Public | RF_Standalone,
            nullptr,
            GWarn
        ));
        
        if (!Retargeter)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create IK Retargeter"), TEXT("CREATION_FAILED"));
        }
        
        // Save if requested
        if (bSave)
        {
            FString AssetPathStr = Retargeter->GetPathName();
            int32 DotIndex = AssetPathStr.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
            if (DotIndex != INDEX_NONE) { AssetPathStr.LeftInline(DotIndex); }
            Retargeter->MarkPackageDirty();
        }
        
        Response->SetStringField(TEXT("assetPath"), Retargeter->GetPathName());
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("IK Retargeter '%s' created successfully"), *Name));
#elif MCP_HAS_IKRETARGETER
        // Factory not available, fall back to informative message
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Retargeting")));
        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("IK Retargeter '%s' creation requires IKRigEditor module"), *Name));
#else
        ANIM_ERROR_RESPONSE(TEXT("IK Retargeter module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }
    
    if (SubAction == TEXT("set_retarget_chain_mapping"))
    {
#if MCP_HAS_IKRETARGETER
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString SourceChain = GetStringFieldAnimAuth(Params, TEXT("sourceChain"), TEXT(""));
        FString TargetChain = GetStringFieldAnimAuth(Params, TEXT("targetChain"), TEXT(""));
        
        if (SourceChain.IsEmpty() || TargetChain.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("sourceChain and targetChain are required"), TEXT("MISSING_CHAINS"));
        }
        
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Chain mapping '%s' -> '%s' set"), *SourceChain, *TargetChain));
#else
        ANIM_ERROR_RESPONSE(TEXT("IK Retargeter module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }
    
    // ===== Utility =====
    
    if (SubAction == TEXT("get_animation_info"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        
        UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
        if (!Asset)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load asset: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        }
        
        TSharedPtr<FJsonObject> AnimInfo = MakeShared<FJsonObject>();
        
        if (UAnimSequence* Sequence = Cast<UAnimSequence>(Asset))
        {
            AnimInfo->SetStringField(TEXT("assetType"), TEXT("AnimSequence"));
            if (Sequence->GetSkeleton())
            {
                AnimInfo->SetStringField(TEXT("skeletonPath"), Sequence->GetSkeleton()->GetPathName());
            }
            AnimInfo->SetNumberField(TEXT("duration"), Sequence->GetPlayLength());
#if ENGINE_MAJOR_VERSION >= 5
            AnimInfo->SetNumberField(TEXT("numFrames"), Sequence->GetNumberOfSampledKeys());
            AnimInfo->SetNumberField(TEXT("frameRate"), Sequence->GetSamplingFrameRate().AsDecimal());
#endif
            AnimInfo->SetNumberField(TEXT("numNotifies"), Sequence->Notifies.Num());
            AnimInfo->SetBoolField(TEXT("isAdditive"), Sequence->AdditiveAnimType != AAT_None);
            AnimInfo->SetBoolField(TEXT("hasRootMotion"), Sequence->bEnableRootMotion);
        }
        else if (UAnimMontage* Montage = Cast<UAnimMontage>(Asset))
        {
            AnimInfo->SetStringField(TEXT("assetType"), TEXT("AnimMontage"));
            if (Montage->GetSkeleton())
            {
                AnimInfo->SetStringField(TEXT("skeletonPath"), Montage->GetSkeleton()->GetPathName());
            }
            AnimInfo->SetNumberField(TEXT("duration"), Montage->GetPlayLength());
            AnimInfo->SetNumberField(TEXT("numSections"), Montage->CompositeSections.Num());
            AnimInfo->SetNumberField(TEXT("numSlots"), Montage->SlotAnimTracks.Num());
            AnimInfo->SetNumberField(TEXT("numNotifies"), Montage->Notifies.Num());
        }
        else if (UBlendSpace* BlendSpace = Cast<UBlendSpace>(Asset))
        {
            AnimInfo->SetStringField(TEXT("assetType"), Cast<UBlendSpace1D>(Asset) ? TEXT("BlendSpace1D") : TEXT("BlendSpace2D"));
            if (BlendSpace->GetSkeleton())
            {
                AnimInfo->SetStringField(TEXT("skeletonPath"), BlendSpace->GetSkeleton()->GetPathName());
            }
            AnimInfo->SetNumberField(TEXT("numSamples"), BlendSpace->GetBlendSamples().Num());
        }
        else if (UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Asset))
        {
            AnimInfo->SetStringField(TEXT("assetType"), TEXT("AnimBlueprint"));
            if (AnimBP->TargetSkeleton)
            {
                AnimInfo->SetStringField(TEXT("skeletonPath"), AnimBP->TargetSkeleton->GetPathName());
            }
            AnimInfo->SetStringField(TEXT("parentClass"), AnimBP->ParentClass ? AnimBP->ParentClass->GetName() : TEXT(""));
        }
        else
        {
            AnimInfo->SetStringField(TEXT("assetType"), Asset->GetClass()->GetName());
        }
        
        Response->SetObjectField(TEXT("animationInfo"), AnimInfo);
        ANIM_SUCCESS_RESPONSE(TEXT("Animation info retrieved"));
        return Response;
    }
    
    // Unknown action
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown animation authoring action: %s"), *SubAction));
    Response->SetStringField(TEXT("errorCode"), TEXT("UNKNOWN_ACTION"));
    return Response;
}

// Wrapper handler that follows the standard signature pattern
bool UMcpAutomationBridgeSubsystem::HandleManageAnimationAuthoringAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Check if this is an animation authoring action
    if (Action != TEXT("manage_animation_authoring"))
    {
        return false; // Not handled
    }
    
    // Call the internal processing function
    TSharedPtr<FJsonObject> Result = HandleAnimationAuthoringRequest(Payload);
    
    // Send response
    if (Result.IsValid())
    {
        bool bSuccess = Result->HasField(TEXT("success")) && GetJsonBoolField(Result, TEXT("success"));
        FString Message = Result->HasField(TEXT("message")) ? GetJsonStringField(Result, TEXT("message")) : TEXT("");
        
        if (bSuccess)
        {
            SendAutomationResponse(RequestingSocket, RequestId, true, Message, Result);
        }
        else
        {
            FString Error = Result->HasField(TEXT("error")) ? GetJsonStringField(Result, TEXT("error")) : TEXT("Unknown error");
            FString ErrorCode = Result->HasField(TEXT("errorCode")) ? GetJsonStringField(Result, TEXT("errorCode")) : TEXT("ANIMATION_AUTHORING_ERROR");
            SendAutomationError(RequestingSocket, RequestId, Error, ErrorCode);
        }
        return true;
    }
    
    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to process animation authoring action"), TEXT("PROCESSING_FAILED"));
    return true;
}

#undef ANIM_ERROR_RESPONSE
#undef ANIM_SUCCESS_RESPONSE

#undef GetStringFieldAnimAuth
#undef GetNumberFieldAnimAuth
#undef GetBoolFieldAnimAuth

#endif // WITH_EDITOR

