# TODO: Remaining TNO dim shadow features

Status of the Wolfenstein: The New Order dim shadow features that are not yet ported to Darklight.
Stages 1 through 11 are merged and verified. Everything below is documented as a gap with rationale,
not as scheduled work. Reference code lives under references/wolftno/ (do not copy, GPL).

The engine baseline is OpenGL 3.3 core (checked in R_CheckPortableExtensions, neo/renderer/RenderSystem_init.cpp).
The TNO dim shadow shaders are #version 150, so no remaining item needs a higher GL version than the
engine already requires. The gaps are one extension, plus data and engine architecture.

## Depth bounds test (portable, optional optimization)

TNO calls GL_DepthBoundsTest(depthMinimum, depthMaximum) twice per model in renderdimshadows.cpp,
around the occlusion query draw and around the final screen-space sample draw. The min/max comes from
idRenderMatrix::DepthBoundsForExtrudedBounds over the extruded reference bounds and the clip plane.
Purpose is fill-rate culling: fragments outside the model's extruded depth range are rejected before
the fragment shader runs.

GL requirement: GL_EXT_depth_bounds_test. Never promoted to core desktop OpenGL in any version.
Core in Vulkan 1.0, available in D3D11.1, but in GL it is extension or nothing.

Current state: Darklight already probes the extension into glConfig.depthBoundsTestAvailable and uses
it for lights via r_useLightDepthBounds. The stage 10 inside gate on shadowCoord.z rejects the same
fragments in-shader, functionally equivalent but it pays the fragment shader cost first.

Port plan if wanted: add a cvar similar to r_useLightDepthBounds, guard on
glConfig.depthBoundsTestAvailable, wrap the two existing dim shadow draws with GL_DepthBoundsTest
using the stage 10 extruded bounds. Small, self-contained, correctness-neutral.

## Per-view-entity occlusion gating (portable, engine work)

TNO gives every committed render model a modelOcclusionQuery slot in occlusionState_t. The job reads
GL_GetDeferredQueryResult for the previous frame's query, begins a fresh query around a debug-box draw
of the extrude volume, and skips the whole dim shadow when queryResult <= 0 and the volume is not near
the camera (depthMinimum >= 0.1), unless dimShadowUseQuery is off or the model has flag 0x40.

GL requirement: none beyond GL 3.3 core. Occlusion queries are core since GL 1.5, conditional
rendering since GL 3.0.

Current state: Darklight's stage 3 occlusion system wires query slots for world and subview models
only. View entities (idRenderEntityLocal side) have no per-entity query result the dim shadow pass
can read.

Port plan if wanted: extend renderocclusion to allocate and begin queries per view entity, thread the
result into the dim shadow caster loop. Real engine work, zero new GL requirements.

## Secondary bucket (blocked on data and vertex format)

TNO routes a surface to progDimShadowCreateSecondary whenever surface->stMap is non-null, a
per-surface second UV-mapped texture (the transition map used for decals and blending). The vertex
shader pipes in_TexCoord1, a second UV channel, into the fragment stage. The fragment shader does a
5-tap tex2DGrad blur on samp_transmap1 using dFdx/dFdy gradients of that second UV, averages, then
clip(coverage.w - 0.3) so low-coverage texels punch holes in the shadow.

Blocked because: idDrawVert has exactly one texCoord set and the BFG asset pipeline emits no transmap.
The bucket would always be empty here.

Port plan if ever wanted: add a second UV channel to the vertex format, the model compiler, and the
asset toolchain, then author transition map content. That is new content infrastructure, not a
renderer port.

## Wound bucket (blocked on a missing gameplay system)

TNO routes a surface to progDimShadowCreateWound when the model has flag 0x04 on r.flags2 and the
material has flag 0x04 on flags1. The vertex shader evaluates ten wound spheres (parms 9 through 18,
xyz center, w scale, per-vertex distance length((center - pos) * scale) - 1) plus one ellipsoid
(parms 19 through 21 forming an inverse ellipsoid matrix), takes the minimum into vofi_TexCoord6.w.
The fragment shader samples samp_woundtexture and clips on texCoord6.w + (woundMap.w - 0.5) * 0.1.
The TNO wound decal systems populate those parms per joint.

