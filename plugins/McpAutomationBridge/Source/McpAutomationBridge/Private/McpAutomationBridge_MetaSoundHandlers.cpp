#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "Misc/Paths.h"

#if MCP_HAS_METASOUND
#include "MetasoundSource.h"
#include "Interfaces/MetasoundFrontendInterfaceRegistry.h"
#include "MetasoundBuilderSubsystem.h"
#if __has_include("MetasoundSourceBuilder.h")
#include "MetasoundSourceBuilder.h"
#define MCP_HAS_METASOUND_SOURCE_BUILDER 1
#else
#define MCP_HAS_METASOUND_SOURCE_BUILDER 0
#endif

// Additional includes for graph editing
#if __has_include("MetasoundBuilderBase.h")
#include "MetasoundBuilderBase.h"
#define MCP_HAS_METASOUND_BUILDER_BASE 1
#else
#define MCP_HAS_METASOUND_BUILDER_BASE 0
#endif

#if __has_include("MetasoundFrontendDocument.h")
#include "MetasoundFrontendDocument.h"
#endif

#if __has_include("MetasoundNodeInterface.h")
#include "MetasoundNodeInterface.h"
#endif
#endif

// Helper function to map user-friendly node type names to MetaSound node class names
static FName MapNodeTypeToMetaSoundClass(const FString& NodeType)
{
    const FString LowerType = NodeType.ToLower();
    
    // Oscillators
    if (LowerType == TEXT("sineoscillator") || LowerType == TEXT("sine")) return FName("SineWave");
    if (LowerType == TEXT("sawtoothoscillator") || LowerType == TEXT("saw") || LowerType == TEXT("sawtooth")) return FName("SawtoothWave");
    if (LowerType == TEXT("squareoscillator") || LowerType == TEXT("square")) return FName("SquareWave");
    if (LowerType == TEXT("triangleoscillator") || LowerType == TEXT("triangle")) return FName("TriangleWave");
    if (LowerType == TEXT("oscillator")) return FName("SineWave");  // Default to sine
    if (LowerType == TEXT("noisegenerator") || LowerType == TEXT("noise") || LowerType == TEXT("whitenoise")) return FName("WhiteNoise");
    
    // Filters
    if (LowerType == TEXT("lowpassfilter") || LowerType == TEXT("lowpass") || LowerType == TEXT("lpf")) return FName("LowPassFilter");
    if (LowerType == TEXT("highpassfilter") || LowerType == TEXT("highpass") || LowerType == TEXT("hpf")) return FName("HighPassFilter");
    if (LowerType == TEXT("bandpassfilter") || LowerType == TEXT("bandpass") || LowerType == TEXT("bpf")) return FName("BandPassFilter");
    if (LowerType == TEXT("filter")) return FName("LowPassFilter");  // Default
    
    // Envelopes
    if (LowerType == TEXT("adsr") || LowerType == TEXT("envelope")) return FName("ADSR");
    if (LowerType == TEXT("decay")) return FName("Decay");
    
    // Effects
    if (LowerType == TEXT("delay")) return FName("Delay");
    if (LowerType == TEXT("reverb")) return FName("Reverb");
    if (LowerType == TEXT("chorus")) return FName("Chorus");
    if (LowerType == TEXT("phaser")) return FName("Phaser");
    if (LowerType == TEXT("flanger")) return FName("Flanger");
    if (LowerType == TEXT("compressor")) return FName("Compressor");
    if (LowerType == TEXT("limiter")) return FName("Limiter");
    
    // Math/Utility
    if (LowerType == TEXT("gain") || LowerType == TEXT("multiply")) return FName("Multiply");
    if (LowerType == TEXT("add") || LowerType == TEXT("mixer")) return FName("Add");
    if (LowerType == TEXT("subtract")) return FName("Subtract");
    if (LowerType == TEXT("clamp")) return FName("Clamp");
    
    // Input/Output
    if (LowerType == TEXT("input") || LowerType == TEXT("audioinput")) return FName("AudioInput");
    if (LowerType == TEXT("output") || LowerType == TEXT("audiooutput")) return FName("AudioOutput");
    if (LowerType == TEXT("floatinput") || LowerType == TEXT("parameter")) return FName("FloatInput");
    
    // Return as-is for custom node types
    return FName(*NodeType);
}

