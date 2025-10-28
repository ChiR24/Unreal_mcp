#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#if WITH_EDITOR
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#if __has_include("Subsystems/UnrealEditorSubsystem.h")
#include "Subsystems/UnrealEditorSubsystem.h"
#elif __has_include("UnrealEditorSubsystem.h")
#include "UnrealEditorSubsystem.h"
#endif
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#elif __has_include("LevelEditorSubsystem.h")
#include "LevelEditorSubsystem.h"
#endif
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#if __has_include("EditorLoadingAndSavingUtils.h")
#include "EditorLoadingAndSavingUtils.h"
#elif __has_include("FileHelpers.h")
#include "FileHelpers.h"
#endif
#include "Misc/Base64.h"
#include "Factories/Factory.h"
#include "UObject/SoftObjectPath.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundCue.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleExecuteEditorFunction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    // Accept either the generic execute_editor_function action or the
    // more specific execute_console_command action. This allows the
    // server to use native console commands for health checks and diagnostics.
    if (!Lower.Equals(TEXT("execute_editor_function"), ESearchCase::IgnoreCase) && !Lower.Contains(TEXT("execute_editor_function"))
        && !Lower.Equals(TEXT("execute_console_command")) && !Lower.Contains(TEXT("execute_console_command"))) return false;

    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("execute_editor_function payload missing."), TEXT("INVALID_PAYLOAD")); return true; }

    // Handle native console command action first — console commands
    // carry a top-level `command` (or params.command) and should not
    // be treated as a generic execute_editor_function requiring a
    // functionName field.
    if (Lower.Equals(TEXT("execute_console_command")) || Lower.Contains(TEXT("execute_console_command")))
    {
        // Accept either a top-level 'command' string or nested params.command
        FString Cmd;
        if (!Payload->TryGetStringField(TEXT("command"), Cmd))
        {
            const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
            if (Payload->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr && (*ParamsPtr).IsValid())
            {
                (*ParamsPtr)->TryGetStringField(TEXT("command"), Cmd);
            }
        }
        if (Cmd.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("command required"), TEXT("INVALID_ARGUMENT")); return true; }

        if (!GEditor)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
            return true;
        }

        bool bExecCalled = false;
        bool bEditorOk = false;
        if (GEditor)
        {
            bEditorOk = GEditor->Exec(nullptr, *Cmd);
            bExecCalled = true;
        }

        if (!bEditorOk && GEngine)
        {
            for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
            {
                if (UWorld* World = Ctx.World())
                {
                    const bool bWorldOk = GEngine->Exec(World, *Cmd);
                    bExecCalled = bExecCalled || bWorldOk;
                    break;
                }
            }
        }

        const bool bOk = bExecCalled;
        TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
        Out->SetStringField(TEXT("command"), Cmd);
        Out->SetBoolField(TEXT("success"), bOk);
        SendAutomationResponse(RequestingSocket, RequestId, bOk, bOk ? TEXT("Command executed") : TEXT("Command not executed"), Out, bOk ? FString() : TEXT("EXEC_FAILED"));
        return true;
    }

    // For other execute_editor_function cases require functionName
    FString FunctionName; Payload->TryGetStringField(TEXT("functionName"), FunctionName);
    if (FunctionName.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("functionName required"), TEXT("INVALID_ARGUMENT")); return true; }

    const FString FN = FunctionName.ToUpper();
        // Accept either a top-level 'command' string or nested params.command
        FString Cmd;
        if (!Payload->TryGetStringField(TEXT("command"), Cmd))
        {
            const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
            if (Payload->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr && (*ParamsPtr).IsValid())
            {
                (*ParamsPtr)->TryGetStringField(TEXT("command"), Cmd);
            }
        }
        // (Console handling moved earlier)

