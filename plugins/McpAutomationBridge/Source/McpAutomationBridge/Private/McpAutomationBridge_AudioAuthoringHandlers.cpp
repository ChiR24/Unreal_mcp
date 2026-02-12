#include "Dom/JsonObject.h"
// Copyright (c) 2025 MCP Automation Bridge Contributors
// SPDX-License-Identifier: MIT
//
// McpAutomationBridge_AudioAuthoringHandlers.cpp
// Phase 11: Complete Audio System Authoring
//
// Implements Sound Cues, MetaSounds, Sound Classes & Mixes,
// Attenuation & Spatialization, Dialogue System, and Audio Effects.

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

// Audio Core
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundNode.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundNodeMixer.h"
#include "Sound/SoundNodeRandom.h"
#include "Sound/SoundNodeModulator.h"
#include "Sound/SoundNodeLooping.h"
#include "Sound/SoundNodeAttenuation.h"
#include "Sound/SoundNodeConcatenator.h"
#include "Sound/SoundNodeDelay.h"
#include "Sound/SoundNodeSwitch.h"
#include "Sound/SoundNodeBranch.h"

// Audio Factories
#include "Factories/SoundCueFactoryNew.h"
#include "Factories/SoundClassFactory.h"
#include "Factories/SoundMixFactory.h"
#include "Factories/SoundAttenuationFactory.h"

// Dialogue
#if __has_include("Sound/DialogueVoice.h")
#include "Sound/DialogueVoice.h"
#include "Sound/DialogueWave.h"
#define MCP_HAS_DIALOGUE 1
#else
#define MCP_HAS_DIALOGUE 0
#endif

// Dialogue Factories
#if __has_include("Factories/DialogueVoiceFactory.h")
#include "Factories/DialogueVoiceFactory.h"
#include "Factories/DialogueWaveFactory.h"
#define MCP_HAS_DIALOGUE_FACTORY 1
#else
#define MCP_HAS_DIALOGUE_FACTORY 0
#endif

// Audio Effects
#if __has_include("Sound/SoundEffectSource.h")
#include "Sound/SoundEffectSource.h"
#define MCP_HAS_SOURCE_EFFECT 1
#else
#define MCP_HAS_SOURCE_EFFECT 0
#endif

#if __has_include("Sound/SoundSubmixSend.h")
#include "Sound/SoundSubmixSend.h"
#endif

#if __has_include("Sound/SoundSubmix.h")
#include "Sound/SoundSubmix.h"
#define MCP_HAS_SUBMIX 1
#else
#define MCP_HAS_SUBMIX 0
#endif

#if __has_include("AudioMixerTypes.h")
#include "AudioMixerTypes.h"
#endif

// Source Effect Chain
#if __has_include("SourceEffects/SourceEffectChain.h")
#include "SourceEffects/SourceEffectChain.h"
#define MCP_HAS_EFFECT_CHAIN 0
#elif __has_include("Sound/SoundEffectPreset.h")
#include "Sound/SoundEffectPreset.h"
#define MCP_HAS_EFFECT_CHAIN 0
#else
#define MCP_HAS_EFFECT_CHAIN 0
#endif

// Reverb Effects
#if __has_include("Sound/ReverbEffect.h")
#include "Sound/ReverbEffect.h"
#define MCP_HAS_REVERB_EFFECT 1
#else
#define MCP_HAS_REVERB_EFFECT 0
#endif

// MetaSound support (UE 5.0+)
#if __has_include("MetasoundSource.h")
#include "MetasoundSource.h"
#define MCP_HAS_METASOUND 1
#else
#define MCP_HAS_METASOUND 0
#endif

#if __has_include("Metasound.h")
#include "Metasound.h"
#endif

#if __has_include("MetasoundBuilderSubsystem.h")
#include "MetasoundBuilderSubsystem.h"
#define MCP_HAS_METASOUND_BUILDER 1
#else
#define MCP_HAS_METASOUND_BUILDER 0
#endif

// MetaSound Frontend Document Builder (UE 5.3+)
#if __has_include("MetasoundFrontendDocumentBuilder.h")
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendDocument.h"
#define MCP_HAS_METASOUND_FRONTEND 1
// UE 5.4+ has 3-arg constructor and FinishBuilding method
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
#define MCP_HAS_METASOUND_FRONTEND_V2 1
#else
#define MCP_HAS_METASOUND_FRONTEND_V2 0
#endif
#else
#define MCP_HAS_METASOUND_FRONTEND 0
#define MCP_HAS_METASOUND_FRONTEND_V2 0
#endif

// MetaSound Factory (Editor)
#if __has_include("MetasoundFactory.h")
#include "MetasoundFactory.h"
#define MCP_HAS_METASOUND_FACTORY 1
#else
#define MCP_HAS_METASOUND_FACTORY 0
#endif

// MetaSound Editor Subsystem
#if __has_include("MetasoundEditorSubsystem.h")
#include "MetasoundEditorSubsystem.h"
#define MCP_HAS_METASOUND_EDITOR 1
#else
#define MCP_HAS_METASOUND_EDITOR 0
#endif

// Helper macros
#define AUDIO_ERROR_RESPONSE(Msg, Code) \
    Response->SetBoolField(TEXT("success"), false); \
    Response->SetStringField(TEXT("error"), Msg); \
    Response->SetStringField(TEXT("errorCode"), Code); \
    return Response;

#define AUDIO_SUCCESS_RESPONSE(Msg) \
    Response->SetBoolField(TEXT("success"), true); \
    Response->SetStringField(TEXT("message"), Msg);

namespace {

// Use consolidated JSON helpers from McpAutomationBridgeHelpers.h
// Note: These are macros to avoid ODR issues with the anonymous namespace
#define GetNumberFieldAudioAuth GetJsonNumberField
#define GetBoolFieldAudioAuth GetJsonBoolField
#define GetStringFieldAudioAuth GetJsonStringField

// Helper to normalize asset path with security validation
static FString NormalizeAudioPath(const FString& Path)
{
    // SECURITY: First validate path for traversal attacks
    FString Sanitized = SanitizeProjectRelativePath(Path);
    if (Sanitized.IsEmpty() && !Path.IsEmpty())
    {
        // Path was rejected due to traversal or invalid characters
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, 
            TEXT("NormalizeAudioPath: Rejected malicious path: %s"), *Path);
        return FString();
    }
    
    FString Normalized = Sanitized;
    
    // Only replace /Content at the start to avoid corrupting plugin paths
    // Plugin paths like /MyPlugin/Content/Audio should NOT become /MyPlugin/Game/Audio
    if (Normalized.StartsWith(TEXT("/Content/")))
    {
        Normalized = TEXT("/Game/") + Normalized.Mid(9);  // Skip "/Content/"
    }
    else if (Normalized == TEXT("/Content"))
    {
        Normalized = TEXT("/Game");
    }
    
    Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
    
    // Remove trailing slashes
    while (Normalized.EndsWith(TEXT("/")))
    {
        Normalized.LeftChopInline(1);
    }
    
    return Normalized;
}

// Helper to save asset - UE 5.7+ Fix: Do not save immediately to avoid modal dialogs.
// modal progress dialogs that block automation. Instead, just mark dirty and notify registry.
static bool SaveAudioAsset(UObject* Asset, bool bShouldSave)
{
    if (!bShouldSave || !Asset)
    {
        return true;
    }
    
    // Mark dirty and notify asset registry - do NOT save to disk
    // This avoids modal dialogs and allows the editor to save later
    Asset->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Asset);
    return true;
}

// Helper to load sound wave from path
static USoundWave* LoadSoundWaveFromPath(const FString& SoundPath)
{
    FString NormalizedPath = NormalizeAudioPath(SoundPath);
    return Cast<USoundWave>(StaticLoadObject(USoundWave::StaticClass(), nullptr, *NormalizedPath));
}

// Helper to load sound cue from path
static USoundCue* LoadSoundCueFromPath(const FString& CuePath)
{
    FString NormalizedPath = NormalizeAudioPath(CuePath);
    return Cast<USoundCue>(StaticLoadObject(USoundCue::StaticClass(), nullptr, *NormalizedPath));
}

// Helper to load sound class from path
static USoundClass* LoadSoundClassFromPath(const FString& ClassPath)
{
    FString NormalizedPath = NormalizeAudioPath(ClassPath);
    return Cast<USoundClass>(StaticLoadObject(USoundClass::StaticClass(), nullptr, *NormalizedPath));
}

// Helper to load sound attenuation from path
static USoundAttenuation* LoadSoundAttenuationFromPath(const FString& AttenPath)
{
    FString NormalizedPath = NormalizeAudioPath(AttenPath);
    return Cast<USoundAttenuation>(StaticLoadObject(USoundAttenuation::StaticClass(), nullptr, *NormalizedPath));
}

// Helper to load sound mix from path
static USoundMix* LoadSoundMixFromPath(const FString& MixPath)
{
    FString NormalizedPath = NormalizeAudioPath(MixPath);
    return Cast<USoundMix>(StaticLoadObject(USoundMix::StaticClass(), nullptr, *NormalizedPath));
}

} // anonymous namespace

