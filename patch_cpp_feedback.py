import re
import os
import sys

repo_root = os.path.join("plugins", "McpAutomationBridge", "Source", "McpAutomationBridge", "Private")
widget_root = os.path.join(repo_root, "Domains", "WidgetAuthoring")

def robust_replace(content, old, new, desc, required=True):
    # Check if already patched
    if new and new in content and old != new:
        print(f"Skipping {desc}: Already applied.")
        return content
    if old not in content:
        if required:
            print(f"Error: Target text not found for {desc}")
            sys.exit(1)
        return content
    return content.replace(old, new)

handlers_file = os.path.join(widget_root, "McpAutomationBridge_WidgetAuthoringHandlers.cpp")
if os.path.exists(handlers_file):
    with open(handlers_file, "r", encoding="utf-8") as f:
        content = f.read()
    content = robust_replace(content, '#include "Widget/UmgMcp.h"', '', 'UmgMcp.h removal', required=False)
    content = robust_replace(content, '#include "FileManage/UmgAttentionSubsystem.h"', '#include "UmgAttentionSubsystem.h"', 'UmgAttentionSubsystem.h include', required=False)
    content = robust_replace(content, '#include "FileManage/UmgFileTransformation.h"', '#include "UmgFileTransformation.h"', 'UmgFileTransformation.h include', required=False)
    with open(handlers_file, "w", encoding="utf-8") as f:
        f.write(content)

get_sub_file = os.path.join(widget_root, "UmgGetSubsystem.cpp")
if os.path.exists(get_sub_file):
    with open(get_sub_file, "r", encoding="utf-8") as f:
        content = f.read()
    content = robust_replace(content, '#include "Widget/UmgGetSubsystem.h"', '#include "UmgGetSubsystem.h"', 'UmgGetSubsystem.h include', required=False)
    content = robust_replace(content, '#include "FileManage/UmgFileTransformation.h"', '#include "UmgFileTransformation.h"', 'UmgFileTransformation.h include 2', required=False)
    with open(get_sub_file, "w", encoding="utf-8") as f:
        f.write(content)

set_sub_file = os.path.join(widget_root, "UmgSetSubsystem.cpp")
if os.path.exists(set_sub_file):
    with open(set_sub_file, "r", encoding="utf-8") as f:
        content = f.read()
    content = robust_replace(content, '#include "FileManage/UmgAttentionSubsystem.h"', '#include "UmgAttentionSubsystem.h"', 'UmgAttentionSubsystem.h include 2', required=False)
    content = robust_replace(content, '#include "FileManage/UmgFileTransformation.h"', '#include "UmgFileTransformation.h"', 'UmgFileTransformation.h include 3', required=False)
    with open(set_sub_file, "w", encoding="utf-8") as f:
        f.write(content)

transform_file = os.path.join(widget_root, "UmgFileTransformation.cpp")
if os.path.exists(transform_file):
    with open(transform_file, "r", encoding="utf-8") as f:
        content = f.read()
    content = robust_replace(content, '#include "FileManage/UmgFileTransformation.h"', '#include "UmgFileTransformation.h"', 'UmgFileTransformation.h include 4', required=False)
    content = robust_replace(content, '#include "Widget/UmgMcp.h"', '', 'UmgMcp.h removal 2', required=False)

    async_include = '#include "Async/Async.h"\\n'
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

    content = robust_replace(content, old_apply_func, new_apply_func, 'ApplyJsonStringToUmgAsset replacement', required=True)

    with open(transform_file, "w", encoding="utf-8") as f:
        f.write(content)

print("C++ Patching complete safely.")
