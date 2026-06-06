#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_LightingHandlersPrivate.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "Dom/JsonObject.h"
#include "Engine/Light.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
namespace McpLightingHandlers
{

bool HandleListLightTypes(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TArray<TSharedPtr<FJsonValue>> Types;
    Types.Add(MakeShared<FJsonValueString>(TEXT("DirectionalLight")));
    Types.Add(MakeShared<FJsonValueString>(TEXT("PointLight")));
    Types.Add(MakeShared<FJsonValueString>(TEXT("SpotLight")));
    Types.Add(MakeShared<FJsonValueString>(TEXT("RectLight")));

    TSet<FString> AddedNames;
    AddedNames.Add(TEXT("DirectionalLight"));
    AddedNames.Add(TEXT("PointLight"));
    AddedNames.Add(TEXT("SpotLight"));
    AddedNames.Add(TEXT("RectLight"));

    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (It->IsChildOf(ALight::StaticClass()) &&
            !It->HasAnyClassFlags(CLASS_Abstract) &&
            !AddedNames.Contains(It->GetName()))
        {
            Types.Add(MakeShared<FJsonValueString>(It->GetName()));
            AddedNames.Add(It->GetName());
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetArrayField(TEXT("types"), Types);
    Resp->SetNumberField(TEXT("count"), Types.Num());
    Subsystem.SendAutomationResponse(
        RequestingSocket, RequestId, true, TEXT("Available light types"), Resp);
    return true;
}

}
#endif
