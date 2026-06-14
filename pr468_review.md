**Actionable comments posted: 1**

> [!NOTE]
> Due to the large number of review comments, Critical severity comments were prioritized as inline comments.

<details>
<summary>🟠 Major comments (20)</summary><blockquote>

<details>
<summary>patch_cpp_feedback.py-4-6 (1)</summary><blockquote>

`4-6`: _🩺 Stability & Availability_ | _🟠 Major_ | _⚡ Quick win_

**Use portable path construction across `patch_cpp_feedback.py`, `patch_handlers.py`, and `patch_ts_handlers.py`.** All three scripts hardcode Windows-style relative paths, so they fail on POSIX before any patching runs. The shared root cause is path construction from raw backslash-separated strings instead of normalized path components.

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In `@patch_cpp_feedback.py` around lines 4 - 6, The scripts currently build
repo_root and file paths using raw backslash strings (see repo_root and
handlers_file in patch_cpp_feedback.py) which breaks on POSIX; update all three
scripts (patch_cpp_feedback.py, patch_handlers.py, patch_ts_handlers.py) to
construct paths portably by composing path components (e.g., using os.path.join
or pathlib.Path with path parts) and normalizing them (os.path.normpath or
Path.resolve()) so repo_root and any derived filenames (like handlers_file) work
on both Windows and POSIX systems; ensure no hardcoded backslashes remain and
replace raw r"plugins\McpAutomationBridge\Source\McpAutomationBridge\Private"
with a joined/normalized form.
```

</details>

<!-- cr-comment:v1:4a0af3d6ec5dc60140e07599 -->

</blockquote></details>
<details>
<summary>patch_cpp_feedback.py-7-82 (1)</summary><blockquote>

`7-82`: _🎯 Functional Correctness_ | _🟠 Major_ | _⚡ Quick win_

**Verify required substitutions in `patch_cpp_feedback.py`, `patch_handlers.py`, and `patch_ts_handlers.py`.** Each script uses unchecked text substitutions and unconditionally reports success, so any upstream formatting drift leaves the C++ or TypeScript target unchanged with no failure. The shared root cause is missing match-count/assertion logic around required edits.

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In `@patch_cpp_feedback.py` around lines 7 - 82, The scripts
(patch_cpp_feedback.py, patch_handlers.py, patch_ts_handlers.py) perform blind
text.replace operations (see occurrences like content.replace(...), and the
UUmgFileTransformation::ApplyJsonStringToUmgAsset replacement using
old_apply_func/new_apply_func) and always print success; add verification after
each substitution to assert the expected match was found and replaced (e.g.,
count occurrences before/after or use regex with count), raise an exception or
exit non‑zero if any required replacement (includes changes and the
ApplyJsonStringToUmgAsset function swap identified by
old_apply_func/new_apply_func) did not occur, and update success logging to only
run when all assertions pass so upstream formatting drift fails the script
instead of silently reporting success.
```

</details>

<!-- cr-comment:v1:ddd6094448b159413af6368a -->

</blockquote></details>
<details>
<summary>patch_handlers.py-28-43 (1)</summary><blockquote>

`28-43`: _🎯 Functional Correctness_ | _🟠 Major_ | _⚡ Quick win_

**Make reruns no-ops in `patch_handlers.py` and `patch_ts_handlers.py`.** Both scripts replace text with output that still matches the same search pattern, so rerunning them keeps adding duplicate `GEditor` guards or `sanitizePath` assignments. The shared root cause is non-idempotent replacement logic in the widget-authoring patch pipeline.

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In `@patch_handlers.py` around lines 28 - 43, The replacements are not idempotent:
patch_handlers.py and patch_ts_handlers.py replace lines like "UUmgGetSubsystem*
GetSubsystem = GEditor->GetEditorSubsystem<UUmgGetSubsystem>();" and
"UUmgSetSubsystem* SetSubsystem =
GEditor->GetEditorSubsystem<UUmgSetSubsystem>();" with blocks that include "if
(!GEditor)" so rerunning the script keeps inserting duplicates; fix by making
the replacement conditional — first check whether the target is already wrapped
(e.g., search for an existing "if (!GEditor)" guard or the specific wrapped
snippet) and only perform the replace when the guard is absent, or use a regex
that matches the unguarded assignment and rejects matches already preceded by
the guard; apply the same idempotent check for the sanitizePath assignment in
patch_ts_handlers.py so reruns become no-ops.
```

</details>

<!-- cr-comment:v1:9692d435af632a9f984058b7 -->

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Data/McpAutomationBridge_DataHandlers.cpp-110-114 (1)</summary><blockquote>

`110-114`: _🎯 Functional Correctness_ | _🟠 Major_ | _🏗️ Heavy lift_

**Replace mocked data asset handlers with real implementations or remove from schema.**

The placeholder returns success for `create_data_*` and `create_curve_*` actions without implementing them. These actions are exposed in the TypeScript `dataActionSet` (consolidated-routing.ts lines 76-87) and will mislead users. Either:
1. Implement the handlers before merging, or
2. Remove these actions from the TypeScript schema until ready.

Shipping advertised-but-nonfunctional actions degrades user trust and creates hidden technical debt.

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Data/McpAutomationBridge_DataHandlers.cpp`
around lines 110 - 114, The current branch contains mocked handlers that always
return success for SubAction values starting with "create_data_" or
"create_curve_" (see SubAction.StartsWith(...) in
McpAutomationBridge_DataHandlers.cpp and the SendAutomationResponse call), but
these actions are advertised in the TypeScript dataActionSet
(consolidated-routing.ts); replace the mock with real implementations that
perform the actual asset/curve creation and return appropriate success/error
payloads via Subsystem->SendAutomationResponse (including error details on
failure), or remove the corresponding actions from the TypeScript dataActionSet
until implementations exist so clients aren’t misled.
```

</details>

<!-- cr-comment:v1:f5c98251eee0c75577c7e206 -->

_Source: Coding guidelines_

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Data/McpAutomationBridge_DataHandlers.cpp-103-103 (1)</summary><blockquote>

`103-103`: _📐 Maintainability & Code Quality_ | _🟠 Major_

**Gameplay tags created via `AddNativeGameplayTag` won’t persist across sessions.**

`plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Data/McpAutomationBridge_DataHandlers.cpp` (line 103) registers the tag only in-memory using `UGameplayTagsManager::Get().AddNativeGameplayTag(...)`. Repo searches show no code that writes/updates `GameplayTags.ini` / `UGameplayTagsSettings` (no `SaveGameplayTags`, `PersistGameplayTag`, `ExportGameplayTags`, or similar persistence), so tags created through this runtime handler won’t survive editor/game restarts unless the operation is run again.

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Data/McpAutomationBridge_DataHandlers.cpp`
at line 103, Current code only calls
UGameplayTagsManager::Get().AddNativeGameplayTag(FName(*TagName), TagComment)
which registers the tag in-memory and won’t persist; update the handler to also
add the tag to the persistent settings and save them: obtain the
UGameplayTagsSettings (e.g., GetMutableDefault<UGameplayTagsSettings>() or via
IGameplayTagsModule::Get().GetGameplayTagsSettings()), push a new
FGameplayTagTableRow or add the tag string to the GameplayTags array on that
settings instance, call SaveConfig() on the settings object (and optionally call
UGameplayTagsManager::Get().RefreshGameplayTagTables() or
IGameplayTagsModule::Get().RequestGameplayTag to refresh runtime state),
ensuring the tag is both registered in-memory and written to GameplayTags.ini so
it survives restarts.
```

</details>

<!-- cr-comment:v1:f126e339a49e262cdd07aefb -->

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlActor/McpAutomationBridge_ControlActorSelection.cpp-7-98 (1)</summary><blockquote>

`7-98`: _🎯 Functional Correctness_ | _🟠 Major_ | _🏗️ Heavy lift_

**Stub handlers claim success without performing work.**

Eight of the nine handlers are non-functional placeholders that ignore all input parameters and unconditionally return `success: true` with hardcoded messages. Production code must not claim successful completion of operations that were never attempted.

`HandleControlActorDeselectAll` (lines 47-58) is the only handler with partial logic, but it still lacks validation (it silently skips the `GEditor->SelectNone` call when `GEditor` is null without returning an error).

All other handlers (select, select_by_class, select_by_tag, select_in_volume, get_selected, group, ungroup, run_utility) must:
- Read and validate required parameters from `Payload`
- Perform the actual editor operation
- Return structured results (e.g., selected actor names/paths)
- Report errors when operations fail





<details>
<summary>Example: Proper implementation pattern for HandleControlActorGetSelected</summary>

```diff
 bool UMcpAutomationBridgeSubsystem::HandleControlActorGetSelected(
     const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
     TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
+  if (!GEditor) {
+    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
+                              TEXT("Editor not available"), nullptr);
+    return true;
+  }
+
   TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
