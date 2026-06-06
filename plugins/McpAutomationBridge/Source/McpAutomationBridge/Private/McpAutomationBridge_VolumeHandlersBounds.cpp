#include "McpAutomationBridge_VolumeActionDeclarations.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_VolumeGeometry.h"
#include "McpAutomationBridge_VolumeRequestParsing.h"
#include "McpAutomationBridge_VolumeResponses.h"
#include "McpAutomationBridge_VolumeWorldResolution.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"

namespace McpVolumeHandlers
{
#if WITH_EDITOR
bool HandleSetVolumeBounds(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;
    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT(""));
    FString ValidationError;
    if (!ValidateVolumeName(VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }
    TArray<float> BoundsValues;
    if (Payload->HasTypedField<EJson::Array>(TEXT("bounds")))
    {
        for (const TSharedPtr<FJsonValue>& Value : Payload->GetArrayField(TEXT("bounds")))
        {
            BoundsValues.Add(static_cast<float>(Value->AsNumber()));
        }
    }
    if (BoundsValues.Num() != 6)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("bounds must be an array of 6 values [minX, minY, minZ, maxX, maxY, maxZ]"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    for (float Value : BoundsValues)
    {
        if (!FMath::IsFinite(Value))
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("bounds contains NaN or Infinity values"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }
    const FVector MinBound(BoundsValues[0], BoundsValues[1], BoundsValues[2]);
    const FVector MaxBound(BoundsValues[3], BoundsValues[4], BoundsValues[5]);
    const FVector Center = (MinBound + MaxBound) * 0.5f;
    const FVector Extent = (MaxBound - MinBound) * 0.5f;
    if (Extent.X <= 0.0f || Extent.Y <= 0.0f || Extent.Z <= 0.0f)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("bounds must define a valid volume (max > min for all axes)"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    UWorld* World = nullptr;
    if (!ResolveEditorWorld(Subsystem, RequestId, Socket, World))
    {
        return true;
    }
    AActor* VolumeActor = FindVolumeByName(World, VolumeName);
    if (!VolumeActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Volume not found: %s"), *VolumeName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }
    VolumeActor->SetActorLocation(Center);
    SetVolumeExtentGeometry(VolumeActor, Extent);
    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("volumeName"), VolumeName);
    McpHandlerUtils::AddVerification(ResponseJson, VolumeActor);
    TSharedPtr<FJsonObject> BoundsJson = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> MinArray;
    TArray<TSharedPtr<FJsonValue>> MaxArray;
    MinArray.Add(MakeShared<FJsonValueNumber>(MinBound.X));
    MinArray.Add(MakeShared<FJsonValueNumber>(MinBound.Y));
    MinArray.Add(MakeShared<FJsonValueNumber>(MinBound.Z));
    MaxArray.Add(MakeShared<FJsonValueNumber>(MaxBound.X));
    MaxArray.Add(MakeShared<FJsonValueNumber>(MaxBound.Y));
    MaxArray.Add(MakeShared<FJsonValueNumber>(MaxBound.Z));
    BoundsJson->SetArrayField(TEXT("min"), MinArray);
    BoundsJson->SetArrayField(TEXT("max"), MaxArray);
    ResponseJson->SetObjectField(TEXT("bounds"), BoundsJson);
    ResponseJson->SetObjectField(TEXT("center"), CreateVectorObject(Center));
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set bounds for volume: %s"), *VolumeName), ResponseJson);
    return true;
}
#endif
}
