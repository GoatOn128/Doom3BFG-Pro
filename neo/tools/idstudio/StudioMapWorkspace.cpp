#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "QE3.H"
#include "StudioMapWorkspace.h"
#include "InspectorDialog.h"

namespace {

enum workspaceItemType_t {
	ITEM_NONE,
	ITEM_LAYER_ROOT,
	ITEM_GROUP_ROOT,
	ITEM_REFERENCE_ROOT,
	ITEM_LAYER,
	ITEM_GROUP,
	ITEM_REFERENCE
};

enum {
	IDC_STUDIO_TREE = 22001,
	IDC_STUDIO_NAME,
	IDC_STUDIO_ADD_LAYER,
	IDC_STUDIO_ADD_GROUP,
	IDC_STUDIO_ADD_REFERENCE,
	IDC_STUDIO_ASSIGN,
	IDC_STUDIO_UNASSIGN,
	IDC_STUDIO_SELECT,
	IDC_STUDIO_VISIBILITY,
	IDC_STUDIO_RELOAD,
	IDC_STUDIO_RENAME,
	IDC_STUDIO_CREATE_REFERENCE,
	IDC_STUDIO_REMOVE
};

idList<idStr> studioLayers;
idList<idStr> studioGroups;
idList<idStr> studioReferences;
idList<idStr> hiddenLayers;
idList<idStr> hiddenGroups;
idList<idStr> hiddenReferences;
idStudioMapWorkspace *workspacePanel = NULL;
bool loadingReferences = false;

static bool Contains( const idList<idStr> &list, const char *name ) {
	for ( int i = 0; i < list.Num(); ++i ) {
		if ( idStr::Icmp( list[i], name ) == 0 ) {
			return true;
		}
	}
	return false;
}

static void AddUnique( idList<idStr> &list, const char *name ) {
	idStr clean = name;
	clean.StripLeading( ' ' );
	clean.StripLeading( '\t' );
	clean.StripTrailingWhitespace();
	if ( !clean.IsEmpty() && !Contains( list, clean ) ) {
		list.Append( clean );
	}
}

static void RemoveName( idList<idStr> &list, const char *name ) {
	for ( int i = list.Num() - 1; i >= 0; --i ) {
		if ( idStr::Icmp( list[i], name ) == 0 ) {
			list.RemoveIndex( i );
		}
	}
}

static void SplitList( const char *text, idList<idStr> &output ) {
	output.Clear();
	if ( text == NULL ) {
		return;
	}
	const char *start = text;
	for ( const char *cursor = text; ; ++cursor ) {
		if ( *cursor == '|' || *cursor == '\0' ) {
			if ( cursor > start ) {
				idStr item( start, 0, static_cast<int>( cursor - start ) );
				AddUnique( output, item );
			}
			if ( *cursor == '\0' ) {
				break;
			}
			start = cursor + 1;
		}
	}
}

static idStr JoinList( const idList<idStr> &list ) {
	idStr result;
	for ( int i = 0; i < list.Num(); ++i ) {
		if ( i > 0 ) {
			result += "|";
		}
		result += list[i];
	}
	return result;
}

static bool TagContains( const char *tag, const char *name ) {
	idList<idStr> values;
	SplitList( tag, values );
	return Contains( values, name );
}

static idStr AddTagValue( const char *tag, const char *name ) {
	idList<idStr> values;
	SplitList( tag, values );
	AddUnique( values, name );
	return JoinList( values );
}

static idStr RemoveTagValue( const char *tag, const char *name ) {
	idList<idStr> values;
	SplitList( tag, values );
	RemoveName( values, name );
	return JoinList( values );
}

static bool TagIntersects( const char *tag, const idList<idStr> &values ) {
	idList<idStr> tags;
	SplitList( tag, tags );
	for ( int i = 0; i < tags.Num(); ++i ) {
		if ( Contains( values, tags[i] ) ) {
			return true;
		}
	}
	return false;
}

static void SaveWorkspaceState() {
	if ( world_entity == NULL ) {
		return;
	}
	world_entity->epairs.Set( "_studio_layers", JoinList( studioLayers ) );
	world_entity->epairs.Set( "_studio_groups", JoinList( studioGroups ) );
	world_entity->epairs.Set( "_studio_referenceMaps", JoinList( studioReferences ) );
	world_entity->epairs.Set( "_studio_hiddenLayers", JoinList( hiddenLayers ) );
	world_entity->epairs.Set( "_studio_hiddenGroups", JoinList( hiddenGroups ) );
	world_entity->epairs.Set( "_studio_hiddenReferences", JoinList( hiddenReferences ) );
}

static void LoadWorkspaceState() {
	studioLayers.Clear();
	studioGroups.Clear();
	studioReferences.Clear();
	hiddenLayers.Clear();
	hiddenGroups.Clear();
	hiddenReferences.Clear();
	if ( world_entity == NULL ) {
		return;
	}
	SplitList( world_entity->epairs.GetString( "_studio_layers" ), studioLayers );
	SplitList( world_entity->epairs.GetString( "_studio_groups" ), studioGroups );
	SplitList( world_entity->epairs.GetString( "_studio_referenceMaps" ), studioReferences );
	SplitList( world_entity->epairs.GetString( "_studio_hiddenLayers" ), hiddenLayers );
	SplitList( world_entity->epairs.GetString( "_studio_hiddenGroups" ), hiddenGroups );
	SplitList( world_entity->epairs.GetString( "_studio_hiddenReferences" ), hiddenReferences );
}

static bool BrushMatches( const brush_t *brush, const char *key, const char *name ) {
	return brush != NULL && TagContains( brush->epairs.GetString( key ), name );
}

static void VisitAllBrushes( void ( *callback )( brush_t *, void * ), void *data ) {
	brush_t *lists[] = { &active_brushes, &selected_brushes, &filtered_brushes };
	for ( int listIndex = 0; listIndex < 3; ++listIndex ) {
		brush_t *head = lists[listIndex];
		for ( brush_t *brush = head->next; brush != NULL && brush != head; ) {
			brush_t *next = brush->next;
			callback( brush, data );
			brush = next;
		}
	}
}

static void ApplyVisibilityToBrush( brush_t *brush, void * ) {
	const char *layer = brush->epairs.GetString( "_studio_layer" );
	const char *group = brush->epairs.GetString( "_studio_group" );
	const char *reference = brush->epairs.GetString( "_studio_reference" );
	brush->studioHidden = ( layer[0] && TagIntersects( layer, hiddenLayers ) ) ||
		( group[0] && TagIntersects( group, hiddenGroups ) ) ||
		( reference[0] && Contains( hiddenReferences, reference ) );
}

static void ApplyVisibility() {
	VisitAllBrushes( ApplyVisibilityToBrush, NULL );
	Sys_UpdateWindows( W_ALL );
}

struct clearTagParms_t {
	const char *key;
	const char *value;
};

struct renameTagParms_t {
	const char *key;
	const char *oldName;
	const char *newName;
};

static void RenameTagOnBrush( brush_t *brush, void *data ) {
	const renameTagParms_t *parms = static_cast<const renameTagParms_t *>( data );
	if ( !BrushMatches( brush, parms->key, parms->oldName ) ) return;
	idStr changed = RemoveTagValue( brush->epairs.GetString( parms->key ), parms->oldName );
	changed = AddTagValue( changed, parms->newName );
	brush->epairs.Set( parms->key, changed );
}

static void ClearTagFromBrush( brush_t *brush, void *data ) {
	const clearTagParms_t *parms = static_cast<const clearTagParms_t *>( data );
	if ( BrushMatches( brush, parms->key, parms->value ) ) {
		const idStr remaining = RemoveTagValue( brush->epairs.GetString( parms->key ), parms->value );
		if ( remaining.IsEmpty() ) brush->epairs.Delete( parms->key ); else brush->epairs.Set( parms->key, remaining );
		brush->studioHidden = false;
	}
}

struct removeReferenceParms_t {
	const char *path;
};

static void RemoveReferenceBrush( brush_t *brush, void *data ) {
	const removeReferenceParms_t *parms = static_cast<const removeReferenceParms_t *>( data );
	if ( brush->studioReference && BrushMatches( brush, "_studio_reference", parms->path ) ) {
		Brush_Free( brush );
	}
}

static void RemoveReferenceGeometry( const char *path ) {
	removeReferenceParms_t parms = { path };
	VisitAllBrushes( RemoveReferenceBrush, &parms );
	for ( entity_t *entity = entities.next; entity != NULL && entity != &entities; ) {
		entity_t *next = entity->next;
		if ( idStr::Icmp( entity->epairs.GetString( "_studio_reference" ), path ) == 0 ) {
			Entity_Free( entity );
		}
		entity = next;
	}
}

static void ImportReference( const char *path ) {
	if ( path == NULL || path[0] == '\0' || idStr::Icmp( path, currentmap ) == 0 ) {
		return;
	}
	const int oldModified = mapModified;
	if ( !Map_ImportReferenceFile( path, path ) ) {
		mapModified = oldModified;
		return;
	}
	for ( brush_t *brush = selected_brushes.next; brush != &selected_brushes; brush = brush->next ) {
		brush->epairs.Set( "_studio_reference", path );
		brush->studioReference = true;
		brush->forceWireFrame = false;
		if ( brush->owner != NULL && brush->owner != world_entity ) {
			brush->owner->epairs.Set( "_studio_reference", path );
		}
	}
	Select_Deselect();
	mapModified = oldModified;
}

static void ReloadReference( const char *path ) {
	RemoveReferenceGeometry( path );
	ImportReference( path );
	ApplyVisibility();
}

static DWORD_PTR EncodeItem( workspaceItemType_t type, int index ) {
	return ( static_cast<DWORD_PTR>( type ) << 24 ) | static_cast<DWORD_PTR>( index + 1 );
}

static workspaceItemType_t DecodeType( DWORD_PTR value ) {
	return static_cast<workspaceItemType_t>( value >> 24 );
}

static int DecodeIndex( DWORD_PTR value ) {
	return static_cast<int>( value & 0x00FFFFFF ) - 1;
}

} // namespace

