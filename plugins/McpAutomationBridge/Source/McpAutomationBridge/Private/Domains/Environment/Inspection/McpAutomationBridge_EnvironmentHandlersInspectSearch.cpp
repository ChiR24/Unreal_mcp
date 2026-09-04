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
            // Honors `filter` (label/name/class substring), `limit` (default 100)
            // and `offset`, and lists the PIE world while a session runs so the
            // numbers match control_actor.list. `actors` mirrors `objects`
            // because the list_objects contract declares the former.
            FString Filter;
            Payload->TryGetStringField(TEXT("filter"), Filter);
            const int32 Limit = FMath::Max(1, McpHandlerUtils::GetOptionalInt(Payload, TEXT("limit"), 100));
            const int32 Offset = FMath::Max(0, McpHandlerUtils::GetOptionalInt(Payload, TEXT("offset"), 0));
            UWorld* World = McpGetRuntimeInspectionWorld();
            TArray<TSharedPtr<FJsonValue>> ObjectsArray;
            int32 TotalCount = 0;
            int32 MatchedCount = 0;
            if (World)
            {
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    AActor* Actor = *It;
                    if (!Actor)
                    {
                        continue;
                    }
                    ++TotalCount;
                    const FString ClassName = Actor->GetClass()->GetName();
                    if (!Filter.IsEmpty() &&
                        !Actor->GetActorLabel().Contains(Filter, ESearchCase::IgnoreCase) &&
                        !Actor->GetName().Contains(Filter, ESearchCase::IgnoreCase) &&
                        !ClassName.Contains(Filter, ESearchCase::IgnoreCase))
                    {
                        continue;
                    }
                    ++MatchedCount;
                    if (MatchedCount <= Offset || ObjectsArray.Num() >= Limit)
                    {
                        continue;
                    }
                    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
                    Obj->SetStringField(TEXT("name"), Actor->GetName());
                    Obj->SetStringField(TEXT("label"), Actor->GetActorLabel());
                    Obj->SetStringField(TEXT("path"), Actor->GetPathName());
                    Obj->SetStringField(TEXT("class"), ClassName);
                    ObjectsArray.Add(MakeShared<FJsonValueObject>(Obj));
                }
            }
            Resp->SetArrayField(TEXT("objects"), ObjectsArray);
            Resp->SetArrayField(TEXT("actors"), ObjectsArray);
            Resp->SetNumberField(TEXT("count"), ObjectsArray.Num());
            Resp->SetNumberField(TEXT("matchedCount"), MatchedCount);
            Resp->SetNumberField(TEXT("totalCount"), TotalCount);
            Resp->SetNumberField(TEXT("limit"), Limit);
            Resp->SetNumberField(TEXT("offset"), Offset);
            Resp->SetBoolField(TEXT("hasMore"), MatchedCount > Offset + ObjectsArray.Num());
            Resp->SetBoolField(TEXT("isPieWorld"), World != nullptr && World->WorldType == EWorldType::PIE);
            Resp->SetStringField(TEXT("worldName"), World ? World->GetName() : TEXT(""));
            if (!Filter.IsEmpty())
            {
                Resp->SetStringField(TEXT("filter"), Filter);
            }
            Resp->SetBoolField(TEXT("success"), true);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Objects listed"), Resp, FString());
            return true;
        }
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
                        Obj->SetStringField(TEXT("label"), Actor->GetActorLabel());
                        Obj->SetStringField(TEXT("name"), Actor->GetName());
                        Obj->SetStringField(TEXT("path"), Actor->GetPathName());
                        Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
                        ObjectsArray.Add(MakeShared<FJsonValueObject>(Obj));
                    }
                }
            }
            Resp->SetArrayField(TEXT("objects"), ObjectsArray);
            // The find_by_tag contract declares `actors`; both names carry the
            // same [{label, name, path, class}] entries.
            Resp->SetArrayField(TEXT("actors"), ObjectsArray);
            Resp->SetStringField(TEXT("tag"), Tag);
            Resp->SetNumberField(TEXT("count"), ObjectsArray.Num());
            Resp->SetBoolField(TEXT("success"), true);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Objects found by tag"), Resp, FString());
            return true;
        }
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
                    // FindFirstObject is UE 5.1+; pre-5.1 falls back to ResolveClassByName
                    // (ANY_PACKAGE-era lookup) — same guard as MontageNotifyBlend.cpp.
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
                    TargetClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None);
#else
                    TargetClass = ResolveClassByName(ClassName);
#endif
                    if (!TargetClass && ClassName.Len() >= 2)
                    {
                        const TCHAR Prefix = ClassName[0];
                        if ((Prefix == TEXT('A') || Prefix == TEXT('U')) && FChar::IsUpper(ClassName[1]))
                        {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
                            TargetClass = FindFirstObject<UClass>(*ClassName.Mid(1), EFindFirstObjectOptions::None);
#else
                            TargetClass = ResolveClassByName(ClassName.Mid(1));
#endif
                        }
                    }
                }
                if (TargetClass)
                {
                    // className/classPath/parentClass plus module, flags,
                    // properties[] and the CDO defaultProperties — see
                    // McpAutomationBridge_EnvironmentHandlersInspectClass.cpp.
                    McpDescribeClass(TargetClass, Resp);
                    Resp->SetBoolField(TEXT("success"), true);
                    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                           TEXT("Class inspected"), Resp, FString());
                }
                else
                {
                    Bridge.SendAutomationError(RequestingSocket, RequestId,
                                        FString::Printf(TEXT("Class not found: %s. Give a full /Script path (e.g. /Script/Engine.PointLight), a loaded short name, or an A/U prefixed name."), *ClassName),
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
