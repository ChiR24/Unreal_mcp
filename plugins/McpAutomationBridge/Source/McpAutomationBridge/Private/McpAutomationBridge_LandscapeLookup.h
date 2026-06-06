#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
class ALandscape;

namespace McpLandscapeHandlers {
ALandscape *FindLandscapeForEdit(const FString &LandscapePath,
                                 const FString &LandscapeName);
FString MakeLandscapeNotFoundMessage(const FString &LandscapePath,
                                     const FString &LandscapeName);
}
#endif
