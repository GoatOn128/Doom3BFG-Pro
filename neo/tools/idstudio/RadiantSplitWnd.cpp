#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "QE3.H"
#include "InspectorDialog.h"
#include "RadiantSplitWnd.h"

BOOL idStudioToolBar::LoadStudioToolBar( UINT resourceId ) {
	// The idStudio image wells do not share one transparency color.  XTP uses
	// the upper-left pixel of each resource as its mask; doing the same here is
	// important for both the green idStudio strips and the grey legacy strips.
	COLORREF transparentColor = CLR_INVALID;
	HBITMAP resourceBitmap = static_cast<HBITMAP>( ::LoadImage( AfxGetResourceHandle(),
		MAKEINTRESOURCE( resourceId ), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION ) );
	if ( resourceBitmap != NULL ) {
		HDC bitmapDC = ::CreateCompatibleDC( NULL );
		if ( bitmapDC != NULL ) {
			HGDIOBJ oldBitmap = ::SelectObject( bitmapDC, resourceBitmap );
			transparentColor = ::GetPixel( bitmapDC, 0, 0 );
			::SelectObject( bitmapDC, oldBitmap );
			::DeleteDC( bitmapDC );
		}
		::DeleteObject( resourceBitmap );
	}
	if ( transparentColor == CLR_INVALID ) transparentColor = RGB( 0, 255, 0 );
	m_ImagesLocked.SetTransparentColor( transparentColor );
	m_ColdImagesLocked.SetTransparentColor( transparentColor );
	m_DisabledImagesLocked.SetTransparentColor( transparentColor );
	if ( !LoadToolBar( resourceId, 0, 0, TRUE ) ) {
		return FALSE;
	}
	// LoadToolBar rebuilds the locked image wells, so restore the resource's
	// actual key on the newly-created lists before promoting them to 32-bit.
	m_ImagesLocked.SetTransparentColor( transparentColor );
	m_ColdImagesLocked.SetTransparentColor( transparentColor );
	m_DisabledImagesLocked.SetTransparentColor( transparentColor );
	// XTP promotes color-keyed command-bar strips to premultiplied 32-bit
	// images at runtime.  CMFCToolBar otherwise replaces the configured key
	// with COLOR_BTNFACE during every paint, which exposes a black image-well
	// background.  ConvertTo32Bits performs the equivalent runtime promotion;
	// the embedded resource remains byte-for-byte identical to DOOMx64.
	const CSize nativeImageSize = m_ImagesLocked.GetImageSize();
	m_ImagesLocked.ConvertTo32Bits( transparentColor );
	if ( m_ColdImagesLocked.IsValid() ) m_ColdImagesLocked.ConvertTo32Bits( transparentColor );
	if ( m_DisabledImagesLocked.IsValid() ) m_DisabledImagesLocked.ConvertTo32Bits( transparentColor );
	typedef UINT ( WINAPI *getDpiForWindow_t )( HWND );
	static getDpiForWindow_t getDpiForWindow = reinterpret_cast<getDpiForWindow_t>(
		::GetProcAddress( ::GetModuleHandleA( "user32.dll" ), "GetDpiForWindow" ) );
	const UINT dpi = getDpiForWindow != NULL && GetSafeHwnd() != NULL ? getDpiForWindow( GetSafeHwnd() ) : 96;
	const double imageScale = static_cast<double>( dpi ) / 96.0;
	if ( imageScale > 1.001 ) {
		m_ImagesLocked.SmoothResize( imageScale );
		if ( m_ColdImagesLocked.IsValid() ) m_ColdImagesLocked.SmoothResize( imageScale );
		if ( m_DisabledImagesLocked.IsValid() ) m_DisabledImagesLocked.SmoothResize( imageScale );
	}
	const int nativeWidth = nativeImageSize.cx > 0 ? nativeImageSize.cx : 16;
	const int nativeHeight = nativeImageSize.cy > 0 ? nativeImageSize.cy : 16;
	const CSize imageSize( Max( nativeWidth, ::MulDiv( nativeWidth, dpi, 96 ) ),
		Max( nativeHeight, ::MulDiv( nativeHeight, dpi, 96 ) ) );
	const CSize buttonSize( imageSize.cx + Max( 6, ::MulDiv( 6, dpi, 96 ) ),
		imageSize.cy + Max( 6, ::MulDiv( 6, dpi, 96 ) ) );
	SetLockedSizes( buttonSize, imageSize, TRUE );
	AdjustLayout();
	return TRUE;
}

void idStudioToolBar::RemoveCommand( UINT commandId ) {
	const int index = CommandToIndex( commandId );
	if ( index >= 0 ) RemoveButton( index );
}

void idStudioToolBar::CheckButton( UINT commandId, BOOL checked ) {
	const int index = CommandToIndex( commandId );
	if ( index < 0 ) return;
	UINT style = GetButtonStyle( index );
	style = checked ? ( style | TBBS_CHECKED ) : ( style & ~TBBS_CHECKED );
	SetButtonStyle( index, style );
}

namespace {
const int CAPTION_HEIGHT = 22;
const int TAB_HEIGHT = 20;
const int SPLITTER_WIDTH = 2;
const int AUTOHIDE_WIDTH = 25;
const int AUTOHIDE_TAB_HEIGHT = 92;
const UINT WM_IDSTUDIO_FLYOUT_COMMAND = WM_APP + 0x341;
const WPARAM IDSTUDIO_FLYOUT_PIN = 1;
const WPARAM IDSTUDIO_FLYOUT_CLOSE = 2;

int StudioDpiScale( HWND window, int logicalPixels ) {
	typedef UINT ( WINAPI *getDpiForWindow_t )( HWND );
	static getDpiForWindow_t getDpiForWindow = reinterpret_cast<getDpiForWindow_t>(
		::GetProcAddress( ::GetModuleHandleA( "user32.dll" ), "GetDpiForWindow" ) );
	const UINT dpi = getDpiForWindow != NULL && window != NULL ? getDpiForWindow( window ) : 96;
	return Max( 1, ::MulDiv( logicalPixels, static_cast<int>( dpi ), 96 ) );
}

int StudioDpiScale( const CWnd *window, int logicalPixels ) {
	return StudioDpiScale( window != NULL ? window->GetSafeHwnd() : NULL, logicalPixels );
}

int StudioDpiUnscale( const CWnd *window, int devicePixels ) {
	typedef UINT ( WINAPI *getDpiForWindow_t )( HWND );
	static getDpiForWindow_t getDpiForWindow = reinterpret_cast<getDpiForWindow_t>(
		::GetProcAddress( ::GetModuleHandleA( "user32.dll" ), "GetDpiForWindow" ) );
	const UINT dpi = getDpiForWindow != NULL && window != NULL && window->GetSafeHwnd() != NULL ?
		getDpiForWindow( window->GetSafeHwnd() ) : 96;
	return Max( 1, ::MulDiv( devicePixels, 96, static_cast<int>( dpi ) ) );
}

const UINT ID_ENGINE_EXIT = 21046;
const UINT ID_ENGINE_ASPECT_FIT = 21048;
const UINT ID_ENGINE_ASPECT_16_9 = 21049;
const UINT ID_ENGINE_ASPECT_4_3 = 21050;
const UINT ID_ENGINE_ASPECT_16_10 = 21051;
const UINT ID_ENGINE_ASPECT_R_MODE = 21091;

idCVar idStudio_leftDockWidth( "idStudio_leftDockWidth", "410", CVAR_TOOL | CVAR_ARCHIVE | CVAR_INTEGER,
	"width of the idStudio camera/browser dock", 280, 900 );
idCVar idStudio_cameraDockFraction( "idStudio_cameraDockFraction", "0.49", CVAR_TOOL | CVAR_ARCHIVE | CVAR_FLOAT,
	"fraction of the left idStudio dock occupied by World Camera", 0.2f, 0.8f );
idCVar idStudio_rightFlyoutWidth( "idStudio_rightFlyoutWidth", "430", CVAR_TOOL | CVAR_ARCHIVE | CVAR_INTEGER,
	"width of idStudio auto-hide tool flyouts", 300, 900 );

CString StudioWindowClass( HBRUSH brush = NULL ) {
	if ( brush == NULL ) brush = static_cast<HBRUSH>( ::GetStockObject( COLOR_3DFACE + 1 ) );
	return AfxRegisterWndClass( CS_DBLCLKS, ::LoadCursor( NULL, IDC_ARROW ), brush, NULL );
}

bool StudioSetWindowRect( CWnd *window, const CWnd *insertAfter, int x, int y, int width, int height, UINT flags ) {
	if ( window == NULL || !window->GetSafeHwnd() ) return false;
	CRect current;
	window->GetWindowRect( &current );
	CWnd *parent = window->GetParent();
	if ( parent != NULL && parent->GetSafeHwnd() ) parent->ScreenToClient( &current );
	const bool wantVisible = ( flags & SWP_SHOWWINDOW ) != 0;
	const bool wantHidden = ( flags & SWP_HIDEWINDOW ) != 0;
	const bool visibilityCurrent = ( !wantVisible || window->IsWindowVisible() ) &&
		( !wantHidden || !window->IsWindowVisible() );
	if ( current.left == x && current.top == y && current.Width() == Max( 0, width ) &&
		current.Height() == Max( 0, height ) && visibilityCurrent ) return false;
	window->SetWindowPos( insertAfter, x, y, Max( 0, width ), Max( 0, height ), flags );
	return true;
}

int StudioChromeHeight( CWnd *window, const CWnd *owner ) {
	if ( window == NULL || !window->GetSafeHwnd() ) return 0;
	if ( idStudioMenuBar *menu = dynamic_cast<idStudioMenuBar *>( window ) ) return menu->PreferredHeight();
	if ( idStudioCameraControlBar *controls = dynamic_cast<idStudioCameraControlBar *>( window ) ) return controls->PreferredHeight();
	if ( idStudioWorldEditCommandBars *bars = dynamic_cast<idStudioWorldEditCommandBars *>( window ) ) return bars->PreferredHeight();
	if ( idStudioToolBar *toolbar = dynamic_cast<idStudioToolBar *>( window ) ) {
		const CSize size = toolbar->CalcFixedLayout( FALSE, TRUE );
		if ( size.cy > 0 ) return size.cy;
	}
	if ( CToolBar *toolbar = dynamic_cast<CToolBar *>( window ) ) {
		const CSize size = toolbar->CalcFixedLayout( FALSE, TRUE );
		if ( size.cy > 0 ) return size.cy;
	}
	return StudioDpiScale( owner, 28 );
}

CString StudioMenuCaption( HMENU menu, int position ) {
	char text[256] = {};
	::GetMenuStringA( menu, position, text, sizeof( text ), MF_BYPOSITION );
	CString caption( text );
	caption.Replace( "&", "" );
	const int tab = caption.Find( '\t' );
	if ( tab >= 0 ) caption = caption.Left( tab );
	return caption;
}

HMENU StudioFindSubMenu( HMENU menu, const char *caption ) {
	if ( menu == NULL ) return NULL;
	for ( int i = 0; i < ::GetMenuItemCount( menu ); ++i ) {
		if ( StudioMenuCaption( menu, i ).CompareNoCase( caption ) == 0 ) return ::GetSubMenu( menu, i );
	}
	return NULL;
}

HMENU StudioClonePopup( HMENU source ) {
	if ( source == NULL ) return NULL;
	HMENU clone = ::CreatePopupMenu();
	for ( int i = 0; i < ::GetMenuItemCount( source ); ++i ) {
		MENUITEMINFOA item = {};
		char text[512] = {};
		item.cbSize = sizeof( item );
		item.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_SUBMENU | MIIM_STRING;
		item.dwTypeData = text;
		item.cch = sizeof( text ) - 1;
		if ( !::GetMenuItemInfoA( source, i, TRUE, &item ) ) continue;
		if ( item.hSubMenu != NULL ) item.hSubMenu = StudioClonePopup( item.hSubMenu );
		item.dwTypeData = text;
		item.cch = static_cast<UINT>( strlen( text ) );
		::InsertMenuItemA( clone, i, TRUE, &item );
	}
	return clone;
}

void StudioAppendTopMenu( HMENU destination, HMENU source, const char *sourceCaption, const char *displayCaption ) {
	HMENU popup = StudioFindSubMenu( source, sourceCaption );
	if ( popup != NULL ) ::AppendMenuA( destination, MF_POPUP, reinterpret_cast<UINT_PTR>( StudioClonePopup( popup ) ), displayCaption );
}

HMENU StudioBuildWorldEditMenu( HMENU doomEditMenu ) {
	HMENU result = ::CreateMenu();
	StudioAppendTopMenu( result, doomEditMenu, "File", "&File" );
	StudioAppendTopMenu( result, doomEditMenu, "Edit", "&Edit" );
	StudioAppendTopMenu( result, doomEditMenu, "View", "&View" );
	StudioAppendTopMenu( result, doomEditMenu, "Selection", "&Selection" );
	StudioAppendTopMenu( result, doomEditMenu, "Bsp", "&Build" );
	StudioAppendTopMenu( result, doomEditMenu, "Grid", "&Grid" );
	HMENU misc = StudioFindSubMenu( doomEditMenu, "Misc" );
	if ( misc != NULL ) {
		HMENU miscClone = StudioClonePopup( misc );
		for ( int i = ::GetMenuItemCount( miscClone ) - 1; i >= 0; --i ) {
			if ( StudioMenuCaption( miscClone, i ).CompareNoCase( "Colors" ) == 0 ) ::DeleteMenu( miscClone, i, MF_BYPOSITION );
		}
		::AppendMenuA( result, MF_POPUP, reinterpret_cast<UINT_PTR>( miscClone ), "&Misc" );
	}
	StudioAppendTopMenu( result, misc, "Colors", "&Colors" );
	StudioAppendTopMenu( result, doomEditMenu, "Region", "&Region" );
	StudioAppendTopMenu( result, doomEditMenu, "Brush", "&Brush" );
	StudioAppendTopMenu( result, doomEditMenu, "Patch", "&Patch" );
	HMENU splineResource = ::LoadMenuA( AfxGetResourceHandle(), MAKEINTRESOURCEA( IDR_POPUP_SPLINE ) );
	if ( splineResource != NULL ) {
		HMENU spline = ::GetSubMenu( splineResource, 0 );
		if ( spline != NULL ) ::AppendMenuA( result, MF_POPUP,
			reinterpret_cast<UINT_PTR>( StudioClonePopup( spline ) ), "&Spline" );
		::DestroyMenu( splineResource );
	}
	StudioAppendTopMenu( result, doomEditMenu, "Plugins", "&Tools" );
	StudioAppendTopMenu( result, doomEditMenu, "idStudio", "&idStudio" );
	if ( ::GetMenuItemCount( result ) == 0 ) {
		::DestroyMenu( result );
		return NULL;
	}
	return result;
}
}

