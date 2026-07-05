#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

bool HandleInspectSearchAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &SubAction, const FString &LowerSubAction,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> Resp)
{
        if (LowerSubAction.Equals(TEXT("list_objects")))
        {
            TArray<TSharedPtr<FJsonValue>> ObjectsArray;
            if (GEditor && GEditor->GetEditorWorldContext().World())
            {
                UWorld* World = GEditor->GetEditorWorldContext().World();
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    AActor* Actor = *It;
                    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
                    Obj->SetStringField(TEXT("name"), Actor->GetName());
                    Obj->SetStringField(TEXT("path"), Actor->GetPathName());
                    Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
                    ObjectsArray.Add(MakeShared<FJsonValueObject>(Obj));
                }
            }
            Resp->SetArrayField(TEXT("objects"), ObjectsArray);
            Resp->SetNumberField(TEXT("count"), ObjectsArray.Num());
            Resp->SetBoolField(TEXT("success"), true);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Objects listed"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // find_by_class
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("find_by_class")))
        {
            FString ClassName;
            Payload->TryGetStringField(TEXT("className"), ClassName);
            if (ClassName.IsEmpty())
            {
                Payload->TryGetStringField(TEXT("classPath"), ClassName);
            }
            TArray<TSharedPtr<FJsonValue>> ObjectsArray;

            if (GEditor && GEditor->GetEditorWorldContext().World() && !ClassName.IsEmpty())
            {
                UWorld* World = GEditor->GetEditorWorldContext().World();
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    AActor* Actor = *It;
                    if (Actor->GetClass()->GetName().Equals(ClassName, ESearchCase::IgnoreCase) ||
                        Actor->GetClass()->GetPathName().Contains(ClassName))
                    {
                        TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
                        Obj->SetStringField(TEXT("name"), Actor->GetName());
                        Obj->SetStringField(TEXT("path"), Actor->GetPathName());
                        Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
                        ObjectsArray.Add(MakeShared<FJsonValueObject>(Obj));
                    }
                }
            }
            Resp->SetArrayField(TEXT("objects"), ObjectsArray);
            Resp->SetNumberField(TEXT("count"), ObjectsArray.Num());
            Resp->SetBoolField(TEXT("success"), true);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Objects found by class"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // find_by_tag
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("find_by_tag")))
        {
            FString Tag;
            Payload->TryGetStringField(TEXT("tag"), Tag);
            TArray<TSharedPtr<FJsonValue>> ObjectsArray;

            if (GEditor && GEditor->GetEditorWorldContext().World() && !Tag.IsEmpty())
            {
                UWorld* World = GEditor->GetEditorWorldContext().World();
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    AActor* Actor = *It;
                    if (Actor->ActorHasTag(FName(*Tag)))
                    {
                        TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
                        Obj->SetStringField(TEXT("name"), Actor->GetName());
                        Obj->SetStringField(TEXT("path"), Actor->GetPathName());
                        Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
                        ObjectsArray.Add(MakeShared<FJsonValueObject>(Obj));
                    }
                }
            }
            Resp->SetArrayField(TEXT("objects"), ObjectsArray);
            Resp->SetNumberField(TEXT("count"), ObjectsArray.Num());
            Resp->SetBoolField(TEXT("success"), true);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Objects found by tag"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // inspect_class
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("inspect_class")))
        {
            FString ClassName;
            Payload->TryGetStringField(TEXT("className"), ClassName);
            if (ClassName.IsEmpty())
            {
                Payload->TryGetStringField(TEXT("classPath"), ClassName);
            }
            if (!ClassName.IsEmpty())
            {
                // Try to find the class
                UClass* TargetClass = FindObject<UClass>(nullptr, *ClassName);
                if (!TargetClass && !ClassName.Contains(TEXT(".")))
                {
                    // Try with /Script/Engine prefix for common classes
                    TargetClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassName));
                }
                if (!TargetClass && !ClassName.Contains(TEXT(".")) && !ClassName.Contains(TEXT("/")))
                {
                    // Bare short name: UE5 removed the ANY_PACKAGE lookup, so FindObject with a null outer
                    // resolves only full /Script/ paths, and the /Script/Engine fallback above only covers
                    // engine classes — every game-module class (e.g. "TDMCharacter") came back
                    // CLASS_NOT_FOUND. Scan all loaded classes by object name instead, and tolerate the
                    // conventional A/U code prefix (reflected class object names carry no prefix:
                    // ATDMCharacter's UClass is named "TDMCharacter"). If two loaded classes share the
                    // short name, first-found wins (best-effort read-only fallback; the full
                    // /Script/<Module>.<Class> path stays the deterministic route).
                    TargetClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None);
                    if (!TargetClass && ClassName.Len() >= 2)
                    {
                        const TCHAR Prefix = ClassName[0];
                        if ((Prefix == TEXT('A') || Prefix == TEXT('U')) && FChar::IsUpper(ClassName[1]))
                        {
                            TargetClass = FindFirstObject<UClass>(*ClassName.Mid(1), EFindFirstObjectOptions::None);
                        }
                    }
                }
                if (TargetClass)
                {
                    Resp->SetStringField(TEXT("className"), TargetClass->GetName());
                    Resp->SetStringField(TEXT("classPath"), TargetClass->GetPathName());
                    Resp->SetStringField(TEXT("parentClass"), TargetClass->GetSuperClass() ? TargetClass->GetSuperClass()->GetName() : TEXT("None"));
                    Resp->SetBoolField(TEXT("success"), true);
                    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                           TEXT("Class inspected"), Resp, FString());
                }
                else
                {
                    Bridge.SendAutomationError(RequestingSocket, RequestId,
                                        FString::Printf(TEXT("Class not found: %s"), *ClassName),
                                        TEXT("CLASS_NOT_FOUND"));
                }
            }
            else
            {
                Bridge.SendAutomationError(RequestingSocket, RequestId,
                                    TEXT("className is required for inspect_class"),
                                    TEXT("INVALID_ARGUMENT"));
            }
            return true;
        }
    else
    {
        return false;
    }

    return true;
}

} // namespace McpEnvironmentHandlers
#endif
