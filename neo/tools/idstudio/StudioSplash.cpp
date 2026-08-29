#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "QE3.H"
#include "StudioSplash.h"

IMPLEMENT_DYNAMIC( idSplashDlg, CWnd )

BEGIN_MESSAGE_MAP( idSplashDlg, CWnd )
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

idSplashDlg::idSplashDlg() {}

bool idSplashDlg::Open( const char *status ) {
	statusText = status != NULL ? status : "Initializing idStudio";
	CString windowClass = AfxRegisterWndClass( CS_SAVEBITS | CS_DBLCLKS, ::LoadCursor( NULL, IDC_WAIT ),
		static_cast<HBRUSH>( ::GetStockObject( BLACK_BRUSH ) ), NULL );
	if ( !CreateEx( WS_EX_TOOLWINDOW | WS_EX_TOPMOST, windowClass, "idStudio",
		WS_POPUP | WS_BORDER, CRect( 0, 0, 400, 268 ), CWnd::FromHandle( win32.hWnd ), 0 ) ) return false;
	bitmap.LoadBitmap( IDB_IDSTUDIO_SPLASH );
	CRect desktop;
	::SystemParametersInfo( SPI_GETWORKAREA, 0, &desktop, 0 );
	SetWindowPos( &wndTopMost, desktop.left + ( desktop.Width() - 400 ) / 2,
		desktop.top + ( desktop.Height() - 268 ) / 2, 400, 268, SWP_SHOWWINDOW | SWP_NOACTIVATE );
	UpdateWindow();
	return true;
}

void idSplashDlg::SetStatus( const char *status ) {
	statusText = status != NULL ? status : "";
	Invalidate( FALSE );
	UpdateWindow();
}

void idSplashDlg::Close() {
	if ( GetSafeHwnd() ) DestroyWindow();
	bitmap.DeleteObject();
}

BOOL idSplashDlg::OnEraseBkgnd( CDC * ) { return TRUE; }

void idSplashDlg::OnPaint() {
	CPaintDC dc( this );
	CRect client;
	GetClientRect( &client );
	dc.FillSolidRect( client, RGB( 12, 12, 14 ) );
	if ( bitmap.GetSafeHandle() != NULL ) {
		CDC source;
		source.CreateCompatibleDC( &dc );
		CBitmap *old = source.SelectObject( &bitmap );
		dc.BitBlt( 0, 0, 400, 240, &source, 0, 0, SRCCOPY );
		if ( old != NULL ) source.SelectObject( old );
	}
	CRect statusRect( 10, 241, client.right - 10, client.bottom );
	dc.SetBkMode( TRANSPARENT );
	dc.SetTextColor( RGB( 210, 210, 210 ) );
	dc.DrawText( statusText, statusRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS );
}
