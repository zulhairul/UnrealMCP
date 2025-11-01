#include "MCPCommandHandlers_UI.h"

#include "MCPFileLogger.h"

#include "AssetToolsModule.h"
#include "Factories/WidgetBlueprintFactory.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewModelContext.h"
#include "MVVMBlueprintPropertyPath.h"
#include "MVVMBindingMode.h"
#include "MVVMViewModelBase.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "MVVMViewModelBlueprint.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "CommonActivatableWidget.h"
#include "CommonTextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Editor.h"
#include "EdGraphSchema_K2.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"

namespace
{
    FString NormalizePackagePath(const FString& InPath)
    {
        if (InPath.StartsWith(TEXT("/")))
        {
            return InPath;
        }

        FString Sanitized = InPath;
        Sanitized.ReplaceInline(TEXT("\\"), TEXT("/"));
        Sanitized.RemoveFromStart(TEXT("/"));
        Sanitized.RemoveFromStart(TEXT("Game/"));

        return FString::Printf(TEXT("/Game/%s"), *Sanitized);
    }

    FString MakeAssetPath(const FString& PackagePath, const FString& AssetName)
    {
        return FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);
    }

    FString EnsureSubDirectory(const FString& PackagePath, const FString& SubDir)
    {
        return FString::Printf(TEXT("%s/%s"), *PackagePath, *SubDir);
    }

    bool PrepareAssetDirectory(const FString& PackagePath)
    {
        FString FileSystemPath = FPackageName::LongPackageNameToFilename(PackagePath, TEXT(""));
        return IFileManager::Get().MakeDirectory(*FileSystemPath, true);
    }

    FEdGraphPinType MakePinType(const FString& TypeString)
    {
        const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
        FEdGraphPinType PinType;

        if (TypeString.Equals(TEXT("float"), ESearchCase::IgnoreCase))
        {
            PinType = FEdGraphPinType(Schema->PC_Real, Schema->PC_Float, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
        }
        else if (TypeString.Equals(TEXT("double"), ESearchCase::IgnoreCase))
        {
            PinType = FEdGraphPinType(Schema->PC_Real, Schema->PC_Double, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
        }
        else if (TypeString.Equals(TEXT("int"), ESearchCase::IgnoreCase) || TypeString.Equals(TEXT("integer"), ESearchCase::IgnoreCase))
        {
            PinType = FEdGraphPinType(Schema->PC_Int, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
        }
        else if (TypeString.Equals(TEXT("bool"), ESearchCase::IgnoreCase))
        {
            PinType = FEdGraphPinType(Schema->PC_Boolean, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
        }
        else if (TypeString.Equals(TEXT("text"), ESearchCase::IgnoreCase))
        {
            PinType = FEdGraphPinType(Schema->PC_Text, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
        }
        else
        {
            PinType = FEdGraphPinType(Schema->PC_String, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
        }

        return PinType;
    }

    FString MakeDefaultValueString(const FString& TypeString, const FString& InputValue)
    {
        if (TypeString.Equals(TEXT("bool"), ESearchCase::IgnoreCase))
        {
            return InputValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) ? TEXT("True") : TEXT("False");
        }
        return InputValue;
    }
}

FMCPCreateMVVMUIHandler::FMCPCreateMVVMUIHandler()
    : FMCPCommandHandlerBase(TEXT("create_mvvm_ui"))
{
}

TSharedPtr<FJsonObject> FMCPCreateMVVMUIHandler::Execute(const TSharedPtr<FJsonObject>& Params, FSocket* /*ClientSocket*/)
{
    FString PackagePath = TEXT("/Game/UI");
    Params->TryGetStringField(TEXT("package_path"), PackagePath);
    PackagePath = NormalizePackagePath(PackagePath);

    FString BaseName;
    if (!Params->TryGetStringField(TEXT("name"), BaseName) || BaseName.IsEmpty())
    {
        return CreateErrorResponse(TEXT("Missing 'name' field"));
    }

    const TSharedPtr<FJsonObject>* OptionsPtr = nullptr;
    Params->TryGetObjectField(TEXT("options"), OptionsPtr);
    TSharedPtr<FJsonObject> Options = OptionsPtr ? *OptionsPtr : MakeShared<FJsonObject>();

    MCP_LOG_INFO("Creating MVVM UI for %s in %s", *BaseName, *PackagePath);

    TPair<FMVVMUICreationResult, bool> CreationResult = CreateMVVMUI(PackagePath, BaseName, Options);
    if (!CreationResult.Value)
    {
        return CreateErrorResponse(TEXT("Failed to create MVVM UI assets"));
    }

    const FMVVMUICreationResult& Assets = CreationResult.Key;

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    if (Assets.ViewModelBlueprint)
    {
        ResultObj->SetStringField(TEXT("view_model"), Assets.ViewModelBlueprint->GetPathName());
    }
    if (Assets.WidgetBlueprint)
    {
        ResultObj->SetStringField(TEXT("widget"), Assets.WidgetBlueprint->GetPathName());
    }

    return CreateSuccessResponse(ResultObj);
}

TPair<FMCPCreateMVVMUIHandler::FMVVMUICreationResult, bool> FMCPCreateMVVMUIHandler::CreateMVVMUI(const FString& PackagePath, const FString& BaseName, const TSharedPtr<FJsonObject>& Options)
{
    FMVVMUICreationResult Result;

    FString ViewModelName = BaseName + TEXT("ViewModel");
    FString WidgetName = BaseName + TEXT("Screen");

    if (Options.IsValid())
    {
        const TSharedPtr<FJsonObject>* ViewModelOptions = nullptr;
        if (Options->TryGetObjectField(TEXT("viewmodel"), ViewModelOptions) && ViewModelOptions)
        {
            FString CustomName;
            if ((*ViewModelOptions)->TryGetStringField(TEXT("name"), CustomName) && !CustomName.IsEmpty())
            {
                ViewModelName = CustomName;
            }
        }

        const TSharedPtr<FJsonObject>* WidgetOptions = nullptr;
        if (Options->TryGetObjectField(TEXT("widget"), WidgetOptions) && WidgetOptions)
        {
            FString CustomName;
            if ((*WidgetOptions)->TryGetStringField(TEXT("name"), CustomName) && !CustomName.IsEmpty())
            {
                WidgetName = CustomName;
            }
        }
    }

    FString ViewModelPath = EnsureSubDirectory(PackagePath, TEXT("ViewModels"));
    FString WidgetPath = EnsureSubDirectory(PackagePath, TEXT("Widgets"));

    PrepareAssetDirectory(ViewModelPath);
    PrepareAssetDirectory(WidgetPath);

    TPair<UBlueprint*, bool> ViewModelResult = CreateViewModelBlueprint(ViewModelPath, ViewModelName, Options);
    if (!ViewModelResult.Value || !ViewModelResult.Key)
    {
        MCP_LOG_ERROR("Failed to create MVVM ViewModel blueprint");
        return TPair<FMVVMUICreationResult, bool>(Result, false);
    }

    Result.ViewModelBlueprint = ViewModelResult.Key;

    TPair<UWidgetBlueprint*, bool> WidgetResult = CreateWidgetBlueprint(WidgetPath, WidgetName, ViewModelResult.Key, Options);
    if (!WidgetResult.Value || !WidgetResult.Key)
    {
        MCP_LOG_ERROR("Failed to create Common Activatable widget blueprint");
        return TPair<FMVVMUICreationResult, bool>(Result, false);
    }

    Result.WidgetBlueprint = WidgetResult.Key;

    return TPair<FMVVMUICreationResult, bool>(Result, true);
}

TPair<UBlueprint*, bool> FMCPCreateMVVMUIHandler::CreateViewModelBlueprint(const FString& PackagePath, const FString& ViewModelName, const TSharedPtr<FJsonObject>& Options)
{
    FString FullAssetPath = MakeAssetPath(PackagePath, ViewModelName);

    if (UBlueprint* Existing = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *FullAssetPath)))
    {
        MCP_LOG_WARNING("ViewModel blueprint already exists at %s", *FullAssetPath);
        return TPair<UBlueprint*, bool>(Existing, true);
    }

    UPackage* Package = CreatePackage(*FullAssetPath);
    if (!Package)
    {
        MCP_LOG_ERROR("Failed to create package for ViewModel at %s", *FullAssetPath);
        return TPair<UBlueprint*, bool>(nullptr, false);
    }

    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        UMVVMViewModelBase::StaticClass(),
        Package,
        FName(*ViewModelName),
        BPTYPE_Normal,
        UMVVMViewModelBlueprint::StaticClass(),
        UMVVMViewModelBlueprintGeneratedClass::StaticClass());

    if (!Blueprint)
    {
        MCP_LOG_ERROR("Failed to instantiate ViewModel blueprint %s", *ViewModelName);
        return TPair<UBlueprint*, bool>(nullptr, false);
    }

    ConfigureViewModelProperties(Blueprint, Options);

    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    SaveAsset(Blueprint);

    return TPair<UBlueprint*, bool>(Blueprint, true);
}

TPair<UWidgetBlueprint*, bool> FMCPCreateMVVMUIHandler::CreateWidgetBlueprint(const FString& PackagePath, const FString& WidgetName, UBlueprint* ViewModelBlueprint, const TSharedPtr<FJsonObject>& Options)
{
    FString FullAssetPath = MakeAssetPath(PackagePath, WidgetName);

    if (UWidgetBlueprint* Existing = Cast<UWidgetBlueprint>(StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *FullAssetPath)))
    {
        MCP_LOG_WARNING("Widget blueprint already exists at %s", *FullAssetPath);
        return TPair<UWidgetBlueprint*, bool>(Existing, true);
    }

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));

    UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
    Factory->bUseWidgetTemplate = false;
    Factory->ParentClass = UCommonActivatableWidget::StaticClass();

    UObject* CreatedAsset = AssetToolsModule.Get().CreateAsset(WidgetName, PackagePath, UWidgetBlueprint::StaticClass(), Factory);
    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(CreatedAsset);
    if (!WidgetBlueprint)
    {
        MCP_LOG_ERROR("Failed to create widget blueprint %s", *WidgetName);
        return TPair<UWidgetBlueprint*, bool>(nullptr, false);
    }

    ConfigureWidgetBindings(WidgetBlueprint, ViewModelBlueprint, Options);

    FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
    SaveAsset(WidgetBlueprint);

    return TPair<UWidgetBlueprint*, bool>(WidgetBlueprint, true);
}

