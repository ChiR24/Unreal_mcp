#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Lighting/McpAutomationBridge_LightingHandlersPrivate.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Dom/JsonObject.h"
#include "Engine/Light.h"
#include "UObject/UObjectIterator.h"

namespace McpLightingHandlers
{

bool HandleListLightTypes(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // The four engine lights first, then every other concrete ALight class.
    TArray<FString> Names = {TEXT("DirectionalLight"), TEXT("PointLight"), TEXT("SpotLight"), TEXT("RectLight")};
    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (It->IsChildOf(ALight::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
        {
            Names.AddUnique(It->GetName());
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetArrayField(TEXT("types"), McpHandlerUtils::ToJsonStringArray(Names));
    Resp->SetNumberField(TEXT("count"), Names.Num());
    Subsystem.SendAutomationResponse(
        RequestingSocket, RequestId, true, TEXT("Available light types"), Resp);
    return true;
}

}