IMPLEMENT_DYNAMIC( idStudioPlaceholderWnd, CWnd )
IMPLEMENT_DYNAMIC( idStudioMenuBar, CWnd )
IMPLEMENT_DYNAMIC( idStudioEngineToolBar, CWnd )
IMPLEMENT_DYNAMIC( idStudioCameraControlBar, CWnd )
IMPLEMENT_DYNAMIC( idStudioWorldEditCommandBars, CWnd )
IMPLEMENT_DYNAMIC( idStudioWorldEditWnd, CWnd )
IMPLEMENT_DYNAMIC( idStudioDockPane, CWnd )
IMPLEMENT_DYNAMIC( idStudioAutoHideBar, CWnd )
IMPLEMENT_DYNAMIC( idStudioMainWnd, CWnd )
IMPLEMENT_DYNAMIC( idRadiantSplitWnd, idStudioMainWnd )

BEGIN_MESSAGE_MAP( idStudioWorldEditWnd, CWnd )
	ON_WM_SIZE()
END_MESSAGE_MAP()

idStudioWorldEditWnd::idStudioWorldEditWnd() : zWindow( NULL ), xyWindow( NULL ), showZ( true ) {}

bool idStudioWorldEditWnd::Create( CWnd *parent ) {
	return CWnd::CreateEx( 0, StudioWindowClass(), "idStudioWorldEditWnd",
		WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		CRect( 0, 0, 100, 100 ), parent, 20991 ) != FALSE;
}

void idStudioWorldEditWnd::AttachViews( CWnd *zView, CWnd *xyView ) {
	zWindow = zView;
	xyWindow = xyView;
	CWnd *views[2] = { zWindow, xyWindow };
	for ( int i = 0; i < 2; ++i ) {
		if ( views[i] == NULL || !views[i]->GetSafeHwnd() ) continue;
		views[i]->SetParent( this );
		views[i]->ModifyStyle( WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU |
			WS_MINIMIZEBOX | WS_MAXIMIZEBOX, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
			SWP_FRAMECHANGED | SWP_NOACTIVATE );
	}
	LayoutViews();
}

void idStudioWorldEditWnd::ToggleZView() {
	showZ = !showZ;
	LayoutViews();
}

void idStudioWorldEditWnd::LayoutViews() {
	if ( !GetSafeHwnd() || xyWindow == NULL || !xyWindow->GetSafeHwnd() ) return;
	CRect client;
	GetClientRect( &client );
	const int zWidth = showZ && zWindow != NULL && zWindow->GetSafeHwnd() ? StudioDpiScale( this, 54 ) : 0;
	bool changed = false;
	if ( zWindow != NULL && zWindow->GetSafeHwnd() ) {
		changed |= StudioSetWindowRect( zWindow, NULL, 0, 0, zWidth, client.Height(),
			SWP_NOACTIVATE | SWP_NOZORDER | ( showZ ? SWP_SHOWWINDOW : SWP_HIDEWINDOW ) );
		// CZWnd's renderer uses the legacy global dimensions instead of asking
		// its HWND for the current client rectangle.  Reparenting it into the
		// document composite can suppress an otherwise redundant WM_SIZE, which
		// left the GL viewport at its 100-pixel construction height.
		z.width = Max( zWidth, 10 );
		z.height = Max( client.Height(), 10 );
	}
	changed |= StudioSetWindowRect( xyWindow, NULL, zWidth, 0, client.Width() - zWidth, client.Height(),
		SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW );
	if ( changed ) {
		if ( zWindow != NULL ) zWindow->Invalidate( FALSE );
		xyWindow->Invalidate( FALSE );
	}
}

void idStudioWorldEditWnd::OnSize( UINT type, int width, int height ) {
	CWnd::OnSize( type, width, height );
	LayoutViews();
}

BEGIN_MESSAGE_MAP( idStudioMenuBar, CWnd )
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

idStudioMenuBar::idStudioMenuBar() : menuHandle( NULL ), target( NULL ) {}

int idStudioMenuBar::PreferredHeight() const { return StudioDpiScale( this, 23 ); }

bool idStudioMenuBar::Create( CWnd *parent, HMENU menu, CWnd *commandTarget ) {
	menuHandle = menu;
	target = commandTarget;
	return CWnd::CreateEx( 0, StudioWindowClass( static_cast<HBRUSH>( ::GetStockObject( WHITE_BRUSH ) ) ),
		"idStudioMenuBar", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
		CRect( 0, 0, 100, PreferredHeight() ), parent, 20990 ) != FALSE;
}

BOOL idStudioMenuBar::OnEraseBkgnd( CDC * ) { return TRUE; }

void idStudioMenuBar::OnPaint() {
	CPaintDC dc( this );
	CRect client;
	GetClientRect( &client );
	dc.FillSolidRect( client, ::GetSysColor( COLOR_MENUBAR ) );
	dc.SetBkMode( TRANSPARENT );
	dc.SetTextColor( ::GetSysColor( COLOR_MENUTEXT ) );
	itemRects.clear();
	int x = 7;
	const int count = menuHandle != NULL ? ::GetMenuItemCount( menuHandle ) : 0;
	for ( int i = 0; i < count; ++i ) {
		char text[128] = {};
		::GetMenuStringA( menuHandle, i, text, sizeof( text ), MF_BYPOSITION );
		CString label( text );
		label.Replace( "&", "" );
		if ( label.CompareNoCase( "Bsp" ) == 0 ) label = "Build";
		else if ( label.CompareNoCase( "Studio" ) == 0 ) label = "idStudio";
		CRect extent( 0, 0, 0, 0 );
		dc.DrawText( label, extent, DT_CALCRECT | DT_SINGLELINE );
		CRect item( x, 0, x + extent.Width() + 14, client.bottom );
		dc.DrawText( label, item, DT_CENTER | DT_VCENTER | DT_SINGLELINE );
		itemRects.push_back( item );
		x = item.right;
	}
}

