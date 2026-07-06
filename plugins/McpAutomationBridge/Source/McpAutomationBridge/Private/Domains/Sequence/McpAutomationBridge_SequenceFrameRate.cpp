#include "Domains/Sequence/McpAutomationBridge_SequenceFrameRate.h"

#include "Dom/JsonObject.h"
#include "String/LexFromString.h"

namespace McpSequenceFrameRate {
namespace {
constexpr double MaxFrameRate = 1000000.0;
constexpr int32 DecimalDenominator = 1000;

bool BuildDecimalRate(double Value, FFrameRate &OutRate) {
  if (!FMath::IsFinite(Value) || Value <= 0.0 || Value > MaxFrameRate)
    return false;
  const double Scaled = Value * DecimalDenominator;
  if (Scaled > MAX_int32)
    return false;
  const int32 Numerator = FMath::RoundToInt(Scaled);
  if (Numerator <= 0)
    return false;
  OutRate = FFrameRate(Numerator, DecimalDenominator);
  return true;
}

bool ParseText(FString Text, FFrameRate &OutRate) {
  Text = Text.TrimStartAndEnd();
  if (Text.EndsWith(TEXT("fps"), ESearchCase::IgnoreCase)) {
    Text.LeftChopInline(3);
    Text = Text.TrimStartAndEnd();
  }
  FString NumeratorText;
  FString DenominatorText;
  if (Text.Split(TEXT("/"), &NumeratorText, &DenominatorText)) {
    if (DenominatorText.Contains(TEXT("/")))
      return false;
    int32 Numerator = 0;
    int32 Denominator = 0;
    if (!LexTryParseString(Numerator, *NumeratorText.TrimStartAndEnd()) ||
        !LexTryParseString(Denominator, *DenominatorText.TrimStartAndEnd()) ||
        Numerator <= 0 || Denominator <= 0 ||
        static_cast<double>(Numerator) / Denominator > MaxFrameRate) {
      return false;
    }
    OutRate = FFrameRate(Numerator, Denominator);
    return true;
  }
  double Decimal = 0.0;
  return LexTryParseString(Decimal, *Text) &&
         BuildDecimalRate(Decimal, OutRate);
}
}

bool TryParse(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName,
              FFrameRate &OutRate, FString &OutError) {
  if (!Payload.IsValid() || !Payload->HasField(FieldName)) {
    OutError = FString::Printf(TEXT("%s is required"), FieldName);
    return false;
  }
  double Numeric = 0.0;
  FString Text;
  const bool bValid =
      Payload->TryGetNumberField(FieldName, Numeric)
          ? BuildDecimalRate(Numeric, OutRate)
          : Payload->TryGetStringField(FieldName, Text) &&
                ParseText(Text, OutRate);
  if (!bValid) {
    OutError = FString::Printf(
        TEXT("%s must be a positive number or a rate such as 24fps or 24000/1001"),
        FieldName);
  }
  return bValid;
}

}
