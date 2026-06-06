#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

static bool McpResolveSnapshotPath(const FString &Path, FString &OutSafePath,
                                   FString &OutAbsolutePath, FString &Message,
                                   FString &ErrorCode)
{
    OutSafePath = SanitizeProjectFilePath(Path);
    if (OutSafePath.IsEmpty())
    {
        Message = FString::Printf(
            TEXT("Invalid or unsafe path: %s. Path must be relative to project (e.g., /Temp/snapshot.json)"),
            *Path);
        ErrorCode = TEXT("SECURITY_VIOLATION");
        return false;
    }

    OutAbsolutePath = FPaths::ProjectDir() / OutSafePath;
    FPaths::MakeStandardFilename(OutAbsolutePath);
    OutAbsolutePath = FPaths::ConvertRelativePathToFull(OutAbsolutePath);
    FPaths::NormalizeFilename(OutAbsolutePath);

    FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(NormalizedProjectDir);
    if (!NormalizedProjectDir.EndsWith(TEXT("/")))
    {
        NormalizedProjectDir += TEXT("/");
    }

    if (!OutAbsolutePath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase))
    {
        Message = FString::Printf(TEXT("Invalid or unsafe path: %s. Path escapes project directory."), *Path);
        ErrorCode = TEXT("SECURITY_VIOLATION");
        return false;
    }
    if (!McpValidateProjectSnapshotFilePath(OutAbsolutePath, Message))
    {
        ErrorCode = TEXT("SECURITY_VIOLATION");
        return false;
    }
    return true;
}

bool HandleBuildSnapshotAndDeletionAction(const FString &LowerSub, FEnvironmentBuildContext &Context)
{
    const TSharedPtr<FJsonObject> &Payload = Context.Payload;
    TSharedPtr<FJsonObject> &Resp = Context.Resp;
    bool &bSuccess = Context.bSuccess;
    FString &Message = Context.Message;
    FString &ErrorCode = Context.ErrorCode;

if (LowerSub == TEXT("export_snapshot"))
    {
        FString Path;
        Payload->TryGetStringField(TEXT("path"), Path);

        if (Path.IsEmpty())
        {
            bSuccess = false;
            Message = TEXT("path required for export_snapshot");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            FString SafePath;
            FString AbsolutePath;
            if (!McpResolveSnapshotPath(Path, SafePath, AbsolutePath, Message, ErrorCode))
            {
                bSuccess = false;
                Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
                TSharedPtr<FJsonObject> Snapshot = McpHandlerUtils::CreateResultObject();
                Snapshot->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToString());
                Snapshot->SetStringField(TEXT("type"), TEXT("environment_snapshot"));

                FString JsonString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
                if (FJsonSerializer::Serialize(Snapshot.ToSharedRef(), Writer))
                {
                    if (FFileHelper::SaveStringToFile(JsonString, *AbsolutePath))
                    {
                        Resp->SetStringField(TEXT("exportPath"), SafePath);
                        Resp->SetStringField(TEXT("message"), TEXT("Snapshot exported"));
                    }
                    else
                    {
                        bSuccess = false;
                        Message = TEXT("Failed to write snapshot file");
                        ErrorCode = TEXT("WRITE_FAILED");
                        Resp->SetStringField(TEXT("error"), Message);
                    }
                }
                else
                {
                    bSuccess = false;
                    Message = TEXT("Failed to serialize snapshot");
                    ErrorCode = TEXT("SERIALIZE_FAILED");
                    Resp->SetStringField(TEXT("error"), Message);
                }
            }
        }
    }
    // -------------------------------------------------------------------------
    // import_snapshot: Import environment snapshot from JSON file
    // -------------------------------------------------------------------------
    else if (LowerSub == TEXT("import_snapshot"))
    {
        FString Path;
        Payload->TryGetStringField(TEXT("path"), Path);

        if (Path.IsEmpty())
        {
            bSuccess = false;
            Message = TEXT("path required for import_snapshot");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            FString SafePath;
            FString AbsolutePath;
            if (!McpResolveSnapshotPath(Path, SafePath, AbsolutePath, Message, ErrorCode))
            {
                bSuccess = false;
                Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
                FString JsonString;
                if (!FFileHelper::LoadFileToString(JsonString, *AbsolutePath))
                {
                    bSuccess = false;
                    Message = TEXT("Failed to read snapshot file");
                    ErrorCode = TEXT("LOAD_FAILED");
                    Resp->SetStringField(TEXT("error"), Message);
                }
                else
                {
                    TSharedPtr<FJsonObject> SnapshotObj;
                    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
                    if (!FJsonSerializer::Deserialize(Reader, SnapshotObj) || !SnapshotObj.IsValid())
                    {
                        bSuccess = false;
                        Message = TEXT("Failed to parse snapshot");
                        ErrorCode = TEXT("PARSE_FAILED");
                        Resp->SetStringField(TEXT("error"), Message);
                    }
                    else
                    {
                        Resp->SetObjectField(TEXT("snapshot"), SnapshotObj.ToSharedRef());
                        Resp->SetStringField(TEXT("message"), TEXT("Snapshot imported"));
                    }
                }
            }
        }
    }
    // -------------------------------------------------------------------------
    // delete: Delete environment actors by name
    // -------------------------------------------------------------------------
    else if (LowerSub == TEXT("delete"))
    {
        const TArray<TSharedPtr<FJsonValue>> *NamesArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("names"), NamesArray) || !NamesArray)
        {
            bSuccess = false;
            Message = TEXT("names array required for delete");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else if (!GEditor)
        {
            bSuccess = false;
            Message = TEXT("Editor not available");
            ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
            if (!ActorSS)
            {
                bSuccess = false;
                Message = TEXT("EditorActorSubsystem not available");
                ErrorCode = TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING");
                Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
                TArray<FString> Deleted;
                TArray<FString> Missing;

                for (const TSharedPtr<FJsonValue> &Val : *NamesArray)
                {
                    if (Val.IsValid() && Val->Type == EJson::String)
                    {
                        FString Name = Val->AsString();
                        TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
                        bool bRemoved = false;

                        for (AActor *A : AllActors)
                        {
                            if (A && A->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase))
                            {
                                if (ActorSS->DestroyActor(A))
                                {
                                    Deleted.Add(Name);
                                    bRemoved = true;
                                }
                                break;
                            }
                        }

                        if (!bRemoved)
                        {
                            Missing.Add(Name);
                        }
                    }
                }

                // Build response arrays
                TArray<TSharedPtr<FJsonValue>> DeletedArray;
                for (const FString &Name : Deleted)
                {
                    DeletedArray.Add(MakeShared<FJsonValueString>(Name));
                }
                Resp->SetArrayField(TEXT("deleted"), DeletedArray);
                Resp->SetNumberField(TEXT("deletedCount"), Deleted.Num());

                if (Missing.Num() > 0)
                {
                    TArray<TSharedPtr<FJsonValue>> MissingArray;
                    for (const FString &Name : Missing)
                    {
                        MissingArray.Add(MakeShared<FJsonValueString>(Name));
                    }
                    Resp->SetArrayField(TEXT("missing"), MissingArray);
                    bSuccess = false;
                    Message = TEXT("Some environment actors could not be removed");
                    ErrorCode = TEXT("DELETE_PARTIAL");
                    Resp->SetStringField(TEXT("error"), Message);
                }
                else
                {
                    Message = TEXT("Environment actors deleted");
                }
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}

} // namespace McpEnvironmentHandlers
#endif
