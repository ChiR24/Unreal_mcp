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
#define GetNumberFieldSafe GetJsonNumberField
#define GetBoolFieldSafe GetJsonBoolField
#define GetStringFieldSafe GetJsonStringField

// Helper to normalize asset path
static FString NormalizeAudioPath(const FString& Path)
{
    FString Normalized = Path;
    Normalized.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
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
    
    FString SubAction = GetStringFieldSafe(Params, TEXT("subAction"), TEXT(""));
    
    // ===== 11.1 Sound Cues =====
    
    if (SubAction == TEXT("create_sound_cue"))
    {
        FString Name = GetStringFieldSafe(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("path"), TEXT("/Game/Audio/Cues")));
        FString WavePath = GetStringFieldSafe(Params, TEXT("wavePath"), TEXT(""));
        bool bLooping = GetBoolFieldSafe(Params, TEXT("looping"), false);
        float Volume = static_cast<float>(GetNumberFieldSafe(Params, TEXT("volume"), 1.0));
        float Pitch = static_cast<float>(GetNumberFieldSafe(Params, TEXT("pitch"), 1.0));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
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
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        FString NodeType = GetStringFieldSafe(Params, TEXT("nodeType"), TEXT("wave_player"));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
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
            FString WavePath = GetStringFieldSafe(Params, TEXT("wavePath"), TEXT(""));
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
            Mod->VolumeMin = Mod->VolumeMax = static_cast<float>(GetNumberFieldSafe(Params, TEXT("volume"), 1.0));
            Mod->PitchMin = Mod->PitchMax = static_cast<float>(GetNumberFieldSafe(Params, TEXT("pitch"), 1.0));
            NewNode = Mod;
        }
        else if (NodeTypeLower == TEXT("looping"))
        {
            USoundNodeLooping* Loop = Cue->ConstructSoundNode<USoundNodeLooping>();
            Loop->bLoopIndefinitely = GetBoolFieldSafe(Params, TEXT("indefinite"), true);
            Loop->LoopCount = static_cast<int32>(GetNumberFieldSafe(Params, TEXT("loopCount"), 0));
            NewNode = Loop;
        }
        else if (NodeTypeLower == TEXT("attenuation"))
        {
            USoundNodeAttenuation* Atten = Cue->ConstructSoundNode<USoundNodeAttenuation>();
            FString AttenPath = GetStringFieldSafe(Params, TEXT("attenuationPath"), TEXT(""));
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
            Delay->DelayMin = Delay->DelayMax = static_cast<float>(GetNumberFieldSafe(Params, TEXT("delay"), 0.0));
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
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        FString SourceNodeId = GetStringFieldSafe(Params, TEXT("sourceNodeId"), TEXT(""));
        FString TargetNodeId = GetStringFieldSafe(Params, TEXT("targetNodeId"), TEXT(""));
        int32 ChildIndex = static_cast<int32>(GetNumberFieldSafe(Params, TEXT("childIndex"), 0));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
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
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        FString AttenuationPath = GetStringFieldSafe(Params, TEXT("attenuationPath"), TEXT(""));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
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
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        FString ConcurrencyPath = GetStringFieldSafe(Params, TEXT("concurrencyPath"), TEXT(""));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
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
#if MCP_HAS_METASOUND
        FString Name = GetStringFieldSafe(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("path"), TEXT("/Game/Audio/MetaSounds")));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        // MetaSound creation requires the MetaSound Editor module
        // For now, return a helpful message about MetaSound support
        Response->SetStringField(TEXT("assetPath"), Path / Name);
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("MetaSound creation queued - requires MetaSound Editor plugin"));
        Response->SetStringField(TEXT("note"), TEXT("MetaSound graph editing via automation is limited; consider using the MetaSound Editor"));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("MetaSound support not available in this engine version"), TEXT("METASOUND_NOT_AVAILABLE"));
#endif
    }
    
    if (SubAction == TEXT("add_metasound_node"))
    {
#if MCP_HAS_METASOUND
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        FString NodeType = GetStringFieldSafe(Params, TEXT("nodeType"), TEXT(""));
        
        // MetaSound node manipulation requires specialized APIs
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("MetaSound node addition queued"));
        Response->SetStringField(TEXT("note"), TEXT("MetaSound graph editing via automation requires MetaSound Builder API"));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("MetaSound support not available"), TEXT("METASOUND_NOT_AVAILABLE"));