-  Result->SetBoolField(TEXT("success"), true);
-  Result->SetStringField(TEXT("message"), TEXT("Selected actors retrieved."));
+  
+  TArray<TSharedPtr<FJsonValue>> SelectedActors;
+  for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It) {
+    if (AActor* Actor = Cast<AActor>(*It)) {
+      TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
+      ActorObj->SetStringField(TEXT("name"), Actor->GetName());
+      ActorObj->SetStringField(TEXT("label"), Actor->GetActorLabel());
+      ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
+      SelectedActors.Add(MakeShared<FJsonValueObject>(ActorObj));
+    }
+  }
+  
+  Result->SetArrayField(TEXT("actors"), SelectedActors);
+  Result->SetBoolField(TEXT("success"), true);
+  Result->SetStringField(TEXT("message"), 
+    FString::Printf(TEXT("Retrieved %d selected actors."), SelectedActors.Num()));
+    
   SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Got Selected"), Result);
   return true;
 }
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlActor/McpAutomationBridge_ControlActorSelection.cpp`
around lines 7 - 98, The handlers currently return hardcoded success without
validating input or performing editor work; update each handler
(HandleControlActorSelect, HandleControlActorSelectByClass,
HandleControlActorSelectByTag, HandleControlActorSelectInVolume,
HandleControlActorDeselectAll, HandleControlActorGetSelected,
HandleControlActorGroup, HandleControlActorUngroup,
HandleControlActorRunUtility) to: validate required fields in Payload (e.g.,
actor name/path, class name, tag, volume bounds, utility name), check GEditor
and relevant world/context early and return a failure JSON via
SendAutomationResponse if missing, perform the actual editor operation using the
appropriate editor APIs (SelectActor(s), SelectNone, iterate selection to build
a result array of actor names/paths, grouping/ungrouping APIs, run editor
utility), and populate Result with structured data (success bool, message, and a
results array/object of actor identifiers or error details) before calling
SendAutomationResponse; ensure HandleControlActorDeselectAll returns an error
when GEditor is null instead of silently succeeding and include clear error
messages when operations fail or no matching actors are found.
```

</details>

<!-- cr-comment:v1:24fa202c282633c41c112dff -->

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorUtilities.cpp-6-64 (1)</summary><blockquote>

`6-64`: _🎯 Functional Correctness_ | _🟠 Major_ | _⚡ Quick win_

**All Phase 34 placeholder handlers return success without implementation in `McpAutomationBridge_ControlEditorUtilities.cpp` (6 handlers) and `McpAutomationBridge_AssetWorkflowBrowser.cpp` (7 handlers).**

All 13 Phase 34 handlers share one root cause: they ignore the `Payload` parameter and return `success: true` without implementing functionality. This misleads clients into believing operations succeeded when nothing actually happened. The handlers should either return `NOT_IMPLEMENTED` error responses or read and validate expected parameters from `Payload` before returning success (if deferring full implementation to a later phase).

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorUtilities.cpp`
around lines 6 - 64, The Phase 34 control-editor handlers
(HandleControlEditorSetGridSettings, HandleControlEditorSetSnapSettings,
HandleControlEditorManageLayouts, HandleControlEditorCreateCustomMode,
HandleControlEditorSpawnUtilityWidget, HandleControlEditorRunUtilityTask)
currently ignore Payload and always send success; change each to either (1)
validate expected fields from Payload and only call SendAutomationResponse with
success=true when required parameters are present and the minimal action is
performed, or (2) return a clear NOT_IMPLEMENTED error response by calling
SendAutomationResponse(RequestingSocket, RequestId, false,
TEXT("NOT_IMPLEMENTED"), Result) with Result->SetStringField("message","Not
implemented") so clients aren’t misled. Update the Result json and response
message strings in these functions and ensure Payload is checked (e.g., required
keys) before reporting success.
```