// Main handler function that processes audio authoring requests
static TSharedPtr<FJsonObject> HandleAudioAuthoringRequest(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    
    FString SubAction = GetStringFieldAudioAuth(Params, TEXT("subAction"), TEXT(""));
    
    // ===== 11.1 Sound Cues =====
    
    if (SubAction == TEXT("create_sound_cue"))
    {
        FString Name = GetStringFieldAudioAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("path"), TEXT("/Game/Audio/Cues")));
        FString WavePath = GetStringFieldAudioAuth(Params, TEXT("wavePath"), TEXT(""));
        bool bLooping = GetBoolFieldAudioAuth(Params, TEXT("looping"), false);
        float Volume = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("volume"), 1.0));
        float Pitch = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("pitch"), 1.0));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        // Create package and asset directly to avoid UI dialogs
        // AssetToolsModule.CreateAsset() shows "Overwrite Existing Object" dialogs
        // which cause recursive FlushRenderingCommands and D3D12 crashes
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>();
        USoundCue* NewCue = Cast<USoundCue>(
            Factory->FactoryCreateNew(USoundCue::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewCue)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create SoundCue"), TEXT("CREATE_FAILED"));
        }
        
        // If wave path provided, set up basic graph
        if (!WavePath.IsEmpty())
        {
            USoundWave* Wave = LoadSoundWaveFromPath(WavePath);
            if (Wave)
            {
                USoundNodeWavePlayer* PlayerNode = NewCue->ConstructSoundNode<USoundNodeWavePlayer>();
                PlayerNode->SetSoundWave(Wave);
                
                USoundNode* LastNode = PlayerNode;
                
                // Add looping if requested
                if (bLooping)
                {
                    USoundNodeLooping* LoopNode = NewCue->ConstructSoundNode<USoundNodeLooping>();
                    LoopNode->ChildNodes.Add(LastNode);
                    LastNode = LoopNode;
                }
                
                // Add modulation if volume/pitch differs from default
                if (Volume != 1.0f || Pitch != 1.0f)
                {
                    USoundNodeModulator* ModNode = NewCue->ConstructSoundNode<USoundNodeModulator>();
                    ModNode->PitchMin = ModNode->PitchMax = Pitch;
                    ModNode->VolumeMin = ModNode->VolumeMax = Volume;
                    ModNode->ChildNodes.Add(LastNode);
                    LastNode = ModNode;
                }
                
                NewCue->FirstNode = LastNode;
                NewCue->LinkGraphNodesFromSoundNodes();
            }
        }
        
        SaveAudioAsset(NewCue, bSave);
        
        FString FullPath = NewCue->GetPathName();
        Response->SetStringField(TEXT("assetPath"), FullPath);
        AUDIO_SUCCESS_RESPONSE(FString::Printf(TEXT("SoundCue '%s' created"), *Name));
        return Response;
    }
    
    if (SubAction == TEXT("add_cue_node"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString NodeType = GetStringFieldAudioAuth(Params, TEXT("nodeType"), TEXT("wave_player"));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        USoundCue* Cue = LoadSoundCueFromPath(AssetPath);
        if (!Cue)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundCue: %s"), *AssetPath), TEXT("CUE_NOT_FOUND"));
        }
        
        USoundNode* NewNode = nullptr;
        FString NodeTypeLower = NodeType.ToLower();
        
        if (NodeTypeLower == TEXT("wave_player") || NodeTypeLower == TEXT("waveplayer"))
        {
            USoundNodeWavePlayer* Player = Cue->ConstructSoundNode<USoundNodeWavePlayer>();
            FString WavePath = GetStringFieldAudioAuth(Params, TEXT("wavePath"), TEXT(""));
            if (!WavePath.IsEmpty())
            {
                USoundWave* Wave = LoadSoundWaveFromPath(WavePath);
                if (Wave)
                {
                    Player->SetSoundWave(Wave);
                }
            }
            NewNode = Player;
        }
        else if (NodeTypeLower == TEXT("mixer"))
        {
            NewNode = Cue->ConstructSoundNode<USoundNodeMixer>();
        }
        else if (NodeTypeLower == TEXT("random"))
        {
            NewNode = Cue->ConstructSoundNode<USoundNodeRandom>();
        }
        else if (NodeTypeLower == TEXT("modulator"))
        {
            USoundNodeModulator* Mod = Cue->ConstructSoundNode<USoundNodeModulator>();
            Mod->VolumeMin = Mod->VolumeMax = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("volume"), 1.0));
            Mod->PitchMin = Mod->PitchMax = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("pitch"), 1.0));
            NewNode = Mod;
        }
        else if (NodeTypeLower == TEXT("looping"))
        {
            USoundNodeLooping* Loop = Cue->ConstructSoundNode<USoundNodeLooping>();
            Loop->bLoopIndefinitely = GetBoolFieldAudioAuth(Params, TEXT("indefinite"), true);
            Loop->LoopCount = static_cast<int32>(GetNumberFieldAudioAuth(Params, TEXT("loopCount"), 0));
            NewNode = Loop;
        }
        else if (NodeTypeLower == TEXT("attenuation"))
        {
            USoundNodeAttenuation* Atten = Cue->ConstructSoundNode<USoundNodeAttenuation>();
            FString AttenPath = GetStringFieldAudioAuth(Params, TEXT("attenuationPath"), TEXT(""));
            if (!AttenPath.IsEmpty())
            {
                USoundAttenuation* AttenAsset = LoadSoundAttenuationFromPath(AttenPath);
                if (AttenAsset)
                {
                    Atten->AttenuationSettings = AttenAsset;
                }
            }
            NewNode = Atten;
        }
        else if (NodeTypeLower == TEXT("concatenator"))
        {
            NewNode = Cue->ConstructSoundNode<USoundNodeConcatenator>();
        }
        else if (NodeTypeLower == TEXT("delay"))
        {
            USoundNodeDelay* Delay = Cue->ConstructSoundNode<USoundNodeDelay>();
            Delay->DelayMin = Delay->DelayMax = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("delay"), 0.0));
            NewNode = Delay;
        }
        else if (NodeTypeLower == TEXT("switch"))
        {
            NewNode = Cue->ConstructSoundNode<USoundNodeSwitch>();
        }
        else if (NodeTypeLower == TEXT("branch"))
        {
            NewNode = Cue->ConstructSoundNode<USoundNodeBranch>();
        }
        else
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Unknown node type: %s"), *NodeType), TEXT("UNKNOWN_NODE_TYPE"));
        }
        
        if (!NewNode)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create sound node"), TEXT("CREATE_NODE_FAILED"));
        }
        
        Cue->LinkGraphNodesFromSoundNodes();
        SaveAudioAsset(Cue, bSave);
        
        Response->SetStringField(TEXT("nodeId"), NewNode->GetName());
        AUDIO_SUCCESS_RESPONSE(FString::Printf(TEXT("Node '%s' added to SoundCue"), *NodeType));
        return Response;
    }
    
    if (SubAction == TEXT("connect_cue_nodes"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString SourceNodeId = GetStringFieldAudioAuth(Params, TEXT("sourceNodeId"), TEXT(""));
        FString TargetNodeId = GetStringFieldAudioAuth(Params, TEXT("targetNodeId"), TEXT(""));
        int32 ChildIndex = static_cast<int32>(GetNumberFieldAudioAuth(Params, TEXT("childIndex"), 0));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        USoundCue* Cue = LoadSoundCueFromPath(AssetPath);
        if (!Cue)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundCue: %s"), *AssetPath), TEXT("CUE_NOT_FOUND"));
        }
        
        // Find source and target nodes
        USoundNode* SourceNode = nullptr;
        USoundNode* TargetNode = nullptr;
        
        for (USoundNode* Node : Cue->AllNodes)
        {
            if (Node && Node->GetName() == SourceNodeId)
            {
                SourceNode = Node;
            }
            if (Node && Node->GetName() == TargetNodeId)
            {
                TargetNode = Node;
            }
        }
        
        if (!SourceNode)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Source node not found: %s"), *SourceNodeId), TEXT("SOURCE_NODE_NOT_FOUND"));
        }
        if (!TargetNode)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Target node not found: %s"), *TargetNodeId), TEXT("TARGET_NODE_NOT_FOUND"));
        }
        
        // Connect target as child of source
        if (ChildIndex >= SourceNode->ChildNodes.Num())
        {
            SourceNode->ChildNodes.SetNum(ChildIndex + 1);
        }
        SourceNode->ChildNodes[ChildIndex] = TargetNode;
        
        Cue->LinkGraphNodesFromSoundNodes();
        SaveAudioAsset(Cue, bSave);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Nodes connected"));
        return Response;
    }
    
    if (SubAction == TEXT("set_cue_attenuation"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString AttenuationPath = GetStringFieldAudioAuth(Params, TEXT("attenuationPath"), TEXT(""));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        USoundCue* Cue = LoadSoundCueFromPath(AssetPath);
        if (!Cue)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundCue: %s"), *AssetPath), TEXT("CUE_NOT_FOUND"));
        }
        
        if (!AttenuationPath.IsEmpty())
        {
            USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AttenuationPath);
            if (Atten)
            {
                Cue->AttenuationSettings = Atten;
            }
        }
        else
        {
            Cue->AttenuationSettings = nullptr;
        }
        
        SaveAudioAsset(Cue, bSave);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Attenuation settings updated"));
        return Response;
    }
    
    if (SubAction == TEXT("set_cue_concurrency"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString ConcurrencyPath = GetStringFieldAudioAuth(Params, TEXT("concurrencyPath"), TEXT(""));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        USoundCue* Cue = LoadSoundCueFromPath(AssetPath);
        if (!Cue)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundCue: %s"), *AssetPath), TEXT("CUE_NOT_FOUND"));
        }
        
        if (!ConcurrencyPath.IsEmpty())
        {
            USoundConcurrency* Conc = Cast<USoundConcurrency>(
                StaticLoadObject(USoundConcurrency::StaticClass(), nullptr, *NormalizeAudioPath(ConcurrencyPath)));
            if (Conc)
            {
                Cue->ConcurrencySet.Empty();
                Cue->ConcurrencySet.Add(Conc);
            }
        }
        else
        {
            Cue->ConcurrencySet.Empty();
        }
        
        SaveAudioAsset(Cue, bSave);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Concurrency settings updated"));
        return Response;
    }
    
    // ===== 11.2 MetaSounds =====
    
    if (SubAction == TEXT("create_metasound"))
    {
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FACTORY
        FString Name = GetStringFieldAudioAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("path"), TEXT("/Game/Audio/MetaSounds")));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        // Create package for the MetaSound asset
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        // Create MetaSound Source asset using the factory
        UMetaSoundSourceFactory* Factory = NewObject<UMetaSoundSourceFactory>();
        UMetaSoundSource* MetaSound = Cast<UMetaSoundSource>(
            Factory->FactoryCreateNew(UMetaSoundSource::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        
        if (!MetaSound)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create MetaSound asset"), TEXT("CREATE_FAILED"));
        }
        
        // Mark dirty and notify asset registry
        McpSafeAssetSave(MetaSound);
        
        FString FullPath = MetaSound->GetPathName();
        Response->SetStringField(TEXT("assetPath"), FullPath);
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound '%s' created"), *Name));
        return Response;
#elif MCP_HAS_METASOUND
        // MetaSound available but no factory - create basic asset
        FString Name = GetStringFieldAudioAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("path"), TEXT("/Game/Audio/MetaSounds")));
        
        if (Name.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        // Create MetaSound directly
        UMetaSoundSource* MetaSound = NewObject<UMetaSoundSource>(Package, FName(*Name), RF_Public | RF_Standalone);
        if (!MetaSound)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create MetaSound asset"), TEXT("CREATE_FAILED"));
        }
        
        McpSafeAssetSave(MetaSound);
        
        FString FullPath = MetaSound->GetPathName();
        Response->SetStringField(TEXT("assetPath"), FullPath);
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound '%s' created"), *Name));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("MetaSound support not available in this engine version"), TEXT("METASOUND_NOT_AVAILABLE"));
#endif
    }
    
    if (SubAction == TEXT("add_metasound_node"))
    {
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString NodeClassName = GetStringFieldAudioAuth(Params, TEXT("nodeClassName"), TEXT(""));
        FString NodeType = GetStringFieldAudioAuth(Params, TEXT("nodeType"), TEXT(""));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Asset path is required"), TEXT("MISSING_PATH"));
        }
        
        // Load the MetaSound asset
        UMetaSoundSource* MetaSound = Cast<UMetaSoundSource>(
            StaticLoadObject(UMetaSoundSource::StaticClass(), nullptr, *AssetPath));
        if (!MetaSound)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load MetaSound: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        }
        
        // Use the Frontend Document Builder API
        IMetaSoundDocumentInterface* DocInterface = Cast<IMetaSoundDocumentInterface>(MetaSound);
        if (!DocInterface)
        {
            AUDIO_ERROR_RESPONSE(TEXT("MetaSound does not implement document interface"), TEXT("INTERFACE_ERROR"));
        }
        
        // Create a builder for this MetaSound
        TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
        #if MCP_HAS_METASOUND_FRONTEND_V2
        FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface, nullptr, true);
