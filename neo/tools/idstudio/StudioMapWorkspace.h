#pragma once

#include "afxcmn.h"

class idMapFile;
class idMapEntity;

class idStudioMapWorkspace : public CWnd {
	DECLARE_DYNAMIC( idStudioMapWorkspace )

public:
	idStudioMapWorkspace();
	bool Create( CWnd *parent );
	void Refresh();
	void CommandAddLayer() { OnAddLayer(); }
	void CommandAddGroup() { OnAddGroup(); }
	void CommandAddReference() { OnAddReference(); }
	void CommandCreateReference() { OnCreateReference(); }

protected:
	CTreeCtrl tree;
	CEdit nameEdit;
	CButton addLayerButton;
	CButton addGroupButton;
	CButton addReferenceButton;
	CButton assignButton;
	CButton unassignButton;
	CButton selectButton;
	CButton visibilityButton;
	CButton reloadButton;
	CButton renameButton;
	CButton createReferenceButton;
	CButton removeButton;
	bool initialized;

	afx_msg int OnCreate( LPCREATESTRUCT createStruct );
	afx_msg void OnSize( UINT type, int width, int height );
	afx_msg void OnAddLayer();
	afx_msg void OnAddGroup();
	afx_msg void OnAddReference();
	afx_msg void OnAssign();
	afx_msg void OnUnassign();
	afx_msg void OnSelectMembers();
	afx_msg void OnToggleVisibility();
	afx_msg void OnReloadReference();
	afx_msg void OnRename();
	afx_msg void OnCreateReference();
	afx_msg void OnTreeSelectionChanged( NMHDR *header, LRESULT *result );
	afx_msg void OnTreeDoubleClick( NMHDR *header, LRESULT *result );
	afx_msg void OnRemove();
	DECLARE_MESSAGE_MAP()
};

void StudioWorkspaceMapLoaded( const idMapFile *mapFile = NULL );
void StudioWorkspaceMapNew();
void StudioWorkspaceBeforeSave();
void StudioWorkspaceAppendReferences( idMapEntity *world );
void StudioWorkspaceApplyEditorStates( idMapFile &mapFile );
void StudioWorkspaceRefreshPanel();
void StudioWorkspaceCommandAddLayer();
void StudioWorkspaceCommandAddGroup();
void StudioWorkspaceCommandAddReference();
void StudioWorkspaceCommandCreateReference();
