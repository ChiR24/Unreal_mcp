#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleSoundCueAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction != TEXT("create_sound_cue"))
	{
		return nullptr;
	}

	FString Name = McpHandlerUtils::GetOptionalString(Params, TEXT("name"), TEXT(""));
	FString Path = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("path"), TEXT("/Game/Audio/Cues")), false);
	FString WavePath = McpHandlerUtils::GetOptionalString(Params, TEXT("wavePath"), TEXT(""));
	bool bLooping = McpHandlerUtils::GetOptionalBool(Params, TEXT("looping"), false);
	float Volume = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("volume"), 1.0));
	float Pitch = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("pitch"), 1.0));
	bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);

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

	if (!WavePath.IsEmpty())
	{
		USoundWave* Wave = LoadSoundWaveFromPath(WavePath);
		if (Wave)
		{
			USoundNodeWavePlayer* PlayerNode = NewCue->ConstructSoundNode<USoundNodeWavePlayer>();
			PlayerNode->SetSoundWave(Wave);
			USoundNode* LastNode = PlayerNode;

			if (bLooping)
			{
				USoundNodeLooping* LoopNode = NewCue->ConstructSoundNode<USoundNodeLooping>();
				LoopNode->InsertChildNode(0);
				LoopNode->ChildNodes[0] = LastNode;
				USoundCueGraphNode* LoopGraphNode = Cast<USoundCueGraphNode>(LoopNode->GetGraphNode());
				USoundCueGraphNode* LastGraphNode = Cast<USoundCueGraphNode>(LastNode->GetGraphNode());
				if (LoopGraphNode && LastGraphNode)
				{
					TArray<UEdGraphPin*> Pins;
					LoopGraphNode->GetInputPins(Pins);
					if (Pins.Num() > 0 && Pins[0] && LastGraphNode->GetOutputPin())
					{
						Pins[0]->MakeLinkTo(LastGraphNode->GetOutputPin());
					}
				}
				LastNode = LoopNode;
			}

			if (Volume != 1.0f || Pitch != 1.0f)
			{
				USoundNodeModulator* ModNode = NewCue->ConstructSoundNode<USoundNodeModulator>();
				ModNode->InsertChildNode(0);
				ModNode->ChildNodes[0] = LastNode;
				ModNode->PitchMin = ModNode->PitchMax = Pitch;
				ModNode->VolumeMin = ModNode->VolumeMax = Volume;
				USoundCueGraphNode* ModGraphNode = Cast<USoundCueGraphNode>(ModNode->GetGraphNode());
				USoundCueGraphNode* LastGraphNode = Cast<USoundCueGraphNode>(LastNode->GetGraphNode());
				if (ModGraphNode && LastGraphNode)
				{
					TArray<UEdGraphPin*> Pins;
					ModGraphNode->GetInputPins(Pins);
					if (Pins.Num() > 0 && Pins[0] && LastGraphNode->GetOutputPin())
					{
						Pins[0]->MakeLinkTo(LastGraphNode->GetOutputPin());
					}
				}
				LastNode = ModNode;
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
	}

	SaveAudioAsset(NewCue, bSave);
	FString FullPath = NewCue->GetPathName();
	Response->SetStringField(TEXT("assetPath"), FullPath);
	Response = McpHandlerUtils::BuildSuccessResponse(FString::Printf(TEXT("SoundCue '%s' created"), *Name));
	McpHandlerUtils::AddVerification(Response, NewCue);
	return Response;
}
}
#endif
