#include "../idlib/precompiled.h"
#pragma hdrstop

#include "virtualtexture.h"
#include "Material.h"

idVMTRData::idVMTRData( idVirtualTexture * virtualTexture )
	: vt( virtualTexture ), vmtrHash( 1024, 1024 ), backgroundUpdater( NULL ) {}

idVMTRData::~idVMTRData() {
	for ( int i = 0; i < vmtrs.Num(); ++i ) {
		delete vmtrs[i];
	}
	vmtrs.Clear();
	delete backgroundUpdater;
}

void idVMTRData::ReadVmtrFile() {
	for ( int i = 0; i < vmtrs.Num(); ++i ) {
		delete vmtrs[i];
	}
	vmtrs.Clear();
	vmtrHash.Clear();

	idLexer source( LEXFL_NOFATALERRORS | LEXFL_NOSTRINGCONCAT |
		LEXFL_NOSTRINGESCAPECHARS | LEXFL_ALLOWPATHNAMES );
	if ( !source.LoadFile( vmtrHeaderPath.c_str(), true ) ) {
		idLib::Warning( "could not read VMTR metadata '%s'", vmtrHeaderPath.c_str() );
		return;
	}
	if ( source.ParseInt() != VMTR_FILE_VERSION ) {
		idLib::Warning( "bad VMTR version in '%s'", vmtrHeaderPath.c_str() );
		return;
	}

	const int count = source.ParseInt();
	if ( count < 0 || count > 1024 * 1024 ) {
		idLib::Warning( "invalid VMTR count %i in '%s'", count, vmtrHeaderPath.c_str() );
		return;
	}
	for ( int index = 0; index < count; ++index ) {
		idToken token;
		idVirtualMaterial * vmtr = new ( TAG_RENDER ) idVirtualMaterial;
		vmtr->allocX = source.ParseInt();
		vmtr->allocY = source.ParseInt();
		vmtr->allocWidth = source.ParseInt();
		vmtr->allocHeight = source.ParseInt();
		vmtr->flags = source.ParseInt();
		vmtr->cpuVmtrTimeStamp = source.ParseInt();
		vmtr->materialChecksum = static_cast< uint32 >( source.ParseInt() );
		if ( !source.ReadToken( &token ) ) {
			delete vmtr;
			idLib::Warning( "VMTR metadata ended after %i of %i records", index, count );
			break;
		}
		vmtr->materialName = token.c_str();
		vmtr->material = declManager->FindMaterial( vmtr->materialName.c_str(), false );
		SetupVirtualMaterial( vmtr );
		vmtrHash.Add( idStr::Hash( vmtr->materialName.c_str() ), vmtrs.Num() );
		vmtrs.Append( vmtr );
	}
	idLib::Printf( "read %i VMTR allocations from '%s'\n", vmtrs.Num(), vmtrHeaderPath.c_str() );
}

void idVMTRData::SetupVirtualMaterial( idVirtualMaterial * vmtr ) {
	if ( vmtr == NULL || vt == NULL || vt->VIRTUAL_TEXELS_WIDE <= 0 ) {
		return;
	}
	const float invWidth = 1.0f / static_cast< float >( vt->VIRTUAL_TEXELS_WIDE );
	vmtr->scaleAndBias.Set( vmtr->allocWidth * invWidth, vmtr->allocHeight * invWidth,
		vmtr->allocX * invWidth, vmtr->allocY * invWidth );
	vmtr->vmtrData = this;
}

idVirtualMaterial * idVMTRData::VirtualMaterialForMaterial( const idMaterial * material ) {
	for ( int i = 0; i < vmtrs.Num(); ++i ) {
		if ( vmtrs[i]->material == material ) {
			return vmtrs[i];
		}
	}
	return NULL;
}

idBackgroundVmtrUpdater::idBackgroundVmtrUpdater( idVMTRData * data )
	: vmtrData( data ), isDoingWork( false ) {}
