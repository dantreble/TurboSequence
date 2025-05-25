// Copyright Lukas Fratzl, 2G22-2G24. All Rights Reserved.

#include "TurboSequence_Editor_Lf.h"

#include "BoneWeights.h"
#include "MeshUtilities.h"
#include "PackageTools.h"
#include "TurboSequence_MeshAsset_Lf.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Rendering/RenderCommandPipes.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "FTurboSequence_Editor_LfModule"

class IMeshUtilities;

void FTurboSequence_Editor_LfModule::StartupModule()
{
	TurboSequence_MeshAssetData_AsyncComputeSwapBack.Empty();

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	PluginAssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("Turbo Sequence")),
	                                                               LOCTEXT("Turbo Sequence", "Turbo Sequence"));

	TurboSequence_MeshAssetTypeActions = MakeShared<FTurboSequence_MeshAssetAction_Lf>();
	AssetTools.RegisterAssetTypeActions(TurboSequence_MeshAssetTypeActions.ToSharedRef());
	
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry.OnFilesLoaded().AddRaw(this, &FTurboSequence_Editor_LfModule::OnFilesLoaded);
}

void FTurboSequence_Editor_LfModule::ShutdownModule()
{
	if (!FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		return;
	}

	FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(TurboSequence_MeshAssetTypeActions.ToSharedRef());
	

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry.OnFilesLoaded().RemoveAll(this);
}


