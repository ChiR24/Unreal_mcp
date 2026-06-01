// Copyright (c) 2025-2026 Winyunq. All rights reserved.
// PropertyNameMappings.h
// 已知的大小写映射表，用于处理UE导出JSON与C++ UPROPERTY之间的命名差异
//
// 使用说明：
// 1. 当发现某个属性在camelCase格式下无法正确应用时，添加到此映射表
// 2. NormalizeJsonKeysToPascalCase会自动使用此映射表进行转换
// 3. 格式：{"camelCase属性名", "PascalCase C++属性名"}

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"

/**
 * 获取已知的属性名映射表（camelCase -> PascalCase）
 * 用于解决UE JSON序列化/反序列化的大小写不一致问题
 */
static const TMap<FString, FString>& GetPropertyNameMappings()
{
    static TMap<FString, FString> Mappings = {
        // Slot.Size 结构体属性
        {"sizeRule", "SizeRule"},
        {"value", "Value"},
        
        // Slot对齐属性
        {"horizontalAlignment", "HorizontalAlignment"},
        {"verticalAlignment", "VerticalAlignment"},
        
        // Padding属性
        {"padding", "Padding"},
        {"left", "Left"},
        {"top", "Top"},
        {"right", "Right"},
        {"bottom", "Bottom"},
        
        // 颜色属性
        {"r", "R"},
        {"g", "G"},
        {"b", "B"},
        {"a", "A"},
        
        // 字体属性
        {"size", "Size"},
        {"typefaceFontName", "TypefaceFontName"},
        
        // Widget通用属性
        {"isEnabled", "IsEnabled"},
        {"visibility", "Visibility"},
        {"renderOpacity", "RenderOpacity"},
        {"toolTipText", "ToolTipText"},
        
        // 添加更多映射...
        // 根据测试结果继续补充
    };
    
    return Mappings;
}

/**
 * 获取反向映射表（PascalCase -> camelCase）
 * 用于导出时的一致性检查
 */
static const TMap<FString, FString>& GetReversePropertyNameMappings()
{
    static TMap<FString, FString> ReverseMappings = {
        {"SizeRule", "sizeRule"},
        {"Value", "value"},
        {"HorizontalAlignment", "horizontalAlignment"},
        {"VerticalAlignment", "verticalAlignment"},
        {"Padding", "padding"},
        {"Left", "left"},
        {"Top", "top"},
        {"Right", "right"},
        {"Bottom", "bottom"},
        {"R", "r"},
        {"G", "g"},
        {"B", "b"},
        {"A", "a"},
        {"Size", "size"},
        {"TypefaceFontName", "typefaceFontName"},
        {"IsEnabled", "isEnabled"},
        {"Visibility", "visibility"},
        {"RenderOpacity", "renderOpacity"},
        {"ToolTipText", "toolTipText"},
    };
    
    return ReverseMappings;
}

/**
 * 规范化属性名：如果在映射表中找到，使用映射值；否则使用首字母大写规则
 * 
 * @param Key 原始属性名（可能是camelCase）
 * @return 规范化后的属性名（PascalCase）
 */
static FString NormalizePropertyName(const FString& Key)
{
    const TMap<FString, FString>& Mappings = GetPropertyNameMappings();
    
    // 1. 优先查找映射表
    if (const FString* MappedName = Mappings.Find(Key))
    {
        return *MappedName;
    }
    
    // 2. 如果不在映射表中，使用首字母大写规则
    FString NormalizedKey = Key;
    if (NormalizedKey.Len() > 0 && FChar::IsLower(NormalizedKey[0]))
    {
        NormalizedKey[0] = FChar::ToUpper(NormalizedKey[0]);
    }
    
    return NormalizedKey;
}
