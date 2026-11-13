#include "Domains/Inspect/McpInspectShared.h"

namespace McpInspectHandlers {

bool HandleInspectSearchAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const FString &SubAction, const FString &LowerSubAction,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> Resp)
{
        if (LowerSubAction.Equals(TEXT("list_objects")))
        {
            // Honors `filter` (label/name/class substring), `limit` (default 100)
            // and `offset`, and lists the PIE world while a session runs so the
            // numbers match control_actor.list, as the `actors` its contract declares.
            FString Filter;
            Payload->TryGetStringField(TEXT("filter"), Filter);
            const int32 Limit = FMath::Max(1, GetJsonIntField(Payload, TEXT("limit"), 100));
            const int32 Offset = FMath::Max(0, GetJsonIntField(Payload, TEXT("offset"), 0));
            UWorld* World = McpHandlerUtils::GetPlayOrEditorWorld();
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
            // No class used to skip the search and answer success with 0 objects.
            if (ClassName.IsEmpty())
            {
                Bridge.SendAutomationError(RequestingSocket, RequestId,
                                           TEXT("className or classPath is required"), TEXT("INVALID_ARGUMENT"));
                return true;
            }
            // The class itself, as control_actor's find takes it: matched as text, a mistyped class answered 0 found,
            // "Light" listed every actor whose class path held light, and a subclass was found only by that accident.
            UClass* Class = ResolveClassByName(ClassName);
            if (!Class || !Class->IsChildOf(AActor::StaticClass()))
            {
                Bridge.SendAutomationError(RequestingSocket, RequestId, Class
                    ? FString::Printf(TEXT("%s is not an Actor class; find_by_class lists actors."), *Class->GetPathName())
                    : McpClassNotFoundMessage(ClassName), Class ? TEXT("INVALID_ARGUMENT") : TEXT("CLASS_NOT_FOUND"));
                return true;
            }
            TArray<TSharedPtr<FJsonValue>> ObjectsArray;

            // The PIE world while a session runs, like list_objects: the editor
            // world has no pawn or GameMode, so a runtime class read as 0 found.
            UWorld* World = McpHandlerUtils::GetPlayOrEditorWorld();
            if (World)
            {
                for (TActorIterator<AActor> It(World, Class); It; ++It)
                {
                    AActor* Actor = *It;
                    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
                    Obj->SetStringField(TEXT("name"), Actor->GetName());
                    Obj->SetStringField(TEXT("path"), Actor->GetPathName());
                    Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
                    ObjectsArray.Add(MakeShared<FJsonValueObject>(Obj));
                }
            }
            McpCapFindRows(Payload, ObjectsArray, Resp);
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

            UWorld* World = McpHandlerUtils::GetPlayOrEditorWorld();
            if (World && !Tag.IsEmpty())
            {
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
            // [{label, name, path, class}], as the `actors` the find_by_tag contract declares.
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
#if ENGINE_MINOR_VERSION >= 1
                    TargetClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None);
#else
                    TargetClass = ResolveClassByName(ClassName);
#endif
                    if (!TargetClass && ClassName.Len() >= 2)
                    {
                        const TCHAR Prefix = ClassName[0];
                        if ((Prefix == TEXT('A') || Prefix == TEXT('U')) && FChar::IsUpper(ClassName[1]))
                        {
#if ENGINE_MINOR_VERSION >= 1
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
                    // McpInspectClass.cpp.
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
    return false;
}

} // namespace McpInspectHandlers
