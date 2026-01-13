#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "Misc/Paths.h"

#if MCP_HAS_METASOUND
#include "MetasoundSource.h"
#include "Interfaces/MetasoundFrontendInterfaceRegistry.h"
#include "MetasoundBuilderSubsystem.h"
#include "MetasoundSourceBuilder.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleMetaSoundAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_METASOUND && WITH_EDITOR
    FString EffectiveAction = Action;
    if (Action == TEXT("manage_audio") && Payload.IsValid() && Payload->HasField(TEXT("action")))
    {
        EffectiveAction = Payload->GetStringField(TEXT("action"));
    }
    
    UMetaSoundBuilderSubsystem* BuilderSubsystem = GEngine->GetEngineSubsystem<UMetaSoundBuilderSubsystem>();
    if (!BuilderSubsystem)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("MetaSoundBuilderSubsystem not available"), TEXT("SUBSYSTEM_MISSING"));
        return true;
    }

    if (EffectiveAction == TEXT("create_metasound"))
    {
        FString Name;
        if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("name required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString PackagePath = TEXT("/Game/Audio/MetaSounds");
        Payload->TryGetStringField(TEXT("packagePath"), PackagePath);
        
        // CreateSourceBuilder returns result via out parameter
        EMetaSoundBuilderResult BuilderResult;
        TScriptInterface<IMetaSoundDocumentBuilder> Builder = BuilderSubsystem->CreateSourceBuilder(
            FName(*Name), 
            FName(*PackagePath), 
            BuilderResult
        );

        if (Builder && BuilderResult == EMetaSoundBuilderResult::Succeeded)
        {
            FString FullPath = FPaths::Combine(PackagePath, Name + TEXT(".") + Name);
            UObject* Asset = LoadObject<UObject>(nullptr, *FullPath);
            
            if (Asset)
            {
                McpSafeAssetSave(Asset);
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("path"), Asset->GetPathName());
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaSound created"), Resp);
            }
            else
            {
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaSound builder created (asset pending save)"), nullptr);
            }
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create MetaSound builder"), TEXT("CREATION_FAILED"));
        }
        return true;
    }
    
    // For editing actions, we need an existing asset
    // Note: These actions require UE 5.4+ MetaSound Builder APIs which vary significantly between versions.
    // The core create_metasound action works across versions; graph editing requires version-specific implementation.
    if (EffectiveAction == TEXT("add_metasound_node") || 
        EffectiveAction == TEXT("connect_metasound_nodes") ||
        EffectiveAction == TEXT("remove_metasound_node"))
    {
        FString AssetPath;
        if (!Payload->TryGetStringField(TEXT("metaSoundPath"), AssetPath) && !Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("metaSoundPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
        if (!Asset)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("MetaSound asset not found"), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        // MetaSound graph editing requires version-specific Builder APIs
        // In UE 5.4+, use AttachSourceBuilderToAsset or FindBuilderObject
        // Return informative error until version-specific implementation is added
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("MetaSound graph editing ('%s') requires UE 5.4+ specific APIs. Use create_metasound for asset creation."), *EffectiveAction), 
            TEXT("VERSION_SPECIFIC_API"));
        return true;
    }

    if (EffectiveAction == TEXT("create_oscillator") || 
        EffectiveAction == TEXT("create_envelope") || 
        EffectiveAction == TEXT("create_filter"))
    {
        // These are convenience aliases that would call add_metasound_node with preset parameters
        // They depend on the graph editing APIs above
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("MetaSound helper action '%s' requires graph editing support (UE 5.4+ APIs). Use create_metasound and configure in Editor."), *EffectiveAction), 
            TEXT("VERSION_SPECIFIC_API"));
        return true;
    }

    return false;
#else
    if (Action.Contains(TEXT("metasound")))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("MetaSound plugin not enabled or supported"), TEXT("NOT_SUPPORTED"));
        return true;
    }
    return false;
#endif
}
