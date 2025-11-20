#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "LevelSequence.h"
#include "MovieSceneSequence.h"

#if __has_include("Misc/ScopedTransaction.h")
#include "Misc/ScopedTransaction.h"
#define MCP_HAS_SCOPED_TRANSACTION 1
#else
#define MCP_HAS_SCOPED_TRANSACTION 0
#endif

#if WITH_EDITOR
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
#if __has_include("Factories/LevelSequenceFactoryNew.h")
#include "Factories/LevelSequenceFactoryNew.h"
#define MCP_HAS_LEVELSEQUENCE_FACTORY 1
#else
#define MCP_HAS_LEVELSEQUENCE_FACTORY 0
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

FString UMcpAutomationBridgeSubsystem::ResolveSequencePath(const TSharedPtr<FJsonObject>& Payload)
{
    FString Path;
    if (Payload.IsValid() && Payload->TryGetStringField(TEXT("path"), Path) && !Path.IsEmpty())
    {
#if WITH_EDITOR
        UObject* Obj = UEditorAssetLibrary::LoadAsset(Path);
        if (Obj)
        {
            FString Norm = Obj->GetPathName();
            if (Norm.Contains(TEXT(".")))
            {
                Norm = Norm.Left(Norm.Find(TEXT(".")));
            }
            return Norm;
        }
#endif
        return Path;
    }
    if (!GCurrentSequencePath.IsEmpty()) return GCurrentSequencePath;
    return FString();
}

TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::EnsureSequenceEntry(const FString& SeqPath)
{
    if (SeqPath.IsEmpty()) return nullptr;
    if (TSharedPtr<FJsonObject>* Found = GSequenceRegistry.Find(SeqPath)) return *Found;
    TSharedPtr<FJsonObject> NewObj = MakeShared<FJsonObject>();
    NewObj->SetStringField(TEXT("sequencePath"), SeqPath);
    GSequenceRegistry.Add(SeqPath, NewObj);
    return NewObj;
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceCreate(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    FString Name; LocalPayload->TryGetStringField(TEXT("name"), Name);
    FString Path; LocalPayload->TryGetStringField(TEXT("path"), Path);
    if (Name.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_create requires name"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
    FString FullPath = Path.IsEmpty() ? FString::Printf(TEXT("/Game/%s"), *Name) : FString::Printf(TEXT("%s/%s"), *Path, *Name);

#if WITH_EDITOR && defined(MCP_HAS_LEVELSEQUENCE) && MCP_HAS_LEVELSEQUENCE && defined(MCP_HAS_LEVELSEQUENCE_FACTORY) && MCP_HAS_LEVELSEQUENCE_FACTORY
    FString DestFolder = Path.IsEmpty() ? TEXT("/Game") : Path;
    if (DestFolder.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase))
    {
        DestFolder = FString::Printf(TEXT("/Game%s"), *DestFolder.RightChop(8));
    }

    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetStringField(TEXT("sequencePath"), FullPath);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence already exists"), Resp, FString());
        return true;
    }

    ULevelSequenceFactoryNew* Factory = NewObject<ULevelSequenceFactoryNew>();
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    UObject* NewObj = AssetToolsModule.Get().CreateAsset(Name, DestFolder, ULevelSequence::StaticClass(), Factory);
    if (NewObj)
    {
        UEditorAssetLibrary::SaveAsset(FullPath);
        GCurrentSequencePath = FullPath;
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetStringField(TEXT("sequencePath"), FullPath);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence created"), Resp, FString());
    }
    else
    {
        SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create sequence asset"), nullptr, TEXT("CREATE_ASSET_FAILED"));
    }
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_create requires editor build"), nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetProperties(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_set_properties requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }

#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
    if (!SeqObj) { SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }

#if defined(MCP_HAS_LEVELSEQUENCE) && defined(MCP_HAS_MOVIESCENE)
    if (ULevelSequence* LevelSeq = Cast<ULevelSequence>(SeqObj))
    {
        if (UMovieScene* MovieScene = LevelSeq->GetMovieScene())
        {
            bool bModified = false;
            double FrameRateValue = 0.0;
            double LengthInFramesValue = 0.0;
            double PlaybackStartValue = 0.0;
            double PlaybackEndValue = 0.0;

            const bool bHasFrameRate = LocalPayload->TryGetNumberField(TEXT("frameRate"), FrameRateValue);
            const bool bHasLengthInFrames = LocalPayload->TryGetNumberField(TEXT("lengthInFrames"), LengthInFramesValue);
            const bool bHasPlaybackStart = LocalPayload->TryGetNumberField(TEXT("playbackStart"), PlaybackStartValue);
            const bool bHasPlaybackEnd = LocalPayload->TryGetNumberField(TEXT("playbackEnd"), PlaybackEndValue);

            if (bHasFrameRate)
            {
                if (FrameRateValue <= 0.0) { SendAutomationResponse(Socket, RequestId, false, TEXT("frameRate must be > 0"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
                const int32 Rounded = FMath::Clamp<int32>(FMath::RoundToInt(FrameRateValue), 1, 960);
                FFrameRate CurrentRate = MovieScene->GetDisplayRate();
                FFrameRate NewRate(Rounded, 1);
                if (NewRate != CurrentRate)
                {
                    MovieScene->SetDisplayRate(NewRate);
                    bModified = true;
                }
            }

            if (bHasPlaybackStart || bHasPlaybackEnd || bHasLengthInFrames)
            {
                TRange<FFrameNumber> ExistingRange = MovieScene->GetPlaybackRange();
                FFrameNumber StartFrame = ExistingRange.GetLowerBoundValue();
                FFrameNumber EndFrame = ExistingRange.GetUpperBoundValue();

                if (bHasPlaybackStart) StartFrame = FFrameNumber(static_cast<int32>(PlaybackStartValue));
                if (bHasPlaybackEnd) EndFrame = FFrameNumber(static_cast<int32>(PlaybackEndValue));
                else if (bHasLengthInFrames) EndFrame = StartFrame + FMath::Max<int32>(0, static_cast<int32>(LengthInFramesValue));

                if (EndFrame < StartFrame) EndFrame = StartFrame;
                MovieScene->SetPlaybackRange(TRange<FFrameNumber>(StartFrame, EndFrame));
                bModified = true;
            }

            if (bModified) MovieScene->Modify();

            FFrameRate FR = MovieScene->GetDisplayRate();
            TSharedPtr<FJsonObject> FrameRateObj = MakeShared<FJsonObject>();
            FrameRateObj->SetNumberField(TEXT("numerator"), FR.Numerator);
            FrameRateObj->SetNumberField(TEXT("denominator"), FR.Denominator);
            Resp->SetObjectField(TEXT("frameRate"), FrameRateObj);

            TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
            const double Start = static_cast<double>(Range.GetLowerBoundValue().Value);
            const double End = static_cast<double>(Range.GetUpperBoundValue().Value);
            Resp->SetNumberField(TEXT("playbackStart"), Start);
            Resp->SetNumberField(TEXT("playbackEnd"), End);
            Resp->SetNumberField(TEXT("duration"), End - Start);
            Resp->SetBoolField(TEXT("applied"), bModified);

            SendAutomationResponse(Socket, RequestId, true, TEXT("properties updated"), Resp, FString());
            return true;
        }
    }
#endif
    Resp->SetObjectField(TEXT("frameRate"), MakeShared<FJsonObject>());
    Resp->SetNumberField(TEXT("playbackStart"), 0.0);
    Resp->SetNumberField(TEXT("playbackEnd"), 0.0);
    Resp->SetNumberField(TEXT("duration"), 0.0);
    Resp->SetBoolField(TEXT("applied"), false);
    SendAutomationResponse(Socket, RequestId, true, TEXT("properties updated (no-op)"), Resp, FString());
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_set_properties requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceOpen(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_open requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }

#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
    if (!SeqObj) { SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }

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
                    SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence opened"), Resp, FString());
                    return true;
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
    SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence opened"), Resp, FString());
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_open requires editor build."), nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddCamera(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_add_camera requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }

#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
    if (!SeqObj) { SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }

    if (GEditor)
    {
        if (UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
        {
            UClass* CameraClass = ACameraActor::StaticClass();
            AActor* Spawned = ActorSS->SpawnActorFromClass(CameraClass, FVector::ZeroVector, FRotator::ZeroRotator);
            if (Spawned)
            {
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("actorLabel"), Spawned->GetActorLabel());
                SendAutomationResponse(Socket, RequestId, true, TEXT("Camera actor spawned (not bound to sequence)"), Resp, FString());
                return true;
            }
        }
    }
    SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to add camera"), nullptr, TEXT("ADD_CAMERA_FAILED"));
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_add_camera requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequencePlay(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("No sequence selected or path provided"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
    TSharedPtr<FJsonObject> Entry = EnsureSequenceEntry(SeqPath);
    if (Entry.IsValid()) Entry->SetBoolField(TEXT("playing"), true);
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("sequencePath"), SeqPath);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence play (registry)."), Resp, FString());
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddActor(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    FString ActorName; LocalPayload->TryGetStringField(TEXT("actorName"), ActorName);
    if (ActorName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_add_actor requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }

#if WITH_EDITOR
    if (GEditor)
    {
        if (UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
        {
            AActor* Found = nullptr;
            TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
            for (AActor* A : AllActors)
            {
                if (A && A->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase)) { Found = A; break; }
            }
            if (!Found) { SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("ACTOR_NOT_FOUND")); return true; }
            
            // Implementation of binding logic would go here if possible
            SendAutomationResponse(Socket, RequestId, false, TEXT("Binding actors to sequences not available in this editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
            return true;
        }
    }
    SendAutomationResponse(Socket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_add_actor requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddActors(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr; LocalPayload->TryGetArrayField(TEXT("actorNames"), Arr);
    if (!Arr || Arr->Num() == 0) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorNames required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_add_actors requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }

#if WITH_EDITOR
    TArray<FString> Names; Names.Reserve(Arr->Num());
    for (const TSharedPtr<FJsonValue>& V : *Arr) { if (V.IsValid() && V->Type == EJson::String) Names.Add(V->AsString()); }

    UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
    if (!SeqObj) { SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
    if (!GEditor) { SendAutomationResponse(Socket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return true; }

    if (UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
    {
        TArray<TSharedPtr<FJsonValue>> Results; Results.Reserve(Names.Num());
        for (const FString& Name : Names)
        {
            TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("name"), Name);
            AActor* Found = nullptr;
            TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
            for (AActor* A : AllActors) { if (A && A->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase)) { Found = A; break; } }

            if (!Found) { Item->SetBoolField(TEXT("success"), false); Item->SetStringField(TEXT("error"), TEXT("Actor not found")); }
            else
            {
                if (ULevelSequence* LevelSeq = Cast<ULevelSequence>(SeqObj))
                {
                    UMovieScene* MovieScene = LevelSeq->GetMovieScene();
                    if (MovieScene)
                    {
                        FGuid BindingGuid = MovieScene->AddPossessable(Found->GetName(), Found->GetClass());
                        if (MovieScene->FindPossessable(BindingGuid))
                        {
                            Item->SetBoolField(TEXT("success"), true);
                            Item->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());
                            MovieScene->Modify();
                        }
                        else { Item->SetBoolField(TEXT("success"), false); Item->SetStringField(TEXT("error"), TEXT("Failed to create possessable binding")); }
                    }
                    else { Item->SetBoolField(TEXT("success"), false); Item->SetStringField(TEXT("error"), TEXT("Sequence has no MovieScene")); }
                }
                else { Item->SetBoolField(TEXT("success"), false); Item->SetStringField(TEXT("error"), TEXT("Sequence object is not a LevelSequence")); }
            }
            Results.Add(MakeShared<FJsonValueObject>(Item));
        }
        TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
        Out->SetArrayField(TEXT("results"), Results);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Actors processed"), Out, FString());
        return true;
    }
    SendAutomationResponse(Socket, RequestId, false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_add_actors requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddSpawnable(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    FString ClassName; LocalPayload->TryGetStringField(TEXT("className"), ClassName);
    if (ClassName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("className required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_add_spawnable_from_class requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }

#if WITH_EDITOR
    UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
    if (!SeqObj) { SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }

    UClass* ResolvedClass = nullptr;
    if (ClassName.StartsWith(TEXT("/")) || ClassName.Contains(TEXT("/")))
    {
        if (UObject* Loaded = UEditorAssetLibrary::LoadAsset(ClassName))
        {
            if (UBlueprint* BP = Cast<UBlueprint>(Loaded)) ResolvedClass = BP->GeneratedClass;
            else if (UClass* C = Cast<UClass>(Loaded)) ResolvedClass = C;
        }
    }
    if (!ResolvedClass) ResolvedClass = ResolveClassByName(ClassName);
    if (!ResolvedClass) { SendAutomationResponse(Socket, RequestId, false, TEXT("Class not found"), nullptr, TEXT("CLASS_NOT_FOUND")); return true; }

    if (ULevelSequence* LevelSeq = Cast<ULevelSequence>(SeqObj))
    {
        UMovieScene* MovieScene = LevelSeq->GetMovieScene();
        if (MovieScene)
        {
            UObject* DefaultObject = ResolvedClass->GetDefaultObject();
            if (DefaultObject)
            {
                FGuid BindingGuid = MovieScene->AddSpawnable(ClassName, *DefaultObject);
                if (MovieScene->FindSpawnable(BindingGuid))
                {
                    MovieScene->Modify();
                    TSharedPtr<FJsonObject> SpawnableResp = MakeShared<FJsonObject>();
                    SpawnableResp->SetBoolField(TEXT("success"), true);
                    SpawnableResp->SetStringField(TEXT("className"), ClassName);
                    SpawnableResp->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());
                    SendAutomationResponse(Socket, RequestId, true, TEXT("Spawnable added to sequence"), SpawnableResp, FString());
                    return true;
                }
            }
        }
        SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create spawnable binding"), nullptr, TEXT("SPAWNABLE_CREATION_FAILED"));
        return true;
    }
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence object is not a LevelSequence"), nullptr, TEXT("INVALID_SEQUENCE_TYPE"));
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_add_spawnable_from_class requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceRemoveActors(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr; LocalPayload->TryGetArrayField(TEXT("actorNames"), Arr);
    if (!Arr || Arr->Num() == 0) { SendAutomationResponse(Socket, RequestId, false, TEXT("actorNames required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_remove_actors requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }

#if WITH_EDITOR
    UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
    if (!SeqObj) { SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
    if (!GEditor) { SendAutomationResponse(Socket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return true; }

    if (UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
    {
        TArray<TSharedPtr<FJsonValue>> Removed;
        int32 RemovedCount = 0;
        for (const TSharedPtr<FJsonValue>& V : *Arr)
        {
            if (!V.IsValid() || V->Type != EJson::String) continue;
            FString Name = V->AsString();
            TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("name"), Name);

            if (ULevelSequence* LevelSeq = Cast<ULevelSequence>(SeqObj))
            {
                UMovieScene* MovieScene = LevelSeq->GetMovieScene();
                if (MovieScene)
                {
                    bool bRemoved = false;
                    for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
                    {
                        if (Binding.GetName().Equals(Name, ESearchCase::IgnoreCase))
                        {
                            MovieScene->RemovePossessable(Binding.GetObjectGuid());
                            MovieScene->Modify();
                            bRemoved = true;
                            break;
                        }
                    }
                    if (bRemoved) { Item->SetBoolField(TEXT("success"), true); Item->SetStringField(TEXT("status"), TEXT("Actor removed")); RemovedCount++; }
                    else { Item->SetBoolField(TEXT("success"), false); Item->SetStringField(TEXT("error"), TEXT("Actor not found in sequence bindings")); }
                }
                else { Item->SetBoolField(TEXT("success"), false); Item->SetStringField(TEXT("error"), TEXT("Sequence has no MovieScene")); }
            }
            else { Item->SetBoolField(TEXT("success"), false); Item->SetStringField(TEXT("error"), TEXT("Sequence object is not a LevelSequence")); }
            Removed.Add(MakeShared<FJsonValueObject>(Item));
        }
        TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
        Out->SetArrayField(TEXT("removedActors"), Removed);
        Out->SetNumberField(TEXT("bindingsProcessed"), RemovedCount);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Actors processed for removal"), Out, FString());
        return true;
    }
    SendAutomationResponse(Socket, RequestId, false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_remove_actors requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceGetBindings(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_get_bindings requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
    if (!SeqObj) { SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
#if defined(MCP_HAS_MOVIESCENE) && defined(MCP_HAS_LEVELSEQUENCE)
    if (ULevelSequence* LevelSeq = Cast<ULevelSequence>(SeqObj))
    {
        if (UMovieScene* MovieScene = LevelSeq->GetMovieScene())
        {
            TArray<TSharedPtr<FJsonValue>> BindingsArray;
            for (const FMovieSceneBinding& B : MovieScene->GetBindings())
            {
                TSharedPtr<FJsonObject> Bobj = MakeShared<FJsonObject>();
                Bobj->SetStringField(TEXT("id"), B.GetObjectGuid().ToString());
                Bobj->SetStringField(TEXT("name"), B.GetName());
                BindingsArray.Add(MakeShared<FJsonValueObject>(Bobj));
            }
            Resp->SetArrayField(TEXT("bindings"), BindingsArray);
            SendAutomationResponse(Socket, RequestId, true, TEXT("bindings listed"), Resp, FString());
            return true;
        }
    }
#endif
    Resp->SetArrayField(TEXT("bindings"), TArray<TSharedPtr<FJsonValue>>());
    SendAutomationResponse(Socket, RequestId, true, TEXT("bindings listed (empty)"), Resp, FString());
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_get_bindings requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceGetProperties(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_get_properties requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
    if (!SeqObj) { SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
#if defined(MCP_HAS_LEVELSEQUENCE) && defined(MCP_HAS_MOVIESCENE)
    if (ULevelSequence* LevelSeq = Cast<ULevelSequence>(SeqObj))
    {
        if (UMovieScene* MovieScene = LevelSeq->GetMovieScene())
        {
            FFrameRate FR = MovieScene->GetDisplayRate();
            TSharedPtr<FJsonObject> FrameRateObj = MakeShared<FJsonObject>();
            FrameRateObj->SetNumberField(TEXT("numerator"), FR.Numerator);
            FrameRateObj->SetNumberField(TEXT("denominator"), FR.Denominator);
            Resp->SetObjectField(TEXT("frameRate"), FrameRateObj);
            TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
            const double Start = static_cast<double>(Range.GetLowerBoundValue().Value);
            const double End = static_cast<double>(Range.GetUpperBoundValue().Value);
            Resp->SetNumberField(TEXT("playbackStart"), Start);
            Resp->SetNumberField(TEXT("playbackEnd"), End);
            Resp->SetNumberField(TEXT("duration"), End - Start);
            SendAutomationResponse(Socket, RequestId, true, TEXT("properties retrieved"), Resp, FString());
            return true;
        }
    }
#endif
    Resp->SetObjectField(TEXT("frameRate"), MakeShared<FJsonObject>());
    Resp->SetNumberField(TEXT("playbackStart"), 0.0);
    Resp->SetNumberField(TEXT("playbackEnd"), 0.0);
    Resp->SetNumberField(TEXT("duration"), 0.0);
    SendAutomationResponse(Socket, RequestId, true, TEXT("properties retrieved"), Resp, FString());
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_get_properties requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetPlaybackSpeed(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    double Speed = 1.0; LocalPayload->TryGetNumberField(TEXT("speed"), Speed);
    if (Speed <= 0.0) { SendAutomationResponse(Socket, RequestId, false, TEXT("Invalid speed (must be > 0)"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_set_playback_speed requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }

#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
    if (!SeqObj) { SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
    Resp->SetBoolField(TEXT("applied"), false);
    Resp->SetNumberField(TEXT("speed"), Speed);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Playback speed updated"), Resp, FString());
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_set_playback_speed requires editor build."), nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequencePause(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_pause requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
    if (!SeqObj) { SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
    bool bControlled = false;
    Resp->SetBoolField(TEXT("paused"), true);
    Resp->SetBoolField(TEXT("controlled"), bControlled);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence paused"), Resp, FString());
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_pause requires editor build."), nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceStop(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_stop requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
    if (!SeqObj) { SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
    bool bControlled = false;
    Resp->SetBoolField(TEXT("stopped"), true);
    Resp->SetBoolField(TEXT("controlled"), bControlled);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence stopped"), Resp, FString());
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_stop requires editor build."), nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceList(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> SequencesArray;
    TArray<FString> SequencePaths = UEditorAssetLibrary::ListAssets(TEXT("/Game"), true, true);
    for (const FString& Path : SequencePaths)
    {
        if (Path.EndsWith(TEXT(".LevelSequence")))
        {
            TSharedPtr<FJsonObject> SeqObj = MakeShared<FJsonObject>();
            SeqObj->SetStringField(TEXT("path"), Path);
            SequencesArray.Add(MakeShared<FJsonValueObject>(SeqObj));
        }
    }
    Resp->SetArrayField(TEXT("sequences"), SequencesArray);
    Resp->SetNumberField(TEXT("count"), SequencesArray.Num());
    SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Found %d sequences"), SequencesArray.Num()), Resp, FString());
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_list requires editor build."), nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceDuplicate(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    FString SourcePath; LocalPayload->TryGetStringField(TEXT("path"), SourcePath);
    FString DestinationPath; LocalPayload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
    if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_duplicate requires path and destinationPath"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
#if WITH_EDITOR
    UObject* SourceSeq = UEditorAssetLibrary::LoadAsset(SourcePath);
    if (!SourceSeq) { SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("Source sequence not found: %s"), *SourcePath), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
    UObject* DuplicatedSeq = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath);
    if (DuplicatedSeq)
    {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetStringField(TEXT("sourcePath"), SourcePath);
        Resp->SetStringField(TEXT("destinationPath"), DestinationPath);
        Resp->SetStringField(TEXT("duplicatedPath"), DuplicatedSeq->GetPathName());
        SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence duplicated successfully"), Resp, FString());
        return true;
    }
    SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to duplicate sequence"), nullptr, TEXT("OPERATION_FAILED"));
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_duplicate requires editor build."), nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceRename(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    FString Path; LocalPayload->TryGetStringField(TEXT("path"), Path);
    FString NewName; LocalPayload->TryGetStringField(TEXT("newName"), NewName);
    if (Path.IsEmpty() || NewName.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_rename requires path and newName"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
#if WITH_EDITOR
    if (UEditorAssetLibrary::RenameAsset(Path, NewName))
    {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetStringField(TEXT("oldPath"), Path);
        Resp->SetStringField(TEXT("newName"), NewName);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence renamed successfully"), Resp, FString());
        return true;
    }
    SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to rename sequence"), nullptr, TEXT("OPERATION_FAILED"));
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_rename requires editor build."), nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceDelete(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    FString Path; LocalPayload->TryGetStringField(TEXT("path"), Path);
    if (Path.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_delete requires path"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
#if WITH_EDITOR
    if (UEditorAssetLibrary::DeleteAsset(Path))
    {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetStringField(TEXT("deletedPath"), Path);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence deleted successfully"), Resp, FString());
        return true;
    }
    SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to delete sequence"), nullptr, TEXT("OPERATION_FAILED"));
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_delete requires editor build."), nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceGetMetadata(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) { SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_get_metadata requires a sequence path"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
#if WITH_EDITOR
    UObject* SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
    if (!SeqObj) { SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"), nullptr, TEXT("INVALID_SEQUENCE")); return true; }
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("path"), SeqPath);
    Resp->SetStringField(TEXT("name"), SeqObj->GetName());
    Resp->SetStringField(TEXT("class"), SeqObj->GetClass()->GetName());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence metadata retrieved"), Resp, FString());
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("sequence_get_metadata requires editor build."), nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.StartsWith(TEXT("sequence_"))) return false;

    if (Lower.Equals(TEXT("sequence_create"))) return HandleSequenceCreate(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_set_properties"))) return HandleSequenceSetProperties(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_open"))) return HandleSequenceOpen(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_add_camera"))) return HandleSequenceAddCamera(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_play"))) return HandleSequencePlay(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_add_actor"))) return HandleSequenceAddActor(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_add_actors"))) return HandleSequenceAddActors(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_add_spawnable_from_class"))) return HandleSequenceAddSpawnable(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_remove_actors"))) return HandleSequenceRemoveActors(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_get_bindings"))) return HandleSequenceGetBindings(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_get_properties"))) return HandleSequenceGetProperties(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_set_playback_speed"))) return HandleSequenceSetPlaybackSpeed(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_pause"))) return HandleSequencePause(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_stop"))) return HandleSequenceStop(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_list"))) return HandleSequenceList(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_duplicate"))) return HandleSequenceDuplicate(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_rename"))) return HandleSequenceRename(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_delete"))) return HandleSequenceDelete(RequestId, Payload, RequestingSocket);
    if (Lower.Equals(TEXT("sequence_get_metadata"))) return HandleSequenceGetMetadata(RequestId, Payload, RequestingSocket);

    SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Sequence action not implemented by plugin: %s"), *Action), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
}
