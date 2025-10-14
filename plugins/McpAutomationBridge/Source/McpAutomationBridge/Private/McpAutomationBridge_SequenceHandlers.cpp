#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#if WITH_EDITOR
#if 0
#include "IPythonScriptPlugin.h"
#endif
#include "Editor.h"
#include "EditorAssetLibrary.h"
#if __has_include("Toolkits/AssetEditorManager.h")
#include "Toolkits/AssetEditorManager.h"
#elif __has_include("AssetEditorManager.h")
#include "AssetEditorManager.h"
#endif
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/Selection.h"
#include "Subsystems/AssetEditorSubsystem.h"
#if __has_include("LevelSequenceEditorSubsystem.h")
#include "LevelSequenceEditorSubsystem.h"
#define MCP_HAS_LEVELSEQUENCE_EDITOR_SUBSYSTEM 1
#endif
#if __has_include("LevelSequence.h")
#include "LevelSequence.h"
#define MCP_HAS_LEVELSEQUENCE 1
#endif
#if __has_include("MovieScene.h")
#include "MovieScene.h"
#define MCP_HAS_MOVIESCENE 1
#endif
#if __has_include("Camera/CameraActor.h")
#include "Camera/CameraActor.h"
#endif
#if __has_include("IAssetTools.h")
#include "IAssetTools.h"
#endif
#include "Editor/EditorEngine.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleSequenceAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.StartsWith(TEXT("sequence_"))) return false;

    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();

    auto SerializeResponseAndSend = [&](bool bOk, const FString& Msg, const TSharedPtr<FJsonObject>& ResObj, const FString& ErrCode = FString())
    {
        SendAutomationResponse(RequestingSocket, RequestId, bOk, Msg, ResObj, ErrCode);
    };

    // Helpers to resolve a sequence path and ensure a registry entry
    auto ResolveSequencePath = [&]() -> FString
    {
        FString Path;
        if (LocalPayload->TryGetStringField(TEXT("path"), Path) && !Path.IsEmpty()) return Path;
        if (!GCurrentSequencePath.IsEmpty()) return GCurrentSequencePath;
        return FString();
    };

    auto EnsureSequenceEntry = [&](const FString& SeqPath) -> TSharedPtr<FJsonObject>
    {
        if (SeqPath.IsEmpty()) return nullptr;
        if (TSharedPtr<FJsonObject>* Found = GSequenceRegistry.Find(SeqPath)) return *Found;
        TSharedPtr<FJsonObject> NewObj = MakeShared<FJsonObject>();
        NewObj->SetStringField(TEXT("sequencePath"), SeqPath);
        NewObj->SetBoolField(TEXT("playing"), false);
        NewObj->SetNumberField(TEXT("position"), 0.0);
        NewObj->SetNumberField(TEXT("playbackSpeed"), 1.0);
        NewObj->SetObjectField(TEXT("properties"), MakeShared<FJsonObject>());
        NewObj->SetArrayField(TEXT("bindings"), TArray<TSharedPtr<FJsonValue>>());
        GSequenceRegistry.Add(SeqPath, NewObj);
        return NewObj;
    };

    if (Lower.Equals(TEXT("sequence_create")))
    {
        FString Name; LocalPayload->TryGetStringField(TEXT("name"), Name);
        FString Path; LocalPayload->TryGetStringField(TEXT("path"), Path);
        if (Name.IsEmpty()) { SerializeResponseAndSend(false, TEXT("sequence_create requires name"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
        FString FullPath = Path.IsEmpty() ? FString::Printf(TEXT("/Game/%s"), *Name) : FString::Printf(TEXT("%s/%s"), *Path, *Name);
        TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(FullPath);
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("sequencePath"), FullPath);
        SerializeResponseAndSend(true, TEXT("Sequence created (registry)."), Resp, FString());
        return true;
    }

    // Open a sequence asset in the editor (editor-only)
    if (Lower.Equals(TEXT("sequence_open")))
    {
        FString SeqPath = ResolveSequencePath(); if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("sequence_open requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
        TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(SeqPath);
#if WITH_EDITOR
    AsyncTask(ENamedThreads::GameThread, [this, RequestId, SeqPath, Entry, RequestingSocket, SerializeResponseAndSend]() {
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            // Try to load the sequence asset via EditorAssetLibrary
            UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
            if (!SeqObj) { SerializeResponseAndSend(false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return; }

#if defined(MCP_HAS_LEVELSEQUENCE_EDITOR_SUBSYSTEM)
            if (ULevelSequence* LevelSeq = Cast<ULevelSequence>(SeqObj))
            {
                if (GEditor)
                {
                    if (ULevelSequenceEditorSubsystem* LSES = GEditor->GetEditorSubsystem<ULevelSequenceEditorSubsystem>())
                    {
                        if (UAssetEditorSubsystem* AssetEditorSS = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
                        {
                            AssetEditorSS->OpenEditorForAsset(LevelSeq);
                            Resp->SetStringField(TEXT("sequencePath"), SeqPath);
                            Resp->SetStringField(TEXT("message"), TEXT("Sequence opened"));
                            SerializeResponseAndSend(true, TEXT("Sequence opened"), Resp, FString());
                            return;
                        }
                    }
                }
            }
#endif

            if (GEditor)
            {
                if (UAssetEditorSubsystem* AssetEditorSS = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
                {
                    AssetEditorSS->OpenEditorForAsset(SeqObj);
                }
            }
            Resp->SetStringField(TEXT("sequencePath"), SeqPath);
            Resp->SetStringField(TEXT("message"), TEXT("Sequence opened (asset editor)"));
            SerializeResponseAndSend(true, TEXT("Sequence opened"), Resp, FString());
        });
        return true;
#else
        SerializeResponseAndSend(false, TEXT("sequence_open requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // Add a camera track or spawn a camera - editor-only behavior
    if (Lower.Equals(TEXT("sequence_add_camera")))
    {
        const bool bSpawnable = LocalPayload->HasField(TEXT("spawnable")) ? LocalPayload->GetBoolField(TEXT("spawnable")) : true;
        FString SeqPath = ResolveSequencePath(); if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("sequence_add_camera requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }

#if WITH_EDITOR
        // Execute on GameThread: call LevelSequenceEditorSubsystem to add a camera where possible
    AsyncTask(ENamedThreads::GameThread, [this, RequestId, SeqPath, bSpawnable, RequestingSocket, SerializeResponseAndSend]() {
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
            if (!SeqObj) { SerializeResponseAndSend(false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return; }

            // Fallback: spawn a Camera actor in the editor world (unbound to sequence)
            if (GEditor)
            {
                UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
                if (ActorSS)
                {
                    UClass* CameraClass = ACameraActor::StaticClass();
                    AActor* Spawned = ActorSS->SpawnActorFromClass(CameraClass, FVector::ZeroVector, FRotator::ZeroRotator);
                    if (Spawned)
                    {
                        Resp->SetBoolField(TEXT("success"), true);
                        Resp->SetStringField(TEXT("actorLabel"), Spawned->GetActorLabel());
                        SerializeResponseAndSend(true, TEXT("Camera actor spawned (not bound to sequence)"), Resp, FString());
                        return;
                    }
                }
            }
            SerializeResponseAndSend(false, TEXT("Failed to add camera"), nullptr, TEXT("ADD_CAMERA_FAILED"));
        });
        return true;
#else
        SerializeResponseAndSend(false, TEXT("sequence_add_camera requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    if (Lower.Equals(TEXT("sequence_play")))
    {
        FString SeqPath = ResolveSequencePath(); if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("No sequence selected or path provided"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
        TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(SeqPath);
        if (Entry.IsValid()) Entry->SetBoolField(TEXT("playing"), true);
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("sequencePath"), SeqPath);
        SerializeResponseAndSend(true, TEXT("Sequence play (registry)."), Resp, FString());
        return true;
    }

    // Add a level actor as a binding to a sequence (editor-only)
    if (Lower.Equals(TEXT("sequence_add_actor")))
    {
        FString ActorName; LocalPayload->TryGetStringField(TEXT("actorName"), ActorName);
        if (ActorName.IsEmpty()) { SerializeResponseAndSend(false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
        FString SeqPath = ResolveSequencePath(); if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("sequence_add_actor requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }

#if WITH_EDITOR
    AsyncTask(ENamedThreads::GameThread, [this, RequestId, SeqPath, ActorName, RequestingSocket, SerializeResponseAndSend]() {
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            if (ActorName.IsEmpty()) { SerializeResponseAndSend(false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return; }
            UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
            if (!SeqObj) { SerializeResponseAndSend(false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return; }

            if (GEditor)
            {
                UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
                if (!ActorSS) { SerializeResponseAndSend(false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING")); return; }
                // Try to find actor by label (case-insensitive)
                AActor* Found = nullptr;
                TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
                for (AActor* A : AllActors)
                {
                    if (!A) continue;
                    if (A->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase)) { Found = A; break; }
                }
                if (!Found) { SerializeResponseAndSend(false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return; }

                SerializeResponseAndSend(false, TEXT("Binding actors to sequences not available in this editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
                return;
            }
            SerializeResponseAndSend(false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
        });
        return true;
#else
        SerializeResponseAndSend(false, TEXT("sequence_add_actor requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    if (Lower.Equals(TEXT("sequence_add_actors")))
    {
        const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr; LocalPayload->TryGetArrayField(TEXT("actorNames"), Arr);
        if (!Arr || Arr->Num() == 0) { SerializeResponseAndSend(false, TEXT("actorNames required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
        FString SeqPath = ResolveSequencePath(); if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("sequence_add_actors requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
#if WITH_EDITOR
        TArray<FString> Names; Names.Reserve(Arr->Num()); for (const TSharedPtr<FJsonValue>& V : *Arr) if (V.IsValid() && V->Type == EJson::String) Names.Add(V->AsString());
    AsyncTask(ENamedThreads::GameThread, [this, RequestId, SeqPath, Names, RequestingSocket, SerializeResponseAndSend]() {
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
            if (!SeqObj) { SerializeResponseAndSend(false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return; }
            if (!GEditor) { SerializeResponseAndSend(false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return; }
            UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>(); if (!ActorSS) { SerializeResponseAndSend(false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING")); return; }
            TArray<TSharedPtr<FJsonValue>> Results; Results.Reserve(Names.Num());
            for (const FString& Name : Names)
            {
                TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>(); Item->SetStringField(TEXT("name"), Name);
                AActor* Found = nullptr;
                TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
                for (AActor* A : AllActors) { if (!A) continue; if (A->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase)) { Found = A; break; } }
                if (!Found) { Item->SetBoolField(TEXT("success"), false); Item->SetStringField(TEXT("error"), TEXT("Actor not found")); Results.Add(MakeShared<FJsonValueObject>(Item)); continue; }
                Item->SetBoolField(TEXT("success"), false);
                Item->SetStringField(TEXT("error"), TEXT("Binding not implemented for this editor build"));
                Results.Add(MakeShared<FJsonValueObject>(Item));
            }
            TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(); Out->SetArrayField(TEXT("results"), Results);
            SerializeResponseAndSend(true, TEXT("Actors processed"), Out, FString());
        });
        return true;
#else
        SerializeResponseAndSend(false, TEXT("sequence_add_actors requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // Create a spawnable from a class (editor-only)
    if (Lower.Equals(TEXT("sequence_add_spawnable_from_class")))
    {
        FString ClassName; LocalPayload->TryGetStringField(TEXT("className"), ClassName);
        if (ClassName.IsEmpty()) { SerializeResponseAndSend(false, TEXT("className required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
        FString SeqPath = ResolveSequencePath(); if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("sequence_add_spawnable_from_class requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
#if WITH_EDITOR
    AsyncTask(ENamedThreads::GameThread, [this, RequestId, SeqPath, ClassName, RequestingSocket, SerializeResponseAndSend]() {
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
            if (!SeqObj) { SerializeResponseAndSend(false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return; }
            if (ClassName.IsEmpty()) { SerializeResponseAndSend(false, TEXT("className required"), nullptr, TEXT("INVALID_ARGUMENT")); return; }

            UClass* ResolvedClass = nullptr;
            // If ClassName looks like an asset path, try loading it
            if (ClassName.StartsWith(TEXT("/")) || ClassName.Contains(TEXT("/")))
            {
                UObject* Loaded = UEditorAssetLibrary::LoadAsset(ClassName);
                if (Loaded)
                {
                    if (UBlueprint* BP = Cast<UBlueprint>(Loaded)) ResolvedClass = BP->GeneratedClass;
                    else if (UClass* C = Cast<UClass>(Loaded)) ResolvedClass = C;
                }
            }
            // Try to find native class by name (e.g., CineCameraActor)
            if (!ResolvedClass)
            {
                ResolvedClass = ResolveClassByName(ClassName);
            }
            if (!ResolvedClass) { SerializeResponseAndSend(false, TEXT("Class not found"), nullptr, TEXT("CLASS_NOT_FOUND")); return; }

            SerializeResponseAndSend(false, TEXT("Add spawnable not implemented in current editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        });
        return true;
#else
        SerializeResponseAndSend(false, TEXT("sequence_add_spawnable_from_class requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    if (Lower.Equals(TEXT("sequence_remove_actors")))
    {
        const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr; LocalPayload->TryGetArrayField(TEXT("actorNames"), Arr);
        if (!Arr || Arr->Num() == 0) { SerializeResponseAndSend(false, TEXT("actorNames required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
        FString SeqPath = ResolveSequencePath(); if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("sequence_remove_actors requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
#if WITH_EDITOR
        TArray<FString> Names; for (const TSharedPtr<FJsonValue>& V : *Arr) if (V.IsValid() && V->Type == EJson::String) Names.Add(V->AsString());
    AsyncTask(ENamedThreads::GameThread, [this, RequestId, SeqPath, Names, RequestingSocket, SerializeResponseAndSend]() {
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
            if (!SeqObj) { SerializeResponseAndSend(false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return; }
            if (!GEditor) { SerializeResponseAndSend(false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return; }
            UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>(); if (!ActorSS) { SerializeResponseAndSend(false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING")); return; }
            TArray<TSharedPtr<FJsonValue>> Removed; int32 RemovedCount = 0;
            for (const FString& Name : Names)
            {
                TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>(); Item->SetStringField(TEXT("name"), Name);
                AActor* Found = nullptr; TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
                for (AActor* A : AllActors) { if (!A) continue; if (A->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase)) { Found = A; break; } }
                if (!Found) { Item->SetBoolField(TEXT("success"), false); Item->SetStringField(TEXT("error"), TEXT("Actor not found")); Removed.Add(MakeShared<FJsonValueObject>(Item)); continue; }
                Item->SetBoolField(TEXT("success"), false); Item->SetStringField(TEXT("error"), TEXT("Remove not implemented for this editor build")); Removed.Add(MakeShared<FJsonValueObject>(Item));
            }
            TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(); Out->SetArrayField(TEXT("removedActors"), Removed); Out->SetNumberField(TEXT("bindingsProcessed"), RemovedCount);
            SerializeResponseAndSend(true, TEXT("Actors processed for removal"), Out, FString());
        });
        return true;
#else
        SerializeResponseAndSend(false, TEXT("sequence_remove_actors requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // Query sequence bindings (editor-only)
    if (Lower.Equals(TEXT("sequence_get_bindings")))
    {
        FString SeqPath = ResolveSequencePath(); if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("sequence_get_bindings requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
#if WITH_EDITOR
    AsyncTask(ENamedThreads::GameThread, [this, RequestId, SeqPath, RequestingSocket, SerializeResponseAndSend]() {
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
            if (!SeqObj) { SerializeResponseAndSend(false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return; }
#if defined(MCP_HAS_MOVIESCENE) && defined(MCP_HAS_LEVELSEQUENCE)
            ULevelSequence* LevelSeq = Cast<ULevelSequence>(SeqObj);
            if (LevelSeq && LevelSeq->GetMovieScene())
            {
                UMovieScene* MovieScene = LevelSeq->GetMovieScene();
                TArray<TSharedPtr<FJsonValue>> BindingsArray;
                for (const FMovieSceneBinding& B : MovieScene->GetBindings())
                {
                    TSharedPtr<FJsonObject> Bobj = MakeShared<FJsonObject>();
                    Bobj->SetStringField(TEXT("id"), B.GetObjectGuid().ToString());
                    Bobj->SetStringField(TEXT("name"), B.GetName());
                    BindingsArray.Add(MakeShared<FJsonValueObject>(Bobj));
                }
                Resp->SetArrayField(TEXT("bindings"), BindingsArray);
                SerializeResponseAndSend(true, TEXT("bindings listed"), Resp, FString());
                return;
            }
#endif
            // Fallback: provide minimal information (registered empty list)
            Resp->SetArrayField(TEXT("bindings"), TArray<TSharedPtr<FJsonValue>>());
            SerializeResponseAndSend(true, TEXT("bindings listed (empty/fallback)"), Resp, FString());
        });
        return true;
#else
        SerializeResponseAndSend(false, TEXT("sequence_get_bindings requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // Get sequence properties (editor-only) — frame rate, playback range, duration
    if (Lower.Equals(TEXT("sequence_get_properties")))
    {
        FString SeqPath = ResolveSequencePath(); if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("sequence_get_properties requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
#if WITH_EDITOR
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, SeqPath, RequestingSocket, SerializeResponseAndSend]() {
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
            if (!SeqObj) { SerializeResponseAndSend(false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return; }
#if defined(MCP_HAS_LEVELSEQUENCE) && defined(MCP_HAS_MOVIESCENE)
            ULevelSequence* LevelSeq = Cast<ULevelSequence>(SeqObj);
            if (LevelSeq && LevelSeq->GetMovieScene())
            {
                UMovieScene* MovieScene = LevelSeq->GetMovieScene();
                // Frame rate
                FFrameRate FR = MovieScene->GetDisplayRate();
                TSharedPtr<FJsonObject> FrameRateObj = MakeShared<FJsonObject>();
                FrameRateObj->SetNumberField(TEXT("numerator"), FR.Numerator);
                FrameRateObj->SetNumberField(TEXT("denominator"), FR.Denominator);
                Resp->SetObjectField(TEXT("frameRate"), FrameRateObj);
                // Playback range
                TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
                double Start = (double)Range.GetLowerBoundValue().Value;
                double End = (double)Range.GetUpperBoundValue().Value;
                Resp->SetNumberField(TEXT("playbackStart"), Start);
                Resp->SetNumberField(TEXT("playbackEnd"), End);
                Resp->SetNumberField(TEXT("duration"), End - Start);
                SerializeResponseAndSend(true, TEXT("properties retrieved"), Resp, FString());
                return;
            }
#endif
            // Fallback: minimal response
            Resp->SetObjectField(TEXT("frameRate"), MakeShared<FJsonObject>());
            Resp->SetNumberField(TEXT("playbackStart"), 0.0);
            Resp->SetNumberField(TEXT("playbackEnd"), 0.0);
            Resp->SetNumberField(TEXT("duration"), 0.0);
            SerializeResponseAndSend(true, TEXT("properties retrieved (fallback)"), Resp, FString());
        });
        return true;
#else
        SerializeResponseAndSend(false, TEXT("sequence_get_properties requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // Set playback speed for the current sequence (editor-only, best-effort)
    if (Lower.Equals(TEXT("sequence_set_playback_speed")))
    {
        double Speed = 1.0; if (LocalPayload->HasField(TEXT("speed"))) LocalPayload->TryGetNumberField(TEXT("speed"), Speed);
        if (Speed <= 0.0) { SerializeResponseAndSend(false, TEXT("Invalid speed (must be > 0)"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
        FString SeqPath = ResolveSequencePath(); if (SeqPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("sequence_set_playback_speed requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }

        // Update registry immediately
        if (TSharedPtr<FJsonObject>* Found = GSequenceRegistry.Find(SeqPath)) { (*Found)->SetNumberField(TEXT("playbackSpeed"), Speed); }

#if WITH_EDITOR
    AsyncTask(ENamedThreads::GameThread, [this, RequestId, SeqPath, Speed, RequestingSocket, SerializeResponseAndSend]() {
            // Update registry immediately (best-effort) and attempt native application when possible
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            if (TSharedPtr<FJsonObject>* Found = GSequenceRegistry.Find(SeqPath)) { (*Found)->SetNumberField(TEXT("playbackSpeed"), Speed); }
            UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
            if (!SeqObj) { SerializeResponseAndSend(true, TEXT("Playback speed recorded (registry)."), Resp, FString()); return; }
            Resp->SetBoolField(TEXT("applied"), false);
            Resp->SetNumberField(TEXT("speed"), Speed);
            SerializeResponseAndSend(true, TEXT("Playback speed recorded (registry)."), Resp, FString());
        });
        return true;
#else
        SerializeResponseAndSend(false, TEXT("sequence_set_playback_speed requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // Unknown sequence action
    SerializeResponseAndSend(false, FString::Printf(TEXT("Sequence action not implemented by plugin: %s"), *Action), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
}
