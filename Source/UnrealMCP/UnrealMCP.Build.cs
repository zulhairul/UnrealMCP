// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMCP : ModuleRules
{
	public UnrealMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core", "CoreUObject", "Engine", "UnrealEd",
				"Networking", "Sockets", "Slate", "SlateCore", "EditorStyle",
				"DeveloperSettings", "Projects", "ToolMenus",
				"BlueprintGraph", "GraphEditor", "KismetCompiler",
				"UMG", "CommonUI", "ModelViewViewModel"
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Json", "JsonUtilities", "Settings", "InputCore", "PythonScriptPlugin",
				"Kismet", "KismetWidgets", "AssetRegistry", "AssetTools",
				"GameplayAbilities", "GameplayTags", "GameplayTasks",
				"UMGEditor", "ModelViewViewModelEditor", "CommonInput"
			}
		);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