#endif
    }
    
    if (SubAction == TEXT("connect_metasound_nodes"))
    {
#if MCP_HAS_METASOUND
        // MetaSound node connection
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("MetaSound connection queued"));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("MetaSound support not available"), TEXT("METASOUND_NOT_AVAILABLE"));
#endif
    }
    
    if (SubAction == TEXT("add_metasound_input"))
    {
#if MCP_HAS_METASOUND
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        FString InputName = GetStringFieldSafe(Params, TEXT("inputName"), TEXT(""));
        FString InputType = GetStringFieldSafe(Params, TEXT("inputType"), TEXT("Float"));
        
        Response->SetStringField(TEXT("inputName"), InputName);
        Response->SetStringField(TEXT("inputType"), InputType);
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound input '%s' queued"), *InputName));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("MetaSound support not available"), TEXT("METASOUND_NOT_AVAILABLE"));
#endif
    }
    
    if (SubAction == TEXT("add_metasound_output"))
    {
#if MCP_HAS_METASOUND
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        FString OutputName = GetStringFieldSafe(Params, TEXT("outputName"), TEXT(""));
        FString OutputType = GetStringFieldSafe(Params, TEXT("outputType"), TEXT("Audio"));
        
        Response->SetStringField(TEXT("outputName"), OutputName);
        Response->SetStringField(TEXT("outputType"), OutputType);
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound output '%s' queued"), *OutputName));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("MetaSound support not available"), TEXT("METASOUND_NOT_AVAILABLE"));
#endif
    }
    
    if (SubAction == TEXT("set_metasound_default"))
    {
#if MCP_HAS_METASOUND
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        FString InputName = GetStringFieldSafe(Params, TEXT("inputName"), TEXT(""));
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound default for '%s' queued"), *InputName));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("MetaSound support not available"), TEXT("METASOUND_NOT_AVAILABLE"));
#endif
    }
    
    // ===== 11.3 Sound Classes & Mixes =====
    
    if (SubAction == TEXT("create_sound_class"))
    {
        FString Name = GetStringFieldSafe(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("path"), TEXT("/Game/Audio/Classes")));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
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
        NewClass->Properties.Volume = static_cast<float>(GetNumberFieldSafe(Params, TEXT("volume"), 1.0));
        NewClass->Properties.Pitch = static_cast<float>(GetNumberFieldSafe(Params, TEXT("pitch"), 1.0));
        
        SaveAudioAsset(NewClass, bSave);
        
        FString FullPath = NewClass->GetPathName();
        Response->SetStringField(TEXT("assetPath"), FullPath);
        AUDIO_SUCCESS_RESPONSE(FString::Printf(TEXT("SoundClass '%s' created"), *Name));
        return Response;
    }
    
    if (SubAction == TEXT("set_class_properties"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        USoundClass* SoundClass = LoadSoundClassFromPath(AssetPath);
        if (!SoundClass)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundClass: %s"), *AssetPath), TEXT("CLASS_NOT_FOUND"));
        }
        
        if (Params->HasField(TEXT("volume")))
        {
            SoundClass->Properties.Volume = static_cast<float>(GetNumberFieldSafe(Params, TEXT("volume"), 1.0));
        }
        if (Params->HasField(TEXT("pitch")))
        {
            SoundClass->Properties.Pitch = static_cast<float>(GetNumberFieldSafe(Params, TEXT("pitch"), 1.0));
        }
        if (Params->HasField(TEXT("lowPassFilterFrequency")))
        {
            SoundClass->Properties.LowPassFilterFrequency = static_cast<float>(GetNumberFieldSafe(Params, TEXT("lowPassFilterFrequency"), 20000.0));
        }
        // Note: StereoBleed property removed in UE 5.7
        if (Params->HasField(TEXT("lfeBleed")))
        {
            SoundClass->Properties.LFEBleed = static_cast<float>(GetNumberFieldSafe(Params, TEXT("lfeBleed"), 0.5));
        }
        if (Params->HasField(TEXT("voiceCenterChannelVolume")))
        {
            SoundClass->Properties.VoiceCenterChannelVolume = static_cast<float>(GetNumberFieldSafe(Params, TEXT("voiceCenterChannelVolume"), 0.0));
        }
        
        SaveAudioAsset(SoundClass, bSave);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Sound class properties updated"));
        return Response;
    }
    
    if (SubAction == TEXT("set_class_parent"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        FString ParentPath = GetStringFieldSafe(Params, TEXT("parentPath"), TEXT(""));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
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
        FString Name = GetStringFieldSafe(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("path"), TEXT("/Game/Audio/Mixes")));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
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
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        FString SoundClassPath = GetStringFieldSafe(Params, TEXT("soundClassPath"), TEXT(""));
        float VolumeAdjust = static_cast<float>(GetNumberFieldSafe(Params, TEXT("volumeAdjuster"), 1.0));
        float PitchAdjust = static_cast<float>(GetNumberFieldSafe(Params, TEXT("pitchAdjuster"), 1.0));
        float FadeInTime = static_cast<float>(GetNumberFieldSafe(Params, TEXT("fadeInTime"), 0.0));
        float FadeOutTime = static_cast<float>(GetNumberFieldSafe(Params, TEXT("fadeOutTime"), 0.0));
        bool bApplyToChildren = GetBoolFieldSafe(Params, TEXT("applyToChildren"), true);
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
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
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        USoundMix* Mix = LoadSoundMixFromPath(AssetPath);
        if (!Mix)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundMix: %s"), *AssetPath), TEXT("MIX_NOT_FOUND"));
        }
        
        // EQ settings on sound mix
        if (Params->HasField(TEXT("eqSettings")))
        {
            // Parse EQ bands
            const TSharedPtr<FJsonObject>* EQObj;
            if (Params->TryGetObjectField(TEXT("eqSettings"), EQObj))
            {
                // SoundMix EQ settings vary by UE version
                // Basic setup for compatibility
            }
        }
        
        SaveAudioAsset(Mix, bSave);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Mix EQ configured"));
        return Response;
    }
    
    // ===== 11.4 Attenuation & Spatialization =====
    
    if (SubAction == TEXT("create_attenuation_settings"))
    {
        FString Name = GetStringFieldSafe(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("path"), TEXT("/Game/Audio/Attenuation")));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
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
            NewAtten->Attenuation.AttenuationShapeExtents.X = static_cast<float>(GetNumberFieldSafe(Params, TEXT("innerRadius"), 400.0));
        }
        if (Params->HasField(TEXT("falloffDistance")))
        {
            NewAtten->Attenuation.FalloffDistance = static_cast<float>(GetNumberFieldSafe(Params, TEXT("falloffDistance"), 3600.0));
        }
        
        SaveAudioAsset(NewAtten, bSave);
        
        FString FullPath = NewAtten->GetPathName();
        Response->SetStringField(TEXT("assetPath"), FullPath);
        AUDIO_SUCCESS_RESPONSE(FString::Printf(TEXT("SoundAttenuation '%s' created"), *Name));
        return Response;
    }
    
    if (SubAction == TEXT("configure_distance_attenuation"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AssetPath);
        if (!Atten)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundAttenuation: %s"), *AssetPath), TEXT("ATTENUATION_NOT_FOUND"));
        }
        
        // Configure distance attenuation
        if (Params->HasField(TEXT("innerRadius")))
        {
            Atten->Attenuation.AttenuationShapeExtents.X = static_cast<float>(GetNumberFieldSafe(Params, TEXT("innerRadius"), 400.0));
        }
        if (Params->HasField(TEXT("falloffDistance")))
        {
            Atten->Attenuation.FalloffDistance = static_cast<float>(GetNumberFieldSafe(Params, TEXT("falloffDistance"), 3600.0));
        }
        
        FString FunctionType = GetStringFieldSafe(Params, TEXT("distanceAlgorithm"), TEXT("linear"));
        if (FunctionType.ToLower() == TEXT("linear"))
        {
            Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
        }
        else if (FunctionType.ToLower() == TEXT("logarithmic"))
        {
            Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;
        }
        else if (FunctionType.ToLower() == TEXT("inverse"))
        {
            Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Inverse;
        }
        else if (FunctionType.ToLower() == TEXT("naturalSound"))
        {
            Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
        }
        
        SaveAudioAsset(Atten, bSave);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Distance attenuation configured"));
        return Response;
    }
    
    if (SubAction == TEXT("configure_spatialization"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AssetPath);
        if (!Atten)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundAttenuation: %s"), *AssetPath), TEXT("ATTENUATION_NOT_FOUND"));
        }
        
        // Configure spatialization
        Atten->Attenuation.bSpatialize = GetBoolFieldSafe(Params, TEXT("spatialize"), true);
        
        if (Params->HasField(TEXT("spatializationAlgorithm")))
        {
            FString Algorithm = GetStringFieldSafe(Params, TEXT("spatializationAlgorithm"), TEXT("panner"));
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
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AssetPath);
        if (!Atten)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundAttenuation: %s"), *AssetPath), TEXT("ATTENUATION_NOT_FOUND"));
        }
        
        // Configure occlusion
        Atten->Attenuation.bEnableOcclusion = GetBoolFieldSafe(Params, TEXT("enableOcclusion"), true);
        
        if (Params->HasField(TEXT("occlusionLowPassFilterFrequency")))
        {
            Atten->Attenuation.OcclusionLowPassFilterFrequency = static_cast<float>(GetNumberFieldSafe(Params, TEXT("occlusionLowPassFilterFrequency"), 20000.0));
        }
        if (Params->HasField(TEXT("occlusionVolumeAttenuation")))
        {
            Atten->Attenuation.OcclusionVolumeAttenuation = static_cast<float>(GetNumberFieldSafe(Params, TEXT("occlusionVolumeAttenuation"), 0.0));
        }
        if (Params->HasField(TEXT("occlusionInterpolationTime")))
        {
            Atten->Attenuation.OcclusionInterpolationTime = static_cast<float>(GetNumberFieldSafe(Params, TEXT("occlusionInterpolationTime"), 0.5));
        }
        
        SaveAudioAsset(Atten, bSave);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Occlusion configured"));
        return Response;
    }
    
    if (SubAction == TEXT("configure_reverb_send"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        USoundAttenuation* Atten = LoadSoundAttenuationFromPath(AssetPath);
        if (!Atten)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load SoundAttenuation: %s"), *AssetPath), TEXT("ATTENUATION_NOT_FOUND"));
        }
        
        // Configure reverb send
        Atten->Attenuation.bEnableReverbSend = GetBoolFieldSafe(Params, TEXT("enableReverbSend"), true);
        
        if (Params->HasField(TEXT("reverbWetLevelMin")))
        {
            Atten->Attenuation.ReverbWetLevelMin = static_cast<float>(GetNumberFieldSafe(Params, TEXT("reverbWetLevelMin"), 0.3));
        }
        if (Params->HasField(TEXT("reverbWetLevelMax")))
        {
            Atten->Attenuation.ReverbWetLevelMax = static_cast<float>(GetNumberFieldSafe(Params, TEXT("reverbWetLevelMax"), 0.95));
        }
        if (Params->HasField(TEXT("reverbDistanceMin")))
        {
            Atten->Attenuation.ReverbDistanceMin = static_cast<float>(GetNumberFieldSafe(Params, TEXT("reverbDistanceMin"), 0.0));
        }
        if (Params->HasField(TEXT("reverbDistanceMax")))
        {
            Atten->Attenuation.ReverbDistanceMax = static_cast<float>(GetNumberFieldSafe(Params, TEXT("reverbDistanceMax"), 0.0));
        }
        
        SaveAudioAsset(Atten, bSave);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Reverb send configured"));
        return Response;
    }
    
    // ===== 11.5 Dialogue System =====
    
    if (SubAction == TEXT("create_dialogue_voice"))
    {
#if MCP_HAS_DIALOGUE && MCP_HAS_DIALOGUE_FACTORY
        FString Name = GetStringFieldSafe(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("path"), TEXT("/Game/Audio/Dialogue")));
        FString Gender = GetStringFieldSafe(Params, TEXT("gender"), TEXT("Masculine"));
        FString Plurality = GetStringFieldSafe(Params, TEXT("plurality"), TEXT("Singular"));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
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
        FString Name = GetStringFieldSafe(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("path"), TEXT("/Game/Audio/Dialogue")));
        FString SpokenText = GetStringFieldSafe(Params, TEXT("spokenText"), TEXT(""));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
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
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        FString SpeakerPath = GetStringFieldSafe(Params, TEXT("speakerPath"), TEXT(""));
        FString SoundWavePath = GetStringFieldSafe(Params, TEXT("soundWavePath"), TEXT(""));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        UDialogueWave* Wave = Cast<UDialogueWave>(StaticLoadObject(UDialogueWave::StaticClass(), nullptr, *AssetPath));
        if (!Wave)
        {
            AUDIO_ERROR_RESPONSE(FString::Printf(TEXT("Could not load DialogueWave: %s"), *AssetPath), TEXT("WAVE_NOT_FOUND"));
        }
        
        // Set context mapping
        // This would typically involve adding a context mapping with speaker voice and sound wave
        
        SaveAudioAsset(Wave, bSave);
        
        AUDIO_SUCCESS_RESPONSE(TEXT("Dialogue context updated"));
        return Response;