</details>

<!-- cr-comment:v1:c7f625253b32598369ec0be0 -->

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorUtilities.cpp-16-24 (1)</summary><blockquote>

`16-24`: _🎯 Functional Correctness_ | _🟠 Major_ | _⚡ Quick win_

**Placeholder returns success without implementation.**

`HandleControlEditorSetSnapSettings` returns `success: true` without reading parameters or implementing functionality. Callers will believe snap settings were applied when nothing actually happened.




<details>
<summary>🔧 Recommended fix: Return NOT_IMPLEMENTED</summary>

```diff
 bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetSnapSettings(
     const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
     TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
-  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
-  Result->SetBoolField(TEXT("success"), true);
-  Result->SetStringField(TEXT("message"), TEXT("Snap settings applied."));
-  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Snap settings updated"), Result);
+  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
+                            TEXT("Snap settings handler not yet implemented."), nullptr);
   return true;
 }
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorUtilities.cpp`
around lines 16 - 24, The handler
UMcpAutomationBridgeSubsystem::HandleControlEditorSetSnapSettings currently
returns success without applying any settings; update this function to parse the
incoming Payload (read expected fields like grid size, snap enabled booleans or
relevant keys), apply those values to the Control Editor subsystem or call the
appropriate setter methods, and only send a success response if the application
succeeds; if the feature isn't implemented yet, change the response to send a
NOT_IMPLEMENTED result instead of success (use SendAutomationResponse with an
appropriate failure flag and message) so callers don't believe snap settings
were applied.
```

</details>

<!-- cr-comment:v1:2e4022ac0e7282d738254aff -->

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowBrowser.cpp-35-43 (1)</summary><blockquote>

`35-43`: _🎯 Functional Correctness_ | _🟠 Major_ | _⚡ Quick win_

**Placeholder returns success without implementation.**

`HandleAddToCollection` returns `success: true` without reading the `collectionName` or `assetPath` parameters or adding to a collection. Callers will believe the asset was added when nothing actually happened.




<details>
<summary>🔧 Recommended fix: Return NOT_IMPLEMENTED</summary>

```diff
 bool UMcpAutomationBridgeSubsystem::HandleAddToCollection(
     const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload,
     TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
-  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
-  Result->SetBoolField(TEXT("success"), true);
-  Result->SetStringField(TEXT("message"), TEXT("Added to collection."));
-  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added to collection"), Result);
+  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
+                            TEXT("Add to collection handler not yet implemented."), nullptr);
   return true;
 }
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowBrowser.cpp`
around lines 35 - 43, HandleAddToCollection currently always returns success
without using Payload; update
UMcpAutomationBridgeSubsystem::HandleAddToCollection to read the expected
parameters (e.g., "collectionName" and "assetPath") from the Payload
FJsonObject, attempt to perform the add-to-collection operation via the
appropriate domain/asset APIs (or call into the existing collection management
methods), and only send a success SendAutomationResponse if the add actually
succeeded; if the functionality isn't implemented yet, return a NOT_IMPLEMENTED
style response instead of success (use SendAutomationResponse with success=false
and a descriptive message) so callers don't assume the asset was added.
```

</details>

<!-- cr-comment:v1:a6029eff99e4932a88473b91 -->

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowBrowser.cpp-5-13 (1)</summary><blockquote>

`5-13`: _🎯 Functional Correctness_ | _🟠 Major_ | _⚡ Quick win_

**Placeholder returns success without implementation.**

`HandleNavigateToPath` returns `success: true` without reading the `path` parameter or implementing navigation. Callers will believe navigation occurred when nothing actually happened.




<details>
<summary>🔧 Recommended fix: Return NOT_IMPLEMENTED</summary>

```diff
 bool UMcpAutomationBridgeSubsystem::HandleNavigateToPath(
     const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload,
     TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
-  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
-  Result->SetBoolField(TEXT("success"), true);
-  Result->SetStringField(TEXT("message"), TEXT("Navigated to path in Content Browser."));
-  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Navigated"), Result);
+  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
+                            TEXT("Navigate to path handler not yet implemented."), nullptr);
   return true;
 }
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowBrowser.cpp`
around lines 5 - 13, HandleNavigateToPath currently always reports success
without doing anything; update it to read the "path" from the Payload
(Payload->GetStringField("path")), attempt the actual Content Browser navigation
using the appropriate editor API, and only send a success response via
SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Navigated"),
Result) when navigation actually succeeds; if you cannot implement navigation
here (no editor APIs available), return a NOT_IMPLEMENTED style response by
setting Result->SetBoolField("success", false) and
Result->SetStringField("message", TEXT("Not implemented: navigation not
performed")) and call SendAutomationResponse(RequestingSocket, RequestId, false,
TEXT("NOT_IMPLEMENTED"), Result) instead.
```

</details>

<!-- cr-comment:v1:f7195db12915af76983e6a33 -->

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowBrowser.cpp-65-73 (1)</summary><blockquote>

`65-73`: _🎯 Functional Correctness_ | _🟠 Major_ | _⚡ Quick win_

**Placeholder returns success without implementation.**

`HandleRunAssetActionUtility` returns `success: true` without reading the `utilityPath` or `assetPaths` parameters or running a utility. Callers will believe the utility ran when nothing actually happened.




<details>
<summary>🔧 Recommended fix: Return NOT_IMPLEMENTED</summary>

```diff
 bool UMcpAutomationBridgeSubsystem::HandleRunAssetActionUtility(
     const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload,
     TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
-  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
-  Result->SetBoolField(TEXT("success"), true);
-  Result->SetStringField(TEXT("message"), TEXT("Asset Action Utility ran."));
-  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Utility Executed"), Result);
+  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
+                            TEXT("Run asset action utility handler not yet implemented."), nullptr);
   return true;
 }
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowBrowser.cpp`
around lines 65 - 73, HandleRunAssetActionUtility currently always reports
success without using payload params; update
UMcpAutomationBridgeSubsystem::HandleRunAssetActionUtility to parse
"utilityPath" and "assetPaths" from the Payload TSharedPtr<FJsonObject> and
invoke the appropriate utility execution code (or, if execution is not yet
implemented, return a NOT_IMPLEMENTED response). Specifically, inspect
Payload->GetStringField("utilityPath") and Payload->GetArrayField("assetPaths"),
validate inputs, attempt to run the utility (or skip execution if
unimplemented), and call SendAutomationResponse(RequestingSocket, RequestId,
false, TEXT("NOT_IMPLEMENTED"), Result) when not implemented or include real
success/failure details in Result when executed.
```

