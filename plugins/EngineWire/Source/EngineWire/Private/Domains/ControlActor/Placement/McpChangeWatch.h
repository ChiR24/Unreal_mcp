#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AActor;

// What a request spawned, moved or redrew in the editor level is checked on the fly, whatever capability did it: the
// reply's warnings name an actor left sunk, floating, intersecting or z-fighting, a mesh slot on the engine's default
// grid, a text still reading "Text" and a copy pasted onto its original. A caller that never runs audit_placement still
// hears what audit_placement would have told it about the actors it touched.
namespace McpChangeWatch
{
/** Starts noting the editor-level actors this request spawns or modifies (game thread, before its handler runs). */
void Begin(const FString& RequestId);
/** Stops noting: the handler returned. A reply sent after it (a latent task) carries no checks. */
void End();
/** Appends the checks' findings to Result's warnings when Result answers the watched request. */
void AddWarnings(const FString& RequestId, const TSharedPtr<FJsonObject>& Result);
/** This actor's reply already reports its placement (spawn, set_transform): only what it shows is checked. */
void NotePlacementReported(const AActor* Actor);
} // namespace McpChangeWatch
