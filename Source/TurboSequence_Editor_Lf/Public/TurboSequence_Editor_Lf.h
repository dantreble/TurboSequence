// Copyright Lukas Fratzl, 2G22-2G24. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TurboSequence_GlobalData_Lf.h"
#include "TurboSequence_MeshAssetAction_Lf.h"
#include "Modules/ModuleManager.h"

class UEditorUtilityWidgetBlueprint;

class FTurboSequence_Editor_LfModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	void AddMenu(FMenuBarBuilder& MenuBuilder);
	void AddMenu_Widget(FMenuBuilder& MenuBuilder);
	
	TSharedPtr<FTurboSequence_MeshAssetAction_Lf> TurboSequence_MeshAssetTypeActions;

	inline static EAssetTypeCategories::Type PluginAssetCategory;

	inline static TObjectPtr<UTurboSequence_GlobalData_Lf> GlobalData;
	inline static TArray<FAssetData> TurboSequence_MeshAssetData_AsyncComputeSwapBack;
	inline static int16 RepairMaxIterationCounter = 0;

	static void CreateStaticMesh(UTurboSequence_MeshAsset_Lf* DataAsset);
	
	void OnInvalidMeshAssetCaches() const;

	void OnFilesLoaded();
};