</details>

<!-- cr-comment:v1:f6fddc3c7cefc45f4596a429 -->

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorUtilities.cpp-36-44 (1)</summary><blockquote>

`36-44`: _🎯 Functional Correctness_ | _🟠 Major_ | _⚡ Quick win_

**Placeholder returns success without implementation.**

`HandleControlEditorCreateCustomMode` returns `success: true` without reading parameters or implementing functionality. Callers will believe the custom mode was created when nothing actually happened.




<details>
<summary>🔧 Recommended fix: Return NOT_IMPLEMENTED</summary>

```diff
 bool UMcpAutomationBridgeSubsystem::HandleControlEditorCreateCustomMode(
     const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
     TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
-  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
-  Result->SetBoolField(TEXT("success"), true);
-  Result->SetStringField(TEXT("message"), TEXT("Custom editor mode created."));
-  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Mode created"), Result);
+  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
+                            TEXT("Custom mode creation handler not yet implemented."), nullptr);
   return true;
 }
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorUtilities.cpp`
around lines 36 - 44, The current
UMcpAutomationBridgeSubsystem::HandleControlEditorCreateCustomMode always
returns success without doing work; change it to return a NOT_IMPLEMENTED
response instead: build a Result JSON (reuse the existing Result variable) with
success=false and a clear message like "Not implemented", and call
SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Not
implemented"), Result); then return false so callers don't assume the mode was
created; if you prefer, validate incoming Payload first and only implement
actual creation later, but for now ensure the function signals NOT_IMPLEMENTED
through Result and the SendAutomationResponse call rather than claiming success.
```

</details>

<!-- cr-comment:v1:3233a36d233fb086ff5e3c2b -->

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowBrowser.cpp-15-23 (1)</summary><blockquote>

`15-23`: _🎯 Functional Correctness_ | _🟠 Major_ | _⚡ Quick win_

**Placeholder returns success without implementation.**

`HandleSyncToAsset` returns `success: true` without reading the `assetPath` parameter or implementing sync. Callers will believe the Content Browser synced to the asset when nothing actually happened.




<details>
<summary>🔧 Recommended fix: Return NOT_IMPLEMENTED</summary>

```diff
 bool UMcpAutomationBridgeSubsystem::HandleSyncToAsset(
     const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload,
     TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
-  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
-  Result->SetBoolField(TEXT("success"), true);
-  Result->SetStringField(TEXT("message"), TEXT("Synced to asset in Content Browser."));
-  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Synced"), Result);
+  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
+                            TEXT("Sync to asset handler not yet implemented."), nullptr);
   return true;
 }
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowBrowser.cpp`
around lines 15 - 23, HandleSyncToAsset currently always returns success; update
UMcpAutomationBridgeSubsystem::HandleSyncToAsset to read the "assetPath" string
from the provided Payload and only perform the Content Browser sync
implementation (or call the existing editor/content browser API) when assetPath
is present; if assetPath is missing or you cannot perform the sync here, respond
using SendAutomationResponse(RequestingSocket, RequestId, false,
TEXT("NOT_IMPLEMENTED"), Result) (or return a NOT_IMPLEMENTED-style error) and
set Result fields to include an explanatory message instead of unconditionally
returning success. Ensure you reference Payload->GetStringField("assetPath"),
RequestId, RequestingSocket and SendAutomationResponse when making the change.
```

</details>

<!-- cr-comment:v1:21924961595791da3cac50de -->

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorUtilities.cpp-26-34 (1)</summary><blockquote>

`26-34`: _🎯 Functional Correctness_ | _🟠 Major_ | _⚡ Quick win_

**Placeholder returns success without implementation.**

`HandleControlEditorManageLayouts` returns `success: true` without reading parameters or implementing functionality. Callers will believe the layout was managed when nothing actually happened.




<details>
<summary>🔧 Recommended fix: Return NOT_IMPLEMENTED</summary>

