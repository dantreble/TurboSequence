// Copyright Lukas Fratzl, 2022-2024. All Rights Reserved.


#include "TurboSequence_MeshAssetAction_Lf.h"

#include "TurboSequence_Editor_Lf.h"
#include "TurboSequence_MeshAsset_Lf.h"


UClass* FTurboSequence_MeshAssetAction_Lf::GetSupportedClass() const
{
	return UTurboSequence_MeshAsset_Lf::StaticClass();
}

FText FTurboSequence_MeshAssetAction_Lf::GetName() const
{
	return INVTEXT("Mesh Asset");
}

FColor FTurboSequence_MeshAssetAction_Lf::GetTypeColor() const
{
	return FColor::Green;
}

uint32 FTurboSequence_MeshAssetAction_Lf::GetCategories()
{
	return FTurboSequence_Editor_LfModule::PluginAssetCategory;
}

void FTurboSequence_MeshAssetAction_Lf::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	TArray<TWeakObjectPtr<UTurboSequence_MeshAsset_Lf>> Objects = GetTypedWeakObjectPtrs<UTurboSequence_MeshAsset_Lf>(InObjects);
	
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("FTurboSequence_MeshAssetAction","MeshAssetAction_CreateSM", "Create/Update Static Mesh"),
		NSLOCTEXT("FTurboSequence_MeshAssetAction", "MeshAssetAction_CreateSMTooltip", "Attempts to create/update static mesh version of the skinned mesh"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FTurboSequence_MeshAssetAction_Lf::CreateStaticMesh, Objects ),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddMenuSeparator();
}

void FTurboSequence_MeshAssetAction_Lf::CreateStaticMesh(TArray<TWeakObjectPtr<class UTurboSequence_MeshAsset_Lf>> Objects)
{
	for (TWeakObjectPtr<UTurboSequence_MeshAsset_Lf> MeshAssetPtr : Objects)
	{
		if(UTurboSequence_MeshAsset_Lf* MeshAsset = MeshAssetPtr.Get())
		{
			FTurboSequence_Editor_LfModule::CreateStaticMesh(MeshAsset);
		}
	}
}

