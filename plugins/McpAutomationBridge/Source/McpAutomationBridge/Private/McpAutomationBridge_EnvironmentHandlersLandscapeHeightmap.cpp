#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

bool McpExportLandscapeHeightmap(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                        FString &OutMessage, FString &OutErrorCode)
{
    ALandscape *Landscape = McpFindLandscape(Payload);
    if (!Landscape)
    {
        OutMessage = TEXT("Landscape not found for export_heightmap");
        OutErrorCode = TEXT("LANDSCAPE_NOT_FOUND");
        return false;
    }

    FString OutputPath = McpGetFirstStringField(Payload, {TEXT("outputPath"), TEXT("path")});
    if (OutputPath.IsEmpty())
    {
        OutMessage = TEXT("outputPath required for export_heightmap");
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }

    FString AbsolutePath;
    FString SafePath;
    FString PathError;
    if (!McpResolveProjectFilePath(OutputPath, AbsolutePath, SafePath, PathError))
    {
        OutMessage = PathError;
        OutErrorCode = TEXT("SECURITY_VIOLATION");
        return false;
    }

    ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
    int32 FullMinX = 0, FullMinY = 0, FullMaxX = 0, FullMaxY = 0;
    if (!McpGetLandscapeExtentForEnvironmentAction(Landscape, FullMinX, FullMinY, FullMaxX, FullMaxY))
    {
        OutMessage = TEXT("Failed to read landscape extent");
        OutErrorCode = TEXT("INVALID_LANDSCAPE");
        return false;
    }

    int32 MinX = FullMinX;
    int32 MinY = FullMinY;
    int32 MaxX = FullMaxX;
    int32 MaxY = FullMaxY;
    McpApplyHeightmapRegionFromPayload(Payload, FullMinX, FullMinY, FullMaxX, FullMaxY, MinX, MinY, MaxX, MaxY);

    if (MinX > MaxX || MinY > MaxY)
    {
        OutMessage = FString::Printf(TEXT("Invalid heightmap export region: min (%d, %d) must not exceed max (%d, %d)"),
                                     MinX, MinY, MaxX, MaxY);
        OutErrorCode = TEXT("INVALID_REGION");
        return false;
    }

    const int32 SizeX = MaxX - MinX + 1;
    const int32 SizeY = MaxY - MinY + 1;
    TArray<uint16> Heights;
    Heights.SetNumZeroed(SizeX * SizeY);
    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo, false);
    LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, Heights.GetData(), 0);

    TArray<uint8> Bytes;
    Bytes.SetNumUninitialized(Heights.Num() * sizeof(uint16));
    FMemory::Memcpy(Bytes.GetData(), Heights.GetData(), Bytes.Num());
    if (!FFileHelper::SaveArrayToFile(Bytes, *AbsolutePath))
    {
        OutMessage = TEXT("Failed to write heightmap file");
        OutErrorCode = TEXT("WRITE_FAILED");
        return false;
    }

    Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
    Resp->SetStringField(TEXT("outputPath"), SafePath);
    Resp->SetNumberField(TEXT("width"), SizeX);
    Resp->SetNumberField(TEXT("height"), SizeY);
    Resp->SetNumberField(TEXT("sampleCount"), Heights.Num());
    McpHandlerUtils::AddVerification(Resp, Landscape);
    OutMessage = TEXT("Landscape heightmap exported");
    return true;
}
bool McpImportLandscapeHeightmap(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                        FString &OutMessage, FString &OutErrorCode)
{
    ALandscape *Landscape = McpFindLandscape(Payload);
    if (!Landscape)
    {
        OutMessage = TEXT("Landscape not found for import_heightmap");
        OutErrorCode = TEXT("LANDSCAPE_NOT_FOUND");
        return false;
    }

    FString HeightmapPath;
    if (!Payload->TryGetStringField(TEXT("heightmapPath"), HeightmapPath) || HeightmapPath.IsEmpty())
    {
        OutMessage = TEXT("heightmapPath required for import_heightmap when heightData is not provided");
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }

    FString AbsolutePath;
    FString SafePath;
    FString PathError;
    if (!McpResolveProjectFilePath(HeightmapPath, AbsolutePath, SafePath, PathError))
    {
        OutMessage = PathError;
        OutErrorCode = TEXT("SECURITY_VIOLATION");
        return false;
    }

    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *AbsolutePath) || Bytes.Num() < 2 || Bytes.Num() % 2 != 0)
    {
        OutMessage = TEXT("heightmapPath must point to a readable raw 16-bit heightmap file");
        OutErrorCode = TEXT("LOAD_FAILED");
        return false;
    }

    ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
    int32 FullMinX = 0, FullMinY = 0, FullMaxX = 0, FullMaxY = 0;
    if (!McpGetLandscapeExtentForEnvironmentAction(Landscape, FullMinX, FullMinY, FullMaxX, FullMaxY))
    {
        OutMessage = TEXT("Failed to read landscape extent");
        OutErrorCode = TEXT("INVALID_LANDSCAPE");
        return false;
    }

    int32 MinX = FullMinX;
    int32 MinY = FullMinY;
    int32 MaxX = FullMaxX;
    int32 MaxY = FullMaxY;
    McpApplyHeightmapRegionFromPayload(Payload, FullMinX, FullMinY, FullMaxX, FullMaxY, MinX, MinY, MaxX, MaxY);

    if (MinX > MaxX || MinY > MaxY)
    {
        OutMessage = FString::Printf(TEXT("Invalid heightmap import region: min (%d, %d) must not exceed max (%d, %d)"),
                                     MinX, MinY, MaxX, MaxY);
        OutErrorCode = TEXT("INVALID_REGION");
        return false;
    }

    const int32 SizeX = MaxX - MinX + 1;
    const int32 SizeY = MaxY - MinY + 1;
    const int64 SampleCount64 = static_cast<int64>(SizeX) * static_cast<int64>(SizeY);
    if (SampleCount64 <= 0 || SampleCount64 > MAX_int32)
    {
        OutMessage = TEXT("Invalid heightmap import region size");
        OutErrorCode = TEXT("INVALID_REGION");
        return false;
    }

    const int32 SampleCount = static_cast<int32>(SampleCount64);
    if (Bytes.Num() / sizeof(uint16) < SampleCount)
    {
        OutMessage = FString::Printf(TEXT("Heightmap file has %d samples but selected landscape region needs %d"),
                                     Bytes.Num() / static_cast<int32>(sizeof(uint16)), SampleCount);
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }

    TArray<uint16> Heights;
    Heights.SetNumUninitialized(SampleCount);
    FMemory::Memcpy(Heights.GetData(), Bytes.GetData(), SampleCount * sizeof(uint16));

    bool bUpdateNormals = false;
    Payload->TryGetBoolField(TEXT("updateNormals"), bUpdateNormals);
    bool bSkipFlush = false;
    Payload->TryGetBoolField(TEXT("skipFlush"), bSkipFlush);

    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo, false);
    LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, Heights.GetData(), SizeX, bUpdateNormals);
    if (!bSkipFlush)
    {
        LandscapeEdit.Flush();
    }
    Landscape->MarkPackageDirty();

    Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
    Resp->SetStringField(TEXT("heightmapPath"), SafePath);
    Resp->SetNumberField(TEXT("width"), SizeX);
    Resp->SetNumberField(TEXT("height"), SizeY);
    Resp->SetNumberField(TEXT("sampleCount"), SampleCount);
    Resp->SetBoolField(TEXT("flushSkipped"), bSkipFlush);
    McpHandlerUtils::AddVerification(Resp, Landscape);
    OutMessage = TEXT("Landscape heightmap imported");
    return true;
}

} // namespace McpEnvironmentHandlers
#endif
