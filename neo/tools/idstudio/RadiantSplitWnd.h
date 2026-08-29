#pragma once

#include <vector>
#include <afxcontrolbars.h>

class CInspectorDialog;

class idStudioToolBar : public CMFCToolBar {
public:
	BOOL LoadStudioToolBar( UINT resourceId );
	void CheckButton( UINT commandId, BOOL checked = TRUE );
	void RemoveCommand( UINT commandId );
};

class idStudioMenuBar : public CWnd {
	DECLARE_DYNAMIC( idStudioMenuBar )
public:
	idStudioMenuBar();
	bool Create( CWnd *parent, HMENU menu, CWnd *commandTarget );
	int PreferredHeight() const;
protected:
	HMENU menuHandle;
	CWnd *target;
	std::vector<CRect> itemRects;
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd( CDC *dc );
	afx_msg void OnLButtonDown( UINT flags, CPoint point );
	DECLARE_MESSAGE_MAP()
};

class idStudioEngineToolBar : public CWnd {
	DECLARE_DYNAMIC( idStudioEngineToolBar )
public:
	idStudioEngineToolBar();
	bool Create( CWnd *parent, CWnd *commandTarget );
	void SetAspectCommand( UINT command );
protected:
	CWnd *target;
	UINT activeCommand;
	std::vector<CRect> buttonRects;
	std::vector<UINT> buttonCommands;
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd( CDC *dc );
	afx_msg void OnLButtonDown( UINT flags, CPoint point );
	DECLARE_MESSAGE_MAP()
};

class idStudioCameraControlBar : public CWnd {
	DECLARE_DYNAMIC( idStudioCameraControlBar )
public:
	idStudioCameraControlBar();
	bool Create( CWnd *parent, CWnd *commandTarget );
	int PreferredHeight() const;
protected:
	CWnd *target;
	int quality;
	int mip;
	CComboBox worldCombo;
	CComboBox qualityCombo;
	CComboBox mipCombo;
	CButton minusButton;
	CButton plusButton;
	void ApplyWorldMode();
	void ApplyQuality();
	void ApplyMip();
	afx_msg void OnSize( UINT type, int width, int height );
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd( CDC *dc );
	afx_msg void OnWorldChanged();
	afx_msg void OnQualityChanged();
	afx_msg void OnMipChanged();
	afx_msg void OnMipMinus();
	afx_msg void OnMipPlus();
	DECLARE_MESSAGE_MAP()
};

class idStudioWorldEditCommandBars : public CWnd {
	DECLARE_DYNAMIC( idStudioWorldEditCommandBars )
public:
	idStudioWorldEditCommandBars();
	bool Create( CWnd *parent, CWnd *commandTarget );
	bool Attach( idStudioMenuBar *menuBar, idStudioToolBar *fileBar,
		idStudioToolBar *worldEditBar, idStudioToolBar *supplementalBar );
	int PreferredHeight() const;
protected:
	CWnd *target;
	idStudioMenuBar *menu;
	idStudioToolBar *fileToolbar;
	idStudioToolBar *worldEditToolbar;
	idStudioToolBar *supplementalToolbar;
	CComboBox layoutCombo;
	CButton saveLayoutButton;
	CButton resetLayoutButton;
	CEdit rotationEdit;
	void LayoutBands();
	afx_msg void OnSize( UINT type, int width, int height );
	afx_msg BOOL OnEraseBkgnd( CDC *dc );
	afx_msg void OnPaint();
	afx_msg void OnResetLayout();
	DECLARE_MESSAGE_MAP()
};

class idStudioWorldEditWnd : public CWnd {
	DECLARE_DYNAMIC( idStudioWorldEditWnd )
public:
	idStudioWorldEditWnd();
	bool Create( CWnd *parent );
	void AttachViews( CWnd *zView, CWnd *xyView );
	void ToggleZView();
protected:
	CWnd *zWindow;
	CWnd *xyWindow;
	bool showZ;
	void LayoutViews();
	afx_msg void OnSize( UINT type, int width, int height );
	DECLARE_MESSAGE_MAP()
};

enum idStudioPaneId {
	STUDIO_PANE_PREVIEW = 20000,
	STUDIO_PANE_WORLD_CAMERA,
	STUDIO_PANE_ENGINE,
	STUDIO_PANE_WORLD_EDIT,
	STUDIO_PANE_MEDIA_BROWSER,
	STUDIO_PANE_MATERIAL_BROWSER,
	STUDIO_PANE_MAP_STRUCTURE,
	STUDIO_PANE_CONSOLE,
	STUDIO_PANE_ENTITY_INSPECTOR,
	STUDIO_PANE_SURFACE_INSPECTOR
};

class idStudioPlaceholderWnd : public CWnd {
	DECLARE_DYNAMIC( idStudioPlaceholderWnd )
public:
	bool Create( CWnd *parent, const char *label );
protected:
	CString label;
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd( CDC *dc );
	DECLARE_MESSAGE_MAP()
};

