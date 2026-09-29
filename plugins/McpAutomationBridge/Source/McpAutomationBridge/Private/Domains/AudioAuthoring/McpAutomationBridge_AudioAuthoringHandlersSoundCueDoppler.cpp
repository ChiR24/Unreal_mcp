#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#include "Sound/SoundNodeDoppler.h"

// set_doppler_effect: the engine's one Doppler mechanism is a USoundNodeDoppler in a Sound
// Cue graph. An existing Doppler is updated wherever it sits; otherwise one goes at the cue
// root, above whatever the cue already plays, the same way the engine's own sound factory
// inserts a node (SoundFactory.cpp InsertSoundNode).
// USoundNodeDoppler has no ENGINE_API or MinimalAPI, so its StaticClass does not link from
// outside Engine: the class is resolved by path and only its public fields are touched.
namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleSoundCueDopplerAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction != TEXT("set_doppler_effect"))
	{
		return nullptr;
	}
	const FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
	const double Intensity = GetJsonNumberField(Params, TEXT("dopplerIntensity"), 1.0);
	const bool bSmoothing = GetJsonBoolField(Params, TEXT("smoothing"), false);
	const bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
	if (Intensity < 0.0)
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ARGUMENT"),
			TEXT("dopplerIntensity must be 0 or more (1 is the normal shift, 0 turns it off)."));
	}
	USoundCue* Cue = LoadSoundCueFromPath(AssetPath);
	if (!Cue)
	{
		if (Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, *AssetPath)))
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("DOPPLER_NEEDS_SOUND_CUE"), FString::Printf(
				TEXT("'%s' is not a Sound Cue. Doppler is a Sound Cue node: create a cue that plays it (create_audio_asset kind sound_cue with wavePath) and set the Doppler on that cue."), *AssetPath));
		}
		return McpHandlerUtils::BuildErrorResponse(TEXT("CUE_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundCue: %s"), *AssetPath));
	}
	if (!Cue->FirstNode)
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("CUE_EMPTY"), FString::Printf(
			TEXT("'%s' plays nothing yet, so there is nothing to shift. Add a wave player first (edit_sound_cue add_node, then connect it to Output)."), *AssetPath));
	}
	if (!Cue->SoundCueGraph)
	{
		Cue->CreateGraph();
	}

	UClass* DopplerClass = LoadClass<USoundNode>(nullptr, TEXT("/Script/Engine.SoundNodeDoppler"));
	if (!DopplerClass)
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("DOPPLER_CLASS_MISSING"), TEXT("The engine's Doppler sound node class could not be resolved in this editor."));
	}
	USoundNodeDoppler* Doppler = nullptr;
	for (USoundNode* Node : Cue->AllNodes)
	{
		if (Node && Node->IsA(DopplerClass))
		{
			Doppler = static_cast<USoundNodeDoppler*>(Node);
			break;
		}
	}
	const bool bInserted = Doppler == nullptr;
	Cue->Modify();
	if (bInserted)
	{
		// Checked before anything is created, so a cue whose graph cannot take the node is left as it was.
		USoundCueGraphNode* OldGraphNode = Cast<USoundCueGraphNode>(Cue->FirstNode->GetGraphNode());
		TArray<USoundCueGraphNode_Root*> Roots;
		Cue->SoundCueGraph->GetNodesOfClass<USoundCueGraphNode_Root>(Roots);
		if (!OldGraphNode || !OldGraphNode->GetOutputPin() || Roots.Num() == 0 || Roots[0]->Pins.Num() == 0)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("GRAPH_NODE_ERROR"), FString::Printf(
				TEXT("The graph of '%s' has no Output node linked to its root sound node; open and resave the cue, then retry."), *AssetPath));
		}
		USoundNode* NewNode = Cue->ConstructSoundNode<USoundNode>(DopplerClass, false);
		USoundCueGraphNode* DopplerGraphNode = NewNode ? Cast<USoundCueGraphNode>(NewNode->GetGraphNode()) : nullptr;
		if (DopplerGraphNode && NewNode->ChildNodes.Num() == 0)
		{
			NewNode->CreateStartingConnectors();
		}
		TArray<UEdGraphPin*> InputPins;
		if (DopplerGraphNode)
		{
			DopplerGraphNode->GetInputPins(InputPins);
		}
		if (!DopplerGraphNode || InputPins.Num() == 0 || NewNode->ChildNodes.Num() == 0 || !DopplerGraphNode->GetOutputPin())
		{
			// ConstructSoundNode already registered it: left in AllNodes, a retry found this
			// unlinked node and reported "updated" while the cue played unshifted.
			if (NewNode)
			{
				Cue->AllNodes.Remove(NewNode);
			}
			if (DopplerGraphNode)
			{
				Cue->SoundCueGraph->RemoveNode(DopplerGraphNode);
			}
			return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_NODE_FAILED"), TEXT("The Doppler node could not be created with an input for the cue's current root."));
		}
		DopplerGraphNode->NodePosX = OldGraphNode->NodePosX + 220;
		DopplerGraphNode->NodePosY = OldGraphNode->NodePosY;
		// Data and graph are linked the same way, so a later CompileSoundNodesFromGraphNodes keeps the Doppler.
		NewNode->ChildNodes[0] = Cue->FirstNode;
		InputPins[0]->MakeLinkTo(OldGraphNode->GetOutputPin());
		Roots[0]->Pins[0]->BreakAllPinLinks();
		Roots[0]->Pins[0]->MakeLinkTo(DopplerGraphNode->GetOutputPin());
		Cue->FirstNode = NewNode;
		Doppler = static_cast<USoundNodeDoppler*>(NewNode);
	}
	Doppler->Modify();
	Doppler->DopplerIntensity = static_cast<float>(Intensity);
	Doppler->bUseSmoothing = bSmoothing;
	Cue->PostEditChange();
	Cue->MarkPackageDirty();

	const bool bSaved = bSave && SaveAudioAsset(Cue, true);
	Response->SetBoolField(TEXT("success"), true);
	const bool bAtRoot = Cue->FirstNode == Doppler;
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("%s the Doppler node %s of %s (intensity %.2f)"),
		bInserted ? TEXT("Inserted") : TEXT("Updated"), bAtRoot ? TEXT("at the root") : TEXT("below the root"), *Cue->GetName(), Doppler->DopplerIntensity));
	Response->SetStringField(TEXT("nodeName"), Doppler->GetName());
	Response->SetBoolField(TEXT("inserted"), bInserted);
	Response->SetBoolField(TEXT("atRoot"), bAtRoot);
	Response->SetStringField(TEXT("rootNodeClass"), Cue->FirstNode ? Cue->FirstNode->GetClass()->GetName() : TEXT(""));
	Response->SetStringField(TEXT("drivesNode"), Doppler->ChildNodes.Num() > 0 && Doppler->ChildNodes[0] ? Doppler->ChildNodes[0]->GetName() : TEXT(""));
	Response->SetNumberField(TEXT("dopplerIntensity"), Doppler->DopplerIntensity);
	Response->SetBoolField(TEXT("smoothing"), Doppler->bUseSmoothing);
	Response->SetBoolField(TEXT("saved"), bSaved);
	McpHandlerUtils::AddVerification(Response, Cue);
	return Response;
}
}
