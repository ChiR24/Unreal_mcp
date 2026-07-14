import re
import os

repo_root = r"plugins\McpAutomationBridge\Source\McpAutomationBridge\Private"

handlers_file = os.path.join(repo_root, "McpAutomationBridge_WidgetAuthoringHandlers.cpp")
with open(handlers_file, "r", encoding="utf-8") as f:
    content = f.read()
content = content.replace('#include "Widget/UmgMcp.h"', '') # If it exists
content = content.replace('#include "FileManage/UmgAttentionSubsystem.h"', '#include "UmgAttentionSubsystem.h"')
content = content.replace('#include "FileManage/UmgFileTransformation.h"', '#include "UmgFileTransformation.h"')
with open(handlers_file, "w", encoding="utf-8") as f:
    f.write(content)

# Fix includes in UmgGetSubsystem.cpp
get_sub_file = os.path.join(repo_root, "UmgGetSubsystem.cpp")
with open(get_sub_file, "r", encoding="utf-8") as f:
    content = f.read()
content = content.replace('#include "Widget/UmgGetSubsystem.h"', '#include "UmgGetSubsystem.h"')
content = content.replace('#include "FileManage/UmgFileTransformation.h"', '#include "UmgFileTransformation.h"')
with open(get_sub_file, "w", encoding="utf-8") as f:
    f.write(content)

# Fix includes in UmgSetSubsystem.cpp
set_sub_file = os.path.join(repo_root, "UmgSetSubsystem.cpp")
with open(set_sub_file, "r", encoding="utf-8") as f:
    content = f.read()
content = content.replace('#include "FileManage/UmgAttentionSubsystem.h"', '#include "UmgAttentionSubsystem.h"')
content = content.replace('#include "FileManage/UmgFileTransformation.h"', '#include "UmgFileTransformation.h"')
with open(set_sub_file, "w", encoding="utf-8") as f:
    f.write(content)

# Fix includes and apply_widget_tree in UmgFileTransformation.cpp
transform_file = os.path.join(repo_root, "UmgFileTransformation.cpp")
with open(transform_file, "r", encoding="utf-8") as f:
    content = f.read()
content = content.replace('#include "FileManage/UmgFileTransformation.h"', '#include "UmgFileTransformation.h"')
content = content.replace('#include "Widget/UmgMcp.h"', '')

# Update ApplyJsonStringToUmgAsset to be synchronous using Async
async_include = '#include "Async/Async.h"\n'
if '#include "Async/Async.h"' not in content:
    content = async_include + content

old_apply_func = '''bool UUmgFileTransformation::ApplyJsonStringToUmgAsset(const FString& AssetPath, const FString& JsonData, const FString& TargetWidgetName)
{
    // Dispatch the task to the game thread asynchronously ("fire and forget").
    // The original implementation used a blocking wait which could cause the editor to freeze or deadlock.
    // We capture parameters by value to ensure they are valid when the task eventually executes.
    FFunctionGraphTask::CreateAndDispatchWhenReady([AssetPath, JsonData, TargetWidgetName]()
    {
        ApplyJsonToUmgAsset_GameThread(AssetPath, JsonData, TargetWidgetName);
    }, TStatId(), nullptr, ENamedThreads::GameThread);

    // Return true to indicate the task was successfully dispatched.
    // The operation itself runs in the background and the result will be visible in the editor.
    return true;
}'''

new_apply_func = '''bool UUmgFileTransformation::ApplyJsonStringToUmgAsset(const FString& AssetPath, const FString& JsonData, const FString& TargetWidgetName)
{
    if (IsInGameThread())
    {
        return ApplyJsonToUmgAsset_GameThread(AssetPath, JsonData, TargetWidgetName);
    }

    TFuture<bool> Future = Async(EAsyncExecution::TaskGraphMainThread, [AssetPath, JsonData, TargetWidgetName]()
    {
        return ApplyJsonToUmgAsset_GameThread(AssetPath, JsonData, TargetWidgetName);
    });

    // Wait for the result and return it
    Future.Wait();
    return Future.Get();
}'''

content = content.replace(old_apply_func, new_apply_func)

with open(transform_file, "w", encoding="utf-8") as f:
    f.write(content)

print("C++ Patching complete safely.")
