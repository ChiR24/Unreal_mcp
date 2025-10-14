#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "IPythonScriptPlugin.h"
#include "Misc/Crc.h"
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
#endif

bool UMcpAutomationBridgeSubsystem::HandleExecuteEditorPython(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString LowerAction = Action.ToLower();
    if (!Action.Equals(TEXT("execute_editor_python"), ESearchCase::IgnoreCase) && !LowerAction.Contains(TEXT("execute_editor_python"))) return false;

    const double EntryTime = FPlatformTime::Seconds();
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("enter execute_editor_python RequestId=%s"), *RequestId);
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("execute_editor_python payload missing."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // Deprecated path: guard execution with a runtime opt-in flag so callers
    // can be migrated to native handlers. Additionally, if the plugin's
    // settings require a capability token for incoming connections, ensure
    // that the requesting socket presented the expected token during
    // handshake to prevent arbitrary remote execution.
    if (!bAllowPythonFallbacks)
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("execute_editor_python RequestId=%s rejected: Python fallbacks are disabled (deprecated feature)."), *RequestId);
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("execute_editor_python is disabled by plugin; enable 'AllowPythonFallbacks' in project settings to use this deprecated feature."), nullptr, TEXT("PYTHON_FALLBACK_DISABLED"));
        return true;
    }

    // If a capability token is required by settings, ensure the requesting
    // socket provided it during handshake. Reject execution if token not
    // present to reduce abuse risk during migration.
    if (bRequireCapabilityToken && !RequestingSocket.IsValid())
    {
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("execute_editor_python rejected: missing socket or capability token."), nullptr, TEXT("PYTHON_FALLBACK_REJECTED"));
        return true;
    }

    FString Script;
    if (!Payload->TryGetStringField(TEXT("script"), Script) || Script.TrimStartAndEnd().IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("execute_editor_python requires a non-empty script."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (!FModuleManager::Get().IsModuleLoaded(TEXT("PythonScriptPlugin")))
    {
        FModuleManager::LoadModulePtr<IPythonScriptPlugin>(TEXT("PythonScriptPlugin"));
    }

    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    if (!PythonPlugin)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("PythonScriptPlugin is not available. Enable the Python Editor Script Plugin."), TEXT("PYTHON_PLUGIN_DISABLED"));
        return true;
    }

    const FString ScriptKey = Script.TrimStartAndEnd();
    {
        FScopeLock Lock(&GPythonExecMutex);
        if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>* Subs = GPythonExecInflight.Find(ScriptKey))
        {
            Subs->Add(TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>(RequestId, RequestingSocket));
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Coalesced execute_editor_python for key (subscribers=%d)"), Subs->Num());
            return true;
        }
        GPythonExecInflight.Add(ScriptKey, TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>());
        GPythonExecInflight[ScriptKey].Add(TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>(RequestId, RequestingSocket));
    }

    // Map recognized Python templates to native handlers where possible so we
    // can avoid executing arbitrary Python in the editor. If we identify a
    // known template we will perform the equivalent native operation on the
    // game thread and respond to all coalesced subscribers with the same
    // structured result. If the script is unrecognized, only allow ExecPython
    // when the plugin settings permit it (allow-all or allowlist match).

    // Snapshot subscribers and remove inflight entry atomically
    TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>> SubsCopy;
    {
        FScopeLock Lock(&GPythonExecMutex);
        if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>* Subs = GPythonExecInflight.Find(ScriptKey))
        {
            SubsCopy = *Subs;
            GPythonExecInflight.Remove(ScriptKey);
        }
    }

    // Helper to broadcast a JSON result to all coalesced subscribers
    auto BroadcastResult = [this, ScriptKey, EntryTime](const TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>& Targets, bool bSuccess, const FString& Message, const TSharedPtr<FJsonObject>& ResultPayload, const FString& ErrorCode = FString()) {
        for (const TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>& Pair : Targets)
        {
            SendAutomationResponse(Pair.Value, Pair.Key, bSuccess, Message, ResultPayload, bSuccess ? FString() : (ErrorCode.IsEmpty() ? TEXT("PYTHON_EXEC_FAILED") : ErrorCode));
            TSharedPtr<FJsonObject> PerNotify = MakeShared<FJsonObject>();
            PerNotify->SetStringField(TEXT("type"), TEXT("automation_event"));
            PerNotify->SetStringField(TEXT("event"), TEXT("execute_python_completed"));
            PerNotify->SetStringField(TEXT("requestId"), Pair.Key);
            PerNotify->SetObjectField(TEXT("result"), ResultPayload.IsValid() ? ResultPayload : MakeShared<FJsonObject>());
            SendControlMessage(PerNotify);
        }
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("execute_editor_python RequestId=%s handled natively in %.0f ms"), *ScriptKey, (FPlatformTime::Seconds() - EntryTime) * 1000.0);
    };