IMPLEMENT_DYNAMIC( idStudioMapWorkspace, CWnd )

BEGIN_MESSAGE_MAP( idStudioMapWorkspace, CWnd )
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_BN_CLICKED( IDC_STUDIO_ADD_LAYER, OnAddLayer )
	ON_BN_CLICKED( IDC_STUDIO_ADD_GROUP, OnAddGroup )
	ON_BN_CLICKED( IDC_STUDIO_ADD_REFERENCE, OnAddReference )
	ON_BN_CLICKED( IDC_STUDIO_ASSIGN, OnAssign )
	ON_BN_CLICKED( IDC_STUDIO_UNASSIGN, OnUnassign )
	ON_BN_CLICKED( IDC_STUDIO_SELECT, OnSelectMembers )
	ON_BN_CLICKED( IDC_STUDIO_VISIBILITY, OnToggleVisibility )
	ON_BN_CLICKED( IDC_STUDIO_RELOAD, OnReloadReference )
	ON_BN_CLICKED( IDC_STUDIO_RENAME, OnRename )
	ON_BN_CLICKED( IDC_STUDIO_CREATE_REFERENCE, OnCreateReference )
	ON_BN_CLICKED( IDC_STUDIO_REMOVE, OnRemove )
	ON_NOTIFY( TVN_SELCHANGED, IDC_STUDIO_TREE, OnTreeSelectionChanged )
	ON_NOTIFY( NM_DBLCLK, IDC_STUDIO_TREE, OnTreeDoubleClick )