void idStudioMenuBar::OnLButtonDown( UINT flags, CPoint point ) {
	for ( size_t i = 0; i < itemRects.size(); ++i ) {
		if ( !itemRects[i].PtInRect( point ) ) continue;
		HMENU popup = ::GetSubMenu( menuHandle, static_cast<int>( i ) );
		if ( popup == NULL || target == NULL || !target->GetSafeHwnd() ) return;
		target->SendMessage( WM_INITMENUPOPUP, reinterpret_cast<WPARAM>( popup ), MAKELPARAM( i, FALSE ) );
		CPoint screen( itemRects[i].left, itemRects[i].bottom );
		ClientToScreen( &screen );
		const UINT command = ::TrackPopupMenuEx( popup, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD,
			screen.x, screen.y, target->GetSafeHwnd(), NULL );
		if ( command != 0 ) target->SendMessage( WM_COMMAND, command, 0 );
		Invalidate( FALSE );
		return;
	}
	CWnd::OnLButtonDown( flags, point );
}

BEGIN_MESSAGE_MAP( idStudioEngineToolBar, CWnd )
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

idStudioEngineToolBar::idStudioEngineToolBar() : target( NULL ), activeCommand( ID_ENGINE_ASPECT_16_9 ) {}

bool idStudioEngineToolBar::Create( CWnd *parent, CWnd *commandTarget ) {
	target = commandTarget;
	buttonCommands.push_back( ID_ENGINE_ASPECT_FIT );
	buttonCommands.push_back( ID_ENGINE_ASPECT_4_3 );
	buttonCommands.push_back( ID_ENGINE_ASPECT_16_9 );
	buttonCommands.push_back( ID_ENGINE_ASPECT_16_10 );
	buttonCommands.push_back( ID_ENGINE_ASPECT_R_MODE );
	return CWnd::CreateEx( 0, StudioWindowClass(), "idStudioEngineToolBar",
		WS_CHILD | WS_CLIPSIBLINGS, CRect( 0, 0, 100, 28 ), parent, 20989 ) != FALSE;
}

void idStudioEngineToolBar::SetAspectCommand( UINT command ) {
	activeCommand = command;
	Invalidate( FALSE );
}

BOOL idStudioEngineToolBar::OnEraseBkgnd( CDC * ) { return TRUE; }

void idStudioEngineToolBar::OnPaint() {
	// Resource 1070 in the reference is an 80x15 strip: five compact 16-pixel
	// aspect glyphs labelled FIT, 43, 169, 1610 and RM.  Keep the same visual
	// density here instead of turning these into a generic full-width toolbar.
	static const char *labels[] = { "FIT", "43", "169", "1610", "RM" };
	CPaintDC dc( this );
	CRect client;
	GetClientRect( &client );
	dc.FillSolidRect( client, ::GetSysColor( COLOR_3DFACE ) );
	dc.SetBkMode( TRANSPARENT );
	CFont font;
	font.CreateFont( StudioDpiScale( this, 10 ), 0, 0, 0, FW_BOLD, FALSE, FALSE, 0, ANSI_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, FIXED_PITCH, "Arial" );
	CFont *oldFont = dc.SelectObject( &font );
	buttonRects.clear();
	int x = StudioDpiScale( this, 5 );
	for ( size_t i = 0; i < buttonCommands.size(); ++i ) {
		CRect button( x, StudioDpiScale( this, 6 ), x + StudioDpiScale( this, 16 ), StudioDpiScale( this, 21 ) );
		buttonRects.push_back( button );
		const bool selected = buttonCommands[i] == activeCommand;
		dc.FillSolidRect( button, RGB( 8, 16, 8 ) );
		dc.Draw3dRect( button, selected ? RGB( 208, 255, 208 ) : RGB( 62, 132, 62 ), RGB( 15, 45, 15 ) );
		dc.SetTextColor( selected ? RGB( 255, 255, 255 ) : RGB( 110, 255, 110 ) );
		dc.DrawText( labels[i], button, DT_CENTER | DT_VCENTER | DT_SINGLELINE );
		x = button.right;
	}
	if ( oldFont != NULL ) dc.SelectObject( oldFont );
}

void idStudioEngineToolBar::OnLButtonDown( UINT flags, CPoint point ) {
	for ( size_t i = 0; i < buttonRects.size(); ++i ) {
		if ( buttonRects[i].PtInRect( point ) && target != NULL && target->GetSafeHwnd() ) {
			target->SendMessage( WM_COMMAND, buttonCommands[i], 0 );
			return;
		}
	}
	CWnd::OnLButtonDown( flags, point );
}

BEGIN_MESSAGE_MAP( idStudioCameraControlBar, CWnd )
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_CBN_SELCHANGE( 20980, OnWorldChanged )
	ON_CBN_SELCHANGE( 20981, OnQualityChanged )
	ON_CBN_SELCHANGE( 20982, OnMipChanged )
	ON_BN_CLICKED( 20983, OnMipMinus )
	ON_BN_CLICKED( 20984, OnMipPlus )
END_MESSAGE_MAP()

idStudioCameraControlBar::idStudioCameraControlBar() : target( NULL ), quality( 2 ), mip( 0 ) {}

bool idStudioCameraControlBar::Create( CWnd *parent, CWnd *commandTarget ) {
	target = commandTarget;
	if ( !CWnd::CreateEx( 0, StudioWindowClass(), "idStudioCameraControlBar",
		WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, CRect( 0, 0, 100, PreferredHeight() ), parent, 20988 ) ) {
		return false;
	}
	const DWORD comboStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL;
	worldCombo.Create( comboStyle, CRect( 0, 0, 80, 200 ), this, 20980 );
	worldCombo.AddString( "World" );
	worldCombo.AddString( "Selection" );
	worldCombo.AddString( "Entity Outlines" );
	worldCombo.SetCurSel( 0 );
	qualityCombo.Create( comboStyle, CRect( 0, 0, 100, 200 ), this, 20981 );
	qualityCombo.AddString( "Quality Low" );
	qualityCombo.AddString( "Quality Medium" );
	qualityCombo.AddString( "Quality High" );
	qualityCombo.SetCurSel( quality );
	mipCombo.Create( comboStyle, CRect( 0, 0, 70, 260 ), this, 20982 );
	for ( int i = 0; i <= 12; ++i ) {
		CString text; text.Format( "Mip %d", i );
		mipCombo.AddString( text );
	}
	mipCombo.SetCurSel( mip );
	OnSize( 0, 400, PreferredHeight() );
	return true;
}

int idStudioCameraControlBar::PreferredHeight() const { return StudioDpiScale( this, 25 ); }
BOOL idStudioCameraControlBar::OnEraseBkgnd( CDC * ) { return TRUE; }

void idStudioCameraControlBar::OnPaint() {
	CPaintDC dc( this );
	CRect client; GetClientRect( &client );
	dc.FillSolidRect( client, ::GetSysColor( COLOR_3DFACE ) );
}

void idStudioCameraControlBar::OnSize( UINT type, int width, int height ) {
	CWnd::OnSize( type, width, height );
	if ( !worldCombo.GetSafeHwnd() ) return;
	const int gap = StudioDpiScale( this, 3 );
	const int y = StudioDpiScale( this, 2 );
	int x = StudioDpiScale( this, 4 );
	worldCombo.MoveWindow( x, y, StudioDpiScale( this, 78 ), StudioDpiScale( this, 220 ) ); x += StudioDpiScale( this, 78 ) + gap;
	qualityCombo.MoveWindow( x, y, StudioDpiScale( this, 108 ), StudioDpiScale( this, 220 ) ); x += StudioDpiScale( this, 108 ) + gap;
	mipCombo.MoveWindow( x, y, StudioDpiScale( this, 68 ), StudioDpiScale( this, 260 ) );
}

void idStudioCameraControlBar::ApplyWorldMode() { if ( target ) target->SendMessage( WM_COMMAND, ID_STUDIO_CAMERA_WORLD_BASE + Max( 0, worldCombo.GetCurSel() ), 0 ); }
void idStudioCameraControlBar::ApplyQuality() { quality = Max( 0, qualityCombo.GetCurSel() ); if ( target ) target->SendMessage( WM_COMMAND, ID_STUDIO_CAMERA_QUALITY_BASE + quality, 0 ); }
void idStudioCameraControlBar::ApplyMip() { mip = Max( 0, mipCombo.GetCurSel() ); if ( target ) target->SendMessage( WM_COMMAND, ID_STUDIO_CAMERA_MIP_BASE + mip, 0 ); }
void idStudioCameraControlBar::OnWorldChanged() { ApplyWorldMode(); }
void idStudioCameraControlBar::OnQualityChanged() { ApplyQuality(); }
void idStudioCameraControlBar::OnMipChanged() { ApplyMip(); }
void idStudioCameraControlBar::OnMipMinus() { mip = Max( 0, mip - 1 ); mipCombo.SetCurSel( mip ); ApplyMip(); }
void idStudioCameraControlBar::OnMipPlus() { mip = Min( 12, mip + 1 ); mipCombo.SetCurSel( mip ); ApplyMip(); }

BEGIN_MESSAGE_MAP( idStudioWorldEditCommandBars, CWnd )
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_BN_CLICKED( 20976, OnResetLayout )
END_MESSAGE_MAP()

idStudioWorldEditCommandBars::idStudioWorldEditCommandBars() : target( NULL ), menu( NULL ), fileToolbar( NULL ),
	worldEditToolbar( NULL ), supplementalToolbar( NULL ) {}

