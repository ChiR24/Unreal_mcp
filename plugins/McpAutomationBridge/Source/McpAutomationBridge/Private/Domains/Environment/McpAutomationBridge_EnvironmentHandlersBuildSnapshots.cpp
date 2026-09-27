#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"
#include "Domains/Environment/McpAutomationBridge_EnvironmentSnapshotPaths.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/DateTime.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace McpEnvironmentHandlers {
namespace {

constexpr int64 MaxSnapshotBytes = 1024 * 1024;

bool ExportSnapshot(FEnvironmentBuildContext &Context, const FString &AbsolutePath,
    const FString &RelativePath)
{
    TSharedPtr<FJsonObject> Snapshot = McpHandlerUtils::CreateResultObject();
    FString DirectionalLightActorPath;
    FString SkyLightActorPath;
    Context.Payload->TryGetStringField(
        TEXT("directionalLightActorPath"), DirectionalLightActorPath);
    Context.Payload->TryGetStringField(TEXT("skyLightActorPath"), SkyLightActorPath);
    FString Message;
    FString ErrorCode;
    const bool bResult = McpCaptureEnvironmentSnapshot(
        Snapshot, Context.Resp, DirectionalLightActorPath, SkyLightActorPath,
        Message, ErrorCode);
    if (!bResult)
    {
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    Snapshot->SetStringField(TEXT("generatedAt"), FDateTime::UtcNow().ToIso8601());

    FString JsonText;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
    if (!FJsonSerializer::Serialize(Snapshot.ToSharedRef(), Writer))
    {
        MarkActorConfigurationResult(Context, false, TEXT("Failed to serialize environment snapshot"),
            TEXT("SNAPSHOT_SERIALIZE_FAILED"));
        return true;
    }
    const FTCHARToUTF8 JsonUtf8(*JsonText);
    if (JsonUtf8.Length() > MaxSnapshotBytes)
    {
        MarkActorConfigurationResult(Context, false, TEXT("Environment snapshot exceeds size limit"),
            TEXT("SNAPSHOT_TOO_LARGE"));
        return true;
    }

    const FString Directory = FPaths::GetPath(AbsolutePath);
    if (!IFileManager::Get().MakeDirectory(*Directory, true))
    {
        MarkActorConfigurationResult(Context, false, TEXT("Failed to create snapshot directory"),
            TEXT("SNAPSHOT_WRITE_FAILED"));
        return true;
    }
    FString SecurityError;
    if (!::McpValidateProjectSnapshotFilePath(AbsolutePath, SecurityError))
    {
        MarkActorConfigurationResult(Context, false, SecurityError, TEXT("SECURITY_VIOLATION"));
        return true;
    }
    if (!FFileHelper::SaveStringToFile(JsonText, *AbsolutePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        MarkActorConfigurationResult(Context, false, TEXT("Failed to write environment snapshot"),
            TEXT("SNAPSHOT_WRITE_FAILED"));
        return true;
    }

    Context.Resp->SetStringField(TEXT("path"), RelativePath);
    Context.Resp->SetNumberField(TEXT("bytesWritten"), JsonUtf8.Length());
    MarkActorConfigurationResult(Context, true,
        FString::Printf(TEXT("Environment snapshot exported to %s"), *RelativePath), FString());
    return true;
}

bool ImportSnapshot(
    FEnvironmentBuildContext &Context, const FString &AbsolutePath,
    const FString &RelativePath)
{
    IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    TUniquePtr<IFileHandle> ReadHandle(PlatformFile.OpenRead(*AbsolutePath));
    if (!ReadHandle.IsValid())
    {
        if (PlatformFile.FileExists(*AbsolutePath))
        {
            MarkActorConfigurationResult(Context, false,
                TEXT("Failed to open environment snapshot"), TEXT("SNAPSHOT_READ_FAILED"));
            return true;
        }
        Context.Resp->SetStringField(TEXT("path"), RelativePath);
        Context.Resp->SetBoolField(TEXT("noOp"), true);
        MarkActorConfigurationResult(Context, true,
            FString::Printf(
                TEXT("Environment snapshot file not found at %s; import treated as no-op"),
                *RelativePath),
            FString());
        return true;
    }
    const int64 FileSize = ReadHandle->Size();
    if (FileSize < 0 || FileSize > MaxSnapshotBytes)
    {
        MarkActorConfigurationResult(Context, false,
            FileSize < 0 ? TEXT("Failed to determine environment snapshot size")
                         : TEXT("Environment snapshot exceeds size limit"),
            FileSize < 0 ? TEXT("SNAPSHOT_READ_FAILED") : TEXT("SNAPSHOT_TOO_LARGE"));
        return true;
    }

    TArray<uint8> Contents;
    Contents.SetNumUninitialized(static_cast<int32>(FileSize));
    if (FileSize > 0 && !ReadHandle->Read(Contents.GetData(), FileSize))
    {
        MarkActorConfigurationResult(Context, false,
            TEXT("Failed to read bounded environment snapshot"), TEXT("SNAPSHOT_READ_FAILED"));
        return true;
    }
    FString JsonText;
    if (!Contents.IsEmpty())
    {
        const FUTF8ToTCHAR Converter(
            reinterpret_cast<const ANSICHAR *>(Contents.GetData()), Contents.Num());
        JsonText = FString(Converter.Length(), Converter.Get());
    }
    TSharedPtr<FJsonObject> Snapshot;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Snapshot) || !Snapshot.IsValid())
    {
        MarkActorConfigurationResult(Context, false, TEXT("Environment snapshot is not a JSON object"),
            TEXT("INVALID_SNAPSHOT"));
        return true;
    }

    FString Message;
    FString ErrorCode;
    const bool bResult = McpApplyEnvironmentSnapshot(Snapshot, Context.Resp, Message, ErrorCode);
    Context.Resp->SetStringField(TEXT("path"), RelativePath);
    Context.Resp->SetNumberField(TEXT("bytesRead"), FileSize);
    MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
    return true;
}

}

bool HandleBuildSnapshotAction(const FString &LowerSub, FEnvironmentBuildContext &Context)
{
    FString AbsolutePath;
    FString RelativePath;
    FString PathError;
    if (!McpResolveEnvironmentSnapshotPath(
            Context.Payload, AbsolutePath, RelativePath, PathError))
    {
        MarkActorConfigurationResult(Context, false, PathError,
            PathError.StartsWith(TEXT("SECURITY_VIOLATION"))
                ? TEXT("SECURITY_VIOLATION")
                : TEXT("INVALID_ARGUMENT"));
        return true;
    }
    return LowerSub == TEXT("export_snapshot")
        ? ExportSnapshot(Context, AbsolutePath, RelativePath)
        : ImportSnapshot(Context, AbsolutePath, RelativePath);
}

}
