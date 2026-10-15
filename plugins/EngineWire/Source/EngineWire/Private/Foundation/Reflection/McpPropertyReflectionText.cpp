#include "Foundation/Reflection/McpPropertyReflectionPrivate.h"

namespace McpPropertyReflection
{
namespace Private
{
// An FColor has byte channels, but callers send the 0-1 floats an FLinearColor
// takes, and the converter truncated {R: 0.6, G: 0.7, B: 1} to a black light. When
// every channel given lies within 0-1 it is read as normalized; byte values are
// left to the converter.
bool TryImportNormalizedColor(const TSharedPtr<FJsonObject>& Object, FColor& OutColor)
{
    uint8* Channels[4] = {&OutColor.R, &OutColor.G, &OutColor.B, &OutColor.A};
    const TCHAR* Names[4] = {TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A")};
    double Values[4] = {-1.0, -1.0, -1.0, -1.0};
    bool bAny = false;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        for (const auto& Pair : Object->Values)
        {
            if (FString(*Pair.Key).Equals(Names[Index], ESearchCase::IgnoreCase) && Pair.Value.IsValid() && Pair.Value->Type == EJson::Number)
            {
                Values[Index] = Pair.Value->AsNumber();
                if (Values[Index] < 0.0 || Values[Index] > 1.0) return false;
                bAny = true;
            }
        }
    }
    for (int32 Index = 0; bAny && Index < 4; ++Index)
    {
        if (Values[Index] >= 0.0) *Channels[Index] = static_cast<uint8>(FMath::RoundToInt(Values[Index] * 255.0));
    }
    return bAny;
}

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
