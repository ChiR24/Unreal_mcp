#include "Domains/GAS/McpAutomationBridge_GASBlueprintCreation.h"
#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Safety/McpSafeOperations.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AttributeSet.h"
#include "Dom/JsonValue.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectExecutionCalculation.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpGASHandlers
{
bool HandleGASAbilityGrantAndExecution(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    // ============================================================
    // 13.7 EXECUTION CALCULATIONS
    // ============================================================

    if (SubAction == TEXT("create_execution_calculation"))
    {
        bool bReusedExisting = false;
        const TSharedPtr<FJsonObject> Result = CreateGASAsset(Context, UGameplayEffectExecutionCalculation::StaticClass(),
            TEXT("GameplayEffectExecutionCalculation"), bReusedExisting,
            [](UBlueprint* Blueprint) { McpSafeCompileBlueprint(Blueprint); });
        if (!Result)
        {
            return true;
        }
        Result->SetStringField(TEXT("note"), TEXT("Override Execute_Implementation in Blueprint to implement custom calculation logic."));

        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            bReusedExisting ? TEXT("Execution calculation already exists") : TEXT("Execution calculation created"), Result);
        return true;
    }

    return false;
}
}
