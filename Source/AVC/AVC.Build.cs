// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class AVC : ModuleRules
{
	public AVC(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Niagara", "GameplayTags", "ImageWrapper" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("dwmapi.lib");
			PublicSystemLibraries.Add("ole32.lib");
		}

		// JUCE headers
		PublicIncludePaths.Add(System.IO.Path.GetFullPath(System.IO.Path.Combine(ModuleDirectory, "../../juce_system_listener/JuceLibraryCode")));
		PublicIncludePaths.Add(System.IO.Path.GetFullPath(System.IO.Path.Combine(ModuleDirectory, "../../../JUCE/modules")));
		PublicDefinitions.Add("JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1");
		PublicDefinitions.Add("JUCE_STANDALONE_APPLICATION=0");
		PublicDefinitions.Add("JUCE_DONT_DECLARE_PROJECTINFO=1");
		bEnableExceptions = true;
		bUseRTTI = true;
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
