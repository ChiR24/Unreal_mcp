// export_mesh: writes a static or skeletal mesh asset (or an animation, on its preview mesh) to a glTF, GLB, FBX or
// OBJ file through the editor's own exporters, so it can be handed to a DCC tool, a web viewer or a mesh checker.

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "AssetExportTask.h"
#include "CoreGlobals.h"
#include "Exporters/Exporter.h"
#include "HAL/FileManager.h"
#include "UObject/GCObjectScopeGuard.h"

// What the exporters log while they run (the glTF exporter's notes on materials it baked or could not convert)
// reaches the reply's warnings through the request's own log capture.
namespace McpMeshExport
{
namespace
{
constexpr int32 McpMaxListedFiles = 50;

// Each file in Folder with its last write time.
TMap<FString, FDateTime> McpFolderFiles(const FString& Folder)
{
    TArray<FString> Names;
    IFileManager::Get().FindFiles(Names, *(Folder / TEXT("*")), true, false);
    TMap<FString, FDateTime> Files;
    for (const FString& Name : Names)
    {
        Files.Add(Name, IFileManager::Get().GetTimeStamp(*(Folder / Name)));
    }
    return Files;
}
} // namespace

bool HandleExportMesh(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId,
                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    auto Fail = [&](const FString& Message, const TCHAR* Code, const TSharedPtr<FJsonObject>& Details = nullptr)
    {
        Bridge->SendAutomationResponse(Socket, RequestId, false, Message, Details, Code);
        return true;
    };

    FString AssetPath;
    FString Format;
    FString OutputPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    Payload->TryGetStringField(TEXT("format"), Format);
    Payload->TryGetStringField(TEXT("outputPath"), OutputPath);
    const FString SafePath = SanitizeProjectRelativePath(AssetPath);
    if (SafePath.IsEmpty())
    {
        return Fail(McpPathRefusalMessage(TEXT("assetPath"), AssetPath), TEXT("SECURITY_VIOLATION"));
    }
    UObject* Asset = McpLoadAsset(SafePath);
    if (!Asset)
    {
        return Fail(FString::Printf(TEXT("assetPath '%s' does not load."), *AssetPath), TEXT("NOT_FOUND"));
    }

    // The file's extension picks the exporter; an outputPath without one is a folder.
    const FString PathExtension = FPaths::GetExtension(OutputPath).ToLower();
    Format = Format.ToLower();
    if (Format.IsEmpty())
    {
        Format = PathExtension.IsEmpty() ? TEXT("glb") : PathExtension;
    }
    if (Format != TEXT("glb") && Format != TEXT("gltf") && Format != TEXT("fbx") && Format != TEXT("obj"))
    {
        return Fail(FString::Printf(TEXT("format '%s' is not glb, gltf, fbx or obj."), *Format), TEXT("INVALID_ARGUMENT"));
    }
    if (!PathExtension.IsEmpty() && PathExtension != Format)
    {
        return Fail(FString::Printf(TEXT("outputPath ends in .%s but format is %s."), *PathExtension, *Format),
                    TEXT("INVALID_ARGUMENT"));
    }
    const FString FileName = Asset->GetName() + TEXT(".") + Format;
    const FString Requested = OutputPath.IsEmpty() ? FString(TEXT("Saved/Exports")) / FileName
                            : PathExtension.IsEmpty() ? OutputPath / FileName
                                                      : OutputPath;
    FString File;
    FString PathError;
    if (!McpResolveProjectFilePath(Requested, File, PathError))
    {
        PathError.RemoveFromStart(TEXT("SECURITY_VIOLATION: "));
        return Fail(FString::Printf(TEXT("outputPath '%s' must stay inside the project: %s."), *Requested, *PathError),
                    TEXT("SECURITY_VIOLATION"));
    }
    const FString Folder = FPaths::GetPath(File);
    IFileManager::Get().MakeDirectory(*Folder, true);

    UAssetExportTask* Task = NewObject<UAssetExportTask>();
    FGCObjectScopeGuard TaskGuard(Task);
    Task->Object = Asset;
    Task->Filename = File;
    Task->bSelected = false;
    Task->bReplaceIdentical = true;
    Task->bPrompt = false;
    Task->bAutomated = true;

    const TMap<FString, FDateTime> Before = McpFolderFiles(Folder);
    bool bExported = false;
    {
        // The FBX and glTF exporters ask for options in a dialog, and the glTF one opens its message log after a
        // warning, unless the editor runs unattended: this call is unattended.
        TGuardValue<bool> Unattended(GIsAutomationTesting, true);
        bExported = UExporter::RunAssetExportTask(Task);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), Asset->GetPathName());
    Result->SetStringField(TEXT("format"), Format);
    Result->SetStringField(TEXT("path"), File);
    const int64 Bytes = IFileManager::Get().FileSize(*File);
    if (!bExported || Bytes <= 0)
    {
        const FString Why = Task->Errors.Num() > 0 ? TEXT(": ") + Task->Errors[0] : FString(TEXT("."));
        return Fail(FString::Printf(TEXT("%s was not exported as %s%s"), *Asset->GetName(), *Format, *Why),
                    TEXT("EXPORT_FAILED"), Result);
    }
    Result->SetNumberField(TEXT("bytes"), static_cast<double>(Bytes));

    // A .gltf keeps its buffers and textures in files beside it, and OBJ writes its UV and collision variants.
    TArray<TSharedPtr<FJsonValue>> Written;
    for (const TPair<FString, FDateTime>& Now : McpFolderFiles(Folder))
    {
        const FDateTime* Was = Before.Find(Now.Key);
        const FString Path = Folder / Now.Key;
        if (Path != File && (!Was || *Was != Now.Value) && Written.Num() < McpMaxListedFiles)
        {
            const TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("name"), Now.Key);
            Entry->SetNumberField(TEXT("bytes"), static_cast<double>(IFileManager::Get().FileSize(*Path)));
            Written.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }
    if (Written.Num() > 0)
    {
        Result->SetArrayField(TEXT("otherFiles"), Written);
    }
    McpHandlerUtils::MarkNoAssetsChanged(Result);
    Bridge->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Exported %s to %s (%lld bytes)."), *Asset->GetName(), *File, Bytes), Result);
    return true;
}
} // namespace McpMeshExport
