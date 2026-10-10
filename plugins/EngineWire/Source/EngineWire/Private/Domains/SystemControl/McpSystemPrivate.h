#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

namespace McpSystemControlHandlers {

using FSystemControlSocket = TSharedPtr<FMcpBridgeWebSocket>;

bool HandleValidateAssets(UEngineWireSubsystem* Self,
                          const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload,
                          FSystemControlSocket RequestingSocket);
bool HandleRunUbt(UEngineWireSubsystem* Self,
                  const FString& RequestId,
                  const TSharedPtr<FJsonObject>& Payload,
                  FSystemControlSocket RequestingSocket);
bool HandleRunTests(UEngineWireSubsystem* Self,
                    const FString& RequestId,
                    const TSharedPtr<FJsonObject>& Payload,
                    FSystemControlSocket RequestingSocket);
bool HandlePackageProject(UEngineWireSubsystem* Self,
                          const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload,
                          FSystemControlSocket RequestingSocket);
bool HandlePackageStatus(UEngineWireSubsystem* Self,
                         const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload,
                         FSystemControlSocket RequestingSocket);
bool HandleLaunchBuild(UEngineWireSubsystem* Self,
                       const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload,
                       FSystemControlSocket RequestingSocket);
// Adds a launch_build job's game-log evidence to a package_status reply.
void AppendLaunchStatus(const FString& GameLogPath, int32 ExitCode, bool bExitedEarly,
                        const TSharedPtr<FJsonObject>& Result);
bool HandleExecutePython(UEngineWireSubsystem* Self,
                         const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload,
                         FSystemControlSocket RequestingSocket);
// list_output_files / delete_output_file: the files the MCP itself writes
// (screenshots, snapshots), and nothing outside those folders.
bool HandleListOutputFiles(UEngineWireSubsystem* Self,
                           const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload,
                           FSystemControlSocket RequestingSocket);
bool HandleDeleteOutputFile(UEngineWireSubsystem* Self,
                            const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload,
                            FSystemControlSocket RequestingSocket);
// list_save_games / edit_save_game: the game's SaveGame slots (Saved/SaveGames), loaded as the game loads them.
bool HandleListSaveGames(UEngineWireSubsystem* Self,
                         const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload,
                         FSystemControlSocket RequestingSocket);
bool HandleEditSaveGame(UEngineWireSubsystem* Self,
                        const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload,
                        FSystemControlSocket RequestingSocket);
bool HandleManagePlugins(UEngineWireSubsystem* Self,
                         const FString& RequestId,
                         const FString& SubAction,
                         const TSharedPtr<FJsonObject>& Payload,
                         FSystemControlSocket RequestingSocket);
// create_cpp_class: a header and .cpp from a parent class in a project module, the editor's New C++ Class.
bool HandleCreateCppClass(UEngineWireSubsystem* Self,
                          const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload,
                          FSystemControlSocket RequestingSocket);

}