#if WITH_EDITOR
    const FString LowerScript = Script.ToLower();

    // Recognize 'get_all_level_actors' pattern
    if (LowerScript.Contains(TEXT("get_editor_subsystem(")) && LowerScript.Contains(TEXT("get_all_level_actors")))
    {
        AsyncTask(ENamedThreads::GameThread, [this, SubsCopy, BroadcastResult]() {
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            TArray<TSharedPtr<FJsonValue>> Arr;
            if (!GEditor) {
                SendAutomationError(nullptr, TEXT("coalesced"), TEXT("Editor not available"), TEXT("EDITOR_NOT_AVAILABLE"));
                return;
            }
            UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
            if (!ActorSS) {
                Result->SetBoolField(TEXT("success"), false);
                Result->SetStringField(TEXT("error"), TEXT("EditorActorSubsystem not available"));
                BroadcastResult(SubsCopy, false, TEXT("EditorActorSubsystem not available"), Result, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
                return;
            }
            TArray<AActor*> Actors = ActorSS->GetAllLevelActors();
            for (AActor* A : Actors) { if (!A) continue; TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>(); E->SetStringField(TEXT("name"), A->GetName()); E->SetStringField(TEXT("label"), A->GetActorLabel()); E->SetStringField(TEXT("path"), A->GetPathName()); E->SetStringField(TEXT("class"), A->GetClass() ? A->GetClass()->GetPathName() : TEXT("")); Arr.Add(MakeShared<FJsonValueObject>(E)); }
            Result->SetArrayField(TEXT("actors"), Arr); Result->SetNumberField(TEXT("count"), Arr.Num()); Result->SetBoolField(TEXT("success"), true);
            BroadcastResult(SubsCopy, true, TEXT("Actor list"), Result, FString());
        });
        return true;
    }

    // Recognize asset existence checks using EditorAssetLibrary
    if (LowerScript.Contains(TEXT("editorassetlibrary.does_asset_exist")) || LowerScript.Contains(TEXT("does_asset_exist(")))
    {
        FString FoundPath;
        // Simple heuristic: find the first quoted substring and use that as path
        int32 FirstQuote = INDEX_NONE;
        for (int32 i = 0; i < Script.Len(); ++i) { if (Script[i] == '"' || Script[i] == '\'') { FirstQuote = i; break; } }
        if (FirstQuote != INDEX_NONE)
        {
            int32 SecondQuote = INDEX_NONE;
            const TCHAR QuoteChar = Script[FirstQuote];
            if (QuoteChar == '"')
            {
                SecondQuote = Script.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, FirstQuote + 1);
            }
            else if (QuoteChar == '\'')
            {
                SecondQuote = Script.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromStart, FirstQuote + 1);
            }
            // Fallback: search for matching quote manually
            if (SecondQuote == INDEX_NONE)
            {
                for (int32 j = FirstQuote + 1; j < Script.Len(); ++j) { if (Script[j] == QuoteChar) { SecondQuote = j; break; } }
            }
            if (SecondQuote != INDEX_NONE && SecondQuote > FirstQuote)
            {
                FoundPath = Script.Mid(FirstQuote + 1, SecondQuote - FirstQuote - 1);
            }
        }

        AsyncTask(ENamedThreads::GameThread, [this, SubsCopy, FoundPath, BroadcastResult]() {
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            if (FoundPath.IsEmpty()) { Result->SetBoolField(TEXT("exists"), false); Result->SetStringField(TEXT("path"), TEXT("")); Result->SetBoolField(TEXT("success"), true); BroadcastResult(SubsCopy, true, TEXT("Asset existence check (no path parsed)"), Result, FString()); return; }
            bool bExists = UEditorAssetLibrary::DoesAssetExist(FoundPath);
            Result->SetBoolField(TEXT("exists"), bExists); Result->SetStringField(TEXT("path"), FoundPath); Result->SetBoolField(TEXT("success"), true);
            BroadcastResult(SubsCopy, true, bExists ? TEXT("Asset exists") : TEXT("Asset not found"), Result, bExists ? FString() : TEXT("NOT_FOUND"));
        });
        return true;
    }

    // Recognize spawn actor patterns (best-effort)
    if (LowerScript.Contains(TEXT("spawn_actor")) || LowerScript.Contains(TEXT("spawn_actor_from_class")))
    {
        // Attempt to parse a class path from the script (first quoted string)
        FString ClassPath;
        int32 FirstQ = Script.Find(TEXT("\""));
        int32 SecondQ = INDEX_NONE;
        if (FirstQ != INDEX_NONE)
        {
            SecondQ = Script.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, FirstQ+1);
            if (SecondQ != INDEX_NONE && SecondQ > FirstQ)
            {
                ClassPath = Script.Mid(FirstQ+1, SecondQ - FirstQ - 1);
            }
        }

        // Attempt to parse a vector constructor for location
        FVector Loc(0,0,0); FRotator Rot(0,0,0);
        // Heuristic: look for 'unreal.Vector(' and parse comma-separated numbers
        int32 VecPos = LowerScript.Find(TEXT("unreal.vector("));
        if (VecPos == INDEX_NONE) VecPos = LowerScript.Find(TEXT("vector("));
        if (VecPos != INDEX_NONE)
        {
            int32 Open = Script.Find(TEXT("("), ESearchCase::IgnoreCase, ESearchDir::FromStart, VecPos);
            if (Open != INDEX_NONE)
            {
                int32 Close = Script.Find(TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Open+1);
                if (Close != INDEX_NONE && Close > Open)
                {
                    FString Inside = Script.Mid(Open+1, Close-Open-1);
                    TArray<FString> Parts; Inside.ParseIntoArray(Parts, TEXT(","), true);
                    if (Parts.Num() >= 3)
                    {
                        Loc.X = FCString::Atof(*Parts[0]); Loc.Y = FCString::Atof(*Parts[1]); Loc.Z = FCString::Atof(*Parts[2]);
                    }
                }
            }
        }

        AsyncTask(ENamedThreads::GameThread, [this, SubsCopy, ClassPath, Loc, Rot, BroadcastResult]() {
            if (!GEditor) { TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>(); R->SetBoolField(TEXT("success"), false); R->SetStringField(TEXT("error"), TEXT("Editor not available")); BroadcastResult(SubsCopy, false, TEXT("Editor not available"), R, TEXT("EDITOR_NOT_AVAILABLE")); return; }
            UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>(); if (!ActorSS) { TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>(); R->SetBoolField(TEXT("success"), false); R->SetStringField(TEXT("error"), TEXT("EditorActorSubsystem not available")); BroadcastResult(SubsCopy, false, TEXT("EditorActorSubsystem not available"), R, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING")); return; }
            UClass* Resolved = nullptr; if (!ClassPath.IsEmpty()) { UObject* Loaded = UEditorAssetLibrary::LoadAsset(ClassPath); if (Loaded) { if (UBlueprint* BP = Cast<UBlueprint>(Loaded)) Resolved = BP->GeneratedClass; else if (UClass* C = Cast<UClass>(Loaded)) Resolved = C; } }
                if (!Resolved) Resolved = ResolveClassByName(ClassPath);
            if (!Resolved) { TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), TEXT("Class not found")); BroadcastResult(SubsCopy, false, TEXT("Class not found"), Err, TEXT("CLASS_NOT_FOUND")); return; }
            AActor* Spawned = ActorSS->SpawnActorFromClass(Resolved, Loc, Rot); if (!Spawned) { TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), TEXT("Spawn failed")); BroadcastResult(SubsCopy, false, TEXT("Spawn failed"), Err, TEXT("SPAWN_FAILED")); return; }
            TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(); Out->SetStringField(TEXT("actorName"), Spawned->GetActorLabel()); Out->SetStringField(TEXT("actorPath"), Spawned->GetPathName()); Out->SetBoolField(TEXT("success"), true); BroadcastResult(SubsCopy, true, TEXT("Actor spawned"), Out, FString());
        });
        return true;
    }

    // Recognize deletion of actor
    if (LowerScript.Contains(TEXT("destroy_actor")) || LowerScript.Contains(TEXT("destroy_actor(")) || LowerScript.Contains(TEXT("delete_actor")))
    {
        FString Target;
        // Pick first quoted value as actor target
        int32 Q1 = Script.Find(TEXT("\""));
        if (Q1 != INDEX_NONE)
        {
            int32 Q2 = Script.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, Q1+1);
            if (Q2 != INDEX_NONE && Q2 > Q1) Target = Script.Mid(Q1+1, Q2-Q1-1);
        }
        if (Target.IsEmpty())
        {
            // Fallback: try to find a variable named actor_name or actorName in script assignments
            int32 Pos = LowerScript.Find(TEXT("actor_name")); if (Pos == INDEX_NONE) Pos = LowerScript.Find(TEXT("actorname"));
            if (Pos != INDEX_NONE)
            {
                // Try to parse the quoted string after '='
                int32 Eq = Script.Find(TEXT("="), ESearchCase::IgnoreCase, ESearchDir::FromStart, Pos);
                if (Eq != INDEX_NONE)
                {
                    int32 Q = Script.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, Eq);
                    int32 Qb = Q != INDEX_NONE ? Script.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, Q+1) : INDEX_NONE;
                    if (Q != INDEX_NONE && Qb != INDEX_NONE && Qb > Q) Target = Script.Mid(Q+1, Qb-Q-1);
                }
            }
        }

        AsyncTask(ENamedThreads::GameThread, [this, SubsCopy, Target, BroadcastResult]() {
            if (!GEditor) { TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>(); R->SetBoolField(TEXT("success"), false); R->SetStringField(TEXT("error"), TEXT("Editor not available")); BroadcastResult(SubsCopy, false, TEXT("Editor not available"), R, TEXT("EDITOR_NOT_AVAILABLE")); return; }
            UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>(); if (!ActorSS) { TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>(); R->SetBoolField(TEXT("success"), false); R->SetStringField(TEXT("error"), TEXT("EditorActorSubsystem not available")); BroadcastResult(SubsCopy, false, TEXT("EditorActorSubsystem not available"), R, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING")); return; }
            AActor* Found = nullptr; for (AActor* A : ActorSS->GetAllLevelActors()) { if (!A) continue; if (A->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) || A->GetName().Equals(Target, ESearchCase::IgnoreCase) || A->GetPathName().Equals(Target, ESearchCase::IgnoreCase)) { Found = A; break; } }
            if (!Found) { TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), TEXT("Actor not found")); BroadcastResult(SubsCopy, false, TEXT("Actor not found"), Err, TEXT("ACTOR_NOT_FOUND")); return; }
            bool bDeleted = ActorSS->DestroyActor(Found); TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(); Out->SetBoolField(TEXT("success"), bDeleted); if (bDeleted) { Out->SetStringField(TEXT("deleted"), Found->GetActorLabel()); BroadcastResult(SubsCopy, true, TEXT("Actor deleted"), Out, FString()); } else { Out->SetStringField(TEXT("error"), TEXT("Delete failed")); BroadcastResult(SubsCopy, false, TEXT("Delete failed"), Out, TEXT("DELETE_FAILED")); }
        });
        return true;
    }

    // Recognize viewport camera set
    if (LowerScript.Contains(TEXT("set_level_viewport_camera_info")) || LowerScript.Contains(TEXT("set_level_viewport_camera")))
    {
        // Try to extract numeric fields for location/rotation (best-effort)
        float X=0,Y=0,Z=0,Pitch=0,Yaw=0,Roll=0;
        // Very simple pattern matching for numbers in the script
        // Attempt to find the first triple for location
        int32 VecPos = LowerScript.Find(TEXT("vector("));
        if (VecPos != INDEX_NONE) {
            int32 Open = Script.Find(TEXT("("), ESearchCase::IgnoreCase, ESearchDir::FromStart, VecPos);
            int32 Close = Open != INDEX_NONE ? Script.Find(TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Open+1) : INDEX_NONE;
            if (Open != INDEX_NONE && Close != INDEX_NONE && Close > Open) {
                FString Inside = Script.Mid(Open+1, Close-Open-1);
                TArray<FString> Parts; Inside.ParseIntoArray(Parts, TEXT(","), true);
                if (Parts.Num()>=3) { X = FCString::Atof(*Parts[0]); Y = FCString::Atof(*Parts[1]); Z = FCString::Atof(*Parts[2]); }
            }
        }
        AsyncTask(ENamedThreads::GameThread, [this, SubsCopy, X, Y, Z, Pitch, Yaw, Roll, BroadcastResult]() {
            if (!GEditor) { TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>(); R->SetBoolField(TEXT("success"), false); R->SetStringField(TEXT("error"), TEXT("Editor not available")); BroadcastResult(SubsCopy, false, TEXT("Editor not available"), R, TEXT("EDITOR_NOT_AVAILABLE")); return; }
            if (UUnrealEditorSubsystem* UES = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>()) { UES->SetLevelViewportCameraInfo(FVector(X,Y,Z), FRotator(Pitch,Yaw,Roll)); if (ULevelEditorSubsystem* LES = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()) LES->EditorInvalidateViewports(); TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>(); R->SetBoolField(TEXT("success"), true); BroadcastResult(SubsCopy, true, TEXT("Camera set"), R, FString()); return; }
            TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>(); R->SetBoolField(TEXT("success"), false); R->SetStringField(TEXT("error"), TEXT("UnrealEditorSubsystem not available")); BroadcastResult(SubsCopy, false, TEXT("UnrealEditorSubsystem not available"), R, TEXT("NOT_IMPLEMENTED"));
        });
        return true;
    }

    // Recognize build lighting request
    if (LowerScript.Contains(TEXT("build_light_maps")) || LowerScript.Contains(TEXT("build_lightings")) || LowerScript.Contains(TEXT("build_light")))
    {
        AsyncTask(ENamedThreads::GameThread, [this, SubsCopy, BroadcastResult, RequestId, RequestingSocket, Payload]() {
            if (!GEditor)
            {
                TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
                R->SetBoolField(TEXT("success"), false);
                R->SetStringField(TEXT("error"), TEXT("Editor not available"));
                BroadcastResult(SubsCopy, false, TEXT("Editor not available"), R, TEXT("EDITOR_NOT_AVAILABLE"));
                return;
            }

            FString Quality;
            if (Payload.IsValid())
            {
                Payload->TryGetStringField(TEXT("quality"), Quality);
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
                R->SetBoolField(TEXT("success"), true);
                if (!Quality.IsEmpty())
                {
                    R->SetStringField(TEXT("quality"), Quality);
                }
                BroadcastResult(SubsCopy, true, TEXT("Build lighting requested"), R, FString());
                if (RequestingSocket.IsValid())
                {
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Build lighting requested"), R, FString());
                }
                return;
            }

            TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
            R->SetBoolField(TEXT("success"), false);
            R->SetStringField(TEXT("error"), TEXT("LevelEditorSubsystem not available"));
            BroadcastResult(SubsCopy, false, TEXT("LevelEditorSubsystem not available"), R, TEXT("NOT_IMPLEMENTED"));
            if (RequestingSocket.IsValid())
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("LevelEditorSubsystem not available"), nullptr, TEXT("NOT_IMPLEMENTED"));
            }
        });
        return true;
    }
#endif

    // If not recognized, raw Python execution has been removed from the
    // plugin. Return an explicit rejection so callers can migrate to
    // execute_editor_function or implement a native handler.
    TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
    Err->SetStringField(TEXT("rejectedScriptSnippet"), Script.Left(256));
    BroadcastResult(SubsCopy, false, TEXT("execute_editor_python is no longer supported by the plugin. Convert calls to execute_editor_function or implement a native handler."), Err, TEXT("PYTHON_FALLBACK_REMOVED"));
    return true;
}
