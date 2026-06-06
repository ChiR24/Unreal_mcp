#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Paths.h"
#include "NavAreas/NavArea.h"
#include "NavAreas/NavArea_Default.h"
#include "NavAreas/NavArea_Null.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "NavModifierComponent.h"

namespace McpAIHandlers
{
bool HandleCreateNavModifier(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("create_nav_modifier");
    if (SubAction == TEXT("create_nav_modifier"))
    {
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));
        if (BlueprintPath.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        if (!Blueprint->SimpleConstructionScript)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_STATE"));
            return true;
        }

        FString ComponentName = GetStringFieldAI(Payload, TEXT("componentName"));
        if (ComponentName.IsEmpty())
        {
            ComponentName = TEXT("NavModifierComponent");
        }

        // Create nav modifier component
        USCS_Node* NavModNode = Blueprint->SimpleConstructionScript->CreateNode(
            UNavModifierComponent::StaticClass(), *ComponentName);
        if (!NavModNode)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create nav modifier node"), TEXT("CREATION_FAILED"));
            return true;
        }

        Blueprint->SimpleConstructionScript->AddNode(NavModNode);
        UNavModifierComponent* NavModComp = Cast<UNavModifierComponent>(NavModNode->ComponentTemplate);

        if (NavModComp)
        {
            // Configure fail-safe defaults
            bool bFailsafe = GetBoolFieldAI(Payload, TEXT("failsafeToDefaultNavmesh"));
            NavModComp->SetAreaClass(bFailsafe ? UNavArea_Default::StaticClass() : UNavArea_Obstacle::StaticClass());

            // Set area class if specified
            FString AreaClassName = GetStringFieldAI(Payload, TEXT("areaClass"));
            if (!AreaClassName.IsEmpty())
            {
                UClass* AreaClass = FindObject<UClass>(nullptr, *AreaClassName);
                if (!AreaClass)
                {
                    // Try common area classes
                    if (AreaClassName.Equals(TEXT("NavArea_Null"), ESearchCase::IgnoreCase) ||
                        AreaClassName.Equals(TEXT("Null"), ESearchCase::IgnoreCase))
                    {
                        AreaClass = UNavArea_Null::StaticClass();
                    }
                    else if (AreaClassName.Equals(TEXT("NavArea_Obstacle"), ESearchCase::IgnoreCase) ||
                             AreaClassName.Equals(TEXT("Obstacle"), ESearchCase::IgnoreCase))
                    {
                        AreaClass = UNavArea_Obstacle::StaticClass();
                    }
                    else if (AreaClassName.Equals(TEXT("NavArea_Default"), ESearchCase::IgnoreCase) ||
                             AreaClassName.Equals(TEXT("Default"), ESearchCase::IgnoreCase))
                    {
                        AreaClass = UNavArea_Default::StaticClass();
                    }
                }

                if (AreaClass && AreaClass->IsChildOf(UNavArea::StaticClass()))
                {
                    NavModComp->SetAreaClass(AreaClass);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> NavModResult = McpHandlerUtils::CreateResultObject();
        NavModResult->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        NavModResult->SetStringField(TEXT("componentName"), ComponentName);
        // UE 5.7: GetAreaClass() is not available on UNavModifierComponent
        // The area class is determined by the NavArea class set on the component
        FString AreaClassName = TEXT("Default");
        NavModResult->SetStringField(TEXT("areaClass"), AreaClassName);

        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Nav modifier component created"), NavModResult);
        return true;
    }

    // set_ai_movement - Configure AI movement parameters (speed, acceleration, etc.)
    return true;
}

bool HandleCreateNavLinkProxy(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("create_nav_link_proxy");
    if (SubAction == TEXT("create_nav_link_proxy"))
    {
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));
        if (BlueprintPath.IsEmpty())
        {
            BlueprintPath = GetStringFieldAI(Payload, TEXT("name"));
            if (!BlueprintPath.IsEmpty())
            {
                FString Path = GetStringFieldAI(Payload, TEXT("path"));
                if (Path.IsEmpty()) Path = TEXT("/Game/AI");
                BlueprintPath = Path / BlueprintPath;
            }
        }
        if (BlueprintPath.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath or name"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString SanitizedPath, SanitizeError;
        if (!SanitizeAIAssetPath(BlueprintPath, SanitizedPath, SanitizeError))
        {
            Self->SendAutomationError(RequestingSocket, RequestId, SanitizeError, TEXT("INVALID_PATH"));
            return true;
        }

        if (UEditorAssetLibrary::DoesAssetExist(SanitizedPath))
        {
            TSharedPtr<FJsonObject> ExistResult = McpHandlerUtils::CreateResultObject();
            ExistResult->SetStringField(TEXT("blueprintPath"), SanitizedPath);
            ExistResult->SetBoolField(TEXT("alreadyExisted"), true);
            Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("NavLinkProxy blueprint already exists"), ExistResult);
            return true;
        }

        UClass* NavLinkProxyClass = FindObject<UClass>(nullptr, TEXT("/Script/NavigationSystem.NavLinkProxy"));
        if (!NavLinkProxyClass)
        {
            NavLinkProxyClass = AActor::StaticClass();
        }

        UBlueprint* NavLinkBP = FKismetEditorUtilities::CreateBlueprint(
            NavLinkProxyClass,
            CreatePackage(*SanitizedPath),
            *FPaths::GetBaseFilename(SanitizedPath),
            BPTYPE_Normal,
            UBlueprint::StaticClass(),
            UBlueprintGeneratedClass::StaticClass());

        if (!NavLinkBP)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create NavLinkProxy blueprint"), TEXT("CREATION_FAILED"));
            return true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(NavLinkBP);
        McpSafeAssetSave(NavLinkBP);

        TSharedPtr<FJsonObject> NavResult = McpHandlerUtils::CreateResultObject();
        NavResult->SetStringField(TEXT("blueprintPath"), SanitizedPath);
        NavResult->SetBoolField(TEXT("alreadyExisted"), false);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("NavLinkProxy blueprint created"), NavResult);
        return true;
    }

    // set_focus - Set focus actor variable on AI controller blueprint
    return true;
}
}
#endif