class idStudioDockPane : public CWnd {
	DECLARE_DYNAMIC( idStudioDockPane )
public:
	idStudioDockPane();
	bool Create( CWnd *parent, UINT controlId, bool caption, bool tabsAtTop = false );
	void AddPane( int id, const char *title, CWnd *window );
	bool SelectPane( int id );
	bool ContainsPane( int id ) const;
	int ActivePane() const { return activePane; }
	void SetPaneChrome( int paneId, CWnd *chromeWindow1, CWnd *chromeWindow2, CWnd *chromeWindow3 = NULL );
	void SetPaneAspectRatio( int paneId, float aspectRatio );
	void SetTabsVisible( bool visible );
	bool IsPaneLayoutCurrent( int paneId ) const;
	void LayoutPane();
protected:
	struct pane_t {
		int id;
		CString title;
		CWnd *window;
		CWnd *chromeWindow1;
		CWnd *chromeWindow2;
		CWnd *chromeWindow3;
		float aspectRatio;
		pane_t( int paneId, const char *paneTitle, CWnd *paneWindow ) : id( paneId ), title( paneTitle ), window( paneWindow ),
			chromeWindow1( NULL ), chromeWindow2( NULL ), chromeWindow3( NULL ), aspectRatio( 0.0f ) {}
	};
	std::vector<pane_t> panes;
	CTabCtrl tabs;
	bool showCaption;
	bool tabsTop;
	bool showTabs;
	int activePane;
	afx_msg void OnSize( UINT type, int width, int height );
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd( CDC *dc );
	afx_msg void OnLButtonDown( UINT flags, CPoint point );
	afx_msg void OnTabChanged( NMHDR *header, LRESULT *result );
	virtual BOOL OnNotify( WPARAM wParam, LPARAM lParam, LRESULT *result );
	DECLARE_MESSAGE_MAP()
};

class idStudioAutoHideBar : public CWnd {
	DECLARE_DYNAMIC( idStudioAutoHideBar )
public:
	idStudioAutoHideBar();
	bool Create( CWnd *parent, UINT controlId );
	void AddPane( int id, const char *title, CWnd *window );
	bool ShowPane( int id );
	void HidePane();
	bool IsPaneOpen() const;
	bool IsPinned() const { return pinned; }
	void TogglePinned();
	int ActivePane() const;
	void LayoutFlyout( const CRect &workspace, int width );
protected:
	struct tab_t {
		int id;
		CString title;
		tab_t( int paneId, const char *paneTitle ) : id( paneId ), title( paneTitle ) {}
	};
	std::vector<tab_t> paneTabs;
	idStudioDockPane flyout;
	bool pinned;
	DWORD outsideSince;
	CRect TabRect( int index, CDC *dc = NULL ) const;
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd( CDC *dc );
	afx_msg void OnLButtonDown( UINT flags, CPoint point );
	afx_msg void OnMouseMove( UINT flags, CPoint point );
	afx_msg void OnTimer( UINT_PTR timerId );
	DECLARE_MESSAGE_MAP()
};

class idStudioMainWnd : public CWnd {
	DECLARE_DYNAMIC( idStudioMainWnd )
public:
	idStudioMainWnd();
	virtual ~idStudioMainWnd();
	bool Create( CWnd *parent );
	bool AttachEngineView( HWND engineWindow );
	void DetachEngineView();
	void AttachViews( CWnd *zView, CWnd *xyView, CWnd *cameraView );
	void AttachInspector( CWnd *inspector );
	bool AttachDocumentChrome( HMENU menu, idStudioToolBar *fileToolbar, idStudioToolBar *worldEditToolbar,
		idStudioToolBar *supplementalToolbar, CWnd *commandTarget );
	bool AttachCameraChrome( idStudioToolBar *cameraToolbar, CWnd *commandTarget );
	void ShowPane( int paneId );
	void ToggleZView() { worldEditWnd.ToggleZView(); }
	void SetInspectorVisible( bool visible );
	bool IsInspectorVisible() const;
	bool IsEngineViewActive() const;
	bool IsConsolePaneOpen() const;
	void LayoutViews();
	void ResetLayout();
protected:
	idStudioDockPane cameraDock;
	idStudioDockPane browserDock;
	idStudioDockPane documentDock;
	idStudioAutoHideBar autoHideBar;
	idStudioMenuBar documentMenu;
	idStudioWorldEditCommandBars worldEditCommandBars;
	idStudioMenuBar engineMenu;
	idStudioEngineToolBar engineToolBar;
	idStudioCameraControlBar cameraControlBar;
	idStudioWorldEditWnd worldEditWnd;
	idStudioPlaceholderWnd previewWnd;
	idStudioPlaceholderWnd engineWnd;
	CWnd engineView;
	HWND engineOriginalParent;
	LONG_PTR engineOriginalStyle;
	LONG_PTR engineOriginalExStyle;
	HMENU documentMenuHandle;
	HMENU engineMenuHandle;
	UINT engineAspectCommand;
	float engineSourceAspect;
	CWnd *zView;
	CWnd *xyView;
	CWnd *cameraView;
	CInspectorDialog *inspectors;
	int leftWidth;
	float cameraFraction;
	int rightFlyoutWidth;
	int dragSplitter;
	afx_msg void OnSize( UINT type, int width, int height );
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd( CDC *dc );
	afx_msg void OnLButtonDown( UINT flags, CPoint point );
	afx_msg void OnLButtonUp( UINT flags, CPoint point );
	afx_msg void OnMouseMove( UINT flags, CPoint point );
	afx_msg BOOL OnSetCursor( CWnd *window, UINT hitTest, UINT message );
	afx_msg LRESULT OnFlyoutCommand( WPARAM command, LPARAM );
	afx_msg void OnEngineAspect( UINT command );
	afx_msg void OnEngineExit();
	DECLARE_MESSAGE_MAP()
};

// Compatibility name retained for the DoomEdit-facing code.
class idRadiantSplitWnd : public idStudioMainWnd {
	DECLARE_DYNAMIC( idRadiantSplitWnd )
};
