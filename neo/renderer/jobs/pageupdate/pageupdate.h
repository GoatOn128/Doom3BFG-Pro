/*
===========================================================================

TNO style page update scheduling for the virtual texture runtime.

Semantics follow the decompiled reference under references/wolftno, which
is GPL incompatible and was used for semantics only. This implementation
is original code against this engine's image API.

===========================================================================
*/

#ifndef __PAGEUPDATE_H__
#define __PAGEUPDATE_H__

class idImage;

// residency change types from the TNO pageupdate job
enum pageUpdateType_t {
	PAGE_UPDATE_MAP_PHYSICAL_PAGE = 0,
	PAGE_UPDATE_UNMAP_PHYSICAL_PAGE,
	PAGE_UPDATE_MAP_PRT,
	PAGE_UPDATE_UNMAP_PRT
};

struct pageImageInfo_t {
	byte *		bits;
	int			bytePitch;
};

// one queued residency change for the virtual texture page images
struct pageImagesUpdate_t {
	pageUpdateType_t		type;
	int						physicalPagesPool;		// pool index, informational
	int						physX;					// physical cache pixel origin
	int						physY;
	int						mipLevel;
	bool					cancelled;
	bool					compressed;
	const byte *			pageData;				// three consecutive plane buffers
	int						pagePlaneBytes;			// size of one plane
	int						pageTexels;				// page edge length in texels
	idImage *				physicalPagesImage0;
	idImage *				physicalPagesImage1;
	idImage *				physicalPagesImage2;
	float					map[4];					// sixteen byte mapping entry
	idImage *				physicalMappingsImage;	// NULL, the engine runtime has no mappings image
	pageImageInfo_t			physicalMappingsImageInfo;
	int						effectivePageLevel;		// highest dirty table level, -1 = none
	int						spotX;					// page table leaf coordinates
	int						spotY;
	byte					pageSpot[4];			// packed table entry written over the region
	idImage *				pageTableImage;
	pageImageInfo_t			pageTableImageInfo[14];	// shadow bits per level
	int						tableMinX;				// dirty leaf rectangle, max exclusive
	int						tableMinY;
	int						tableMaxX;
	int						tableMaxY;
};

struct pageImagesUpdateParms_t {
	pageImagesUpdate_t *	updates;
	int						numUpdates;
	int						numCancelled;
	bool					submittedJob;			// true when a job list already ran the CPU side
	bool					skipPhysicalTextureUploads;
};

void CopyImageRect( byte * destination, int destinationPitch, const byte * source, int sourcePitch, int numScanlines, int byteWidth );
void UpdatePageMappings_PC( const pageImagesUpdateParms_t * parms );
void UpdatePageTables_PC( const pageImagesUpdateParms_t * parms );
void UpdatePageImagesJob( const pageImagesUpdateParms_t * parms );
void UploadPhysicalPages( const pageImagesUpdateParms_t ** parms, int numParms );
void UploadPageMappings( const pageImagesUpdateParms_t ** parms, int numParms );
void UploadPageTables( const pageImagesUpdateParms_t ** parms, int numParms );
void UploadPageImages( const pageImagesUpdateParms_t * parms1, const pageImagesUpdateParms_t * parms2 );

#endif /* !__PAGEUPDATE_H__ */
