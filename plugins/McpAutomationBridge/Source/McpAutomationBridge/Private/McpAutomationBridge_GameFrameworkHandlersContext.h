#pragma once

#include "McpVersionCompatibility.h"

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Templates/SharedPointer.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Factories/BlueprintFactory.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/WorldSettings.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "Misc/EngineVersionComparison.h"
#include "UObject/SoftObjectPath.h"
#endif

class FMcpBridgeWebSocket;
class UBlueprint;

DECLARE_LOG_CATEGORY_EXTERN(LogMcpGameFrameworkHandlers, Log, All);

namespace McpGameFrameworkHandlers
{
struct FActionContext
{
    UMcpAutomationBridgeSubsystem* Subsystem = nullptr;
    FString RequestId;
    TSharedPtr<FJsonObject> Payload;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
    FString SubAction;
    FString Name;
    FString Path = TEXT("/Game");
    FString GameModeBlueprint;
    FString BlueprintPath;
    bool bSave = false;

    void SendError(const FString& Message, const FString& ErrorCode) const;
    void SendSuccess(const TSharedPtr<FJsonObject>& Response) const;
};

FActionContext MakeActionContext(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

#if WITH_EDITOR
bool ValidateCommonFields(FActionContext& Context);
bool RequireGameModePath(FActionContext& Context);
FString GetStringField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, const FString& Default = TEXT(""));
double GetNumberField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, double Default = 0.0);
bool GetBoolField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, bool Default = false);
TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName);
const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName);
UBlueprint* LoadBlueprintFromPath(const FString& BlueprintPath);
UBlueprint* LoadRequiredGameMode(FActionContext& Context);
UBlueprint* CreateGameFrameworkBlueprint(const FString& Path, const FString& Name, UClass* ParentClass, FString& OutError);
UClass* LoadClassFromPath(const FString& ClassPath);
bool SetClassProperty(UBlueprint* Blueprint, const FName& PropertyName, UClass* ClassToSet, FString& OutError);
bool AddBlueprintVariable(UBlueprint* Blueprint, const FString& VarName, const FEdGraphPinType& PinType, const FString& Category = TEXT(""));
void SetVariableDefaultValue(UBlueprint* Blueprint, const FString& VarName, const FString& DefaultValue);
int32 AddVariable(UBlueprint* Blueprint, const FString& VarName, const FEdGraphPinType& PinType, const FString& Category);
int32 AddVariableWithDefault(UBlueprint* Blueprint, const FString& VarName, const FEdGraphPinType& PinType, const FString& Category, const FString& DefaultValue);
void FinishBlueprintMutation(UBlueprint* Blueprint, bool bSave);
TSharedPtr<FJsonObject> MakeBlueprintResponse(const FString& Message, UBlueprint* Blueprint);
FEdGraphPinType MakeIntPinType();
FEdGraphPinType MakeFloatPinType();
FEdGraphPinType MakeBoolPinType();
FEdGraphPinType MakeNamePinType();
FEdGraphPinType MakeBytePinType();
bool HandleCoreClassAction(FActionContext& Context);
bool HandleGameModeConfigAction(FActionContext& Context);
bool HandleMatchFlowAction(FActionContext& Context);
bool HandlePlayerFlowAction(FActionContext& Context);
bool HandleInfoAction(FActionContext& Context);
#endif
}
