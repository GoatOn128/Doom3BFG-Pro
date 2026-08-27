#include "../idlib/precompiled.h"
#pragma hdrstop

#include "materialmap.h"

idCVar r_writePackedMaterials( "r_writePackedMaterials", "0", CVAR_RENDERER | CVAR_BOOL,
	"write packed virtual materials as viewable images" );

idMaterialMap::idMaterialMap() : width( 0 ), height( 0 ), skinFileTimeStamp( 0 ) {}

bool idMaterialMap::GetImageDimensions( int & imageWidth, int & imageHeight ) const {
	imageWidth = width;
	imageHeight = height;
	return width > 0 && height > 0;
}

bool idVirtualImageFile::NeedsReload() {
	ID_TIME_T current = 0;
	if ( fileSystem->ReadFile( fileName.c_str(), NULL, &current ) < 0 ) {
		return false;
	}
	return timeStamp == 0 || current > static_cast< ID_TIME_T >( timeStamp );
}