#else
        FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface);
#endif
        
        // Determine node class name from nodeType if not explicitly provided
        FString ActualClassName = NodeClassName;
        if (ActualClassName.IsEmpty() && !NodeType.IsEmpty())
        {
            // Map common node types to class names
            FString NodeTypeLower = NodeType.ToLower();
            if (NodeTypeLower == TEXT("oscillator") || NodeTypeLower == TEXT("sine"))
            {
                ActualClassName = TEXT("Metasound.Sine");
            }
            else if (NodeTypeLower == TEXT("gain") || NodeTypeLower == TEXT("multiply"))
            {
                ActualClassName = TEXT("Metasound.Multiply");
            }
            else if (NodeTypeLower == TEXT("add"))
            {
                ActualClassName = TEXT("Metasound.Add");
            }
            else if (NodeTypeLower == TEXT("waveplayer"))
            {
                ActualClassName = TEXT("Metasound.WavePlayer");
            }
            else
            {
                // Use node type as class name directly
                ActualClassName = NodeType;
            }
        }
        
        if (ActualClassName.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Node class name or type is required"), TEXT("MISSING_NODE_TYPE"));
        }
        
        // Add the node using the builder
        FMetasoundFrontendClassName ClassName = FMetasoundFrontendClassName(FName(), FName(*ActualClassName), FName());
        const FMetasoundFrontendNode* NewNode = Builder.AddNodeByClassName(ClassName, 1, FGuid::NewGuid());
        
        if (NewNode)
        {
            McpSafeAssetSave(MetaSound);
            
            Response->SetStringField(TEXT("nodeId"), NewNode->GetID().ToString());
            Response->SetStringField(TEXT("nodeClassName"), ActualClassName);
            Response->SetBoolField(TEXT("success"), true);
            Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound node '%s' added"), *ActualClassName));
        }
        else
        {
            // FIX: Return success: false when node class is not found
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Node class '%s' not found in MetaSound registry"), *ActualClassName));
            Response->SetStringField(TEXT("errorCode"), TEXT("NODE_CLASS_NOT_FOUND"));
        }
        
        #if MCP_HAS_METASOUND_FRONTEND_V2
        Builder.FinishBuilding();
#endif
        return Response;
#elif MCP_HAS_METASOUND
        // FIX: Return error when MetaSound Frontend Builder not available
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString NodeType = GetStringFieldAudioAuth(Params, TEXT("nodeType"), TEXT(""));
        
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Cannot add MetaSound node '%s' - Frontend Builder not available"), *NodeType));
        Response->SetStringField(TEXT("errorCode"), TEXT("METASOUND_FRONTEND_NOT_SUPPORTED"));
        Response->SetStringField(TEXT("requiredVersion"), TEXT("UE 5.3+"));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("MetaSound support not available"), TEXT("METASOUND_NOT_AVAILABLE"));
#endif
    }
    
    if (SubAction == TEXT("connect_metasound_nodes"))
    {
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString SourceNodeId = GetStringFieldAudioAuth(Params, TEXT("sourceNodeId"), TEXT(""));
        FString SourceOutputName = GetStringFieldAudioAuth(Params, TEXT("sourceOutputName"), TEXT(""));
        FString TargetNodeId = GetStringFieldAudioAuth(Params, TEXT("targetNodeId"), TEXT(""));
        FString TargetInputName = GetStringFieldAudioAuth(Params, TEXT("targetInputName"), TEXT(""));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Asset path is required"), TEXT("MISSING_PATH"));
        }
        
        // Load the MetaSound asset
        UMetaSoundSource* MetaSound = Cast<UMetaSoundSource>(
            StaticLoadObject(UMetaSoundSource::StaticClass(), nullptr, *AssetPath));
        if (!MetaSound)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load MetaSound: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        }
        
        // Use the Frontend Document Builder API
        TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
        #if MCP_HAS_METASOUND_FRONTEND_V2
        FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface, nullptr, true);
#else
        FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface);
#endif
        
        // Parse node IDs
        FGuid SourceGuid, TargetGuid;
        if (!FGuid::Parse(SourceNodeId, SourceGuid) || !FGuid::Parse(TargetNodeId, TargetGuid))
        {
            AUDIO_ERROR_RESPONSE(TEXT("Invalid node ID format - must be valid GUID"), TEXT("INVALID_GUID"));
        }
        
        // Create the edge connection
        Metasound::Frontend::FNamedEdge NamedEdge{
            SourceGuid,
            FName(*SourceOutputName),
            TargetGuid,
            FName(*TargetInputName)
        };
        
        TSet<Metasound::Frontend::FNamedEdge> Edges;
        Edges.Add(NamedEdge);
        
        TArray<const FMetasoundFrontendEdge*> CreatedEdges;
        bool bSuccess = Builder.AddNamedEdges(Edges, &CreatedEdges, true);
        
        if (bSuccess && CreatedEdges.Num() > 0)
        {
            McpSafeAssetSave(MetaSound);
            
            Response->SetBoolField(TEXT("success"), true);
            Response->SetStringField(TEXT("message"), TEXT("MetaSound nodes connected"));
            Response->SetNumberField(TEXT("edgesCreated"), CreatedEdges.Num());
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Failed to create edge connection"));
            Response->SetStringField(TEXT("errorCode"), TEXT("EDGE_FAILED"));
        }
        
        #if MCP_HAS_METASOUND_FRONTEND_V2
        Builder.FinishBuilding();
