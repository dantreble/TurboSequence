// Copyright Lukas Fratzl, 2022-2024. All Rights Reserved.


#include "TurboSequence_RenderData.h"

#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "TurboSequence_Data_Lf.h"
#include "TurboSequence_GlobalData_Lf.h"
#include "TurboSequence_MeshAsset_Lf.h"
#include "TurboSequence_MinimalData_Lf.h"

FName& UTurboSequence_RenderData::GetEmitterName() const
{	
	return DataAsset->GlobalData->NameNiagaraEmitter;
}

FName& UTurboSequence_RenderData::GetPositionName() const
{
	return DataAsset->GlobalData->NameNiagaraParticleLocations;
}

FName& UTurboSequence_RenderData::GetRotationName() const
{
	return DataAsset->GlobalData->NameNiagaraParticleRotations;
}

FName& UTurboSequence_RenderData::GetScaleName() const
{
	return DataAsset->GlobalData->NameNiagaraParticleScales;
}

FString& UTurboSequence_RenderData::GetMeshName() const
{
	return DataAsset->GlobalData->NameNiagaraMeshObject;
}

FString& UTurboSequence_RenderData::GetMaterialsName() const
{
	return DataAsset->GlobalData->NameNiagaraMaterialObject;
}

FName& UTurboSequence_RenderData::GetFlagsName() const
{
	return DataAsset->GlobalData->NameNiagaraFlags;
}

FName& UTurboSequence_RenderData::GetCustomDataName() const
{
	return DataAsset->GlobalData->NameNiagaraCustomData;
}


void UTurboSequence_RenderData::PrintRenderData() const
{
	FBox RenderBounds = GetRenderBounds();

	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, FString::Printf(TEXT("Active Count %d"),GetActiveNum()));
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, FString::Printf(TEXT("Count %d, Position %d, Rotation %d, Scale %d, Flags %d, Custom %d"),bChangedCollectionSizeThisFrame, bChangedPositionCollectionThisFrame, bChangedRotationCollectionThisFrame, bChangedScaleCollectionThisFrame, bChangedFlagsCollectionThisFrame,bChangedCustomDataCollectionThisFrame));
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, FString::Printf(TEXT("Bounds Min %s Max %s"),*RenderBounds.Min.ToString(), *RenderBounds.Max.ToString()));
}

void UTurboSequence_RenderData::ResetBounds()
{
	MinBounds = FVector(TNumericLimits<double>::Max());
	MaxBounds = FVector(TNumericLimits<double>::Lowest());
	MaxScale = 0;
}

FBox UTurboSequence_RenderData::GetRenderBounds() const
{
	return FBox(MinBounds + MeshMinBounds * MaxScale, MaxBounds + MeshMaxBounds * MaxScale);
}

void UTurboSequence_RenderData::UpdateNiagaraBounds() const
{
	SetEmitterBounds(GetRenderBounds());
}

void UTurboSequence_RenderData::UpdateNiagaraEmitter() 
{
	if (bChangedFlagsCollectionThisFrame || bChangedCollectionSizeThisFrame)
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayUInt8(
			NiagaraComponent, GetFlagsName(), ParticleFlags);
	}

	if (bChangedCustomDataCollectionThisFrame || bChangedCollectionSizeThisFrame)
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
			NiagaraComponent, GetCustomDataName(), ParticleCustomData);
	}

	if (bChangedPositionCollectionThisFrame || bChangedCollectionSizeThisFrame)
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(
			NiagaraComponent, GetPositionName(), ParticlePositions);
	}

	if (bChangedRotationCollectionThisFrame || bChangedCollectionSizeThisFrame)
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(
			NiagaraComponent, GetRotationName(), ParticleRotations);
	}

	if (bChangedScaleCollectionThisFrame || bChangedCollectionSizeThisFrame)
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
			NiagaraComponent, GetScaleName(), ParticleScales);
	}

	if (bChangedSkeletonIndexesThisFrame || bChangedCollectionSizeThisFrame)
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
			NiagaraComponent, TEXT("User.Particle_SkeletonIndex"), SkeletonIndexes);
	}
		
	bChangedFlagsCollectionThisFrame = false;
	bChangedCustomDataCollectionThisFrame = false;
	bChangedPositionCollectionThisFrame = false;
	bChangedRotationCollectionThisFrame = false;
	bChangedScaleCollectionThisFrame = false;
	bChangedSkeletonIndexesThisFrame = false;

	bChangedCollectionSizeThisFrame = false;
	
	UpdateNiagaraBounds();

}

