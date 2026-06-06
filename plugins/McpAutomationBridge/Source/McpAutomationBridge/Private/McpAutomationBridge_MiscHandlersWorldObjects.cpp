#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_MiscHandlersSupport.h"
#include "McpHandlerUtils.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_EDITOR
namespace McpMiscHandlers
{
bool HandleCreatePostProcessVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString VolumeName = GetStringField(Payload, TEXT("volumeName"), TEXT("PostProcessVolume"));
    FVector Location = GetVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    FVector Extent = GetVectorField(Payload, TEXT("extent"), FVector(1000.0f, 1000.0f, 500.0f));
    bool bUnbound = GetBoolField(Payload, TEXT("unbound"), false);
    double BlendRadius = GetNumberField(Payload, TEXT("blendRadius"), 100.0);
    double BlendWeight = GetNumberField(Payload, TEXT("blendWeight"), 1.0);
    double Priority = GetNumberField(Payload, TEXT("priority"), 0.0);
    (void)Extent;

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Editor world not available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APostProcessVolume* Volume = World->SpawnActor<APostProcessVolume>(Location, FRotator::ZeroRotator, SpawnParams);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to spawn PostProcessVolume"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }

    Volume->SetActorLabel(VolumeName);
    Volume->bUnbound = bUnbound;
    Volume->BlendRadius = static_cast<float>(BlendRadius);
    Volume->BlendWeight = static_cast<float>(BlendWeight);
    Volume->Priority = static_cast<float>(Priority);

    if (Payload->HasField(TEXT("settings")))
    {
        const TSharedPtr<FJsonObject>* SettingsPtr = nullptr;
        if (Payload->TryGetObjectField(TEXT("settings"), SettingsPtr) && SettingsPtr)
        {
            FPostProcessSettings& Settings = Volume->Settings;
            double Bloom;
            if ((*SettingsPtr)->TryGetNumberField(TEXT("bloomIntensity"), Bloom))
            {
                Settings.bOverride_BloomIntensity = true;
                Settings.BloomIntensity = static_cast<float>(Bloom);
            }
            double Exposure;
            if ((*SettingsPtr)->TryGetNumberField(TEXT("exposureCompensation"), Exposure))
            {
                Settings.bOverride_AutoExposureBias = true;
                Settings.AutoExposureBias = static_cast<float>(Exposure);
            }
            double Saturation;
            if ((*SettingsPtr)->TryGetNumberField(TEXT("saturation"), Saturation))
            {
                Settings.bOverride_ColorSaturation = true;
                Settings.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.0f);
            }
            double Contrast;
            if ((*SettingsPtr)->TryGetNumberField(TEXT("contrast"), Contrast))
            {
                Settings.bOverride_ColorContrast = true;
                Settings.ColorContrast = FVector4(Contrast, Contrast, Contrast, 1.0f);
            }
            double VignetteIntensity;
            if ((*SettingsPtr)->TryGetNumberField(TEXT("vignetteIntensity"), VignetteIntensity))
            {
                Settings.bOverride_VignetteIntensity = true;
                Settings.VignetteIntensity = static_cast<float>(VignetteIntensity);
            }
        }
    }

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("volumeName"), Volume->GetActorLabel());
    ResponseJson->SetStringField(TEXT("volumePath"), Volume->GetPathName());
    ResponseJson->SetBoolField(TEXT("unbound"), bUnbound);
    ResponseJson->SetNumberField(TEXT("blendRadius"), BlendRadius);
    ResponseJson->SetNumberField(TEXT("priority"), Priority);
    McpHandlerUtils::AddVerification(ResponseJson, Volume);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created PostProcessVolume: %s"), *VolumeName), ResponseJson);
    return true;
}

bool HandleCreateCamera(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString CameraName = GetStringField(Payload, TEXT("cameraName"), TEXT("Camera"));
    FVector Location = GetVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = GetRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    double FOV = GetNumberField(Payload, TEXT("fov"), 90.0);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Editor world not available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ACameraActor* Camera = World->SpawnActor<ACameraActor>(Location, Rotation, SpawnParams);
    if (!Camera)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to spawn camera actor"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }

    Camera->SetActorLabel(CameraName);
    if (UCameraComponent* CamComp = Camera->GetCameraComponent())
    {
        CamComp->SetFieldOfView(static_cast<float>(FOV));
    }

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("cameraName"), Camera->GetActorLabel());
    ResponseJson->SetStringField(TEXT("cameraPath"), Camera->GetPathName());
    ResponseJson->SetNumberField(TEXT("fov"), FOV);
    McpHandlerUtils::AddVerification(ResponseJson, Camera);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created camera: %s"), *CameraName), ResponseJson);
    return true;
}

bool HandleSetCameraFOV(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString CameraName = GetStringField(Payload, TEXT("cameraName"), TEXT(""));
    double FOV = GetNumberField(Payload, TEXT("fov"), 90.0);

    if (CameraName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("cameraName is required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Editor world not available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    ACameraActor* Camera = nullptr;
    for (TActorIterator<ACameraActor> It(World); It; ++It)
    {
        if (It->GetActorLabel().Equals(CameraName, ESearchCase::IgnoreCase) ||
            It->GetName().Equals(CameraName, ESearchCase::IgnoreCase))
        {
            Camera = *It;
            break;
        }
    }

    if (!Camera)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Camera not found: %s"), *CameraName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    if (UCameraComponent* CamComp = Camera->GetCameraComponent())
    {
        CamComp->SetFieldOfView(static_cast<float>(FOV));
    }

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("cameraName"), Camera->GetActorLabel());
    ResponseJson->SetNumberField(TEXT("fov"), FOV);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set FOV to %.1f for camera: %s"), FOV, *CameraName), ResponseJson);
    return true;
}
}
#endif
