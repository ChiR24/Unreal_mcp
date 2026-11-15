#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"

class FMcpBridgeWebSocket;
class UClass;
class UObject;
class UEngineWireSubsystem;

namespace McpSequenceMedia {

struct FMediaAssetCreateResult {
  UObject *Object = nullptr;
  FString PackageName;
  FString ObjectPath;
  bool bCreated = false;
  bool bSaved = false;
};

bool TryHandleMediaAction(UEngineWireSubsystem *Subsystem,
                          const FString &RequestId,
                          const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleCreateMediaPlayer(UEngineWireSubsystem *Subsystem,
                             const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateMediaSource(UEngineWireSubsystem *Subsystem,
                             const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateMediaTexture(UEngineWireSubsystem *Subsystem,
                              const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateMediaSoundComponent(UEngineWireSubsystem *Subsystem,
                                     const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateMediaPlaylist(UEngineWireSubsystem *Subsystem,
                               const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandlePlayMedia(UEngineWireSubsystem *Subsystem,
                     const FString &RequestId,
                     const TSharedPtr<FJsonObject> &Payload,
                     TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandlePauseMedia(UEngineWireSubsystem *Subsystem,
                      const FString &RequestId,
                      const TSharedPtr<FJsonObject> &Payload,
                      TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSeekMedia(UEngineWireSubsystem *Subsystem,
                     const FString &RequestId,
                     const TSharedPtr<FJsonObject> &Payload,
                     TSharedPtr<FMcpBridgeWebSocket> Socket);

FString GetStringAny(const TSharedPtr<FJsonObject> &Payload,
                     std::initializer_list<const TCHAR *> Fields,
                     const FString &DefaultValue = FString());
bool GetBoolAny(const TSharedPtr<FJsonObject> &Payload,
                std::initializer_list<const TCHAR *> Fields,
                bool DefaultValue = false);
double GetNumberAny(const TSharedPtr<FJsonObject> &Payload,
                    std::initializer_list<const TCHAR *> Fields,
                    double DefaultValue = 0.0);
void SendMediaError(UEngineWireSubsystem *Subsystem,
                    TSharedPtr<FMcpBridgeWebSocket> Socket,
                    const FString &RequestId,
                    const FString &ErrorCode,
                    const FString &Message,
                    const TSharedPtr<FJsonObject> &Details = nullptr);
bool ResolveMediaAssetIdentity(const TSharedPtr<FJsonObject> &Payload,
                               const FString &DefaultFolder,
                               const FString &DefaultName,
                               FString &OutPackageName,
                               FString &OutAssetName,
                               FString &OutObjectPath,
                               FString &OutError);
bool CreateMediaAssetFromPayload(const TSharedPtr<FJsonObject> &Payload,
                                 const FString &DefaultFolder,
                                 const FString &ClassName,
                                 FMediaAssetCreateResult &OutCreated,
                                 FString &OutError, FString &OutCode);
UClass *ResolveMediaClass(const FString &ClassName, FString &OutError);
UObject *LoadMediaObject(const FString &ObjectPath,
                         UClass *ExpectedClass,
                         FString &OutResolvedPath,
                         FString &OutError);
// OutCode: MEDIA_ASSET_ALREADY_EXISTS for a media asset of that class already there, else MEDIA_ASSET_CREATE_FAILED.
bool CreateMediaAsset(UClass *AssetClass,
                      const FString &PackageName,
                      const FString &AssetName,
                      FMediaAssetCreateResult &OutCreated,
                      FString &OutError, FString &OutCode);
void DiscardCreatedMediaAsset(FMediaAssetCreateResult &Created);
TSharedPtr<FJsonObject> BuildAssetResponse(
    const FMediaAssetCreateResult &Created,
    const FString &AssetType);
bool SaveMediaAsset(UObject *Object);
// Writes by reflection, for members that are protected or private (McpSeqMediaProperties.cpp).
bool SetObjectProperty(UObject *Object,
                       const FString &PropertyName,
                       UObject *Value);
bool SetBoolProperty(UObject *Object,
                     const FString &PropertyName,
                     bool Value);
// The bool property's value as Object holds it; false when it has none by that name.
bool GetBoolProperty(const UObject *Object, const FString &PropertyName);
bool SetStringProperty(UObject *Object,
                       const FString &PropertyName,
                       const FString &Value);
bool SetStringObjectMapEntry(UObject *Object,
                             const FString &PropertyName,
                             const FString &Key,
                             UObject *Value);
bool ValidateMediaSourcePolicy(UObject *MediaSource, FString &OutErrorCode,
                               FString &OutError);

}