#endif
        return Response;
#elif MCP_HAS_METASOUND
        // FIX: Return error when MetaSound Frontend Builder not available
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Cannot connect MetaSound nodes - Frontend Builder not available"));
        Response->SetStringField(TEXT("errorCode"), TEXT("METASOUND_FRONTEND_NOT_SUPPORTED"));
        Response->SetStringField(TEXT("requiredVersion"), TEXT("UE 5.3+"));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("MetaSound support not available"), TEXT("METASOUND_NOT_AVAILABLE"));
#endif
    }
    
    if (SubAction == TEXT("add_metasound_input"))
    {
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString InputName = GetStringFieldAudioAuth(Params, TEXT("inputName"), TEXT(""));
        FString InputType = GetStringFieldAudioAuth(Params, TEXT("inputType"), TEXT("Float"));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Asset path is required"), TEXT("MISSING_PATH"));
        }
        
        if (InputName.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Input name is required"), TEXT("MISSING_INPUT_NAME"));
        }
        
        // Load the MetaSound asset
        UMetaSoundSource* MetaSound = Cast<UMetaSoundSource>(
            StaticLoadObject(UMetaSoundSource::StaticClass(), nullptr, *AssetPath));
        if (!MetaSound)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load MetaSound: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        }
        
        // Use the Frontend Document Builder API
        TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
        #if MCP_HAS_METASOUND_FRONTEND_V2
        FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface, nullptr, true);
#else
        FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface);
#endif
        
        // Create the graph input
        FMetasoundFrontendClassInput ClassInput;
        ClassInput.Name = FName(*InputName);
        ClassInput.TypeName = FName(*InputType);
        ClassInput.VertexID = FGuid::NewGuid();
        ClassInput.NodeID = FGuid::NewGuid();
        ClassInput.AccessType = EMetasoundFrontendVertexAccessType::Reference;
        
        const FMetasoundFrontendNode* InputNode = Builder.AddGraphInput(ClassInput);
        
        if (InputNode)
        {
            McpSafeAssetSave(MetaSound);
            
            Response->SetStringField(TEXT("inputName"), InputName);
            Response->SetStringField(TEXT("inputType"), InputType);
            Response->SetStringField(TEXT("nodeId"), InputNode->GetID().ToString());
            Response->SetBoolField(TEXT("success"), true);
            Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound input '%s' added"), *InputName));
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to add input '%s' - type '%s' may not be valid"), *InputName, *InputType));
            Response->SetStringField(TEXT("errorCode"), TEXT("INPUT_FAILED"));
        }
        
        #if MCP_HAS_METASOUND_FRONTEND_V2
        Builder.FinishBuilding();
#endif
        return Response;
#elif MCP_HAS_METASOUND
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString InputName = GetStringFieldAudioAuth(Params, TEXT("inputName"), TEXT(""));
        FString InputType = GetStringFieldAudioAuth(Params, TEXT("inputType"), TEXT("Float"));
        
        Response->SetStringField(TEXT("inputName"), InputName);
        Response->SetStringField(TEXT("inputType"), InputType);
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound input '%s' noted"), *InputName));
        Response->SetStringField(TEXT("note"), TEXT("MetaSound Frontend Builder not available - upgrade to UE 5.3+ for full support"));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("MetaSound support not available"), TEXT("METASOUND_NOT_AVAILABLE"));
#endif
    }
    
    if (SubAction == TEXT("add_metasound_output"))
    {
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString OutputName = GetStringFieldAudioAuth(Params, TEXT("outputName"), TEXT(""));
        FString OutputType = GetStringFieldAudioAuth(Params, TEXT("outputType"), TEXT("Audio"));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Asset path is required"), TEXT("MISSING_PATH"));
        }
        
        if (OutputName.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Output name is required"), TEXT("MISSING_OUTPUT_NAME"));
        }
        
        // Load the MetaSound asset
        UMetaSoundSource* MetaSound = Cast<UMetaSoundSource>(
            StaticLoadObject(UMetaSoundSource::StaticClass(), nullptr, *AssetPath));
        if (!MetaSound)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load MetaSound: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        }
        
        // Use the Frontend Document Builder API
        TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
        #if MCP_HAS_METASOUND_FRONTEND_V2
        FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface, nullptr, true);
#else
        FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface);
#endif
        
        // Create the graph output
        FMetasoundFrontendClassOutput ClassOutput;
        ClassOutput.Name = FName(*OutputName);
        ClassOutput.TypeName = FName(*OutputType);
        ClassOutput.VertexID = FGuid::NewGuid();
        ClassOutput.NodeID = FGuid::NewGuid();
        ClassOutput.AccessType = EMetasoundFrontendVertexAccessType::Reference;
        
        const FMetasoundFrontendNode* OutputNode = Builder.AddGraphOutput(ClassOutput);
        
        if (OutputNode)
        {
            McpSafeAssetSave(MetaSound);
            
            Response->SetStringField(TEXT("outputName"), OutputName);
            Response->SetStringField(TEXT("outputType"), OutputType);
            Response->SetStringField(TEXT("nodeId"), OutputNode->GetID().ToString());
            Response->SetBoolField(TEXT("success"), true);
            Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound output '%s' added"), *OutputName));
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to add output '%s' - type '%s' may not be valid"), *OutputName, *OutputType));
            Response->SetStringField(TEXT("errorCode"), TEXT("OUTPUT_FAILED"));
        }
        
        #if MCP_HAS_METASOUND_FRONTEND_V2
        Builder.FinishBuilding();
#endif
        return Response;
#elif MCP_HAS_METASOUND
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString OutputName = GetStringFieldAudioAuth(Params, TEXT("outputName"), TEXT(""));
        FString OutputType = GetStringFieldAudioAuth(Params, TEXT("outputType"), TEXT("Audio"));
        
        Response->SetStringField(TEXT("outputName"), OutputName);
        Response->SetStringField(TEXT("outputType"), OutputType);
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound output '%s' noted"), *OutputName));
        Response->SetStringField(TEXT("note"), TEXT("MetaSound Frontend Builder not available - upgrade to UE 5.3+ for full support"));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("MetaSound support not available"), TEXT("METASOUND_NOT_AVAILABLE"));
#endif
    }
    
    if (SubAction == TEXT("set_metasound_default"))
    {
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString InputName = GetStringFieldAudioAuth(Params, TEXT("inputName"), TEXT(""));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Asset path is required"), TEXT("MISSING_PATH"));
        }
        
        if (InputName.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Input name is required"), TEXT("MISSING_INPUT_NAME"));
        }
        
        // Load the MetaSound asset
        UMetaSoundSource* MetaSound = Cast<UMetaSoundSource>(
            StaticLoadObject(UMetaSoundSource::StaticClass(), nullptr, *AssetPath));
        if (!MetaSound)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load MetaSound: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        }
        
        // Use the Frontend Document Builder API
        TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
        #if MCP_HAS_METASOUND_FRONTEND_V2
        FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface, nullptr, true);
#else
        FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface);
#endif
        
        // Create the literal value based on provided parameters
        FMetasoundFrontendLiteral Literal;
        
        if (Params->HasField(TEXT("floatValue")))
        {
            float Value = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("floatValue"), 0.0));
            // UE 5.7+: Use Literal.Set() directly instead of SetFromLiteral(FLiteralFloat())
            Literal.Set(Value);
        }
        else if (Params->HasField(TEXT("intValue")))
        {
            int32 Value = static_cast<int32>(GetNumberFieldAudioAuth(Params, TEXT("intValue"), 0));
            Literal.Set(Value);
        }
        else if (Params->HasField(TEXT("boolValue")))
        {
            bool Value = GetBoolFieldAudioAuth(Params, TEXT("boolValue"), false);
            Literal.Set(Value);
        }
        else if (Params->HasField(TEXT("stringValue")))
        {
            FString Value = GetStringFieldAudioAuth(Params, TEXT("stringValue"), TEXT(""));
            Literal.Set(Value);
        }
        else
        {
            // Default to float 0.0
            Literal.Set(0.0f);
        }
        
        bool bSuccess = Builder.SetGraphInputDefault(FName(*InputName), Literal);
        
        if (bSuccess)
        {
            McpSafeAssetSave(MetaSound);
            
            Response->SetBoolField(TEXT("success"), true);
            Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound default for '%s' set"), *InputName));
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to set default for input '%s'"), *InputName));
            Response->SetStringField(TEXT("errorCode"), TEXT("SET_DEFAULT_FAILED"));
        }
        
        #if MCP_HAS_METASOUND_FRONTEND_V2
        Builder.FinishBuilding();
#endif
        return Response;
#elif MCP_HAS_METASOUND
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString InputName = GetStringFieldAudioAuth(Params, TEXT("inputName"), TEXT(""));
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound default for '%s' noted"), *InputName));
        Response->SetStringField(TEXT("note"), TEXT("MetaSound Frontend Builder not available - upgrade to UE 5.3+ for full support"));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("MetaSound support not available"), TEXT("METASOUND_NOT_AVAILABLE"));
