#include "CircumcircleDlg.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <system_error>

namespace
{
    constexpr UINT WM_RANDOM_MOVE_POINT = WM_APP + 1;
}

BEGIN_MESSAGE_MAP(CCircumcircleDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_RESET, &CCircumcircleDlg::OnBnClickedBtnReset)
    ON_BN_CLICKED(IDC_BTN_RANDOM, &CCircumcircleDlg::OnBnClickedBtnRandom)
    ON_EN_CHANGE(IDC_EDIT_RADIUS, &CCircumcircleDlg::OnEnChangeEditRadius)
    ON_EN_CHANGE(IDC_EDIT_THICKNESS, &CCircumcircleDlg::OnEnChangeEditThickness)
    ON_EN_KILLFOCUS(IDC_EDIT_RADIUS, &CCircumcircleDlg::OnEnKillfocusEditRadius)
    ON_EN_KILLFOCUS(IDC_EDIT_THICKNESS, &CCircumcircleDlg::OnEnKillfocusEditThickness)
    ON_MESSAGE(WM_RANDOM_MOVE_POINT, &CCircumcircleDlg::OnRandomMove)
    ON_WM_DESTROY()
END_MESSAGE_MAP()

CCircumcircleDlg::~CCircumcircleDlg() { StopRandomMovement(); }

BOOL CCircumcircleDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetIcon(::LoadIcon(nullptr, IDI_APPLICATION), TRUE);
    SetIcon(::LoadIcon(nullptr, IDI_APPLICATION), FALSE);
    m_canvas.SubclassDlgItem(IDC_CANVAS, this);
    m_canvas.onChanged = [this] { UpdatePointCoordinateUI(); };
    m_canvas.onBeginInteraction = [this] {
        StopRandomMovement();
        UpdatePointCoordinateUI();
    };
    static_cast<CEdit*>(GetDlgItem(IDC_EDIT_RADIUS))->SetLimitText(10);
    static_cast<CEdit*>(GetDlgItem(IDC_EDIT_THICKNESS))->SetLimitText(10);
    SetDlgItemInt(IDC_EDIT_RADIUS, m_nRadius, FALSE);
    SetDlgItemInt(IDC_EDIT_THICKNESS, m_nThickness, FALSE);
    m_bReady = true;
    ApplyInputs();
    UpdatePointCoordinateUI();
    UpdateProgressUI();
    return TRUE;
}

void CCircumcircleDlg::UpdatePointCoordinateUI()
{
    for (int i = 0; i < 3; ++i)
    {
        CString strText;
        if (i < m_canvas.PointCount())
        {
            const auto& point = m_canvas.Points()[i];
            strText.Format(L"Point %d : (%d, %d)", i + 1, point.x, point.y);
        }
        else
            strText.Format(L"Point %d : -", i + 1);
        SetDlgItemText(IDC_POINT1 + i, strText);
    }
    GetDlgItem(IDC_BTN_RANDOM)->EnableWindow(m_canvas.HasCircumcircle() && !m_pRandomRun);
    CString strInfo;
    if (m_canvas.HasCircumcircle())
    {
        const auto& circle = m_canvas.Circumcircle();
        strInfo.Format(L"중심: (%.2f, %.2f)\r\n반지름: %.2f px", circle.dCenterX, circle.dCenterY, circle.dRadius);
    }
    SetDlgItemText(IDC_CIRCLE_INFO, strInfo);
    if (m_pRandomRun)
        SetDlgItemText(IDC_STATUS, L"랜덤 이동 중입니다. 초기화 또는 점 드래그로 중지할 수 있습니다.");
    else if (m_canvas.PointCount() < 3)
        SetDlgItemText(IDC_STATUS, L"영역 안을 클릭해 점 3개를 만드세요. 기존 점을 드래그할 수 있습니다.");
    else if (!m_canvas.HasCircumcircle())
        SetDlgItemText(IDC_STATUS, L"세 점이 겹치거나 거의 일직선입니다. 점을 드래그해 위치를 바꾸세요.");
    else
        SetDlgItemText(IDC_STATUS, L"세 점을 지나는 외접원입니다. 점을 드래그하면 즉시 다시 계산합니다.");
}