bool idStudioWorldEditCommandBars::Create( CWnd *parent, CWnd *commandTarget ) {
	target = commandTarget;
	if ( !CWnd::CreateEx( 0, StudioWindowClass(), "idStudioWorldEditCommandBars",
		WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, CRect( 0, 0, 100, PreferredHeight() ), parent, 20972 ) ) return false;
	const DWORD comboStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL;
	layoutCombo.Create( comboStyle, CRect( 0, 0, 150, 240 ), this, 20973 );
	layoutCombo.AddString( "Layout User Default" );
	layoutCombo.AddString( "Layout World Editing" );
	layoutCombo.SetCurSel( 0 );
	saveLayoutButton.Create( "Save", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		CRect( 0, 0, 42, 22 ), this, 20975 );
	// The reference disables Save until a named layout differs from its stored
	// state.  Persistence is not reconstructed yet, so reflect that state.
	saveLayoutButton.EnableWindow( FALSE );
	resetLayoutButton.Create( "Reset", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		CRect( 0, 0, 46, 22 ), this, 20976 );
	rotationEdit.Create( WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_NUMBER,
		CRect( 0, 0, 42, 22 ), this, 20977 );
	rotationEdit.SetWindowText( "90" );
	return true;
}

bool idStudioWorldEditCommandBars::Attach( idStudioMenuBar *menuBar, idStudioToolBar *fileBar,
	idStudioToolBar *worldEditBar, idStudioToolBar *supplementalBar ) {
	if ( menuBar == NULL || fileBar == NULL || worldEditBar == NULL || supplementalBar == NULL ) return false;
	menu = menuBar;
	fileToolbar = fileBar;
	worldEditToolbar = worldEditBar;
	supplementalToolbar = supplementalBar;
	CWnd *bars[3] = { fileToolbar, worldEditToolbar, supplementalToolbar };
	for ( int i = 0; i < 3; ++i ) {
		bars[i]->SetParent( this );
		bars[i]->SetOwner( target );
		bars[i]->ModifyStyle( CBRS_GRIPPER | CBRS_FLOAT_MULTI | CBRS_SIZE_DYNAMIC | CBRS_ORIENT_VERT,
			WS_CHILD | WS_CLIPSIBLINGS | CBRS_ORIENT_HORZ, SWP_FRAMECHANGED | SWP_NOACTIVATE );
		static_cast<idStudioToolBar *>( bars[i] )->AdjustLayout();
	}
	menu->SetParent( this );
	LayoutBands();
	return true;
}

int idStudioWorldEditCommandBars::PreferredHeight() const {
	const int menuHeight = menu != NULL ? menu->PreferredHeight() : StudioDpiScale( this, 24 );
	int toolbarHeight = StudioDpiScale( this, 25 );
	if ( fileToolbar != NULL && fileToolbar->GetSafeHwnd() ) toolbarHeight = Max( toolbarHeight,
		static_cast<int>( fileToolbar->CalcFixedLayout( FALSE, TRUE ).cy ) );
	return menuHeight + toolbarHeight * 2;
}

void idStudioWorldEditCommandBars::LayoutBands() {
	if ( !GetSafeHwnd() ) return;
	CRect client; GetClientRect( &client );
	const int menuHeight = menu != NULL ? menu->PreferredHeight() : StudioDpiScale( this, 24 );
	const int rowHeight = Max( StudioDpiScale( this, 25 ), ( client.Height() - menuHeight ) / 2 );
	if ( menu != NULL && menu->GetSafeHwnd() ) StudioSetWindowRect( menu, NULL, 0, 0, client.Width(), menuHeight,
		SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW );
	int fileWidth = 0;
	if ( fileToolbar != NULL && fileToolbar->GetSafeHwnd() ) {
		fileWidth = Min( client.Width(), static_cast<int>( fileToolbar->CalcFixedLayout( FALSE, TRUE ).cx ) );
		StudioSetWindowRect( fileToolbar, NULL, 0, menuHeight, fileWidth, rowHeight,
			SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW );
	}
	if ( worldEditToolbar != NULL && worldEditToolbar->GetSafeHwnd() ) {
		const int remainingWidth = static_cast<int>( client.Width() ) - fileWidth;
		StudioSetWindowRect( worldEditToolbar, NULL, fileWidth, menuHeight, Max( 0, remainingWidth ), rowHeight,
			SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW );
	}
	const int secondTop = menuHeight + rowHeight;
	const int gap = StudioDpiScale( this, 3 );
	const int controlHeight = StudioDpiScale( this, 22 );
	const int y = secondTop + Max( 0, ( rowHeight - controlHeight ) / 2 );
	const int rotationWidth = StudioDpiScale( this, 42 );
	const int resetWidth = StudioDpiScale( this, 48 );
	const int saveWidth = StudioDpiScale( this, 42 );
	const int layoutWidth = StudioDpiScale( this, 150 );
	const int controlsX = static_cast<int>( client.right ) - rotationWidth - resetWidth - saveWidth - layoutWidth - gap * 4;
	int controlsLeft = Max( 0, controlsX );
	if ( supplementalToolbar != NULL && supplementalToolbar->GetSafeHwnd() ) {
		StudioSetWindowRect( supplementalToolbar, NULL, 0, secondTop, controlsLeft, rowHeight,
			SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW );
	}
	int x = controlsLeft + gap;
	if ( layoutCombo.GetSafeHwnd() ) layoutCombo.MoveWindow( x, y, layoutWidth, StudioDpiScale( this, 220 ) );
	x += layoutWidth + gap;
	if ( saveLayoutButton.GetSafeHwnd() ) saveLayoutButton.MoveWindow( x, y, saveWidth, controlHeight );
	x += saveWidth + gap;
	if ( resetLayoutButton.GetSafeHwnd() ) resetLayoutButton.MoveWindow( x, y, resetWidth, controlHeight );
	x += resetWidth + gap;
	if ( rotationEdit.GetSafeHwnd() ) rotationEdit.MoveWindow( x, y, rotationWidth, controlHeight );
}

void idStudioWorldEditCommandBars::OnSize( UINT type, int width, int height ) {
	CWnd::OnSize( type, width, height );
	LayoutBands();
}

BOOL idStudioWorldEditCommandBars::OnEraseBkgnd( CDC * ) { return TRUE; }
void idStudioWorldEditCommandBars::OnPaint() {
	CPaintDC dc( this ); CRect client; GetClientRect( &client );
	dc.FillSolidRect( client, ::GetSysColor( COLOR_3DFACE ) );
}

void idStudioWorldEditCommandBars::OnResetLayout() {
	if ( target != NULL && target->GetSafeHwnd() ) target->SendMessage( WM_COMMAND, ID_STUDIO_RESETLAYOUT, 0 );
}

BEGIN_MESSAGE_MAP( idStudioPlaceholderWnd, CWnd )
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

bool idStudioPlaceholderWnd::Create( CWnd *parent, const char *text ) {
	label = text;
	return CWnd::CreateEx( 0, StudioWindowClass( static_cast<HBRUSH>( ::GetStockObject( BLACK_BRUSH ) ) ), text,
		WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, CRect( 0, 0, 64, 64 ), parent, 0 ) != FALSE;
}

BOOL idStudioPlaceholderWnd::OnEraseBkgnd( CDC * ) { return TRUE; }

void idStudioPlaceholderWnd::OnPaint() {
	CPaintDC dc( this );
	CRect rect;
	GetClientRect( &rect );
	dc.FillSolidRect( rect, RGB( 48, 48, 48 ) );
	dc.SetBkMode( TRANSPARENT );
	dc.SetTextColor( RGB( 150, 150, 150 ) );
	dc.DrawText( label, rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE );
}

BEGIN_MESSAGE_MAP( idStudioDockPane, CWnd )
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_NOTIFY( TCN_SELCHANGE, 1, OnTabChanged )
END_MESSAGE_MAP()

idStudioDockPane::idStudioDockPane() : showCaption( false ), tabsTop( false ), showTabs( true ), activePane( -1 ) {}

bool idStudioDockPane::Create( CWnd *parent, UINT controlId, bool caption, bool topTabs ) {
	showCaption = caption;
	tabsTop = topTabs;
	if ( !CWnd::CreateEx( WS_EX_CONTROLPARENT, StudioWindowClass(), "idStudioDockPane",
		WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		CRect( 0, 0, 100, 100 ), parent, controlId ) ) return false;
	return tabs.Create( WS_CHILD | WS_VISIBLE | TCS_TOOLTIPS | TCS_FOCUSNEVER |
		( tabsTop ? 0 : TCS_BOTTOM ), CRect( 0, 0, 100, StudioDpiScale( this, TAB_HEIGHT ) ), this, 1 ) != FALSE;
}

void idStudioDockPane::SetTabsVisible( bool visible ) {
	showTabs = visible;
	if ( tabs.GetSafeHwnd() ) tabs.ShowWindow( visible ? SW_SHOWNA : SW_HIDE );
	LayoutPane();
}

void idStudioDockPane::AddPane( int id, const char *title, CWnd *window ) {
	if ( window == NULL || !window->GetSafeHwnd() ) return;
	window->SetParent( this );
	window->ModifyStyle( WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU,
		WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, SWP_FRAMECHANGED | SWP_NOACTIVATE );
	window->ModifyStyleEx( WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, SWP_FRAMECHANGED | SWP_NOACTIVATE );
	panes.push_back( pane_t( id, title, window ) );
	TCITEM item = {};
	item.mask = TCIF_TEXT | TCIF_PARAM;
	item.pszText = const_cast<char *>( title );
	item.lParam = id;
	tabs.InsertItem( static_cast<int>( panes.size() - 1 ), &item );
	window->ShowWindow( SW_HIDE );
	if ( activePane < 0 ) SelectPane( id );
}

bool idStudioDockPane::ContainsPane( int id ) const {
	for ( size_t i = 0; i < panes.size(); ++i ) if ( panes[i].id == id ) return true;
	return false;
}

bool idStudioDockPane::SelectPane( int id ) {
	bool found = false;
	for ( size_t i = 0; i < panes.size(); ++i ) {
		const bool selected = panes[i].id == id;
		// Hide inactive surfaces immediately.  The selected surface is shown by
		// LayoutPane only after it has its final client rectangle; this matters
		// for the real engine HWND because showing it at its old top-level size
		// can dispatch a render/resize cycle in the middle of a tab change.
		if ( !selected ) panes[i].window->ShowWindow( SW_HIDE );
		if ( selected ) {
			found = true;
			activePane = id;
			tabs.SetCurSel( static_cast<int>( i ) );
		}
	}
	if ( found ) { LayoutPane(); Invalidate( FALSE ); }
	return found;
}

