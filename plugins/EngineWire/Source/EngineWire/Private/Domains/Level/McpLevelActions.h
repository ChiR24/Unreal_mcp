#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#include "Foundation/BridgeHelpers/Security/McpProjectPaths.h"

struct FMcpLevelHandlerAccess {
  static bool ManageLevelStructure(
      UEngineWireSubsystem& Subsystem,
      const FString& RequestId,
      const FString& Action,
      const TSharedPtr<FJsonObject>& Payload,
      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    return Subsystem.HandleManageLevelStructureAction(
        RequestId, Action, Payload, RequestingSocket);
  }

  static bool SetMetadata(
      UEngineWireSubsystem& Subsystem,
      const FString& RequestId,
      const TSharedPtr<FJsonObject>& Payload,
      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    return Subsystem.HandleSetMetadata(RequestId, Payload, RequestingSocket);
  }
};

namespace McpLevelHandlers {
// levelPath as load opens it: through the shared canonicalizer, an object path (/Game/Maps/Demo.Demo) read as its
// package. A path without its leading slash in a mounted root (Engine/Maps/Entry) is that root's; one the canonicalizer
// refuses there (Demo, Maps/Demo) is a level under /Game/Maps when one is there, else under /Game. Empty when refused,
// the refusal logged only when bLogRefusal.
FString ResolveLoadLevelPath(const FString& Requested, bool bLogRefusal = true);
bool HandleLoadLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleGetCurrentLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleSaveCurrentLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleSaveLevelAsAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleSetMetadataAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleValidateLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
// The editor's Map Check on the open level, as rows (Lifecycle/McpLevelMapCheck.cpp).
bool HandleRunMapCheckAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleCreateNewLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStreamLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, bool bForceStreamUnload);
bool HandleListLevelsAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleExportLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleImportLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleAddSublevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleRemoveSublevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleMoveActorsToLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
// The editor's current level, the one new actors are spawned into (World/McpLevelSetCurrent.cpp).
bool HandleSetCurrentLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleDeleteLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleRenameLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleDuplicateLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleGetLevelInfoAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleSetLevelWorldSettingsAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
} // namespace McpLevelHandlers
