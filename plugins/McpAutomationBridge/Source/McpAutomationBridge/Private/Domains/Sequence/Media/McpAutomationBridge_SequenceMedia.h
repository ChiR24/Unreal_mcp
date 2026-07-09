#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"

class FMcpBridgeWebSocket;
class UClass;
class UObject;
class UMcpAutomationBridgeSubsystem;

namespace McpSequenceMedia {

struct FMediaAssetCreateResult {
  UObject *Object = nullptr;
  FString PackageName;
  FString ObjectPath;
  bool bCreated = false;
  bool bSaved = false;
};

bool TryHandleMediaAction(UMcpAutomationBridgeSubsystem *Subsystem,
                          const FString &RequestId,
                          const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleCreateMediaPlayer(UMcpAutomationBridgeSubsystem *Subsystem,
                             const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateMediaSource(UMcpAutomationBridgeSubsystem *Subsystem,
                             const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateMediaTexture(UMcpAutomationBridgeSubsystem *Subsystem,
                              const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateMediaSoundComponent(UMcpAutomationBridgeSubsystem *Subsystem,
                                     const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateMediaPlaylist(UMcpAutomationBridgeSubsystem *Subsystem,
                               const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandlePlayMedia(UMcpAutomationBridgeSubsystem *Subsystem,
                     const FString &RequestId,
                     const TSharedPtr<FJsonObject> &Payload,
                     TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandlePauseMedia(UMcpAutomationBridgeSubsystem *Subsystem,
                      const FString &RequestId,
                      const TSharedPtr<FJsonObject> &Payload,
                      TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSeekMedia(UMcpAutomationBridgeSubsystem *Subsystem,
                     const FString &RequestId,
                     const TSharedPtr<FJsonObject> &Payload,
                     TSharedPtr<FMcpBridgeWebSocket> Socket);

FString CanonicalMediaAction(const FString &Action);
FString GetStringAny(const TSharedPtr<FJsonObject> &Payload,
                     std::initializer_list<const TCHAR *> Fields,
                     const FString &DefaultValue = FString());
bool GetBoolAny(const TSharedPtr<FJsonObject> &Payload,
                std::initializer_list<const TCHAR *> Fields,
                bool DefaultValue = false);
double GetNumberAny(const TSharedPtr<FJsonObject> &Payload,
                    std::initializer_list<const TCHAR *> Fields,
                    double DefaultValue = 0.0);
void SendMediaError(UMcpAutomationBridgeSubsystem *Subsystem,
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
                                 FString &OutError);
UClass *ResolveMediaClass(const FString &ClassName, FString &OutError);
UObject *LoadMediaObject(const FString &ObjectPath,
                         UClass *ExpectedClass,
                         FString &OutResolvedPath,
                         FString &OutError);
bool CreateMediaAsset(UClass *AssetClass,
                      const FString &PackageName,
                      const FString &AssetName,
                      FMediaAssetCreateResult &OutCreated,
                      FString &OutError);
void DiscardCreatedMediaAsset(FMediaAssetCreateResult &Created);
TSharedPtr<FJsonObject> BuildAssetResponse(
    const FMediaAssetCreateResult &Created,
    const FString &AssetType);
bool SaveMediaAsset(UObject *Object);
bool SetObjectProperty(UObject *Object,
                       const FString &PropertyName,
                       UObject *Value);
bool SetBoolProperty(UObject *Object,
                     const FString &PropertyName,
                     bool Value);
bool SetStringProperty(UObject *Object,
                       const FString &PropertyName,
                       const FString &Value);
bool SetStringObjectMapEntry(UObject *Object,
                             const FString &PropertyName,
                             const FString &Key,
                             UObject *Value);
bool CallBoolFunction(UObject *Object, const FName &FunctionName);
bool CallBoolObjectFunction(UObject *Object,
                            const FName &FunctionName,
                            UObject *Argument);
bool CallBoolStringFunction(UObject *Object,
                            const FName &FunctionName,
                            const FString &Argument);
bool CallBoolObjectIntFunction(UObject *Object,
                               const FName &FunctionName,
                               UObject *ObjectArgument,
                               int32 IntArgument);
UObject *CallObjectIntFunction(UObject *Object,
                               const FName &FunctionName,
                               int32 IntArgument);
bool CallBoolTimespanFunction(UObject *Object,
                              const FName &FunctionName,
                              const FTimespan &Argument);
bool CallVoidStringFunction(UObject *Object,
                            const FName &FunctionName,
                            const FString &Argument);
void CallVoidObjectFunction(UObject *Object,
                            const FName &FunctionName,
                            UObject *Argument);
void CallVoidFunction(UObject *Object, const FName &FunctionName);
FString GetMediaUrl(UObject *MediaSource);
bool ValidateMediaSourcePolicy(UObject *MediaSource, FString &OutErrorCode,
                               FString &OutError);

}
