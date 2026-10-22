#include "Domains/PCG/Execution/McpPCGExecution.h"
#if MCP_HAS_PCG_EXECUTION
#include "Editor/IPCGEditorModule.h"
#include "Graph/PCGStackContext.h"

namespace McpPCGHandlers
{
void ClearPCGExecutionDiagnostics(UPCGComponent* Component)
{
    if (IPCGEditorModule* Editor = IPCGEditorModule::Get()) Editor->GetNodeVisualLogsMutable().ClearLogs(Component);
}

TArray<TSharedPtr<FJsonValue>> ReadPCGExecutionDiagnostics(UPCGComponent* Component, bool& bOutAvailable, int32& OutErrorCount)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    OutErrorCount = 0;
    IPCGEditorModule* Editor = IPCGEditorModule::Get();
    bOutAvailable = Editor != nullptr;
    if (!Editor || !Component || !Component->GetGraph()) return Result;
    FPCGStack Root;
    Root.PushFrame(Component);
    Root.PushFrame(Component->GetGraph());
    Editor->GetNodeVisualLogs().ForAllMatchingLogs(Root, [&](const FPCGStack& Stack, const FPCGPerNodeVisualLogs& Logs)
    {
        const UPCGNode* Node = Stack.GetNodeForCurrentFrame();
        for (const FPCGNodeLogEntry& Entry : Logs)
        {
            if (Entry.Verbosity <= ELogVerbosity::Error) ++OutErrorCount;
            if (Result.Num() >= 100) continue;
            auto Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("severity"), Entry.Verbosity <= ELogVerbosity::Error ? TEXT("error") : TEXT("warning"));
            Row->SetStringField(TEXT("message"), Entry.Message.ToString().Left(2000));
            if (Node) Row->SetStringField(TEXT("nodeId"), Node->GetName());
            Result.Add(MakeShared<FJsonValueObject>(Row));
        }
        return true;
    });
    return Result;
}
}
#endif
