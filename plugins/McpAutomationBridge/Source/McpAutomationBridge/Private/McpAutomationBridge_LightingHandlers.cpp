#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#include "Engine/TextureCube.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"

#if WITH_EDITOR
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/RectLight.h"
#include "Engine/SkyLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Lightmass/LightmassImportanceVolume.h"
/* UE5.6: LightingBuildOptions.h removed; use console exec */
#include "Editor/UnrealEd/Public/Editor.h"
#include "FileHelpers.h"
#include "LevelEditor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Kismet/GameplayStatics.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleLightingAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.StartsWith(TEXT("spawn_light")) && 
        !Lower.StartsWith(TEXT("spawn_sky_light")) && 
        !Lower.StartsWith(TEXT("build_lighting")) &&
        !Lower.StartsWith(TEXT("ensure_single_sky_light")) &&
        !Lower.StartsWith(TEXT("create_lighting_enabled_level")) &&
        !Lower.StartsWith(TEXT("create_lightmass_volume")) &&
        !Lower.StartsWith(TEXT("setup_volumetric_fog")))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid()) 
    { 
        SendAutomationError(RequestingSocket, RequestId, TEXT("Lighting payload missing"), TEXT("INVALID_PAYLOAD")); 
        return true; 
    }

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("EditorActorSubsystem not available"), TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    if (Lower == TEXT("spawn_light"))
    {
        FString LightClassStr;
        if (!Payload->TryGetStringField(TEXT("lightClass"), LightClassStr) || LightClassStr.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("lightClass required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UClass* LightClass = nullptr;
        if (LightClassStr == TEXT("DirectionalLight")) LightClass = ADirectionalLight::StaticClass();
        else if (LightClassStr == TEXT("PointLight")) LightClass = APointLight::StaticClass();
        else if (LightClassStr == TEXT("SpotLight")) LightClass = ASpotLight::StaticClass();
        else if (LightClassStr == TEXT("RectLight")) LightClass = ARectLight::StaticClass();
        else
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown light class: %s"), *LightClassStr), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FVector Location = FVector::ZeroVector;
        const TSharedPtr<FJsonObject>* LocObj;
        if (Payload->TryGetObjectField(TEXT("location"), LocObj))
        {
            Location.X = (*LocObj)->GetNumberField(TEXT("x"));
            Location.Y = (*LocObj)->GetNumberField(TEXT("y"));
            Location.Z = (*LocObj)->GetNumberField(TEXT("z"));
        }

        FRotator Rotation = FRotator::ZeroRotator;
        const TSharedPtr<FJsonObject>* RotObj;
        if (Payload->TryGetObjectField(TEXT("rotation"), RotObj))
        {
            Rotation.Pitch = (*RotObj)->GetNumberField(TEXT("pitch"));
            Rotation.Yaw = (*RotObj)->GetNumberField(TEXT("yaw"));
            Rotation.Roll = (*RotObj)->GetNumberField(TEXT("roll"));
        }

        AActor* NewLight = ActorSS->SpawnActorFromClass(LightClass, Location, Rotation);
        if (!NewLight)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn light actor"), TEXT("SPAWN_FAILED"));
            return true;
        }

        FString Name;
        if (Payload->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty())
        {
            NewLight->SetActorLabel(Name);
        }

        // Apply properties
        const TSharedPtr<FJsonObject>* Props;
        if (Payload->TryGetObjectField(TEXT("properties"), Props))
        {
            ULightComponent* LightComp = NewLight->FindComponentByClass<ULightComponent>();
            if (LightComp)
            {
                double Intensity;
                if ((*Props)->TryGetNumberField(TEXT("intensity"), Intensity))
                {
                    LightComp->SetIntensity((float)Intensity);
                }

                const TSharedPtr<FJsonObject>* ColorObj;
                if ((*Props)->TryGetObjectField(TEXT("color"), ColorObj))
                {
                    FLinearColor Color;
                    Color.R = (*ColorObj)->GetNumberField(TEXT("r"));
                    Color.G = (*ColorObj)->GetNumberField(TEXT("g"));
                    Color.B = (*ColorObj)->GetNumberField(TEXT("b"));
                    Color.A = (*ColorObj)->HasField(TEXT("a")) ? (*ColorObj)->GetNumberField(TEXT("a")) : 1.0f;
                    LightComp->SetLightColor(Color);
                }

                bool bCastShadows;
                if ((*Props)->TryGetBoolField(TEXT("castShadows"), bCastShadows))
                {
                    LightComp->SetCastShadows(bCastShadows);
                }

                // Type specific properties
                if (UPointLightComponent* PointComp = Cast<UPointLightComponent>(LightComp))
                {
                    double Radius;
                    if ((*Props)->TryGetNumberField(TEXT("attenuationRadius"), Radius))
                    {
                        PointComp->SetAttenuationRadius((float)Radius);
                    }
                }
                
                if (USpotLightComponent* SpotComp = Cast<USpotLightComponent>(LightComp))
                {
                    double InnerCone;
                    if ((*Props)->TryGetNumberField(TEXT("innerConeAngle"), InnerCone))
                    {
                        SpotComp->SetInnerConeAngle((float)InnerCone);
                    }
                    double OuterCone;
                    if ((*Props)->TryGetNumberField(TEXT("outerConeAngle"), OuterCone))
                    {
                        SpotComp->SetOuterConeAngle((float)OuterCone);
                    }
                }

                if (URectLightComponent* RectComp = Cast<URectLightComponent>(LightComp))
                {
                    double Width;
                    if ((*Props)->TryGetNumberField(TEXT("sourceWidth"), Width))
                    {
                        RectComp->SetSourceWidth((float)Width);
                    }
                    double Height;
                    if ((*Props)->TryGetNumberField(TEXT("sourceHeight"), Height))
                    {
                        RectComp->SetSourceHeight((float)Height);
                    }
                }
            }
        }

        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("actorName"), NewLight->GetActorLabel());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Light spawned"), Resp);
        return true;
    }
    else if (Lower == TEXT("spawn_sky_light"))
    {
        AActor* SkyLight = ActorSS->SpawnActorFromClass(ASkyLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
        if (!SkyLight)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn SkyLight"), TEXT("SPAWN_FAILED"));
            return true;
        }

        FString Name;
        if (Payload->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty())
        {
            SkyLight->SetActorLabel(Name);
        }

        USkyLightComponent* SkyComp = SkyLight->FindComponentByClass<USkyLightComponent>();
        if (SkyComp)
        {
            FString SourceType;
            if (Payload->TryGetStringField(TEXT("sourceType"), SourceType))
            {
                if (SourceType == TEXT("SpecifiedCubemap"))
                {
                    SkyComp->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
                    FString CubemapPath;
                    if (Payload->TryGetStringField(TEXT("cubemapPath"), CubemapPath) && !CubemapPath.IsEmpty())
                    {
                                                UTextureCube* Cubemap = Cast<UTextureCube>(StaticLoadObject(UTextureCube::StaticClass(), nullptr, *CubemapPath));
                        if (Cubemap)
                        {
                            SkyComp->Cubemap = Cubemap;
                        }
                    }
                }
                else
                {
                    SkyComp->SourceType = ESkyLightSourceType::SLS_CapturedScene;
                }
            }

            double Intensity;
            if (Payload->TryGetNumberField(TEXT("intensity"), Intensity))
            {
                SkyComp->SetIntensity((float)Intensity);
            }

            bool bRecapture;
            if (Payload->TryGetBoolField(TEXT("recapture"), bRecapture) && bRecapture)
            {
                SkyComp->RecaptureSky();
            }
        }

        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("actorName"), SkyLight->GetActorLabel());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SkyLight spawned"), Resp);
        return true;
    }
    else if (Lower == TEXT("build_lighting"))
    {
        if (GEditor && GEditor->GetEditorWorldContext().World()) {
            if (GEditor && GEditor->GetEditorWorldContext().World()) GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("BuildLighting Production"));
        }
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Lighting build started"), nullptr);
        return true;
    }
    else if (Lower == TEXT("ensure_single_sky_light"))
    {
        TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
        TArray<AActor*> SkyLights;
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->IsA<ASkyLight>())
            {
                SkyLights.Add(Actor);
            }
        }

        FString TargetName;
        Payload->TryGetStringField(TEXT("name"), TargetName);
        if (TargetName.IsEmpty()) TargetName = TEXT("SkyLight");

        int32 RemovedCount = 0;
        AActor* KeptActor = nullptr;

        // Keep the one matching the name, or the first one
        for (AActor* SkyLight : SkyLights)
        {
            if (!KeptActor && (SkyLight->GetActorLabel() == TargetName || TargetName.IsEmpty()))
            {
                KeptActor = SkyLight;
                if (!TargetName.IsEmpty()) SkyLight->SetActorLabel(TargetName);
            }
            else if (!KeptActor)
            {
                KeptActor = SkyLight;
                if (!TargetName.IsEmpty()) SkyLight->SetActorLabel(TargetName);
            }
            else
            {
                ActorSS->DestroyActor(SkyLight);
                RemovedCount++;
            }
        }

        if (!KeptActor)
        {
            // Spawn one if none existed
            KeptActor = ActorSS->SpawnActorFromClass(ASkyLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
            if (KeptActor) KeptActor->SetActorLabel(TargetName);
        }

        if (KeptActor)
        {
            bool bRecapture;
            if (Payload->TryGetBoolField(TEXT("recapture"), bRecapture) && bRecapture)
            {
                if (USkyLightComponent* Comp = KeptActor->FindComponentByClass<USkyLightComponent>())
                {
                    Comp->RecaptureSky();
                }
            }
        }

        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetNumberField(TEXT("removed"), RemovedCount);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ensured single SkyLight"), Resp);
        return true;
    }
    else if (Lower == TEXT("create_lightmass_volume"))
    {
        FVector Location = FVector::ZeroVector;
        const TSharedPtr<FJsonObject>* LocObj;
        if (Payload->TryGetObjectField(TEXT("location"), LocObj))
        {
            Location.X = (*LocObj)->GetNumberField(TEXT("x"));
            Location.Y = (*LocObj)->GetNumberField(TEXT("y"));
            Location.Z = (*LocObj)->GetNumberField(TEXT("z"));
        }

        FVector Size = FVector(1000, 1000, 1000);
        const TSharedPtr<FJsonObject>* SizeObj;
        if (Payload->TryGetObjectField(TEXT("size"), SizeObj))
        {
            Size.X = (*SizeObj)->GetNumberField(TEXT("x"));
            Size.Y = (*SizeObj)->GetNumberField(TEXT("y"));
            Size.Z = (*SizeObj)->GetNumberField(TEXT("z"));
        }

        AActor* Volume = ActorSS->SpawnActorFromClass(ALightmassImportanceVolume::StaticClass(), Location, FRotator::ZeroRotator);
        if (Volume)
        {
            Volume->SetActorScale3D(Size / 200.0f); // Brush size adjustment approximation
            
            FString Name;
            if (Payload->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty())
            {
                Volume->SetActorLabel(Name);
            }
            
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("LightmassImportanceVolume created"), nullptr);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn LightmassImportanceVolume"), TEXT("SPAWN_FAILED"));
        }
        return true;
    }
    else if (Lower == TEXT("setup_volumetric_fog"))
    {
        // Find existing or spawn new ExponentialHeightFog
        AExponentialHeightFog* FogActor = nullptr;
        TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->IsA<AExponentialHeightFog>())
            {
                FogActor = Cast<AExponentialHeightFog>(Actor);
                break;
            }
        }

        if (!FogActor)
        {
            FogActor = Cast<AExponentialHeightFog>(ActorSS->SpawnActorFromClass(AExponentialHeightFog::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
        }

        if (FogActor && FogActor->GetComponent())
        {
            UExponentialHeightFogComponent* FogComp = FogActor->GetComponent();
            FogComp->bEnableVolumetricFog = true;
            
            double Distance;
            if (Payload->TryGetNumberField(TEXT("viewDistance"), Distance))
            {
                FogComp->VolumetricFogDistance = (float)Distance;
            }
            
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Volumetric fog enabled"), nullptr);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to find or spawn ExponentialHeightFog"), TEXT("EXECUTION_ERROR"));
        }
        return true;
    }
    else if (Lower == TEXT("create_lighting_enabled_level"))
    {
        FString Path;
        if (!Payload->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("path required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        if (GEditor)
        {
            // Create a new blank map
            GEditor->NewMap();
            bool bNewMap = true; // Assume success
            if (!bNewMap)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create new map"), TEXT("CREATION_FAILED"));
                return true;
            }

            // Add basic lighting
            ActorSS->SpawnActorFromClass(ADirectionalLight::StaticClass(), FVector(0, 0, 500), FRotator(-45, 0, 0));
            ActorSS->SpawnActorFromClass(ASkyLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
            
            // Save the level
            bool bSaved = FEditorFileUtils::SaveLevel(GEditor->GetEditorWorldContext().World()->PersistentLevel, *Path);
            if (bSaved)
            {
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level created with lighting"), nullptr);
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save level"), TEXT("SAVE_FAILED"));
            }
        }
        else
        {
             SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("EDITOR_NOT_AVAILABLE"));
        }
        return true;
    }

    return false;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Lighting actions require editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
