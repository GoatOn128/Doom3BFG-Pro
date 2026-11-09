#ifndef __DECLRENDERPROG_H__
#define __DECLRENDERPROG_H__

/*
Render program declaration for the renderprog system migration.

Adapted from the Wolfenstein: The New Order idDeclRenderProg interface
reconstructed under references/wolfnto-reversedengineered (declrenderprog.h).
That material is decompiled proprietary code and is not GPL-compatible; this
is a re-implementation on this engine's idlib. The platform resource manager,
the generated-file cache, and the console targets of the original are dropped;
PC GLSL is the only target.
*/

#include "../framework/DeclManager.h"
#include "../idlib/Str.h"
#include "../idlib/containers/StaticList.h"

#include "DeclRenderProgAttribs.h"
#include "ParmBlock.h"

class idImage;

/*
Per-decl data filled in by the parser and the generator. The inherit, state,
flags, parms, interaction, and hlsl_vp/hlsl_fp blocks are all parsed; the
shader bodies are translated to GLSL by DeclRenderProgGenerator.
*/
struct renderProgData_t {
	renderProgData_t() { Clear(); }

	void				Clear();
	renderProgData_t &	operator=( const renderProgData_t & other );

	idStr				inheritsFrom;
	unsigned int		compoundCheckSum;
	unsigned int		parentCheckSum;
	idList<idStr>		dependencies;
	unsigned long long	glState;			// gl state bits
	idParmBlock			parmBlock;			// parms{} block expressions
	idStaticList<short, 42>	vertexParms;	// uniform parms used by the vertex program
	idStaticList<short, 42>	fragmentParms;	// uniform parms used by the fragment program
	idStaticList<short, 16>	textureParms;	// texture parms used by either program
	idStaticList<short, 16>	samplerParms;	// sampler for each texture parm
	int					flags;				// perforated, sky, modelfade
	float				stageSort;			// sort value computed from the parm block
	bool				hasInteractions;	// has _point/_spot/_parallel variants
	bool				hasAlphaToCoverage;
	bool				has16BitScaleBias;
	bool				hasHardwareSkinning;
	bool				hasVertexTexture;
	bool				hasClip;
	bool				hasDepthOutput;
	bool				hasBranchStripping;
	unsigned int		vertexMask;			// vertex attributes used by the vertex program
	unsigned short		fragmentOutputs;		// number of fragment outputs
	idStr				vertexCode;
	idStr				fragmentCode;
};

class idDeclRenderProg : public idDecl {
public:
	idDeclRenderProg() { Clear(); }
	virtual					~idDeclRenderProg() { FreeData(); }

	virtual const char *	DefaultDefinition() const;
	virtual bool			Parse( const char *text, const int textLength, bool allowBinaryVersion );
	virtual void			FreeData();
	virtual size_t			Size() const { return sizeof( idDeclRenderProg ); }
	virtual void			List() const;
	virtual void			Print() const;

	// reset to the default state; an immediate FreeData() must be safe
	void					Clear();

	// true if any of the parm lists references the given parm
	bool					ProgUsesParm( const idRenderProgParm * parm ) const;

	// light interaction variants (_point/_spot/_parallel), resolved when the
	// decl has an interaction block; entries may be null
	const idDeclRenderProg *	GetInteractionVariant( int lightType ) const { return versions[lightType]; }

	renderProgData_t		data;

	// shader construct info of the last parse; an inherit copies these from
	// the parent so inherited shader bodies keep their attributes
	shaderConstructInfo_t	vertexConstructInfo;
	shaderConstructInfo_t	fragmentConstructInfo;

	// Uploaded GL state (phase 4). The program handle is a GLuint stored as
	// an unsigned int so this header does not pull in the GL headers; 0 means
	// the decl has not been uploaded yet. The location lists hold the GL
	// uniform location + 1 for each entry of the matching parm list in data,
	// so 0 means the compiled program does not use that parm. The program and
	// the location lists are released together when the decl data is cleared,
	// and EnsureUploaded rebuilds the program whenever the lists no longer
	// match the current parm lists.
	unsigned int			program = 0;
	idList<int>				vertexUniformLocations;
	idList<int>				fragmentUniformLocations;
	idList<int>				samplerLocations;

	// compile and link the generated code into a GL program and resolve all
	// uniform and sampler locations; safe to call repeatedly
	void					EnsureUploaded();
	void					UploadTargetCode();
	bool					IsUploaded() const { return program != 0; }
	unsigned int			GetProgram() const { return program; }

	// drop the linked GL program but keep the parsed decl data; the program
	// compiles again on the next EnsureUploaded. Full decl reloads go
	// through the decl manager (FreeData).
	void					FreeProgram();

	// true when the vertex program declares the joint matrix uniform block
	bool					UsesJoints() const { return data.hasHardwareSkinning; }
	int						GetVertexUniformLocation( int index ) const { return vertexUniformLocations[index]; }
	int						GetFragmentUniformLocation( int index ) const { return fragmentUniformLocations[index]; }
	int						GetSamplerUniformLocation( int index ) const { return samplerLocations[index]; }

private:
	const idDeclRenderProg *	inheritsFromDecl;
	unsigned int				vertexProgramChecksum;
	unsigned int				fragmentProgramChecksum;
	bool						hasDerived;		// some other renderprog inherits from this one
	idStaticList<const idDeclRenderProg *, 3> versions;
};

#endif
