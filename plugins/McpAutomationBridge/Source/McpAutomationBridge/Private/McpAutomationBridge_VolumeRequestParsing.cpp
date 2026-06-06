#include "McpAutomationBridge_VolumeRequestParsing.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"

namespace VolumeHelpers
{
#if WITH_EDITOR
FVector GetVectorFromPayload(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, FVector Default)
{
    return ExtractVectorField(Payload, *FieldName, Default);
}

FRotator GetRotatorFromPayload(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, FRotator Default)
{
    return ExtractRotatorField(Payload, *FieldName, Default);
}

bool ValidateVolumeName(const FString& VolumeName, FString& OutError)
{
    if (VolumeName.IsEmpty())
    {
        OutError = TEXT("volumeName is required");
        return false;
    }
    if (VolumeName.Contains(TEXT("..")) || VolumeName.Contains(TEXT("/")) || VolumeName.Contains(TEXT("\\")))
    {
        OutError = TEXT("volumeName must not contain path separators or traversal sequences");
        return false;
    }
    if (VolumeName.Contains(TEXT(":")))
    {
        OutError = TEXT("volumeName must not contain drive letters");
        return false;
    }
    return true;
}

bool ValidateExtent(const FVector& Extent, FString& OutError)
{
    if (!FMath::IsFinite(Extent.X) || !FMath::IsFinite(Extent.Y) || !FMath::IsFinite(Extent.Z))
    {
        OutError = TEXT("extent contains NaN or Infinity values");
        return false;
    }
    if (Extent.X <= 0.0f || Extent.Y <= 0.0f || Extent.Z <= 0.0f)
    {
        OutError = TEXT("extent values must be positive");
        return false;
    }
    return true;
}

bool ValidateRadius(float Radius, FString& OutError)
{
    if (!FMath::IsFinite(Radius))
    {
        OutError = TEXT("radius contains NaN or Infinity value");
        return false;
    }
    if (Radius <= 0.0f)
    {
        OutError = TEXT("radius must be positive");
        return false;
    }
    return true;
}

bool ValidateCapsuleDimensions(float Radius, float HalfHeight, FString& OutError)
{
    if (!FMath::IsFinite(Radius) || !FMath::IsFinite(HalfHeight))
    {
        OutError = TEXT("capsule dimensions contain NaN or Infinity values");
        return false;
    }
    if (Radius <= 0.0f)
    {
        OutError = TEXT("capsule radius must be positive");
        return false;
    }
    if (HalfHeight <= 0.0f)
    {
        OutError = TEXT("capsule half height must be positive");
        return false;
    }
    return true;
}

bool ValidateLocation(const FVector& Location, FString& OutError)
{
    if (!FMath::IsFinite(Location.X) || !FMath::IsFinite(Location.Y) || !FMath::IsFinite(Location.Z))
    {
        OutError = TEXT("location contains NaN or Infinity values");
        return false;
    }
    return true;
}

bool ReadNamedTransform(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& DefaultName, FVolumeCreateArgs& OutArgs)
{
    FString ValidationError;
    OutArgs.VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), DefaultName);
    if (!ValidateVolumeName(OutArgs.VolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return false;
    }
    OutArgs.Location = GetVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    if (!ValidateLocation(OutArgs.Location, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return false;
    }
    OutArgs.Rotation = GetRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    return true;
}

bool ReadExtent(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& FieldName, const FVector& DefaultExtent, FVector& OutExtent)
{
    FString ValidationError;
    OutExtent = GetVectorFromPayload(Payload, FieldName, DefaultExtent);
    if (!ValidateExtent(OutExtent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return false;
    }
    return true;
}
#endif
}
