#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleSoundCueNodeActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction == TEXT("add_cue_node"))
	{
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString NodeType = McpHandlerUtils::GetOptionalString(Params, TEXT("nodeType"), TEXT("wave_player"));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);
		USoundCue* Cue = LoadSoundCueFromPath(AssetPath);
		if (!Cue)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("CUE_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundCue: %s"), *AssetPath));
		}
		if (!Cue->SoundCueGraph)
		{
			Cue->CreateGraph();
		}

		USoundNode* NewNode = nullptr;
		FString NodeTypeLower = NodeType.ToLower();
		if (NodeTypeLower == TEXT("wave_player") || NodeTypeLower == TEXT("waveplayer"))
		{
			USoundNodeWavePlayer* Player = Cue->ConstructSoundNode<USoundNodeWavePlayer>();
			FString WavePath = McpHandlerUtils::GetOptionalString(Params, TEXT("wavePath"), TEXT(""));
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
		else if (NodeTypeLower == TEXT("mixer")) { NewNode = Cue->ConstructSoundNode<USoundNodeMixer>(); }
		else if (NodeTypeLower == TEXT("random")) { NewNode = Cue->ConstructSoundNode<USoundNodeRandom>(); }
		else if (NodeTypeLower == TEXT("modulator"))
		{
			USoundNodeModulator* Mod = Cue->ConstructSoundNode<USoundNodeModulator>();
			Mod->VolumeMin = Mod->VolumeMax = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("volume"), 1.0));
			Mod->PitchMin = Mod->PitchMax = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("pitch"), 1.0));
			NewNode = Mod;
		}
		else if (NodeTypeLower == TEXT("looping"))
		{
			USoundNodeLooping* Loop = Cue->ConstructSoundNode<USoundNodeLooping>();
			Loop->bLoopIndefinitely = McpHandlerUtils::GetOptionalBool(Params, TEXT("indefinite"), true);
			Loop->LoopCount = static_cast<int32>(McpHandlerUtils::GetOptionalInt(Params, TEXT("loopCount"), 0));
			NewNode = Loop;
		}
		else if (NodeTypeLower == TEXT("attenuation"))
		{
			USoundNodeAttenuation* Atten = Cue->ConstructSoundNode<USoundNodeAttenuation>();
			FString AttenPath = McpHandlerUtils::GetOptionalString(Params, TEXT("attenuationPath"), TEXT(""));
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
		else if (NodeTypeLower == TEXT("concatenator")) { NewNode = Cue->ConstructSoundNode<USoundNodeConcatenator>(); }
		else if (NodeTypeLower == TEXT("delay"))
		{
			USoundNodeDelay* Delay = Cue->ConstructSoundNode<USoundNodeDelay>();
			Delay->DelayMin = Delay->DelayMax = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("delay"), 0.0));
			NewNode = Delay;
		}
		else if (NodeTypeLower == TEXT("switch")) { NewNode = Cue->ConstructSoundNode<USoundNodeSwitch>(); }
		else if (NodeTypeLower == TEXT("branch")) { NewNode = Cue->ConstructSoundNode<USoundNodeBranch>(); }
		else
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("UNKNOWN_NODE_TYPE"), FString::Printf(TEXT("Unknown node type: %s"), *NodeType));
		}

		if (!NewNode)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_NODE_FAILED"), TEXT("Failed to create sound node"));
		}

		SaveAudioAsset(Cue, bSave);
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("nodeId"), NewNode->GetName());
		McpHandlerUtils::AddVerification(Response, Cue);
		return Response;
	}

	if (SubAction == TEXT("connect_cue_nodes"))
	{
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString SourceNodeId = McpHandlerUtils::GetOptionalString(Params, TEXT("sourceNodeId"), TEXT(""));
		FString TargetNodeId = McpHandlerUtils::GetOptionalString(Params, TEXT("targetNodeId"), TEXT(""));
		int32 ChildIndex = static_cast<int32>(McpHandlerUtils::GetOptionalInt(Params, TEXT("childIndex"), 0));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);
		USoundCue* Cue = LoadSoundCueFromPath(AssetPath);
		if (!Cue)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("CUE_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundCue: %s"), *AssetPath));
		}
		if (!Cue->SoundCueGraph)
		{
			Cue->CreateGraph();
		}

		USoundNode* SourceNode = nullptr;
		USoundNode* TargetNode = nullptr;
		for (USoundNode* Node : Cue->AllNodes)
		{
			if (Node && Node->GetName() == SourceNodeId) { SourceNode = Node; }
			if (Node && Node->GetName() == TargetNodeId) { TargetNode = Node; }
		}
		if (!SourceNode)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("SOURCE_NODE_NOT_FOUND"), FString::Printf(TEXT("Source node not found: %s"), *SourceNodeId));
		}
		if (!TargetNode)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("TARGET_NODE_NOT_FOUND"), FString::Printf(TEXT("Target node not found: %s"), *TargetNodeId));
		}

		USoundCueGraphNode* SourceGraphNode = Cast<USoundCueGraphNode>(SourceNode->GetGraphNode());
		USoundCueGraphNode* TargetGraphNode = Cast<USoundCueGraphNode>(TargetNode->GetGraphNode());
		if (!SourceGraphNode || !TargetGraphNode)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("GRAPH_NODE_ERROR"), TEXT("Could not get graph nodes for sound nodes"));
		}

		UEdGraphPin* TargetOutputPin = TargetGraphNode->GetOutputPin();
		if (!TargetOutputPin)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("GRAPH_PIN_ERROR"), TEXT("Target node has no output pin"));
		}

		TArray<UEdGraphPin*> InputPins;
		SourceGraphNode->GetInputPins(InputPins);
		while (InputPins.Num() <= ChildIndex)
		{
			if (SourceNode->GetMaxChildNodes() <= InputPins.Num())
			{
				return McpHandlerUtils::BuildErrorResponse(TEXT("MAX_CHILDREN_EXCEEDED"), FString::Printf(TEXT("Source node supports max %d children, requested index %d"), SourceNode->GetMaxChildNodes(), ChildIndex));
			}
			SourceNode->InsertChildNode(SourceNode->ChildNodes.Num());
			InputPins.Empty();
			SourceGraphNode->GetInputPins(InputPins);
		}

		if (ChildIndex < InputPins.Num() && InputPins[ChildIndex])
		{
			InputPins[ChildIndex]->BreakAllPinLinks();
			InputPins[ChildIndex]->MakeLinkTo(TargetOutputPin);
		}

		Cue->CompileSoundNodesFromGraphNodes();
		SaveAudioAsset(Cue, bSave);
		McpHandlerUtils::AddVerification(Response, Cue);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	if (SubAction == TEXT("set_cue_attenuation"))
	{
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString AttenuationPath = McpHandlerUtils::GetOptionalString(Params, TEXT("attenuationPath"), TEXT(""));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);
		USoundCue* Cue = LoadSoundCueFromPath(AssetPath);
		if (!Cue)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("CUE_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundCue: %s"), *AssetPath));
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
		Response->SetStringField(TEXT("attenuationPath"), Cue->AttenuationSettings ? Cue->AttenuationSettings->GetPathName() : TEXT(""));
		McpHandlerUtils::AddVerification(Response, Cue);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	if (SubAction == TEXT("set_cue_concurrency"))
	{
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString ConcurrencyPath = McpHandlerUtils::GetOptionalString(Params, TEXT("concurrencyPath"), TEXT(""));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);
		USoundCue* Cue = LoadSoundCueFromPath(AssetPath);
		if (!Cue)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("CUE_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundCue: %s"), *AssetPath));
		}

		if (!ConcurrencyPath.IsEmpty())
		{
			USoundConcurrency* Conc = Cast<USoundConcurrency>(StaticLoadObject(USoundConcurrency::StaticClass(), nullptr, *NormalizeAudioPath(ConcurrencyPath)));
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
		Response->SetNumberField(TEXT("concurrencyCount"), Cue->ConcurrencySet.Num());
		McpHandlerUtils::AddVerification(Response, Cue);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	return nullptr;
}
}
#endif
