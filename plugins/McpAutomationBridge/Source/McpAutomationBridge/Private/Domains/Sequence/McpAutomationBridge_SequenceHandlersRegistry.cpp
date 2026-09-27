#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

FString McpSequence::ResolvePath(const TSharedPtr<FJsonObject> &Payload) {
  FString Path;
  if (Payload.IsValid()) {
    for (const TCHAR *Field :
         {TEXT("path"), TEXT("sequencePath"), TEXT("assetPath")}) {
      if (Payload->TryGetStringField(Field, Path) && !Path.IsEmpty())
        break;
      Path.Reset();
    }
  }
  if (!Path.IsEmpty()) {
    if (UEditorAssetLibrary::DoesAssetExist(Path)) {
      UObject *Obj = UEditorAssetLibrary::LoadAsset(Path);
      if (Obj) {
        return Obj->GetPathName();
      }
    }
    return Path;
  }
  if (!GCurrentSequencePath.IsEmpty())
    return GCurrentSequencePath;
  return FString();
}

ULevelSequence *McpSequence::LoadOrReply(UMcpAutomationBridgeSubsystem *Subsystem, const FString &RequestId,
                                         TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject> &Payload,
                                         const TCHAR *Action, UMovieScene *&OutMovieScene) {
  OutMovieScene = nullptr;
  const FString Path = ResolvePath(Payload);
  if (Path.IsEmpty()) {
    Subsystem->SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("%s requires a sequence path"), Action), nullptr, TEXT("INVALID_SEQUENCE"));
    return nullptr;
  }
  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
  if (!Sequence) {
    Subsystem->SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Level sequence not found: %s"), *Path), nullptr, TEXT("SEQUENCE_NOT_FOUND"));
    return nullptr;
  }
  OutMovieScene = Sequence->GetMovieScene();
  if (!OutMovieScene) {
    Subsystem->SendAutomationResponse(Socket, RequestId, false,
        TEXT("MovieScene not available"), nullptr, TEXT("MOVIESCENE_UNAVAILABLE"));
    return nullptr;
  }
  return Sequence;
}

ULevelSequence *McpSequence::CreateSequenceAsset(const FString &Name, const FString &Folder) {
  UClass *FactoryClass = LoadClass<UFactory>(nullptr, TEXT("/Script/LevelSequenceEditor.LevelSequenceFactoryNew"));
  if (!FactoryClass) {
    return nullptr;
  }
  UFactory *Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
  return Cast<ULevelSequence>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
                                  .Get()
                                  .CreateAsset(Name, Folder, ULevelSequence::StaticClass(), Factory));
}

FString UMcpAutomationBridgeSubsystem::ResolveSequencePath(
    const TSharedPtr<FJsonObject> &Payload) {
  return McpSequence::ResolvePath(Payload);
}