END_MESSAGE_MAP()

idStudioMapWorkspace::idStudioMapWorkspace() : initialized( false ) {
}

bool idStudioMapWorkspace::Create( CWnd *parent ) {
	const CString className = AfxRegisterWndClass( CS_DBLCLKS, ::LoadCursor( NULL, IDC_ARROW ),
		static_cast<HBRUSH>( ::GetStockObject( COLOR_WINDOW + 1 ) ), NULL );
	return CWnd::CreateEx( 0, className, "Layers, Groups && References", WS_CHILD | WS_CLIPCHILDREN,
		CRect( 0, 0, 320, 480 ), parent, W_STUDIO_WORKSPACE ) != FALSE;
}

int idStudioMapWorkspace::OnCreate( LPCREATESTRUCT createStruct ) {
	if ( CWnd::OnCreate( createStruct ) == -1 ) {
		return -1;
	}
	nameEdit.Create( WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_STUDIO_NAME );
	addLayerButton.Create( "Layer +", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_STUDIO_ADD_LAYER );
	addGroupButton.Create( "Group +", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_STUDIO_ADD_GROUP );
	addReferenceButton.Create( "Reference +", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_STUDIO_ADD_REFERENCE );
	assignButton.Create( "Assign", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_STUDIO_ASSIGN );
	unassignButton.Create( "Unassign", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_STUDIO_UNASSIGN );
	selectButton.Create( "Select", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_STUDIO_SELECT );
	visibilityButton.Create( "Show / Hide", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_STUDIO_VISIBILITY );
	reloadButton.Create( "Reload", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_STUDIO_RELOAD );
	renameButton.Create( "Rename", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_STUDIO_RENAME );
	createReferenceButton.Create( "Move Selection to Reference Map", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_STUDIO_CREATE_REFERENCE );
	removeButton.Create( "Remove", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_STUDIO_REMOVE );
	tree.Create( WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
		CRect(), this, IDC_STUDIO_TREE );
	initialized = true;
	workspacePanel = this;
	Refresh();
	return 0;
}