```diff
 bool UMcpAutomationBridgeSubsystem::HandleControlEditorManageLayouts(
     const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
     TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
-  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
-  Result->SetBoolField(TEXT("success"), true);
-  Result->SetStringField(TEXT("message"), TEXT("Editor layout updated."));
-  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Layout managed"), Result);
+  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
+                            TEXT("Layout management handler not yet implemented."), nullptr);
   return true;
 }
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorUtilities.cpp`
around lines 26 - 34, The handler
UMcpAutomationBridgeSubsystem::HandleControlEditorManageLayouts currently always
returns success without processing Payload; replace the placeholder behavior
with a NOT_IMPLEMENTED response: inspect Payload and implement layout management
or, if not ready, set Result->SetBoolField("success", false) and use
SendAutomationResponse(RequestingSocket, RequestId, false,
TEXT("NOT_IMPLEMENTED"), Result) (or the project's standard NOT_IMPLEMENTED
error code/message) so callers get an accurate failure; ensure you reference and
update the function HandleControlEditorManageLayouts and the
SendAutomationResponse call site accordingly.
```

</details>

<!-- cr-comment:v1:e63be5c1f501ab7368f23d79 -->

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorUtilities.cpp-6-14 (1)</summary><blockquote>

`6-14`: _🎯 Functional Correctness_ | _🟠 Major_ | _⚡ Quick win_

**Placeholder returns success without implementation.**

`HandleControlEditorSetGridSettings` returns `success: true` without reading parameters or implementing functionality. Callers will believe the grid settings were applied when nothing actually happened.




<details>
<summary>🔧 Recommended fix: Return NOT_IMPLEMENTED or read expected parameters</summary>

```diff
 bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetGridSettings(
     const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
     TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
-  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
-  Result->SetBoolField(TEXT("success"), true);
-  Result->SetStringField(TEXT("message"), TEXT("Grid settings applied."));
-  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Grid settings updated"), Result);
+  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
+                            TEXT("Grid settings handler not yet implemented."), nullptr);
   return true;
 }
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorUtilities.cpp`
around lines 6 - 14, HandleControlEditorSetGridSettings currently always reports
success without parsing Payload or applying settings; update
UMcpAutomationBridgeSubsystem::HandleControlEditorSetGridSettings to parse the
expected parameters from Payload (e.g., grid enabled, grid size, snap settings —
whatever the editor API expects), validate them, call the appropriate
control-editor API to apply the grid settings, and only set
Result->SetBoolField("success", true) and SendAutomationResponse success when
the apply call succeeds; if required parameters are missing or the apply fails,
return a NOT_IMPLEMENTED/false response by setting success=false, a descriptive
message, and send that via SendAutomationResponse(RequestingSocket, RequestId,
false,...). Ensure you reference and use the existing Result JSON object and
RequestId/RequestingSocket in the updated flow.
```

</details>

<!-- cr-comment:v1:aeaf171eed7b1fbfe7ff947a -->

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowBrowser.cpp-45-53 (1)</summary><blockquote>

`45-53`: _🎯 Functional Correctness_ | _🟠 Major_ | _⚡ Quick win_

**Placeholder returns success without implementation.**

`HandleSetAssetColor` returns `success: true` without reading the `assetPath` or `color` parameters or setting asset color. Callers will believe the color was set when nothing actually happened.




<details>
<summary>🔧 Recommended fix: Return NOT_IMPLEMENTED</summary>

```diff
 bool UMcpAutomationBridgeSubsystem::HandleSetAssetColor(
     const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload,
     TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
-  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
-  Result->SetBoolField(TEXT("success"), true);
-  Result->SetStringField(TEXT("message"), TEXT("Set asset/folder color."));
-  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Color set"), Result);
+  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
+                            TEXT("Set asset color handler not yet implemented."), nullptr);
   return true;
 }
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowBrowser.cpp`
around lines 45 - 53, HandleSetAssetColor currently returns success without
doing anything; change it to return a NOT_IMPLEMENTED-style response instead of
claiming "Color set." Specifically, in
UMcpAutomationBridgeSubsystem::HandleSetAssetColor replace the
Result->SetBoolField/SetStringField success message with a clear not-implemented
response (use SendAutomationResponse to send RequestId with success=false or a
NOT_IMPLEMENTED flag and an explanatory message), and do not parse or apply
assetPath/color until a real implementation is added; keep references to
HandleSetAssetColor and SendAutomationResponse so reviewers can find and later
implement actual parameter parsing and color application.
```

</details>

<!-- cr-comment:v1:6b51d56613f1974c65c7d427 -->

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorUtilities.cpp-46-54 (1)</summary><blockquote>

`46-54`: _🎯 Functional Correctness_ | _🟠 Major_ | _⚡ Quick win_

**Placeholder returns success without implementation.**

`HandleControlEditorSpawnUtilityWidget` returns `success: true` without reading parameters or implementing functionality. Callers will believe the widget was spawned when nothing actually happened.




<details>
<summary>🔧 Recommended fix: Return NOT_IMPLEMENTED</summary>

```diff
 bool UMcpAutomationBridgeSubsystem::HandleControlEditorSpawnUtilityWidget(
     const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
     TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
-  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
-  Result->SetBoolField(TEXT("success"), true);
-  Result->SetStringField(TEXT("message"), TEXT("Editor utility widget spawned."));
-  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget spawned"), Result);
+  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
+                            TEXT("Utility widget spawn handler not yet implemented."), nullptr);
   return true;
 }
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorUtilities.cpp`
around lines 46 - 54, The handler
UMcpAutomationBridgeSubsystem::HandleControlEditorSpawnUtilityWidget currently
returns success without doing any work; change it to return a NOT_IMPLEMENTED
response instead: stop claiming the widget was spawned, build a Result JSON
indicating not implemented (e.g., success=false and a "Not implemented" message)
and call SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Not
implemented"), Result) or use the project's canonical NOT_IMPLEMENTED response
helper/enum if one exists; leave actual spawning logic for a future
implementation.
```

</details>

<!-- cr-comment:v1:ff273dbc80a79403357e4e28 -->

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowBrowser.cpp-55-63 (1)</summary><blockquote>

`55-63`: _🎯 Functional Correctness_ | _🟠 Major_ | _⚡ Quick win_

**Placeholder returns success without implementation.**

`HandleShowInExplorer` returns `success: true` without reading the `assetPath` parameter or opening the explorer. Callers will believe the explorer was opened when nothing actually happened.




<details>
<summary>🔧 Recommended fix: Return NOT_IMPLEMENTED</summary>