void idStudioDockPane::SetPaneChrome( int paneId, CWnd *chrome1, CWnd *chrome2, CWnd *chrome3 ) {
	for ( size_t i = 0; i < panes.size(); ++i ) if ( panes[i].id == paneId ) {
		panes[i].chromeWindow1 = chrome1;
		panes[i].chromeWindow2 = chrome2;
		panes[i].chromeWindow3 = chrome3;
	}
	LayoutPane();
}

void idStudioDockPane::SetPaneAspectRatio( int paneId, float aspectRatio ) {
	for ( size_t i = 0; i < panes.size(); ++i ) if ( panes[i].id == paneId ) panes[i].aspectRatio = aspectRatio;
	LayoutPane();
	Invalidate( TRUE );
}

bool idStudioDockPane::IsPaneLayoutCurrent( int paneId ) const {
	if ( !GetSafeHwnd() || !tabs.GetSafeHwnd() ) return false;
	for ( size_t i = 0; i < panes.size(); ++i ) {
		if ( panes[i].id != paneId || panes[i].window == NULL || !panes[i].window->GetSafeHwnd() ) continue;
		CRect client;
		GetClientRect( &client );
		const int captionHeight = StudioDpiScale( this, CAPTION_HEIGHT );
		const int tabHeight = StudioDpiScale( this, TAB_HEIGHT );
		int top = showCaption ? captionHeight : 0;
		int bottom = client.bottom;
		if ( showTabs ) {
			if ( tabsTop ) top += tabHeight; else bottom -= tabHeight;
		}
		top += StudioChromeHeight( panes[i].chromeWindow1, this );
		top += StudioChromeHeight( panes[i].chromeWindow2, this );
		top += StudioChromeHeight( panes[i].chromeWindow3, this );
		CRect expected( 0, top, client.Width(), bottom );
		if ( panes[i].aspectRatio > 0.0f && expected.Width() > 0 && expected.Height() > 0 ) {
			const float availableAspect = static_cast<float>( expected.Width() ) / expected.Height();
			if ( availableAspect > panes[i].aspectRatio ) {
				const int width = static_cast<int>( expected.Height() * panes[i].aspectRatio );
				expected.left += ( expected.Width() - width ) / 2;
				expected.right = expected.left + width;
			} else {
				const int height = static_cast<int>( expected.Width() / panes[i].aspectRatio );
				expected.top += ( expected.Height() - height ) / 2;
				expected.bottom = expected.top + height;
			}
		}
		CRect actual;
		panes[i].window->GetWindowRect( &actual );
		const_cast<idStudioDockPane *>( this )->ScreenToClient( &actual );
		return abs( actual.left - expected.left ) <= 1 && abs( actual.top - expected.top ) <= 1 &&
			abs( actual.right - expected.right ) <= 1 && abs( actual.bottom - expected.bottom ) <= 1;
	}
	return false;
}

void idStudioDockPane::LayoutPane() {
	// CreateWindow sends WM_SIZE before Create() has finished constructing the
	// tab control.  MFC's CWnd::SetWindowPos asserts when called on that
	// temporarily unbound CWnd, so do not lay out a half-created pane.
	if ( !GetSafeHwnd() || !tabs.GetSafeHwnd() ) return;
	CRect client;
	GetClientRect( &client );
	const int captionHeight = StudioDpiScale( this, CAPTION_HEIGHT );
	const int tabHeight = StudioDpiScale( this, TAB_HEIGHT );
	int top = showCaption ? captionHeight : 0;
	int bottom = client.bottom;
	if ( showTabs && tabsTop ) {
		StudioSetWindowRect( &tabs, NULL, 0, top, client.Width(), tabHeight, SWP_NOACTIVATE | SWP_NOZORDER );
		top += tabHeight;
	} else if ( showTabs ) {
		bottom -= tabHeight;
		StudioSetWindowRect( &tabs, NULL, 0, bottom, client.Width(), tabHeight, SWP_NOACTIVATE | SWP_NOZORDER );
	}
	CWnd *chromeWindows[3] = { NULL, NULL, NULL };
	for ( size_t i = 0; i < panes.size(); ++i ) {
		CWnd *paneChrome[3] = { panes[i].chromeWindow1, panes[i].chromeWindow2, panes[i].chromeWindow3 };
		for ( int c = 0; c < 3; ++c ) if ( paneChrome[c] != NULL && paneChrome[c]->GetSafeHwnd() ) paneChrome[c]->ShowWindow( SW_HIDE );
		if ( panes[i].id == activePane ) {
			for ( int c = 0; c < 3; ++c ) chromeWindows[c] = paneChrome[c];
		}
	}
	for ( int c = 0; c < 3; ++c ) if ( chromeWindows[c] != NULL && chromeWindows[c]->GetSafeHwnd() ) {
		const int chromeHeight = StudioChromeHeight( chromeWindows[c], this );
		StudioSetWindowRect( chromeWindows[c], NULL, 0, top, client.Width(), chromeHeight,
			SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW );
		top += chromeHeight;
	}
	for ( size_t i = 0; i < panes.size(); ++i ) if ( panes[i].id == activePane &&
		panes[i].window != NULL && panes[i].window->GetSafeHwnd() != NULL ) {
		int paneX = 0;
		int paneY = top;
		int paneWidth = client.Width();
		int paneHeight = bottom - top;
		if ( panes[i].aspectRatio > 0.0f && paneWidth > 0 && paneHeight > 0 ) {
			const float availableAspect = static_cast<float>( paneWidth ) / paneHeight;
			if ( availableAspect > panes[i].aspectRatio ) {
				const int width = static_cast<int>( paneHeight * panes[i].aspectRatio );
				paneX += ( paneWidth - width ) / 2;
				paneWidth = width;
			} else {
				const int height = static_cast<int>( paneWidth / panes[i].aspectRatio );
				paneY += ( paneHeight - height ) / 2;
				paneHeight = height;
			}
		}
		StudioSetWindowRect( panes[i].window, NULL, paneX, paneY, paneWidth, paneHeight,
			SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW );
	}
}

void idStudioDockPane::OnSize( UINT type, int width, int height ) { CWnd::OnSize( type, width, height ); LayoutPane(); }
BOOL idStudioDockPane::OnEraseBkgnd( CDC * ) { return TRUE; }

void idStudioDockPane::OnPaint() {
	CPaintDC dc( this );
	CRect client;
	GetClientRect( &client );
	dc.FillSolidRect( client, RGB( 55, 55, 55 ) );
	if ( showCaption ) {
		CRect caption( 0, 0, client.right, StudioDpiScale( this, CAPTION_HEIGHT ) );
		dc.FillSolidRect( caption, RGB( 196, 216, 232 ) );
		dc.SetBkMode( TRANSPARENT );
		dc.SetTextColor( RGB( 25, 25, 25 ) );
		CString title = "Tool Window";
		for ( size_t i = 0; i < panes.size(); ++i ) if ( panes[i].id == activePane ) title = panes[i].title;
		caption.left += 7;
		dc.DrawText( title, caption, DT_LEFT | DT_VCENTER | DT_SINGLELINE );
		// Codejock's CXTPDockingPaneCaptionButtons are paint-manager glyphs,
		// not bitmap resources. Draw the same two actions: auto-hide pin and close.
		const int button = StudioDpiScale( this, 22 );
		const int cy = caption.CenterPoint().y;
		CPen pen( PS_SOLID, Max( 1, StudioDpiScale( this, 1 ) ), RGB( 35, 35, 35 ) );
		CPen *oldPen = dc.SelectObject( &pen );
		const int pinX = caption.right - button - button / 2;
		dc.MoveTo( pinX - 4, cy - 4 ); dc.LineTo( pinX + 4, cy - 4 );
		dc.MoveTo( pinX - 2, cy - 4 ); dc.LineTo( pinX - 2, cy + 1 );
		dc.LineTo( pinX + 2, cy + 1 ); dc.LineTo( pinX + 2, cy - 4 );
		dc.MoveTo( pinX, cy + 1 ); dc.LineTo( pinX, cy + 6 );
		const int closeX = caption.right - button / 2;
		dc.MoveTo( closeX - 4, cy - 4 ); dc.LineTo( closeX + 4, cy + 4 );
		dc.MoveTo( closeX + 4, cy - 4 ); dc.LineTo( closeX - 4, cy + 4 );
		if ( oldPen != NULL ) dc.SelectObject( oldPen );
	}
}

void idStudioDockPane::OnLButtonDown( UINT flags, CPoint point ) {
	if ( showCaption ) {
		CRect client; GetClientRect( &client );
		const int captionHeight = StudioDpiScale( this, CAPTION_HEIGHT );
		const int button = StudioDpiScale( this, 22 );
		if ( point.y >= 0 && point.y < captionHeight && point.x >= client.right - button * 2 ) {
			CWnd *owner = GetParent();
			if ( owner != NULL && owner->GetSafeHwnd() ) owner->SendMessage( WM_IDSTUDIO_FLYOUT_COMMAND,
				point.x >= client.right - button ? IDSTUDIO_FLYOUT_CLOSE : IDSTUDIO_FLYOUT_PIN, 0 );
			return;
		}
	}
	CWnd::OnLButtonDown( flags, point );
}

void idStudioDockPane::OnTabChanged( NMHDR *, LRESULT *result ) {
	const int index = tabs.GetCurSel();
	if ( index >= 0 && index < static_cast<int>( panes.size() ) ) SelectPane( panes[index].id );
	if ( result != NULL ) *result = 0;
}

BOOL idStudioDockPane::OnNotify( WPARAM wParam, LPARAM lParam, LRESULT *result ) {
	NMHDR *header = reinterpret_cast<NMHDR *>( lParam );
	if ( header != NULL && header->hwndFrom == tabs.GetSafeHwnd() &&
		( header->code == TCN_SELCHANGE || header->code == NM_CLICK ) ) {
		const int index = tabs.GetCurSel();
		if ( index >= 0 && index < static_cast<int>( panes.size() ) ) {
			SelectPane( panes[index].id );
			if ( result != NULL ) *result = 0;
			return TRUE;
		}
	}
	return CWnd::OnNotify( wParam, lParam, result );
}

BEGIN_MESSAGE_MAP( idStudioAutoHideBar, CWnd )
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_TIMER()
END_MESSAGE_MAP()

idStudioAutoHideBar::idStudioAutoHideBar() : pinned( false ), outsideSince( 0 ) {}