Blocked because: Darklight has no wound representation at all. There is no source for the parms or
the texture.

Port plan if ever wanted: design a wound feature first (parms population, wound textures, per-joint
data), then the shader port itself is trivial. Out of scope for a renderer port.

## Group master/slave chaining and singleDimShadow (blocked on structure)

TNO builds a linked list in dimShadowModelSlaves: entities with a groupMasterIndex and flag 0x10 on
flags4 (attachments, weapons, held items) walk up the master chain and link themselves to the master's
dim shadow, so the caster render loop walks committedRenderModels and renders all slaves under one
shadow target with the master's light matrix. singleDimShadow is a settings field that filters to
exactly one model index for debugging.

Blocked because: viewEntity_t has no groupMasterIndex or slave linkage, and the dim shadow caster
collection is per view entity with no committed-model indirection.

Port plan if ever wanted: an approximation where attachments inherit their owner's dim shadow via the
entity hierarchy is possible but is a new feature decision, not a port.

## Summary of GL requirements

- Depth bounds test: GL 3.3 plus GL_EXT_depth_bounds_test (already probed, optional to wire).
- Occlusion gating: GL 3.3 core only, pure engine work.
- Secondary bucket, wound bucket, master/slave chaining, singleDimShadow: no GL requirement at all,
  blocked by vertex data, asset content, and engine structure respectively.

Nothing left requires an OpenGL upgrade. The 3.3 floor is sufficient for everything TNO's dim
shadows do.

## Offline lighting tools

The engine consumes two offline produced lighting files that Tech5 shipped with standalone
compiler tools. The engine side of both loaders is ported; the compiler tools are not.

- .ambient: the ambient lighting octree loaded by idAmbientLighting::LoadAmbientLighting
  (<map>.ambient, magic 0x21155701, big endian header, nodes and samples, layout checked by
  static_asserts in ambientmapcpu.h). Needs a tool that samples ambient lighting per grid cell
  over the map and writes the octree.
- .globalShadows2.bin: the global shadow depth map loaded by idGlobalShadows::LoadGlobalShadows
  (<map>.globalShadows2.bin, 64 byte light matrix plus 1024x1024 16 bit depth). Needs a tool
  that renders the map's static geometry from the sun into that depth map.

## Committed model and light parms gaps

TNO's idRenderModelParms carries per model fields this engine's renderEntity_t does not have:
castDimShadows quality selection, modelFade, maxVisibleRange, fadeVisibilityOver, highlightColor,
and localLightPositionOverride. The dim shadow and approximate lighting passes gate on cvars and
bounds instead. Porting these means adding fields to renderEntity_t plus game side plumbing.

Ported: renderEntity_t now carries castDimShadows (DIMSHADOW_DEFAULT keeps the old cvar driven
behavior), modelFade, maxVisibleRange, fadeVisibilityOver, highlightColor and
localLightPositionOverride, and renderLight_t carries lightClass, maxVisibleRange and
fadeVisibilityOver. Wired so far: castDimShadows gates and quality selects the dim shadow casters,
maxVisibleRange and fadeVisibilityOver hard cull models and lights past their range in the portal
walk, localLightPositionOverride moves the committed lighting sample point, lightClass keeps
dynamic only lights out of the approximate lighting queries and prelight only lights out of the
view. The gradual model fade and the highlight render path do not exist yet, so modelFade and
highlightColor are stored for game plumbing until those paths are ported.

The game facing approximate lighting query APIs exist now (idRenderWorld::
ApproximateLightingForBounds and ApproximateAmbientForBounds, implemented on the committed world),
so model code that Tech5 lights through approximateLighting_t (glass, transparency, tools) has a
direct integration point. The committed debug layer (r_showDynamicLighting,
r_showDynamicLightingSingleModel, r_showAmbientLighting) replaces TNO's ShowApproximateLighting
and finally wires idAmbientLighting::DebugDrawAmbientLightingSamples into the debug tools.
