#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Task 30: native mirror of src/server/gateway/direct-call-migration.ts. A direct
// call to a removed canonical parent tool is answered with this bounded, copy-
// paste-executable migration receipt (used as the tool-result structuredContent)
// instead of a routed dispatch. Pure: ParentNames is the authoritative known-
// parent set, passed in so the 3-way branch is testable without the registry
// singleton — unknown -> search + closest suggestions, known/no-action ->
// describe, known + a trimmed, non-empty, string-typed action|subAction (TS
// getString parity) -> execute with stripped params.
TSharedPtr<FJsonObject> McpBuildDirectCallMigration(
	const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments,
	const TArray<FString>& ParentNames);
