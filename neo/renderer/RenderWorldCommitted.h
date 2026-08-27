#ifndef __RENDERWORLDCOMMITTED_H__
#define __RENDERWORLDCOMMITTED_H__

class idRenderWorldLocal;
class idRenderEntityCommitted;
class idRenderLightCommitted;

// Renderer-owned view of a world. Producer objects never appear in these
// arrays; only stable committed objects cross the world/render boundary.
class idRenderWorldCommitted {
public:
	explicit idRenderWorldCommitted( idRenderWorldLocal * owner );

	void PublishEntity( int index, idRenderEntityCommitted * entity, bool postCommit );
	void PublishLight( int index, idRenderLightCommitted * light, bool postCommit );
	void ForgetEntity( int index );
	void ForgetLight( int index );
	void PostCommitData();

	int GetNumCommittedEntities() const { return committedRenderEntities.Num(); }
	int GetNumCommittedLights() const { return committedRenderLights.Num(); }

	idRenderWorldLocal * world;
	idList< idRenderEntityCommitted * > committedRenderEntities;
	idList< idRenderLightCommitted * > committedRenderLights;

private:
	idList< byte > postCommitEntities;
	idList< byte > postCommitLights;
};

#endif