bool idStudioAutoHideBar::Create( CWnd *parent, UINT controlId ) {
	if ( !CWnd::CreateEx( 0, StudioWindowClass(), "idStudioAutoHideBar",
		WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		CRect( 0, 0, StudioDpiScale( parent, AUTOHIDE_WIDTH ), 100 ), parent, controlId ) ) return false;
	if ( !flyout.Create( parent, controlId + 1, true, false ) ) return false;
	flyout.SetTabsVisible( false );
	flyout.ShowWindow( SW_HIDE );
	return true;
}

void idStudioAutoHideBar::AddPane( int id, const char *title, CWnd *window ) {
	paneTabs.push_back( tab_t( id, title ) );
	flyout.AddPane( id, title, window );
	Invalidate( FALSE );
}

bool idStudioAutoHideBar::ShowPane( int id ) {
	if ( !flyout.SelectPane( id ) ) return false;
	flyout.ShowWindow( SW_SHOW );
	flyout.BringWindowToTop();
	outsideSince = 0;
	SetTimer( 1, 100, NULL );
	Invalidate( FALSE );
	return true;
}

void idStudioAutoHideBar::HidePane() {
	pinned = false;
	outsideSince = 0;
	KillTimer( 1 );
	flyout.ShowWindow( SW_HIDE );
	Invalidate( FALSE );
}
bool idStudioAutoHideBar::IsPaneOpen() const { return flyout.GetSafeHwnd() && flyout.IsWindowVisible(); }
int idStudioAutoHideBar::ActivePane() const { return flyout.ActivePane(); }

void idStudioAutoHideBar::TogglePinned() {
	if ( !IsPaneOpen() ) return;
	pinned = !pinned;
	outsideSince = 0;
	if ( pinned ) KillTimer( 1 ); else SetTimer( 1, 100, NULL );
	Invalidate( FALSE );
}

void idStudioAutoHideBar::LayoutFlyout( const CRect &workspace, int width ) {
	if ( !GetSafeHwnd() || !flyout.GetSafeHwnd() ) return;
	const int x = workspace.right - StudioDpiScale( this, AUTOHIDE_WIDTH ) - width;
	StudioSetWindowRect( &flyout, &wndTop, x, workspace.top, width, workspace.Height(),
		SWP_NOACTIVATE | ( IsPaneOpen() ? SWP_SHOWWINDOW : 0 ) );
}

BOOL idStudioAutoHideBar::OnEraseBkgnd( CDC * ) { return TRUE; }

CRect idStudioAutoHideBar::TabRect( int index, CDC * ) const {
	int top = 0;
	for ( int i = 0; i < index && i < static_cast<int>( paneTabs.size() ); ++i ) {
		top += StudioDpiScale( this, Max( 48, paneTabs[i].title.GetLength() * 7 + 18 ) );
	}
	const int height = index >= 0 && index < static_cast<int>( paneTabs.size() ) ?
		StudioDpiScale( this, Max( 48, paneTabs[index].title.GetLength() * 7 + 18 ) ) : 0;
	CRect client; const_cast<idStudioAutoHideBar *>( this )->GetClientRect( &client );
	return CRect( 0, top, client.right, top + height );
}

