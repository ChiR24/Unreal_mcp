#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

namespace McpEnvironmentHandlers {
namespace {

bool MatchesSnapshotActorPath(const AActor *Actor, const FString &ActorPath)
{
    if (ActorPath.IsEmpty())
    {
        return true;
    }
    return Actor->GetPathName().Equals(ActorPath, ESearchCase::CaseSensitive) ||
        Actor->GetPackage()->GetName().Equals(ActorPath, ESearchCase::CaseSensitive);
}

ADirectionalLight *FindSnapshotDirectionalLight(const FString &ActorPath = FString())
{
    UWorld *World = McpHandlerUtils::GetEditorWorld();
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<ADirectionalLight> It(World); It; ++It)
    {
        if (IsValid(*It) && MatchesSnapshotActorPath(*It, ActorPath))
        {
            return *It;
        }
    }
    return nullptr;
}

ASkyLight *FindSnapshotSkyLight(const FString &ActorPath = FString())
{
    UWorld *World = McpHandlerUtils::GetEditorWorld();
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<ASkyLight> It(World); It; ++It)
    {
        if (IsValid(*It) && MatchesSnapshotActorPath(*It, ActorPath))
        {
            return *It;
        }
    }
    return nullptr;
}

bool ReadSnapshotNumber(
    const TSharedPtr<FJsonObject> &Snapshot, const TCHAR *FieldName,
    const bool bAllowNegative, double &OutValue)
{
    return Snapshot->TryGetNumberField(FieldName, OutValue) &&
        FMath::IsFinite(OutValue) &&
        FMath::Abs(OutValue) <= static_cast<double>(TNumericLimits<float>::Max()) &&
        (bAllowNegative || OutValue >= 0.0);
}

FRotator RotationFromTimeOfDay(const double TimeOfDay)
{
    const double WrappedTime = FMath::Fmod(TimeOfDay, 24.0);
    return FRotator(WrappedTime / 24.0 * 360.0 - 90.0, 0.0, 0.0);
}

}

bool McpParseEnvironmentSnapshotLighting(
    const TSharedPtr<FJsonObject> &Snapshot, double &OutTimeOfDay,
    double &OutSunIntensity, double &OutSkylightIntensity, FRotator &OutRotation)
{
    if (!Snapshot.IsValid())
    {
        return false;
    }

    double TimeOfDay = 0.0;
    double SunIntensity = 0.0;
    double SkylightIntensity = 0.0;
    const bool bValidLightingFields =
        ReadSnapshotNumber(Snapshot, TEXT("timeOfDay"), false, TimeOfDay) &&
        TimeOfDay <= 24.0 &&
        ReadSnapshotNumber(Snapshot, TEXT("sunIntensity"), false, SunIntensity) &&
        ReadSnapshotNumber(Snapshot, TEXT("skylightIntensity"), false, SkylightIntensity);

    FRotator Rotation = FRotator::ZeroRotator;
    bool bValidRotation = false;
    const TSharedPtr<FJsonValue> *VersionField =
        Snapshot->Values.Find(TEXT("version"));
    const bool bHasVersion = VersionField != nullptr;
    double SnapshotVersion = 0.0;
    if (bHasVersion &&
        (!VersionField->IsValid() ||
         (*VersionField)->Type != EJson::Number ||
         !Snapshot->TryGetNumberField(TEXT("version"), SnapshotVersion) ||
         !FMath::IsFinite(SnapshotVersion) || SnapshotVersion != 1.0))
    {
        return false;
    }
    if (Snapshot->HasField(TEXT("directionalLightRotation")))
    {
        double Pitch = 0.0;
        double Yaw = 0.0;
        double Roll = 0.0;
        const TSharedPtr<FJsonObject> *RotationObject = nullptr;
        bValidRotation =
            Snapshot->TryGetObjectField(TEXT("directionalLightRotation"), RotationObject) &&
            RotationObject && RotationObject->IsValid() &&
            ReadSnapshotNumber(*RotationObject, TEXT("pitch"), true, Pitch) &&
            ReadSnapshotNumber(*RotationObject, TEXT("yaw"), true, Yaw) &&
            ReadSnapshotNumber(*RotationObject, TEXT("roll"), true, Roll);
        if (bValidRotation)
        {
            Rotation = FRotator(Pitch, Yaw, Roll);
        }
    }
    else if (!bHasVersion && bValidLightingFields)
    {
        Rotation = RotationFromTimeOfDay(TimeOfDay);
        bValidRotation = true;
    }

    const double RotationTimeOfDay =
        FMath::Fmod(static_cast<double>(Rotation.Pitch) + 450.0, 360.0) / 360.0 * 24.0;
    const double TimeDifference = FMath::Abs(TimeOfDay - RotationTimeOfDay);
    const bool bConsistentTime =
        TimeDifference <= 0.01 || FMath::Abs(TimeDifference - 24.0) <= 0.01;
    if (!bValidLightingFields || !bValidRotation || !bConsistentTime)
    {
        return false;
    }

    OutTimeOfDay = TimeOfDay;
    OutSunIntensity = SunIntensity;
    OutSkylightIntensity = SkylightIntensity;
    OutRotation = Rotation;
    return true;
}