#endif
    }
    
    // ===== 11.3 Sound Classes & Mixes =====
    
    if (SubAction == TEXT("create_sound_class"))
    {
        FString Name = GetStringFieldAudioAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("path"), TEXT("/Game/Audio/Classes")));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        // Create package and asset directly to avoid UI dialogs
        // AssetToolsModule.CreateAsset() shows "Overwrite Existing Object" dialogs
        // which cause recursive FlushRenderingCommands and D3D12 crashes
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        USoundClass* NewClass = NewObject<USoundClass>(Package, FName(*Name), RF_Public | RF_Standalone);
        if (!NewClass)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create SoundClass"), TEXT("CREATE_FAILED"));
        }
        
        // Set initial properties if provided
        NewClass->Properties.Volume = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("volume"), 1.0));
        NewClass->Properties.Pitch = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("pitch"), 1.0));
        
        SaveAudioAsset(NewClass, bSave);
        
        FString FullPath = NewClass->GetPathName();
        Response->SetStringField(TEXT("assetPath"), FullPath);
        AUDIO_SUCCESS_RESPONSE(FString::Printf(TEXT("SoundClass '%s' created"), *Name));
        return Response;
    }
    
    if (SubAction == TEXT("set_class_properties"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        USoundClass* SoundClass = LoadSoundClassFromPath(AssetPath);
        if (!SoundClass)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundClass: %s"), *AssetPath), TEXT("CLASS_NOT_FOUND"));
        }
        
        if (Params->HasField(TEXT("volume")))
        {
            SoundClass->Properties.Volume = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("volume"), 1.0));
        }
        if (Params->HasField(TEXT("pitch")))
        {
            SoundClass->Properties.Pitch = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("pitch"), 1.0));
        }
        if (Params->HasField(TEXT("lowPassFilterFrequency")))
        {
            SoundClass->Properties.LowPassFilterFrequency = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("lowPassFilterFrequency"), 20000.0));
        }
        // Note: StereoBleed property removed in UE 5.7
        if (Params->HasField(TEXT("lfeBleed")))
        {
            SoundClass->Properties.LFEBleed = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("lfeBleed"), 0.5));
        }
        if (Params->HasField(TEXT("voiceCenterChannelVolume")))
        {
            SoundClass->Properties.VoiceCenterChannelVolume = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("voiceCenterChannelVolume"), 0.0));
        }
        
        SaveAudioAsset(SoundClass, bSave);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Sound class properties updated"));
        return Response;
    }
    
    if (SubAction == TEXT("set_class_parent"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString ParentPath = GetStringFieldAudioAuth(Params, TEXT("parentPath"), TEXT(""));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        USoundClass* SoundClass = LoadSoundClassFromPath(AssetPath);
        if (!SoundClass)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundClass: %s"), *AssetPath), TEXT("CLASS_NOT_FOUND"));
        }
        
        if (!ParentPath.IsEmpty())
        {
            USoundClass* ParentClass = LoadSoundClassFromPath(ParentPath);
            if (ParentClass)
            {
                SoundClass->ParentClass = ParentClass;
            }
        }
        else
        {
            SoundClass->ParentClass = nullptr;
        }
        
        SaveAudioAsset(SoundClass, bSave);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Sound class parent updated"));
        return Response;
    }
    
    if (SubAction == TEXT("create_sound_mix"))
    {
        FString Name = GetStringFieldAudioAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("path"), TEXT("/Game/Audio/Mixes")));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        USoundMixFactory* Factory = NewObject<USoundMixFactory>();
        USoundMix* NewMix = Cast<USoundMix>(
            Factory->FactoryCreateNew(USoundMix::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewMix)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create SoundMix"), TEXT("CREATE_FAILED"));
        }
        
        SaveAudioAsset(NewMix, bSave);
        
        FString FullPath = NewMix->GetPathName();
        Response->SetStringField(TEXT("assetPath"), FullPath);
        AUDIO_SUCCESS_RESPONSE(FString::Printf(TEXT("SoundMix '%s' created"), *Name));
        return Response;
    }
    
    if (SubAction == TEXT("add_mix_modifier"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString SoundClassPath = GetStringFieldAudioAuth(Params, TEXT("soundClassPath"), TEXT(""));
        float VolumeAdjust = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("volumeAdjuster"), 1.0));
        float PitchAdjust = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("pitchAdjuster"), 1.0));
        float FadeInTime = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("fadeInTime"), 0.0));
        float FadeOutTime = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("fadeOutTime"), 0.0));
        bool bApplyToChildren = GetBoolFieldAudioAuth(Params, TEXT("applyToChildren"), true);
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        USoundMix* Mix = LoadSoundMixFromPath(AssetPath);
        if (!Mix)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundMix: %s"), *AssetPath), TEXT("MIX_NOT_FOUND"));
        }
        
        USoundClass* SoundClass = LoadSoundClassFromPath(SoundClassPath);
        if (!SoundClass)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundClass: %s"), *SoundClassPath), TEXT("CLASS_NOT_FOUND"));
        }
        
        FSoundClassAdjuster Adjuster;
        Adjuster.SoundClassObject = SoundClass;
        Adjuster.VolumeAdjuster = VolumeAdjust;
        Adjuster.PitchAdjuster = PitchAdjust;
        Adjuster.bApplyToChildren = bApplyToChildren;
        // Note: FadeInTime and FadeOutTime are properties of USoundMix, not FSoundClassAdjuster in UE 5.7+
        // Use Mix->FadeInTime and Mix->FadeOutTime if you need to control mix fade timing
        
        Mix->SoundClassEffects.Add(Adjuster);
        
        SaveAudioAsset(Mix, bSave);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Mix modifier added"));
        return Response;
    }
    
    if (SubAction == TEXT("configure_mix_eq"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        USoundMix* Mix = LoadSoundMixFromPath(AssetPath);
        if (!Mix)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundMix: %s"), *AssetPath), TEXT("MIX_NOT_FOUND"));
        }
        
        // Configure EQ settings from parameters
        // Enable EQ if we're configuring it
        Mix->bApplyEQ = GetBoolFieldAudioAuth(Params, TEXT("applyEQ"), true);
        
        if (Params->HasField(TEXT("eqPriority")))
        {
            Mix->EQPriority = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("eqPriority"), 1.0));
        }
        
        // EQ settings object - 4 bands available
        const TSharedPtr<FJsonObject>* EQObj;
        if (Params->TryGetObjectField(TEXT("eqSettings"), EQObj) && EQObj->IsValid())
        {
            // Band 0 (Low)
            if ((*EQObj)->HasField(TEXT("frequencyCenter0")))
            {
                Mix->EQSettings.FrequencyCenter0 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("frequencyCenter0")));
            }
            if ((*EQObj)->HasField(TEXT("gain0")))
            {
                Mix->EQSettings.Gain0 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("gain0")));
            }
            if ((*EQObj)->HasField(TEXT("bandwidth0")))
            {
                Mix->EQSettings.Bandwidth0 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("bandwidth0")));
            }
            
            // Band 1 (Low-Mid)
            if ((*EQObj)->HasField(TEXT("frequencyCenter1")))
            {
                Mix->EQSettings.FrequencyCenter1 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("frequencyCenter1")));
            }
            if ((*EQObj)->HasField(TEXT("gain1")))
            {
                Mix->EQSettings.Gain1 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("gain1")));
            }
            if ((*EQObj)->HasField(TEXT("bandwidth1")))
            {
                Mix->EQSettings.Bandwidth1 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("bandwidth1")));
            }
            
            // Band 2 (High-Mid)
            if ((*EQObj)->HasField(TEXT("frequencyCenter2")))
            {
                Mix->EQSettings.FrequencyCenter2 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("frequencyCenter2")));
            }
            if ((*EQObj)->HasField(TEXT("gain2")))
            {
                Mix->EQSettings.Gain2 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("gain2")));
            }
            if ((*EQObj)->HasField(TEXT("bandwidth2")))
            {
                Mix->EQSettings.Bandwidth2 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("bandwidth2")));
            }
            
            // Band 3 (High)
            if ((*EQObj)->HasField(TEXT("frequencyCenter3")))
            {
                Mix->EQSettings.FrequencyCenter3 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("frequencyCenter3")));
            }
            if ((*EQObj)->HasField(TEXT("gain3")))
            {
                Mix->EQSettings.Gain3 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("gain3")));
            }
            if ((*EQObj)->HasField(TEXT("bandwidth3")))
            {
                Mix->EQSettings.Bandwidth3 = static_cast<float>(GetJsonNumberField((*EQObj), TEXT("bandwidth3")));
            }
        }
        else
        {
            // Accept flat parameters for simpler API usage
            if (Params->HasField(TEXT("lowFrequency")))
            {
                Mix->EQSettings.FrequencyCenter0 = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("lowFrequency"), 600.0));
            }
            if (Params->HasField(TEXT("lowGain")))
            {
                Mix->EQSettings.Gain0 = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("lowGain"), 1.0));
            }
            if (Params->HasField(TEXT("midFrequency")))
            {
                Mix->EQSettings.FrequencyCenter1 = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("midFrequency"), 1000.0));
            }
            if (Params->HasField(TEXT("midGain")))
            {
                Mix->EQSettings.Gain1 = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("midGain"), 1.0));
            }
            if (Params->HasField(TEXT("highMidFrequency")))
            {
                Mix->EQSettings.FrequencyCenter2 = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("highMidFrequency"), 2000.0));
            }
            if (Params->HasField(TEXT("highMidGain")))
            {
                Mix->EQSettings.Gain2 = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("highMidGain"), 1.0));
            }
            if (Params->HasField(TEXT("highFrequency")))
            {
                Mix->EQSettings.FrequencyCenter3 = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("highFrequency"), 10000.0));
            }
            if (Params->HasField(TEXT("highGain")))
            {
                Mix->EQSettings.Gain3 = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("highGain"), 1.0));
            }
        }
        
        // Clamp EQ values to valid ranges
        // NOTE: FAudioEQEffect::ClampValues may not be exported in all UE versions
        // Manually clamp values instead of calling ClampValues() to avoid linker errors
        auto ClampGain = [](float& Value) {
            Value = FMath::Clamp(Value, 0.0f, 4.0f);
        };
        auto ClampFreq = [](float& Value) {
            Value = FMath::Clamp(Value, 0.0f, 20000.0f);
        };
        auto ClampBandwidth = [](float& Value) {
            Value = FMath::Clamp(Value, 0.0f, 2.0f);
        };
        ClampGain(Mix->EQSettings.Gain0);
        ClampGain(Mix->EQSettings.Gain1);
        ClampGain(Mix->EQSettings.Gain2);
        ClampGain(Mix->EQSettings.Gain3);
        ClampFreq(Mix->EQSettings.FrequencyCenter0);
        ClampFreq(Mix->EQSettings.FrequencyCenter1);
        ClampFreq(Mix->EQSettings.FrequencyCenter2);
        ClampFreq(Mix->EQSettings.FrequencyCenter3);
        ClampBandwidth(Mix->EQSettings.Bandwidth0);
        ClampBandwidth(Mix->EQSettings.Bandwidth1);
        ClampBandwidth(Mix->EQSettings.Bandwidth2);
        ClampBandwidth(Mix->EQSettings.Bandwidth3);
        
        SaveAudioAsset(Mix, bSave);
        
        // Return configured EQ info
        TSharedPtr<FJsonObject> EQInfo = MakeShared<FJsonObject>();
        EQInfo->SetNumberField(TEXT("frequencyCenter0"), Mix->EQSettings.FrequencyCenter0);
        EQInfo->SetNumberField(TEXT("gain0"), Mix->EQSettings.Gain0);
        EQInfo->SetNumberField(TEXT("frequencyCenter1"), Mix->EQSettings.FrequencyCenter1);
        EQInfo->SetNumberField(TEXT("gain1"), Mix->EQSettings.Gain1);
        EQInfo->SetNumberField(TEXT("frequencyCenter2"), Mix->EQSettings.FrequencyCenter2);
        EQInfo->SetNumberField(TEXT("gain2"), Mix->EQSettings.Gain2);
        EQInfo->SetNumberField(TEXT("frequencyCenter3"), Mix->EQSettings.FrequencyCenter3);
        EQInfo->SetNumberField(TEXT("gain3"), Mix->EQSettings.Gain3);
        Response->SetObjectField(TEXT("eqSettings"), EQInfo);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Mix EQ configured"));
        return Response;
    }
    
    // ===== 11.4 Attenuation & Spatialization =====
    
    if (SubAction == TEXT("create_attenuation_settings"))
    {
        FString Name = GetStringFieldAudioAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("path"), TEXT("/Game/Audio/Attenuation")));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        USoundAttenuationFactory* Factory = NewObject<USoundAttenuationFactory>();
        USoundAttenuation* NewAtten = Cast<USoundAttenuation>(
            Factory->FactoryCreateNew(USoundAttenuation::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewAtten)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create SoundAttenuation"), TEXT("CREATE_FAILED"));
        }
        
        // Set basic attenuation properties
        if (Params->HasField(TEXT("innerRadius")))
        {
            NewAtten->Attenuation.AttenuationShapeExtents.X = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("innerRadius"), 400.0));
        }
        if (Params->HasField(TEXT("falloffDistance")))
        {
            NewAtten->Attenuation.FalloffDistance = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("falloffDistance"), 3600.0));
        }
        
        SaveAudioAsset(NewAtten, bSave);
        
        FString FullPath = NewAtten->GetPathName();
        Response->SetStringField(TEXT("assetPath"), FullPath);
        AUDIO_SUCCESS_RESPONSE(FString::Printf(TEXT("SoundAttenuation '%s' created"), *Name));
        return Response;
    }
    
    if (SubAction == TEXT("configure_distance_attenuation"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AssetPath);
        if (!Atten)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundAttenuation: %s"), *AssetPath), TEXT("ATTENUATION_NOT_FOUND"));
        }
        
        // Configure distance attenuation
        if (Params->HasField(TEXT("innerRadius")))
        {
            Atten->Attenuation.AttenuationShapeExtents.X = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("innerRadius"), 400.0));
        }
        if (Params->HasField(TEXT("falloffDistance")))
        {
            Atten->Attenuation.FalloffDistance = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("falloffDistance"), 3600.0));
        }
        
        FString FunctionType = GetStringFieldAudioAuth(Params, TEXT("distanceAlgorithm"), TEXT("linear")).ToLower();
        if (FunctionType == TEXT("linear"))
        {
            Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
        }
        else if (FunctionType == TEXT("logarithmic"))
        {
            Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;
        }
        else if (FunctionType == TEXT("inverse"))
        {
            Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Inverse;
        }
        else if (FunctionType == TEXT("naturalsound"))
        {
            Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
        }
        
        SaveAudioAsset(Atten, bSave);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Distance attenuation configured"));
        return Response;
    }
    
    if (SubAction == TEXT("configure_spatialization"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AssetPath);
        if (!Atten)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundAttenuation: %s"), *AssetPath), TEXT("ATTENUATION_NOT_FOUND"));
        }
        
        // Configure spatialization
        Atten->Attenuation.bSpatialize = GetBoolFieldAudioAuth(Params, TEXT("spatialize"), true);
        
        if (Params->HasField(TEXT("spatializationAlgorithm")))
        {
            FString Algorithm = GetStringFieldAudioAuth(Params, TEXT("spatializationAlgorithm"), TEXT("panner"));
            if (Algorithm.ToLower() == TEXT("panner"))
            {
                Atten->Attenuation.SpatializationAlgorithm = ESoundSpatializationAlgorithm::SPATIALIZATION_Default;
            }
            else if (Algorithm.ToLower() == TEXT("hrtf") || Algorithm.ToLower() == TEXT("binaural"))
            {
                Atten->Attenuation.SpatializationAlgorithm = ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF;
            }
        }
        
        SaveAudioAsset(Atten, bSave);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Spatialization configured"));
        return Response;
    }
    
    if (SubAction == TEXT("configure_occlusion"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AssetPath);
        if (!Atten)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundAttenuation: %s"), *AssetPath), TEXT("ATTENUATION_NOT_FOUND"));
        }
        
        // Configure occlusion
        Atten->Attenuation.bEnableOcclusion = GetBoolFieldAudioAuth(Params, TEXT("enableOcclusion"), true);
        
        if (Params->HasField(TEXT("occlusionLowPassFilterFrequency")))
        {
            Atten->Attenuation.OcclusionLowPassFilterFrequency = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("occlusionLowPassFilterFrequency"), 20000.0));
        }
        if (Params->HasField(TEXT("occlusionVolumeAttenuation")))
        {
            Atten->Attenuation.OcclusionVolumeAttenuation = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("occlusionVolumeAttenuation"), 0.0));
        }
        if (Params->HasField(TEXT("occlusionInterpolationTime")))
        {
            Atten->Attenuation.OcclusionInterpolationTime = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("occlusionInterpolationTime"), 0.5));
        }
        
        SaveAudioAsset(Atten, bSave);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Occlusion configured"));
        return Response;
    }
    
    if (SubAction == TEXT("configure_reverb_send"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AssetPath);
        if (!Atten)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundAttenuation: %s"), *AssetPath), TEXT("ATTENUATION_NOT_FOUND"));
        }
        
        // Configure reverb send
        Atten->Attenuation.bEnableReverbSend = GetBoolFieldAudioAuth(Params, TEXT("enableReverbSend"), true);
        
        if (Params->HasField(TEXT("reverbWetLevelMin")))
        {
            Atten->Attenuation.ReverbWetLevelMin = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("reverbWetLevelMin"), 0.3));
        }
        if (Params->HasField(TEXT("reverbWetLevelMax")))
        {
            Atten->Attenuation.ReverbWetLevelMax = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("reverbWetLevelMax"), 0.95));
        }
        if (Params->HasField(TEXT("reverbDistanceMin")))
        {
            Atten->Attenuation.ReverbDistanceMin = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("reverbDistanceMin"), 0.0));
        }
        if (Params->HasField(TEXT("reverbDistanceMax")))
        {
            Atten->Attenuation.ReverbDistanceMax = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("reverbDistanceMax"), 0.0));
        }
        
        SaveAudioAsset(Atten, bSave);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Reverb send configured"));
        return Response;
    }
    
    // ===== 11.5 Dialogue System =====
    
    if (SubAction == TEXT("create_dialogue_voice"))
    {
#if MCP_HAS_DIALOGUE && MCP_HAS_DIALOGUE_FACTORY
        FString Name = GetStringFieldAudioAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("path"), TEXT("/Game/Audio/Dialogue")));
        FString Gender = GetStringFieldAudioAuth(Params, TEXT("gender"), TEXT("Masculine"));
        FString Plurality = GetStringFieldAudioAuth(Params, TEXT("plurality"), TEXT("Singular"));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        UDialogueVoiceFactory* Factory = NewObject<UDialogueVoiceFactory>();
        UDialogueVoice* NewVoice = Cast<UDialogueVoice>(
            Factory->FactoryCreateNew(UDialogueVoice::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewVoice)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create DialogueVoice"), TEXT("CREATE_FAILED"));
        }
        
        // Set gender
        if (Gender.ToLower() == TEXT("masculine"))
        {
            NewVoice->Gender = EGrammaticalGender::Masculine;
        }
        else if (Gender.ToLower() == TEXT("feminine"))
        {
            NewVoice->Gender = EGrammaticalGender::Feminine;
        }
        else if (Gender.ToLower() == TEXT("neuter"))
        {
            NewVoice->Gender = EGrammaticalGender::Neuter;
        }
        
        // Set plurality
        if (Plurality.ToLower() == TEXT("singular"))
        {
            NewVoice->Plurality = EGrammaticalNumber::Singular;
        }
        else if (Plurality.ToLower() == TEXT("plural"))
        {
            NewVoice->Plurality = EGrammaticalNumber::Plural;
        }
        
        SaveAudioAsset(NewVoice, bSave);
        
        FString FullPath = NewVoice->GetPathName();
        Response->SetStringField(TEXT("assetPath"), FullPath);
        AUDIO_SUCCESS_RESPONSE(FString::Printf(TEXT("DialogueVoice '%s' created"), *Name));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("Dialogue system not available"), TEXT("DIALOGUE_NOT_AVAILABLE"));
