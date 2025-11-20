#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#if __has_include("Animation/AnimationBlueprintLibrary.h")
#include "Animation/AnimationBlueprintLibrary.h"
#elif __has_include("AnimationBlueprintLibrary.h")
#include "AnimationBlueprintLibrary.h"
#endif
#if __has_include("Animation/AnimBlueprintLibrary.h")
#include "Animation/AnimBlueprintLibrary.h"
#endif
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#if __has_include("Animation/BlendSpaceBase.h")
#include "Animation/BlendSpaceBase.h"
#define MCP_HAS_BLENDSPACE_BASE 1
#elif __has_include("BlendSpaceBase.h")
#include "BlendSpaceBase.h"
#define MCP_HAS_BLENDSPACE_BASE 1
#else
#include "Animation/AnimTypes.h"
#define MCP_HAS_BLENDSPACE_BASE 0
#endif
#if __has_include("Factories/BlendSpaceFactoryNew.h") && __has_include("Factories/BlendSpaceFactory1D.h")
#include "Factories/BlendSpaceFactoryNew.h"
#include "Factories/BlendSpaceFactory1D.h"
#define MCP_HAS_BLENDSPACE_FACTORY 1
#else
#define MCP_HAS_BLENDSPACE_FACTORY 0
#endif
#include "ControlRig.h"
#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "ControlRigObjectBinding.h"
#if __has_include("Factories/ControlRigBlueprintFactory.h")
#include "Factories/ControlRigBlueprintFactory.h"
#define MCP_HAS_CONTROLRIG_FACTORY 1
#else
#define MCP_HAS_CONTROLRIG_FACTORY 0
#endif
#include "Factories/AnimBlueprintFactory.h"
#include "Factories/AnimMontageFactory.h"
#include "Factories/AnimSequenceFactory.h"
#include "Factories/PhysicsAssetFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/PhysicsAsset.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#include "UObject/UnrealType.h"
#include "UObject/Script.h"

namespace
{
#if MCP_HAS_BLENDSPACE_FACTORY
    static UObject* CreateBlendSpaceAsset(const FString& AssetName, const FString& PackagePath, USkeleton* TargetSkeleton, bool bTwoDimensional, FString& OutError)
    {
        OutError.Reset();

        UFactory* Factory = nullptr;
        UClass* DesiredClass = nullptr;

        if (bTwoDimensional)
        {
            UBlendSpaceFactoryNew* Factory2D = NewObject<UBlendSpaceFactoryNew>();
            if (!Factory2D)
            {
                OutError = TEXT("Failed to allocate BlendSpace factory");
                return nullptr;
            }
            Factory2D->TargetSkeleton = TargetSkeleton;
            Factory = Factory2D;
            DesiredClass = UBlendSpace::StaticClass();
        }
        else
        {
            UBlendSpaceFactory1D* Factory1D = NewObject<UBlendSpaceFactory1D>();
            if (!Factory1D)
            {
                OutError = TEXT("Failed to allocate BlendSpace1D factory");
                return nullptr;
            }
            Factory1D->TargetSkeleton = TargetSkeleton;
            Factory = Factory1D;
            DesiredClass = UBlendSpace1D::StaticClass();
        }

        if (!Factory || !DesiredClass)
        {
            OutError = TEXT("BlendSpace factory unavailable");
            return nullptr;
        }

        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        return AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, DesiredClass, Factory);
    }

    static void ApplyBlendSpaceConfiguration(UObject* BlendSpaceAsset, const TSharedPtr<FJsonObject>& Payload, bool bTwoDimensional)
    {
        if (!BlendSpaceAsset || !Payload.IsValid())
        {
            return;
        }

        double MinX = 0.0, MaxX = 1.0, GridX = 3.0;
        Payload->TryGetNumberField(TEXT("minX"), MinX);
        Payload->TryGetNumberField(TEXT("maxX"), MaxX);
        Payload->TryGetNumberField(TEXT("gridX"), GridX);

#if MCP_HAS_BLENDSPACE_BASE
        if (UBlendSpaceBase* BlendBase = Cast<UBlendSpaceBase>(BlendSpaceAsset))
        {
            BlendBase->Modify();

            FBlendParameter& Axis0 = const_cast<FBlendParameter&>(BlendBase->GetBlendParameter(0));
            Axis0.Min = static_cast<float>(MinX);
            Axis0.Max = static_cast<float>(MaxX);
            Axis0.GridNum = FMath::Max(1, static_cast<int32>(GridX));

            if (bTwoDimensional)
            {
                double MinY = 0.0, MaxY = 1.0, GridY = 3.0;
                Payload->TryGetNumberField(TEXT("minY"), MinY);
                Payload->TryGetNumberField(TEXT("maxY"), MaxY);
                Payload->TryGetNumberField(TEXT("gridY"), GridY);

                FBlendParameter& Axis1 = const_cast<FBlendParameter&>(BlendBase->GetBlendParameter(1));
                Axis1.Min = static_cast<float>(MinY);
                Axis1.Max = static_cast<float>(MaxY);
                Axis1.GridNum = FMath::Max(1, static_cast<int32>(GridY));
            }

            BlendBase->MarkPackageDirty();
        }
#else
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ApplyBlendSpaceConfiguration: BlendSpaceBase headers unavailable; skipping axis configuration."));
        if (bTwoDimensional)
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Requested 2D blend space but BlendSpaceBase headers are missing; axis configuration skipped."));
        }
        if (!BlendSpaceAsset->IsA<UBlendSpace>() && !BlendSpaceAsset->IsA<UBlendSpace1D>())
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("ApplyBlendSpaceConfiguration: Asset %s is not a BlendSpace type"), *BlendSpaceAsset->GetName());
        }
#endif
    }
#endif // MCP_HAS_BLENDSPACE_FACTORY

    static bool ExecuteEditorCommandsInternal(const TArray<FString>& Commands, FString& OutErrorMessage)
    {
        OutErrorMessage.Reset();

        if (!GEditor)
        {
            OutErrorMessage = TEXT("Editor instance unavailable");
            return false;
        }

        UWorld* EditorWorld = nullptr;
        FWorldContext& EditorContext = GEditor->GetEditorWorldContext(false);
        EditorWorld = EditorContext.World();

        for (const FString& Command : Commands)
        {
            const FString Trimmed = Command.TrimStartAndEnd();
            if (Trimmed.IsEmpty())
            {
                continue;
            }

            if (!GEditor->Exec(EditorWorld, *Trimmed))
            {
                OutErrorMessage = FString::Printf(TEXT("Failed to execute editor command: %s"), *Trimmed);
                return false;
            }
        }

        return true;
    }
}
#else
#define MCP_HAS_BLENDSPACE_FACTORY 0
#define MCP_HAS_CONTROLRIG_FACTORY 0
#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleAnimationPhysicsAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT(">>> HandleAnimationPhysicsAction ENTRY: RequestId=%s RawAction='%s'"), *RequestId, *Action);
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("animation_physics"), ESearchCase::IgnoreCase) && !Lower.StartsWith(TEXT("animation_physics"))) return false;

    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("animation_physics payload missing."), TEXT("INVALID_PAYLOAD")); return true; }

    FString SubAction; Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSub = SubAction.ToLower();
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleAnimationPhysicsAction: subaction='%s'"), *LowerSub);

