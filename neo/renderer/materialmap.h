#pragma once

struct idVirtualImageFile {
	idStr		fileName;
	uint32		timeStamp;
	bool		NeedsReload();
};

struct idMaterialMapping {
	idStr		materialName;
	uint32		materialChecksum;
	uint32		materialTimestamp;
	int			x;
	int			y;
	int			width;
	int			height;
	idVec4		scaleBias;
	idVirtualImageFile specularFile;
	idVirtualImageFile coverFile;
	idVirtualImageFile diffuseFile;
	idVirtualImageFile bumpFile;
	idVirtualImageFile powerFile;
};

class idMaterialMap {
public:
					idMaterialMap();
	bool			GetImageDimensions( int & imageWidth, int & imageHeight ) const;

	idStr			name;
	int				width;
	int				height;
	uint32			skinFileTimeStamp;
	idList< idMaterialMapping > mappings;
};

extern idCVar r_writePackedMaterials;

