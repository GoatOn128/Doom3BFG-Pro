# Render parity audit: Darklight vs Wolfenstein: The New Order tech5

Baseline: main at 2c1c9b32, with PR #39 (debug tool skinning fix) and PR #40 (committed model
and light parms) pending CI. This audit compares the engine renderer (neo/renderer) against the
reference of record under references/wolftno/src/engine/renderer. The reference is decompiled
retail code and GPL incompatible; it was used for semantics and feature inventory only, nothing
was copied from it.

## Already ported and merged

- renderprog architecture: DeclRenderProg, DeclRenderProgGenerator, render parm decls
  (RenderProgParm), ParmBlock, GL upload in gl_declrenderprog_ogl_pc.cpp.
- OpenGL 3.3 core via glad 2.0.8, GraphicsAPIWrapper abstraction, render destinations.
- Shadow buffer split (rendershadowbuffer.cpp/.h) with caster sorting, facing options, per light
  polygon offsets and suppressShadowInViewID.
- TNO global shadows: shadowmapcpu plus the .globalShadows2.bin loader, 4-slice parallel cascade,
  shadow buffer split and build fixes.
- TNO approximate lighting: per model prime light accumulation, approximateLighting_t, the
  committed world (idRenderWorldCommitted) with ApproximateLightingForBounds and
  ApproximateAmbientForBounds query APIs, and the committed debug layer
  (r_showDynamicLighting, r_showAmbientLighting).
- TNO ambient lighting: ambientmapcpu plus the .ambient loader with layout static_asserts.
- TNO dim shadows stages 1 through 11: per model dim shadows from the prime light, lixel
  resolution, alpha tested caster buckets, mid and high quality dithered PCF draws, clip plane
  fade, occlusion gating for world and subview models.
- Self shadows (renderselfshadows.cpp).
- GPU occlusion culling of world models (occlusionQueryState).
- Virtual textures: virtualtexturesystem, virtualmaterialatlas, preloader, encoder, plus the
  pageminlod, pagesort and transcode jobs.
- Debug tools rounds 1 through 5: immediate mode tools on the shader pipeline, per vertex tools
  that follow the animated pose via CPU skinning, committed lighting debug draws.
- Resolution scale and the quake style viewport shrink with centering.
- renderdeform and tr_trace operating on the committed pose.
- Easy ports batch: dim shadow depth bounds test, r_dimShadowSingleModel filter, the
  r_showPVS / r_showRuler / r_showModelGroupMasters / r_showVmtrChannel / r_drawLoadedImages
  overlays, and r_captureFrames frame capture.

## Easy ports (implemented)

Small, self-contained, no new GL requirements, no data or content dependency. All four are done.

1. Depth bounds test around the two dim shadow draws. GL_EXT_depth_bounds_test is already probed
   into glConfig.depthBoundsTestAvailable and used for lights. Wrap the occlusion query draw and
   the screen space sample draw with GL_DepthBoundsTest using the stage 10 extruded bounds. Fill
   rate culling only, correctness neutral. Done as r_dimShadowDepthBounds around the screen space
   sample draw of each caster.
2. singleDimShadow debug filter. TNO's settings field filters the dim shadow casters to exactly
   one model for debugging. Done as the r_dimShadowSingleModel cvar on the caster index in
   RB_RenderDimShadows. The master/slave chaining around it is a separate, larger item.
3. Committed debug overlays from the reference tool set: r_showPVS, r_showRuler,
   r_showModelGroupMasters, r_showVmtrChannel, r_drawLoadedImages. Done in gl_RenderDebug.cpp;
   r_showModelGroupMasters reads the new renderEntity_t::groupMasterIndex.
4. Frame capture (rendercapture). A readback plus file write of the current framebuffer. Done as
   the r_captureFrames cvar in the back end, one numbered TGA per executed frame.

## Lacking: medium effort

Real engine work, contained, no content dependency.

1. Per-view-entity occlusion gating for dim shadows. TNO gives every committed model a
   modelOcclusionQuery slot and skips the whole dim shadow when the previous frame's query was
   empty and the volume is not near the camera. The stage 3 occlusion system in this engine only
   wires world and subview models. Extend it to view entities and thread the result into the dim
   shadow caster loop. GL 3.3 core only. Done in the dim shadow pass itself: r_dimShadowUseQuery
   issues a samples passed query around a depth only box of the extruded volume and skips the
   caster when the previous result was empty and the volume stays off the near camera.
2. Group master/slave dim shadow chaining. Attachments, weapons and held items link to their
   group master's dim shadow through groupMasterIndex and a flag, so one shadow target covers the
   whole group. Needs the group master index on viewEntity_t and slave linkage in the caster
   collection. Done on the merged groupMasterIndex plumbing: r_dimShadowGroupSlaves renders group
   slaves into their master's dim shadow target with the master's light matrix and master bounds,
   slaves without a casting master get no dim shadow, matching the decompiled reference.