void FTurboSequence_Editor_LfModule::CreateStaticMesh(UTurboSequence_MeshAsset_Lf* DataAsset)
{
	const uint32 UVChannel = 1;

	USkeletalMesh* OriginalMesh = DataAsset->ReferenceMeshNative.Get();
	
	if(!IsValid(OriginalMesh))
	{
		return;
	}

	//Create a duplicate of the original mesh, because we are going to modify its vertex colours and uvs
	USkeletalMesh* SkeletalMeshCopy = DuplicateObject<USkeletalMesh>(OriginalMesh, OriginalMesh->GetOuter());
	
	const int32 NumLoDs = SkeletalMeshCopy->GetLODNum(); 
	
	TSet<uint32> UsedBoneSet;
	TSet<uint32> UsedBoneSetWithParents;

	FSkeletalMeshRenderData* SkeletalMeshRenderData = SkeletalMeshCopy->GetResourceForRendering();

	FReferenceSkeleton& ReferenceSkeleton = OriginalMesh->GetRefSkeleton();
	
	bool bAddParents = true;

	uint32 MaxSupportedInfluences = 4;
	
	for (int32 LODIndex = 0; LODIndex < NumLoDs; LODIndex++)
	{
		const FSkeletalMeshLODRenderData &SkeletalMeshLODRenderData = SkeletalMeshRenderData->LODRenderData[LODIndex];
		const FSkinWeightVertexBuffer* SkinWeightBuffer = SkeletalMeshLODRenderData.GetSkinWeightVertexBuffer();

		uint32 Influences = FMath::Min(SkinWeightBuffer->GetMaxBoneInfluences(),MaxSupportedInfluences);

		for (const FSkelMeshRenderSection& RenderSection : SkeletalMeshLODRenderData.RenderSections)
		{
			TSet<int32> SectionBones;
			
			int32 SectionVertexBufferIndex = RenderSection.GetVertexBufferIndex();
			for (int32 SectionVertexIndex = 0; SectionVertexIndex < RenderSection.GetNumVertices(); ++SectionVertexIndex)
			{
				int32 VertexIndex = SectionVertexIndex + SectionVertexBufferIndex;

				for( uint32 InfluenceIndex = 0; InfluenceIndex < Influences; ++InfluenceIndex)
				{
					SectionBones.Add(SkinWeightBuffer->GetBoneIndex(VertexIndex, InfluenceIndex));
				}
			}

			for (int32 SectionBone : SectionBones)
			{
				int32 Bone = RenderSection.BoneMap[SectionBone];

				UsedBoneSet.Add(Bone);
				
				while( Bone != INDEX_NONE )
				{
					bool bAlreadyInSet = false;
					UsedBoneSetWithParents.Add(Bone, &bAlreadyInSet);
					Bone = bAddParents && !bAlreadyInSet ? ReferenceSkeleton.GetParentIndex(Bone) : INDEX_NONE;	
				}
			}
		}
	}

	//Add Sockets (and parents)
	for (FName KeepSocket : DataAsset->KeepSockets)
	{
		if(USkeletalMeshSocket* SkeletalMeshSocket = DataAsset->ReferenceMeshNative->FindSocket(KeepSocket))
		{
			int32 Bone = ReferenceSkeleton.FindBoneIndex(SkeletalMeshSocket->BoneName);

			UsedBoneSet.Add(Bone);

			while( Bone != INDEX_NONE )
			{
				bool bAlreadyInSet = false;
				UsedBoneSetWithParents.Add(Bone, &bAlreadyInSet);
				Bone = bAddParents && !bAlreadyInSet ? ReferenceSkeleton.GetParentIndex(Bone) : INDEX_NONE;	
			}
		}
	}
	
	TArray UsedBoneOrdered(UsedBoneSetWithParents.Array());
	UsedBoneOrdered.Sort();

	TArray<int32> UsedBoneOrderedDirectParent;
	const int32 UsedBoneCount = UsedBoneOrdered.Num();
	for(int32 UsedBoneIndex = 0; UsedBoneIndex < UsedBoneCount; ++UsedBoneIndex)
	{
		UsedBoneOrderedDirectParent.Add(ReferenceSkeleton.GetParentIndex(UsedBoneOrdered[UsedBoneIndex]));
	}

	//Initialise the local parent bone cache to INDEX_NONE
	const int MaxParentCache = 5;
	int MaxParentCacheUsed = 0;

	TArray<int32> EmptyCache;

	EmptyCache.Init(INDEX_NONE,MaxParentCache );

	TArray< TArray<int32> > UsedBoneParentCache;
	for(int32 UsedBoneIndex = 0; UsedBoneIndex < UsedBoneCount; ++UsedBoneIndex)
	{
		UsedBoneParentCache.Add(EmptyCache);
	}

	//Allocate the lines in the local parent cache
	TArray<int32> UsedBoneParentWriteIndex; //INDEX_NONE no write, 0 nothing, 1 and beyond local bone cache

	UsedBoneParentWriteIndex.Init(INDEX_NONE, UsedBoneCount);
	
	for(int32 UsedBoneIndex = 0; UsedBoneIndex < UsedBoneCount; ++UsedBoneIndex)
	{
		uint32 BoneIndex = UsedBoneOrdered[UsedBoneIndex];
		int32 FirstUse = UsedBoneIndex;
		int32 LastUseAsParent;

		//If LastUse is more than the next index away from first use, the bone will need to exist in the cache from first use -1 last use
		if(UsedBoneOrderedDirectParent.FindLast(BoneIndex, LastUseAsParent))
		{
			if(LastUseAsParent > FirstUse + 1)
			{
				int32 FoundLine = INDEX_NONE;
				//Add the parent to the local cache and assign a position
				//Allocate a cache line that the bone can exist on for the duration it is needed
				for(int32 CacheIndex = 0; CacheIndex < MaxParentCache; ++CacheIndex)
				{
					bool LineClear = true;
					
					for(int32 UseIndex = FirstUse; UseIndex < LastUseAsParent; ++UseIndex)
					{
						if(UsedBoneParentCache[UseIndex][CacheIndex] != INDEX_NONE)
						{
							LineClear = false;
							break;
						}
					}

					if(LineClear)
					{
						FoundLine = CacheIndex;
						break;
					}
				}

				if(FoundLine != INDEX_NONE)
				{
					//Allocate the line
					for(int32 UseIndex = FirstUse; UseIndex < LastUseAsParent; ++UseIndex)
					{
						UsedBoneParentCache[UseIndex][FoundLine] = BoneIndex;
					}

					UsedBoneParentWriteIndex[FirstUse] = FoundLine + 1; //Needs to be written to the local cache
					
					MaxParentCacheUsed = FMath::Max(MaxParentCacheUsed, FoundLine);
				}
			}
		}
	}

	

	//Encode the cache read/write instructions
	
	TArray<int32> UsedBoneParentReadIndex; //-1 no parent (reset), 0 previous bone parent, 1 and beyond local bone cache

	for(int32 UsedBoneIndex = 0; UsedBoneIndex < UsedBoneCount; ++UsedBoneIndex)
	{
		const int32 BoneParentIndex = UsedBoneOrderedDirectParent[UsedBoneIndex];

		if(BoneParentIndex != INDEX_NONE)
		{
			const int32 BoneIndex = UsedBoneOrdered[UsedBoneIndex];
			
			if (BoneParentIndex == BoneIndex - 1)
			{
				UsedBoneParentReadIndex.Add(0);
			}
			else
			{
				int32 PreviousCacheLine = UsedBoneIndex - 1;
				//Find the cache line to read from (read from previous cache line)
				if(PreviousCacheLine > 0)
				{
					const TArray<int32>& CachedBones = UsedBoneParentCache[PreviousCacheLine];
					int32 ParentCacheIndex = CachedBones.Find(BoneParentIndex);
				
					UsedBoneParentReadIndex.Add(ParentCacheIndex + 1);
				}
				else
				{
					UsedBoneParentReadIndex.Add(0);
				}
				
			}
		}
		else
		{
			UsedBoneParentReadIndex.Add(INDEX_NONE);
		}

	}
	
	//Print the cache
	for(int32 UsedBoneIndex = 0; UsedBoneIndex < UsedBoneCount; ++UsedBoneIndex)
	{
		uint32 BoneIndex = UsedBoneOrdered[UsedBoneIndex];
	
		TArray<int32> &Cache = UsedBoneParentCache[UsedBoneIndex];
	 	
		UE_LOG(LogTurboSequence_Lf, Display, TEXT("Bone %d, Parent %d, %d, %d, %d, %d, %d, read %d, write %d "), BoneIndex, UsedBoneOrderedDirectParent[UsedBoneIndex], Cache[0],Cache[1],Cache[2],Cache[3],Cache[4], UsedBoneParentReadIndex[UsedBoneIndex], UsedBoneParentWriteIndex[UsedBoneIndex]  );
	}

	UE_LOG(LogTurboSequence_Lf, Display, TEXT("MaxParentCacheUsed %d"), MaxParentCacheUsed);
	
	TMap<int32, int32> CPUBoneToGPUBoneIndicesMap;
		
	for(int32 RemappedIndex = 0; RemappedIndex < UsedBoneCount; ++RemappedIndex)
	{
		CPUBoneToGPUBoneIndicesMap.Add(UsedBoneOrdered[RemappedIndex],RemappedIndex);
	}

	//Modify the skeletal mesh
	SkeletalMeshCopy->SetFlags(RF_Transactional);
	SkeletalMeshCopy->Modify();

	SkeletalMeshCopy->SetHasVertexColors(true);
	SkeletalMeshCopy->SetVertexColorGuid(FGuid::NewGuid());

	// Release the static mesh's resources.
	SkeletalMeshCopy->ReleaseResources();

	// Flush the resource release commands to the rendering thread to ensure that the build doesn't occur while a resource is still
	// allocated, and potentially accessing the UStaticMesh.
	SkeletalMeshCopy->ReleaseResourcesFence.Wait();
	
	for (int32 LODIndex = 0; LODIndex < NumLoDs; LODIndex++)
	{
		FSkeletalMeshLODRenderData &LODData = SkeletalMeshRenderData->LODRenderData[LODIndex];

		auto& [StaticMeshVertexBuffer, PositionVertexBuffer, ColorVertexBuffer] = LODData.StaticVertexBuffers;
		
		uint32 NumVertices = LODData.GetNumVertices();
		
		if (LODData.StaticVertexBuffers.ColorVertexBuffer.GetNumVertices() == 0)
		{
			// Mesh doesn't have a color vertex buffer yet!  We'll create one now.
			ColorVertexBuffer.InitFromSingleColor(FColor::White, NumVertices);
			BeginInitResource(&ColorVertexBuffer, &UE::RenderCommandPipe::SkeletalMesh);
		}
		
		//Take a copy of the original uv0 and tangents before I recreate the buffer 
		TArray<FVector2f> UV0;
		TArray<FVector3f> TangentX;
		TArray<FVector3f> TangentY;
		TArray<FVector3f> TangentZ;
		for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			UV0.Add(StaticMeshVertexBuffer.GetVertexUV(VertexIndex,0));
			TangentX.Add(StaticMeshVertexBuffer.VertexTangentX(VertexIndex));
			TangentY.Add(StaticMeshVertexBuffer.VertexTangentY(VertexIndex));
			TangentZ.Add(StaticMeshVertexBuffer.VertexTangentZ(VertexIndex));
		}
		
		//Re init the buffer with more uvs
		StaticMeshVertexBuffer.Init(StaticMeshVertexBuffer.GetNumVertices(),UVChannel+2,StaticMeshVertexBuffer.GetAllowCPUAccess());
		BeginInitResource(&StaticMeshVertexBuffer, &UE::RenderCommandPipe::SkeletalMesh);

		const FSkinWeightVertexBuffer* SkinWeightBuffer = LODData.GetSkinWeightVertexBuffer();

		uint32 Influences = FMath::Min(SkinWeightBuffer->GetMaxBoneInfluences(),MaxSupportedInfluences);

		for (const FSkelMeshRenderSection& RenderSection : LODData.RenderSections)
		{
			const TArray<FBoneIndexType> &SectionBoneMap = RenderSection.BoneMap;
			const int32 SectionVertexBufferIndex = RenderSection.GetVertexBufferIndex();
			
			for (int32 SectionVertexIndex = 0; SectionVertexIndex < RenderSection.GetNumVertices(); ++SectionVertexIndex)
			{
				const int32 VertexIndex = SectionVertexIndex + SectionVertexBufferIndex;
				
				FColor BoneIndices;
			
				BoneIndices.R = CPUBoneToGPUBoneIndicesMap[SectionBoneMap[SkinWeightBuffer->GetBoneIndex(VertexIndex, 0)]];
				BoneIndices.G = CPUBoneToGPUBoneIndicesMap[SectionBoneMap[SkinWeightBuffer->GetBoneIndex(VertexIndex, 1)]];
				BoneIndices.B = CPUBoneToGPUBoneIndicesMap[SectionBoneMap[SkinWeightBuffer->GetBoneIndex(VertexIndex, 2)]];
				BoneIndices.A = CPUBoneToGPUBoneIndicesMap[SectionBoneMap[SkinWeightBuffer->GetBoneIndex(VertexIndex, 3)]];

				ColorVertexBuffer.VertexColor(VertexIndex) = BoneIndices;

				//Copy the tangents
				StaticMeshVertexBuffer.SetVertexTangents(VertexIndex, TangentX[VertexIndex],  TangentY[VertexIndex],TangentZ[VertexIndex]);
				//Copy the first uv channel
				StaticMeshVertexBuffer.SetVertexUV(VertexIndex, 0, UV0[VertexIndex]);

				//Weights
				FVector4 Weights;
				Weights.X = SkinWeightBuffer->GetBoneWeight(VertexIndex, 0);
				Weights.Y = SkinWeightBuffer->GetBoneWeight(VertexIndex, 1);
				Weights.Z = SkinWeightBuffer->GetBoneWeight(VertexIndex, 2);
				Weights.W = SkinWeightBuffer->GetBoneWeight(VertexIndex, 3);
				Weights *= UE::AnimationCore::InvMaxRawBoneWeightFloat; 

				float Sum = Weights.X + Weights.Y + Weights.Z + Weights.W;
				//Scale so sum to 1.0
				Weights *= 1.0f / Sum;

				StaticMeshVertexBuffer.SetVertexUV(VertexIndex, UVChannel+0, FVector2f(Weights.X, Weights.Y));
				StaticMeshVertexBuffer.SetVertexUV(VertexIndex, UVChannel+1, FVector2f(Weights.Z, Weights.W));
				
			}
		}
	}
	
	SkeletalMeshCopy->InitResources();
	SkeletalMeshCopy->GetOnMeshChanged().Broadcast();
	
	// Create Temp Actor
	UWorld* World = GEditor->GetEditorWorldContext().World();
	AActor* Actor = World->SpawnActor<AActor>();

	// Create Temp SkeletalMesh Component
	USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(Actor);
	Component->RegisterComponent();
	Component->SetSkeletalMesh(SkeletalMeshCopy);
	
	FString PackageName;

	const UStaticMesh* ExistingMesh = DataAsset->StaticMesh.Get();

	UBodySetup* ExistingBodySetup = nullptr; 
	
	if (IsValid(ExistingMesh))
	{
		PackageName = ExistingMesh->GetPackage()->GetName();
		ExistingBodySetup = ExistingMesh->GetBodySetup();
	}
	else
	{
		const FString AssetName = DataAsset->GetPackage()->GetName();
		const FString SanitizedBasePackageName = UPackageTools::SanitizePackageName(AssetName);
		const FString DataAssetPackageName = FPackageName::GetLongPackagePath(SanitizedBasePackageName) + TEXT("/");
			
		PackageName = DataAssetPackageName + OriginalMesh->GetName().Replace(TEXT("SK_"),TEXT("SM_TS_"));
	}

	//Create a static mesh
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	UStaticMesh* StaticMesh = MeshUtilities.ConvertMeshesToStaticMesh({ Component }, Component->GetComponentToWorld(), PackageName);
	
	if(!IsValid(StaticMesh))
	{
		return;
	}

	if (ExistingBodySetup)
	{
		StaticMesh->SetBodySetup(ExistingBodySetup);
	}

	check(NumLoDs == StaticMesh->GetNumLODs());

	StaticMesh->ImportVersion = LastVersion;
	
	// Reset LODs to the settings of the skeletal mesh
	StaticMesh->bAutoComputeLODScreenSize = false;
	
	// LOD Info for ScreenSize
	for (int32 LODIndex = 0; LODIndex < SkeletalMeshCopy->GetLODNum(); LODIndex++)
	{
		FSkeletalMeshLODInfo* SkeletalMeshLODInfo = SkeletalMeshCopy->GetLODInfo(LODIndex);

		FPerPlatformFloat ScreenSize = SkeletalMeshLODInfo->ScreenSize;
		if (StaticMesh->IsSourceModelValid(LODIndex))
		{
			FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(LODIndex);
			SourceModel.ScreenSize = ScreenSize;
			SourceModel.BuildSettings.bGenerateLightmapUVs = false;
		}
	}
	
	// Apply lod settings
	StaticMesh->Build(false);
	StaticMesh->PostEditChange();
	StaticMesh->MarkPackageDirty();

	DataAsset->StaticMesh = StaticMesh;

	if(!IsValid(ExistingMesh))
	{
		DataAsset->Materials.Empty();
		
		for (const FStaticMaterial& StaticMaterial : StaticMesh->GetStaticMaterials())
		{
			DataAsset->Materials.Add(StaticMaterial.MaterialInterface);
		}
	}
	
	DataAsset->CPUBoneToGPUBoneIndicesMap = CPUBoneToGPUBoneIndicesMap;
	DataAsset->GPUParentCacheRead = UsedBoneParentReadIndex;
	DataAsset->GPUParentCacheWrite = UsedBoneParentWriteIndex;

		
	DataAsset->MarkPackageDirty();
	
	// Destroy Temp Component and Actor
	Component->UnregisterComponent();
	Component->DestroyComponent();
	Actor->Destroy();
}



