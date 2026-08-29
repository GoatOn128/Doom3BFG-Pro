#pragma once

// Lightweight reconstruction of the idStudio startup splash dialog.  The
// artwork is the original idStudio bitmap resource from the reference build.
class idSplashDlg : public CWnd {
	DECLARE_DYNAMIC( idSplashDlg )
public:
	idSplashDlg();
	bool Open( const char *status );
	void SetStatus( const char *status );
	void Close();
protected:
	CBitmap bitmap;
	CString statusText;
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd( CDC *dc );
	DECLARE_MESSAGE_MAP()
};
