1. **Analyze Security Issue**: The issue is that in `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_EnvironmentHandlers.cpp`, the `export_snapshot` and `import_snapshot` handlers use `SanitizeProjectFilePath` which allows paths starting with `/`. Then they construct an absolute path using `FPaths::ProjectDir() / SafePath` and `FPaths::MakeStandardFilename(AbsolutePath)`.
However, `SafePath` will start with `/`. In Unreal, `FPaths::ConvertRelativePathToFull` treats paths starting with `/` as absolute on Unix/Mac (and sometimes resolves them from the drive root on Windows if not careful). Wait, actually `FPaths::ProjectDir() / SafePath` might be fine if `operator/` strips leading slashes or handles them?
Ah, no! Let's read the rule from the instructions:
> ### 3. Path Bypass via Leading Slash (C++)
> **Problem**: `ConvertRelativePathToFull` ignores base dir when path starts with `/`.
> ❌ **Wrong**:
> `FString SafePath = SanitizeProjectFilePath(OutputPath);  // Returns "/path"`
> `FString Absolute = FPaths::ConvertRelativePathToFull(ProjectDir, SafePath);`
> ✅ **Correct**:
> `FString SafePath = SanitizeProjectFilePath(OutputPath);`
> `// Use / operator for concatenation (handles leading / correctly)`
> `FString Absolute = FPaths::ProjectDir() / SafePath;`
> `// Verify final path is under project`
> `if (!Absolute.StartsWith(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()))) {`

Wait, in `McpAutomationBridge_EnvironmentHandlers.cpp`, they DO use the `/` operator:
`FString AbsolutePath = FPaths::ProjectDir() / SafePath;`
But they *lack* the `ConvertRelativePathToFull` and `StartsWith` checks to verify that the final absolute path is actually inside the project directory. Without `ConvertRelativePathToFull`, `AbsolutePath` might not be fully normalized against the current working directory, and the project directory could be resolved differently, or they miss the check that guarantees the path does not escape `FPaths::ProjectDir()`.

Wait, the instructions say:
```cpp
// Final verification
FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
FPaths::NormalizeDirectoryName(NormalizedProjectDir);
if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
  NormalizedProjectDir += TEXT("/");
}

if (!AbsolutePath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
  SendAutomationResponse(Socket, RequestId, false,
    FString::Printf(TEXT("Path escapes project directory: %s"), *OutputPath),
    nullptr, TEXT("SECURITY_VIOLATION"));
  return;
}
```

This verification is missing in `McpAutomationBridge_EnvironmentHandlers.cpp` for both `export_snapshot` and `import_snapshot`.

2. **Formulate Fix**:
Modify `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_EnvironmentHandlers.cpp` at around line 208 and 256.

For `export_snapshot`:
```cpp
        // Convert project-relative path to absolute file path
        FString AbsolutePath = FPaths::ProjectDir() / SafePath;
        FPaths::MakeStandardFilename(AbsolutePath);

        AbsolutePath = FPaths::ConvertRelativePathToFull(AbsolutePath);
        FPaths::NormalizeFilename(AbsolutePath);

        FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
        FPaths::NormalizeDirectoryName(NormalizedProjectDir);
        if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
          NormalizedProjectDir += TEXT("/");
        }

        if (!AbsolutePath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
          bSuccess = false;
          Message = FString::Printf(TEXT("Invalid or unsafe path: %s. Path escapes project directory."), *Path);
          ErrorCode = TEXT("SECURITY_VIOLATION");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
            // Rest of export_snapshot logic
        }
```

For `import_snapshot`:
```cpp
        // Convert project-relative path to absolute file path
        FString AbsolutePath = FPaths::ProjectDir() / SafePath;
        FPaths::MakeStandardFilename(AbsolutePath);

        AbsolutePath = FPaths::ConvertRelativePathToFull(AbsolutePath);
        FPaths::NormalizeFilename(AbsolutePath);

        FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
        FPaths::NormalizeDirectoryName(NormalizedProjectDir);
        if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
          NormalizedProjectDir += TEXT("/");
        }

        if (!AbsolutePath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
          bSuccess = false;
          Message = FString::Printf(TEXT("Invalid or unsafe path: %s. Path escapes project directory."), *Path);
          ErrorCode = TEXT("SECURITY_VIOLATION");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
            // Rest of import_snapshot logic
        }
```

Wait! Is there an existing helper in `McpAutomationBridgeHelpers.h`?
```bash
grep -rn "NormalizedProjectDir" plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/
```