```diff
 bool UMcpAutomationBridgeSubsystem::HandleShowInExplorer(
     const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload,
     TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
-  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
-  Result->SetBoolField(TEXT("success"), true);
-  Result->SetStringField(TEXT("message"), TEXT("Shown in explorer."));
-  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Opened Explorer"), Result);
+  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
+                            TEXT("Show in explorer handler not yet implemented."), nullptr);
   return true;
 }
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowBrowser.cpp`
around lines 55 - 63, HandleShowInExplorer currently returns success without
doing anything; update it to read the "assetPath" string from the Payload, and
either implement the explorer action (e.g., convert the path to an absolute path
and call FPlatformProcess::ExploreFolder or
FPlatformProcess::LaunchFileInDefaultExternalApplication on that path) and send
a true response only on success, or if you don't implement it now, change the
response to indicate NOT_IMPLEMENTED (use
SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Not
implemented"), Result) and set success=false and message accordingly). Reference
the UMcpAutomationBridgeSubsystem::HandleShowInExplorer function and the
SendAutomationResponse call to locate where to change the behavior.
```

</details>

<!-- cr-comment:v1:f9db67130aa3ac441174cb5d -->

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorUtilities.cpp-56-64 (1)</summary><blockquote>

`56-64`: _🎯 Functional Correctness_ | _🟠 Major_ | _⚡ Quick win_

**Placeholder returns success without implementation.**

`HandleControlEditorRunUtilityTask` returns `success: true` without reading parameters or implementing functionality. Callers will believe the task was executed when nothing actually happened.




<details>
<summary>🔧 Recommended fix: Return NOT_IMPLEMENTED</summary>

```diff
 bool UMcpAutomationBridgeSubsystem::HandleControlEditorRunUtilityTask(
     const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
     TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
-  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
-  Result->SetBoolField(TEXT("success"), true);
-  Result->SetStringField(TEXT("message"), TEXT("Editor utility task executed."));
-  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Task executed"), Result);
+  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
+                            TEXT("Utility task execution handler not yet implemented."), nullptr);
   return true;
 }
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorUtilities.cpp`
around lines 56 - 64, The function HandleControlEditorRunUtilityTask currently
always reports success without doing any work; change it to return a
NOT_IMPLEMENTED response instead. Update the Result JSON (Result variable) to
set success to false and message to "NOT_IMPLEMENTED" (or similar), and call
SendAutomationResponse(RequestingSocket, RequestId, false,
TEXT("NOT_IMPLEMENTED"), Result) so callers receive the correct failure status;
leave a TODO comment in HandleControlEditorRunUtilityTask for the real
implementation to be added later.
```

</details>

<!-- cr-comment:v1:88be64e6d34d72f2966e14f9 -->

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowBrowser.cpp-25-33 (1)</summary><blockquote>

`25-33`: _🎯 Functional Correctness_ | _🟠 Major_ | _⚡ Quick win_

**Placeholder returns success without implementation.**

`HandleCreateCollection` returns `success: true` without reading the `collectionName` parameter or creating a collection. Callers will believe the collection was created when nothing actually happened.




<details>
<summary>🔧 Recommended fix: Return NOT_IMPLEMENTED</summary>

```diff
 bool UMcpAutomationBridgeSubsystem::HandleCreateCollection(
     const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload,
     TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
-  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
-  Result->SetBoolField(TEXT("success"), true);
-  Result->SetStringField(TEXT("message"), TEXT("Created collection."));
-  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Collection created"), Result);
+  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
+                            TEXT("Create collection handler not yet implemented."), nullptr);
   return true;
 }
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/McpAutomationBridge_AssetWorkflowBrowser.cpp`
around lines 25 - 33, UMcpAutomationBridgeSubsystem::HandleCreateCollection
currently returns success:true without implementing creation; change it to
report not-implemented instead of claiming success by building a Result JSON
with success=false (or an error code like "NOT_IMPLEMENTED") and an explanatory
message, then call SendAutomationResponse(RequestingSocket, RequestId, false,
TEXT("Not implemented"), Result); do not attempt to read or act on
Payload/collectionName until a real implementation is added so callers are not
misled.
```

</details>

<!-- cr-comment:v1:3486f5ac89cac3e0c9aa5fdb -->

</blockquote></details>

</blockquote></details>

<details>
<summary>🟡 Minor comments (6)</summary><blockquote>

<details>
<summary>tests/mcp-tools/gameplay/manage-data.test.mjs-44-49 (1)</summary><blockquote>

`44-49`: _📐 Maintainability & Code Quality_ | _🟡 Minor_ | _⚡ Quick win_

**Replace broad expectation mask with narrow alternative.**

Line 48 uses `expected: 'success|error'`, which violates the coding guideline: "Do not use broad expectation masks such as `success|error`."

For this "delete non-existent slot" scenario, choose one of:
1. **Error-primary** (if the implementation should fail when slot not found):  
   `expected: 'error|not found'`
2. **Success-primary with narrow alternative** (if the implementation succeeds idempotently):  
   `expected: 'success|not found'`

The guideline allows narrow state alternatives like `not found`, but forbids the generic `success|error` mask.






<details>
<summary>📝 Proposed fix</summary>

```diff
   {
     scenario: 'SAVE: delete_save_slot (not found)',
     toolName: 'manage_data',
     arguments: { action: 'delete_save_slot', slotName: SLOT_NAME, userIndex: 0 },
-    expected: 'success|error', // Can return error if not found depending on implementation
+    expected: 'success|not found',
   },
```

</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In `@tests/mcp-tools/gameplay/manage-data.test.mjs` around lines 44 - 49, Update
the test case object for the scenario 'SAVE: delete_save_slot (not found)' in
tests/mcp-tools/gameplay/manage-data.test.mjs by replacing the broad expectation
string 'success|error' with a narrow alternative; decide whether the
implementation should be error-primary or idempotent and set the expected field
accordingly to either 'error|not found' (if delete should fail when the slot is
missing) or 'success|not found' (if delete should be idempotent), keeping the
rest of the scenario (toolName 'manage_data', arguments including SLOT_NAME and
userIndex) unchanged.
```

</details>

<!-- cr-comment:v1:7011e12c3b1cef6f2df32a66 -->

_Source: Coding guidelines_

</blockquote></details>
<details>
<summary>scripts/smoke-test.ts-18-18 (1)</summary><blockquote>

`18-18`: _📐 Maintainability & Code Quality_ | _🟡 Minor_

**Align tool-count documentation/guidance with canonical tool definitions (25)**

- `scripts/smoke-test.ts` expects `totalTools: 25`
- `src/tools/definitions/shared/all-tool-definitions.ts` enumerates **25** parent tool definitions

