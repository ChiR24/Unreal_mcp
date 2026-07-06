#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpSequenceIntegerValidation {

bool ValidateSequenceIntegerFields(
    const TSharedPtr<FJsonObject> &Payload, FString &OutError);

}
