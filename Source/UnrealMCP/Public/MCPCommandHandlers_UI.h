#pragma once

#include "CoreMinimal.h"
#include "MCPCommandHandlers.h"

class UBlueprint;
class UWidgetBlueprint;

/**
 * Handler to create MVVM-powered UI built on Common Activatable Widgets
 */
class FMCPCreateMVVMUIHandler : public FMCPCommandHandlerBase
{
public:
    FMCPCreateMVVMUIHandler();

    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket) override;

private:
    struct FMVVMUICreationResult
    {
        UBlueprint* ViewModelBlueprint = nullptr;
        UWidgetBlueprint* WidgetBlueprint = nullptr;
    };

    TPair<FMVVMUICreationResult, bool> CreateMVVMUI(const FString& PackagePath, const FString& BaseName, const TSharedPtr<FJsonObject>& Options);

    TPair<UBlueprint*, bool> CreateViewModelBlueprint(const FString& PackagePath, const FString& ViewModelName, const TSharedPtr<FJsonObject>& Options);

    TPair<UWidgetBlueprint*, bool> CreateWidgetBlueprint(const FString& PackagePath, const FString& WidgetName, UBlueprint* ViewModelBlueprint, const TSharedPtr<FJsonObject>& Options);

    bool ConfigureViewModelProperties(UBlueprint* ViewModelBlueprint, const TSharedPtr<FJsonObject>& Options);

    bool ConfigureWidgetBindings(UWidgetBlueprint* WidgetBlueprint, UBlueprint* ViewModelBlueprint, const TSharedPtr<FJsonObject>& Options);

    bool SaveAsset(UObject* Asset);
};
