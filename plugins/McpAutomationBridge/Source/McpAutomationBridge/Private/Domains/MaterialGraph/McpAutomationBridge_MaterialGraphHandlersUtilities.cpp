#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/MaterialGraph/McpAutomationBridge_MaterialGraphHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringMainInputs.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVectorParameter.h"

namespace McpMaterialGraphHandlers
{
UMaterialExpression* FindExpressionByIdOrNameOrIndex(
    UMaterial& Material,
    const FString& IdOrName,
    int32 Index)
{
    if (Index >= 0)
    {
        const TArray<UMaterialExpression*>& Expressions = MCP_GET_MATERIAL_EXPRESSIONS((&Material));
        return Index < Expressions.Num() ? Expressions[Index] : nullptr;
    }

    if (IdOrName.IsEmpty())
    {
        return nullptr;
    }

    if (IdOrName.IsNumeric())
    {
        const int32 ParsedIndex = FCString::Atoi(*IdOrName);
        const TArray<UMaterialExpression*>& Expressions = MCP_GET_MATERIAL_EXPRESSIONS((&Material));
        if (ParsedIndex >= 0 && ParsedIndex < Expressions.Num())
        {
            return Expressions[ParsedIndex];
        }
    }

    const FString Needle = IdOrName.TrimStartAndEnd();
    FString ShortNeedle = Needle;
    int32 SeparatorIndex = INDEX_NONE;
    if (ShortNeedle.FindLastChar(TEXT(':'), SeparatorIndex))
    {
        ShortNeedle = ShortNeedle.Mid(SeparatorIndex + 1);
    }
    if (ShortNeedle.FindLastChar(TEXT('.'), SeparatorIndex))
    {
        ShortNeedle = ShortNeedle.Mid(SeparatorIndex + 1);
    }

    for (UMaterialExpression* Expr : MCP_GET_MATERIAL_EXPRESSIONS((&Material)))
    {
        if (!Expr)
        {
            continue;
        }
        if (Expr->MaterialExpressionGuid.ToString() == Needle ||
            Expr->GetName() == Needle ||
            Expr->GetName() == ShortNeedle ||
            Expr->GetPathName() == Needle)
        {
            return Expr;
        }
        if (UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Expr))
        {
            const FString ParamName = ParamExpr->ParameterName.ToString();
            if (ParamName == Needle || ParamName == ShortNeedle)
            {
                return Expr;
            }
        }
    }
    return nullptr;
}

UClass* ResolveGraphNodeClass(const FString& NodeType, bool bUseExtendedAliases)
{
    if (NodeType == TEXT("TextureSample")) { return UMaterialExpressionTextureSample::StaticClass(); }
    if (NodeType == TEXT("VectorParameter") ||
        (bUseExtendedAliases && NodeType == TEXT("ConstantVectorParameter")))
    {
        return UMaterialExpressionVectorParameter::StaticClass();
    }
    if (NodeType == TEXT("ScalarParameter") ||
        (bUseExtendedAliases && NodeType == TEXT("ConstantScalarParameter")))
    {
        return UMaterialExpressionScalarParameter::StaticClass();
    }
    if (NodeType == TEXT("Add")) { return UMaterialExpressionAdd::StaticClass(); }
    if (NodeType == TEXT("Multiply")) { return UMaterialExpressionMultiply::StaticClass(); }
    if (NodeType == TEXT("Constant") ||
        (bUseExtendedAliases && (NodeType == TEXT("Float") || NodeType == TEXT("Scalar"))))
    {
        return UMaterialExpressionConstant::StaticClass();
    }
    if (NodeType == TEXT("Constant3Vector") ||
        NodeType == TEXT("Color") ||
        (bUseExtendedAliases && (NodeType == TEXT("ConstantVector") || NodeType == TEXT("Vector3"))))
    {
        return UMaterialExpressionConstant3Vector::StaticClass();
    }

    UClass* ExpressionClass = ResolveClassByName(NodeType);
    if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
    {
        const FString PrefixedName = FString::Printf(TEXT("MaterialExpression%s"), *NodeType);
        ExpressionClass = ResolveClassByName(PrefixedName);
    }
    return ExpressionClass && ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())
        ? ExpressionClass
        : nullptr;
}

UClass* ResolveBatchNodeClass(const FString& NodeType)
{
    if (NodeType == TEXT("TextureSample")) { return UMaterialExpressionTextureSample::StaticClass(); }
    if (NodeType == TEXT("VectorParameter")) { return UMaterialExpressionVectorParameter::StaticClass(); }
    if (NodeType == TEXT("ScalarParameter")) { return UMaterialExpressionScalarParameter::StaticClass(); }
    if (NodeType == TEXT("Add")) { return UMaterialExpressionAdd::StaticClass(); }
    if (NodeType == TEXT("Multiply")) { return UMaterialExpressionMultiply::StaticClass(); }
    if (NodeType == TEXT("Constant")) { return UMaterialExpressionConstant::StaticClass(); }
    if (NodeType == TEXT("Constant3Vector") || NodeType == TEXT("Color"))
    {
        return UMaterialExpressionConstant3Vector::StaticClass();
    }

    UClass* ExpressionClass = FindObject<UClass>(
        nullptr,
        *FString::Printf(TEXT("/Script/Engine.%s"), *NodeType));
    if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
    {
        const FString PrefixedName = FString::Printf(TEXT("MaterialExpression%s"), *NodeType);
        ExpressionClass = FindObject<UClass>(
            nullptr,
            *FString::Printf(TEXT("/Script/Engine.%s"), *PrefixedName));
    }
    if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
    {
        ExpressionClass = ResolveClassByName(NodeType);
    }
    return ExpressionClass && ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())
        ? ExpressionClass
        : nullptr;
}

UClass* ResolveExpressionClass(const FString& ExpressionClassName)
{
    UClass* ExpressionClass = FindObject<UClass>(
        nullptr,
        *FString::Printf(TEXT("/Script/Engine.%s"), *ExpressionClassName));

    if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
    {
        const FString PrefixedName = FString::Printf(TEXT("MaterialExpression%s"), *ExpressionClassName);
        ExpressionClass = FindObject<UClass>(
            nullptr,
            *FString::Printf(TEXT("/Script/Engine.%s"), *PrefixedName));
    }
    if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
    {
        ExpressionClass = ResolveClassByName(ExpressionClassName);
        if (ExpressionClass && !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
        {
            ExpressionClass = nullptr;
        }
    }
    if (!ExpressionClass)
    {
        const FString PrefixedName = FString::Printf(TEXT("MaterialExpression%s"), *ExpressionClassName);
        ExpressionClass = ResolveClassByName(PrefixedName);
        if (ExpressionClass && !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
        {
            ExpressionClass = nullptr;
        }
    }
    return ExpressionClass;
}

void AddExpressionToMaterial(UMaterial& Material, UMaterialExpression& Expression)
{
#if WITH_EDITORONLY_DATA
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    if (Material.GetEditorOnlyData())
    {
        MCP_GET_MATERIAL_EXPRESSIONS((&Material)).Add(&Expression);
    }
#else
    Material.Expressions.Add(&Expression);
#endif
#endif
}

bool SetMainMaterialInputExpression(
    UMaterial& Material,
    const FString& InputName,
    UMaterialExpression* Expression)
{
    if (FExpressionInput* Input = GetMainMaterialInput(&Material, InputName))
    {
        Input->Expression = Expression;
        return true;
    }
    return false;
}
}
#endif
