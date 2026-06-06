#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

ALandscape *McpFindLandscape(const TSharedPtr<FJsonObject> &Payload)
{
    const FString LandscapeName = McpGetFirstStringField(Payload, {TEXT("landscapeName"), TEXT("name"), TEXT("targetActor")});
    FString LandscapePath = McpGetFirstStringField(Payload, {TEXT("landscapePath"), TEXT("landscapeActorPath"), TEXT("actorPath")});
    LandscapePath.TrimStartAndEndInline();
    if (!LandscapePath.IsEmpty() && !LandscapePath.StartsWith(TEXT("/")))
    {
        LandscapePath = TEXT("/") + LandscapePath;
    }

    UWorld *World = McpGetEditorWorld();
    if (World)
    {
        for (TActorIterator<ALandscape> It(World); It; ++It)
        {
            ALandscape *Landscape = *It;
            if (!Landscape)
            {
                continue;
            }
            if (!LandscapeName.IsEmpty() && Landscape->GetActorLabel().Equals(LandscapeName, ESearchCase::IgnoreCase))
            {
                return Landscape;
            }
            if (!LandscapePath.IsEmpty() &&
                ((Landscape->GetPackage() && Landscape->GetPackage()->GetPathName().Equals(LandscapePath, ESearchCase::IgnoreCase)) ||
                 Landscape->GetPathName().Equals(LandscapePath, ESearchCase::IgnoreCase) ||
                 Landscape->GetPathName(nullptr).Equals(LandscapePath, ESearchCase::IgnoreCase)))
            {
                return Landscape;
            }
        }

        if (LandscapeName.IsEmpty() && LandscapePath.IsEmpty())
        {
            for (TActorIterator<ALandscape> It(World); It; ++It)
            {
                return *It;
            }
        }
    }

    if (!LandscapePath.IsEmpty())
    {
        return Cast<ALandscape>(StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
    }
    return nullptr;
}
bool McpResolveProjectFilePath(const FString &InputPath, FString &OutAbsolutePath, FString &OutSafePath, FString &OutError)
{
    OutSafePath = SanitizeProjectFilePath(InputPath);
    if (OutSafePath.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Invalid or unsafe project-relative file path: %s"), *InputPath);
        return false;
    }

    OutAbsolutePath = FPaths::ProjectDir() / OutSafePath;
    OutAbsolutePath = FPaths::ConvertRelativePathToFull(OutAbsolutePath);
    FPaths::NormalizeFilename(OutAbsolutePath);
    return McpValidateProjectSnapshotFilePath(OutAbsolutePath, OutError);
}
bool McpGetLandscapeExtentForEnvironmentAction(ALandscape *Landscape, int32 &OutMinX, int32 &OutMinY, int32 &OutMaxX, int32 &OutMaxY)
{
    ULandscapeInfo *LandscapeInfo = Landscape ? Landscape->GetLandscapeInfo() : nullptr;
    if (!LandscapeInfo)
    {
        return false;
    }
    if (LandscapeInfo->GetLandscapeExtent(OutMinX, OutMinY, OutMaxX, OutMaxY))
    {
        return true;
    }
    return McpLandscapeMetadataTags::GetLandscapeMetadataExtent(Landscape, OutMinX, OutMinY, OutMaxX, OutMaxY);
}
void McpApplyHeightmapRegionFromPayload(const TSharedPtr<FJsonObject> &Payload,
                                               const int32 FullMinX, const int32 FullMinY,
                                               const int32 FullMaxX, const int32 FullMaxY,
                                               int32 &OutMinX, int32 &OutMinY,
                                               int32 &OutMaxX, int32 &OutMaxY)
{
    OutMinX = FullMinX;
    OutMinY = FullMinY;
    OutMaxX = FullMaxX;
    OutMaxY = FullMaxY;

    const TSharedPtr<FJsonObject> *RegionObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("region"), RegionObj) && RegionObj && RegionObj->IsValid())
    {
        (*RegionObj)->TryGetNumberField(TEXT("minX"), OutMinX);
        (*RegionObj)->TryGetNumberField(TEXT("minY"), OutMinY);
        (*RegionObj)->TryGetNumberField(TEXT("maxX"), OutMaxX);
        (*RegionObj)->TryGetNumberField(TEXT("maxY"), OutMaxY);
    }
    else
    {
        Payload->TryGetNumberField(TEXT("minX"), OutMinX);
        Payload->TryGetNumberField(TEXT("minY"), OutMinY);
        Payload->TryGetNumberField(TEXT("maxX"), OutMaxX);
        Payload->TryGetNumberField(TEXT("maxY"), OutMaxY);
    }

    OutMinX = FMath::Clamp(OutMinX, FullMinX, FullMaxX);
    OutMinY = FMath::Clamp(OutMinY, FullMinY, FullMaxY);
    OutMaxX = FMath::Clamp(OutMaxX, FullMinX, FullMaxX);
    OutMaxY = FMath::Clamp(OutMaxY, FullMinY, FullMaxY);
}
ALandscape *McpFindLandscapeForEnvironmentAction(const TSharedPtr<FJsonObject> &Payload)
{
    return McpFindLandscape(Payload);
}
bool McpBuildProjectFilePath(const FString &InputPath, FString &OutAbsolutePath, FString &OutSafePath, FString &OutError)
{
    return McpResolveProjectFilePath(InputPath, OutAbsolutePath, OutSafePath, OutError);
}

} // namespace McpEnvironmentHandlers
#endif