void idStudioMapWorkspace::OnSize( UINT type, int width, int height ) {
	CWnd::OnSize( type, width, height );
	if ( !initialized ) {
		return;
	}
	const int margin = 5;
	const int rowHeight = 23;
	nameEdit.MoveWindow( margin, margin, width - margin * 2, rowHeight );
	const int column = ( width - margin * 4 ) / 3;
	addLayerButton.MoveWindow( margin, margin + rowHeight + 3, column, rowHeight );
	addGroupButton.MoveWindow( margin * 2 + column, margin + rowHeight + 3, column, rowHeight );
	addReferenceButton.MoveWindow( margin * 3 + column * 2, margin + rowHeight + 3, column, rowHeight );
	const int secondRow = margin + ( rowHeight + 3 ) * 2;
	const int actionWidth = ( width - margin * 6 ) / 5;
	assignButton.MoveWindow( margin, secondRow, actionWidth, rowHeight );
	unassignButton.MoveWindow( margin * 2 + actionWidth, secondRow, actionWidth, rowHeight );
	selectButton.MoveWindow( margin * 3 + actionWidth * 2, secondRow, actionWidth, rowHeight );
	visibilityButton.MoveWindow( margin * 4 + actionWidth * 3, secondRow, actionWidth, rowHeight );
	removeButton.MoveWindow( margin * 5 + actionWidth * 4, secondRow, actionWidth, rowHeight );
	const int thirdRow = secondRow + rowHeight + 3;
	reloadButton.MoveWindow( margin, thirdRow, ( width - margin * 3 ) / 2, rowHeight );
	renameButton.MoveWindow( margin * 2 + ( width - margin * 3 ) / 2, thirdRow, ( width - margin * 3 ) / 2, rowHeight );
	const int fourthRow = thirdRow + rowHeight + 3;
	createReferenceButton.MoveWindow( margin, fourthRow, width - margin * 2, rowHeight );
	const int treeTop = fourthRow + rowHeight + 4;
	tree.MoveWindow( margin, treeTop, width - margin * 2, std::max( 1, height - treeTop - margin ) );
}

void idStudioMapWorkspace::Refresh() {
	if ( !initialized || !tree.GetSafeHwnd() ) {
		return;
	}
	tree.DeleteAllItems();
	HTREEITEM layerRoot = tree.InsertItem( "Layers" );
	tree.SetItemData( layerRoot, EncodeItem( ITEM_LAYER_ROOT, -1 ) );
	for ( int i = 0; i < studioLayers.Num(); ++i ) {
		CString title = studioLayers[i];
		if ( Contains( hiddenLayers, studioLayers[i] ) ) title += " [hidden]";
		HTREEITEM item = tree.InsertItem( title, layerRoot );
		tree.SetItemData( item, EncodeItem( ITEM_LAYER, i ) );
	}
	HTREEITEM groupRoot = tree.InsertItem( "Groups" );
	tree.SetItemData( groupRoot, EncodeItem( ITEM_GROUP_ROOT, -1 ) );
	for ( int i = 0; i < studioGroups.Num(); ++i ) {
		CString title = studioGroups[i];
		if ( Contains( hiddenGroups, studioGroups[i] ) ) title += " [hidden]";
		HTREEITEM item = tree.InsertItem( title, groupRoot );
		tree.SetItemData( item, EncodeItem( ITEM_GROUP, i ) );
	}
	HTREEITEM referenceRoot = tree.InsertItem( "Reference Maps" );
	tree.SetItemData( referenceRoot, EncodeItem( ITEM_REFERENCE_ROOT, -1 ) );
	for ( int i = 0; i < studioReferences.Num(); ++i ) {
		CString title = studioReferences[i];
		if ( Contains( hiddenReferences, studioReferences[i] ) ) title += " [hidden]";
		HTREEITEM item = tree.InsertItem( title, referenceRoot );
		tree.SetItemData( item, EncodeItem( ITEM_REFERENCE, i ) );
	}
	tree.Expand( layerRoot, TVE_EXPAND );
	tree.Expand( groupRoot, TVE_EXPAND );
	tree.Expand( referenceRoot, TVE_EXPAND );
}