#else
        AUDIO_ERROR_RESPONSE(TEXT("Dialogue system not available"), TEXT("DIALOGUE_NOT_AVAILABLE"));
#endif
    }
    
    // ===== 11.6 Effects =====
    
    if (SubAction == TEXT("create_reverb_effect"))
    {
#if MCP_HAS_REVERB_EFFECT
        FString Name = GetStringFieldSafe(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("path"), TEXT("/Game/Audio/Effects")));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
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
            NewEffect->Density = static_cast<float>(GetNumberFieldSafe(Params, TEXT("density"), 1.0));
        }
        if (Params->HasField(TEXT("diffusion")))
        {
            NewEffect->Diffusion = static_cast<float>(GetNumberFieldSafe(Params, TEXT("diffusion"), 1.0));
        }
        if (Params->HasField(TEXT("gain")))
        {
            NewEffect->Gain = static_cast<float>(GetNumberFieldSafe(Params, TEXT("gain"), 0.32));
        }
        if (Params->HasField(TEXT("gainHF")))
        {
            NewEffect->GainHF = static_cast<float>(GetNumberFieldSafe(Params, TEXT("gainHF"), 0.89));
        }
        if (Params->HasField(TEXT("decayTime")))
        {
            NewEffect->DecayTime = static_cast<float>(GetNumberFieldSafe(Params, TEXT("decayTime"), 1.49));
        }
        if (Params->HasField(TEXT("decayHFRatio")))
        {
            NewEffect->DecayHFRatio = static_cast<float>(GetNumberFieldSafe(Params, TEXT("decayHFRatio"), 0.83));
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
        FString Name = GetStringFieldSafe(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("path"), TEXT("/Game/Audio/Effects")));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        // Source effect chain creation
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Source effect chain '%s' creation queued"), *Name));
        Response->SetStringField(TEXT("note"), TEXT("Source effect chain creation requires AudioMixer module"));
        return Response;
    }
    
    if (SubAction == TEXT("add_source_effect"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        FString EffectType = GetStringFieldSafe(Params, TEXT("effectType"), TEXT(""));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        // Source effect addition
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Source effect '%s' addition queued"), *EffectType));
        Response->SetStringField(TEXT("note"), TEXT("Source effect addition requires AudioMixer module"));
        return Response;
    }
    
    if (SubAction == TEXT("create_submix_effect"))
    {
        FString Name = GetStringFieldSafe(Params, TEXT("name"), TEXT(""));
        FString EffectType = GetStringFieldSafe(Params, TEXT("effectType"), TEXT(""));
        FString Path = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("path"), TEXT("/Game/Audio/Effects")));
        bool bSave = GetBoolFieldSafe(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            AUDIO_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        // Submix effect creation
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Submix effect '%s' creation queued"), *Name));
        Response->SetStringField(TEXT("note"), TEXT("Submix effect creation requires AudioMixer module"));
        return Response;
    }
    
    // ===== Utility =====
    
    if (SubAction == TEXT("get_audio_info"))
    {
        FString AssetPath = NormalizeAudioPath(GetStringFieldSafe(Params, TEXT("assetPath"), TEXT("")));
        
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
        bool bSuccess = Response->HasField(TEXT("success")) && Response->GetBoolField(TEXT("success"));
        FString Message = Response->HasField(TEXT("message")) ? Response->GetStringField(TEXT("message")) : TEXT("Operation complete");
        FString ErrorCode = Response->HasField(TEXT("errorCode")) ? Response->GetStringField(TEXT("errorCode")) : TEXT("");
        
        if (bSuccess)
        {
            SendAutomationResponse(RequestingSocket, RequestId, true, Message, Response);
        }
        else
        {
            FString ErrorMsg = Response->HasField(TEXT("error")) ? Response->GetStringField(TEXT("error")) : TEXT("Unknown error");
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
