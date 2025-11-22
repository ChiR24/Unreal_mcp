#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Editor.h"
#include "LevelEditor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "HAL/FileManager.h"
#include "IMergeActorsModule.h"
#include "IMergeActorsTool.h"
#include "EngineUtils.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandlePerformanceAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.StartsWith(TEXT("generate_memory_report")) && 
        !Lower.StartsWith(TEXT("configure_texture_streaming")) && 
        !Lower.StartsWith(TEXT("merge_actors")))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid()) 
    { 
        SendAutomationError(RequestingSocket, RequestId, TEXT("Performance payload missing"), TEXT("INVALID_PAYLOAD")); 
        return true; 
    }

    if (Lower == TEXT("generate_memory_report"))
    {
        bool bDetailed = false;
        Payload->TryGetBoolField(TEXT("detailed"), bDetailed);
        
        FString OutputPath;
        Payload->TryGetStringField(TEXT("outputPath"), OutputPath);

        // Execute memreport command
        FString Cmd = bDetailed ? TEXT("memreport -full") : TEXT("memreport");
        GEngine->Exec(GEditor->GetEditorWorldContext().World(), *Cmd);

        // If output path provided, we might want to move the log file, but memreport writes to a specific location.
        // For now, just acknowledge execution.
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Memory report generated"), nullptr);
        return true;
    }
    else if (Lower == TEXT("configure_texture_streaming"))
    {
        bool bEnabled = true;
        Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
        
        double PoolSize = 0;
        if (Payload->TryGetNumberField(TEXT("poolSize"), PoolSize))
        {
            IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.PoolSize"));
            if (CVar) CVar->Set((float)PoolSize);
        }

        bool bBoost = false;
        if (Payload->TryGetBoolField(TEXT("boostPlayerLocation"), bBoost) && bBoost)
        {
            // Logic to boost streaming around player
            if (GEditor && GEditor->GetEditorWorldContext().World())
            {
                APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(GEditor->GetEditorWorldContext().World(), 0);
                if (Cam)
                {
                    IStreamingManager::Get().AddViewLocation(Cam->GetCameraLocation());
                }
            }
        }

        IConsoleVariable* CVarStream = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TextureStreaming"));
        if (CVarStream) CVarStream->Set(bEnabled ? 1 : 0);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Texture streaming configured"), nullptr);
        return true;
    }
    else if (Lower == TEXT("merge_actors"))
    {
        // merge_actors: drive the editor's Merge Actors tools by selecting the
        // requested actors in the current editor world and invoking
        // IMergeActorsTool::RunMergeFromSelection(). This relies on the
        // MergeActors module and registered tools, but never reports success
        // unless a real merge was requested and executed.

        const TArray<TSharedPtr<FJsonValue>>* NamesArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("actors"), NamesArray) || !NamesArray || NamesArray->Num() < 2)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("merge_actors requires an 'actors' array with at least 2 entries"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        if (!GEditor || !GEditor->GetEditorWorldContext().World())
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor world not available for merge_actors"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
            return true;
        }

        UWorld* World = GEditor->GetEditorWorldContext().World();
        TArray<AActor*> ActorsToMerge;

        auto ResolveActorByName = [World](const FString& Name) -> AActor*
        {
            if (Name.IsEmpty())
            {
                return nullptr;
            }

            // Try to resolve by full object path first
            if (AActor* ByPath = FindObject<AActor>(nullptr, *Name))
            {
                return ByPath;
            }

            // Fallback: search the current editor world by label and by name
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                AActor* Actor = *It;
                if (!Actor)
                {
                    continue;
                }

                const FString Label = Actor->GetActorLabel();
                const FString ObjName = Actor->GetName();
                if (Label.Equals(Name, ESearchCase::IgnoreCase) || ObjName.Equals(Name, ESearchCase::IgnoreCase))
                {
                    return Actor;
                }
            }

            return nullptr;
        };

        for (const TSharedPtr<FJsonValue>& Val : *NamesArray)
        {
            if (!Val.IsValid() || Val->Type != EJson::String)
            {
                continue;
            }

            const FString RawName = Val->AsString().TrimStartAndEnd();
            if (RawName.IsEmpty())
            {
                continue;
            }

            if (AActor* Resolved = ResolveActorByName(RawName))
            {
                ActorsToMerge.AddUnique(Resolved);
            }
        }

        if (ActorsToMerge.Num() < 2)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("merge_actors resolved fewer than 2 valid actors"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Prepare selection for the Merge Actors tool
        GEditor->SelectNone(true, true, false);
        for (AActor* Actor : ActorsToMerge)
        {
            if (Actor)
            {
                GEditor->SelectActor(Actor, true, true, true);
            }
        }

        IMergeActorsModule& MergeModule = IMergeActorsModule::Get();
        TArray<IMergeActorsTool*> Tools;
        MergeModule.GetRegisteredMergeActorsTools(Tools);

        if (Tools.Num() == 0)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("No Merge Actors tools are registered in this editor"), nullptr, TEXT("MERGE_TOOL_MISSING"));
            return true;
        }

        FString RequestedToolName;
        Payload->TryGetStringField(TEXT("toolName"), RequestedToolName);
        IMergeActorsTool* ChosenTool = nullptr;

        // Prefer a tool whose display name matches the requested toolName
        if (!RequestedToolName.IsEmpty())
        {
            for (IMergeActorsTool* Tool : Tools)
            {
                if (!Tool)
                {
                    continue;
                }

                const FText ToolNameText = Tool->GetToolNameText();
                if (ToolNameText.ToString().Equals(RequestedToolName, ESearchCase::IgnoreCase))
                {
                    ChosenTool = Tool;
                    break;
                }
            }
        }

        // Fallback: first tool that can merge from the current selection
        if (!ChosenTool)
        {
            for (IMergeActorsTool* Tool : Tools)
            {
                if (Tool && Tool->CanMergeFromSelection())
                {
                    ChosenTool = Tool;
                    break;
                }
            }
        }

        if (!ChosenTool)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("No Merge Actors tool can operate on the current selection"), nullptr, TEXT("MERGE_TOOL_UNAVAILABLE"));
            return true;
        }

        bool bReplaceSources = false;
        if (Payload->TryGetBoolField(TEXT("replaceSourceActors"), bReplaceSources))
        {
            ChosenTool->SetReplaceSourceActors(bReplaceSources);
        }

        if (!ChosenTool->CanMergeFromSelection())
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Merge operation is not valid for the current selection"), nullptr, TEXT("MERGE_NOT_POSSIBLE"));
            return true;
        }

        const FString DefaultPackageName = ChosenTool->GetDefaultPackageName();
        const bool bMerged = ChosenTool->RunMergeFromSelection();
        if (!bMerged)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor merge operation failed"), nullptr, TEXT("MERGE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetNumberField(TEXT("mergedActorCount"), ActorsToMerge.Num());
        Resp->SetBoolField(TEXT("replaceSourceActors"), ChosenTool->GetReplaceSourceActors());
        if (!DefaultPackageName.IsEmpty())
        {
            Resp->SetStringField(TEXT("defaultPackageName"), DefaultPackageName);
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actors merged using Merge Actors tool"), Resp, FString());
        return true;
    }

    return false;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Performance actions require editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