bool FMCPCreateMVVMUIHandler::ConfigureViewModelProperties(UBlueprint* ViewModelBlueprint, const TSharedPtr<FJsonObject>& Options)
{
    if (!ViewModelBlueprint || !Options.IsValid())
    {
        return true;
    }

    const TSharedPtr<FJsonObject>* ViewModelOptions = nullptr;
    if (!Options->TryGetObjectField(TEXT("viewmodel"), ViewModelOptions) || !ViewModelOptions)
    {
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* Properties = nullptr;
    if (!(*ViewModelOptions)->TryGetArrayField(TEXT("properties"), Properties) || !Properties)
    {
        return true;
    }

    bool bModified = false;
    for (const TSharedPtr<FJsonValue>& Entry : *Properties)
    {
        const TSharedPtr<FJsonObject>* PropObj = nullptr;
        if (!Entry->TryGetObject(PropObj) || !PropObj)
        {
            continue;
        }

        FString PropertyName;
        if (!(*PropObj)->TryGetStringField(TEXT("name"), PropertyName) || PropertyName.IsEmpty())
        {
            continue;
        }

        FString TypeString = TEXT("String");
        (*PropObj)->TryGetStringField(TEXT("type"), TypeString);

        FEdGraphPinType PinType = MakePinType(TypeString);
        if (!FBlueprintEditorUtils::AddMemberVariable(ViewModelBlueprint, FName(*PropertyName), PinType))
        {
            continue;
        }

        FString DefaultValue;
        if ((*PropObj)->TryGetStringField(TEXT("default"), DefaultValue))
        {
            FBPVariableDescription* VarDesc = FBlueprintEditorUtils::FindNewVariable(ViewModelBlueprint, FName(*PropertyName));
            if (VarDesc)
            {
                VarDesc->DefaultValue = MakeDefaultValueString(TypeString, DefaultValue);
            }
        }

        bModified = true;
    }

    if (bModified)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ViewModelBlueprint);
    }

    return true;
}