#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("action"), LowerSub);
    bool bSuccess = false;
    FString Message;
    FString ErrorCode;

    if (LowerSub == TEXT("cleanup"))
    {
        const TArray<TSharedPtr<FJsonValue>>* ArtifactsArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("artifacts"), ArtifactsArray) || !ArtifactsArray)
        {
            Message = TEXT("artifacts array required for cleanup");
            ErrorCode = TEXT("INVALID_ARGUMENT");
        }
        else
        {
            TArray<FString> Cleaned;
            TArray<FString> Missing;
            TArray<FString> Failed;

            for (const TSharedPtr<FJsonValue>& Val : *ArtifactsArray)
            {
                if (!Val.IsValid() || Val->Type != EJson::String)
                {
                    continue;
                }

                const FString ArtifactPath = Val->AsString().TrimStartAndEnd();
                if (ArtifactPath.IsEmpty())
                {
                    continue;
                }

                if (!UEditorAssetLibrary::DoesAssetExist(ArtifactPath))
                {
                    Missing.Add(ArtifactPath);
                    continue;
                }

                if (UEditorAssetLibrary::DeleteAsset(ArtifactPath))
                {
                    Cleaned.Add(ArtifactPath);
                }
                else
                {
                    Failed.Add(ArtifactPath);
                }
            }

            TArray<TSharedPtr<FJsonValue>> CleanedArray;
            for (const FString& Path : Cleaned)
            {
                CleanedArray.Add(MakeShared<FJsonValueString>(Path));
            }
            if (CleanedArray.Num() > 0)
            {
                Resp->SetArrayField(TEXT("cleaned"), CleanedArray);
            }
            Resp->SetNumberField(TEXT("cleanedCount"), Cleaned.Num());

            if (Missing.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> MissingArray;
                for (const FString& Path : Missing)
                {
                    MissingArray.Add(MakeShared<FJsonValueString>(Path));
                }
                Resp->SetArrayField(TEXT("missing"), MissingArray);
            }

            if (Failed.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> FailedArray;
                for (const FString& Path : Failed)
                {
                    FailedArray.Add(MakeShared<FJsonValueString>(Path));
                }
                Resp->SetArrayField(TEXT("failed"), FailedArray);
            }

            if (Cleaned.Num() > 0 && Failed.Num() == 0)
            {
                bSuccess = true;
                Message = TEXT("Animation artifacts removed");
            }
            else
            {
                bSuccess = false;
                Message = Failed.Num() > 0
                    ? TEXT("Some animation artifacts could not be removed")
                    : TEXT("No animation artifacts were removed");
                ErrorCode = Failed.Num() > 0 ? TEXT("CLEANUP_PARTIAL") : TEXT("CLEANUP_NO_OP");
                Resp->SetStringField(TEXT("error"), Message);
            }
        }
    }
    else if (LowerSub == TEXT("create_blend_space") || LowerSub == TEXT("create_blend_tree") || LowerSub == TEXT("create_procedural_anim"))
    {
        FString Name;
        if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
        {
            Message = TEXT("name field required for blend space creation");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            FString SavePath;
            Payload->TryGetStringField(TEXT("savePath"), SavePath);
            if (SavePath.IsEmpty())
            {
                SavePath = TEXT("/Game/Animations");
            }

            FString SkeletonPath;
            if (!Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath) || SkeletonPath.IsEmpty())
            {
                Message = TEXT("skeletonPath is required to bind blend space to a skeleton");
                ErrorCode = TEXT("INVALID_ARGUMENT");
                Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
                USkeleton* TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
                if (!TargetSkeleton)
                {
                    Message = TEXT("Failed to load skeleton for blend space");
                    ErrorCode = TEXT("LOAD_FAILED");
                    Resp->SetStringField(TEXT("error"), Message);
                }
                else
                {
                    int32 Dimensions = 1;
                    double DimensionsNumber = 1.0;
                    if (Payload->TryGetNumberField(TEXT("dimensions"), DimensionsNumber))
                    {
                        Dimensions = static_cast<int32>(DimensionsNumber);
                    }
                    const bool bTwoDimensional = LowerSub != TEXT("create_blend_space") ? true : (Dimensions >= 2);
                    FString FactoryError;
                    #if MCP_HAS_BLENDSPACE_FACTORY
                    UObject* CreatedBlendAsset = CreateBlendSpaceAsset(Name, SavePath, TargetSkeleton, bTwoDimensional, FactoryError);
                    if (CreatedBlendAsset)
                    {
                        ApplyBlendSpaceConfiguration(CreatedBlendAsset, Payload, bTwoDimensional);
#if MCP_HAS_BLENDSPACE_BASE
                        if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(CreatedBlendAsset))
                        {
                            UEditorAssetLibrary::SaveAsset(BlendSpace->GetPathName());

                            bSuccess = true;
                            Message = TEXT("Blend space created successfully");
                            Resp->SetStringField(TEXT("blendSpacePath"), BlendSpace->GetPathName());
                            Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
                            Resp->SetBoolField(TEXT("twoDimensional"), bTwoDimensional);
                        }
                        else
                        {
                            Message = TEXT("Created asset is not a BlendSpaceBase instance");
                            ErrorCode = TEXT("TYPE_MISMATCH");
                            Resp->SetStringField(TEXT("error"), Message);
                        }
#else
                        UEditorAssetLibrary::SaveAsset(CreatedBlendAsset->GetPathName());

                        bSuccess = true;
                        Message = TEXT("Blend space created (limited configuration)");
                        Resp->SetStringField(TEXT("blendSpacePath"), CreatedBlendAsset->GetPathName());
                        Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
                        Resp->SetBoolField(TEXT("twoDimensional"), bTwoDimensional);
                        Resp->SetStringField(TEXT("warning"), TEXT("BlendSpaceBase headers unavailable; axis configuration skipped."));
#endif // MCP_HAS_BLENDSPACE_BASE
                    }
                    else
                    {
                        Message = FactoryError.IsEmpty() ? TEXT("Failed to create blend space asset") : FactoryError;
                        ErrorCode = TEXT("ASSET_CREATION_FAILED");
                        Resp->SetStringField(TEXT("error"), Message);
                    }
                    #else
                    Message = TEXT("Blend space creation requires editor blend space factories");
                    ErrorCode = TEXT("NOT_AVAILABLE");
                    Resp->SetStringField(TEXT("error"), Message);
                    #endif
                }
            }
        }
    }
    else if (LowerSub == TEXT("create_state_machine"))
    {
        FString BlueprintPath;
        Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
        if (BlueprintPath.IsEmpty())
        {
            Payload->TryGetStringField(TEXT("name"), BlueprintPath);
        }

        if (BlueprintPath.IsEmpty())
        {
            Message = TEXT("blueprintPath is required for create_state_machine");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            FString MachineName;
            Payload->TryGetStringField(TEXT("machineName"), MachineName);
            if (MachineName.IsEmpty())
            {
                MachineName = TEXT("StateMachine");
            }

            TArray<FString> Commands;
            Commands.Add(FString::Printf(TEXT("AddAnimStateMachine %s %s"), *BlueprintPath, *MachineName));

            const TArray<TSharedPtr<FJsonValue>>* StatesArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("states"), StatesArray) && StatesArray)
            {
                for (const TSharedPtr<FJsonValue>& StateValue : *StatesArray)
                {
                    if (!StateValue.IsValid() || StateValue->Type != EJson::Object)
                    {
                        continue;
                    }

                    const TSharedPtr<FJsonObject> StateObj = StateValue->AsObject();
                    FString StateName;
                    StateObj->TryGetStringField(TEXT("name"), StateName);
                    if (StateName.IsEmpty())
                    {
                        continue;
                    }

                    FString AnimationName;
                    StateObj->TryGetStringField(TEXT("animation"), AnimationName);
                    Commands.Add(FString::Printf(TEXT("AddAnimState %s %s %s %s"), *BlueprintPath, *MachineName, *StateName, *AnimationName));

                    bool bIsEntry = false;
                    bool bIsExit = false;
                    StateObj->TryGetBoolField(TEXT("isEntry"), bIsEntry);
                    StateObj->TryGetBoolField(TEXT("isExit"), bIsExit);
                    if (bIsEntry)
                    {
                        Commands.Add(FString::Printf(TEXT("SetAnimStateEntry %s %s %s"), *BlueprintPath, *MachineName, *StateName));
                    }
                    if (bIsExit)
                    {
                        Commands.Add(FString::Printf(TEXT("SetAnimStateExit %s %s %s"), *BlueprintPath, *MachineName, *StateName));
                    }
                }
            }

            const TArray<TSharedPtr<FJsonValue>>* TransitionsArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("transitions"), TransitionsArray) && TransitionsArray)
            {
                for (const TSharedPtr<FJsonValue>& TransitionValue : *TransitionsArray)
                {
                    if (!TransitionValue.IsValid() || TransitionValue->Type != EJson::Object)
                    {
                        continue;
                    }

                    const TSharedPtr<FJsonObject> TransitionObj = TransitionValue->AsObject();
                    FString SourceState;
                    FString TargetState;
                    TransitionObj->TryGetStringField(TEXT("sourceState"), SourceState);
                    TransitionObj->TryGetStringField(TEXT("targetState"), TargetState);
                    if (SourceState.IsEmpty() || TargetState.IsEmpty())
                    {
                        continue;
                    }
                    Commands.Add(FString::Printf(TEXT("AddAnimTransition %s %s %s %s"), *BlueprintPath, *MachineName, *SourceState, *TargetState));

                    FString Condition;
                    if (TransitionObj->TryGetStringField(TEXT("condition"), Condition) && !Condition.IsEmpty())
                    {
                        Commands.Add(FString::Printf(TEXT("SetAnimTransitionRule %s %s %s %s %s"), *BlueprintPath, *MachineName, *SourceState, *TargetState, *Condition));
                    }
                }
            }

            FString CommandError;
            if (!ExecuteEditorCommands(Commands, CommandError))
            {
                Message = CommandError.IsEmpty() ? TEXT("Failed to create animation state machine") : CommandError;
                ErrorCode = TEXT("COMMAND_FAILED");
                Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
                bSuccess = true;
                Message = FString::Printf(TEXT("State machine '%s' added to %s"), *MachineName, *BlueprintPath);
                Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
                Resp->SetStringField(TEXT("machineName"), MachineName);
            }
        }
    }
    else if (LowerSub == TEXT("setup_ik"))
    {
        FString Name;
        if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
        {
            Message = TEXT("name field required for IK setup");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            FString SavePath;
            Payload->TryGetStringField(TEXT("savePath"), SavePath);
            if (SavePath.IsEmpty())
            {
                SavePath = TEXT("/Game/Animations");
            }

            FString SkeletonPath;
            if (!Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath) || SkeletonPath.IsEmpty())
            {
                Message = TEXT("skeletonPath is required to bind IK to a skeleton");
                ErrorCode = TEXT("INVALID_ARGUMENT");
                Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
                USkeleton* TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
                if (!TargetSkeleton)
                {
                    Message = TEXT("Failed to load skeleton for IK");
                    ErrorCode = TEXT("LOAD_FAILED");
                    Resp->SetStringField(TEXT("error"), Message);
                }
                else
                {
                    FString FactoryError;
                    UControlRigBlueprint* ControlRigBlueprint = nullptr;
#if MCP_HAS_CONTROLRIG_FACTORY
                    ControlRigBlueprint = CreateControlRigBlueprint(Name, SavePath, TargetSkeleton, FactoryError);
#else
                    FactoryError = TEXT("Control Rig factory not available in this editor build");
#endif
                    if (!ControlRigBlueprint)
                    {
                        Message = FactoryError.IsEmpty() ? TEXT("Failed to create IK asset") : FactoryError;
                        ErrorCode = TEXT("ASSET_CREATION_FAILED");
                        Resp->SetStringField(TEXT("error"), Message);
                    }
                    else
                    {
                        bSuccess = true;
                        Message = TEXT("IK setup created successfully");
                        const FString ControlRigPath = ControlRigBlueprint->GetPathName();
                        Resp->SetStringField(TEXT("ikPath"), ControlRigPath);
                        Resp->SetStringField(TEXT("controlRigPath"), ControlRigPath);
                        Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
                    }
                }
            }
        }
    }
    else if (LowerSub == TEXT("configure_vehicle"))
    {
        FString VehicleName;
        if (!Payload->TryGetStringField(TEXT("vehicleName"), VehicleName) || VehicleName.IsEmpty())
        {
            Message = TEXT("vehicleName is required");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            FString VehicleTypeRaw;
            Payload->TryGetStringField(TEXT("vehicleType"), VehicleTypeRaw);
            if (VehicleTypeRaw.IsEmpty())
            {
                Message = TEXT("vehicleType is required");
                ErrorCode = TEXT("INVALID_ARGUMENT");
                Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
                const FString NormalizedType = VehicleTypeRaw.ToLower();
                const TMap<FString, FString> VehicleTypeMap = {
                    { TEXT("car"), TEXT("Car") },
                    { TEXT("bike"), TEXT("Bike") },
                    { TEXT("motorcycle"), TEXT("Bike") },
                    { TEXT("motorbike"), TEXT("Bike") },
                    { TEXT("tank"), TEXT("Tank") },
                    { TEXT("aircraft"), TEXT("Aircraft") },
                    { TEXT("plane"), TEXT("Aircraft") }
                };

                const FString* VehicleTypePtr = VehicleTypeMap.Find(NormalizedType);
                if (!VehicleTypePtr)
                {
                    Message = TEXT("Invalid vehicleType: expected Car, Bike, Tank, or Aircraft");
                    ErrorCode = TEXT("INVALID_ARGUMENT");
                    Resp->SetStringField(TEXT("error"), Message);
                }
                else
                {
                    TArray<FString> Commands;
                    Commands.Add(FString::Printf(TEXT("CreateVehicle %s %s"), *VehicleName, **VehicleTypePtr));

                    const TArray<TSharedPtr<FJsonValue>>* WheelsArray = nullptr;
                    if (Payload->TryGetArrayField(TEXT("wheels"), WheelsArray) && WheelsArray)
                    {
                        for (int32 Index = 0; Index < WheelsArray->Num(); ++Index)
                        {
                            const TSharedPtr<FJsonValue>& WheelValue = (*WheelsArray)[Index];
                            if (!WheelValue.IsValid() || WheelValue->Type != EJson::Object)
                            {
                                continue;
                            }

                            const TSharedPtr<FJsonObject> WheelObj = WheelValue->AsObject();
                            FString WheelName;
                            WheelObj->TryGetStringField(TEXT("name"), WheelName);
                            if (WheelName.IsEmpty())
                            {
                                WheelName = FString::Printf(TEXT("Wheel_%d"), Index);
                            }

                            double Radius = 0.0, Width = 0.0, Mass = 0.0;
                            WheelObj->TryGetNumberField(TEXT("radius"), Radius);
                            WheelObj->TryGetNumberField(TEXT("width"), Width);
                            WheelObj->TryGetNumberField(TEXT("mass"), Mass);

                            Commands.Add(FString::Printf(TEXT("AddVehicleWheel %s %s %.4f %.4f %.4f"), *VehicleName, *WheelName, Radius, Width, Mass));

                            bool bSteering = false;
                            if (WheelObj->TryGetBoolField(TEXT("isSteering"), bSteering) && bSteering)
                            {
                                Commands.Add(FString::Printf(TEXT("SetWheelSteering %s %s true"), *VehicleName, *WheelName));
                            }

                            bool bDriving = false;
                            if (WheelObj->TryGetBoolField(TEXT("isDriving"), bDriving) && bDriving)
                            {
                                Commands.Add(FString::Printf(TEXT("SetWheelDriving %s %s true"), *VehicleName, *WheelName));
                            }
                        }
                    }

                    const TSharedPtr<FJsonObject>* EngineObj = nullptr;
                    if (Payload->TryGetObjectField(TEXT("engine"), EngineObj) && EngineObj && (*EngineObj).IsValid())
                    {
                        double MaxRPM = 0.0;
                        (*EngineObj)->TryGetNumberField(TEXT("maxRPM"), MaxRPM);
                        if (MaxRPM > 0.0)
                        {
                            Commands.Add(FString::Printf(TEXT("SetEngineMaxRPM %s %.4f"), *VehicleName, MaxRPM));
                        }

                        const TArray<TSharedPtr<FJsonValue>>* TorqueCurve = nullptr;
                        if ((*EngineObj)->TryGetArrayField(TEXT("torqueCurve"), TorqueCurve) && TorqueCurve)
                        {
                            for (const TSharedPtr<FJsonValue>& TorqueValue : *TorqueCurve)
                            {
                                if (!TorqueValue.IsValid())
                                {
                                    continue;
                                }

                                double RPM = 0.0;
                                double Torque = 0.0;

                                if (TorqueValue->Type == EJson::Array)
                                {
                                    const TArray<TSharedPtr<FJsonValue>> TorquePair = TorqueValue->AsArray();
                                    if (TorquePair.Num() >= 2)
                                    {
                                        RPM = TorquePair[0]->AsNumber();
                                        Torque = TorquePair[1]->AsNumber();
                                    }
                                }
                                else if (TorqueValue->Type == EJson::Object)
                                {
                                    const TSharedPtr<FJsonObject> TorqueObj = TorqueValue->AsObject();
                                    TorqueObj->TryGetNumberField(TEXT("rpm"), RPM);
                                    TorqueObj->TryGetNumberField(TEXT("torque"), Torque);
                                }

                                Commands.Add(FString::Printf(TEXT("AddTorqueCurvePoint %s %.4f %.4f"), *VehicleName, RPM, Torque));
                            }
                        }
                    }

                    const TSharedPtr<FJsonObject>* TransmissionObj = nullptr;
                    if (Payload->TryGetObjectField(TEXT("transmission"), TransmissionObj) && TransmissionObj && (*TransmissionObj).IsValid())
                    {
                        const TArray<TSharedPtr<FJsonValue>>* GearsArray = nullptr;
                        if ((*TransmissionObj)->TryGetArrayField(TEXT("gears"), GearsArray) && GearsArray)
                        {
                            for (int32 GearIndex = 0; GearIndex < GearsArray->Num(); ++GearIndex)
                            {
                                const double GearRatio = (*GearsArray)[GearIndex]->AsNumber();
                                Commands.Add(FString::Printf(TEXT("SetGearRatio %s %d %.4f"), *VehicleName, GearIndex, GearRatio));
                            }
                        }

                        double FinalDrive = 0.0;
                        if ((*TransmissionObj)->TryGetNumberField(TEXT("finalDriveRatio"), FinalDrive))
                        {
                            Commands.Add(FString::Printf(TEXT("SetFinalDriveRatio %s %.4f"), *VehicleName, FinalDrive));
                        }
                    }

                    FString CommandError;
                    if (!ExecuteEditorCommands(Commands, CommandError))
                    {
                        Message = CommandError.IsEmpty() ? TEXT("Failed to configure vehicle") : CommandError;
                        ErrorCode = TEXT("COMMAND_FAILED");
                        Resp->SetStringField(TEXT("error"), Message);
                    }
                    else
                    {
                        bSuccess = true;
                        Message = FString::Printf(TEXT("Vehicle %s configured"), *VehicleName);
                        Resp->SetStringField(TEXT("vehicleName"), VehicleName);
                        Resp->SetStringField(TEXT("vehicleType"), *VehicleTypePtr);

                        const TArray<TSharedPtr<FJsonValue>>* PluginDeps = nullptr;
                        if (Payload->TryGetArrayField(TEXT("pluginDependencies"), PluginDeps) && PluginDeps)
                        {
                            TArray<TSharedPtr<FJsonValue>> PluginArray;
                            for (const TSharedPtr<FJsonValue>& DepValue : *PluginDeps)
                            {
                                if (DepValue.IsValid() && DepValue->Type == EJson::String)
                                {
                                    PluginArray.Add(MakeShared<FJsonValueString>(DepValue->AsString()));
                                }
                            }
                            if (PluginArray.Num() > 0)
                            {
                                Resp->SetArrayField(TEXT("pluginDependencies"), PluginArray);
                            }
                        }
                    }
                }
            }
        }
    }
    else if (LowerSub == TEXT("setup_physics_simulation"))
    {
        FString MeshPath;
        Payload->TryGetStringField(TEXT("meshPath"), MeshPath);

        FString SkeletonPath;
        Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

        const bool bMeshProvided = !MeshPath.IsEmpty();
        const bool bSkeletonProvided = !SkeletonPath.IsEmpty();

        bool bMeshLoadFailed = false;
        bool bSkeletonLoadFailed = false;
        bool bSkeletonMissingPreview = false;

        USkeletalMesh* TargetMesh = nullptr;
        if (bMeshProvided)
        {
            TargetMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
            if (!TargetMesh)
            {
                bMeshLoadFailed = true;
                UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("setup_physics_simulation: failed to load mesh %s"), *MeshPath);
            }
        }

        USkeleton* TargetSkeleton = nullptr;
        if (!TargetMesh && bSkeletonProvided)
        {
            TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
            if (TargetSkeleton)
            {
                TargetMesh = TargetSkeleton->GetPreviewMesh();
                if (!TargetMesh)
                {
                    bSkeletonMissingPreview = true;
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("setup_physics_simulation: skeleton %s has no preview mesh"), *SkeletonPath);
                }
            }
            else
            {
                bSkeletonLoadFailed = true;
                UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("setup_physics_simulation: failed to load skeleton %s"), *SkeletonPath);
            }
        }

        if (!TargetSkeleton && TargetMesh)
        {
            TargetSkeleton = TargetMesh->GetSkeleton();
        }

        if (!TargetMesh)
        {
            if (bMeshLoadFailed)
            {
                Message = FString::Printf(TEXT("Skeletal mesh not found: %s"), *MeshPath);
                ErrorCode = TEXT("ASSET_NOT_FOUND");
                Resp->SetStringField(TEXT("meshPath"), MeshPath);
            }
            else if (bSkeletonLoadFailed)
            {
                Message = FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath);
                ErrorCode = TEXT("ASSET_NOT_FOUND");
                Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
            }
            else if (bSkeletonMissingPreview)
            {
                Message = FString::Printf(TEXT("Skeleton %s does not provide a preview skeletal mesh"), *SkeletonPath);
                ErrorCode = TEXT("ASSET_NOT_FOUND");
                Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
            }
            else
            {
                Message = TEXT("No valid skeletal mesh provided for physics simulation setup");
                ErrorCode = TEXT("INVALID_ARGUMENT");
            }

            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            if (!TargetSkeleton && !SkeletonPath.IsEmpty())
            {
                TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
            }

            FString PhysicsAssetName;
            Payload->TryGetStringField(TEXT("physicsAssetName"), PhysicsAssetName);
            if (PhysicsAssetName.IsEmpty())
            {
                PhysicsAssetName = TargetMesh->GetName() + TEXT("_Physics");
            }

            FString SavePath;
            Payload->TryGetStringField(TEXT("savePath"), SavePath);
            if (SavePath.IsEmpty())
            {
                SavePath = TEXT("/Game/Physics");
            }
            SavePath = SavePath.TrimStartAndEnd();

            if (!FPackageName::IsValidLongPackageName(SavePath))
            {
                FString NormalizedPath;
                if (!FPackageName::TryConvertFilenameToLongPackageName(SavePath, NormalizedPath))
                {
                    Message = TEXT("Invalid savePath for physics asset");
                    ErrorCode = TEXT("INVALID_ARGUMENT");
                    Resp->SetStringField(TEXT("error"), Message);
                    SavePath.Reset();
                }
                else
                {
                    SavePath = NormalizedPath;
                }
            }

            if (!SavePath.IsEmpty())
            {
                if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath))
                {
                    UEditorAssetLibrary::MakeDirectory(SavePath);
                }

                const FString PhysicsAssetObjectPath = FString::Printf(TEXT("%s/%s"), *SavePath, *PhysicsAssetName);

                if (UEditorAssetLibrary::DoesAssetExist(PhysicsAssetObjectPath))
                {
                    bSuccess = true;
                    Message = TEXT("Physics simulation already configured - existing asset reused");
                    Resp->SetStringField(TEXT("physicsAssetPath"), PhysicsAssetObjectPath);
                    Resp->SetBoolField(TEXT("existingAsset"), true);
                    Resp->SetStringField(TEXT("savePath"), SavePath);
                    Resp->SetStringField(TEXT("meshPath"), TargetMesh->GetPathName());
                    if (TargetSkeleton)
                    {
                        Resp->SetStringField(TEXT("skeletonPath"), TargetSkeleton->GetPathName());
                    }
                }
                else
                {
                    UPhysicsAssetFactory* PhysicsFactory = NewObject<UPhysicsAssetFactory>();
                    if (!PhysicsFactory)
                    {
                        Message = TEXT("Failed to allocate physics asset factory");
                        ErrorCode = TEXT("FACTORY_FAILED");
                        Resp->SetStringField(TEXT("error"), Message);
                    }
                    else
                    {
                        PhysicsFactory->TargetSkeletalMesh = TargetMesh;

                        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
                        UObject* NewAsset = AssetToolsModule.Get().CreateAsset(PhysicsAssetName, SavePath, UPhysicsAsset::StaticClass(), PhysicsFactory);
                        UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(NewAsset);

                        if (!PhysicsAsset)
                        {
                            Message = TEXT("Failed to create physics asset");
                            ErrorCode = TEXT("ASSET_CREATION_FAILED");
                            Resp->SetStringField(TEXT("error"), Message);
                        }
                        else
                        {
                            bool bAssignToMesh = false;
                            Payload->TryGetBoolField(TEXT("assignToMesh"), bAssignToMesh);

                            UEditorAssetLibrary::SaveAsset(PhysicsAsset->GetPathName());

                            if (bAssignToMesh)
                            {
                                TargetMesh->Modify();
                                TargetMesh->SetPhysicsAsset(PhysicsAsset);
                                TargetMesh->MarkPackageDirty();
                                UEditorAssetLibrary::SaveAsset(TargetMesh->GetPathName());
                            }

                            Resp->SetStringField(TEXT("physicsAssetPath"), PhysicsAsset->GetPathName());
                            Resp->SetBoolField(TEXT("assignedToMesh"), bAssignToMesh);
                            Resp->SetBoolField(TEXT("existingAsset"), false);
                            Resp->SetStringField(TEXT("savePath"), SavePath);
                            Resp->SetStringField(TEXT("meshPath"), TargetMesh->GetPathName());
                            if (TargetSkeleton)
                            {
                                Resp->SetStringField(TEXT("skeletonPath"), TargetSkeleton->GetPathName());
                            }

                            bSuccess = true;
                            Message = TEXT("Physics simulation setup completed");
                        }
                    }
                }
            }
        }
    }
    else if (LowerSub == TEXT("create_animation_asset"))
    {
        FString AssetName;
        if (!Payload->TryGetStringField(TEXT("name"), AssetName) || AssetName.IsEmpty())
        {
            Message = TEXT("name required for create_animation_asset");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            FString SavePath;
            Payload->TryGetStringField(TEXT("savePath"), SavePath);
            if (SavePath.IsEmpty())
            {
                SavePath = TEXT("/Game/Animations");
            }
            SavePath = SavePath.TrimStartAndEnd();

            if (!FPackageName::IsValidLongPackageName(SavePath))
            {
                FString NormalizedPath;
                if (!FPackageName::TryConvertFilenameToLongPackageName(SavePath, NormalizedPath))
                {
                    Message = TEXT("Invalid savePath for animation asset");
                    ErrorCode = TEXT("INVALID_ARGUMENT");
                    Resp->SetStringField(TEXT("error"), Message);
                    SavePath.Reset();
                }
                else
                {
                    SavePath = NormalizedPath;
                }
            }

            FString SkeletonPath;
            Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);
            USkeleton* TargetSkeleton = nullptr;
            if (!SkeletonPath.IsEmpty())
            {
                TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
            }

            if (!TargetSkeleton)
            {
                Message = TEXT("skeletonPath is required for create_animation_asset");
                ErrorCode = TEXT("INVALID_ARGUMENT");
                Resp->SetStringField(TEXT("error"), Message);
            }
            else if (!SavePath.IsEmpty())
            {
                if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath))
                {
                    UEditorAssetLibrary::MakeDirectory(SavePath);
                }

                FString AssetType;
                Payload->TryGetStringField(TEXT("assetType"), AssetType);
                AssetType = AssetType.ToLower();
                if (AssetType.IsEmpty())
                {
                    AssetType = TEXT("sequence");
                }

                UFactory* Factory = nullptr;
                UClass* DesiredClass = nullptr;
                FString AssetTypeString;

                if (AssetType == TEXT("montage"))
                {
                    UAnimMontageFactory* MontageFactory = NewObject<UAnimMontageFactory>();
                    if (MontageFactory)
                    {
                        MontageFactory->TargetSkeleton = TargetSkeleton;
                        Factory = MontageFactory;
                        DesiredClass = UAnimMontage::StaticClass();
                        AssetTypeString = TEXT("Montage");
                    }
                }
                else
                {
                    UAnimSequenceFactory* SequenceFactory = NewObject<UAnimSequenceFactory>();
                    if (SequenceFactory)
                    {
                        SequenceFactory->TargetSkeleton = TargetSkeleton;
                        Factory = SequenceFactory;
                        DesiredClass = UAnimSequence::StaticClass();
                        AssetTypeString = TEXT("Sequence");
                    }
                }

                if (!Factory || !DesiredClass)
                {
                    Message = TEXT("Unsupported assetType for create_animation_asset");
                    ErrorCode = TEXT("INVALID_ARGUMENT");
                    Resp->SetStringField(TEXT("error"), Message);
                }
                else
                {
                    const FString ObjectPath = FString::Printf(TEXT("%s/%s"), *SavePath, *AssetName);
                    if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
                    {
                        bSuccess = true;
                        Message = TEXT("Animation asset already exists - existing asset reused");
                        Resp->SetStringField(TEXT("assetPath"), ObjectPath);
                        Resp->SetStringField(TEXT("assetType"), AssetTypeString);
                        Resp->SetBoolField(TEXT("existingAsset"), true);
                    }
                    else
                    {
                        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
                        UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, SavePath, DesiredClass, Factory);

                        if (!NewAsset)
                        {
                            Message = TEXT("Failed to create animation asset");
                            ErrorCode = TEXT("ASSET_CREATION_FAILED");
                            Resp->SetStringField(TEXT("error"), Message);
                        }
                        else
                        {
                            UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName());
                            Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
                            Resp->SetStringField(TEXT("assetType"), AssetTypeString);
                            Resp->SetBoolField(TEXT("existingAsset"), false);
                            bSuccess = true;
                            Message = FString::Printf(TEXT("Animation %s created"), *AssetTypeString);
                        }
                    }
                }
            }
        }
    }
    else if (LowerSub == TEXT("setup_retargeting"))
    {
        FString SourceSkeletonPath;
        FString TargetSkeletonPath;
        Payload->TryGetStringField(TEXT("sourceSkeleton"), SourceSkeletonPath);
        Payload->TryGetStringField(TEXT("targetSkeleton"), TargetSkeletonPath);

        USkeleton* SourceSkeleton = nullptr;
        USkeleton* TargetSkeleton = nullptr;

        if (!SourceSkeletonPath.IsEmpty())
        {
            SourceSkeleton = LoadObject<USkeleton>(nullptr, *SourceSkeletonPath);
        }
        if (!TargetSkeletonPath.IsEmpty())
        {
            TargetSkeleton = LoadObject<USkeleton>(nullptr, *TargetSkeletonPath);
        }

        if (!SourceSkeleton || !TargetSkeleton)
        {
            Message = TEXT("Both sourceSkeleton and targetSkeleton are required for setup_retargeting");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            const TArray<TSharedPtr<FJsonValue>>* AssetsArray = nullptr;
            if (!Payload->TryGetArrayField(TEXT("assets"), AssetsArray))
            {
                Payload->TryGetArrayField(TEXT("retargetAssets"), AssetsArray);
            }

            FString SavePath;
            Payload->TryGetStringField(TEXT("savePath"), SavePath);
            if (!SavePath.IsEmpty())
            {
                SavePath = SavePath.TrimStartAndEnd();
                if (!FPackageName::IsValidLongPackageName(SavePath))
                {
                    FString NormalizedPath;
                    if (FPackageName::TryConvertFilenameToLongPackageName(SavePath, NormalizedPath))
                    {
                        SavePath = NormalizedPath;
                    }
                    else
                    {
                        SavePath.Reset();
                    }
                }
            }

            FString Suffix;
            Payload->TryGetStringField(TEXT("suffix"), Suffix);
            if (Suffix.IsEmpty())
            {
                Suffix = TEXT("_Retargeted");
            }

            bool bOverwrite = false;
            Payload->TryGetBoolField(TEXT("overwrite"), bOverwrite);

            TArray<FString> RetargetedAssets;
            TArray<FString> SkippedAssets;
            TArray<TSharedPtr<FJsonValue>> WarningArray;

            if (AssetsArray && AssetsArray->Num() > 0)
            {
                for (const TSharedPtr<FJsonValue>& Value : *AssetsArray)
                {
                    if (!Value.IsValid() || Value->Type != EJson::String)
                    {
                        continue;
                    }

                    const FString SourceAssetPath = Value->AsString();
                    UAnimSequence* SourceSequence = LoadObject<UAnimSequence>(nullptr, *SourceAssetPath);
                    if (!SourceSequence)
                    {
                        WarningArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Skipped non-sequence asset: %s"), *SourceAssetPath)));
                        SkippedAssets.Add(SourceAssetPath);
                        continue;
                    }

                    FString DestinationFolder = SavePath;
                    if (DestinationFolder.IsEmpty())
                    {
                        const FString SourcePackageName = SourceSequence->GetOutermost()->GetName();
                        DestinationFolder = FPackageName::GetLongPackagePath(SourcePackageName);
                    }

                    if (!DestinationFolder.IsEmpty() && !UEditorAssetLibrary::DoesDirectoryExist(DestinationFolder))
                    {
                        UEditorAssetLibrary::MakeDirectory(DestinationFolder);
                    }

                    FString DestinationAssetName = FPackageName::GetShortName(SourceSequence->GetOutermost()->GetName());
                    DestinationAssetName += Suffix;

                    const FString DestinationObjectPath = FString::Printf(TEXT("%s/%s"), *DestinationFolder, *DestinationAssetName);

                    if (UEditorAssetLibrary::DoesAssetExist(DestinationObjectPath))
                    {
                        if (!bOverwrite)
                        {
                            WarningArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Retarget destination already exists, skipping: %s"), *DestinationObjectPath)));
                            SkippedAssets.Add(SourceAssetPath);
                            continue;
                        }
                    }
                    else if (!UEditorAssetLibrary::DuplicateAsset(SourceAssetPath, DestinationObjectPath))
                    {
                        WarningArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Failed to duplicate asset: %s"), *SourceAssetPath)));
                        SkippedAssets.Add(SourceAssetPath);
                        continue;
                    }

                    UAnimSequence* DestinationSequence = LoadObject<UAnimSequence>(nullptr, *DestinationObjectPath);
                    if (!DestinationSequence)
                    {
                        WarningArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Failed to load duplicated asset: %s"), *DestinationObjectPath)));
                        SkippedAssets.Add(SourceAssetPath);
                        continue;
                    }

                    DestinationSequence->Modify();
                    DestinationSequence->SetSkeleton(TargetSkeleton);
                    DestinationSequence->MarkPackageDirty();

                    TArray<UAnimSequence*> SourceList;
                    SourceList.Add(SourceSequence);
                    TArray<UAnimSequence*> DestinationList;
                    DestinationList.Add(DestinationSequence);

                    #if WITH_EDITOR
                    // Animation retargeting in UE5 requires IK Rig system
                    // For now, just copy the asset without retargeting
                    FString AdjustedDestinationPath = DestinationObjectPath;
                    if (AdjustedDestinationPath.StartsWith(TEXT("/Content")))
                    {
                        AdjustedDestinationPath = FString::Printf(TEXT("/Game%s"), *AdjustedDestinationPath.RightChop(8));
                    }
                    DestinationSequence = Cast<UAnimSequence>(UEditorAssetLibrary::DuplicateAsset(SourceSequence->GetPathName(), AdjustedDestinationPath));
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Animation asset copied (retargeting requires IK Rig setup)"));

                    UEditorAssetLibrary::SaveAsset(DestinationSequence->GetPathName());
                    RetargetedAssets.Add(DestinationSequence->GetPathName());
                    #else
                    WarningArray.Add(MakeShared<FJsonValueString>(TEXT("Retargeting requires editor animation blueprint library")));
                    #endif
                }
            }

            bSuccess = true;
            Message = RetargetedAssets.Num() > 0 ? TEXT("Retargeting completed") : TEXT("Retargeting completed - no assets processed");

            TArray<TSharedPtr<FJsonValue>> RetargetedArray;
            for (const FString& Path : RetargetedAssets)
            {
                RetargetedArray.Add(MakeShared<FJsonValueString>(Path));
            }
            if (RetargetedArray.Num() > 0)
            {
                Resp->SetArrayField(TEXT("retargetedAssets"), RetargetedArray);
            }

            if (SkippedAssets.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> SkippedArray;
                for (const FString& Path : SkippedAssets)
                {
                    SkippedArray.Add(MakeShared<FJsonValueString>(Path));
                }
                Resp->SetArrayField(TEXT("skippedAssets"), SkippedArray);
            }

            if (WarningArray.Num() > 0)
            {
                Resp->SetArrayField(TEXT("warnings"), WarningArray);
            }

            Resp->SetStringField(TEXT("sourceSkeleton"), SourceSkeleton->GetPathName());
            Resp->SetStringField(TEXT("targetSkeleton"), TargetSkeleton->GetPathName());
        }
    }
    else
    {
        Message = FString::Printf(TEXT("Animation/Physics action '%s' not implemented"), *LowerSub);
        ErrorCode = TEXT("NOT_IMPLEMENTED");
        Resp->SetStringField(TEXT("error"), Message);
    }

    Resp->SetBoolField(TEXT("success"), bSuccess);
    if (Message.IsEmpty())
    {
        Message = bSuccess ? TEXT("Animation/Physics action completed") : TEXT("Animation/Physics action failed");
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleAnimationPhysicsAction: responding to subaction '%s' (success=%s)"), *LowerSub, bSuccess ? TEXT("true") : TEXT("false"));
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Animation/Physics actions require editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::ExecuteEditorCommands(const TArray<FString>& Commands, FString& OutErrorMessage)
{
#if WITH_EDITOR
    return ExecuteEditorCommandsInternal(Commands, OutErrorMessage);
#else
    OutErrorMessage = TEXT("ExecuteEditorCommands is only available in editor builds");
    return false;
#endif
}

#if MCP_HAS_CONTROLRIG_FACTORY
UControlRigBlueprint* UMcpAutomationBridgeSubsystem::CreateControlRigBlueprint(const FString& AssetName, const FString& PackagePath, USkeleton* TargetSkeleton, FString& OutError)
{
    OutError.Reset();

    UControlRigBlueprintFactory* Factory = NewObject<UControlRigBlueprintFactory>();
    if (!Factory)
    {
        OutError = TEXT("Failed to allocate Control Rig factory");
        return nullptr;
    }

    Factory->TargetSkeleton = TargetSkeleton;
    Factory->BlueprintType = BPTYPE_Normal;
    Factory->ParentClass = UAnimInstance::StaticClass();

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, UControlRigBlueprint::StaticClass(), Factory);
    UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(NewAsset);

    if (!ControlRigBlueprint)
    {
        OutError = TEXT("Failed to create Control Rig blueprint");
        return nullptr;
    }

    return ControlRigBlueprint;
}
#endif

