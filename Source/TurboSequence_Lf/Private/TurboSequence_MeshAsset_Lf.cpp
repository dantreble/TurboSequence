// Copyright Lukas Fratzl, 2022-2024. All Rights Reserved.


#include "TurboSequence_MeshAsset_Lf.h"

#include "NiagaraMeshVertexFactory.h"
#include "TurboSequence_Helper_Lf.h"
#include "Engine/SkeletalMeshSocket.h"


UTurboSequence_MeshAsset_Lf::UTurboSequence_MeshAsset_Lf()
{
}

bool UTurboSequence_MeshAsset_Lf::IsMeshAssetValid() const
{
	if (!IsValid(this))
	{
		UE_LOG(LogTurboSequence_Lf, Warning,
		       TEXT("Can't create Mesh Instance, the Asset you use is not valid...."));
		return false;
	}

	if (!IsValid(GetSkeleton()))
	{
		UE_LOG(LogTurboSequence_Lf, Warning,
		       TEXT("Can't create Mesh Instance, the Asset you use has no Skeleton assigned...."));
		return false;
	}

	if (!StaticMesh)
	{
		UE_LOG(LogTurboSequence_Lf, Warning, TEXT("Can't create Mesh Instance, the Asset you use has no LODs...."));
		return false;
	}

	if (bExcludeFromSystem)
	{
		UE_LOG(LogTurboSequence_Lf, Warning,
		       TEXT(
			       "Can't create Mesh Instance, the Asset you use is excluded from the system because bExcludeFromSystem is true"
		       ));
		return false;
	}

	return true;
}

TArray<FString> UTurboSequence_MeshAsset_Lf::GetSocketNames() const
{
	TArray<FString> SocketNames;

	if(const USkeletalMesh* SkeletalMesh = ReferenceMeshNative.Get())
	{
		const int32 NumSockets = SkeletalMesh->NumSockets();

		for(int32 SocketIndex = 0; SocketIndex < NumSockets; ++SocketIndex)
		{
			if(const USkeletalMeshSocket* SkeletalMeshSocket = SkeletalMesh->GetSocketByIndex(SocketIndex))
			{
				SocketNames.Add(SkeletalMeshSocket->SocketName.ToString()); 
			}
		}
	}

	// Get skeletal mesh, populate list of socket names
	return SocketNames;
}

void UTurboSequence_MeshAsset_Lf::PrecachePSOs()
{
	if (((IsComponentPSOPrecachingEnabled() && RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))) )
	{
		
		FPSOPrecacheParams PrecachePSOParams;
		
		PrecachePSOParams.bRenderInMainPass = true;
		PrecachePSOParams.bRenderInDepthPass = true;
		PrecachePSOParams.bStaticLighting = false;
		//PrecachePSOParams.bUsesIndirectLightingCache = PrecachePSOParams.bStaticLighting && IndirectLightingCacheQuality != ILCQ_Off && (!IsPrecomputedLightingValid() || GetLightmapType() == ELightmapType::ForceVolumetric);
		//PrecachePSOParams.bAffectDynamicIndirectLighting = bAffectDynamicIndirectLighting;
		PrecachePSOParams.bCastShadow = true;
		// Custom depth can be toggled at runtime with PSO precache call so assume it might be needed when depth pass is needed
		// Ideally precache those with lower priority and don't wait on these (UE-174426)
		PrecachePSOParams.bRenderCustomDepth = false;
		PrecachePSOParams.bCastShadowAsTwoSided = false;
		PrecachePSOParams.SetMobility(EComponentMobility::Movable);	
		//PrecachePSOParams.SetStencilWriteMask(FRendererStencilMaskEvaluation::ToStencilMask(CustomDepthStencilWriteMask));
		PrecachePSOParams.bAnyMaterialHasWorldPositionOffset = true;
		
		const FVertexFactoryType* VFType = &FNiagaraMeshVertexFactory::StaticType;
		FPSOPrecacheVertexFactoryDataList VFDataList;
		VFDataList.Add(FPSOPrecacheVertexFactoryData(VFType));

		for (const auto & Material : Materials)
		{
			Material->PrecachePSOs(VFType, PrecachePSOParams);
		}
	}
	
}

void UTurboSequence_MeshAsset_Lf::PostLoad()
{
	PrecachePSOs();
	
	Super::PostLoad();
}
