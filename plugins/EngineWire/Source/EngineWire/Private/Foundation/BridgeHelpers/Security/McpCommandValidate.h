#pragma once

static inline bool McpContainsUnsafeCommandSeparator(const FString &Value) {
  return Value.Contains(TEXT("\n")) || Value.Contains(TEXT("\r")) ||
         Value.Contains(TEXT("&&")) || Value.Contains(TEXT("||")) ||
         Value.Contains(TEXT(";")) || Value.Contains(TEXT("|")) ||
         Value.Contains(TEXT("`"));
}

// Every character is alphanumeric or one of Extra. NUL is never allowed
// (Strchr would match Extra's own terminator).
static inline bool McpAllAlnumOr(const FString &Value, const TCHAR *Extra) {
  for (const TCHAR Char : Value) {
    if (!FChar::IsAlnum(Char) && (Char == TEXT('\0') || !FCString::Strchr(Extra, Char))) {
      return false;
    }
  }
  return true;
}

/** Match TS-side UBT argument hardening for native-direct MCP requests. */
static inline bool McpHasUnsafeUbtArgumentCharacters(const FString &Value) {
  return McpContainsUnsafeCommandSeparator(Value) || Value.Contains(TEXT(">")) ||
         Value.Contains(TEXT("<"));
}

static inline bool McpIsSafeUbtArgumentToken(const FString &Value) {
  const FString Trimmed = Value.TrimStartAndEnd();
  if (Trimmed.IsEmpty() || McpHasUnsafeUbtArgumentCharacters(Trimmed) ||
      Trimmed.Contains(TEXT("\"")) || Trimmed.Contains(TEXT("'"))) {
    return false;
  }
  return McpAllAlnumOr(Trimmed, TEXT("_-.=:/\\+"));
}

static inline bool McpIsSafeUbtPositionalToken(const FString &Value) {
  const FString Trimmed = Value.TrimStartAndEnd();
  // The allowlist already excludes '/', '@', '=', ':' and '\\'.
  return McpIsSafeUbtArgumentToken(Trimmed) && !Trimmed.StartsWith(TEXT("-")) &&
         McpAllAlnumOr(Trimmed, TEXT("_-.+"));
}

static inline bool McpIsAllowedUbtPlatform(const FString &Value) {
  const FString Trimmed = Value.TrimStartAndEnd();
  return Trimmed.Equals(TEXT("Win64"), ESearchCase::IgnoreCase) ||
         Trimmed.Equals(TEXT("Mac"), ESearchCase::IgnoreCase) ||
         Trimmed.Equals(TEXT("Linux"), ESearchCase::IgnoreCase) ||
         Trimmed.Equals(TEXT("LinuxArm64"), ESearchCase::IgnoreCase) ||
         Trimmed.Equals(TEXT("Android"), ESearchCase::IgnoreCase) ||
         Trimmed.Equals(TEXT("IOS"), ESearchCase::IgnoreCase) ||
         Trimmed.Equals(TEXT("TVOS"), ESearchCase::IgnoreCase) ||
         Trimmed.Equals(TEXT("HoloLens"), ESearchCase::IgnoreCase) ||
         Trimmed.Equals(TEXT("VisionOS"), ESearchCase::IgnoreCase);
}

static inline bool McpIsAllowedUbtConfiguration(const FString &Value) {
  const FString Trimmed = Value.TrimStartAndEnd();
  return Trimmed.Equals(TEXT("Debug"), ESearchCase::IgnoreCase) ||
         Trimmed.Equals(TEXT("DebugGame"), ESearchCase::IgnoreCase) ||
         Trimmed.Equals(TEXT("Development"), ESearchCase::IgnoreCase) ||
         Trimmed.Equals(TEXT("Shipping"), ESearchCase::IgnoreCase) ||
         Trimmed.Equals(TEXT("Test"), ESearchCase::IgnoreCase);
}

static inline bool McpIsBlockedUbtOverrideArgument(const FString &Value) {
  const FString Trimmed = Value.TrimStartAndEnd().ToLower();
  if (Trimmed.StartsWith(TEXT("@"))) {
    return true;
  }
  if (!Trimmed.StartsWith(TEXT("-")) && !Trimmed.StartsWith(TEXT("/"))) {
    return false;
  }

  FString WithoutPrefix = Trimmed;
  while (WithoutPrefix.StartsWith(TEXT("-")) || WithoutPrefix.StartsWith(TEXT("/"))) {
    WithoutPrefix.RightChopInline(1);
  }
  int32 SeparatorIndex = 0; // first '=' or ':', else the whole token
  while (SeparatorIndex < WithoutPrefix.Len() && WithoutPrefix[SeparatorIndex] != TEXT('=') &&
         WithoutPrefix[SeparatorIndex] != TEXT(':')) {
    ++SeparatorIndex;
  }
  const FString OptionName = WithoutPrefix.Left(SeparatorIndex);
  return OptionName == TEXT("project") || OptionName == TEXT("projectfile") ||
         OptionName == TEXT("target") || OptionName == TEXT("mode");
}

static inline bool McpIsSafeUbtExtraArgumentToken(const FString &Value) {
  return McpIsSafeUbtArgumentToken(Value) &&
         !McpIsBlockedUbtOverrideArgument(Value);
}

static inline bool McpIsSafeUbtArgumentList(const FString &Value) {
  const FString Trimmed = Value.TrimStartAndEnd();
  if (Trimmed.IsEmpty()) {
    return true;
  }
  if (McpHasUnsafeUbtArgumentCharacters(Trimmed)) {
    return false;
  }

  TArray<FString> Tokens;
  Trimmed.ParseIntoArrayWS(Tokens);
  for (const FString &Token : Tokens) {
    if (!McpIsSafeUbtExtraArgumentToken(Token)) {
      return false;
    }
  }

  return true;
}

static inline bool McpIsSafeAutomationTestFilter(const FString &Value) {
  const FString Trimmed = Value.TrimStartAndEnd();
  if (Trimmed.IsEmpty()) {
    return true;
  }
  return !McpContainsUnsafeCommandSeparator(Trimmed) &&
         McpAllAlnumOr(Trimmed, TEXT("_-.:/+^$"));
}
