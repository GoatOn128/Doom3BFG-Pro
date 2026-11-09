/*
===========================================================================

Doom 3 BFG Edition GPL Source Code
Copyright (C) 1993-2012 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 BFG Edition GPL Source Code ("Doom 3 BFG Edition Source Code").  

Doom 3 BFG Edition Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 BFG Edition Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 BFG Edition Source Code.  If not, please request a copy in writing to id Software at the address below.

id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA

===========================================================================
*/
#ifndef __DECLRENDERPROG_H__
#define __DECLRENDERPROG_H__

#include "../framework/DeclManager.h"

// Named render program declaration. A renderprog decl holds the vertex
// and fragment shader sources in hlsl_vp and hlsl_fp blocks, following
// the decl renderprog design of id Tech 5 as reconstructed in
// references/wolftno (unlicensed decompiled MachineGames/Bethesda
// code). This implementation is written from scratch on the BFG idlib.
// The shader sources are stored verbatim so preprocessor directives and
// comments survive intact, and are translated to GLSL when the program
// is loaded on the GPU.

class idDeclRenderProg : public idDecl {
public:
	virtual const char *	DefaultDefinition() const;
	virtual bool			Parse( const char *text, const int textLength, bool allowBinaryVersion = false );
	virtual void			FreeData();
	virtual size_t			Size() const;
	virtual void			List() const;
	virtual void			Print() const;

	// shader sources, empty when the decl has no such block
	const char *			GetVertexSource() const { return vertexSource; }
	const char *			GetFragmentSource() const { return fragmentSource; }
	int						GetVertexSourceLength() const { return vertexSource.Length(); }
	int						GetFragmentSourceLength() const { return fragmentSource.Length(); }

private:
	idStr					vertexSource;
	idStr					fragmentSource;
};

#endif /* !__DECLRENDERPROG_H__ */