void idStudioMapWorkspace::OnAddLayer() {
	CString name;
	nameEdit.GetWindowText( name );
	AddUnique( studioLayers, name );
	SaveWorkspaceState();
	Sys_MarkMapModified();
	Refresh();
}

void idStudioMapWorkspace::OnAddGroup() {
	CString name;
	nameEdit.GetWindowText( name );
	AddUnique( studioGroups, name );
	SaveWorkspaceState();
	Sys_MarkMapModified();
	Refresh();
}

void idStudioMapWorkspace::OnAddReference() {
	CFileDialog dialog( TRUE, "refmap", NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		"Reference Map Files (*.refmap)|*.refmap|Map Files (*.map)|*.map||", this );
	if ( dialog.DoModal() != IDOK ) {
		return;
	}
	idStr path = dialog.GetPathName();
	path.BackSlashesToSlashes();
	const char *relativePath = fileSystem->OSPathToRelativePath( path );
	if ( relativePath != NULL && relativePath[0] != '\0' ) {
		path = relativePath;
	}
	AddUnique( studioReferences, path );
	SaveWorkspaceState();
	ReloadReference( path );
	Sys_MarkMapModified();
	Refresh();
}

void idStudioMapWorkspace::OnAssign() {
	HTREEITEM selected = tree.GetSelectedItem();
	if ( selected == NULL ) return;
	const DWORD_PTR data = tree.GetItemData( selected );
	const workspaceItemType_t itemType = DecodeType( data );
	const int index = DecodeIndex( data );
	const char *key = itemType == ITEM_LAYER ? "_studio_layer" : itemType == ITEM_GROUP ? "_studio_group" : NULL;
	const idList<idStr> *list = itemType == ITEM_LAYER ? &studioLayers : itemType == ITEM_GROUP ? &studioGroups : NULL;
	if ( key == NULL || list == NULL || index < 0 || index >= list->Num() ) return;
	for ( brush_t *brush = selected_brushes.next; brush != &selected_brushes; brush = brush->next ) {
		brush->epairs.Set( key, AddTagValue( brush->epairs.GetString( key ), ( *list )[index] ) );
	}
	ApplyVisibility();
	Sys_MarkMapModified();
}

void idStudioMapWorkspace::OnUnassign() {
	HTREEITEM selected = tree.GetSelectedItem();
	if ( selected == NULL ) return;
	const DWORD_PTR data = tree.GetItemData( selected );
	const workspaceItemType_t itemType = DecodeType( data );
	const int index = DecodeIndex( data );
	const char *key = itemType == ITEM_LAYER ? "_studio_layer" : itemType == ITEM_GROUP ? "_studio_group" : NULL;
	const idList<idStr> *list = itemType == ITEM_LAYER ? &studioLayers : itemType == ITEM_GROUP ? &studioGroups : NULL;
	if ( key == NULL || list == NULL || index < 0 || index >= list->Num() ) return;
	for ( brush_t *brush = selected_brushes.next; brush != &selected_brushes; brush = brush->next ) {
		if ( BrushMatches( brush, key, ( *list )[index] ) ) {
			const idStr remaining = RemoveTagValue( brush->epairs.GetString( key ), ( *list )[index] );
			if ( remaining.IsEmpty() ) brush->epairs.Delete( key ); else brush->epairs.Set( key, remaining );
		}
	}
	ApplyVisibility();
	Sys_MarkMapModified();
}

