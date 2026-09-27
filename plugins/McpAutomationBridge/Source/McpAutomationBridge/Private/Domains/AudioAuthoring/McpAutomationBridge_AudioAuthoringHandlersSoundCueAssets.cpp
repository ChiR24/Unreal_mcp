#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleSoundCueAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction != TEXT("create_sound_cue"))
	{
		return nullptr;
	}

	FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
	FString Path = NormalizeAudioPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Audio/Cues")), false);
	FString WavePath = GetJsonStringField(Params, TEXT("wavePath"), TEXT(""));
	bool bLooping = GetJsonBoolField(Params, TEXT("looping"), false);
	float Volume = static_cast<float>(GetJsonNumberField(Params, TEXT("volume"), 1.0));
	float Pitch = static_cast<float>(GetJsonNumberField(Params, TEXT("pitch"), 1.0));
	bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

	if (Name.IsEmpty())
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("Name is required"));
	}

	FString PackagePath;
	FString PathError;
	if (!BuildAudioCreationPath(Path, Name, PackagePath, PathError))
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ASSET_PATH"), PathError);
	}

	// Resolved first: a wrong wavePath used to leave an empty cue behind and still report success.
	USoundWave* Wave = WavePath.IsEmpty() ? nullptr : LoadSoundWaveFromPath(WavePath);
	if (!WavePath.IsEmpty() && !Wave)
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("WAVE_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundWave: %s"), *WavePath));
	}

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("PACKAGE_ERROR"), TEXT("Failed to create package"));
	}

	USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>();
	USoundCue* NewCue = Cast<USoundCue>(Factory->FactoryCreateNew(USoundCue::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn));
	if (!NewCue)
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_FAILED"), TEXT("Failed to create SoundCue"));
	}

	if (!NewCue->SoundCueGraph)
	{
		NewCue->CreateGraph();
	}

	if (Wave)
	{
		USoundNodeWavePlayer* PlayerNode = NewCue->ConstructSoundNode<USoundNodeWavePlayer>();
		PlayerNode->SetSoundWave(Wave);
		USoundNode* LastNode = PlayerNode;

		// Put Parent above the chain so far: the data child and the graph pin link.
		auto Chain = [&LastNode](USoundNode* Parent)
		{
			Parent->InsertChildNode(0);
			Parent->ChildNodes[0] = LastNode;
			USoundCueGraphNode* ParentGraphNode = Cast<USoundCueGraphNode>(Parent->GetGraphNode());
			USoundCueGraphNode* ChildGraphNode = Cast<USoundCueGraphNode>(LastNode->GetGraphNode());
			TArray<UEdGraphPin*> Pins;
			if (ParentGraphNode) { ParentGraphNode->GetInputPins(Pins); }
			if (Pins.Num() > 0 && Pins[0] && ChildGraphNode && ChildGraphNode->GetOutputPin())
			{
				Pins[0]->MakeLinkTo(ChildGraphNode->GetOutputPin());
			}
			LastNode = Parent;
		};
		if (bLooping)
		{
			Chain(NewCue->ConstructSoundNode<USoundNodeLooping>());
		}
		if (Volume != 1.0f || Pitch != 1.0f)
		{
			USoundNodeModulator* ModNode = NewCue->ConstructSoundNode<USoundNodeModulator>();
			ModNode->PitchMin = ModNode->PitchMax = Pitch;
			ModNode->VolumeMin = ModNode->VolumeMax = Volume;
			Chain(ModNode);
		}

		NewCue->FirstNode = LastNode;
		USoundCueGraphNode* FirstGraphNode = Cast<USoundCueGraphNode>(LastNode->GetGraphNode());
		if (FirstGraphNode && NewCue->SoundCueGraph)
		{
			TArray<USoundCueGraphNode_Root*> RootNodeList;
			NewCue->SoundCueGraph->GetNodesOfClass<USoundCueGraphNode_Root>(RootNodeList);
			if (RootNodeList.Num() > 0 && RootNodeList[0]->Pins.Num() > 0 && FirstGraphNode->GetOutputPin())
			{
				RootNodeList[0]->Pins[0]->MakeLinkTo(FirstGraphNode->GetOutputPin());
			}
		}
	}

	SaveAudioAsset(NewCue, bSave);
	FString FullPath = NewCue->GetPathName();
	Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("SoundCue '%s' created"), *Name));
	Response->SetStringField(TEXT("assetPath"), FullPath);
	McpHandlerUtils::AddVerification(Response, NewCue);
	return Response;
}
}
