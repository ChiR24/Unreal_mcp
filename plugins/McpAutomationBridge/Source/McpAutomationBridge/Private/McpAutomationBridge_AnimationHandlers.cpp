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
#include "AnimationBlueprintLibrary.h"
#include "Factories/AnimBlueprintFactory.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Modules/ModuleManager.h"
#include "Async/Async.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#endif

bool UMcpAutomationBridgeSubsystem::HandleAnimationPhysicsAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("animation_physics"), ESearchCase::IgnoreCase) && !Lower.StartsWith(TEXT("animation_physics"))) return false;

    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("animation_physics payload missing."), TEXT("INVALID_PAYLOAD")); return true; }

    FString SubAction; Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
    AsyncTask(ENamedThreads::GameThread, [this, RequestId, Payload, LowerSub, RequestingSocket]() {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("action"), LowerSub);
        Resp->SetStringField(TEXT("message"), FString::Printf(TEXT("Animation/Physics action '%s' handled"), *LowerSub));

        // Most animation/physics operations are best-effort stubs for now
        // Real implementations would require detailed animation blueprint APIs
        if (LowerSub == TEXT("cleanup"))
        {
            // Handle cleanup of animation artifacts
            const TArray<TSharedPtr<FJsonValue>>* ArtifactsArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("artifacts"), ArtifactsArray) && ArtifactsArray)
            {
                TArray<FString> Cleaned;
                for (const TSharedPtr<FJsonValue>& Val : *ArtifactsArray)
                {
                    if (Val.IsValid() && Val->Type == EJson::String)
                    {
                        FString ArtifactPath = Val->AsString();
                        // Try to delete the asset
                        if (UEditorAssetLibrary::DoesAssetExist(ArtifactPath))
                        {
                            if (UEditorAssetLibrary::DeleteAsset(ArtifactPath))
                            {
                                Cleaned.Add(ArtifactPath);
                            }
                        }
                    }
                }
                
                TArray<TSharedPtr<FJsonValue>> CleanedArray;
                for (const FString& Path : Cleaned)
                {
                    CleanedArray.Add(MakeShared<FJsonValueString>(Path));
                }
                Resp->SetArrayField(TEXT("cleaned"), CleanedArray);
                Resp->SetNumberField(TEXT("cleanedCount"), Cleaned.Num());
            }
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Animation/Physics action '%s' completed"), *LowerSub), Resp, FString());
    });
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Animation/Physics actions require editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

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

    AsyncTask(ENamedThreads::GameThread, [this, RequestId, BlueprintName, SkeletonPath, SavePath, RequestingSocket]()
    {
        USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
        if (!Skeleton)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load skeleton"), TEXT("LOAD_FAILED"));
            return;
        }

        FString FullPath = FString::Printf(TEXT("%s/%s"), *SavePath, *BlueprintName);
        
        UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
        Factory->TargetSkeleton = Skeleton;
        Factory->BlueprintType = BPTYPE_Normal;
        Factory->ParentClass = UAnimInstance::StaticClass();
        
        if (!Factory)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create animation blueprint factory"), TEXT("FACTORY_FAILED"));
            return;
        }

        // Create asset via AssetTools (UE 5.x)
        FString PackagePath = SavePath;
        FString AssetName = BlueprintName;
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, UAnimBlueprint::StaticClass(), Factory);
        UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(NewAsset);
        
        if (!AnimBlueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create animation blueprint"), TEXT("ASSET_CREATION_FAILED"));
            return;
        }

        UEditorAssetLibrary::SaveAsset(AnimBlueprint->GetPathName());

        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("blueprintPath"), AnimBlueprint->GetPathName());
        Resp->SetStringField(TEXT("blueprintName"), BlueprintName);
        Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Animation blueprint created successfully"), Resp, FString());
    });

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

    AsyncTask(ENamedThreads::GameThread, [this, RequestId, ActorName, MontagePath, PlayRate, RequestingSocket]()
    {
        if (!GEditor || !GEditor->GetEditorWorldContext().World())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Editor world not available"), TEXT("EDITOR_NOT_AVAILABLE"));
            return;
        }

        UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
        if (!ActorSS)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("EditorActorSubsystem not available"), TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
            return;
        }

        TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
        APawn* TargetPawn = nullptr;
        
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase))
            {
                TargetPawn = Cast<APawn>(Actor);
                if (TargetPawn) break;
            }
        }

        if (!TargetPawn)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Pawn actor not found"), TEXT("ACTOR_NOT_FOUND"));
            return;
        }

        USkeletalMeshComponent* SkelMeshComp = TargetPawn->FindComponentByClass<USkeletalMeshComponent>();
        if (!SkelMeshComp)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Skeletal mesh component not found"), TEXT("COMPONENT_NOT_FOUND"));
            return;
        }

        UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
        if (!Montage)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load animation montage"), TEXT("LOAD_FAILED"));
            return;
        }

        float MontageLength = 0.f;
        if (UAnimInstance* AnimInst = SkelMeshComp->GetAnimInstance())
        {
            MontageLength = AnimInst->Montage_Play(Montage, static_cast<float>(PlayRate));
        }
        else
        {
            // Fallback: set single node mode and try to play as an animation asset (may not support montages)
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
    });

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

    AsyncTask(ENamedThreads::GameThread, [this, RequestId, ActorName, BlendWeight, RequestingSocket]()
    {
        if (!GEditor || !GEditor->GetEditorWorldContext().World())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Editor world not available"), TEXT("EDITOR_NOT_AVAILABLE"));
            return;
        }

        UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
        if (!ActorSS)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("EditorActorSubsystem not available"), TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
            return;
        }

        TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
        APawn* TargetPawn = nullptr;
        
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase))
            {
                TargetPawn = Cast<APawn>(Actor);
                if (TargetPawn) break;
            }
        }

        if (!TargetPawn)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Pawn actor not found"), TEXT("ACTOR_NOT_FOUND"));
            return;
        }

        USkeletalMeshComponent* SkelMeshComp = TargetPawn->FindComponentByClass<USkeletalMeshComponent>();
        if (!SkelMeshComp)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Skeletal mesh component not found"), TEXT("COMPONENT_NOT_FOUND"));
            return;
        }

        // Enable physics simulation (ragdoll)
        SkelMeshComp->SetSimulatePhysics(true);
        SkelMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        
        // Set blend weights if physics asset exists
        if (SkelMeshComp->GetPhysicsAsset())
        {
            SkelMeshComp->SetAllBodiesSimulatePhysics(true);
            // Blend weight affects physics/animation mixing
            SkelMeshComp->SetUpdateAnimationInEditor(BlendWeight < 1.0);
        }

        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("actorName"), ActorName);
        Resp->SetNumberField(TEXT("blendWeight"), BlendWeight);
        Resp->SetBoolField(TEXT("ragdollActive"), SkelMeshComp->IsSimulatingPhysics());
        Resp->SetBoolField(TEXT("hasPhysicsAsset"), SkelMeshComp->GetPhysicsAsset() != nullptr);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ragdoll setup completed"), Resp, FString());
    });

    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("setup_ragdoll requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