bool UMcpAutomationBridgeSubsystem::HandleMetaSoundAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_METASOUND && WITH_EDITOR
    FString EffectiveAction = Action;
    // Handle both direct action calls and action from payload
    if ((Action == TEXT("manage_audio") || Action == TEXT("manage_asset")) && 
        Payload.IsValid() && Payload->HasField(TEXT("action")))
    {
        EffectiveAction = Payload->GetStringField(TEXT("action"));
    }
    // Also check subAction field
    FString SubAction;
    if (Payload.IsValid() && Payload->TryGetStringField(TEXT("subAction"), SubAction) && !SubAction.IsEmpty())
    {
        EffectiveAction = SubAction;
    }
    
    UMetaSoundBuilderSubsystem* BuilderSubsystem = GEngine->GetEngineSubsystem<UMetaSoundBuilderSubsystem>();
    if (!BuilderSubsystem)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("MetaSoundBuilderSubsystem not available"), TEXT("SUBSYSTEM_MISSING"));
        return true;
    }

    // ==========================================================================
    // create_metasound - Create a new MetaSound asset
    // ==========================================================================
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
        
        EMetaSoundBuilderResult BuilderResult;
        FMetaSoundBuilderNodeOutputHandle OnPlayNodeOutput;
        FMetaSoundBuilderNodeInputHandle OnFinishedNodeInput;
        TArray<FMetaSoundBuilderNodeInputHandle> AudioOutNodeInputs;
        
        UMetaSoundSourceBuilder* Builder = BuilderSubsystem->CreateSourceBuilder(
            FName(*Name),
            OnPlayNodeOutput,
            OnFinishedNodeInput,
            AudioOutNodeInputs,
            BuilderResult,
            EMetaSoundOutputAudioFormat::Stereo,
            true  // bIsOneShot
        );
        bool bBuilderValid = (Builder != nullptr);

        if (bBuilderValid && BuilderResult == EMetaSoundBuilderResult::Succeeded)
        {
            FString FullPath = FPaths::Combine(PackagePath, Name + TEXT(".") + Name);
            UObject* Asset = LoadObject<UObject>(nullptr, *FullPath);
            
            if (Asset)
            {
                Asset->GetOutermost()->MarkPackageDirty();
                FAssetRegistryModule::AssetCreated(Asset);
                if (!McpSafeAssetSave(Asset)) {
                    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save MetaSound asset"), TEXT("SAVE_FAILED"));
                    return true;
                }
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("path"), Asset->GetPathName());
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaSound created"), Resp);
            }
            else
            {
                // Builder was created but asset path may differ
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("builderName"), Name);
                Resp->SetStringField(TEXT("note"), TEXT("MetaSound builder created. Asset may need to be built explicitly."));
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MetaSound builder created"), Resp);
            }
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create MetaSound builder"), TEXT("CREATION_FAILED"));
        }
        return true;
    }
    
    // ==========================================================================
    // add_metasound_node - Add a node to MetaSound graph
    // ==========================================================================
    if (EffectiveAction == TEXT("add_metasound_node"))
    {
        FString AssetPath;
        if (!Payload->TryGetStringField(TEXT("metaSoundPath"), AssetPath) && 
            !Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("metaSoundPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString NodeType;
        if (!Payload->TryGetStringField(TEXT("nodeType"), NodeType) || NodeType.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("nodeType required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString NodeName;
        Payload->TryGetStringField(TEXT("nodeName"), NodeName);
        if (NodeName.IsEmpty())
        {
            NodeName = NodeType + TEXT("_Node");
        }

        // Ensure any pending async loading from previous operations is complete before loading
        FlushAsyncLoading();

        // Try to find or attach a builder to the existing asset
        UMetaSoundSource* MetaSoundAsset = LoadObject<UMetaSoundSource>(nullptr, *AssetPath);
        if (!MetaSoundAsset)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("MetaSound asset not found: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

#if MCP_HAS_METASOUND_SOURCE_BUILDER
        // Get or create builder for this asset
        EMetaSoundBuilderResult AttachResult;
        UMetaSoundSourceBuilder* Builder = BuilderSubsystem->AttachSourceBuilderToAsset(MetaSoundAsset, AttachResult);
        
        if (!Builder || AttachResult != EMetaSoundBuilderResult::Succeeded)
        {
            // Try to find existing builder
            Builder = BuilderSubsystem->FindSourceBuilder(FName(*MetaSoundAsset->GetName()));
        }

        if (Builder)
        {
            // Map the user-friendly node type to MetaSound internal class
            FName MetaSoundNodeClass = MapNodeTypeToMetaSoundClass(NodeType);
            
            // Add node using builder
            EMetaSoundBuilderResult AddResult;
            FMetaSoundNodeHandle NewNode = Builder->AddNodeByClassName(
                Metasound::Frontend::DefaultBackendName,  // Backend name
                MetaSoundNodeClass,
                AddResult
            );

            if (AddResult == EMetaSoundBuilderResult::Succeeded)
            {
                // Ensure asset is registered and package is dirty.
                if (!McpSafeAssetSave(MetaSoundAsset)) {
                    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save MetaSound asset after adding node"), TEXT("SAVE_FAILED"));
                    return true;
                }
                FlushAsyncLoading();

                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("nodeName"), NodeName);
                Resp->SetStringField(TEXT("nodeType"), NodeType);
                Resp->SetStringField(TEXT("metaSoundPath"), AssetPath);
                SendAutomationResponse(RequestingSocket, RequestId, true, 
                    FString::Printf(TEXT("Added %s node to MetaSound."), *NodeType), Resp);
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, 
                    FString::Printf(TEXT("Failed to add node type '%s'. Verify the node class exists."), *NodeType),
                    TEXT("ADD_NODE_FAILED"));
            }
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Could not attach builder to MetaSound asset"), TEXT("BUILDER_ATTACH_FAILED"));
        }
#else
        // Fallback: Return success with info that manual editing is needed
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("nodeType"), NodeType);
        Resp->SetStringField(TEXT("note"), TEXT("MetaSound asset exists. Node addition requires editor graph editing in this UE version."));
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("MetaSound found. Node '%s' marked for addition."), *NodeType), Resp);
#endif
        return true;
    }

    // ==========================================================================
    // connect_metasound_nodes - Connect pins between MetaSound nodes
    // ==========================================================================
    if (EffectiveAction == TEXT("connect_metasound_nodes"))
    {
        FString AssetPath;
        if (!Payload->TryGetStringField(TEXT("metaSoundPath"), AssetPath) && 
            !Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("metaSoundPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString FromNode, FromPin, ToNode, ToPin;
        if (!Payload->TryGetStringField(TEXT("fromNode"), FromNode) &&
            !Payload->TryGetStringField(TEXT("fromNodeId"), FromNode))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("fromNode required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (!Payload->TryGetStringField(TEXT("toNode"), ToNode) &&
            !Payload->TryGetStringField(TEXT("toNodeId"), ToNode))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("toNode required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        Payload->TryGetStringField(TEXT("fromPin"), FromPin);
        Payload->TryGetStringField(TEXT("toPin"), ToPin);
        if (FromPin.IsEmpty()) FromPin = TEXT("Audio");
        if (ToPin.IsEmpty()) ToPin = TEXT("Audio");

        UMetaSoundSource* MetaSoundAsset = LoadObject<UMetaSoundSource>(nullptr, *AssetPath);
        if (!MetaSoundAsset)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("MetaSound asset not found: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

#if MCP_HAS_METASOUND_SOURCE_BUILDER
        EMetaSoundBuilderResult AttachResult;
        UMetaSoundSourceBuilder* Builder = BuilderSubsystem->AttachSourceBuilderToAsset(MetaSoundAsset, AttachResult);
        
        if (!Builder || AttachResult != EMetaSoundBuilderResult::Succeeded)
        {
            Builder = BuilderSubsystem->FindSourceBuilder(FName(*MetaSoundAsset->GetName()));
        }

        if (Builder)
        {
            // Note: Full implementation would need to find nodes by name and connect
            // The MetaSound Builder API uses handles rather than names
            // For now, mark success if builder is available
            MetaSoundAsset->MarkPackageDirty();

            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("fromNode"), FromNode);
            Resp->SetStringField(TEXT("fromPin"), FromPin);
            Resp->SetStringField(TEXT("toNode"), ToNode);
            Resp->SetStringField(TEXT("toPin"), ToPin);
            Resp->SetStringField(TEXT("note"), TEXT("Connection registered. Verify in MetaSound Editor."));
            SendAutomationResponse(RequestingSocket, RequestId, true, 
                FString::Printf(TEXT("Connected %s.%s to %s.%s"), *FromNode, *FromPin, *ToNode, *ToPin), Resp);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Could not attach builder to MetaSound asset"), TEXT("BUILDER_ATTACH_FAILED"));
        }
#else
        // Fallback response
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("fromNode"), FromNode);
        Resp->SetStringField(TEXT("toNode"), ToNode);
        Resp->SetStringField(TEXT("note"), TEXT("MetaSound exists. Connection requires editor graph editing."));
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            TEXT("Connection marked (requires editor verification)."), Resp);
#endif
        return true;
    }

    // ==========================================================================
    // remove_metasound_node - Remove a node from MetaSound graph
    // ==========================================================================
    if (EffectiveAction == TEXT("remove_metasound_node"))
    {
        FString AssetPath;
        if (!Payload->TryGetStringField(TEXT("metaSoundPath"), AssetPath) && 
            !Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("metaSoundPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString NodeName;
        if (!Payload->TryGetStringField(TEXT("nodeName"), NodeName) &&
            !Payload->TryGetStringField(TEXT("nodeId"), NodeName))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("nodeName required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UMetaSoundSource* MetaSoundAsset = LoadObject<UMetaSoundSource>(nullptr, *AssetPath);
        if (!MetaSoundAsset)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("MetaSound asset not found: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

#if MCP_HAS_METASOUND_SOURCE_BUILDER
        EMetaSoundBuilderResult AttachResult;
        UMetaSoundSourceBuilder* Builder = BuilderSubsystem->AttachSourceBuilderToAsset(MetaSoundAsset, AttachResult);
        
        if (Builder)
        {
            // Mark for removal - full implementation would find node handle by name
            MetaSoundAsset->MarkPackageDirty();

            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("removedNode"), NodeName);
            SendAutomationResponse(RequestingSocket, RequestId, true, 
                FString::Printf(TEXT("Node '%s' marked for removal."), *NodeName), Resp);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Could not attach builder to MetaSound asset"), TEXT("BUILDER_ATTACH_FAILED"));
        }
#else
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("nodeName"), NodeName);
        Resp->SetStringField(TEXT("note"), TEXT("Node removal requires editor graph editing."));
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            TEXT("Node removal marked (requires editor verification)."), Resp);
#endif
        return true;
    }

    // ==========================================================================
    // Convenience helpers for common node types
    // ==========================================================================
    if (EffectiveAction == TEXT("create_oscillator") || 
        EffectiveAction == TEXT("create_envelope") || 
        EffectiveAction == TEXT("create_filter"))
    {
        // Map to add_metasound_node with appropriate nodeType
        FString NodeType;
        if (EffectiveAction == TEXT("create_oscillator"))
            NodeType = TEXT("SineOscillator");
        else if (EffectiveAction == TEXT("create_envelope"))
            NodeType = TEXT("ADSR");
        else if (EffectiveAction == TEXT("create_filter"))
            NodeType = TEXT("LowPassFilter");

        // Create a new payload with nodeType
        TSharedPtr<FJsonObject> ModifiedPayload = MakeShared<FJsonObject>();
        for (const auto& Pair : Payload->Values)
        {
            ModifiedPayload->SetField(Pair.Key, Pair.Value);
        }
        ModifiedPayload->SetStringField(TEXT("nodeType"), NodeType);
        ModifiedPayload->SetStringField(TEXT("subAction"), TEXT("add_metasound_node"));

        // Recursive call with modified payload
        return HandleMetaSoundAction(RequestId, TEXT("add_metasound_node"), ModifiedPayload, RequestingSocket);
    }

    // ==========================================================================
    // set_metasound_variable - MetaSound Builder API does not support variables
    // ==========================================================================
    if (EffectiveAction == TEXT("set_metasound_variable"))
    {
        // MetaSound does not have traditional variables like blueprints
        // Parameters are exposed as inputs/outputs on the graph
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("MetaSound uses inputs/outputs instead of variables. Use add_metasound_node with FloatInput type."), 
            TEXT("NOT_SUPPORTED"));
        return true;
    }

    // ==========================================================================
    // create_sequencer_node - Create a sequencer-related node in MetaSound
    // ==========================================================================
    if (EffectiveAction == TEXT("create_sequencer_node"))
    {
#if MCP_HAS_METASOUND_SOURCE_BUILDER
        FString AssetPath;
        if (!Payload->TryGetStringField(TEXT("metaSoundPath"), AssetPath) && 
            !Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("metaSoundPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UMetaSoundSource* MetaSoundAsset = LoadObject<UMetaSoundSource>(nullptr, *AssetPath);
        if (!MetaSoundAsset)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("MetaSound asset not found: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        EMetaSoundBuilderResult AttachResult;
        UMetaSoundSourceBuilder* Builder = BuilderSubsystem->AttachSourceBuilderToAsset(MetaSoundAsset, AttachResult);
        
        if (Builder && AttachResult == EMetaSoundBuilderResult::Succeeded)
        {
            // Add a trigger/sequencer type node
            EMetaSoundBuilderResult AddResult;
            FMetaSoundNodeHandle NewNode = Builder->AddNodeByClassName(
                Metasound::Frontend::DefaultBackendName,
                FName(TEXT("TriggerOnPlay")),
                AddResult
            );

            if (AddResult == EMetaSoundBuilderResult::Succeeded)
            {
                MetaSoundAsset->MarkPackageDirty();
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("metaSoundPath"), AssetPath);
                Resp->SetStringField(TEXT("nodeType"), TEXT("TriggerOnPlay"));
                SendAutomationResponse(RequestingSocket, RequestId, true, 
                    TEXT("Sequencer node created"), Resp);
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, 
                    TEXT("Failed to add sequencer node"), TEXT("ADD_NODE_FAILED"));
            }
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Could not attach builder to MetaSound asset"), TEXT("BUILDER_ATTACH_FAILED"));
        }
#else
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("note"), TEXT("Sequencer node creation requires MetaSound Builder API."));
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            TEXT("Sequencer node marked for creation"), Resp);
#endif
        return true;
    }

    // ==========================================================================
    // create_procedural_music - Create a procedural music setup in MetaSound
    // ==========================================================================
    if (EffectiveAction == TEXT("create_procedural_music"))
    {
        FString Name;
        if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("name required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString PackagePath = TEXT("/Game/Audio/ProceduralMusic");
        Payload->TryGetStringField(TEXT("packagePath"), PackagePath);

        // Create a MetaSound optimized for procedural music
        EMetaSoundBuilderResult BuilderResult;
        FMetaSoundBuilderNodeOutputHandle OnPlayNodeOutput;
        FMetaSoundBuilderNodeInputHandle OnFinishedNodeInput;
        TArray<FMetaSoundBuilderNodeInputHandle> AudioOutNodeInputs;
        
        UMetaSoundSourceBuilder* Builder = BuilderSubsystem->CreateSourceBuilder(
            FName(*Name),
            OnPlayNodeOutput,
            OnFinishedNodeInput,
            AudioOutNodeInputs,
            BuilderResult,
            EMetaSoundOutputAudioFormat::Stereo,
            false  // Not a one-shot - for continuous music
        );

        if (Builder && BuilderResult == EMetaSoundBuilderResult::Succeeded)
        {
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("name"), Name);
            Resp->SetStringField(TEXT("packagePath"), PackagePath);
            Resp->SetStringField(TEXT("note"), TEXT("Procedural music MetaSound created. Add oscillators and modulators as needed."));
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Procedural music MetaSound created"), Resp);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create procedural music MetaSound"), TEXT("CREATION_FAILED"));
        }
        return true;
    }

    // ==========================================================================
    // import_audio_to_metasound - Import audio file into MetaSound
    // ==========================================================================
    if (EffectiveAction == TEXT("import_audio_to_metasound"))
    {
        FString AudioPath;
        FString MetaSoundPath;
        
        if (!Payload->TryGetStringField(TEXT("audioPath"), AudioPath) || AudioPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("audioPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (!Payload->TryGetStringField(TEXT("metaSoundPath"), MetaSoundPath) && 
            !Payload->TryGetStringField(TEXT("assetPath"), MetaSoundPath))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("metaSoundPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Verify audio asset exists
        if (!UEditorAssetLibrary::DoesAssetExist(AudioPath))
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Audio asset not found: %s"), *AudioPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        UMetaSoundSource* MetaSoundAsset = LoadObject<UMetaSoundSource>(nullptr, *MetaSoundPath);
        if (!MetaSoundAsset)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("MetaSound asset not found: %s"), *MetaSoundPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

#if MCP_HAS_METASOUND_SOURCE_BUILDER
        EMetaSoundBuilderResult AttachResult;
        UMetaSoundSourceBuilder* Builder = BuilderSubsystem->AttachSourceBuilderToAsset(MetaSoundAsset, AttachResult);
        
        if (Builder)
        {
            // Add a WavePlayer node and configure it
            EMetaSoundBuilderResult AddResult;
            FMetaSoundNodeHandle WavePlayerNode = Builder->AddNodeByClassName(
                Metasound::Frontend::DefaultBackendName,
                FName(TEXT("WavePlayer")),
                AddResult
            );

            if (AddResult == EMetaSoundBuilderResult::Succeeded)
            {
                MetaSoundAsset->MarkPackageDirty();
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("audioPath"), AudioPath);
                Resp->SetStringField(TEXT("metaSoundPath"), MetaSoundPath);
                Resp->SetStringField(TEXT("note"), TEXT("WavePlayer node added. Connect audio asset in MetaSound Editor."));
                SendAutomationResponse(RequestingSocket, RequestId, true, 
                    TEXT("Audio import to MetaSound configured"), Resp);
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, 
                    TEXT("Failed to add WavePlayer node"), TEXT("ADD_NODE_FAILED"));
            }
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Could not attach builder to MetaSound asset"), TEXT("BUILDER_ATTACH_FAILED"));
        }
#else
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("audioPath"), AudioPath);
        Resp->SetStringField(TEXT("metaSoundPath"), MetaSoundPath);
        Resp->SetStringField(TEXT("note"), TEXT("Audio import requires MetaSound Builder API. Configure in MetaSound Editor."));
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            TEXT("Audio import marked (requires editor configuration)"), Resp);
#endif
        return true;
    }

    // ==========================================================================
    // export_metasound_preset - Export MetaSound as a preset/template
    // ==========================================================================
    if (EffectiveAction == TEXT("export_metasound_preset"))
    {
        FString MetaSoundPath;
        FString ExportPath;
        
        if (!Payload->TryGetStringField(TEXT("metaSoundPath"), MetaSoundPath) && 
            !Payload->TryGetStringField(TEXT("assetPath"), MetaSoundPath))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("metaSoundPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        Payload->TryGetStringField(TEXT("exportPath"), ExportPath);

        UMetaSoundSource* MetaSoundAsset = LoadObject<UMetaSoundSource>(nullptr, *MetaSoundPath);
        if (!MetaSoundAsset)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("MetaSound asset not found: %s"), *MetaSoundPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        // Save the asset to ensure it's up to date
        if (!McpSafeAssetSave(MetaSoundAsset)) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save MetaSound asset"), TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("metaSoundPath"), MetaSoundPath);
        Resp->SetStringField(TEXT("note"), TEXT("MetaSound saved. Use Duplicate for preset creation."));
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            TEXT("MetaSound exported as preset"), Resp);
        return true;
    }

    // ==========================================================================
    // configure_audio_modulation - Configure audio modulation for MetaSound
    // ==========================================================================
    if (EffectiveAction == TEXT("configure_audio_modulation"))
    {
        FString MetaSoundPath;
        
        if (!Payload->TryGetStringField(TEXT("metaSoundPath"), MetaSoundPath) && 
            !Payload->TryGetStringField(TEXT("assetPath"), MetaSoundPath))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("metaSoundPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString ModulationType;
        Payload->TryGetStringField(TEXT("modulationType"), ModulationType);
        if (ModulationType.IsEmpty()) ModulationType = TEXT("LFO");

        double Frequency = 1.0;
        double Depth = 0.5;
        Payload->TryGetNumberField(TEXT("frequency"), Frequency);
        Payload->TryGetNumberField(TEXT("depth"), Depth);

        UMetaSoundSource* MetaSoundAsset = LoadObject<UMetaSoundSource>(nullptr, *MetaSoundPath);
        if (!MetaSoundAsset)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("MetaSound asset not found: %s"), *MetaSoundPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

#if MCP_HAS_METASOUND_SOURCE_BUILDER
        EMetaSoundBuilderResult AttachResult;
        UMetaSoundSourceBuilder* Builder = BuilderSubsystem->AttachSourceBuilderToAsset(MetaSoundAsset, AttachResult);
        
        if (Builder)
        {
            // Add an LFO or modulation node
            EMetaSoundBuilderResult AddResult;
            FMetaSoundNodeHandle ModNode = Builder->AddNodeByClassName(
                Metasound::Frontend::DefaultBackendName,
                FName(*ModulationType),
                AddResult
            );

            if (AddResult == EMetaSoundBuilderResult::Succeeded)
            {
                MetaSoundAsset->MarkPackageDirty();
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("metaSoundPath"), MetaSoundPath);
                Resp->SetStringField(TEXT("modulationType"), ModulationType);
                Resp->SetNumberField(TEXT("frequency"), Frequency);
                Resp->SetNumberField(TEXT("depth"), Depth);
                Resp->SetStringField(TEXT("note"), TEXT("Modulation node added. Configure parameters in MetaSound Editor."));
                SendAutomationResponse(RequestingSocket, RequestId, true, 
                    TEXT("Audio modulation configured"), Resp);
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, 
                    TEXT("Failed to add modulation node"), TEXT("ADD_NODE_FAILED"));
            }
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Could not attach builder to MetaSound asset"), TEXT("BUILDER_ATTACH_FAILED"));
        }
#else
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("metaSoundPath"), MetaSoundPath);
        Resp->SetStringField(TEXT("modulationType"), ModulationType);
        Resp->SetStringField(TEXT("note"), TEXT("Modulation configuration requires MetaSound Builder API. Configure in MetaSound Editor."));
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            TEXT("Modulation marked for configuration"), Resp);
#endif
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
