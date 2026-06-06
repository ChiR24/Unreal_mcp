#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SplineHandlersPrivate.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

bool HandleConfigureMeshSpacing(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    const FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    AActor* Target = ResolveSplineConfigTarget(World, ActorName);
    if (!Target)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Spline configuration target not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    const double Spacing = GetJsonNumberFieldSpline(Payload, TEXT("spacing"), 100.0);
    const bool bUseRandomOffset = GetJsonBoolFieldSpline(Payload, TEXT("useRandomOffset"), false);
    const double RandomOffsetRange = GetJsonNumberFieldSpline(Payload, TEXT("randomOffsetRange"), 0.0);

    if (Spacing <= 0.0 || RandomOffsetRange < 0.0)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("spacing must be greater than 0 and randomOffsetRange must be non-negative"), nullptr, TEXT("INVALID_PARAM"));
        return true;
    }

    SetSplineConfigValue(Target, TEXT("meshSpacing"), FString::SanitizeFloat(Spacing));
    SetSplineConfigValue(Target, TEXT("useRandomOffset"), BoolToSplineConfigString(bUseRandomOffset));
    SetSplineConfigValue(Target, TEXT("randomOffsetRange"), FString::SanitizeFloat(RandomOffsetRange));
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("targetName"), GetSplineConfigTargetName(Target));
    Result->SetStringField(TEXT("targetPath"), Target->GetPathName());
    Result->SetBoolField(TEXT("stored"), true);
    Result->SetNumberField(TEXT("spacing"), Spacing);
    Result->SetBoolField(TEXT("useRandomOffset"), bUseRandomOffset);
    Result->SetNumberField(TEXT("randomOffsetRange"), RandomOffsetRange);

    Self->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Mesh spacing configuration stored on Unreal spline target"), Result);
    return true;
}

bool HandleConfigureMeshRandomization(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    const FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    AActor* Target = ResolveSplineConfigTarget(World, ActorName);
    if (!Target)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Spline configuration target not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    const bool bRandomizeScale = GetJsonBoolFieldSpline(Payload, TEXT("randomizeScale"), false);
    const double MinScale = GetJsonNumberFieldSpline(Payload, TEXT("minScale"), 0.8);
    const double MaxScale = GetJsonNumberFieldSpline(Payload, TEXT("maxScale"), 1.2);
    const bool bRandomizeRotation = GetJsonBoolFieldSpline(Payload, TEXT("randomizeRotation"), false);
    const double RotationRange = GetJsonNumberFieldSpline(Payload, TEXT("rotationRange"), 360.0);

    if (MinScale <= 0.0 || MaxScale <= 0.0 || MinScale > MaxScale || RotationRange < 0.0)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Scale values must be positive, minScale must not exceed maxScale, and rotationRange must be non-negative"), nullptr, TEXT("INVALID_PARAM"));
        return true;
    }

    SetSplineConfigValue(Target, TEXT("randomizeScale"), BoolToSplineConfigString(bRandomizeScale));
    SetSplineConfigValue(Target, TEXT("minScale"), FString::SanitizeFloat(MinScale));
    SetSplineConfigValue(Target, TEXT("maxScale"), FString::SanitizeFloat(MaxScale));
    SetSplineConfigValue(Target, TEXT("randomizeRotation"), BoolToSplineConfigString(bRandomizeRotation));
    SetSplineConfigValue(Target, TEXT("rotationRange"), FString::SanitizeFloat(RotationRange));
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("targetName"), GetSplineConfigTargetName(Target));
    Result->SetStringField(TEXT("targetPath"), Target->GetPathName());
    Result->SetBoolField(TEXT("stored"), true);
    Result->SetBoolField(TEXT("randomizeScale"), bRandomizeScale);
    Result->SetNumberField(TEXT("minScale"), MinScale);
    Result->SetNumberField(TEXT("maxScale"), MaxScale);
    Result->SetBoolField(TEXT("randomizeRotation"), bRandomizeRotation);
    Result->SetNumberField(TEXT("rotationRange"), RotationRange);

    Self->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Mesh randomization configuration stored on Unreal spline target"), Result);
    return true;
}
#endif
