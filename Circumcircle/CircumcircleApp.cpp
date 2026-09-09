#include <afxwin.h>
#include <afxcmn.h>
#include "CircumcircleDlg.h"

class CCircumcircleApp final : public CWinApp
{
public:
    BOOL InitInstance() override
    {
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES};
        InitCommonControlsEx(&controls);
        CWinApp::InitInstance();
        CCircumcircleDlg dialog;
        m_pMainWnd = &dialog;
        dialog.DoModal();
        m_pMainWnd = nullptr;
        return FALSE;
    }
};

CCircumcircleApp theApp;