3. Full lightClass routing. PR #40 ports the enum plus the two clean consumers (dynamic only
   lights out of the approximate lighting queries, prelight only lights out of the view). TNO
   also splits area references into draw and approximate lighting lists and routes the slowlight,
   private prime and blended only classes through them. Faithful routing means restructuring the
   committed area reference lists.
4. modelDepthSort. TNO's per model depth sort enum (portal inside and plane, view head and body,
   scene, after world, sky, transparent) drives the sort pipeline. This engine sorts by material
   sort values; per model overrides touch the draw surf sort and the sort pass.
5. Per model viewport. TNO's idRenderModelParms carries an idScreenRect viewport per model. This
   engine has per view and per light scissors; per model scissoring touches
   R_LinkDrawSurfToView and the back end scissor state.
6. Virtual texture pageupdate job. The engine has pageminlod, pagesort and transcode but not
   TNO's pageupdate scheduling; residency updates currently go through the preloader path.
7. Depth prepass (renderdepth, rendersortdepth). TNO runs a dedicated depth job that the sorted
   and post passes build on. This engine fills depth during the ambient draw. Contained but it
   touches every draw path.
8. Fog job parity check. TNO implements fog lights in a dedicated renderfog job. This engine
   draws fog lights through the interaction path inherited from BFG. Verify visual parity before
   considering a port; it may already be equivalent.

Note: the reference jobifies the whole front end (renderwalkbsp, rendercull, renderdrawsurf,
renderstate, rendersort, setupmodelmvp, parmstate). This engine keeps the classic BFG front end
with the committed world boundary. That is architecture, not a missing feature, and is not worth
porting unless full jobification becomes a goal.

## Lacking: large effort or blocked on data, content or game systems

1. Model fade render path. modelFade and fadeVisibilityOver are stored as of PR #40. TNO fades
   models through a dedicated material coverage path plus shader support this engine lacks. Both
   the gradual distance fade and game driven fades need it.
2. Highlight render pass. highlightColor is stored as of PR #40. TNO draws highlighted items
   through the highlightColor and highlightScale renderparms in a dedicated pass. New pass plus
   game plumbing.
3. Dim shadow secondary bucket. Needs a second UV channel in idDrawVert, model compiler and asset
   toolchain changes, transmap content, and the 5-tap tex2DGrad blur program. Blocked on data,
   not code.
4. Dim shadow wound bucket. Needs a wound gameplay system first (per joint wound spheres, wound
   textures). The shader port itself is trivial once the data exists. Out of scope for a renderer
   port.
5. Post process stack. The reference has renderpostprocess, setupcolorgrading,
   renderdynamiccolorcorrection, renderglare, rendergodrays and renderhazeflare, driven by
   renderparms for exposure, radialBlurCenter, globalBlur, doubleVision, the dof family,
   brightnessScale and the fog family. This engine has only the resolution scale post pass and
   motion blur. Each effect is a screen space pass with its own renderparms; the stack is the
   single largest remaining block of renderer work and should be staged effect by effect.
6. Screen space reflections (renderscreenspacereflections). Needs depth and normal buffers plus
   the reflection pass. Large.
7. Dynamic environment rendering (renderdynamicenv). TNO updates environment probes dynamically.
   This engine has no environment probe system. Large.
8. Skin blending and wrinkles (useSkinBlending, wrinkleScale, wrinkleFactor0-3). Character
   material systems tied to the animation pipeline. Large.
9. Bink video overlays (ingamevideo, rendervideooverlay, videotranscode job). The engine plays
   RoQ cinematics through Cinematic.cpp. Bink adds a licensed codec plus the overlay model.
   Probably skip.
10. Texture atlas streaming (idAtlasResource). TNO streams atlas pages as resources. The virtual
    texture system covers the main use case; the atlas resource layer is separate infrastructure.
11. Fracture decals (the rp_fracture renderparm family). TNO's destructible fracture system.
    Needs the game system first.
12. Imposters (imposterParent, imposterNext in idRenderModelParms). A new model representation
    for distant LOD. Large, low priority.
13. maxTexelDensity. Per model texture density scaling tied to the virtual texture system.
    Medium-large, VT integration.
14. Offline compiler tools. The .ambient octree compiler and the .globalShadows2.bin depth
    renderer. Both loaders are ported; the tools are standalone programs. Tool-side effort, not
    renderer-side.

## GL requirements

Everything above fits the GL 3.3 core floor except the optional depth bounds test
(GL_EXT_depth_bounds_test, already probed; in GL it is extension or nothing). No item requires a
GL upgrade.

## Suggested order

1. Easy list: depth bounds test, singleDimShadow, the debug overlays, frame capture.
2. Medium list by value: per-view-entity occlusion gating, group master/slave dim shadow
   chaining, full lightClass routing, then modelDepthSort, per model viewport, pageupdate,
   depth prepass.
3. Large list only on demand: the post process stack staged effect by effect is the biggest
   block; everything else is blocked on data, content or game systems and should wait for those.