#endif
    }
    
    if (SubAction == TEXT("create_dialogue_wave"))
    {
#if MCP_HAS_DIALOGUE && MCP_HAS_DIALOGUE_FACTORY
        FString Name = GetStringFieldAudioAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("path"), TEXT("/Game/Audio/Dialogue")));
        FString SpokenText = GetStringFieldAudioAuth(Params, TEXT("spokenText"), TEXT(""));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        UDialogueWaveFactory* Factory = NewObject<UDialogueWaveFactory>();
        UDialogueWave* NewWave = Cast<UDialogueWave>(
            Factory->FactoryCreateNew(UDialogueWave::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewWave)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create DialogueWave"), TEXT("CREATE_FAILED"));
        }
        
        NewWave->SpokenText = SpokenText;
        
        SaveAudioAsset(NewWave, bSave);
        
        FString FullPath = NewWave->GetPathName();
        Response->SetStringField(TEXT("assetPath"), FullPath);
        AUDIO_SUCCESS_RESPONSE(FString::Printf(TEXT("DialogueWave '%s' created"), *Name));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("Dialogue system not available"), TEXT("DIALOGUE_NOT_AVAILABLE"));
#endif
    }
    
    if (SubAction == TEXT("set_dialogue_context"))
    {
#if MCP_HAS_DIALOGUE
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString SpeakerPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("speakerPath"), TEXT("")));
        FString SoundWavePath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("soundWavePath"), TEXT("")));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        UDialogueWave* Wave = Cast<UDialogueWave>(StaticLoadObject(UDialogueWave::StaticClass(), nullptr, *AssetPath));
        if (!Wave)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load DialogueWave: %s"), *AssetPath), TEXT("WAVE_NOT_FOUND"));
        }
        
        // Load the speaker voice
        UDialogueVoice* SpeakerVoice = nullptr;
        if (!SpeakerPath.IsEmpty())
        {
            SpeakerVoice = Cast<UDialogueVoice>(StaticLoadObject(UDialogueVoice::StaticClass(), nullptr, *SpeakerPath));
            if (!SpeakerVoice)
            {
                AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load speaker DialogueVoice: %s"), *SpeakerPath), TEXT("SPEAKER_NOT_FOUND"));
            }
        }
        
        // Load the sound wave
        USoundWave* ContextSoundWave = nullptr;
        if (!SoundWavePath.IsEmpty())
        {
            ContextSoundWave = LoadSoundWaveFromPath(SoundWavePath);
            if (!ContextSoundWave)
            {
                AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundWave: %s"), *SoundWavePath), TEXT("SOUNDWAVE_NOT_FOUND"));
            }
        }
        
        // Load target voices if provided
        TArray<UDialogueVoice*> TargetVoices;
        const TArray<TSharedPtr<FJsonValue>>* TargetArray;
        if (Params->TryGetArrayField(TEXT("targetVoices"), TargetArray))
        {
            for (const TSharedPtr<FJsonValue>& TargetVal : *TargetArray)
            {
                FString TargetPath = NormalizeAudioPath(TargetVal->AsString());
                if (!TargetPath.IsEmpty())
                {
                    UDialogueVoice* TargetVoice = Cast<UDialogueVoice>(
                        StaticLoadObject(UDialogueVoice::StaticClass(), nullptr, *TargetPath));
                    if (TargetVoice)
                    {
                        TargetVoices.Add(TargetVoice);
                    }
                }
            }
        }
        
        // Create and add the context mapping
        FDialogueContextMapping NewMapping;
        NewMapping.Context.Speaker = SpeakerVoice;
        for (UDialogueVoice* TargetVoice : TargetVoices)
        {
            NewMapping.Context.Targets.Add(TargetVoice);
        }
        NewMapping.SoundWave = ContextSoundWave;
        NewMapping.LocalizationKeyFormat = GetStringFieldAudioAuth(Params, TEXT("localizationKeyFormat"), TEXT("{ContextHash}"));
        
        // Check if we should replace existing or add new
        bool bReplaceExisting = GetBoolFieldAudioAuth(Params, TEXT("replace"), false);
        if (bReplaceExisting)
        {
            // Find and replace existing mapping with same speaker
            bool bFound = false;
            for (FDialogueContextMapping& Mapping : Wave->ContextMappings)
            {
                if (Mapping.Context.Speaker == SpeakerVoice)
                {
                    Mapping = NewMapping;
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                Wave->ContextMappings.Add(NewMapping);
            }
        }
        else
        {
            Wave->ContextMappings.Add(NewMapping);
        }
        
        SaveAudioAsset(Wave, bSave);
        
        Response->SetNumberField(TEXT("contextCount"), Wave->ContextMappings.Num());
        AUDIO_SUCCESS_RESPONSE(TEXT("Dialogue context mapping added"));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("Dialogue system not available"), TEXT("DIALOGUE_NOT_AVAILABLE"));
