#include "McpPropertyReflectionPrivate.h"

namespace McpPropertyReflection
{
namespace Private
{
FString ExportTextToJsonString(const FText& TextValue)
{
    if (TextValue.IsCultureInvariant() && !TextValue.IsFromStringTable())
    {
        return TextValue.ToString();
    }

    FString ExportedText;
    FTextStringHelper::WriteToBuffer(ExportedText, TextValue);
    return ExportedText;
}

FText ImportTextFromJsonString(const FString& TextValue)
{
    return FTextStringHelper::CreateFromBuffer(*TextValue);
}
}
}