void CCircumcircleDlg::UpdateProgressUI()
{
    CString strText;
    strText.Format(L"랜덤 이동 : %d / 10", m_nMoveCount);
    SetDlgItemText(IDC_PROGRESS, strText);
}

bool CCircumcircleDlg::ReadPositiveInteger(int nID, int nMax, int& nValue) const
{
    CString strText;
    GetDlgItemText(nID, strText);
    strText.Trim();
    if (strText.IsEmpty() || strText.GetLength() > 10)
        return false;
    int nParsed = 0;
    for (int i = 0; i < strText.GetLength(); ++i)
    {
        if (strText[i] < L'0' || strText[i] > L'9')
            return false;
        nParsed = nParsed * 10 + (strText[i] - L'0');
        if (nParsed > nMax)
            return false;
    }
    if (nParsed < 1)
        return false;
    nValue = nParsed;
    return true;
}

void CCircumcircleDlg::ApplyInputs()
{
    if (!m_bReady || m_bUpdatingInputs)
        return;
    int nRadius = m_nRadius;
    int nThickness = m_nThickness;
    const bool bValidRadius = ReadPositiveInteger(IDC_EDIT_RADIUS, 64, nRadius);
    const bool bValidThickness = ReadPositiveInteger(IDC_EDIT_THICKNESS, 32, nThickness);
    if (bValidRadius && nRadius != m_nRadius)
    {
        StopRandomMovement();
        m_nRadius = nRadius;
        m_canvas.SetPointRadius(nRadius);
        UpdatePointCoordinateUI();
    }
    if (bValidThickness)
    {
        m_nThickness = nThickness;
        m_canvas.SetCircleThickness(nThickness);
    }
    SetDlgItemText(IDC_VALIDATION, bValidRadius && bValidThickness
        ? L"빈 공간은 최대 3점까지 입력됩니다. 검은 점을 잡고 드래그하세요."
        : L"반지름 1~64, 두께 1~32의 정수를 입력하세요. 잘못된 값은 적용되지 않습니다.");
}

void CCircumcircleDlg::NormalizeInput(int nID, int nMax, int nPrevious)
{
    if (!m_bReady || m_bUpdatingInputs)
        return;
    int nValue = nPrevious;
    ReadPositiveInteger(nID, nMax, nValue);
    m_bUpdatingInputs = true;
    SetDlgItemInt(nID, nValue, FALSE);
    m_bUpdatingInputs = false;
    ApplyInputs();
}

void CCircumcircleDlg::OnEnChangeEditRadius() { ApplyInputs(); }
void CCircumcircleDlg::OnEnChangeEditThickness() { ApplyInputs(); }
void CCircumcircleDlg::OnEnKillfocusEditRadius() { NormalizeInput(IDC_EDIT_RADIUS, 64, m_nRadius); }
void CCircumcircleDlg::OnEnKillfocusEditThickness() { NormalizeInput(IDC_EDIT_THICKNESS, 32, m_nThickness); }
void CCircumcircleDlg::OnBnClickedBtnReset() { ResetAll(); }
void CCircumcircleDlg::OnBnClickedBtnRandom() { StartRandomMovement(); }

void CCircumcircleDlg::StopRandomMovement()
{
    // Old queued notifications contain no owning pointers and are invalidated here.
    ++m_nRunId;
    if (m_pRandomRun)
    {
        {
            std::lock_guard<std::mutex> lock(m_pRandomRun->mutex);
            m_pRandomRun->cancelled = true;
        }
        m_pRandomRun->wake.notify_all();
    }
    if (m_thread.joinable())
        m_thread.join();
    m_pRandomRun.reset();
}

void CCircumcircleDlg::ResetAll()
{
    StopRandomMovement();
    m_nMoveCount = 0;
    m_canvas.ResetAll();
    UpdateProgressUI();
}