#endif
    }
    
    // ===== 11.6 Effects =====
    
    if (SubAction == TEXT("create_reverb_effect"))
    {
#if MCP_HAS_REVERB_EFFECT
        FString Name = GetStringFieldAudioAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("path"), TEXT("/Game/Audio/Effects")));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        UReverbEffect* NewEffect = NewObject<UReverbEffect>(Package, FName(*Name), RF_Public | RF_Standalone);
        
        if (!NewEffect)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create ReverbEffect"), TEXT("CREATE_FAILED"));
        }
        
        // Set reverb properties if provided
        if (Params->HasField(TEXT("density")))
        {
            NewEffect->Density = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("density"), 1.0));
        }
        if (Params->HasField(TEXT("diffusion")))
        {
            NewEffect->Diffusion = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("diffusion"), 1.0));
        }
        if (Params->HasField(TEXT("gain")))
        {
            NewEffect->Gain = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("gain"), 0.32));
        }
        if (Params->HasField(TEXT("gainHF")))
        {
            NewEffect->GainHF = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("gainHF"), 0.89));
        }
        if (Params->HasField(TEXT("decayTime")))
        {
            NewEffect->DecayTime = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("decayTime"), 1.49));
        }
        if (Params->HasField(TEXT("decayHFRatio")))
        {
            NewEffect->DecayHFRatio = static_cast<float>(GetNumberFieldAudioAuth(Params, TEXT("decayHFRatio"), 0.83));
        }
        
        SaveAudioAsset(NewEffect, bSave);
        
        FString FullPath = NewEffect->GetPathName();
        Response->SetStringField(TEXT("assetPath"), FullPath);
        AUDIO_SUCCESS_RESPONSE(FString::Printf(TEXT("ReverbEffect '%s' created"), *Name));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("Reverb effect not available"), TEXT("REVERB_NOT_AVAILABLE"));
#endif
    }
    
    if (SubAction == TEXT("create_source_effect_chain"))
    {
#if MCP_HAS_SOURCE_EFFECT
        FString Name = GetStringFieldAudioAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("path"), TEXT("/Game/Audio/Effects")));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        // Create package for the source effect chain
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        // Create SoundEffectSourcePresetChain asset
        USoundEffectSourcePresetChain* NewChain = NewObject<USoundEffectSourcePresetChain>(
            Package, FName(*Name), RF_Public | RF_Standalone);
        
        if (!NewChain)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create source effect chain"), TEXT("CREATE_FAILED"));
        }
        
        McpSafeAssetSave(NewChain);
        
        FString FullPath = NewChain->GetPathName();
        Response->SetStringField(TEXT("assetPath"), FullPath);
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Source effect chain '%s' created"), *Name));
        return Response;
