#pragma once

#include "CoreMinimal.h"
#include "MCPCommandHandlers.h"

class UGameplayEffect;

/**
 * Handler for creating Gameplay Effect assets via MCP commands.
 */
class FMCPCreateGameplayEffectHandler : public FMCPCommandHandlerBase
{
public:
    FMCPCreateGameplayEffectHandler();

    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket) override;

private:
    bool ConfigureGameplayEffect(UGameplayEffect* GameplayEffect, const TSharedPtr<FJsonObject>& Config, FString& OutErrorMessage) const;
    bool ConfigureDuration(UGameplayEffect* GameplayEffect, const TSharedPtr<FJsonObject>& Config, FString& OutErrorMessage) const;
    bool ConfigurePeriodAndStacking(UGameplayEffect* GameplayEffect, const TSharedPtr<FJsonObject>& Config, FString& OutErrorMessage) const;
    bool ConfigureModifiers(UGameplayEffect* GameplayEffect, const TArray<TSharedPtr<FJsonValue>>& ModifiersArray, FString& OutErrorMessage) const;
    bool ConfigureTags(UGameplayEffect* GameplayEffect, const TSharedPtr<FJsonObject>& Config, FString& OutErrorMessage) const;
    bool SaveGameplayEffect(UGameplayEffect* GameplayEffect, const FString& PackageName, bool bCreatedNewAsset, FString& OutErrorMessage) const;
};

/**
 * Handler for registering Gameplay Effects inside Data Tables.
 */
class FMCPRegisterGameplayEffectHandler : public FMCPCommandHandlerBase
{
public:
    FMCPRegisterGameplayEffectHandler();

    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket) override;

private:
    bool BuildRowPayload(const FString& RowName, const FString& EffectPath, const FString& EffectField, const TSharedPtr<FJsonObject>& AdditionalData, TSharedPtr<FJsonObject>& OutRowPayload, FString& OutErrorMessage) const;
};

/**
 * Handler for generating Gameplay Attribute Set C++ classes.
 */
class FMCPCreateAttributeSetHandler : public FMCPCommandHandlerBase
{
public:
    FMCPCreateAttributeSetHandler();

    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket) override;

private:
    struct FGeneratedAttribute
    {
        FString PropertyName;
        FString Category;
        FString Tooltip;
        float InitialValue = 0.0f;
        bool bReplicated = true;
    };

    FString SanitiseIdentifier(const FString& InName, const FString& FallbackPrefix, FString& OutError) const;
    bool ParseAttributes(const TArray<TSharedPtr<FJsonValue>>& AttributeArray, TArray<FGeneratedAttribute>& OutAttributes, FString& OutError) const;
    FString BuildHeaderContent(const FString& ClassName, const FString& ModuleApiMacro, const FString& GeneratedInclude, const TArray<FGeneratedAttribute>& Attributes, bool& bOutHasReplication) const;
    FString BuildSourceContent(const FString& ClassName, const FString& SourceInclude, const TArray<FGeneratedAttribute>& Attributes, bool bHasReplication) const;
    FString FormatFloatLiteral(float Value) const;
};
