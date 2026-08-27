#include "../idlib/precompiled.h"
#pragma hdrstop

#include "virtualtexture.h"
#include "Material.h"
#include "../framework/File.h"

namespace {

bool VmtrMetadataLooksSane( const char * path ) {
	idFile * file = fileSystem->OpenExplicitFileRead( path );
	if ( file == NULL ) {
		return false;
	}
	const int64 length64 = file->Length64();
	if ( length64 <= 0 || length64 > 16 * 1024 * 1024 ) {
		delete file;
		return false;
	}
	idList< byte > bytes;
	bytes.SetNum( static_cast< int >( length64 ) );
	const bool readOK = file->Read( bytes.Ptr(), bytes.Num() ) == bytes.Num();
	delete file;
	if ( !readOK ) {
		return false;
	}

	// The writer emits short, printable ASCII records.  Reject stale or
	// interrupted background output before feeding it to idLexer; debug-heap
	// poison in a dangling material name can otherwise become one enormous
	// quoted token and terminate the load instead of triggering a rebuild.
	int lineLength = 0;
	for ( int i = 0; i < bytes.Num(); ++i ) {
		const byte value = bytes[i];
		if ( value == '\r' || value == '\n' ) {
			lineLength = 0;
			continue;
		}
		if ( value < 0x20 || value > 0x7e || ++lineLength > 4096 ) {
			return false;
		}
	}
	return true;
}

}

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
	if ( !VmtrMetadataLooksSane( vmtrHeaderPath.c_str() ) ) {
		idLib::Warning( "invalid VMTR metadata '%s'; rebuilding it", vmtrHeaderPath.c_str() );
		return;
	}

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
		if ( source.HadError() || !source.ReadToken( &token ) || token.Length() == 0 ||
				vmtr->allocX < 0 || vmtr->allocY < 0 || vmtr->allocWidth <= 0 ||
				vmtr->allocHeight <= 0 || vmtr->allocX + vmtr->allocWidth > vt->VIRTUAL_TEXELS_WIDE ||
				vmtr->allocY + vmtr->allocHeight > vt->VIRTUAL_TEXELS_WIDE ) {
			delete vmtr;
			break;
		}
		vmtr->materialName = token.c_str();
		// VMTR metadata is attached by EndLevelLoad, which may execute on the
		// game thread.  Do not resolve declarations here.  Tech 5 leaves this
		// pointer empty and lazily associates the already-resolved material passed
		// by the render backend in VirtualMaterialForMaterial().
		vmtr->material = NULL;
		SetupVirtualMaterial( vmtr );
		vmtrHash.Add( idStr::Hash( vmtr->materialName.c_str() ), vmtrs.Num() );
		vmtrs.Append( vmtr );
	}
	if ( source.HadError() || vmtrs.Num() != count ) {
		for ( int i = 0; i < vmtrs.Num(); ++i ) {
			delete vmtrs[i];
		}
		vmtrs.Clear();
		vmtrHash.Clear();
		idLib::Warning( "incomplete VMTR metadata '%s'; rebuilding it", vmtrHeaderPath.c_str() );
		return;
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
	if ( material == NULL ) {
		return NULL;
	}

	const char * materialName = material->GetName();
	const int hashKey = idStr::Hash( materialName );
	for ( int index = vmtrHash.First( hashKey ); index != -1;
			index = vmtrHash.Next( index ) ) {
		idVirtualMaterial * vmtr = vmtrs[index];
		if ( vmtr->material == material || vmtr->materialName.Icmp( materialName ) == 0 ) {
			vmtr->material = material;
			return vmtr;
		}
	}
	return NULL;
}

idBackgroundVmtrUpdater::idBackgroundVmtrUpdater( idVMTRData * data )
	: vmtrData( data ), isDoingWork( false ) {}