Update `src/tools/AGENTS.md` and any “maintain exactly 23”/canonical-surface documentation (and ensure `manage_data` is included) so the documented canonical tool count matches the actual list (25).

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In `@scripts/smoke-test.ts` at line 18, The smoke-test asserts totalTools:
z.literal(25) but documentation still says "maintain exactly 23"; update the
canonical documentation and guidance to reflect the actual tool list of 25 as
defined in src/tools/definitions/shared/all-tool-definitions.ts, ensuring the
manage_data tool is listed and any wording referencing "23" is changed to "25"
(or to a dynamic phrase like "canonical tool count (25)" where appropriate) in
src/tools/AGENTS.md and any other docs that mention the canonical surface so
they match the totalTools symbol used in scripts/smoke-test.ts.
```

</details>

<!-- cr-comment:v1:51595fc0dceb7f057f5499a8 -->

</blockquote></details>
<details>
<summary>src/tools/orchestration/consolidated-handler-registration.ts-220-224 (1)</summary><blockquote>

`220-224`: _🎯 Functional Correctness_ | _🟡 Minor_ | _⚡ Quick win_

**Remove redundant conditional or reject unknown actions.**

Both branches of the conditional call `handleDataTools` with identical arguments, making the `dataActionSet.has(action)` check useless. Either:
1. Remove the conditional entirely (simpler), or
2. Reject actions NOT in `dataActionSet` with an error (aligns with guideline to "reject unknown actions with domain/action context").

Other registrations like `manage_asset` (lines 91-108) route to different handlers based on action sets, suggesting option 2 may be intended.






<details>
<summary>🔧 Proposed fixes</summary>

**Option 1 (simpler): Remove the redundant conditional**
```diff
  toolRegistry.register('manage_data', async (args, tools) => {
-   const action = getToolAction(args);
-   if (dataActionSet.has(action)) return await handleDataTools(action, args, tools);
-   return await handleDataTools(action, args, tools);
+   return await handleDataTools(getToolAction(args), args, tools);
  });
```

**Option 2 (stricter): Reject unknown actions**
```diff
  toolRegistry.register('manage_data', async (args, tools) => {
    const action = getToolAction(args);
    if (dataActionSet.has(action)) return await handleDataTools(action, args, tools);
-   return await handleDataTools(action, args, tools);
+   return { success: false, error: 'UNKNOWN_ACTION', message: `Unknown data action: ${action}` };
  });
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In `@src/tools/orchestration/consolidated-handler-registration.ts` around lines
220 - 224, The tool registration for 'manage_data' contains a redundant
conditional: toolRegistry.register('manage_data', ...) calls getToolAction and
then always calls handleDataTools regardless of dataActionSet.has(action);
either remove the conditional and directly return await handleDataTools(action,
args, tools), or (preferred to match manage_asset behavior) validate the action
against dataActionSet and throw/reject an error for unknown actions (include
action and domain/context in the error) before calling handleDataTools; update
the toolRegistry.register('manage_data', ...) block and use getToolAction,
dataActionSet, and handleDataTools identifiers accordingly.
```

</details>

<!-- cr-comment:v1:1006251fbf80c3fd4cd4148b -->

_Source: Coding guidelines_

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Data/McpAutomationBridge_DataHandlers.cpp-116-118 (1)</summary><blockquote>

`116-118`: _🎯 Functional Correctness_ | _🟡 Minor_ | _⚡ Quick win_

**Reject unknown subActions instead of silently succeeding.**

The default handler returns success for any unrecognized `subAction`, violating the guideline to "reject unknown actions with domain/action context." This masks bugs and future typos.






<details>
<summary>🔧 Proposed fix</summary>

```diff
-    // Default handler
-    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Data action executed."), ResultJson);
-    return true;
+    // Unknown action
+    Subsystem->SendAutomationError(RequestingSocket, RequestId, 
+        FString::Printf(TEXT("Unknown data subAction: %s"), *SubAction), 
+        TEXT("UNKNOWN_ACTION"));
+    return true;
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Data/McpAutomationBridge_DataHandlers.cpp`
around lines 116 - 118, The default handler currently returns success for
unknown subAction values; change it to reject unknown subActions by calling
Subsystem->SendAutomationResponse with success=false and a clear message that
includes the domain ("Data"), the action name, and the unknown subAction (use
the existing RequestingSocket, RequestId, and ResultJson parameters), and return
false; also add a warning log mentioning the unrecognized subAction to aid
debugging.
```

</details>

<!-- cr-comment:v1:bc7ef4345fc3576e59c1d876 -->

_Source: Coding guidelines_

</blockquote></details>
<details>
<summary>src/tools/handlers/data/data-handlers.ts-6-6 (1)</summary><blockquote>

`6-6`: _📐 Maintainability & Code Quality_ | _🟡 Minor_ | _⚡ Quick win_

**Replace `Promise<any>` with a specific return type.**

The return type `Promise<any>` violates the strict TypeScript guideline. Use `Promise<Record<string, unknown>>` or a more specific interface.






<details>
<summary>🔧 Proposed fix</summary>

```diff
-export async function handleDataTools(action: string, args: HandlerArgs, tools: ITools): Promise<any> {
+export async function handleDataTools(action: string, args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In `@src/tools/handlers/data/data-handlers.ts` at line 6, The function
handleDataTools currently returns Promise<any>; update its signature to return a
concrete type such as Promise<Record<string, unknown>> (or a more specific
interface if available) to satisfy strict TypeScript rules — change the exported
function declaration (handleDataTools) to use the new return type and adjust any
related type aliases or imports (HandlerArgs, ITools) and call sites to align
with the chosen return type so type-checking passes.
```

</details>

<!-- cr-comment:v1:6a68b9068d0f7204ec695fe3 -->

_Source: Coding guidelines_

</blockquote></details>
<details>
<summary>src/tools/handlers/core/project-settings-handlers.ts-2-2 (1)</summary><blockquote>

`2-2`: _📐 Maintainability & Code Quality_ | _🟡 Minor_ | _⚡ Quick win_

**Replace `any` with `ITools` type.**

The `tools` parameter uses `any`, violating the strict TypeScript guideline. Import and use `ITools` from `tool-interfaces.js`.






<details>
<summary>🔧 Proposed fix</summary>