void idStudioAutoHideBar::OnPaint() {
	CPaintDC dc( this );
	CRect client;
	GetClientRect( &client );
	dc.FillSolidRect( client, RGB( 225, 225, 225 ) );
	CFont font;
	font.CreateFont( StudioDpiScale( this, 13 ), 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial" );
	CFont *oldFont = dc.SelectObject( &font );
	dc.SetBkMode( TRANSPARENT );
	dc.SetTextColor( RGB( 32, 32, 32 ) );
	for ( size_t i = 0; i < paneTabs.size(); ++i ) {
		CRect tab = TabRect( static_cast<int>( i ), &dc );
		dc.FillSolidRect( tab, flyout.ActivePane() == paneTabs[i].id && IsPaneOpen() ? RGB( 245, 177, 77 ) : RGB( 232, 232, 232 ) );
		dc.Draw3dRect( tab, RGB( 180, 180, 180 ), RGB( 245, 245, 245 ) );

		// Do not rely on a rotated font's baseline direction here.  MFC/GDI
		// disagree about the ink box returned for escapement fonts at some DPI
		// settings, which placed the idStudio tab captions outside this narrow
		// window.  Rotate the coordinate system instead, then center the normal
		// horizontal text by its measured ink extent.  The resulting caption
		// reads from the top of the right edge toward the bottom, as in idStudio.
		const CSize text = dc.GetTextExtent( paneTabs[i].title );
		const int saved = dc.SaveDC();
		dc.IntersectClipRect( tab );
		::SetGraphicsMode( dc.GetSafeHdc(), GM_ADVANCED );
		XFORM transform = {};
		transform.eM11 = 0.0f;
		transform.eM12 = 1.0f;
		transform.eM21 = -1.0f;
		transform.eM22 = 0.0f;
		transform.eDx = static_cast<FLOAT>( tab.CenterPoint().x + text.cy / 2 );
		const int centeredY = ( static_cast<int>( tab.Height() ) - text.cx ) / 2;
		transform.eDy = static_cast<FLOAT>( tab.top + Max( 0, centeredY ) );
		if ( ::SetWorldTransform( dc.GetSafeHdc(), &transform ) ) {
			dc.TextOut( 0, 0, paneTabs[i].title );
		}
		dc.RestoreDC( saved );
	}
	if ( oldFont != NULL ) dc.SelectObject( oldFont );
}

void idStudioAutoHideBar::OnLButtonDown( UINT flags, CPoint point ) {
	int index = -1;
	for ( int i = 0; i < static_cast<int>( paneTabs.size() ); ++i ) if ( TabRect( i ).PtInRect( point ) ) { index = i; break; }
	if ( index >= 0 ) {
		if ( IsPaneOpen() && ActivePane() == paneTabs[index].id ) HidePane();
		else ShowPane( paneTabs[index].id );
		// An opened idStudio tool pane is docked workspace, not a translucent
		// overlay over the active document.  Ask the workspace to recompute its
		// document edge immediately after a vertical tab is toggled.
		CWnd *parent = GetParent();
		if ( parent != NULL && parent->GetSafeHwnd() ) {
			CRect parentClient;
			parent->GetClientRect( &parentClient );
			parent->SendMessage( WM_SIZE, SIZE_RESTORED,
				MAKELPARAM( parentClient.Width(), parentClient.Height() ) );
		}
	} else CWnd::OnLButtonDown( flags, point );
}

void idStudioAutoHideBar::OnMouseMove( UINT flags, CPoint point ) {
	outsideSince = 0;
	CWnd::OnMouseMove( flags, point );
}

void idStudioAutoHideBar::OnTimer( UINT_PTR timerId ) {
	if ( timerId != 1 || pinned || !IsPaneOpen() ) { CWnd::OnTimer( timerId ); return; }
	POINT cursor = {}; ::GetCursorPos( &cursor );
	CRect barRect, flyoutRect;
	GetWindowRect( &barRect ); flyout.GetWindowRect( &flyoutRect );
	HWND focus = ::GetFocus();
	const bool focusInside = focus != NULL && ( focus == flyout.GetSafeHwnd() || ::IsChild( flyout.GetSafeHwnd(), focus ) );
	if ( barRect.PtInRect( cursor ) || flyoutRect.PtInRect( cursor ) || focusInside ) {
		outsideSince = 0;
		return;
	}
	const DWORD now = ::GetTickCount();
	if ( outsideSince == 0 ) outsideSince = now;
	else if ( now - outsideSince >= 650 ) {
		HidePane();
		CWnd *parent = GetParent();
		if ( parent != NULL && parent->GetSafeHwnd() ) parent->SendMessage( WM_SIZE );
	}
}

BEGIN_MESSAGE_MAP( idStudioMainWnd, CWnd )
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_SETCURSOR()
	ON_MESSAGE( WM_IDSTUDIO_FLYOUT_COMMAND, OnFlyoutCommand )
	ON_COMMAND_RANGE( ID_ENGINE_ASPECT_FIT, ID_ENGINE_ASPECT_16_10, OnEngineAspect )
	ON_COMMAND_RANGE( ID_ENGINE_ASPECT_R_MODE, ID_ENGINE_ASPECT_R_MODE, OnEngineAspect )
	ON_COMMAND( ID_ENGINE_EXIT, OnEngineExit )
END_MESSAGE_MAP()

idStudioMainWnd::idStudioMainWnd() : zView( NULL ), xyView( NULL ), cameraView( NULL ), inspectors( NULL ),
	engineOriginalParent( NULL ), engineOriginalStyle( 0 ), engineOriginalExStyle( 0 ), documentMenuHandle( NULL ), engineMenuHandle( NULL ),
	engineAspectCommand( ID_ENGINE_ASPECT_16_9 ), engineSourceAspect( 16.0f / 9.0f ), leftWidth( 410 ),
	cameraFraction( 0.49f ), rightFlyoutWidth( 430 ), dragSplitter( 0 ) {}

idStudioMainWnd::~idStudioMainWnd() {
	if ( documentMenuHandle != NULL ) {
		::DestroyMenu( documentMenuHandle );
		documentMenuHandle = NULL;
	}
	if ( engineMenuHandle != NULL ) {
		::DestroyMenu( engineMenuHandle );
		engineMenuHandle = NULL;
	}
}

bool idStudioMainWnd::Create( CWnd *parent ) {
	if ( !CWnd::CreateEx( 0, StudioWindowClass(), "idStudioMainWnd",
		WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		CRect( 0, 0, 1280, 720 ), parent, AFX_IDW_PANE_FIRST ) ) return false;
	if ( !cameraDock.Create( this, 21000, true, false ) || !browserDock.Create( this, 21010, true, false ) ||
		!documentDock.Create( this, 21020, false, true ) || !autoHideBar.Create( this, 21030 ) ) return false;
	if ( !previewWnd.Create( &cameraDock, "Preview Window" ) || !engineWnd.Create( &documentDock, "Engine View" ) ||
		!worldEditWnd.Create( &documentDock ) ) return false;
	leftWidth = StudioDpiScale( this, idStudio_leftDockWidth.GetInteger() );
	cameraFraction = idStudio_cameraDockFraction.GetFloat();
	rightFlyoutWidth = StudioDpiScale( this, idStudio_rightFlyoutWidth.GetInteger() );
	return true;
}

bool idStudioMainWnd::AttachEngineView( HWND engineWindow ) {
	if ( engineWindow == NULL || !::IsWindow( engineWindow ) || !documentDock.GetSafeHwnd() ) return false;
	const bool alreadyAttached = engineView.GetSafeHwnd() == engineWindow;
	const LONG_PTR currentStyle = ::GetWindowLongPtr( engineWindow, GWL_STYLE );
	const LONG_PTR currentExStyle = ::GetWindowLongPtr( engineWindow, GWL_EXSTYLE );
	const bool dockStateValid = alreadyAttached &&
		::GetAncestor( engineWindow, GA_PARENT ) == documentDock.GetSafeHwnd() &&
		( currentStyle & WS_CHILD ) != 0 &&
		( currentStyle & ( WS_POPUP | WS_CAPTION | WS_THICKFRAME ) ) == 0;
	if ( dockStateValid ) {
		// R_SetNewMode/GLimp_SetScreenParms may resize the render HWND without
		// changing its already-correct child style or parent.  That previously
		// bypassed our repair path and allowed a video-mode-sized child (often
		// DPI-scaled as well) to spill far beyond the Engine document.  Compare
		// physical HWND coordinates in the common parent coordinate space and
		// re-run the document layout only when the renderer disturbed them.
		if ( !documentDock.IsPaneLayoutCurrent( STUDIO_PANE_ENGINE ) ) documentDock.LayoutPane();
		return true;
	}

	if ( !alreadyAttached ) {
		if ( engineView.GetSafeHwnd() != NULL ) engineView.Detach();
		engineOriginalParent = ::GetParent( engineWindow );
		engineOriginalStyle = currentStyle;
		engineOriginalExStyle = currentExStyle;
		RECT sourceClient = {};
		if ( ::GetClientRect( engineWindow, &sourceClient ) && sourceClient.right > 0 && sourceClient.bottom > 0 ) {
			engineSourceAspect = static_cast<float>( sourceClient.right ) / sourceClient.bottom;
		}
	}

	// GLimp_SetScreenParms can restore WINDOW_STYLE after the editor command
	// has run (for example when a latched video cvar settles).  Reassert the
	// child relationship and style whenever that happens.
	::ShowWindow( engineWindow, SW_HIDE );
	::SetParent( engineWindow, documentDock.GetSafeHwnd() );
	if ( ::GetAncestor( engineWindow, GA_PARENT ) != documentDock.GetSafeHwnd() ) {
		common->Warning( "idStudio could not dock the engine render window" );
		return false;
	}
	::SetWindowLongPtr( engineWindow, GWL_STYLE,
		( currentStyle & ~( WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX ) ) |
		WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS );
	::SetWindowLongPtr( engineWindow, GWL_EXSTYLE, currentExStyle & ~( WS_EX_APPWINDOW | WS_EX_TOOLWINDOW | WS_EX_TOPMOST ) );
	::SetWindowPos( engineWindow, NULL, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED );
	if ( !alreadyAttached && !engineView.Attach( engineWindow ) ) return false;
	documentDock.LayoutPane();
	common->Printf( alreadyAttached ? "idStudio repaired engine HWND dock state\n" :
		"idStudio docked engine HWND %p\n", engineWindow );
	return true;
}

void idStudioMainWnd::DetachEngineView() {
	HWND window = engineView.GetSafeHwnd();
	if ( window == NULL ) return;
	::ShowWindow( window, SW_HIDE );
	engineView.Detach();
	::SetParent( window, engineOriginalParent );
	::SetWindowLongPtr( window, GWL_STYLE, engineOriginalStyle );
	::SetWindowLongPtr( window, GWL_EXSTYLE, engineOriginalExStyle );
	::SetWindowPos( window, NULL, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED );
}

void idStudioMainWnd::AttachViews( CWnd *z, CWnd *xy, CWnd *camera ) {
	zView = z; xyView = xy; cameraView = camera;
	worldEditWnd.AttachViews( zView, xyView );
	cameraDock.AddPane( STUDIO_PANE_PREVIEW, "Preview", &previewWnd );
	cameraDock.AddPane( STUDIO_PANE_WORLD_CAMERA, "World Camera", cameraView );
	cameraDock.SelectPane( STUDIO_PANE_WORLD_CAMERA );
	documentDock.AddPane( STUDIO_PANE_ENGINE, "Engine",
		engineView.GetSafeHwnd() != NULL ? &engineView : static_cast<CWnd *>( &engineWnd ) );
	documentDock.AddPane( STUDIO_PANE_WORLD_EDIT, "World Edit", &worldEditWnd );
	documentDock.SelectPane( STUDIO_PANE_WORLD_EDIT );
	LayoutViews();
}

void idStudioMainWnd::AttachInspector( CWnd *inspector ) {
	inspectors = static_cast<CInspectorDialog *>( inspector );
	if ( inspectors == NULL || !inspectors->GetSafeHwnd() ) return;
	inspectors->ShowWindow( SW_HIDE );
	browserDock.AddPane( STUDIO_PANE_MEDIA_BROWSER, "Media Browser", &inspectors->mediaDlg );
	browserDock.AddPane( STUDIO_PANE_MATERIAL_BROWSER, "Material Browser", &inspectors->texWnd );
	browserDock.AddPane( STUDIO_PANE_MAP_STRUCTURE, "Layers / Groups", &inspectors->workspaceWnd );
	browserDock.SelectPane( STUDIO_PANE_MEDIA_BROWSER );
	autoHideBar.AddPane( STUDIO_PANE_CONSOLE, "Console", &inspectors->consoleWnd );
	autoHideBar.AddPane( STUDIO_PANE_ENTITY_INSPECTOR, "Entity Inspector", &inspectors->entityDlg );
	autoHideBar.AddPane( STUDIO_PANE_SURFACE_INSPECTOR, "Surface Inspector", inspectors->surfaceDlg );
	LayoutViews();
}

bool idStudioMainWnd::AttachDocumentChrome( HMENU menu, idStudioToolBar *fileToolbar,
	idStudioToolBar *worldEditToolbar, idStudioToolBar *supplementalToolbar, CWnd *commandTarget ) {
	if ( menu == NULL || fileToolbar == NULL || worldEditToolbar == NULL || supplementalToolbar == NULL ||
		!fileToolbar->GetSafeHwnd() || !worldEditToolbar->GetSafeHwnd() || !supplementalToolbar->GetSafeHwnd() ) return false;
	if ( !worldEditCommandBars.GetSafeHwnd() && !worldEditCommandBars.Create( &documentDock, commandTarget ) ) return false;
	if ( documentMenuHandle == NULL ) documentMenuHandle = StudioBuildWorldEditMenu( menu );
	HMENU activeMenu = documentMenuHandle != NULL ? documentMenuHandle : menu;
	if ( !documentMenu.GetSafeHwnd() && !documentMenu.Create( &worldEditCommandBars, activeMenu, commandTarget ) ) return false;
	if ( !worldEditCommandBars.Attach( &documentMenu, fileToolbar, worldEditToolbar, supplementalToolbar ) ) return false;
	documentDock.SetPaneChrome( STUDIO_PANE_WORLD_EDIT, &worldEditCommandBars, NULL, NULL );
	if ( engineMenuHandle == NULL ) {
		engineMenuHandle = ::CreateMenu();
		HMENU fileMenu = ::CreatePopupMenu();
		::AppendMenuA( fileMenu, MF_STRING, ID_ENGINE_EXIT, "Exit idStudio" );
		::AppendMenuA( engineMenuHandle, MF_POPUP, reinterpret_cast<UINT_PTR>( fileMenu ), "&File" );
		HMENU viewMenu = ::CreatePopupMenu();
		HMENU aspectMenu = ::CreatePopupMenu();
		::AppendMenuA( aspectMenu, MF_STRING, ID_ENGINE_ASPECT_FIT, "&Fit To Window" );
		::AppendMenuA( aspectMenu, MF_STRING, ID_ENGINE_ASPECT_4_3, "&4:3" );
		::AppendMenuA( aspectMenu, MF_STRING, ID_ENGINE_ASPECT_16_9, "&16:9" );
		::AppendMenuA( aspectMenu, MF_STRING, ID_ENGINE_ASPECT_16_10, "16:10" );
		::AppendMenuA( aspectMenu, MF_STRING, ID_ENGINE_ASPECT_R_MODE, "r_mode" );
		::AppendMenuA( viewMenu, MF_POPUP, reinterpret_cast<UINT_PTR>( aspectMenu ), "&Aspect Ratio" );
		::AppendMenuA( engineMenuHandle, MF_POPUP, reinterpret_cast<UINT_PTR>( viewMenu ), "View" );
	}
	if ( !engineMenu.GetSafeHwnd() && !engineMenu.Create( &documentDock, engineMenuHandle, this ) ) return false;
	if ( !engineToolBar.GetSafeHwnd() && !engineToolBar.Create( &documentDock, this ) ) return false;
	documentDock.SetPaneChrome( STUDIO_PANE_ENGINE, &engineMenu, &engineToolBar );
	OnEngineAspect( engineAspectCommand );
	LayoutViews();
	return true;
}

bool idStudioMainWnd::AttachCameraChrome( idStudioToolBar *cameraToolbar, CWnd *commandTarget ) {
	if ( cameraToolbar == NULL || !cameraToolbar->GetSafeHwnd() || commandTarget == NULL ) return false;
	cameraToolbar->SetParent( &cameraDock );
	cameraToolbar->SetOwner( commandTarget );
	cameraToolbar->ModifyStyle( CBRS_GRIPPER | CBRS_FLOAT_MULTI | CBRS_SIZE_DYNAMIC | CBRS_ORIENT_VERT,
		WS_CHILD | WS_CLIPSIBLINGS | CBRS_ORIENT_HORZ, SWP_FRAMECHANGED | SWP_NOACTIVATE );
	cameraToolbar->AdjustLayout();
	if ( !cameraControlBar.GetSafeHwnd() && !cameraControlBar.Create( &cameraDock, commandTarget ) ) return false;
	// The reference places the icon command bar first, followed by the render
	// settings / Quality / Mip band.
	cameraDock.SetPaneChrome( STUDIO_PANE_WORLD_CAMERA, cameraToolbar, &cameraControlBar );
	LayoutViews();
	return true;
}

void idStudioMainWnd::OnEngineAspect( UINT command ) {
	float aspect = 0.0f;
	switch ( command ) {
		case ID_ENGINE_ASPECT_FIT: aspect = engineSourceAspect; break;
		case ID_ENGINE_ASPECT_4_3: aspect = 4.0f / 3.0f; break;
		case ID_ENGINE_ASPECT_16_9: aspect = 16.0f / 9.0f; break;
		case ID_ENGINE_ASPECT_16_10: aspect = 16.0f / 10.0f; break;
		case ID_ENGINE_ASPECT_R_MODE: {
			const int width = cvarSystem->GetCVarInteger( "r_customWidth" );
			const int height = cvarSystem->GetCVarInteger( "r_customHeight" );
			aspect = width > 0 && height > 0 ? static_cast<float>( width ) / height : 16.0f / 9.0f;
			break;
		}
		default: command = ID_ENGINE_ASPECT_16_9; aspect = 16.0f / 9.0f; break;
	}
	engineAspectCommand = command;
	const UINT commands[] = { ID_ENGINE_ASPECT_FIT, ID_ENGINE_ASPECT_4_3, ID_ENGINE_ASPECT_16_9,
		ID_ENGINE_ASPECT_16_10, ID_ENGINE_ASPECT_R_MODE };
	for ( int i = 0; i < 5; ++i ) ::CheckMenuItem( engineMenuHandle, commands[i],
		MF_BYCOMMAND | ( commands[i] == command ? MF_CHECKED : MF_UNCHECKED ) );
	engineMenu.Invalidate( FALSE );
	engineToolBar.SetAspectCommand( command );
	documentDock.SetPaneAspectRatio( STUDIO_PANE_ENGINE, aspect );
}

void idStudioMainWnd::OnEngineExit() {
	CWnd *frame = GetTopLevelFrame();
	if ( frame != NULL && frame->GetSafeHwnd() ) frame->PostMessage( WM_CLOSE );
}

LRESULT idStudioMainWnd::OnFlyoutCommand( WPARAM command, LPARAM ) {
	if ( command == IDSTUDIO_FLYOUT_PIN ) autoHideBar.TogglePinned();
	else if ( command == IDSTUDIO_FLYOUT_CLOSE ) autoHideBar.HidePane();
	LayoutViews();
	return 0;
}

void idStudioMainWnd::ShowPane( int paneId ) {
	if ( browserDock.ContainsPane( paneId ) ) { browserDock.SelectPane( paneId ); autoHideBar.HidePane(); }
	else if ( cameraDock.ContainsPane( paneId ) ) cameraDock.SelectPane( paneId );
	else if ( documentDock.ContainsPane( paneId ) ) documentDock.SelectPane( paneId );
	else autoHideBar.ShowPane( paneId );
	LayoutViews();
}

void idStudioMainWnd::SetInspectorVisible( bool visible ) {
	if ( visible ) autoHideBar.ShowPane( autoHideBar.ActivePane() >= 0 ? autoHideBar.ActivePane() : STUDIO_PANE_CONSOLE );
	else autoHideBar.HidePane();
	LayoutViews();
}

bool idStudioMainWnd::IsInspectorVisible() const { return autoHideBar.IsPaneOpen(); }

bool idStudioMainWnd::IsEngineViewActive() const {
	return documentDock.ActivePane() == STUDIO_PANE_ENGINE &&
		engineView.GetSafeHwnd() != NULL && engineView.IsWindowVisible();
}

bool idStudioMainWnd::IsConsolePaneOpen() const {
	return autoHideBar.IsPaneOpen() && autoHideBar.ActivePane() == STUDIO_PANE_CONSOLE;
}

void idStudioMainWnd::ResetLayout() {
	leftWidth = StudioDpiScale( this, 410 ); cameraFraction = 0.49f; rightFlyoutWidth = StudioDpiScale( this, 430 );
	idStudio_leftDockWidth.SetInteger( 410 );
	idStudio_cameraDockFraction.SetFloat( cameraFraction );
	idStudio_rightFlyoutWidth.SetInteger( 430 );
	autoHideBar.HidePane();
	cameraDock.SelectPane( STUDIO_PANE_WORLD_CAMERA );
	browserDock.SelectPane( STUDIO_PANE_MEDIA_BROWSER );
	documentDock.SelectPane( STUDIO_PANE_WORLD_EDIT );
	LayoutViews();
}

void idStudioMainWnd::LayoutViews() {
	// The parent receives WM_SIZE synchronously from CreateWindowEx, before its
	// four child hosts exist.  Wait until the complete workspace is live.
	if ( !GetSafeHwnd() || !cameraDock.GetSafeHwnd() || !browserDock.GetSafeHwnd() ||
		!documentDock.GetSafeHwnd() || !autoHideBar.GetSafeHwnd() ) return;
	CRect client; GetClientRect( &client );
	if ( client.Width() < 640 || client.Height() < 320 ) return;
	const int splitterWidth = StudioDpiScale( this, SPLITTER_WIDTH );
	const int autoHideWidth = StudioDpiScale( this, AUTOHIDE_WIDTH );
	const int minimumLeftWidth = StudioDpiScale( this, 280 );
	leftWidth = leftWidth < minimumLeftWidth ? minimumLeftWidth : ( leftWidth > client.Width() / 2 ? client.Width() / 2 : leftWidth );
	int topHeight = static_cast<int>( client.Height() * cameraFraction );
	topHeight = topHeight < 150 ? 150 : ( topHeight > client.Height() - 150 ? client.Height() - 150 : topHeight );
	const int documentLeft = leftWidth + splitterWidth;
	// When an inspector is open its flyout is the reference idStudio's right
	// dock column.  Previously documentRight ignored that width, so the Engine
	// HWND continued beneath the Console/Inspector and only its left portion
	// remained visible, which looked exactly like an aspect-ratio stretch.
	const int maximumFlyoutWidth = Max( 0, client.Width() - documentLeft - autoHideWidth - StudioDpiScale( this, 320 ) );
	// XTP auto-hide flyouts overlay the client. Only a pinned pane participates
	// in the docking layout and permanently reduces the document width.
	const int activeFlyoutWidth = autoHideBar.IsPaneOpen() && autoHideBar.IsPinned() ?
		Min( rightFlyoutWidth, maximumFlyoutWidth ) : 0;
	const int documentRight = client.right - autoHideWidth - activeFlyoutWidth;
	bool changed = false;
	changed |= StudioSetWindowRect( &cameraDock, NULL, 0, 0, leftWidth, topHeight, SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW );
	changed |= StudioSetWindowRect( &browserDock, NULL, 0, topHeight + splitterWidth, leftWidth,
		client.Height() - topHeight - splitterWidth, SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW );
	changed |= StudioSetWindowRect( &documentDock, NULL, documentLeft, 0, documentRight - documentLeft, client.Height(),
		SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW );
	changed |= StudioSetWindowRect( &autoHideBar, &wndTop, documentRight, 0, autoHideWidth, client.Height(), SWP_NOACTIVATE | SWP_SHOWWINDOW );
	autoHideBar.LayoutFlyout( client, activeFlyoutWidth > 0 ? activeFlyoutWidth : rightFlyoutWidth );
	if ( changed ) Invalidate( FALSE );
}

void idStudioMainWnd::OnSize( UINT type, int width, int height ) { CWnd::OnSize( type, width, height ); LayoutViews(); }
BOOL idStudioMainWnd::OnEraseBkgnd( CDC * ) { return TRUE; }
void idStudioMainWnd::OnPaint() { CPaintDC dc( this ); CRect rect; GetClientRect( &rect ); dc.FillSolidRect( rect, RGB( 65, 65, 65 ) ); }

void idStudioMainWnd::OnLButtonDown( UINT flags, CPoint point ) {
	CRect client; GetClientRect( &client );
	const int topHeight = static_cast<int>( client.Height() * cameraFraction );
	const int splitterWidth = StudioDpiScale( this, SPLITTER_WIDTH );
	if ( point.x >= leftWidth && point.x < leftWidth + splitterWidth ) dragSplitter = 1;
	else if ( point.x < leftWidth && point.y >= topHeight && point.y < topHeight + splitterWidth ) dragSplitter = 2;
	if ( dragSplitter ) SetCapture(); else CWnd::OnLButtonDown( flags, point );
}

void idStudioMainWnd::OnMouseMove( UINT flags, CPoint point ) {
	if ( dragSplitter == 0 || GetCapture() != this ) { CWnd::OnMouseMove( flags, point ); return; }
	CRect client; GetClientRect( &client );
	if ( dragSplitter == 1 ) leftWidth = point.x;
	else if ( client.Height() > 0 ) cameraFraction = static_cast<float>( point.y ) / client.Height();
	LayoutViews();
}

void idStudioMainWnd::OnLButtonUp( UINT flags, CPoint point ) {
	if ( dragSplitter ) {
		if ( GetCapture() == this ) ReleaseCapture();
		dragSplitter = 0;
		idStudio_leftDockWidth.SetInteger( StudioDpiUnscale( this, leftWidth ) );
		idStudio_cameraDockFraction.SetFloat( cameraFraction );
		return;
	}
	CWnd::OnLButtonUp( flags, point );
}

BOOL idStudioMainWnd::OnSetCursor( CWnd *window, UINT hitTest, UINT message ) {
	CPoint point; GetCursorPos( &point ); ScreenToClient( &point );
	CRect client; GetClientRect( &client );
	const int topHeight = static_cast<int>( client.Height() * cameraFraction );
	const int splitterWidth = StudioDpiScale( this, SPLITTER_WIDTH );
	if ( point.x >= leftWidth && point.x < leftWidth + splitterWidth ) { ::SetCursor( ::LoadCursor( NULL, IDC_SIZEWE ) ); return TRUE; }
	if ( point.x < leftWidth && point.y >= topHeight && point.y < topHeight + splitterWidth ) { ::SetCursor( ::LoadCursor( NULL, IDC_SIZENS ) ); return TRUE; }
	return CWnd::OnSetCursor( window, hitTest, message );
}
