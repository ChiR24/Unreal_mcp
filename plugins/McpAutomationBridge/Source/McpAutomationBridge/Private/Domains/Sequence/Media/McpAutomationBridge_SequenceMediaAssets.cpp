#include "Domains/Sequence/Media/McpAutomationBridge_SequenceMedia.h"

#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"

namespace McpSequenceMedia {
namespace {

bool SaveCreatedAsset(UMcpAutomationBridgeSubsystem *Subsystem,
                      const FString &RequestId,
                      TSharedPtr<FMcpBridgeWebSocket> Socket,
                      FMediaAssetCreateResult &Created,
                      const FString &AssetType,
                      TSharedPtr<FJsonObject> &OutResult) {
  Created.Object->Modify();
  Created.Object->MarkPackageDirty();
  Created.bSaved = SaveMediaAsset(Created.Object);
  OutResult = BuildAssetResponse(Created, AssetType);
  if (!Created.bSaved) {
    DiscardCreatedMediaAsset(Created);
    SendMediaError(Subsystem, Socket, RequestId, TEXT("ASSET_SAVE_FAILED"),
                   TEXT("Media asset was created in memory but could not be saved"),
                   OutResult);
    return false;
  }
  return true;
}

bool CreateAssetForAction(UMcpAutomationBridgeSubsystem *Subsystem,
                          const FString &RequestId,
                          TSharedPtr<FMcpBridgeWebSocket> Socket,
                          const TSharedPtr<FJsonObject> &Payload,
                          const FString &DefaultFolder,
                          const FString &ClassName,
                          FMediaAssetCreateResult &OutCreated) {
  FString PackageName;
  FString AssetName;
  FString ObjectPath;
  FString Error;
  if (!ResolveMediaAssetIdentity(Payload, DefaultFolder, FString(),
                                 PackageName, AssetName, ObjectPath, Error)) {
    SendMediaError(Subsystem, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                   Error);
    return false;
  }
  UClass *AssetClass = ResolveMediaClass(ClassName, Error);
  if (!AssetClass ||
      !CreateMediaAsset(AssetClass, PackageName, AssetName, OutCreated, Error)) {
    const bool bAlreadyExists =
        Error.Contains(TEXT("[MEDIA_ASSET_ALREADY_EXISTS]"));
    SendMediaError(Subsystem, Socket, RequestId,
                   bAlreadyExists ? TEXT("MEDIA_ASSET_ALREADY_EXISTS")
                   : AssetClass ? TEXT("MEDIA_ASSET_CREATE_FAILED")
                              : TEXT("MEDIA_FRAMEWORK_UNAVAILABLE"),
                   Error);
    return false;
  }
  return true;
}

}

bool HandleCreateMediaPlayer(UMcpAutomationBridgeSubsystem *Subsystem,
                             const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const bool bLoop = GetBoolAny(Payload, {TEXT("loop"), TEXT("looping")});
  const bool bAutoPlay =
      GetBoolAny(Payload, {TEXT("autoPlay"), TEXT("playOnOpen")}, true);
  const FString SourcePath =
      GetStringAny(Payload, {TEXT("mediaSourcePath"), TEXT("sourcePath")});
  FString Error;
  UClass *PlayerClass = ResolveMediaClass(TEXT("MediaPlayer"), Error);
  UClass *SourceClass = SourcePath.IsEmpty()
                            ? nullptr
                            : ResolveMediaClass(TEXT("MediaSource"), Error);
  FString ResolvedSourcePath;
  UObject *Source =
      SourceClass ? LoadMediaObject(SourcePath, SourceClass,
                                    ResolvedSourcePath, Error)
                  : nullptr;
  FString PolicyCode;
  FString PolicyError;
  if (!SourcePath.IsEmpty() &&
      !ValidateMediaSourcePolicy(Source, PolicyCode, PolicyError)) {
    SendMediaError(Subsystem, Socket, RequestId, PolicyCode, PolicyError);
    return true;
  }
  UObject *Prototype =
      PlayerClass ? NewObject<UObject>(GetTransientPackage(), PlayerClass)
                  : nullptr;
  const bool bSourceValid =
      SourcePath.IsEmpty() ||
      (Source && CallBoolFunction(Source, TEXT("Validate")) &&
       CallBoolObjectFunction(Prototype, TEXT("CanPlaySource"), Source) &&
       CallBoolObjectFunction(Prototype, TEXT("OpenSource"), Source));
  if (!Prototype || !bSourceValid) {
    SendMediaError(
        Subsystem, Socket, RequestId,
        PlayerClass ? TEXT("MEDIA_OPEN_FAILED")
                    : TEXT("MEDIA_FRAMEWORK_UNAVAILABLE"),
        Error.IsEmpty()
            ? TEXT("The media player rejected the supplied media source")
            : Error);
    return true;
  }
  CallVoidFunction(Prototype, TEXT("Close"));

  FMediaAssetCreateResult Created;
  if (!CreateAssetForAction(Subsystem, RequestId, Socket, Payload,
                            TEXT("/Game/Media/Players"),
                            TEXT("MediaPlayer"), Created)) {
    return true;
  }
  UObject *Player = Created.Object;
  SetBoolProperty(Player, TEXT("Loop"), bLoop);
  SetBoolProperty(Player, TEXT("PlayOnOpen"), bAutoPlay);

  TSharedPtr<FJsonObject> Result;
  if (!SourcePath.IsEmpty()) {
    if (!CallBoolObjectFunction(Player, TEXT("CanPlaySource"), Source) ||
        !CallBoolObjectFunction(Player, TEXT("OpenSource"), Source)) {
      Result = BuildAssetResponse(Created, TEXT("MediaPlayer"));
      Result->SetStringField(TEXT("mediaSourcePath"), ResolvedSourcePath);
      Result->SetBoolField(TEXT("openRequested"), false);
      DiscardCreatedMediaAsset(Created);
      SendMediaError(
          Subsystem, Socket, RequestId, TEXT("MEDIA_OPEN_FAILED"),
          TEXT("The media player rejected the prevalidated media source"),
          Result);
      return true;
    }
    Result = BuildAssetResponse(Created, TEXT("MediaPlayer"));
    Result->SetStringField(TEXT("mediaSourcePath"), ResolvedSourcePath);
    Result->SetBoolField(TEXT("openRequested"), true);
    Result->SetStringField(TEXT("openStatus"), TEXT("pending"));
  }

  if (!SaveCreatedAsset(Subsystem, RequestId, Socket, Created,
                        TEXT("MediaPlayer"), Result)) {
    return true;
  }
  Result->SetBoolField(TEXT("looping"), bLoop);
  Result->SetBoolField(TEXT("playOnOpen"), bAutoPlay);
  if (!SourcePath.IsEmpty()) {
    Result->SetBoolField(TEXT("openRequested"), true);
    Result->SetStringField(TEXT("openStatus"), TEXT("pending"));
  }
  Subsystem->SendAutomationResponse(
      Socket, RequestId, true,
      SourcePath.IsEmpty()
          ? TEXT("Media player asset created")
          : TEXT("Media player created and source open requested"),
      Result);
  return true;
}

bool HandleCreateMediaTexture(UMcpAutomationBridgeSubsystem *Subsystem,
                              const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString PlayerPath =
      GetStringAny(Payload, {TEXT("mediaPlayerPath"), TEXT("playerPath")});
  FString ResolvedPlayerPath;
  UObject *Player = nullptr;
  if (!PlayerPath.IsEmpty()) {
    FString Error;
    UClass *PlayerClass = ResolveMediaClass(TEXT("MediaPlayer"), Error);
    Player = PlayerClass
                 ? LoadMediaObject(PlayerPath, PlayerClass, ResolvedPlayerPath,
                                   Error)
                 : nullptr;
    if (!Player) {
      SendMediaError(Subsystem, Socket, RequestId,
                     TEXT("MEDIA_PLAYER_NOT_FOUND"), Error);
      return true;
    }
  }

  FMediaAssetCreateResult Created;
  if (!CreateAssetForAction(Subsystem, RequestId, Socket, Payload,
                            TEXT("/Game/Media/Textures"),
                            TEXT("MediaTexture"), Created)) {
    return true;
  }
  UObject *Texture = Created.Object;
  const bool bAutoClear =
      GetBoolAny(Payload, {TEXT("autoClear")}, true);
  SetBoolProperty(Texture, TEXT("AutoClear"), bAutoClear);

  if (Player) {
    if (!SetObjectProperty(Texture, TEXT("MediaPlayer"), Player)) {
      TSharedPtr<FJsonObject> Result =
          BuildAssetResponse(Created, TEXT("MediaTexture"));
      DiscardCreatedMediaAsset(Created);
      SendMediaError(Subsystem, Socket, RequestId, TEXT("MEDIA_BIND_FAILED"),
                     TEXT("Failed to bind media player to media texture"),
                     Result);
      return true;
    }
    CallVoidObjectFunction(Texture, TEXT("SetMediaPlayer"), Player);
  }
  CallVoidFunction(Texture, TEXT("UpdateResource"));

  TSharedPtr<FJsonObject> Result;
  if (!SaveCreatedAsset(Subsystem, RequestId, Socket, Created,
                        TEXT("MediaTexture"), Result)) {
    return true;
  }
  Result->SetBoolField(TEXT("autoClear"), bAutoClear);
  if (!ResolvedPlayerPath.IsEmpty()) {
    Result->SetStringField(TEXT("mediaPlayerPath"), ResolvedPlayerPath);
  }
  Subsystem->SendAutomationResponse(Socket, RequestId, true,
                                    TEXT("Media texture asset created"), Result);
  return true;
}

}