void CCircumcircleDlg::StartRandomMovement()
{
    if (m_pRandomRun || !m_canvas.HasCircumcircle())
        return;
    CRect rect;
    m_canvas.GetClientRect(&rect);
    rect.DeflateRect(m_nRadius, m_nRadius);
    if (rect.Width() <= 1 || rect.Height() <= 1)
        return;
    StopRandomMovement();
    m_nMoveCount = 0;
    m_pRandomRun = std::make_shared<RandomRun>();
    try
    {
        m_thread = std::thread(&CCircumcircleDlg::threadProcess,
            m_pRandomRun, GetSafeHwnd(), m_nRunId, rect);
    }
    catch (const std::system_error&)
    {
        StopRandomMovement();
        SetDlgItemText(IDC_STATUS, L"작업 스레드를 시작하지 못했습니다. 다시 시도하세요.");
        return;
    }
    UpdateProgressUI();
    UpdatePointCoordinateUI();
}

void CCircumcircleDlg::threadProcess(std::shared_ptr<RandomRun> pRun,
    HWND hWnd, UINT_PTR nRunId, CRect rect)
{
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    const int nMinX = static_cast<int>(rect.left);
    const int nMinY = static_cast<int>(rect.top);
    const int nMaxX = static_cast<int>(rect.right) - 1;
    const int nMaxY = static_cast<int>(rect.bottom) - 1;
    std::mt19937 engine(static_cast<unsigned int>(start.time_since_epoch().count()));
    std::uniform_int_distribution<int> randomX(nMinX, nMaxX);
    std::uniform_int_distribution<int> randomY(nMinY, nMaxY);
    for (int i = 0; i < 10; ++i)
    {
        Frame ptData{};
        bool bValid = false;
        for (int nTry = 0; nTry < 128; ++nTry)
        {
            for (auto& point : ptData)
                point = {randomX(engine), randomY(engine)};
            geometry::Circle circle;
            if (geometry::calculateCircumcircle(ptData[0], ptData[1], ptData[2], circle))
            {
                bValid = true;
                break;
            }
        }
        if (!bValid)
            ptData = {geometry::Point{nMinX, nMinY},
                geometry::Point{nMaxX, nMinY}, geometry::Point{nMinX, nMaxY}};
        {
            std::unique_lock<std::mutex> lock(pRun->mutex);
            if (pRun->wake.wait_until(lock, start + std::chrono::milliseconds(500 * (i + 1)),
                [&pRun] { return pRun->cancelled; }))
                return;
            pRun->frames[i] = ptData;
        }
        // Keep the lesson's function name, but pass a HWND instead of a UI object.
        if (!::PostMessage(hWnd, WM_RANDOM_MOVE_POINT, nRunId, i))
            return;
    }
}

LRESULT CCircumcircleDlg::OnRandomMove(WPARAM nRunId, LPARAM nFrameIndex)
{
    if (!m_pRandomRun || nRunId != m_nRunId || nFrameIndex < 0 || nFrameIndex >= 10
        || nFrameIndex != m_nMoveCount)
        return 0;
    Frame frame;
    {
        std::lock_guard<std::mutex> lock(m_pRandomRun->mutex);
        frame = m_pRandomRun->frames[static_cast<size_t>(nFrameIndex)];
    }
    ++m_nMoveCount;
    m_canvas.SetPoints(frame);
    UpdateProgressUI();
    if (m_nMoveCount == 10)
    {
        StopRandomMovement();
        UpdatePointCoordinateUI();
    }
    return 0;
}

void CCircumcircleDlg::OnOK()
{
    OnEnKillfocusEditRadius();
    OnEnKillfocusEditThickness();
    m_canvas.SetFocus();
}

void CCircumcircleDlg::OnCancel()
{
    StopRandomMovement();
    CDialogEx::OnCancel();
}

void CCircumcircleDlg::OnDestroy()
{
    StopRandomMovement();
    m_bReady = false;
    CDialogEx::OnDestroy();
}