void idStudioMapWorkspace::OnSelectMembers() {
	HTREEITEM selected = tree.GetSelectedItem();
	if ( selected == NULL ) return;
	const DWORD_PTR data = tree.GetItemData( selected );
	const workspaceItemType_t itemType = DecodeType( data );
	const int index = DecodeIndex( data );
	const char *key = itemType == ITEM_LAYER ? "_studio_layer" : itemType == ITEM_GROUP ? "_studio_group" : NULL;
	const idList<idStr> *list = itemType == ITEM_LAYER ? &studioLayers : itemType == ITEM_GROUP ? &studioGroups : NULL;
	if ( key == NULL || list == NULL || index < 0 || index >= list->Num() ) return;
	Select_Deselect();
	for ( brush_t *brush = active_brushes.next; brush != &active_brushes; ) {
		brush_t *next = brush->next;
		if ( BrushMatches( brush, key, ( *list )[index] ) ) {
			Select_Brush( brush, false, false );
		}
		brush = next;
	}
	Sys_UpdateWindows( W_ALL );
}

void idStudioMapWorkspace::OnToggleVisibility() {
	HTREEITEM selected = tree.GetSelectedItem();
	if ( selected == NULL ) return;
	const DWORD_PTR data = tree.GetItemData( selected );
	const workspaceItemType_t type = DecodeType( data );
	const int index = DecodeIndex( data );
	idList<idStr> *hidden = type == ITEM_LAYER ? &hiddenLayers : type == ITEM_GROUP ? &hiddenGroups : type == ITEM_REFERENCE ? &hiddenReferences : NULL;
	const idList<idStr> *items = type == ITEM_LAYER ? &studioLayers : type == ITEM_GROUP ? &studioGroups : type == ITEM_REFERENCE ? &studioReferences : NULL;
	if ( hidden == NULL || items == NULL || index < 0 || index >= items->Num() ) return;
	if ( Contains( *hidden, ( *items )[index] ) ) RemoveName( *hidden, ( *items )[index] ); else AddUnique( *hidden, ( *items )[index] );
	SaveWorkspaceState();
	ApplyVisibility();
	Sys_MarkMapModified();
	Refresh();
}

void idStudioMapWorkspace::OnReloadReference() {
	HTREEITEM selected = tree.GetSelectedItem();
	if ( selected == NULL ) return;
	const DWORD_PTR data = tree.GetItemData( selected );
	const int index = DecodeIndex( data );
	if ( DecodeType( data ) != ITEM_REFERENCE || index < 0 || index >= studioReferences.Num() ) return;
	ReloadReference( studioReferences[index] );
	Refresh();
}

void idStudioMapWorkspace::OnRename() {
	HTREEITEM selected = tree.GetSelectedItem();
	if ( selected == NULL ) return;
	const DWORD_PTR data = tree.GetItemData( selected );
	const workspaceItemType_t type = DecodeType( data );
	const int index = DecodeIndex( data );
	idList<idStr> *items = type == ITEM_LAYER ? &studioLayers : type == ITEM_GROUP ? &studioGroups : NULL;
	idList<idStr> *hidden = type == ITEM_LAYER ? &hiddenLayers : type == ITEM_GROUP ? &hiddenGroups : NULL;
	const char *key = type == ITEM_LAYER ? "_studio_layer" : type == ITEM_GROUP ? "_studio_group" : NULL;
	if ( items == NULL || hidden == NULL || key == NULL || index < 0 || index >= items->Num() ) return;
	CString entered;
	nameEdit.GetWindowText( entered );
	idStr newName = entered;
	newName.StripLeading( ' ' );
	newName.StripLeading( '\t' );
	newName.StripTrailingWhitespace();
	if ( newName.IsEmpty() || Contains( *items, newName ) ) return;
	const idStr oldName = ( *items )[index];
	renameTagParms_t parms = { key, oldName, newName };
	VisitAllBrushes( RenameTagOnBrush, &parms );
	if ( Contains( *hidden, oldName ) ) {
		RemoveName( *hidden, oldName );
		AddUnique( *hidden, newName );
	}
	( *items )[index] = newName;
	SaveWorkspaceState();
	ApplyVisibility();
	Sys_MarkMapModified();
	Refresh();
}

