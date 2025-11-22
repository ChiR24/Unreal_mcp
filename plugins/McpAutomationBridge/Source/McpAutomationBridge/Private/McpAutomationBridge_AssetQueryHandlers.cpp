#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "AssetRegistry/AssetRegistryModule.h"

#if WITH_EDITOR
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleAssetQueryAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = Payload->GetStringField(TEXT("subAction"));

    if (SubAction == TEXT("get_dependencies"))
    {
        FString AssetPath;
        Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
        bool bRecursive = false;
        Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        TArray<FName> Dependencies;
        UE::AssetRegistry::EDependencyQuery Query = bRecursive ? UE::AssetRegistry::EDependencyQuery::Hard : UE::AssetRegistry::EDependencyQuery::Hard; // Simplified
        
        AssetRegistryModule.Get().GetDependencies(FName(*AssetPath), Dependencies, UE::AssetRegistry::EDependencyCategory::Package, Query);

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> DepArray;
        for (const FName& Dep : Dependencies)
        {
            DepArray.Add(MakeShared<FJsonValueString>(Dep.ToString()));
        }
        Result->SetArrayField(TEXT("dependencies"), DepArray);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Dependencies retrieved."), Result);
        return true;
    }
#if WITH_EDITOR
    else if (SubAction == TEXT("get_source_control_state"))
    {
        FString AssetPath;
        Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

        if (ISourceControlModule::Get().IsEnabled())
        {
            ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
            FSourceControlStatePtr State = Provider.GetState(AssetPath, EStateCacheUsage::Use);
            
            if (State.IsValid())
            {
                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetBoolField(TEXT("isCheckedOut"), State->IsCheckedOut());
                Result->SetBoolField(TEXT("isAdded"), State->IsAdded());
                Result->SetBoolField(TEXT("isDeleted"), State->IsDeleted());
                Result->SetBoolField(TEXT("isModified"), State->IsModified());
                // Result->SetStringField(TEXT("whoCheckedOut"), State->GetCheckOutUser());
                
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Source control state retrieved."), Result);
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Could not get source control state."), TEXT("STATE_FAILED"));
            }
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Source control not enabled."), TEXT("SC_DISABLED"));
        }
        return true;
    }
#endif

    SendAutomationError(RequestingSocket, RequestId, TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
