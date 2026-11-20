#include "Domains/Inspect/McpInspectShared.h"

namespace McpInspectHandlers {

bool HandleInspectSearchAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const FString &SubAction, const FString &LowerSubAction,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> Resp)
{
        if (LowerSubAction.Equals(TEXT("inspect_class")))
        {
            FString ClassName;
            Payload->TryGetStringField(TEXT("className"), ClassName);
            if (ClassName.IsEmpty())
            {
                Payload->TryGetStringField(TEXT("classPath"), ClassName);
            }
            if (!ClassName.IsEmpty())
            {
                // A native class by name or path, a loaded class by its short name (any case), a Blueprint asset path
                // or its _C class.
                UClass* TargetClass = ResolveClassByName(ClassName);
                // The conventional A/U code prefix: a reflected class object carries none (ATDMCharacter's is TDMCharacter).
                if (!TargetClass && ClassName.Len() >= 2 && (ClassName[0] == TEXT('A') || ClassName[0] == TEXT('U')) &&
                    FChar::IsUpper(ClassName[1]))
                {
                    TargetClass = ResolveClassByName(ClassName.Mid(1));
                }
                if (TargetClass)
                {
                    // className/classPath/parentClass plus module, flags,
                    // properties[] and the CDO defaultProperties — see
                    // McpInspectClass.cpp.
                    McpDescribeClass(TargetClass, Resp);
                    McpDescribeClassCode(TargetClass, Payload, Resp);
                    // A question about one function or the subclasses gets that answer, not 200 properties and every
                    // default beside it (about 10 KB for Character); propertyCount stays.
                    if (Payload->HasField(TEXT("function")) || Payload->HasField(TEXT("derived")))
                    {
                        Resp->RemoveField(TEXT("properties"));
                        Resp->RemoveField(TEXT("defaultProperties"));
                    }
                    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                           TEXT("Class inspected"), Resp, FString());
                }
                else
                {
                    Bridge.SendAutomationError(RequestingSocket, RequestId,
                                        FString::Printf(TEXT("Class not found: %s. Give a full /Script path (e.g. /Script/Engine.PointLight), a loaded short name, or an A/U prefixed name."), *ClassName),
                                        TEXT("CLASS_NOT_FOUND"));
                }
            }
            else
            {
                Bridge.SendAutomationError(RequestingSocket, RequestId,
                                    TEXT("className is required for inspect_class"),
                                    TEXT("INVALID_ARGUMENT"));
            }
            return true;
        }
    return false;
}

} // namespace McpInspectHandlers
