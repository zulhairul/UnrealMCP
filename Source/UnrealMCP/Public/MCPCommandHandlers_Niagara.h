#pragma once

#include "CoreMinimal.h"
#include "MCPCommandHandlers.h"

class UNiagaraSystem;
class UNiagaraEmitter;

/**
 * Command handler for creating Niagara systems via MCP
 */
class FMCPCreateNiagaraSystemHandler : public FMCPCommandHandlerBase
{
public:
    FMCPCreateNiagaraSystemHandler()
        : FMCPCommandHandlerBase(TEXT("create_niagara_system"))
    {
    }

    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket) override;

private:
    TPair<UNiagaraSystem*, bool> CreateNiagaraSystem(const FString& PackagePath, const FString& SystemName, const TSharedPtr<FJsonObject>& Options, TArray<FString>& OutWarnings);
    bool ApplySystemCustomizations(UNiagaraSystem* NiagaraSystem, const TSharedPtr<FJsonObject>& Options, TArray<FString>& WarningsOut);
    bool SaveNiagaraSystem(UNiagaraSystem* NiagaraSystem);
};

/**
 * Command handler for modifying existing Niagara systems
 */
class FMCPModifyNiagaraSystemHandler : public FMCPCommandHandlerBase
{
public:
    FMCPModifyNiagaraSystemHandler()
        : FMCPCommandHandlerBase(TEXT("modify_niagara_system"))
    {
    }

    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket) override;

private:
    bool ApplySystemCustomizations(UNiagaraSystem* NiagaraSystem, const TSharedPtr<FJsonObject>& Options, TArray<FString>& WarningsOut);
    bool SaveNiagaraSystem(UNiagaraSystem* NiagaraSystem);
};

/**
 * Command handler for querying Niagara system metadata
 */
class FMCPGetNiagaraSystemInfoHandler : public FMCPCommandHandlerBase
{
public:
    FMCPGetNiagaraSystemInfoHandler()
        : FMCPCommandHandlerBase(TEXT("get_niagara_system_info"))
    {
    }

    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket) override;

private:
    TSharedPtr<FJsonObject> BuildSystemInfoJson(UNiagaraSystem* NiagaraSystem);
};

