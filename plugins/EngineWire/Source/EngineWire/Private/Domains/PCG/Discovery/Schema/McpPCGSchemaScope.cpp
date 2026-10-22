#include "Domains/PCG/Discovery/Schema/McpPCGSchemaScope.h"

#if MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Domains/PCG/Editing/McpPCGEditState.h"
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "Foundation/BridgeHelpers/Properties/McpNestedPropPath.h"

namespace McpPCGHandlers
{
namespace
{
bool FailSchemaScope(FPCGSchemaScope& OutScope, FString& OutError, const TCHAR* Message)
{
    OutScope = FPCGSchemaScope{};
    OutError = Message;
    return false;
}
}

bool ResolvePCGSchemaScope(UPCGGraph* Graph, UPCGNode* Node, const FString& ScopePath,
    FPCGSchemaScope& OutScope, FString& OutError)
{
    OutScope = FPCGSchemaScope{};
    OutError.Reset();
    if (!IsInGameThread() || !IsValid(Graph) || !IsValid(Node) || Graph->GetNodes().Num() > FPCGReadBudget::MaxNodes || !HasOwnedPCGNodeSettings(Graph, Node))
        return FailSchemaScope(OutScope, OutError, TEXT("An existing game-thread node with settings owned by this exact graph is required."));
    if (ScopePath.Len() > 512)
        return FailSchemaScope(OutScope, OutError, TEXT("The relative schema scope path exceeds its 512-character limit."));
    UPCGSettings* Settings = Node->GetSettings();
    FPCGSchemaScope Scope{Settings, Settings, Settings->GetClass(), Settings, FString()};
    FPCGReadBudget Budget;
    if (ScopePath.IsEmpty())
    {
        OutScope = MoveTemp(Scope);
        return true;
    }
    TArray<FString> Segments;
    ScopePath.ParseIntoArray(Segments, TEXT("."), false);
    if (Segments.Num() > 8 || Segments.IsEmpty())
        return FailSchemaScope(OutScope, OutError, TEXT("A relative schema path must have at most eight property-name segments."));
    FString Prefix;
    for (const FString& Segment : Segments)
    {
        if (!Budget.IsWithinTime())
            return FailSchemaScope(OutScope, OutError, TEXT("The cooperative schema scope resolution budget expired."));
        if (Segment.IsEmpty() || Segment.Len() >= NAME_SIZE)
            return FailSchemaScope(OutScope, OutError, TEXT("Schema scope paths require nonempty bounded property names, not object paths or array indices."));
        for (const TCHAR Character : Segment)
            if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
                return FailSchemaScope(OutScope, OutError, TEXT("Schema scopes accept dotted reflected property names only; array indexing and object paths are not supported."));
        Prefix += (Prefix.IsEmpty() ? TEXT("") : TEXT(".")) + Segment;
        void* Container = nullptr;
        FString ResolveError;
        FProperty* Property = ResolveNestedPropertyPath(Settings, Prefix, Container, ResolveError);
        if (!Property || !Container || !Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
            return FailSchemaScope(OutScope, OutError, TEXT("The requested schema scope is not an existing editable, nontransient reflected property."));
        Scope.Path += (Scope.Path.IsEmpty() ? TEXT("") : TEXT(".")) + Property->GetName();
        if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
        {
            if (!Struct->Struct || Property->ArrayDim != 1)
                return FailSchemaScope(OutScope, OutError, TEXT("Only a single existing struct can be entered as a schema scope."));
            Scope.Type = Struct->Struct;
            Scope.Container = Struct->ContainerPtrToValuePtr<void>(Container);
        }
        else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
        {
            if (CastField<FClassProperty>(Property) || Property->ArrayDim != 1)
                return FailSchemaScope(OutScope, OutError, TEXT("Class references and static object arrays are not owned instance schema scopes."));
            UObject* Object = ObjectProperty->GetObjectPropertyValue_InContainer(Container);
            if (!IsValid(Object) || Object->HasAnyFlags(RF_ClassDefaultObject) || !Object->IsIn(Settings))
                return FailSchemaScope(OutScope, OutError, TEXT("Only an existing subobject owned by these node settings can be inspected; null, shared and CDO references are refused."));
            Scope.Object = Object;
            Scope.Type = Object->GetClass();
            Scope.Container = Object;
        }
        else
            return FailSchemaScope(OutScope, OutError, TEXT("Only existing owned hard-object and by-value struct scopes are supported; no reference loads or array traversal are performed."));
    }
    if (!Budget.IsWithinTime())
        return FailSchemaScope(OutScope, OutError, TEXT("The cooperative schema scope resolution budget expired."));
    OutScope = MoveTemp(Scope);
    return true;
}
}
#endif
