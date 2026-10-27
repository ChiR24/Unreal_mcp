#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpEnvironmentHandlers {

bool McpResolveEnvironmentSnapshotPath(
    const TSharedPtr<FJsonObject> &Payload, FString &OutAbsolutePath,
    FString &OutRelativePath, FString &OutError);

}
