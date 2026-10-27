#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpSplinePrivate.h"

#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

bool HandleConfigureMeshSpacing(
    UEngineWireSubsystem* Self,
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

    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    AActor* Target = ResolveSplineConfigTarget(World, ActorName);
    if (!Target)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Spline configuration target not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Only what was sent is stored: the defaults of the fields left out overwrote what an earlier call stored.
    const bool bHasSpacing = Payload->HasField(TEXT("spacing"));
    const bool bHasUseOffset = Payload->HasField(TEXT("useRandomOffset"));
    const bool bHasOffsetRange = Payload->HasField(TEXT("randomOffsetRange"));
    const double Spacing = GetJsonNumberField(Payload, TEXT("spacing"), 100.0);
    const double RandomOffsetRange = GetJsonNumberField(Payload, TEXT("randomOffsetRange"), 0.0);
    if (!bHasSpacing && !bHasUseOffset && !bHasOffsetRange)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Pass spacing, useRandomOffset or randomOffsetRange. Nothing was stored."), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if ((bHasSpacing && Spacing <= 0.0) || (bHasOffsetRange && RandomOffsetRange < 0.0))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("spacing must be greater than 0 and randomOffsetRange must be non-negative"), nullptr, TEXT("INVALID_PARAM"));
        return true;
    }

    if (bHasSpacing) SetSplineConfigValue(Target, TEXT("meshSpacing"), FString::SanitizeFloat(Spacing));
    if (bHasUseOffset) SetSplineConfigValue(Target, TEXT("useRandomOffset"), BoolToSplineConfigString(GetJsonBoolField(Payload, TEXT("useRandomOffset"), false)));
    if (bHasOffsetRange) SetSplineConfigValue(Target, TEXT("randomOffsetRange"), FString::SanitizeFloat(RandomOffsetRange));
    World->MarkPackageDirty();

    // What scatter_meshes_along_spline will now read for this target.
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("targetName"), GetSplineConfigTargetName(Target));
    Result->SetStringField(TEXT("targetPath"), Target->GetPathName());
    Result->SetBoolField(TEXT("stored"), true);
    Result->SetNumberField(TEXT("spacing"), GetConfiguredSplineNumber(Target, World, TEXT("meshSpacing"), 100.0));
    Result->SetBoolField(TEXT("useRandomOffset"), GetConfiguredSplineBool(Target, World, TEXT("useRandomOffset"), false));
    Result->SetNumberField(TEXT("randomOffsetRange"), GetConfiguredSplineNumber(Target, World, TEXT("randomOffsetRange"), 0.0));

    Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Spacing stored on %s for "
        "scatter_meshes_along_spline; nothing is placed until it runs"), *GetSplineConfigTargetName(Target)), Result);
    return true;
}

bool HandleConfigureMeshRandomization(
    UEngineWireSubsystem* Self,
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

    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    AActor* Target = ResolveSplineConfigTarget(World, ActorName);
    if (!Target)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Spline configuration target not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Only what was sent is stored; a scale sent alone is checked against the other one already stored.
    const TCHAR* const Fields[] = {TEXT("randomizeScale"), TEXT("minScale"), TEXT("maxScale"), TEXT("randomizeRotation"), TEXT("rotationRange")};
    bool bAnySent = false;
    for (const TCHAR* Field : Fields)
    {
        bAnySent |= Payload->HasField(Field);
    }
    if (!bAnySent)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Pass randomizeScale, minScale, maxScale, randomizeRotation or "
            "rotationRange. Nothing was stored."), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const double MinScale = Payload->HasField(TEXT("minScale")) ? GetJsonNumberField(Payload, TEXT("minScale"), 0.8)
                                                                 : GetConfiguredSplineNumber(Target, World, TEXT("minScale"), 0.8);
    const double MaxScale = Payload->HasField(TEXT("maxScale")) ? GetJsonNumberField(Payload, TEXT("maxScale"), 1.2)
                                                                 : GetConfiguredSplineNumber(Target, World, TEXT("maxScale"), 1.2);
    const double RotationRange = GetJsonNumberField(Payload, TEXT("rotationRange"), 360.0);

    if (MinScale <= 0.0 || MaxScale <= 0.0 || MinScale > MaxScale || RotationRange < 0.0)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Scale values must be positive, minScale must not exceed maxScale, and rotationRange must be non-negative"), nullptr, TEXT("INVALID_PARAM"));
        return true;
    }

    if (Payload->HasField(TEXT("randomizeScale"))) SetSplineConfigValue(Target, TEXT("randomizeScale"), BoolToSplineConfigString(GetJsonBoolField(Payload, TEXT("randomizeScale"), false)));
    if (Payload->HasField(TEXT("minScale"))) SetSplineConfigValue(Target, TEXT("minScale"), FString::SanitizeFloat(MinScale));
    if (Payload->HasField(TEXT("maxScale"))) SetSplineConfigValue(Target, TEXT("maxScale"), FString::SanitizeFloat(MaxScale));
    if (Payload->HasField(TEXT("randomizeRotation"))) SetSplineConfigValue(Target, TEXT("randomizeRotation"), BoolToSplineConfigString(GetJsonBoolField(Payload, TEXT("randomizeRotation"), false)));
    if (Payload->HasField(TEXT("rotationRange"))) SetSplineConfigValue(Target, TEXT("rotationRange"), FString::SanitizeFloat(RotationRange));
    World->MarkPackageDirty();

    // What scatter_meshes_along_spline will now read for this target.
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("targetName"), GetSplineConfigTargetName(Target));
    Result->SetStringField(TEXT("targetPath"), Target->GetPathName());
    Result->SetBoolField(TEXT("stored"), true);
    Result->SetBoolField(TEXT("randomizeScale"), GetConfiguredSplineBool(Target, World, TEXT("randomizeScale"), false));
    Result->SetNumberField(TEXT("minScale"), GetConfiguredSplineNumber(Target, World, TEXT("minScale"), 0.8));
    Result->SetNumberField(TEXT("maxScale"), GetConfiguredSplineNumber(Target, World, TEXT("maxScale"), 1.2));
    Result->SetBoolField(TEXT("randomizeRotation"), GetConfiguredSplineBool(Target, World, TEXT("randomizeRotation"), false));
    Result->SetNumberField(TEXT("rotationRange"), GetConfiguredSplineNumber(Target, World, TEXT("rotationRange"), 360.0));

    Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Randomization stored on %s for "
        "scatter_meshes_along_spline; nothing is placed until it runs"), *GetSplineConfigTargetName(Target)), Result);
    return true;
}
