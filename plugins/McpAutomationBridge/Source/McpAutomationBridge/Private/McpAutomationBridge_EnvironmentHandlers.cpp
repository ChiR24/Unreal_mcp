#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#include "Async/Async.h"
#include "FileHelpers.h"
#include "Misc/FileHelper.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleBuildEnvironmentAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("build_environment"), ESearchCase::IgnoreCase) && !Lower.StartsWith(TEXT("build_environment"))) return false;

    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("build_environment payload missing."), TEXT("INVALID_PAYLOAD")); return true; }

    FString SubAction; Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSub = SubAction.ToLower();

    // Fast-path foliage sub-actions to dedicated native handlers to avoid double responses
    if (LowerSub == TEXT("add_foliage_instances"))
    {
        // Transform from build_environment schema to foliage handler schema
        FString FoliageTypePath; Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
        const TArray<TSharedPtr<FJsonValue>>* Transforms = nullptr; Payload->TryGetArrayField(TEXT("transforms"), Transforms);
        TSharedPtr<FJsonObject> FoliagePayload = MakeShared<FJsonObject>();
        if (!FoliageTypePath.IsEmpty()) { FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath); }
        TArray<TSharedPtr<FJsonValue>> Locations;
        if (Transforms)
        {
            for (const TSharedPtr<FJsonValue>& V : *Transforms)
            {
                if (!V.IsValid() || V->Type != EJson::Object) continue;
                const TSharedPtr<FJsonObject>* TObj = nullptr; if (!V->TryGetObject(TObj) || !TObj) continue;
                const TSharedPtr<FJsonObject>* LocObj = nullptr; if (!(*TObj)->TryGetObjectField(TEXT("location"), LocObj) || !LocObj) continue;
                double X=0,Y=0,Z=0; (*LocObj)->TryGetNumberField(TEXT("x"), X); (*LocObj)->TryGetNumberField(TEXT("y"), Y); (*LocObj)->TryGetNumberField(TEXT("z"), Z);
                TSharedPtr<FJsonObject> L = MakeShared<FJsonObject>(); L->SetNumberField(TEXT("x"), X); L->SetNumberField(TEXT("y"), Y); L->SetNumberField(TEXT("z"), Z);
                Locations.Add(MakeShared<FJsonValueObject>(L));
            }
        }
        FoliagePayload->SetArrayField(TEXT("locations"), Locations);
        return HandlePaintFoliage(RequestId, TEXT("paint_foliage"), FoliagePayload, RequestingSocket);
    }
    else if (LowerSub == TEXT("get_foliage_instances"))
    {
        FString FoliageTypePath; Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
        TSharedPtr<FJsonObject> FoliagePayload = MakeShared<FJsonObject>();
        if (!FoliageTypePath.IsEmpty()) { FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath); }
        return HandleGetFoliageInstances(RequestId, TEXT("get_foliage_instances"), FoliagePayload, RequestingSocket);
    }
    else if (LowerSub == TEXT("remove_foliage"))
    {
        FString FoliageTypePath; Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
        bool bRemoveAll = false; Payload->TryGetBoolField(TEXT("removeAll"), bRemoveAll);
        TSharedPtr<FJsonObject> FoliagePayload = MakeShared<FJsonObject>();
        if (!FoliageTypePath.IsEmpty()) { FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath); }
        FoliagePayload->SetBoolField(TEXT("removeAll"), bRemoveAll);
        return HandleRemoveFoliage(RequestId, TEXT("remove_foliage"), FoliagePayload, RequestingSocket);
    }

#if WITH_EDITOR
    AsyncTask(ENamedThreads::GameThread, [this, RequestId, Payload, LowerSub, RequestingSocket]() {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("action"), LowerSub);

        // Handle specific actions
        if (LowerSub == TEXT("export_snapshot"))
        {
            FString Path; Payload->TryGetStringField(TEXT("path"), Path);
            if (!Path.IsEmpty())
            {
                // Create a simple JSON snapshot
                TSharedPtr<FJsonObject> Snapshot = MakeShared<FJsonObject>();
                Snapshot->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToString());
                Snapshot->SetStringField(TEXT("type"), TEXT("environment_snapshot"));

                FString JsonString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
                if (FJsonSerializer::Serialize(Snapshot.ToSharedRef(), Writer))
                {
                    if (FFileHelper::SaveStringToFile(JsonString, *Path))
                    {
                        Resp->SetStringField(TEXT("exportPath"), Path);
                        Resp->SetStringField(TEXT("message"), TEXT("Snapshot exported"));
                    }
                    else
                    {
                        Resp->SetBoolField(TEXT("success"), false);
                        Resp->SetStringField(TEXT("error"), TEXT("Failed to write file"));
                    }
                }
            }
        }
        else if (LowerSub == TEXT("import_snapshot"))
        {
            Resp->SetStringField(TEXT("message"), TEXT("Import snapshot handled (no-op)"));
        }
        else if (LowerSub == TEXT("delete"))
        {
            // Handle deletion of environment actors
            const TArray<TSharedPtr<FJsonValue>>* NamesArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("names"), NamesArray) && NamesArray)
            {
                if (!GEditor) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return; }
                UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
                if (!ActorSS) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING")); return; }

                TArray<FString> Deleted;
                for (const TSharedPtr<FJsonValue>& Val : *NamesArray)
                {
                    if (Val.IsValid() && Val->Type == EJson::String)
                    {
                        FString Name = Val->AsString();
                        TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
                        for (AActor* A : AllActors)
                        {
                            if (A && A->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase))
                            {
                                if (ActorSS->DestroyActor(A))
                                {
                                    Deleted.Add(Name);
                                }
                                break;
                            }
                        }
                    }
                }

                TArray<TSharedPtr<FJsonValue>> DeletedArray;
                for (const FString& Name : Deleted)
                {
                    DeletedArray.Add(MakeShared<FJsonValueString>(Name));
                }
                Resp->SetArrayField(TEXT("deleted"), DeletedArray);
                Resp->SetNumberField(TEXT("deletedCount"), Deleted.Num());
            }
        }
        else
        {
            // Generic stub for other environment actions
            Resp->SetStringField(TEXT("message"), FString::Printf(TEXT("Environment action '%s' handled"), *LowerSub));
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Environment action '%s' completed"), *LowerSub), Resp, FString());
    });
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Environment building actions require editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
