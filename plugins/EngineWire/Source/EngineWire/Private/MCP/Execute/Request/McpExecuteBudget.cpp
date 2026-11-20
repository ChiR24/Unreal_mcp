// McpExecuteBudget.cpp — see header.
#include "MCP/Execute/Request/McpExecuteBudget.h"

#include "Dom/JsonValue.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Execute/McpNativeReceiptRedaction.h"
#include "MCP/Protocol/McpJsonRpcReplyFields.h"

namespace
{
// A rebuilt copy of Value with every string longer than Limit cut; Path names where it sits, for OutCut.
TSharedPtr<FJsonValue> Cut(const TSharedPtr<FJsonValue>& Value, int32 Limit, const FString& Path, TArray<FString>& OutCut)
{
    if (!Value.IsValid())
    {
        return Value;
    }
    if (Value->Type == EJson::String)
    {
        const FString Text = Value->AsString();
        if (Text.Len() <= Limit)
        {
            return Value;
        }
        OutCut.Add(FString::Printf(TEXT("%s (%d characters)"), *Path, Text.Len()));
        return MakeShared<FJsonValueString>(FString::Printf(TEXT("%s... [%d more characters cut]"), *Text.Left(Limit), Text.Len() - Limit));
    }
    if (Value->Type == EJson::Array)
    {
        TArray<TSharedPtr<FJsonValue>> Items;
        for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
        {
            Items.Add(Cut(Item, Limit, FString::Printf(TEXT("%s[%d]"), *Path, Items.Num()), OutCut));
        }
        return MakeShared<FJsonValueArray>(Items);
    }
    if (Value->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
        for (const auto& Pair : Value->AsObject()->Values)
        {
            const FString Key(*Pair.Key); // FString on every engine: 5.8 keys the map by another string type
            Out->SetField(Key, Cut(Pair.Value, Limit, Path.IsEmpty() ? Key : Path + TEXT(".") + Key, OutCut));
        }
        return MakeShared<FJsonValueObject>(Out);
    }
    return Value;
}
} // namespace

namespace McpExecuteBudget
{
bool Validate(const TSharedPtr<FJsonObject>& Options, FMcpSemanticError& OutError)
{
    const TSharedPtr<FJsonValue> Value = Options.IsValid() ? Options->TryGetField(TEXT("maxReplyChars")) : nullptr;
    if (!Value.IsValid())
    {
        return true;
    }
    double Number = 0.0;
    if (Value->Type != EJson::Number || !Value->TryGetNumber(Number) || Number != FMath::FloorToDouble(Number) ||
        Number < MinChars || Number > MaxChars)
    {
        OutError = McpValidationError(TEXT("INVALID_OPTIONS"), FString::Printf(
            TEXT("options.maxReplyChars must be a whole number of characters from %d to %d."), MinChars, MaxChars),
            TEXT("/options/maxReplyChars"));
        return false;
    }
    return true;
}

int32 Of(const TSharedPtr<FJsonObject>& Options)
{
    double Number = 0.0;
    return Options.IsValid() && Options->TryGetNumberField(TEXT("maxReplyChars"), Number) ? static_cast<int32>(Number) : 0;
}

TSharedPtr<FJsonObject> DropExtras(const TSharedPtr<FJsonObject>& Payload, const TSharedPtr<FJsonObject>& OutputSchema,
                                   int32 Budget, TArray<FString>& OutCuts)
{
    if (!Payload.IsValid() || !McpSerializedResultExceeds(Payload, Budget))
    {
        return Payload;
    }
    TArray<FString> Required;
    if (OutputSchema.IsValid())
    {
        OutputSchema->TryGetStringArrayField(TEXT("required"), Required);
    }
    TSharedPtr<FJsonObject> Out = McpJsonRpcReplyFields::Copy(Payload);
    for (const TCHAR* Field : {TEXT("details"), TEXT("examples")})
    {
        if (Out->HasField(Field) && !Required.Contains(Field))
        {
            Out->RemoveField(Field);
            OutCuts.Add(FString::Printf(TEXT("dropped %s"), Field));
        }
    }
    return Out;
}

bool CutStrings(TSharedPtr<FJsonObject>& Payload, int32 Budget, TArray<FString>& OutCuts)
{
    if (!Payload.IsValid() || !McpSerializedResultExceeds(Payload, Budget))
    {
        return Payload.IsValid();
    }
    // Each limit cuts the original, so a cut string's count is what it lost; the first limit that fits wins.
    const TSharedPtr<FJsonValue> Original = MakeShared<FJsonValueObject>(Payload);
    TArray<FString> CutPaths;
    int32 Used = 0;
    for (const int32 Limit : {4000, 1000, 200})
    {
        CutPaths.Reset();
        Payload = Cut(Original, Limit, FString(), CutPaths)->AsObject();
        Used = Limit;
        if (!McpSerializedResultExceeds(Payload, Budget))
        {
            break;
        }
    }
    if (CutPaths.Num() > 0)
    {
        const TArray<FString> Shown(CutPaths.GetData(), FMath::Min(5, CutPaths.Num()));
        OutCuts.Add(FString::Printf(TEXT("cut %d string(s) to %d characters: %s%s"), CutPaths.Num(), Used,
                                    *FString::Join(Shown, TEXT(", ")), CutPaths.Num() > 5 ? TEXT(", ...") : TEXT("")));
    }
    return !McpSerializedResultExceeds(Payload, Budget);
}
} // namespace McpExecuteBudget