bool FMCPCreateMVVMUIHandler::ConfigureWidgetBindings(UWidgetBlueprint* WidgetBlueprint, UBlueprint* ViewModelBlueprint, const TSharedPtr<FJsonObject>& Options)
{
    if (!WidgetBlueprint || !ViewModelBlueprint)
    {
        return false;
    }

    UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(WidgetBlueprint, TEXT("WidgetTree"));
        WidgetBlueprint->WidgetTree = WidgetTree;
    }

    WidgetBlueprint->Modify();

    UCanvasPanel* RootPanel = Cast<UCanvasPanel>(WidgetTree->RootWidget);
    if (!RootPanel)
    {
        RootPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootPanel"));
        WidgetTree->RootWidget = RootPanel;
    }

    UCommonTextBlock* HeaderText = WidgetTree->FindWidget<UCommonTextBlock>(TEXT("HeaderText"));
    if (!HeaderText)
    {
        HeaderText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("HeaderText"));
        HeaderText->SetText(FText::FromString(TEXT("Hello from MVVM")));
        if (RootPanel)
        {
            UCanvasPanelSlot* Slot = RootPanel->AddChildToCanvas(HeaderText);
            if (Slot)
            {
                Slot->SetAnchors(FAnchors(0.5f, 0.5f));
                Slot->SetAlignment(FVector2D(0.5f, 0.5f));
                Slot->SetPosition(FVector2D(0.f, 0.f));
            }
        }
    }

    UMVVMWidgetBlueprintExtension_View* ViewExtension = WidgetBlueprint->GetExtensionByType<UMVVMWidgetBlueprintExtension_View>();
    if (!ViewExtension)
    {
        ViewExtension = WidgetBlueprint->AddExtension<UMVVMWidgetBlueprintExtension_View>();
    }

    if (!ViewExtension)
    {
        MCP_LOG_WARNING("Failed to acquire MVVM view extension for widget %s", *WidgetBlueprint->GetName());
        return false;
    }

    UMVVMBlueprintView* BlueprintView = ViewExtension->GetBlueprintView();
    if (!BlueprintView)
    {
        MCP_LOG_WARNING("Failed to obtain MVVM blueprint view for widget %s", *WidgetBlueprint->GetName());
        return false;
    }

    const FName ViewModelId(*ViewModelBlueprint->GetName());
    FMVVMBlueprintViewModelContext ViewModelContext;
    ViewModelContext.SetViewModelName(ViewModelId);
    ViewModelContext.SetViewModelClass(ViewModelBlueprint->GeneratedClass);

    if (!BlueprintView->FindViewModel(ViewModelId))
    {
        BlueprintView->AddViewModel(ViewModelContext);
    }

    // Bind the first property if one exists
    FString FirstPropertyName;
    if (ViewModelBlueprint->NewVariables.Num() > 0)
    {
        FirstPropertyName = ViewModelBlueprint->NewVariables[0].VarName.ToString();
    }

    if (!FirstPropertyName.IsEmpty() && HeaderText)
    {
        FMVVMBlueprintPropertyPath SourcePath;
        SourcePath.SetViewModelName(ViewModelId);
        SourcePath.AppendProperty(FirstPropertyName);

        FMVVMBlueprintPropertyPath DestinationPath;
        DestinationPath.SetWidgetName(HeaderText->GetFName());
        DestinationPath.AppendProperty(TEXT("Text"));

        FMVVMBlueprintViewBinding Binding;
        Binding.SetSourcePath(SourcePath);
        Binding.SetDestinationPath(DestinationPath);
        Binding.SetBindingType(EMVVMBindingMode::OneWay);

        BlueprintView->AddBinding(Binding);
    }

    return true;
}

bool FMCPCreateMVVMUIHandler::SaveAsset(UObject* Asset)
{
    if (!Asset)
    {
        return false;
    }

    UPackage* Package = Asset->GetOutermost();
    if (!Package)
    {
        return false;
    }

    FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    const bool bSuccess = UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);
    if (bSuccess)
    {
        FAssetRegistryModule::AssetCreated(Asset);
    }

    return bSuccess;
}