```diff
+import type { ITools } from '../../../types/tools/tool-interfaces.js';
 import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
-export async function handleProjectSettingsTools(action: string, args: Record<string, unknown>, tools: any): Promise<Record<string, unknown>> {
+export async function handleProjectSettingsTools(action: string, args: Record<string, unknown>, tools: ITools): Promise<Record<string, unknown>> {
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In `@src/tools/handlers/core/project-settings-handlers.ts` at line 2, Update the
handleProjectSettingsTools function signature to replace the loose any on the
tools parameter with the proper ITools interface: import ITools from
'tool-interfaces.js' at the top of the file and change the parameter type in
export async function handleProjectSettingsTools(action: string, args:
Record<string, unknown>, tools: ITools): Promise<Record<string, unknown>>;
ensure any usages of tools inside the function conform to ITools and adjust
imports/exports if needed so the file compiles.
```

</details>

<!-- cr-comment:v1:60c97226f2d0524767975ea5 -->

_Source: Coding guidelines_

</blockquote></details>

</blockquote></details>

<details>
<summary>🧹 Nitpick comments (3)</summary><blockquote>

<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Project/McpAutomationBridge_ProjectSettingsHandlers.cpp (1)</summary><blockquote>

`4-18`: _🎯 Functional Correctness_ | _⚡ Quick win_

**Placeholder implementation lacks action validation.**

The handler is marked as a placeholder (lines 8-9) and currently returns success for any `SubAction` without validation. Consider adding a check to ensure `SubAction` matches one of the expected actions from the tool schema (create_collision_channel, create_collision_profile, etc.) to catch invalid actions early during development.





<details>
<summary>🛡️ Suggested validation guard</summary>

```diff
 bool UMcpAutomationBridgeSubsystem::HandleProjectSettingsAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
 {
     FString SubAction = McpConsolidatedActions::GetPayloadSubAction(Payload);
+    
+    // Validate SubAction against known actions
+    static const TSet<FString> ValidActions = {
+        TEXT("create_collision_channel"), TEXT("create_collision_profile"),
+        TEXT("configure_channel_responses"), TEXT("configure_object_type"),
+        TEXT("configure_trace_channel"), TEXT("set_actor_collision_profile"),
+        TEXT("create_physical_material"), TEXT("set_physical_material_properties")
+    };
+    
+    if (!ValidActions.Contains(SubAction))
+    {
+        SendAutomationError(RequestingSocket, RequestId,
+            FString::Printf(TEXT("Unknown project settings action: %s"), *SubAction),
+            TEXT("UNKNOWN_ACTION"));
+        return false;
+    }

     // Placeholder implementations until Phase 34 properties are finalized.
```
</details>

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Project/McpAutomationBridge_ProjectSettingsHandlers.cpp`
around lines 4 - 18, HandleProjectSettingsAction currently accepts any SubAction
and always returns success; add a validation guard in
UMcpAutomationBridgeSubsystem::HandleProjectSettingsAction that calls
McpConsolidatedActions::GetPayloadSubAction, checks the returned SubAction
against an explicit allowed set (e.g., "create_collision_channel",
"create_collision_profile", and other schema-defined actions), and if the
SubAction is not in that set call SendAutomationResponse(RequestingSocket,
RequestId, false, <meaningful error message>, Result) and return false; only
proceed to build the success Result and send the success response when the
SubAction is validated.
```

</details>

<!-- cr-comment:v1:939a587e37cb93f4f8a2d7dd -->

</blockquote></details>
<details>
<summary>src/tools/handlers/core/project-settings-handlers.ts (1)</summary><blockquote>

`2-18`: _🎯 Functional Correctness_ | _⚡ Quick win_

**Consider validating required fields before dispatch.**

The handler forwards all arguments without validating action-specific required fields. As per coding guidelines, handlers should validate required fields before dispatch. Consider adding validation for common project settings parameters (e.g., collision profile names, physical material properties).

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In `@src/tools/handlers/core/project-settings-handlers.ts` around lines 2 - 18,
The handler handleProjectSettingsTools currently forwards all args to
executeAutomationRequest without validating action-specific required fields; add
pre-dispatch validation inside handleProjectSettingsTools that checks the
incoming action and validates required parameters (e.g., for collision profile
actions ensure collisionProfileName is present, for physical material actions
ensure required material properties like density/friction are provided), return
a structured error { success: false, message: "...", error: "missing field X" }
immediately if validation fails, and only call executeAutomationRequest when
validation passes; keep the existing catch behavior for runtime errors.
```

</details>

<!-- cr-comment:v1:69607d9fc48d19cc91e0106f -->

_Source: Coding guidelines_

</blockquote></details>
<details>
<summary>plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Data/McpAutomationBridge_DataHandlers.cpp (1)</summary><blockquote>

`36-36`: _🔒 Security & Privacy_ | _💤 Low value_

**Consider validating config file paths to prevent unauthorized access.**

`read_config_value` and `write_config_value` accept user-provided `ConfigFilename` without validation. If sensitive configs (e.g., encryption keys, credentials) are accessible, this could be a security risk. Consider restricting to safe config files (e.g., Game.ini, Engine.ini) or documenting the security model.






Also applies to: 55-56

<details>
<summary>🤖 Prompt for AI Agents</summary>

```
Verify each finding against current code. Fix only still-valid issues, skip the
rest with a brief reason, keep changes minimal, and validate.

In
`@plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Data/McpAutomationBridge_DataHandlers.cpp`
at line 36, read_config_value and write_config_value currently pass the
user-supplied ConfigFilename directly into GConfig->GetString /
GConfig->SetString, allowing arbitrary file paths; add validation to
canonicalize and restrict ConfigFilename before calling GConfig: reject path
traversal or absolute paths outside the game's Config directory, enforce a
whitelist (e.g., "Game.ini","Engine.ini") or a configured set of allowed
filenames, and return/log an error if the filename is invalid; implement the
check in the entry points (read_config_value/write_config_value) so
GConfig->GetString and GConfig->SetString are only called with validated
filenames.
```

</details>

<!-- cr-comment:v1:c3a1249cbc40027017b3b3f2 -->

</blockquote></details>

</blockquote></details>

<!-- This is an auto-generated comment by CodeRabbit for review status -->