void FTurboSequence_Editor_LfModule::OnFilesLoaded()
{
	UE_LOG(LogTurboSequence_Lf, Display, TEXT("Initialize Turbo Sequence"));
	const FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<
		FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> GlobalAssetData;
	AssetRegistry.Get().GetAssetsByClass(FTopLevelAssetPath(UTurboSequence_GlobalData_Lf::StaticClass()->GetPathName()),
	                                     GlobalAssetData);
	if (GlobalAssetData.Num())
	{
		FTurboSequence_Helper_Lf::SortAssetsByPathName(GlobalAssetData);
		GlobalData = Cast<UTurboSequence_GlobalData_Lf>(GlobalAssetData[0].GetAsset());

		bool bAssetEdited = false;

		FString EmitterName = FString("");
		FString MeshName = FString("");
		FString MaterialsName = FString("");
		FString PositionName = FString("");
		FString RotationName = FString("");
		FString ScaleName = FString("");
		FString FlagsName = FString("");
		FString CustomDataName = FString("");
		FTurboSequence_Helper_Lf::GetStringConfigSetting(EmitterName,
		                                                 TEXT(
			                                                 "/Script/TurboSequence_Editor_Lf.TurboSequence_NiagaraSettings_Lf"),
		                                                 TEXT("NameNiagaraEmitter"));
		FTurboSequence_Helper_Lf::GetStringConfigSetting(MeshName,
		                                                 TEXT(
			                                                 "/Script/TurboSequence_Editor_Lf.TurboSequence_NiagaraSettings_Lf"),
		                                                 TEXT("NameNiagaraMeshObject"));
		FTurboSequence_Helper_Lf::GetStringConfigSetting(MaterialsName,
		                                                 TEXT(
			                                                 "/Script/TurboSequence_Editor_Lf.TurboSequence_NiagaraSettings_Lf"),
		                                                 TEXT("NameNiagaraMaterialObject"));
		FTurboSequence_Helper_Lf::GetStringConfigSetting(PositionName,
		                                                 TEXT(
			                                                 "/Script/TurboSequence_Editor_Lf.TurboSequence_NiagaraSettings_Lf"),
		                                                 TEXT("NameNiagaraParticleLocations"));
		FTurboSequence_Helper_Lf::GetStringConfigSetting(RotationName,
		                                                 TEXT(
			                                                 "/Script/TurboSequence_Editor_Lf.TurboSequence_NiagaraSettings_Lf"),
		                                                 TEXT("NameNiagaraParticleRotations"));
		FTurboSequence_Helper_Lf::GetStringConfigSetting(ScaleName,
		                                                 TEXT(
			                                                 "/Script/TurboSequence_Editor_Lf.TurboSequence_NiagaraSettings_Lf"),
		                                                 TEXT("NameNiagaraParticleScales"));
		FTurboSequence_Helper_Lf::GetStringConfigSetting(FlagsName,
		                                                 TEXT(
			                                                 "/Script/TurboSequence_Editor_Lf.TurboSequence_NiagaraSettings_Lf"),
		                                                 TEXT("NameNiagaraLevelOfDetailIndex"));
		FTurboSequence_Helper_Lf::GetStringConfigSetting(CustomDataName,
		                                                 TEXT(
			                                                 "/Script/TurboSequence_Editor_Lf.TurboSequence_NiagaraSettings_Lf"),
		                                                 TEXT("NameNiagaraCustomData"));
		if (EmitterName.IsEmpty())
		{
			EmitterName = FTurboSequence_Helper_Lf::NameNiagaraEmitter;
		}
		if (MeshName.IsEmpty())
		{
			MeshName = FTurboSequence_Helper_Lf::NameNiagaraMeshObject;
		}
		if (MaterialsName.IsEmpty())
		{
			MaterialsName = FTurboSequence_Helper_Lf::NameNiagaraMaterialObject;
		}
		if (PositionName.IsEmpty())
		{
			PositionName = FTurboSequence_Helper_Lf::NameNiagaraParticleLocations;
		}
		if (RotationName.IsEmpty())
		{
			RotationName = FTurboSequence_Helper_Lf::NameNiagaraParticleRotations;
		}
		if (ScaleName.IsEmpty())
		{
			ScaleName = FTurboSequence_Helper_Lf::NameNiagaraParticleScales;
		}
		if (FlagsName.IsEmpty())
		{
			FlagsName = FTurboSequence_Helper_Lf::NameNiagaraLevelOfDetailIndex;
		}
		if (CustomDataName.IsEmpty())
		{
			CustomDataName = FTurboSequence_Helper_Lf::NameNiagaraCustomData;
		}

		if (GlobalData->NameNiagaraEmitter.ToString() != EmitterName)
		{
			bAssetEdited = true;
		}
		if (GlobalData->NameNiagaraMeshObject != MeshName)
		{
			bAssetEdited = true;
		}
		if (GlobalData->NameNiagaraMaterialObject != MaterialsName)
		{
			bAssetEdited = true;
		}
		if (GlobalData->NameNiagaraParticleLocations.ToString() != PositionName)
		{
			bAssetEdited = true;
		}
		if (GlobalData->NameNiagaraParticleRotations.ToString() != RotationName)
		{
			bAssetEdited = true;
		}
		if (GlobalData->NameNiagaraParticleScales.ToString() != ScaleName)
		{
			bAssetEdited = true;
		}
		if (GlobalData->NameNiagaraFlags.ToString() != FlagsName)
		{
			bAssetEdited = true;
		}
		if (GlobalData->NameNiagaraCustomData.ToString() != CustomDataName)
		{
			bAssetEdited = true;
		}

		// Makes no sense at all
		if (GlobalData->bUseHighPrecisionAnimationMode != false)
		{
			GlobalData->bUseHighPrecisionAnimationMode = false;
			bAssetEdited = true;
		}

		GlobalData->NameNiagaraEmitter = FName(EmitterName);
		GlobalData->NameNiagaraMeshObject = MeshName;
		GlobalData->NameNiagaraMaterialObject = MaterialsName;
		GlobalData->NameNiagaraParticleLocations = FName(PositionName);
		GlobalData->NameNiagaraParticleRotations = FName(RotationName);
		GlobalData->NameNiagaraParticleScales = FName(ScaleName);
		GlobalData->NameNiagaraFlags = FName(FlagsName);
		GlobalData->NameNiagaraCustomData = FName(CustomDataName);


		FString DefaultTransformTextureReferencePathCurrentFrame;
		FTurboSequence_Helper_Lf::GetStringConfigSetting(DefaultTransformTextureReferencePathCurrentFrame,
		                                                 TEXT(
			                                                 "/Script/TurboSequence_Editor_Lf.TurboSequence_RefSettings_Lf"),
		                                                 TEXT("Default_Rendering_TransformTexture_CurrentFrame"));
		if (DefaultTransformTextureReferencePathCurrentFrame.IsEmpty())
		{
			DefaultTransformTextureReferencePathCurrentFrame =
				FTurboSequence_Helper_Lf::ReferenceTurboSequenceTransformTextureCurrentFrame;
		}
		if (!IsValid(GlobalData->TransformTexture_CurrentFrame))
		{
			GlobalData->TransformTexture_CurrentFrame = FTurboSequence_Helper_Lf::LoadAssetFromReferencePath<
				UTextureRenderTarget2DArray>(DefaultTransformTextureReferencePathCurrentFrame);
			bAssetEdited = true;
		}
		if (!IsValid(GlobalData->TransformTexture_CurrentFrame))
		{
			UE_LOG(LogTurboSequence_Lf, Error,
			       TEXT(
				       "Can not find Transform Texture, it should at .../Plugins/TurboSequence_Lf/Resources/T_TurboSequence_TransformTexture_Lf, please assign it manually in the Project settings under TurboSequence Lf -> Reference Paths, if it's not there please create a default Render Target 2D Array Texture and assign the reference in the TurboSequence Lf -> Reference Paths Project settings and open ../Plugins/TurboSequence_Lf/Resources/MF_TurboSequence_PositionOffset_Lf and assign it into the Texture Object with the Transform Texture Comment"
			       ));
		}
		else
		{
			bool bUse32BitTexture = GlobalData->bUseHighPrecisionAnimationMode;

			ETextureRenderTargetFormat Format = RTF_RGBA16f;
			if (bUse32BitTexture)
			{
				Format = RTF_RGBA32f;
			}

			if (GlobalData->TransformTexture_CurrentFrame->GetFormat() != GetPixelFormatFromRenderTargetFormat(Format))
			{
				bAssetEdited = true;
				const UPackage* Package = GlobalData->TransformTexture_CurrentFrame->GetOutermost();
				const FString PackagePath = FPackageName::LongPackageNameToFilename(
					Package->GetName(), FPackageName::GetAssetPackageExtension());
				UPackageTools::LoadPackage(*PackagePath);
				GlobalData->TransformTexture_CurrentFrame = FTurboSequence_Helper_Lf::GenerateBlankRenderTargetArray(
					PackagePath, GlobalData->TransformTexture_CurrentFrame->GetName(), 2048, 12,
					GetPixelFormatFromRenderTargetFormat(Format));
			}
		}

		FString DefaultTransformTextureReferencePathPreviousFrame;
		FTurboSequence_Helper_Lf::GetStringConfigSetting(DefaultTransformTextureReferencePathPreviousFrame,
		                                                 TEXT(
			                                                 "/Script/TurboSequence_Editor_Lf.TurboSequence_RefSettings_Lf"),
		                                                 TEXT("Default_Rendering_TransformTexture_PreviousFrame"));
		if (DefaultTransformTextureReferencePathPreviousFrame.IsEmpty())
		{
			DefaultTransformTextureReferencePathPreviousFrame =
				FTurboSequence_Helper_Lf::ReferenceTurboSequenceTransformTexturePreviousFrame;
		}
		if (!IsValid(GlobalData->TransformTexture_PreviousFrame))
		{
			GlobalData->TransformTexture_PreviousFrame = FTurboSequence_Helper_Lf::LoadAssetFromReferencePath<
				UTextureRenderTarget2DArray>(DefaultTransformTextureReferencePathPreviousFrame);
			bAssetEdited = true;
		}

		if (!IsValid(GlobalData->TransformTexture_PreviousFrame))
		{
			UE_LOG(LogTurboSequence_Lf, Error,
			       TEXT(
				       "Can not find Transform Texture, it should at .../Plugins/TurboSequence_Lf/Resources/T_TurboSequence_TransformTexture_Lf, please assign it manually in the Project settings under TurboSequence Lf -> Reference Paths, if it's not there please create a default Render Target 2D Array Texture and assign the reference in the TurboSequence Lf -> Reference Paths Project settings and open ../Plugins/TurboSequence_Lf/Resources/MF_TurboSequence_PositionOffset_Lf and assign it into the Texture Object with the Transform Texture Comment"
			       ));
		}
		else
		{
			if (GlobalData->TransformTexture_CurrentFrame->SizeX != GlobalData->TransformTexture_PreviousFrame->SizeX ||
				GlobalData->TransformTexture_CurrentFrame->SizeY
				!= GlobalData->TransformTexture_PreviousFrame->SizeY || GlobalData->TransformTexture_PreviousFrame->
				Slices != GlobalData->TransformTexture_CurrentFrame->Slices || GlobalData->
				                                                               TransformTexture_PreviousFrame->
				                                                               GetFormat() != GlobalData->
				TransformTexture_CurrentFrame->GetFormat())
			{
				bAssetEdited = true;
				const UPackage* Package = GlobalData->TransformTexture_PreviousFrame->GetOutermost();
				const FString PackagePath = FPackageName::LongPackageNameToFilename(
					Package->GetName(), FPackageName::GetAssetPackageExtension());
				UPackageTools::LoadPackage(*PackagePath);
				GlobalData->TransformTexture_PreviousFrame = FTurboSequence_Helper_Lf::GenerateBlankRenderTargetArray(
					PackagePath, GlobalData->TransformTexture_PreviousFrame->GetName(),
					GlobalData->TransformTexture_CurrentFrame->SizeX, GlobalData->TransformTexture_CurrentFrame->Slices,
					GlobalData->TransformTexture_CurrentFrame->GetFormat());
			}
		}
		

		FString DefaultDataTextureReferencePath;
		FTurboSequence_Helper_Lf::GetStringConfigSetting(DefaultDataTextureReferencePath,
		                                                 TEXT(
			                                                 "/Script/TurboSequence_Editor_Lf.TurboSequence_RefSettings_Lf"),
		                                                 TEXT("Default_Rendering_DataTexture"));
		if (DefaultDataTextureReferencePath.IsEmpty())
		{
			DefaultDataTextureReferencePath = FTurboSequence_Helper_Lf::ReferenceTurboSequenceDataTexture;
		}

		if (bAssetEdited)
		{
			FTurboSequence_Helper_Lf::SaveAsset(GlobalData);
		}
	}
	else
	{
		UE_LOG(LogTurboSequence_Lf, Error,
		       TEXT(
			       "Can not find the Global Data asset -> This is really bad, without it Turbo Sequence does not work, you can recover it by creating an UTurboSequence_GlobalData_Lf Data Asset, Right click in the content browser anywhere in the Project, select Data Asset and choose UTurboSequence_GlobalData_Lf, save it and restart the editor"
		       ));
		return;
	}


	AssetRegistry.Get().GetAssetsByClass(FTopLevelAssetPath(UTurboSequence_MeshAsset_Lf::StaticClass()->GetPathName()),
	                                     TurboSequence_MeshAssetData_AsyncComputeSwapBack);


	if (TurboSequence_MeshAssetData_AsyncComputeSwapBack.Num())
	{
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTurboSequence_Editor_LfModule, TurboSequence_Editor_Lf)