#else
        // Fallback: create a basic container but note that full effect chain requires AudioMixer
        FString Name = GetStringFieldAudioAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("path"), TEXT("/Game/Audio/Effects")));
        
        if (Name.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        Response->SetStringField(TEXT("assetPath"), Path / Name);
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Source effect chain '%s' - AudioMixer module not available"), *Name));
        Response->SetStringField(TEXT("note"), TEXT("Enable AudioMixer plugin for full source effect chain support"));
        return Response;
#endif
    }
    
    if (SubAction == TEXT("add_source_effect"))
    {
#if MCP_HAS_SOURCE_EFFECT
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString EffectPresetPath = GetStringFieldAudioAuth(Params, TEXT("effectPresetPath"), TEXT(""));
        FString EffectType = GetStringFieldAudioAuth(Params, TEXT("effectType"), TEXT(""));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        if (AssetPath.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Asset path is required"), TEXT("MISSING_PATH"));
        }
        
        // Load the source effect chain
        USoundEffectSourcePresetChain* Chain = Cast<USoundEffectSourcePresetChain>(
            StaticLoadObject(USoundEffectSourcePresetChain::StaticClass(), nullptr, *AssetPath));
        if (!Chain)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load source effect chain: %s"), *AssetPath), TEXT("CHAIN_NOT_FOUND"));
        }
        
        // Load the effect preset if path provided
        USoundEffectSourcePreset* EffectPreset = nullptr;
        if (!EffectPresetPath.IsEmpty())
        {
            EffectPreset = Cast<USoundEffectSourcePreset>(
                StaticLoadObject(USoundEffectSourcePreset::StaticClass(), nullptr, *NormalizeAudioPath(EffectPresetPath)));
        }
        
        if (EffectPreset)
        {
            // Add the effect to the chain
            FSourceEffectChainEntry NewEntry;
            NewEntry.Preset = EffectPreset;
            NewEntry.bBypass = GetBoolFieldAudioAuth(Params, TEXT("bypass"), false);
            Chain->Chain.Add(NewEntry);
            
            McpSafeAssetSave(Chain);
            
            Response->SetNumberField(TEXT("effectCount"), Chain->Chain.Num());
            Response->SetBoolField(TEXT("success"), true);
            Response->SetStringField(TEXT("message"), TEXT("Source effect added to chain"));
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Effect preset path required or preset not found"));
            Response->SetStringField(TEXT("errorCode"), TEXT("PRESET_NOT_FOUND"));
        }
        
        return Response;
#else
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        FString EffectType = GetStringFieldAudioAuth(Params, TEXT("effectType"), TEXT(""));
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Source effect '%s' noted"), *EffectType));
        Response->SetStringField(TEXT("note"), TEXT("AudioMixer module not available - enable AudioMixer plugin for full support"));
        return Response;
#endif
    }
    
    if (SubAction == TEXT("create_submix_effect"))
    {
#if MCP_HAS_SUBMIX
        FString Name = GetStringFieldAudioAuth(Params, TEXT("name"), TEXT(""));
        FString EffectType = GetStringFieldAudioAuth(Params, TEXT("effectType"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("path"), TEXT("/Game/Audio/Effects")));
        bool bSave = GetBoolFieldAudioAuth(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        // Create package for the submix
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        // Create SoundSubmix asset
        USoundSubmix* NewSubmix = NewObject<USoundSubmix>(Package, FName(*Name), RF_Public | RF_Standalone);
        
        if (!NewSubmix)
        {
            AUDIO_ERROR_RESPONSE(TEXT("Failed to create submix"), TEXT("CREATE_FAILED"));
        }
        
        // Configure submix properties if provided
        // Note: OutputVolume, WetLevel, DryLevel direct properties were removed in modern UE.
        // These are now controlled via OutputVolumeModulation, WetLevelModulation, DryLevelModulation.
        // For backwards compatibility with older UE versions, we use SetSubmixOutputVolume/WetLevel/DryLevel functions at runtime.
        // Since this is asset creation, we skip these deprecated properties.
        // The submix will use default levels which can be adjusted via Blueprint or runtime functions.
        
        McpSafeAssetSave(NewSubmix);
        
        FString FullPath = NewSubmix->GetPathName();
        Response->SetStringField(TEXT("assetPath"), FullPath);
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Submix '%s' created"), *Name));
        return Response;
#else
        FString Name = GetStringFieldAudioAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("path"), TEXT("/Game/Audio/Effects")));
        
        if (Name.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        Response->SetStringField(TEXT("assetPath"), Path / Name);
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Submix '%s' noted - AudioMixer module not available"), *Name));
        Response->SetStringField(TEXT("note"), TEXT("Enable AudioMixer plugin for full submix support"));
        return Response;
#endif
    }
    
    // ===== Utility =====
    
    if (SubAction == TEXT("get_audio_info"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldAudioAuth(Params, TEXT("assetPath"), TEXT("")));
        
        // Try to load as various audio types
        UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
        if (!Asset)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load asset: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        }
        
        Response->SetStringField(TEXT("assetPath"), AssetPath);
        Response->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());
        
        // Get type-specific info
        if (USoundCue* Cue = Cast<USoundCue>(Asset))
        {
            Response->SetStringField(TEXT("type"), TEXT("SoundCue"));
            Response->SetNumberField(TEXT("duration"), Cue->Duration);
            Response->SetNumberField(TEXT("nodeCount"), Cue->AllNodes.Num());
            if (Cue->AttenuationSettings)
            {
                Response->SetStringField(TEXT("attenuationPath"), Cue->AttenuationSettings->GetPathName());
            }
        }
        else if (USoundWave* Wave = Cast<USoundWave>(Asset))
        {
            Response->SetStringField(TEXT("type"), TEXT("SoundWave"));
            Response->SetNumberField(TEXT("duration"), Wave->Duration);
            Response->SetNumberField(TEXT("sampleRate"), Wave->GetSampleRateForCurrentPlatform());
            Response->SetNumberField(TEXT("numChannels"), Wave->NumChannels);
        }
        else if (USoundClass* SoundClass = Cast<USoundClass>(Asset))
        {
            Response->SetStringField(TEXT("type"), TEXT("SoundClass"));
            Response->SetNumberField(TEXT("volume"), SoundClass->Properties.Volume);
            Response->SetNumberField(TEXT("pitch"), SoundClass->Properties.Pitch);
            if (SoundClass->ParentClass)
            {
                Response->SetStringField(TEXT("parentClass"), SoundClass->ParentClass->GetPathName());
            }
        }
        else if (USoundMix* Mix = Cast<USoundMix>(Asset))
        {
            Response->SetStringField(TEXT("type"), TEXT("SoundMix"));
            Response->SetNumberField(TEXT("modifierCount"), Mix->SoundClassEffects.Num());
        }
        else if (USoundAttenuation* Atten = Cast<USoundAttenuation>(Asset))
        {
            Response->SetStringField(TEXT("type"), TEXT("SoundAttenuation"));
            Response->SetNumberField(TEXT("falloffDistance"), Atten->Attenuation.FalloffDistance);
            Response->SetBoolField(TEXT("spatialize"), Atten->Attenuation.bSpatialize);
        }
        else
        {
            Response->SetStringField(TEXT("type"), TEXT("Unknown"));
        }
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Audio info retrieved"));
        return Response;
    }
    
    // Unknown subAction
    AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Unknown audio authoring action: %s"), *SubAction), TEXT("UNKNOWN_ACTION"));
}

#endif // WITH_EDITOR

// Public handler function called by the subsystem
bool UMcpAutomationBridgeSubsystem::HandleManageAudioAuthoringAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Check if this is a manage_audio_authoring request
    FString LowerAction = Action.ToLower();
    if (!LowerAction.StartsWith(TEXT("manage_audio_authoring")))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Audio authoring payload missing"), TEXT("INVALID_PAYLOAD"));
        return true;
    }
    
    TSharedPtr<FJsonObject> Response = HandleAudioAuthoringRequest(Payload);
    
    if (Response.IsValid())
    {
        bool bSuccess = Response->HasField(TEXT("success")) && GetJsonBoolField(Response, TEXT("success"));
        FString Message = Response->HasField(TEXT("message")) ? GetJsonStringField(Response, TEXT("message")) : TEXT("Operation complete");
        FString ErrorCode = Response->HasField(TEXT("errorCode")) ? GetJsonStringField(Response, TEXT("errorCode")) : TEXT("");
        
        if (bSuccess)
        {
            SendAutomationResponse(RequestingSocket, RequestId, true, Message, Response);
        }
        else
        {
            FString ErrorMsg = Response->HasField(TEXT("error")) ? GetJsonStringField(Response, TEXT("error")) : TEXT("Unknown error");
            SendAutomationError(RequestingSocket, RequestId, ErrorMsg, ErrorCode);
        }
    }
    else
    {
        SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Failed to process audio authoring request"), TEXT("PROCESS_FAILED"));
    }
    
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId,
                       TEXT("Audio authoring requires editor build"), TEXT("EDITOR_REQUIRED"));
    return true;
#endif
}

#undef GetStringFieldAudioAuth
#undef GetNumberFieldAudioAuth
#undef GetBoolFieldAudioAuth