void UTurboSequence_RenderData::SetEmitterBounds(const FBox& RendererBounds) const
{
	NiagaraComponent->SetEmitterFixedBounds(GetEmitterName(), RendererBounds);
}

void UTurboSequence_RenderData::SpawnNiagaraComponent(const TArray<UMaterialInterface*>& OverrideMaterials,
                                                      USceneComponent* AttachToComponent,
                                                      UNiagaraSystem* RendererSystem, UStaticMesh* StaticMesh,
                                                      bool bNewReceivesDecals, FLightingChannels LightingChannels, bool bInRenderInCustomDepth, int32 InStencilValue)
{

	NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(AttachToComponent->GetWorld(), RendererSystem, FVector::ZeroVector);

	NiagaraComponent->SetReceivesDecals(bNewReceivesDecals);
	
	// Add the mesh to the component we just created
	const FName& WantedMeshName = FName(FString::Format(
		*GetMeshName(),
		{*FString::FormatAsNumber(0)}));
	NiagaraComponent->SetVariableStaticMesh(
		WantedMeshName, StaticMesh);
	
	const int32 MaterialCount = StaticMesh->GetStaticMaterials().Num();
			
	for(int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		const FName& WantedMaterialName = FName(FString::Format(
			*GetMaterialsName(), {*FString::FormatAsNumber(MaterialIndex)}));

		UMaterialInterface* Material = OverrideMaterials.IsValidIndex(MaterialIndex) ? OverrideMaterials[MaterialIndex] : StaticMesh->GetMaterial(MaterialIndex);
		NiagaraComponent->SetVariableMaterial(WantedMaterialName, Material);
	}
	
	//Set Bound
	//Basically 5000-Meter Radius
	constexpr float BoundsExtend = 1000 * 1000 / 2;
	const FBox& Bounds = FBox(FVector::OneVector * -BoundsExtend, FVector::OneVector * BoundsExtend);
	
	SetEmitterBounds(Bounds);

	if(bInRenderInCustomDepth)
	{
		NiagaraComponent->SetRenderCustomDepth(bInRenderInCustomDepth);
		NiagaraComponent->SetCustomDepthStencilValue(InStencilValue);
	}

	NiagaraComponent->LightingChannels = LightingChannels;
}

void UTurboSequence_RenderData::DestroyNiagaraComponent()
{
	if(NiagaraComponent)
	{
		NiagaraComponent->DestroyComponent();

		NiagaraComponent = nullptr;
	}
}


void UTurboSequence_RenderData::UpdateInstanceTransformInternal(const int32 InstanceIndex, const FTransform& WorldSpaceTransform, bool bForceUpdate)
{
	const FVector& Position = WorldSpaceTransform.GetLocation();
	if (bForceUpdate || !Position.Equals(ParticlePositions[InstanceIndex]))
	{
		bChangedPositionCollectionThisFrame = true;
		ParticlePositions[InstanceIndex] = Position;
	}

	const FVector4f& Rotation = FTurboSequence_Helper_Lf::ConvertQuaternionToVector4F(
		WorldSpaceTransform.GetRotation());
	if (bForceUpdate || !Rotation.Equals(ParticleRotations[InstanceIndex]))
	{
		bChangedRotationCollectionThisFrame = true;
		ParticleRotations[InstanceIndex] = Rotation;
	}
	
	const FVector3f& Scale = FTurboSequence_Helper_Lf::ConvertVectorToVector3F(WorldSpaceTransform.GetScale3D());
	if (bForceUpdate || !Scale.Equals(ParticleScales[InstanceIndex]))
	{
		bChangedScaleCollectionThisFrame = true;
		ParticleScales[InstanceIndex] = Scale;
	}
}