void idStudioMapWorkspace::OnCreateReference() {
	if ( selected_brushes.next == NULL || selected_brushes.next == &selected_brushes ) {
		AfxMessageBox( "Select brushes, patches, or entities to move into a reference map first." );
		return;
	}
	CFileDialog dialog( FALSE, "refmap", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		"Reference Map Files (*.refmap)|*.refmap||", this );
	if ( dialog.DoModal() != IDOK ) return;
	idStr osPath = dialog.GetPathName();
	osPath.SetFileExtension( "refmap" );
	char savePath[MAX_OSPATH];
	idStr::Copynz( savePath, osPath, sizeof( savePath ) );
	Map_SaveSelected( savePath );

	Undo_Start( "move selection to reference map" );
	Undo_AddBrushList( &selected_brushes );
	for ( brush_t *brush = selected_brushes.next; brush != &selected_brushes; brush = brush->next ) {
		Undo_AddEntity( brush->owner );
	}
	Select_Delete();
	Undo_EndBrushList( &selected_brushes );
	Undo_End();

	idStr referencePath = osPath;
	referencePath.BackSlashesToSlashes();
	const char *relativePath = fileSystem->OSPathToRelativePath( referencePath );
	if ( relativePath != NULL && relativePath[0] != '\0' ) referencePath = relativePath;
	AddUnique( studioReferences, referencePath );
	SaveWorkspaceState();
	ReloadReference( referencePath );
	Sys_MarkMapModified();
	Refresh();
}

void idStudioMapWorkspace::OnTreeSelectionChanged( NMHDR *, LRESULT *result ) {
	HTREEITEM selected = tree.GetSelectedItem();
	if ( selected != NULL ) {
		const DWORD_PTR data = tree.GetItemData( selected );
		const workspaceItemType_t type = DecodeType( data );
		const int index = DecodeIndex( data );
		const idList<idStr> *items = type == ITEM_LAYER ? &studioLayers : type == ITEM_GROUP ? &studioGroups : type == ITEM_REFERENCE ? &studioReferences : NULL;
		if ( items != NULL && index >= 0 && index < items->Num() ) {
			nameEdit.SetWindowText( ( *items )[index].c_str() );
		}
	}
	*result = 0;
}

void idStudioMapWorkspace::OnTreeDoubleClick( NMHDR *, LRESULT *result ) {
	OnToggleVisibility();
	*result = 0;
}

void idStudioMapWorkspace::OnRemove() {
	HTREEITEM selected = tree.GetSelectedItem();
	if ( selected == NULL ) return;
	const DWORD_PTR data = tree.GetItemData( selected );
	const workspaceItemType_t type = DecodeType( data );
	const int index = DecodeIndex( data );
	if ( type == ITEM_LAYER && index >= 0 && index < studioLayers.Num() ) {
		const idStr name = studioLayers[index];
		clearTagParms_t parms = { "_studio_layer", name };
		VisitAllBrushes( ClearTagFromBrush, &parms );
		RemoveName( hiddenLayers, name );
		studioLayers.RemoveIndex( index );
	} else if ( type == ITEM_GROUP && index >= 0 && index < studioGroups.Num() ) {
		const idStr name = studioGroups[index];
		clearTagParms_t parms = { "_studio_group", name };
		VisitAllBrushes( ClearTagFromBrush, &parms );
		RemoveName( hiddenGroups, name );
		studioGroups.RemoveIndex( index );
	} else if ( type == ITEM_REFERENCE && index >= 0 && index < studioReferences.Num() ) {
		const idStr path = studioReferences[index];
		RemoveReferenceGeometry( path );
		RemoveName( hiddenReferences, path );
		studioReferences.RemoveIndex( index );
	}
	SaveWorkspaceState();
	ApplyVisibility();
	Sys_MarkMapModified();
	Refresh();
}