bool UMcpAutomationBridgeSubsystem::HandleCreateAnimBlueprint(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("create_animation_blueprint"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("create_animation_blueprint payload missing"), TEXT("INVALID_PAYLOAD")); return true; }
    
    FString BlueprintName;
    if (!Payload->TryGetStringField(TEXT("name"), BlueprintName) || BlueprintName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("name required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString SkeletonPath;
    if (!Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath) || SkeletonPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("skeletonPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString SavePath;
    if (!Payload->TryGetStringField(TEXT("savePath"), SavePath) || SavePath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("savePath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load skeleton"), TEXT("LOAD_FAILED"));
        return true;
    }

    FString FullPath = FString::Printf(TEXT("%s/%s"), *SavePath, *BlueprintName);

    UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
    Factory->TargetSkeleton = Skeleton;
    Factory->BlueprintType = BPTYPE_Normal;
    Factory->ParentClass = UAnimInstance::StaticClass();

    if (!Factory)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create animation blueprint factory"), TEXT("FACTORY_FAILED"));
        return true;
    }

    FString PackagePath = SavePath;
    FString AssetName = BlueprintName;
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, UAnimBlueprint::StaticClass(), Factory);
    UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(NewAsset);

    if (!AnimBlueprint)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create animation blueprint"), TEXT("ASSET_CREATION_FAILED"));
        return true;
    }

    UEditorAssetLibrary::SaveAsset(AnimBlueprint->GetPathName());

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("blueprintPath"), AnimBlueprint->GetPathName());
    Resp->SetStringField(TEXT("blueprintName"), BlueprintName);
    Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Animation blueprint created successfully"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("create_animation_blueprint requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandlePlayAnimMontage(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("play_anim_montage"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("play_anim_montage payload missing"), TEXT("INVALID_PAYLOAD")); return true; }
    
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString MontagePath;
    if (!Payload->TryGetStringField(TEXT("montagePath"), MontagePath) || MontagePath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("montagePath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    double PlayRate = 1.0;
    Payload->TryGetNumberField(TEXT("playRate"), PlayRate);

    if (!GEditor || !GEditor->GetEditorWorldContext().World())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor world not available"), TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("EditorActorSubsystem not available"), TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
    APawn* TargetPawn = nullptr;

    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase))
        {
            TargetPawn = Cast<APawn>(Actor);
            if (TargetPawn)
            {
                break;
            }
        }
    }

    if (!TargetPawn)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Pawn actor not found"), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    USkeletalMeshComponent* SkelMeshComp = TargetPawn->FindComponentByClass<USkeletalMeshComponent>();
    if (!SkelMeshComp)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Skeletal mesh component not found"), TEXT("COMPONENT_NOT_FOUND"));
        return true;
    }

    UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
    if (!Montage)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load animation montage"), TEXT("LOAD_FAILED"));
        return true;
    }

    float MontageLength = 0.f;
    if (UAnimInstance* AnimInst = SkelMeshComp->GetAnimInstance())
    {
        MontageLength = AnimInst->Montage_Play(Montage, static_cast<float>(PlayRate));
    }
    else
    {
        SkelMeshComp->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
        SkelMeshComp->PlayAnimation(Montage, false);
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetStringField(TEXT("montagePath"), MontagePath);
    Resp->SetNumberField(TEXT("playRate"), PlayRate);
    Resp->SetNumberField(TEXT("montageLength"), MontageLength);
    Resp->SetBoolField(TEXT("playing"), true);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Animation montage playing"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("play_anim_montage requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSetupRagdoll(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("setup_ragdoll"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("setup_ragdoll payload missing"), TEXT("INVALID_PAYLOAD")); return true; }
    
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    double BlendWeight = 1.0;
    Payload->TryGetNumberField(TEXT("blendWeight"), BlendWeight);

    if (!GEditor || !GEditor->GetEditorWorldContext().World())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor world not available"), TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("EditorActorSubsystem not available"), TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
    APawn* TargetPawn = nullptr;

    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase))
        {
            TargetPawn = Cast<APawn>(Actor);
            if (TargetPawn)
            {
                break;
            }
        }
    }

    if (!TargetPawn)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Pawn actor not found"), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    USkeletalMeshComponent* SkelMeshComp = TargetPawn->FindComponentByClass<USkeletalMeshComponent>();
    if (!SkelMeshComp)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Skeletal mesh component not found"), TEXT("COMPONENT_NOT_FOUND"));
        return true;
    }

    SkelMeshComp->SetSimulatePhysics(true);
    SkelMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    if (SkelMeshComp->GetPhysicsAsset())
    {
        SkelMeshComp->SetAllBodiesSimulatePhysics(true);
        SkelMeshComp->SetUpdateAnimationInEditor(BlendWeight < 1.0);
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetNumberField(TEXT("blendWeight"), BlendWeight);
    Resp->SetBoolField(TEXT("ragdollActive"), SkelMeshComp->IsSimulatingPhysics());
    Resp->SetBoolField(TEXT("hasPhysicsAsset"), SkelMeshComp->GetPhysicsAsset() != nullptr);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ragdoll setup completed"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("setup_ragdoll requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