#if WITH_EDITOR
    // Dispatch a handful of well-known functions to native handlers
    if (FN == TEXT("GET_ALL_ACTORS") || FN == TEXT("GET_ALL_ACTORS_SIMPLE"))
    {
        if (!GEditor)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
            return true;
        }
        UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
        if (!ActorSS)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
            return true;
        }
        TArray<AActor*> Actors = ActorSS->GetAllLevelActors();
        TArray<TSharedPtr<FJsonValue>> Arr; Arr.Reserve(Actors.Num());
        for (AActor* A : Actors)
        {
            if (!A) continue;
            TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
            E->SetStringField(TEXT("name"), A->GetName());
            E->SetStringField(TEXT("label"), A->GetActorLabel());
            E->SetStringField(TEXT("path"), A->GetPathName());
            E->SetStringField(TEXT("class"), A->GetClass() ? A->GetClass()->GetPathName() : TEXT(""));
            Arr.Add(MakeShared<FJsonValueObject>(E));
        }
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetArrayField(TEXT("actors"), Arr);
        Result->SetNumberField(TEXT("count"), Arr.Num());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor list"), Result, FString());
        return true;
    }

    if (FN == TEXT("SPAWN_ACTOR") || FN == TEXT("SPAWN_ACTOR_AT_LOCATION"))
    {
        FString ClassPath; Payload->TryGetStringField(TEXT("class_path"), ClassPath); if (ClassPath.IsEmpty()) Payload->TryGetStringField(TEXT("classPath"), ClassPath);
        const TSharedPtr<FJsonObject>* LocObj = nullptr; FVector Loc(0,0,0); FRotator Rot(0,0,0);
        if (Payload->TryGetObjectField(TEXT("params"), LocObj) && LocObj && (*LocObj).IsValid())
        {
            const TSharedPtr<FJsonObject>& P = *LocObj; ReadVectorField(P, TEXT("location"), Loc, Loc); ReadRotatorField(P, TEXT("rotation"), Rot, Rot);
        }
        else
        {
            if (const TSharedPtr<FJsonValue> LocVal = Payload->TryGetField(TEXT("location")))
            {
                if (LocVal->Type == EJson::Array)
                {
                    const TArray<TSharedPtr<FJsonValue>>& A = LocVal->AsArray(); if (A.Num() >= 3) Loc = FVector((float)A[0]->AsNumber(), (float)A[1]->AsNumber(), (float)A[2]->AsNumber());
                }
                else if (LocVal->Type == EJson::Object)
                {
                    const TSharedPtr<FJsonObject> LocObject = LocVal->AsObject(); if (LocObject.IsValid()) ReadVectorField(LocObject, TEXT("location"), Loc, Loc);
                }
            }
        }

        if (!GEditor)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
            return true;
        }
        UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
        if (!ActorSS)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
            return true;
        }
        UClass* Resolved = nullptr;
        if (!ClassPath.IsEmpty())
        {
            Resolved = ResolveClassByName(ClassPath);
        }
        if (!Resolved)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("Class not found"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Class not found"), Err, TEXT("CLASS_NOT_FOUND"));
            return true;
        }
        AActor* Spawned = ActorSS->SpawnActorFromClass(Resolved, Loc, Rot);
        if (!Spawned)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("Spawn failed"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Spawn failed"), Err, TEXT("SPAWN_FAILED"));
            return true;
        }
        TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
        Out->SetStringField(TEXT("actorName"), Spawned->GetActorLabel());
        Out->SetStringField(TEXT("actorPath"), Spawned->GetPathName());
        Out->SetBoolField(TEXT("success"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor spawned"), Out, FString());
        return true;
    }

    if (FN == TEXT("DELETE_ACTOR") || FN == TEXT("DESTROY_ACTOR"))
    {
        FString Target; Payload->TryGetStringField(TEXT("actor_name"), Target); if (Target.IsEmpty()) Payload->TryGetStringField(TEXT("actorName"), Target);
        if (Target.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("actor_name required"), TEXT("INVALID_ARGUMENT")); return true; }
        if (!GEditor)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
            return true;
        }
        UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
        if (!ActorSS)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
            return true;
        }
        AActor* Found = nullptr;
        for (AActor* A : ActorSS->GetAllLevelActors())
        {
            if (!A) continue;
            if (A->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase)
                || A->GetName().Equals(Target, ESearchCase::IgnoreCase)
                || A->GetPathName().Equals(Target, ESearchCase::IgnoreCase))
            {
                Found = A;
                break;
            }
        }
        if (!Found)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("Actor not found"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Err, TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        const bool bDeleted = ActorSS->DestroyActor(Found);
        TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
        Out->SetBoolField(TEXT("success"), bDeleted);
        if (bDeleted)
        {
            Out->SetStringField(TEXT("deleted"), Found->GetActorLabel());
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor deleted"), Out, FString());
        }
        else
        {
            Out->SetStringField(TEXT("error"), TEXT("Delete failed"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Delete failed"), Out, TEXT("DELETE_FAILED"));
        }
        return true;
    }

    if (FN == TEXT("ASSET_EXISTS") || FN == TEXT("ASSET_EXISTS_SIMPLE"))
    {
        FString PathToCheck;
        // Accept either top-level 'path' or nested params.path
        if (!Payload->TryGetStringField(TEXT("path"), PathToCheck))
        {
            const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
            if (Payload->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr && (*ParamsPtr).IsValid())
            {
                (*ParamsPtr)->TryGetStringField(TEXT("path"), PathToCheck);
            }
        }
        if (PathToCheck.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("path required"), TEXT("INVALID_ARGUMENT")); return true; }

        // Perform check on game thread
        TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
        const bool bExists = UEditorAssetLibrary::DoesAssetExist(PathToCheck);
        Out->SetBoolField(TEXT("exists"), bExists);
        Out->SetStringField(TEXT("path"), PathToCheck);
        Out->SetBoolField(TEXT("success"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true, bExists ? TEXT("Asset exists") : TEXT("Asset not found"), Out, bExists ? FString() : TEXT("NOT_FOUND"));
        return true;
    }

    if (FN == TEXT("SET_VIEWPORT_CAMERA") || FN == TEXT("SET_VIEWPORT_CAMERA_INFO"))
    {
        const TSharedPtr<FJsonObject>* Params = nullptr; FVector Loc(0,0,0); FRotator Rot(0,0,0);
        if (Payload->TryGetObjectField(TEXT("params"), Params) && Params && (*Params).IsValid()) { ReadVectorField(*Params, TEXT("location"), Loc, Loc); ReadRotatorField(*Params, TEXT("rotation"), Rot, Rot); }
        else { ReadVectorField(Payload, TEXT("location"), Loc, Loc); ReadRotatorField(Payload, TEXT("rotation"), Rot, Rot); }
        if (!GEditor)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
            return true;
        }

        if (UUnrealEditorSubsystem* UES = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>())
        {
            UES->SetLevelViewportCameraInfo(Loc, Rot);
            if (ULevelEditorSubsystem* LES = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>())
            {
                LES->EditorInvalidateViewports();
            }
            TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
            R->SetBoolField(TEXT("success"), true);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Camera set"), R, FString());
        }
        else
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("UnrealEditorSubsystem not available"), nullptr, TEXT("NOT_IMPLEMENTED"));
        }
        return true;
    }

    if (FN == TEXT("BUILD_LIGHTING"))
    {
        FString Quality; Payload->TryGetStringField(TEXT("quality"), Quality);
        if (!GEditor)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
            return true;
        }
        if (ULevelEditorSubsystem* LES = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>())
        {
            ELightingBuildQuality QualityEnum = ELightingBuildQuality::Quality_Production;
            if (!Quality.IsEmpty())
            {
                const FString LowerQuality = Quality.ToLower();
                if (LowerQuality == TEXT("preview")) { QualityEnum = ELightingBuildQuality::Quality_Preview; }
                else if (LowerQuality == TEXT("medium")) { QualityEnum = ELightingBuildQuality::Quality_Medium; }
                else if (LowerQuality == TEXT("high")) { QualityEnum = ELightingBuildQuality::Quality_High; }
            }
            LES->BuildLightMaps(QualityEnum, /*bWithReflectionCaptures*/false);
            TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
            R->SetBoolField(TEXT("requested"), true);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Build lighting requested"), R, FString());
        }
        else
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("LevelEditorSubsystem not available"), nullptr, TEXT("NOT_IMPLEMENTED"));
        }
        return true;
    }

    // RESOLVE_OBJECT: return basic object/asset discovery info
    if (FN == TEXT("RESOLVE_OBJECT"))
    {
        FString Path; Payload->TryGetStringField(TEXT("path"), Path);
        if (Path.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("path required"), TEXT("INVALID_ARGUMENT")); return true; }
        TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
        bool bExists = false;
        FString ClassName;
        if (UEditorAssetLibrary::DoesAssetExist(Path))
        {
            bExists = true;
            if (UObject* Obj = UEditorAssetLibrary::LoadAsset(Path))
            {
                if (UClass* Cls = Obj->GetClass())
                {
                    ClassName = Cls->GetPathName();
                }
            }
        }
        else if (UObject* Obj = FindObject<UObject>(nullptr, *Path))
        {
            bExists = true;
            if (UClass* Cls = Obj->GetClass())
            {
                ClassName = Cls->GetPathName();
            }
        }
        Out->SetBoolField(TEXT("exists"), bExists);
        Out->SetStringField(TEXT("path"), Path);
        Out->SetStringField(TEXT("class"), ClassName);
        Out->SetBoolField(TEXT("success"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true, bExists ? TEXT("Object resolved") : TEXT("Object not found"), Out, bExists ? FString() : TEXT("NOT_FOUND"));
        return true;
    }

    // LIST_ACTOR_COMPONENTS: provide a simple listing of components for a given editor actor
    if (FN == TEXT("LIST_ACTOR_COMPONENTS"))
    {
        FString ActorPath; Payload->TryGetStringField(TEXT("actorPath"), ActorPath);
        if (ActorPath.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("actorPath required"), TEXT("INVALID_ARGUMENT")); return true; }
        if (!GEditor)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
            return true;
        }
        UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
        if (!ActorSS)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
            return true;
        }
        AActor* Found = nullptr;
        for (AActor* A : ActorSS->GetAllLevelActors())
        {
            if (!A) continue;
            if (A->GetActorLabel().Equals(ActorPath, ESearchCase::IgnoreCase)
                || A->GetName().Equals(ActorPath, ESearchCase::IgnoreCase)
                || A->GetPathName().Equals(ActorPath, ESearchCase::IgnoreCase))
            {
                Found = A;
                break;
            }
        }
        if (!Found)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("Actor not found"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Err, TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        TArray<UActorComponent*> Comps;
        Found->GetComponents(Comps);
        TArray<TSharedPtr<FJsonValue>> Arr;
        for (UActorComponent* C : Comps)
        {
            if (!C) continue;
            TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
            R->SetStringField(TEXT("name"), C->GetName());
            R->SetStringField(TEXT("class"), C->GetClass() ? C->GetClass()->GetPathName() : TEXT(""));
            R->SetStringField(TEXT("path"), C->GetPathName());
            Arr.Add(MakeShared<FJsonValueObject>(R));
        }
        TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
        Out->SetArrayField(TEXT("components"), Arr);
        Out->SetNumberField(TEXT("count"), Arr.Num());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Components listed"), Out, FString());
        return true;
    }

    // GET_BLUEPRINT_CDO: best-effort CDO/class info for a Blueprint asset
    if (FN == TEXT("GET_BLUEPRINT_CDO"))
    {
        FString BlueprintPath; Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
        if (BlueprintPath.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("blueprintPath required"), TEXT("INVALID_ARGUMENT")); return true; }

        TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
        UObject* Obj = UEditorAssetLibrary::LoadAsset(BlueprintPath);
        if (!Obj)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint not found"), nullptr, TEXT("NOT_FOUND"));
            return true;
        }

        if (UBlueprint* BP = Cast<UBlueprint>(Obj))
        {
            if (BP->GeneratedClass)
            {
                UClass* Gen = BP->GeneratedClass;
                Out->SetStringField(TEXT("blueprintPath"), BlueprintPath);
                Out->SetStringField(TEXT("classPath"), Gen->GetPathName());
                Out->SetStringField(TEXT("className"), Gen->GetName());
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint CDO info"), Out, FString());
                return true;
            }
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint/GeneratedClass not available"), nullptr, TEXT("NOT_IMPLEMENTED"));
            return true;
        }

        if (UClass* C = Cast<UClass>(Obj))
        {
            Out->SetStringField(TEXT("classPath"), C->GetPathName());
            Out->SetStringField(TEXT("className"), C->GetName());
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Class info"), Out, FString());
            return true;
        }

        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint/GeneratedClass not available"), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
    }

    if (FN == TEXT("BLUEPRINT_ADD_COMPONENT"))
    {
        const TSharedPtr<FJsonObject>* Params = nullptr; TSharedPtr<FJsonObject> LocalParams = MakeShared<FJsonObject>();
        if (Payload->TryGetObjectField(TEXT("params"), Params) && Params && (*Params).IsValid())
        {
            LocalParams = *Params;
        }
        else if (Payload->HasField(TEXT("payloadBase64")))
        {
            FString Enc; Payload->TryGetStringField(TEXT("payloadBase64"), Enc);
            if (!Enc.IsEmpty())
            {
                TArray<uint8> DecodedBytes;
                if (FBase64::Decode(Enc, DecodedBytes) && DecodedBytes.Num() > 0)
                {
                    DecodedBytes.Add(0);
                    const ANSICHAR* Utf8 = reinterpret_cast<const ANSICHAR*>(DecodedBytes.GetData());
                    FString Decoded = FString(UTF8_TO_TCHAR(Utf8));
                    TSharedPtr<FJsonObject> Parsed = MakeShared<FJsonObject>();
                    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Decoded);
                    if (FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid())
                    {
                        LocalParams = Parsed;
                    }
                }
            }
        }

        FString TargetBP; LocalParams->TryGetStringField(TEXT("blueprintPath"), TargetBP);
        if (TargetBP.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("blueprintPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        TSharedPtr<FJsonObject> SCSPayload = MakeShared<FJsonObject>();
        SCSPayload->SetStringField(TEXT("blueprintPath"), TargetBP);

        TArray<TSharedPtr<FJsonValue>> Ops;
        TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
        Op->SetStringField(TEXT("type"), TEXT("add_component"));
        FString Name; LocalParams->TryGetStringField(TEXT("componentName"), Name); if (!Name.IsEmpty()) Op->SetStringField(TEXT("componentName"), Name);
        FString Class; LocalParams->TryGetStringField(TEXT("componentClass"), Class); if (!Class.IsEmpty()) Op->SetStringField(TEXT("componentClass"), Class);
        FString AttachTo; LocalParams->TryGetStringField(TEXT("attachTo"), AttachTo); if (!AttachTo.IsEmpty()) Op->SetStringField(TEXT("attachTo"), AttachTo);
        Ops.Add(MakeShared<FJsonValueObject>(Op));
        SCSPayload->SetArrayField(TEXT("operations"), Ops);

        return HandleBlueprintAction(RequestId, TEXT("blueprint_modify_scs"), SCSPayload, RequestingSocket);
    }

    // PLAY_SOUND helpers
    if (FN == TEXT("PLAY_SOUND_AT_LOCATION") || FN == TEXT("PLAY_SOUND_2D"))
    {
        const TSharedPtr<FJsonObject>* Params = nullptr; if (!Payload->TryGetObjectField(TEXT("params"), Params) || !(*Params).IsValid()) { /* allow top-level path fields */ }
        FString SoundPath; if (!Payload->TryGetStringField(TEXT("path"), SoundPath)) Payload->TryGetStringField(TEXT("soundPath"), SoundPath);
        if (SoundPath.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("soundPath or path required"), TEXT("INVALID_ARGUMENT")); return true; }
        if (!GEditor)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor world not available"), nullptr, TEXT("NOT_IMPLEMENTED"));
            return true;
        }
        UWorld* World = nullptr;
        if (UUnrealEditorSubsystem* UES = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>())
        {
            World = UES->GetEditorWorld();
        }
        if (!World)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor world not available"), nullptr, TEXT("NOT_IMPLEMENTED"));
            return true;
        }

        USoundBase* Snd = Cast<USoundBase>(UEditorAssetLibrary::LoadAsset(SoundPath));
        if (!Snd)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("error"), TEXT("Sound asset not found"));
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Sound not found"), Err, TEXT("NOT_FOUND"));
            return true;
        }

        if (FN == TEXT("PLAY_SOUND_AT_LOCATION"))
        {
            float x = 0, y = 0, z = 0;
            const TSharedPtr<FJsonObject>* LocObj = nullptr;
            if (Payload->TryGetObjectField(TEXT("params"), LocObj) && LocObj && (*LocObj).IsValid())
            {
                (*LocObj)->TryGetNumberField(TEXT("x"), x);
                (*LocObj)->TryGetNumberField(TEXT("y"), y);
                (*LocObj)->TryGetNumberField(TEXT("z"), z);
            }
            FVector Loc(x, y, z);
            UGameplayStatics::SpawnSoundAtLocation(World, Snd, Loc);
        }
        else
        {
            UGameplayStatics::SpawnSoundAtLocation(World, Snd, FVector::ZeroVector);
        }

        TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
        Out->SetBoolField(TEXT("success"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sound played"), Out, FString());
        return true;
    }

    // ADD_WIDGET_TO_VIEWPORT: best-effort; not always supported in editor context
    if (FN == TEXT("ADD_WIDGET_TO_VIEWPORT"))
    {
        FString WidgetPath; Payload->TryGetStringField(TEXT("widget_path"), WidgetPath);
        int32 z = 0; Payload->TryGetNumberField(TEXT("z_order"), z);
        int32 playerIndex = 0; Payload->TryGetNumberField(TEXT("player_index"), playerIndex);
        // Editor-time widget addition is not supported reliably; return NOT_IMPLEMENTED so server may fallback
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Add widget to viewport not implemented natively in editor context"), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
    }

    // RC_* pass-through: indicate not implemented natively so server may fallback to Python
    if (FN.StartsWith(TEXT("RC_")))
    {
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Remote Control functions are not implemented natively in plugin; allow Python fallback or implement RC handlers"), nullptr, TEXT("UNKNOWN_PLUGIN_ACTION"));
        return true;
    }

    // Map several BLUEPRINT_* editor-function fallbacks to the blueprint_* action handlers
    if (FN == TEXT("CREATE_BLUEPRINT") || FN == TEXT("BLUEPRINT_CREATE"))
    {
        // Expect either 'payload' containing JSON string or nested params
        FString JsonStr; if (Payload->TryGetStringField(TEXT("payload"), JsonStr) && !JsonStr.IsEmpty())
        {
            TSharedPtr<FJsonObject> Parsed; TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
            if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid JSON payload"), TEXT("INVALID_ARGUMENT")); return true; }
            return HandleBlueprintAction(RequestId, TEXT("blueprint_create"), Parsed, RequestingSocket);
        }
        // Try nested params object
        const TSharedPtr<FJsonObject>* ParamsObj = nullptr; if (Payload->TryGetObjectField(TEXT("params"), ParamsObj) && ParamsObj && (*ParamsObj).IsValid()) { return HandleBlueprintAction(RequestId, TEXT("blueprint_create"), *ParamsObj, RequestingSocket); }
        // Fallback: try forwarding full payload
        return HandleBlueprintAction(RequestId, TEXT("blueprint_create"), Payload, RequestingSocket);
    }

    if (FN == TEXT("BLUEPRINT_ADD_VARIABLE") || FN == TEXT("BLUEPRINT_ADD_VAR"))
    {
        // Accept either 'payload' JSON string or params
        FString JsonStr; if (Payload->TryGetStringField(TEXT("payload"), JsonStr) && !JsonStr.IsEmpty()) { TSharedPtr<FJsonObject> Parsed; TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr); if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid JSON payload"), TEXT("INVALID_ARGUMENT")); return true; } return HandleBlueprintAction(RequestId, TEXT("blueprint_add_variable"), Parsed, RequestingSocket); }
        const TSharedPtr<FJsonObject>* ParamsObj = nullptr; if (Payload->TryGetObjectField(TEXT("params"), ParamsObj) && ParamsObj && (*ParamsObj).IsValid()) { return HandleBlueprintAction(RequestId, TEXT("blueprint_add_variable"), *ParamsObj, RequestingSocket); }
        return HandleBlueprintAction(RequestId, TEXT("blueprint_add_variable"), Payload, RequestingSocket);
    }

    if (FN == TEXT("BLUEPRINT_SET_VARIABLE_METADATA") || FN == TEXT("BLUEPRINT_SET_VAR_METADATA"))
    {
        FString JsonStr; if (Payload->TryGetStringField(TEXT("payload"), JsonStr) && !JsonStr.IsEmpty()) { TSharedPtr<FJsonObject> Parsed; TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr); if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid JSON payload"), TEXT("INVALID_ARGUMENT")); return true; } return HandleBlueprintAction(RequestId, TEXT("blueprint_set_variable_metadata"), Parsed, RequestingSocket); }
        const TSharedPtr<FJsonObject>* ParamsObj = nullptr; if (Payload->TryGetObjectField(TEXT("params"), ParamsObj) && ParamsObj && (*ParamsObj).IsValid()) { return HandleBlueprintAction(RequestId, TEXT("blueprint_set_variable_metadata"), *ParamsObj, RequestingSocket); }
        return HandleBlueprintAction(RequestId, TEXT("blueprint_set_variable_metadata"), Payload, RequestingSocket);
    }

    if (FN == TEXT("BLUEPRINT_ADD_CONSTRUCTION_SCRIPT") || FN == TEXT("BLUEPRINT_ADD_CONSTRUCTION"))
    {
        FString JsonStr; if (Payload->TryGetStringField(TEXT("payload"), JsonStr) && !JsonStr.IsEmpty()) { TSharedPtr<FJsonObject> Parsed; TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr); if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid JSON payload"), TEXT("INVALID_ARGUMENT")); return true; } return HandleBlueprintAction(RequestId, TEXT("blueprint_add_construction_script"), Parsed, RequestingSocket); }
        const TSharedPtr<FJsonObject>* ParamsObj = nullptr; if (Payload->TryGetObjectField(TEXT("params"), ParamsObj) && ParamsObj && (*ParamsObj).IsValid()) { return HandleBlueprintAction(RequestId, TEXT("blueprint_add_construction_script"), *ParamsObj, RequestingSocket); }
        return HandleBlueprintAction(RequestId, TEXT("blueprint_add_construction_script"), Payload, RequestingSocket);
    }

    // CREATE_SOUND_CUE: create a SoundCue asset when factory is available
    if (FN == TEXT("CREATE_SOUND_CUE"))
    {
        const TSharedPtr<FJsonObject>* ParamsPtr = nullptr; TSharedPtr<FJsonObject> Params = Payload;
        if (Payload->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr && (*ParamsPtr).IsValid()) Params = *ParamsPtr;
        FString Name; Params->TryGetStringField(TEXT("name"), Name);
        FString Package; Params->TryGetStringField(TEXT("package_path"), Package);
        if (Name.IsEmpty() || Package.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("name and package_path required"), TEXT("INVALID_ARGUMENT")); return true; }
#if WITH_EDITOR
        UFactory* FactoryInstance = nullptr;
        UClass* FactoryClass = ResolveClassByName(TEXT("SoundCueFactoryNew"));
        if (FactoryClass && FactoryClass->IsChildOf(UFactory::StaticClass()))
        {
            FactoryInstance = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
        }
        FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        UObject* Created = AssetTools.Get().CreateAsset(Name, Package, USoundCue::StaticClass(), FactoryInstance);
        if (!Created)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to create SoundCue"), nullptr, TEXT("CREATE_FAILED"));
            return true;
        }
        SaveLoadedAssetThrottled(Created);
        TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
        Out->SetStringField(TEXT("path"), Created->GetPathName());
        Out->SetBoolField(TEXT("success"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SoundCue created"), Out, FString());
        return true;
#else
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Create sound cue requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // Unknown function -> indicate the plugin does not implement it so callers
    // can either fall back to Python (server opt-in) or surface UNKNOWN_PLUGIN_ACTION.
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Unknown editor function or not implemented by plugin"), nullptr, TEXT("UNKNOWN_PLUGIN_ACTION"));
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor functions require editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
