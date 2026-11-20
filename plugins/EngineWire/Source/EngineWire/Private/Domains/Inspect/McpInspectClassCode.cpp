// inspect_class's code view (McpInspectShared.h): where a native class lives in source (its header, its .cpp, the
// include line, the module a Build.cs depends on), one function's signature as C++ and Blueprint see it, and the
// classes derived from it. The same exported calls in every 5.x.

#include "Domains/Inspect/McpInspectShared.h"

#include "Misc/Paths.h"
#include "SourceCodeNavigation.h"
#include "UObject/UObjectHash.h"

namespace McpInspectClassCodeDetail
{
TSharedPtr<FJsonObject> Source(UClass* Class)
{
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    FString Header, Cpp, Module;
    if (FSourceCodeNavigation::FindClassHeaderPath(Class, Header))
    {
        Out->SetStringField(TEXT("header"), FPaths::ConvertRelativePathToFull(Header));
    }
    if (FSourceCodeNavigation::FindClassSourcePath(Class, Cpp))
    {
        Out->SetStringField(TEXT("sourceFile"), FPaths::ConvertRelativePathToFull(Cpp));
    }
    // The include a C++ file writes (#include "GameFramework/Character.h"), and the module its Build.cs lists.
    Out->SetStringField(TEXT("includePath"), Class->GetMetaData(TEXT("IncludePath")));
    if (FSourceCodeNavigation::FindClassModuleName(Class, Module))
    {
        Out->SetStringField(TEXT("buildDependency"), Module);
    }
    return Out;
}

TSharedPtr<FJsonObject> Signature(const UFunction* Function)
{
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("name"), Function->GetName());
    Out->SetStringField(TEXT("declaredIn"), GetNameSafe(Function->GetOwnerClass()));
    TArray<TSharedPtr<FJsonValue>> Params;
    for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
    {
        if (It->HasAnyPropertyFlags(CPF_ReturnParm))
        {
            Out->SetStringField(TEXT("returnType"), It->GetCPPType());
            continue;
        }
        TSharedPtr<FJsonObject> Param = MakeShared<FJsonObject>();
        Param->SetStringField(TEXT("name"), It->GetName());
        Param->SetStringField(TEXT("type"), It->GetCPPType());
        Param->SetBoolField(TEXT("out"), It->HasAnyPropertyFlags(CPF_OutParm) && !It->HasAnyPropertyFlags(CPF_ReferenceParm | CPF_ConstParm));
        Param->SetBoolField(TEXT("byRef"), It->HasAnyPropertyFlags(CPF_ReferenceParm));
        // The default Blueprint gives an omitted argument, as the function's own metadata states it.
        const FString Default = Function->GetMetaData(*(TEXT("CPP_Default_") + It->GetName()));
        if (!Default.IsEmpty())
        {
            Param->SetStringField(TEXT("default"), Default);
        }
        Params.Add(MakeShared<FJsonValueObject>(Param));
    }
    Out->SetArrayField(TEXT("params"), Params);
    Out->SetBoolField(TEXT("static"), Function->HasAnyFunctionFlags(FUNC_Static));
    Out->SetBoolField(TEXT("const"), Function->HasAnyFunctionFlags(FUNC_Const));
    Out->SetBoolField(TEXT("pure"), Function->HasAnyFunctionFlags(FUNC_BlueprintPure));
    Out->SetBoolField(TEXT("blueprintCallable"), Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));
    Out->SetBoolField(TEXT("blueprintEvent"), Function->HasAnyFunctionFlags(FUNC_BlueprintEvent));
    Out->SetBoolField(TEXT("rpc"), Function->HasAnyFunctionFlags(FUNC_Net));
    Out->SetStringField(TEXT("category"), Function->GetMetaData(TEXT("Category")));
    Out->SetStringField(TEXT("tooltip"), Function->GetMetaData(TEXT("ToolTip")).Left(400));
    return Out;
}
} // namespace McpInspectClassCodeDetail

namespace McpInspectHandlers
{
void McpDescribeClassCode(UClass* Class, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FJsonObject> Resp)
{
    using namespace McpInspectClassCodeDetail;
    if (!Class || !Resp.IsValid())
    {
        return;
    }
    if (Class->HasAnyClassFlags(CLASS_Native))
    {
        Resp->SetObjectField(TEXT("source"), Source(Class));
    }
    FString Wanted;
    if (Payload.IsValid() && Payload->TryGetStringField(TEXT("function"), Wanted) && !Wanted.IsEmpty())
    {
        if (const UFunction* Function = Class->FindFunctionByName(FName(*Wanted)))
        {
            Resp->SetObjectField(TEXT("functionSignature"), Signature(Function));
        }
        else
        {
            // Not there by that name: the class's functions whose names hold it, so the next call can name one.
            TArray<TSharedPtr<FJsonValue>> Near;
            for (TFieldIterator<UFunction> It(Class); It && Near.Num() < 20; ++It)
            {
                if (It->GetName().Contains(Wanted))
                {
                    Near.Add(MakeShared<FJsonValueString>(It->GetName()));
                }
            }
            Resp->SetArrayField(TEXT("functionMatches"), Near);
        }
    }
    bool bDerived = false;
    if (Payload.IsValid() && Payload->TryGetBoolField(TEXT("derived"), bDerived) && bDerived)
    {
        TArray<UClass*> Derived;
        GetDerivedClasses(Class, Derived, true);
        TArray<TSharedPtr<FJsonValue>> Names;
        for (const UClass* Each : Derived)
        {
            // Skeleton and reinstanced classes are the editor's working copies, not classes of the project.
            if (Names.Num() < 100 && !Each->GetName().StartsWith(TEXT("SKEL_")) && !Each->GetName().StartsWith(TEXT("REINST_")))
            {
                Names.Add(MakeShared<FJsonValueString>(Each->GetPathName()));
            }
        }
        Resp->SetArrayField(TEXT("derivedClasses"), Names);
        Resp->SetNumberField(TEXT("derivedCount"), Derived.Num());
    }
}
} // namespace McpInspectHandlers
