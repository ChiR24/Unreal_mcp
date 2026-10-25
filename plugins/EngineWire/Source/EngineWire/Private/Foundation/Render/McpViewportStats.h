#pragma once

#include "CoreMinimal.h"
#include "Editor.h"
#include "Engine/GameViewportClient.h"
#include "LevelEditorViewport.h"

// Whether a stat is on screen in the viewport a Stat command lands on (UnrealEdSrv.cpp): the running game's, else the
// level viewport used last. Stat commands toggle, so a "show" checks this first or it hides a stat that is already up.
inline bool McpIsStatShown(const FString& Stat)
{
    if (GEditor && GEditor->GameViewport && !GEditor->GameViewport->IsSimulateInEditorViewport())
    {
        return GEditor->GameViewport->IsStatEnabled(Stat);
    }
    const FLevelEditorViewportClient* Client =
        GLastKeyLevelEditingViewportClient ? GLastKeyLevelEditingViewportClient : GCurrentLevelEditingViewportClient;
    return Client && Client->IsRealtime() && Client->ShouldShowStats() && Client->IsStatEnabled(Stat);
}
