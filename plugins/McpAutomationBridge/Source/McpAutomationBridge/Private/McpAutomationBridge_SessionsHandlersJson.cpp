#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SessionsHandlersPrivate.h"

#if WITH_EDITOR
namespace SessionsHelpers
{
TSharedPtr<FJsonObject> GetObjectField(
    const TSharedPtr<FJsonObject>& Payload,
    const FString& FieldName)
{
    if (Payload.IsValid() && Payload->HasTypedField<EJson::Object>(FieldName))
    {
        return Payload->GetObjectField(FieldName);
    }
    return nullptr;
}
}
#endif
