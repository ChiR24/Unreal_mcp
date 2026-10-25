#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
class FJsonValue;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
namespace McpPackageJobs { struct FJob; }

namespace McpSystemControlHandlers {

using FSystemControlSocket = TSharedPtr<FMcpBridgeWebSocket>;

// The material usage flags an asset's meshes need and their materials lack: a Nanite static mesh's slots, a
// skeletal mesh's, and a Blueprint's mesh components (Nanite, instanced, spline or skeletal). Each as {material,
// usage, property (the flag to set true on the material), unsaved (set in the editor, not on disk), component
// (Blueprints)}. The editor sets a missing flag only when it draws the mesh outside play, so a mesh only ever spawned
// in play renders the default material there and in a cooked game. McpSystemMaterialUsage.cpp.
TArray<TSharedPtr<FJsonValue>> McpMissingMaterialUsage(UObject* Asset);
// Starts or stops recording the usage flags the editor sets on 5.8 material instances, which the engine's own record
// of flags awaiting a save leaves out. The subsystem's lifetime; nothing on earlier engines.
void McpWatchMaterialUsage(bool bWatch);

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
// package_project executableName: Source/<Name>.Target.cs, a copy of the project's own game target under the new
// name, made when the project has no target of that name. False with OutError and OutCode when it cannot be had.
bool McpEnsureGameTarget(const FString& Name, FString& OutFile, bool& bOutCreated, FString& OutError,
                         FString& OutCode);
bool HandlePackageStatus(UEngineWireSubsystem* Self,
                         const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload,
                         FSystemControlSocket RequestingSocket);
bool HandleLaunchBuild(UEngineWireSubsystem* Self,
                       const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload,
                       FSystemControlSocket RequestingSocket);
// Adds a launch_build job's game-log evidence to a package_status reply.
void AppendLaunchStatus(const McpPackageJobs::FJob& Job, const TSharedPtr<FJsonObject>& Result);
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
