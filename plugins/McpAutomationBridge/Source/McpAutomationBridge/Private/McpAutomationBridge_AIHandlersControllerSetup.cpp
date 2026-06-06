#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "AIController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "EdGraphSchema_K2.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"

namespace McpAIHandlers
{
static UBlueprint* CreateAIControllerBlueprint(const FString& Path, const FString& Name, FString& OutError)
{
    // Sanitize and validate path first
    FString SanitizedPath;
    if (!SanitizeAIAssetPath(Path, SanitizedPath, OutError))
    {
        return nullptr;
    }

    FString FullPath = SanitizedPath / Name;

    // CRITICAL: Use UEditorAssetLibrary for reliable existence check
    // This prevents the Kismet2.cpp assertion failure when blueprint already exists
    // The FindObject check alone is insufficient because:
    // 1. It checks for exact object path format
    // 2. The package may exist even if the blueprint object doesn't
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        OutError = FString::Printf(TEXT("Asset already exists: %s"), *FullPath);
        return nullptr;
    }

    // Also check with .AssetName suffix (blueprint object path format)
    FString ObjectPath = FullPath + TEXT(".") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
    {
        OutError = FString::Printf(TEXT("Blueprint already exists: %s"), *ObjectPath);
        return nullptr;
    }

    // Check if package exists on disk (for assets that haven't been loaded yet)
    if (FPackageName::DoesPackageExist(FullPath))
    {
        OutError = FString::Printf(TEXT("Package already exists on disk: %s"), *FullPath);
        return nullptr;
    }

    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    if (!Factory)
    {
        OutError = TEXT("Failed to create BlueprintFactory");
        return nullptr;
    }

    Factory->ParentClass = AAIController::StaticClass();

    UBlueprint* Blueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name,
                                  RF_Public | RF_Standalone, nullptr, GWarn));

    if (!Blueprint)
    {
        OutError = TEXT("Failed to create AI Controller blueprint");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Blueprint);
    McpSafeAssetSave(Blueprint);

    return Blueprint;
}

// Helper to create Blackboard asset

bool HandleCreateAIController(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("create_ai_controller");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("create_ai_controller"))
    {
        FString Name = GetStringFieldAI(Payload, TEXT("name"));
        FString Path = GetStringFieldAI(Payload, TEXT("path"), TEXT("/Game/AI/Controllers"));

        if (Name.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                TEXT("Missing name parameter"),
                                TEXT("INVALID_PARAMS"));
            return true;
        }

        FString Error;
        UBlueprint* Blueprint = CreateAIControllerBlueprint(Path, Name, Error);
        if (!Blueprint)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        Result->SetStringField(TEXT("controllerPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created AI Controller: %s"), *Name));
        McpHandlerUtils::AddVerification(Result, Blueprint);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("AI Controller created"), Result);
        return true;
    }

    return true;
}

bool HandleAssignBehaviorTree(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("assign_behavior_tree");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("assign_behavior_tree"))
    {
        FString ControllerPath = GetStringFieldAI(Payload, TEXT("controllerPath"));
        FString BehaviorTreePath = GetStringFieldAI(Payload, TEXT("behaviorTreePath"));

        // CRITICAL: Remove DoesAssetExist pre-check - newly created assets may not yet be
        // indexed in the asset registry. Rely on LoadObject null-check instead.
        UBlueprint* ControllerBP = LoadObject<UBlueprint>(nullptr, *ControllerPath);
        if (!ControllerBP)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Controller blueprint not found: %s"), *ControllerPath), TEXT("NOT_FOUND"));
            return true;
        }

        UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BehaviorTreePath);
        if (!BT)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Behavior tree not found: %s"), *BehaviorTreePath), TEXT("NOT_FOUND"));
            return true;
        }

        // Set default BehaviorTree property on the generated class CDO using reflection
        if (ControllerBP->GeneratedClass)
        {
            if (AAIController* CDO = Cast<AAIController>(ControllerBP->GeneratedClass->GetDefaultObject()))
            {
                // Use reflection to find and set BehaviorTree-related properties
                // Look for common property names used in AI Controller blueprints
                bool bPropertySet = false;

                // Try to find a UBehaviorTree* property on the CDO
                for (TFieldIterator<FObjectProperty> PropIt(ControllerBP->GeneratedClass); PropIt; ++PropIt)
                {
                    FObjectProperty* ObjProp = *PropIt;
                    if (ObjProp && ObjProp->PropertyClass && ObjProp->PropertyClass->IsChildOf(UBehaviorTree::StaticClass()))
                    {
                        // Found a BehaviorTree property - set it
                        ObjProp->SetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(CDO), BT);
                        bPropertySet = true;
                        Result->SetStringField(TEXT("propertyName"), ObjProp->GetName());
                        break;
                    }
                }

                // If no existing property found, add a Blueprint variable for the BT reference
                if (!bPropertySet)
                {
                    // Add a Blueprint variable to store the BehaviorTree reference
                    FEdGraphPinType PinType;
                    PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
                    PinType.PinSubCategoryObject = UBehaviorTree::StaticClass();

                    const FName VarName = TEXT("DefaultBehaviorTree");
                    if (FBlueprintEditorUtils::AddMemberVariable(ControllerBP, VarName, PinType))
                    {
                        // Set the default value for the variable
                        FProperty* NewProp = ControllerBP->GeneratedClass->FindPropertyByName(VarName);
                        if (FObjectProperty* ObjProp = CastField<FObjectProperty>(NewProp))
                        {
                            ObjProp->SetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(CDO), BT);
                            bPropertySet = true;
                        }
                    }
                    Result->SetStringField(TEXT("propertyName"), VarName.ToString());
                }

                Result->SetBoolField(TEXT("propertyAssigned"), bPropertySet);
                Result->SetStringField(TEXT("message"), bPropertySet
                    ? TEXT("Behavior Tree property assigned on CDO")
                    : TEXT("Behavior Tree reference registered (call RunBehaviorTree in BeginPlay)"));
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ControllerBP);
        McpSafeAssetSave(ControllerBP);
        Result->SetStringField(TEXT("controllerPath"), ControllerPath);
        Result->SetStringField(TEXT("behaviorTreePath"), BehaviorTreePath);
        McpHandlerUtils::AddVerification(Result, ControllerBP);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior Tree reference set"), Result);
        return true;
    }

    return true;
}
}
#endif
