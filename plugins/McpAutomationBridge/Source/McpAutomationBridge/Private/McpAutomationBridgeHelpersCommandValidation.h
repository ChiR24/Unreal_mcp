#pragma once

static inline bool McpContainsUnsafeCommandSeparator(const FString &Value) {
  return Value.Contains(TEXT("\n")) || Value.Contains(TEXT("\r")) ||
         Value.Contains(TEXT("&&")) || Value.Contains(TEXT("||")) ||
         Value.Contains(TEXT(";")) || Value.Contains(TEXT("|")) ||
         Value.Contains(TEXT("`"));
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

  for (int32 Index = 0; Index < Trimmed.Len(); ++Index) {
    const TCHAR Char = Trimmed[Index];
    const bool bAllowed = FChar::IsAlnum(Char) || Char == TEXT('_') ||
                          Char == TEXT('-') || Char == TEXT('.') ||
                          Char == TEXT('=') || Char == TEXT(':') ||
                          Char == TEXT('/') || Char == TEXT('\\') ||
                          Char == TEXT('+');
    if (!bAllowed) {
      return false;
    }
  }

  return true;
}

static inline bool McpIsSafeUbtPositionalToken(const FString &Value) {
  const FString Trimmed = Value.TrimStartAndEnd();
  if (!McpIsSafeUbtArgumentToken(Trimmed) || Trimmed.StartsWith(TEXT("-")) ||
      Trimmed.StartsWith(TEXT("/")) || Trimmed.StartsWith(TEXT("@")) ||
      Trimmed.Contains(TEXT("=")) || Trimmed.Contains(TEXT(":")) ||
      Trimmed.Contains(TEXT("/")) || Trimmed.Contains(TEXT("\\"))) {
    return false;
  }

  for (int32 Index = 0; Index < Trimmed.Len(); ++Index) {
    const TCHAR Char = Trimmed[Index];
    const bool bAllowed = FChar::IsAlnum(Char) || Char == TEXT('_') ||
                          Char == TEXT('-') || Char == TEXT('.') ||
                          Char == TEXT('+');
    if (!bAllowed) {
      return false;
    }
  }

  return true;
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
  int32 EqualsIndex = INDEX_NONE;
  int32 ColonIndex = INDEX_NONE;
  WithoutPrefix.FindChar(TEXT('='), EqualsIndex);
  WithoutPrefix.FindChar(TEXT(':'), ColonIndex);

  int32 SeparatorIndex = INDEX_NONE;
  if (EqualsIndex != INDEX_NONE && ColonIndex != INDEX_NONE) {
    SeparatorIndex = FMath::Min(EqualsIndex, ColonIndex);
  } else if (EqualsIndex != INDEX_NONE) {
    SeparatorIndex = EqualsIndex;
  } else {
    SeparatorIndex = ColonIndex;
  }

  const FString OptionName = SeparatorIndex == INDEX_NONE
                                 ? WithoutPrefix
                                 : WithoutPrefix.Left(SeparatorIndex);
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
  if (McpContainsUnsafeCommandSeparator(Trimmed)) {
    return false;
  }

  for (int32 Index = 0; Index < Trimmed.Len(); ++Index) {
    const TCHAR Char = Trimmed[Index];
    const bool bAllowed = FChar::IsAlnum(Char) || Char == TEXT('_') ||
                          Char == TEXT('-') || Char == TEXT('.') ||
                          Char == TEXT(':') || Char == TEXT('/') ||
                          Char == TEXT('+') || Char == TEXT('^') ||
                          Char == TEXT('$');
    if (!bAllowed) {
      return false;
    }
  }

  return true;
}

/**
 * Validate a basic asset path format.
 *
 * @returns `true` if Path is non-empty, begins with a leading '/', does not
 * contain the parent-traversal segment (".."), consecutive slashes ("//"),
 * or Windows drive letters (":"); `false` otherwise.
 */