void UTurboSequence_RenderData::UpdateInstanceTransform(
	FAttachmentMeshHandle AttachmentMeshHandle,
	const FTransform& WorldSpaceTransform)
{
	if(const int* InstanceIndexPtr = InstanceMap.Find(AttachmentMeshHandle))
	{
		UpdateInstanceTransformInternal(*InstanceIndexPtr, WorldSpaceTransform);
	}
}

int32 UTurboSequence_RenderData::AllocateRenderInstanceInternal(const FAttachmentMeshHandle MeshHandle, bool& bReuse)
{
	bReuse = FreeList.Num() > 0;

	const int32 InstanceIndex = bReuse ? FreeList.Pop() : InstanceMap.Num();

	InstanceMap.Add(MeshHandle, InstanceIndex);

	const FParticleFlags AliveFlags(true, 0);
		
	if(bReuse)
	{
		ParticleFlags[InstanceIndex] = AliveFlags.Val;

		bChangedFlagsCollectionThisFrame = true;
		
		for(int i = 0; i < FTurboSequence_Helper_Lf::NumInstanceCustomData; ++i)
		{
			ParticleCustomData[ InstanceIndex * FTurboSequence_Helper_Lf::NumInstanceCustomData + i] = float();
		}
	}
	else
	{
		ParticlePositions.AddDefaulted(1);
		ParticleRotations.AddDefaulted(1);
		ParticleScales.AddDefaulted(1);
		ParticleFlags.Add(AliveFlags.Val);
		SkeletonIndexes.AddDefaulted(1);
		ParticleCustomData.AddDefaulted(FTurboSequence_Helper_Lf::NumInstanceCustomData);

		bChangedCollectionSizeThisFrame = true;
	}

	return InstanceIndex;
}

void UTurboSequence_RenderData::SetSkeletonIndexInternal(const int32 InstanceIndex, const int32 SkeletonIndex)
{
	if(SkeletonIndexes[InstanceIndex] != SkeletonIndex)
	{
		SkeletonIndexes[InstanceIndex] = SkeletonIndex;
		bChangedSkeletonIndexesThisFrame = true;
	}
}

void UTurboSequence_RenderData::AddRenderInstance(const FAttachmentMeshHandle MeshHandle,
                                       const FTransform& WorldSpaceTransform, const int32 SkeletonIndex)
{
	bool bReuse;
	const int32 InstanceIndex = AllocateRenderInstanceInternal(MeshHandle, bReuse);

	UpdateInstanceTransformInternal(InstanceIndex, WorldSpaceTransform, bReuse);

	SetSkeletonIndexInternal(InstanceIndex, SkeletonIndex);
}


void UTurboSequence_RenderData::RemoveRenderInstance(
	const FAttachmentMeshHandle Handle)
{
	const FParticleFlags DeadFlags(false, 0);

	int32 InstanceIndex;
	if(InstanceMap.RemoveAndCopyValue(Handle,InstanceIndex))
	{
		FreeList.Add(InstanceIndex);
		ParticleFlags[InstanceIndex] = DeadFlags.Val;
		bChangedFlagsCollectionThisFrame = true;	
	}
}

void UTurboSequence_RenderData::SetGPUBoneIndexForInstanceInternal(const uint8 BoneIndex, const int Index)
{
	FParticleFlags Flags;

	Flags.Val = ParticleFlags[Index];

	if(Flags.BoneIndex != BoneIndex)
	{
		Flags.BoneIndex = BoneIndex;
		bChangedFlagsCollectionThisFrame = true;

		ParticleFlags[Index] = Flags.Val;
	}
}

void UTurboSequence_RenderData::Init(UTurboSequence_MeshAsset_Lf* InMeshAsset, UTurboSequence_RenderData* TurboSequenceRenderData, const FVector&
                                     MeshMinBounds, const FVector& MaxMaxBounds)
{
	TurboSequenceRenderData->DataAsset = InMeshAsset;

	TurboSequenceRenderData->MeshMinBounds = MeshMinBounds;
	TurboSequenceRenderData->MeshMaxBounds = MaxMaxBounds;
}

