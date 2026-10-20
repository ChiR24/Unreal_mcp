#include "Domains/GAS/McpGASPayloadFields.h"
#include "Domains/GAS/McpGASRequestContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Safety/McpSafeOperations.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "AbilitySystemComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"

namespace McpGASHandlers
{
bool HandleGASComponents(const FGASRequestContext& Context, const FString& SubAction)
{
    UEngineWireSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("add_ability_system_component"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Only an Actor Blueprint has a construction script to hold components; this dereferenced it and crashed.
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        if (!SCS)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("%s is not an Actor Blueprint, so it holds no components; add the Ability System Component to the pawn or its PlayerState."), *Blueprint->GetName()),
                TEXT("INVALID_TARGET"));
            return true;
        }
        FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT("AbilitySystemComponent"));

        // The actor's one Ability System Component, its own or inherited from a native parent, is the one GAS finds.
        // A second under another name was dead weight, and one under the same name made CreateNode build its
        // template over the existing one.
        FString Existing;
        for (USCS_Node* Node : SCS->GetAllNodes())
        {
            if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<UAbilitySystemComponent>())
            {
                Existing = Node->GetVariableName().ToString();
                break;
            }
        }
        const AActor* ActorDefaults = Blueprint->GeneratedClass ? Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
        const UAbilitySystemComponent* Inherited = ActorDefaults ? ActorDefaults->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
        if (Existing.IsEmpty() && Inherited)
        {
            Existing = Inherited->GetName();
        }
        if (!Existing.IsEmpty())
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("componentName"), Existing);
            Result->SetStringField(TEXT("componentClass"), TEXT("AbilitySystemComponent"));
            Result->SetBoolField(TEXT("alreadyExists"), true);
            McpHandlerUtils::AddVerification(Result, Blueprint);
            McpHandlerUtils::MarkNoAssetsChanged(Result);
            Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
                FString::Printf(TEXT("%s already has the Ability System Component %s; nothing was added."), *Blueprint->GetName(), *Existing), Result);
            return true;
        }
        if (FKismetNameValidator(Blueprint).IsValid(ComponentName) != EValidatorResult::Ok)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("componentName '%s' is already used by a member of %s, or is not a valid name; nothing was added."), *ComponentName, *Blueprint->GetName()),
                TEXT("NAME_TAKEN"));
            return true;
        }

        USCS_Node* NewNode = SCS->CreateNode(UAbilitySystemComponent::StaticClass(), FName(*ComponentName));

        if (!NewNode)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Failed to create ASC node"), TEXT("CREATION_FAILED"));
            return true;
        }

        SCS->AddNode(NewNode);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        if (!CommitGASBlueprintEdit(Context, Blueprint, TEXT("Ability System Component")))
        {
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetStringField(TEXT("componentClass"), TEXT("AbilitySystemComponent"));
        McpHandlerUtils::AddVerification(Result, Blueprint);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ASC added"), Result);
        return true;
    }

    // configure_asc
    if (SubAction == TEXT("configure_asc"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        // componentName picks one; without it, the Blueprint's own Ability System Component (the first, as GAS finds it).
        FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
        FString ReplicationMode = GetJsonStringField(Payload, TEXT("replicationMode"), TEXT("Full"));
        UAbilitySystemComponent* ASCTemplate = nullptr;
        if (!Blueprint->SimpleConstructionScript)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("%s is not an Actor Blueprint, so it holds no components."), *Blueprint->GetName()), TEXT("INVALID_TARGET"));
            return true;
        }
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<UAbilitySystemComponent>() &&
                (ComponentName.IsEmpty() || Node->GetVariableName().ToString() == ComponentName))
            {
                ASCTemplate = Cast<UAbilitySystemComponent>(Node->ComponentTemplate);
                ComponentName = Node->GetVariableName().ToString();
                break;
            }
        }

        if (!ASCTemplate)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                ComponentName.IsEmpty()
                    ? FString::Printf(TEXT("%s has no Ability System Component of its own; add one with add_component (one inherited from a native parent is set in C++)."), *Blueprint->GetName())
                    : FString::Printf(TEXT("%s has no Ability System Component named %s"), *Blueprint->GetName(), *ComponentName),
                TEXT("NOT_FOUND"));
            return true;
        }

        // Full / Mixed / Minimal. An unknown mode used to leave the template alone while it was echoed
        // back as applied, and the edit was never saved.
        EGameplayEffectReplicationMode Mode = EGameplayEffectReplicationMode::Full;
        if (!TryParseGASEnum(ReplicationMode, Mode))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Unknown replicationMode '%s'; use Full, Mixed or Minimal."), *ReplicationMode), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        ASCTemplate->Modify();
        ASCTemplate->SetReplicationMode(Mode);
        if (!CommitGASBlueprintEdit(Context, Blueprint, TEXT("ASC replication mode")))
        {
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetStringField(TEXT("replicationMode"), GASEnumName(Mode));
        McpHandlerUtils::AddVerification(Result, Blueprint);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ASC configured"), Result);
        return true;
    }

    return false;
}
}