void StudioWorkspaceMapLoaded( const idMapFile *mapFile ) {
	if ( loadingReferences ) return;
	LoadWorkspaceState();
	if ( mapFile != NULL ) {
		for ( int stateIndex = 0; stateIndex < mapFile->GetGroupStates().Num(); ++stateIndex ) {
			const idMapEditorState &state = mapFile->GetGroupStates()[stateIndex];
			AddUnique( studioGroups, state.name );
			if ( state.state == 3 ) {
				AddUnique( hiddenGroups, state.name );
			}
		}
		for ( int stateIndex = 0; stateIndex < mapFile->GetLayerStates().Num(); ++stateIndex ) {
			const idMapEditorState &state = mapFile->GetLayerStates()[stateIndex];
			AddUnique( studioLayers, state.name );
			if ( state.state == 3 ) {
				AddUnique( hiddenLayers, state.name );
			}
		}
		for ( int entityIndex = 0; entityIndex < mapFile->GetNumEntities(); ++entityIndex ) {
			const idMapEntity *entity = mapFile->GetEntity( entityIndex );
			for ( int layerIndex = 0; layerIndex < entity->layers.Num(); ++layerIndex ) {
				AddUnique( studioLayers, entity->layers[layerIndex] );
			}
			for ( int groupIndex = 0; groupIndex < entity->groups.Num(); ++groupIndex ) {
				AddUnique( studioGroups, entity->groups[groupIndex] );
			}
			for ( int primitiveIndex = 0; primitiveIndex < entity->GetNumPrimitives(); ++primitiveIndex ) {
				const idMapPrimitive *primitive = entity->GetPrimitive( primitiveIndex );
				for ( int groupIndex = 0; groupIndex < primitive->groups.Num(); ++groupIndex ) {
					AddUnique( studioGroups, primitive->groups[groupIndex] );
				}
				if ( primitive->GetType() == idMapPrimitive::TYPE_REFERENCE ) {
					AddUnique( studioReferences, static_cast<const idMapReference *>( primitive )->GetMapName() );
				}
			}
		}
	}
	loadingReferences = true;
	for ( int i = 0; i < studioReferences.Num(); ++i ) {
		ImportReference( studioReferences[i] );
	}
	loadingReferences = false;
	ApplyVisibility();
	StudioWorkspaceRefreshPanel();
}

void StudioWorkspaceAppendReferences( idMapEntity *world ) {
	if ( world == NULL ) {
		return;
	}
	for ( int i = 0; i < studioReferences.Num(); ++i ) {
		idMapReference *reference = new (TAG_IDLIB) idMapReference;
		reference->SetMapName( studioReferences[i] );
		world->AddPrimitive( reference );
	}
}

void StudioWorkspaceApplyEditorStates( idMapFile &mapFile ) {
	for ( int i = 0; i < studioGroups.Num(); ++i ) {
		idMapEditorState state;
		state.name = studioGroups[i];
		state.state = Contains( hiddenGroups, studioGroups[i] ) ? 3u : 0u;
		mapFile.EditGroupStates().Append( state );
	}
	for ( int i = 0; i < studioLayers.Num(); ++i ) {
		idMapEditorState state;
		state.name = studioLayers[i];
		state.state = Contains( hiddenLayers, studioLayers[i] ) ? 3u : 0u;
		mapFile.EditLayerStates().Append( state );
	}
}

void StudioWorkspaceMapNew() {
	studioLayers.Clear();
	studioGroups.Clear();
	studioReferences.Clear();
	hiddenLayers.Clear();
	hiddenGroups.Clear();
	hiddenReferences.Clear();
	SaveWorkspaceState();
	StudioWorkspaceRefreshPanel();
}

void StudioWorkspaceBeforeSave() {
	SaveWorkspaceState();
}

void StudioWorkspaceRefreshPanel() {
	if ( workspacePanel != NULL && workspacePanel->GetSafeHwnd() ) {
		workspacePanel->Refresh();
	}
}

static void ShowWorkspacePanel() {
	if ( g_Inspectors != NULL && g_Inspectors->GetSafeHwnd() ) {
		g_Inspectors->ShowWindow( SW_SHOWNOACTIVATE );
		g_Inspectors->SetMode( W_STUDIO_WORKSPACE );
	}
}

void StudioWorkspaceCommandAddLayer() {
	ShowWorkspacePanel();
	if ( workspacePanel != NULL ) workspacePanel->CommandAddLayer();
}

void StudioWorkspaceCommandAddGroup() {
	ShowWorkspacePanel();
	if ( workspacePanel != NULL ) workspacePanel->CommandAddGroup();
}

void StudioWorkspaceCommandAddReference() {
	ShowWorkspacePanel();
	if ( workspacePanel != NULL ) workspacePanel->CommandAddReference();
}

void StudioWorkspaceCommandCreateReference() {
	ShowWorkspacePanel();
	if ( workspacePanel != NULL ) workspacePanel->CommandCreateReference();
}
