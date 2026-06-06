#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;

namespace McpSystemControlHandlers {

using FSystemControlSocket = TSharedPtr<FMcpBridgeWebSocket>;

bool HandleValidateAssets(UMcpAutomationBridgeSubsystem* Self,
                          const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload,
                          FSystemControlSocket RequestingSocket);
bool HandleRunUbt(UMcpAutomationBridgeSubsystem* Self,
                  const FString& RequestId,
                  const TSharedPtr<FJsonObject>& Payload,
                  FSystemControlSocket RequestingSocket);
bool HandleRunTests(UMcpAutomationBridgeSubsystem* Self,
                    const FString& RequestId,
                    const TSharedPtr<FJsonObject>& Payload,
                    FSystemControlSocket RequestingSocket);
bool HandleTestProgressProtocol(UMcpAutomationBridgeSubsystem* Self,
                                const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload,
                                FSystemControlSocket RequestingSocket);
bool HandleTestStaleProgress(UMcpAutomationBridgeSubsystem* Self,
                             const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload,
                             FSystemControlSocket RequestingSocket);
bool HandleExportAsset(UMcpAutomationBridgeSubsystem* Self,
                       const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload,
                       FSystemControlSocket RequestingSocket);
bool HandleExecutePython(UMcpAutomationBridgeSubsystem* Self,
                         const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload,
                         FSystemControlSocket RequestingSocket);

}