bool McpCaptureEnvironmentSnapshot(
    TSharedPtr<FJsonObject> Snapshot, TSharedPtr<FJsonObject> Resp,
    const FString &DirectionalLightActorPath, const FString &SkyLightActorPath,
    FString &OutMessage, FString &OutErrorCode)
{
    ADirectionalLight *SunActor =
        FindSnapshotDirectionalLight(DirectionalLightActorPath);
    ASkyLight *SkyActor = FindSnapshotSkyLight(SkyLightActorPath);
    if (!SunActor || !SkyActor)
    {
        OutMessage = !SunActor ? TEXT("No directional light found for environment snapshot")
                               : TEXT("No skylight found for environment snapshot");
        OutErrorCode = !SunActor ? TEXT("SUN_NOT_FOUND") : TEXT("SKYLIGHT_NOT_FOUND");
        return false;
    }

    UDirectionalLightComponent *SunComponent =
        Cast<UDirectionalLightComponent>(SunActor->GetLightComponent());
    USkyLightComponent *SkyComponent = SkyActor->GetLightComponent();
    if (!SunComponent || !SkyComponent)
    {
        OutMessage = TEXT("Environment snapshot light components are unavailable");
        OutErrorCode = TEXT("LIGHT_COMPONENT_NOT_FOUND");
        return false;
    }

    const FRotator Rotation = SunActor->GetActorRotation();
    const double TimeOfDay =
        FMath::Fmod(static_cast<double>(Rotation.Pitch) + 450.0, 360.0) / 360.0 * 24.0;
    Snapshot->SetNumberField(TEXT("version"), 1);
    Snapshot->SetNumberField(TEXT("timeOfDay"), TimeOfDay);
    Snapshot->SetObjectField(TEXT("directionalLightRotation"), McpHandlerUtils::RotatorToJson(Rotation));
    Snapshot->SetNumberField(TEXT("sunIntensity"), SunComponent->Intensity);
    Snapshot->SetNumberField(TEXT("skylightIntensity"), SkyComponent->Intensity);
    Snapshot->SetStringField(TEXT("directionalLightActorPath"), SunActor->GetPathName());
    Snapshot->SetStringField(TEXT("skyLightActorPath"), SkyActor->GetPathName());

    Resp->SetObjectField(TEXT("snapshot"), Snapshot);
    Resp->SetStringField(TEXT("directionalLightActorPath"), SunActor->GetPathName());
    Resp->SetStringField(TEXT("skyLightActorPath"), SkyActor->GetPathName());
    OutMessage = TEXT("Environment snapshot captured");
    OutErrorCode.Empty();
    return true;
}

bool McpApplyEnvironmentSnapshot(
    const TSharedPtr<FJsonObject> &Snapshot, TSharedPtr<FJsonObject> Resp,
    FString &OutMessage, FString &OutErrorCode)
{
    double TimeOfDay = 0.0;
    double SunIntensity = 0.0;
    double SkylightIntensity = 0.0;
    FRotator Rotation = FRotator::ZeroRotator;
    if (!McpParseEnvironmentSnapshotLighting(
            Snapshot, TimeOfDay, SunIntensity, SkylightIntensity, Rotation))
    {
        OutMessage = TEXT("Environment snapshot has invalid or missing lighting fields");
        OutErrorCode = TEXT("INVALID_SNAPSHOT");
        return false;
    }

    FString SunActorPath;
    FString SkyActorPath;
    Snapshot->TryGetStringField(TEXT("directionalLightActorPath"), SunActorPath);
    Snapshot->TryGetStringField(TEXT("skyLightActorPath"), SkyActorPath);
    ADirectionalLight *SunActor = FindSnapshotDirectionalLight(SunActorPath);
    ASkyLight *SkyActor = FindSnapshotSkyLight(SkyActorPath);
    UDirectionalLightComponent *SunComponent =
        SunActor ? Cast<UDirectionalLightComponent>(SunActor->GetLightComponent()) : nullptr;
    USkyLightComponent *SkyComponent = SkyActor ? SkyActor->GetLightComponent() : nullptr;
    if (!SunActor || !SkyActor || !SunComponent || !SkyComponent)
    {
        OutMessage = TEXT("Environment snapshot target lights are unavailable");
        OutErrorCode = TEXT("LIGHT_NOT_FOUND");
        return false;
    }

    SunActor->Modify();
    SunComponent->Modify();
    SkyActor->Modify();
    SkyComponent->Modify();
    SunActor->SetActorRotation(Rotation);
    SunComponent->SetIntensity(static_cast<float>(SunIntensity));
    SkyComponent->SetIntensity(static_cast<float>(SkylightIntensity));
    SunComponent->MarkRenderStateDirty();
    SkyComponent->MarkRenderStateDirty();
    SunActor->MarkPackageDirty();
    SkyActor->MarkPackageDirty();

    Resp->SetNumberField(TEXT("timeOfDay"), TimeOfDay);
    Resp->SetObjectField(TEXT("directionalLightRotation"), McpHandlerUtils::RotatorToJson(Rotation));
    Resp->SetNumberField(TEXT("sunIntensity"), SunIntensity);
    Resp->SetNumberField(TEXT("skylightIntensity"), SkylightIntensity);
    Resp->SetStringField(TEXT("directionalLightActorPath"), SunActor->GetPathName());
    Resp->SetStringField(TEXT("skyLightActorPath"), SkyActor->GetPathName());
    OutMessage = TEXT("Environment snapshot applied");
    OutErrorCode.Empty();
    return true;
}

}