UTurboSequence_RenderData* UTurboSequence_RenderData::CreateObject(UObject* Outer,
                                                                 UTurboSequence_MeshAsset_Lf* InMeshAsset, const UStaticMesh* StaticMesh)
{
	UTurboSequence_RenderData* TurboSequenceRenderData = NewObject<UTurboSequence_RenderData>(Outer);

	Init(InMeshAsset, TurboSequenceRenderData, StaticMesh->GetBounds().GetBoxExtrema(false),StaticMesh->GetBounds().GetBoxExtrema(true));
	
	return TurboSequenceRenderData;
}

bool UTurboSequence_RenderData::GetInstanceCustomDataBaseIndex(const FAttachmentMeshHandle MeshHandle, const int32 CustomDataIndex, int32& InstanceBaseIndex)
{
	constexpr int32 ReservedCustomDataNum = 1;
	
	if (CustomDataIndex < ReservedCustomDataNum || (CustomDataIndex - ReservedCustomDataNum) >
		FTurboSequence_Helper_Lf::NumInstanceCustomData)
	{
		return false;
	}
	
	const int* InstanceIndexPtr = InstanceMap.Find(MeshHandle);

	ensure(InstanceIndexPtr); //Shouldn't be updating is not in the set
	
	if(InstanceIndexPtr == nullptr)
	{
		return false;
	}

	const int32 InstanceIndex = *InstanceIndexPtr;

	InstanceBaseIndex = (InstanceIndex * FTurboSequence_Helper_Lf::NumInstanceCustomData) - ReservedCustomDataNum;
	return true;
}

void UTurboSequence_RenderData::SetCustomDataIndex(const float CustomDataValue, const uint32 CustomDataIndex)
{
	if (!bChangedCustomDataCollectionThisFrame && ParticleCustomData[CustomDataIndex] != CustomDataValue)
	{
		bChangedCustomDataCollectionThisFrame = true;
	}
	
	ParticleCustomData[CustomDataIndex] = CustomDataValue;
}

bool UTurboSequence_RenderData::SetCustomDataForInstance(
	const FAttachmentMeshHandle MeshHandle, const int32 Index, const float Value)
{
	int32 InstanceBaseIndex;
	if (!GetInstanceCustomDataBaseIndex(MeshHandle, Index, InstanceBaseIndex))
	{
		return false;
	}

	const uint32 CustomDataIndex = InstanceBaseIndex + Index;
	
	SetCustomDataIndex(Value, CustomDataIndex);

	return true;
}

bool UTurboSequence_RenderData::SetCustomDataArrayForInstance(const FAttachmentMeshHandle MeshHandle, const int32 StartIndex,
                                                              const TArray<float>& CustomDataValues)
{
	int32 InstanceBaseIndex;
	if (!GetInstanceCustomDataBaseIndex(MeshHandle, StartIndex, InstanceBaseIndex))
	{
		return false;
	}

	const uint32 CustomDataIndex = InstanceBaseIndex + StartIndex;

	const int32 NumValuesToCopy= FMath::Min(CustomDataValues.Num(), FTurboSequence_Helper_Lf::NumInstanceCustomData);

	for(int32 ArrayIndex = 0; ArrayIndex < NumValuesToCopy; ++ ArrayIndex)
	{
		SetCustomDataIndex(CustomDataValues[ArrayIndex], CustomDataIndex+ArrayIndex);
	}

	return true;
}

void UTurboSequence_RenderData::UpdateRendererBounds(
	const FTransform& WorldSpaceTransform)
{
	// We assume it's called already inside bIsVisible
	const FVector& MeshLocation = WorldSpaceTransform.GetLocation();
	const float Scale = WorldSpaceTransform.GetScale3D().GetMax();

	MinBounds = MinBounds.ComponentMin(MeshLocation);
	MaxBounds = MaxBounds.ComponentMax(MeshLocation);
	MaxScale = FMath::Max(MaxScale, Scale);
}
