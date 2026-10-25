#pragma once

#include "CoreMinimal.h"
#include "Editor.h"
#if MCP_HAS_MOVIE_RENDER_PIPELINE
#include "MoviePipelineQueueSubsystem.h"
#endif

// True while Movie Render Queue renders in the editor. A render rewrites its sequence for the length of the job (the
// playback range set to the job's, camera cuts widened by handle frames) and puts it back when the job ends, so a save
// in between writes that temporary state to disk, and a restart cuts the render off.
inline bool McpMovieRenderRunning()
{
#if MCP_HAS_MOVIE_RENDER_PIPELINE
	const UMoviePipelineQueueSubsystem* Queue = GEditor ? GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>() : nullptr;
	return Queue && (Queue->GetActiveExecutor() || Queue->IsRendering());
#else
	return false;
#endif
}

// The refusal both save_all and restart_editor give while one runs.
inline const TCHAR* McpMovieRenderRunningMessage()
{
	return TEXT("A Movie Render Queue render is running. Until its job ends, the render holds its sequence in a temporary "
		"state (the job's playback range, camera cuts widened by handle frames), so saving would write that state to disk "
		"and a restart would cut the render off. Wait for the render to end, or stop it with manage_sequence "
		"cancel_render.");
}
