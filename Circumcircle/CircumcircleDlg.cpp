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
    ON_BN_CLICKED(IDC_RESET, &CCircumcircleDlg::ResetAll)
    ON_BN_CLICKED(IDC_RANDOM, &CCircumcircleDlg::StartRandomMovement)
    ON_EN_CHANGE(IDC_POINT_RADIUS, &CCircumcircleDlg::OnRadiusChanged)
    ON_EN_CHANGE(IDC_CIRCLE_THICKNESS, &CCircumcircleDlg::OnThicknessChanged)
    ON_EN_KILLFOCUS(IDC_POINT_RADIUS, &CCircumcircleDlg::OnRadiusKillFocus)
    ON_EN_KILLFOCUS(IDC_CIRCLE_THICKNESS, &CCircumcircleDlg::OnThicknessKillFocus)
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
    static_cast<CEdit*>(GetDlgItem(IDC_POINT_RADIUS))->SetLimitText(10);
    static_cast<CEdit*>(GetDlgItem(IDC_CIRCLE_THICKNESS))->SetLimitText(10);
    SetDlgItemInt(IDC_POINT_RADIUS, m_pointRadius, FALSE);
    SetDlgItemInt(IDC_CIRCLE_THICKNESS, m_circleThickness, FALSE);
    m_ready = true;
    ApplyInputs();
    UpdatePointCoordinateUI();
    UpdateProgressUI();
    return TRUE;
}

void CCircumcircleDlg::UpdatePointCoordinateUI()
{
    for (int i = 0; i < 3; ++i)
    {
        CString text;
        if (i < m_canvas.PointCount())
        {
            const auto& point = m_canvas.Points()[i];
            text.Format(L"Point %d : (%d, %d)", i + 1, point.x, point.y);
        }
        else
            text.Format(L"Point %d : -", i + 1);
        SetDlgItemText(IDC_POINT1 + i, text);
    }
    GetDlgItem(IDC_RANDOM)->EnableWindow(m_canvas.HasCircumcircle() && !m_randomRun);
    CString info;
    if (m_canvas.HasCircumcircle())
    {
        const auto& circle = m_canvas.Circumcircle();
        info.Format(L"중심: (%.2f, %.2f)\r\n반지름: %.2f px", circle.centerX, circle.centerY, circle.radius);
    }
    SetDlgItemText(IDC_CIRCLE_INFO, info);
    if (m_randomRun)
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
    CString text;
    text.Format(L"랜덤 이동 : %d / 10", m_randomMoves);
    SetDlgItemText(IDC_PROGRESS, text);
}

bool CCircumcircleDlg::ReadPositiveInteger(int control, int maximum, int& value) const
{
    CString text;
    GetDlgItemText(control, text);
    text.Trim();
    if (text.IsEmpty() || text.GetLength() > 10)
        return false;
    int parsed = 0;
    for (int i = 0; i < text.GetLength(); ++i)
    {
        if (text[i] < L'0' || text[i] > L'9')
            return false;
        parsed = parsed * 10 + (text[i] - L'0');
        if (parsed > maximum)
            return false;
    }
    if (parsed < 1)
        return false;
    value = parsed;
    return true;
}

void CCircumcircleDlg::ApplyInputs()
{
    if (!m_ready || m_updatingInputs)
        return;
    int radius = m_pointRadius;
    int thickness = m_circleThickness;
    const bool validRadius = ReadPositiveInteger(IDC_POINT_RADIUS, 64, radius);
    const bool validThickness = ReadPositiveInteger(IDC_CIRCLE_THICKNESS, 32, thickness);
    if (validRadius && radius != m_pointRadius)
    {
        StopRandomMovement();
        m_pointRadius = radius;
        m_canvas.SetPointRadius(radius);
        UpdatePointCoordinateUI();
    }
    if (validThickness)
    {
        m_circleThickness = thickness;
        m_canvas.SetCircleThickness(thickness);
    }
    SetDlgItemText(IDC_VALIDATION, validRadius && validThickness
        ? L"빈 공간은 최대 3점까지 입력됩니다. 검은 점을 잡고 드래그하세요."
        : L"반지름 1~64, 두께 1~32의 정수를 입력하세요. 잘못된 값은 적용되지 않습니다.");
}

void CCircumcircleDlg::NormalizeInput(int control, int maximum, int previous)
{
    if (!m_ready || m_updatingInputs)
        return;
    int value = previous;
    ReadPositiveInteger(control, maximum, value);
    m_updatingInputs = true;
    SetDlgItemInt(control, value, FALSE);
    m_updatingInputs = false;
    ApplyInputs();
}

void CCircumcircleDlg::OnRadiusChanged() { ApplyInputs(); }
void CCircumcircleDlg::OnThicknessChanged() { ApplyInputs(); }
void CCircumcircleDlg::OnRadiusKillFocus() { NormalizeInput(IDC_POINT_RADIUS, 64, m_pointRadius); }
void CCircumcircleDlg::OnThicknessKillFocus() { NormalizeInput(IDC_CIRCLE_THICKNESS, 32, m_circleThickness); }

void CCircumcircleDlg::StopRandomMovement()
{
    // Old queued notifications contain no owning pointers and are invalidated here.
    ++m_generation;
    if (m_randomRun)
    {
        {
            std::lock_guard<std::mutex> lock(m_randomRun->mutex);
            m_randomRun->cancelled = true;
        }
        m_randomRun->wake.notify_all();
    }
    if (m_worker.joinable())
        m_worker.join();
    m_randomRun.reset();
}

void CCircumcircleDlg::ResetAll()
{
    StopRandomMovement();
    m_randomMoves = 0;
    m_canvas.ResetAll();
    UpdateProgressUI();
}

void CCircumcircleDlg::StartRandomMovement()
{
    if (m_randomRun || !m_canvas.HasCircumcircle())
        return;
    CRect area;
    m_canvas.GetClientRect(&area);
    const int minX = m_pointRadius;
    const int minY = m_pointRadius;
    const int maxX = area.Width() - 1 - m_pointRadius;
    const int maxY = area.Height() - 1 - m_pointRadius;
    if (maxX <= minX || maxY <= minY)
        return;
    StopRandomMovement();
    m_randomMoves = 0;
    auto run = std::make_shared<RandomRun>();
    m_randomRun = run;
    const HWND target = GetSafeHwnd();
    const UINT_PTR generation = m_generation;
    try
    {
        m_worker = std::thread([run, target, generation, minX, minY, maxX, maxY] {
            using Clock = std::chrono::steady_clock;
            const auto start = Clock::now();
            std::mt19937 engine(static_cast<unsigned int>(start.time_since_epoch().count()));
            std::uniform_int_distribution<int> randomX(minX, maxX);
            std::uniform_int_distribution<int> randomY(minY, maxY);
            for (int i = 0; i < 10; ++i)
            {
                Frame frame{};
                bool valid = false;
                for (int attempt = 0; attempt < 128; ++attempt)
                {
                    for (auto& point : frame)
                        point = {randomX(engine), randomY(engine)};
                    geometry::Circle circle;
                    if (geometry::CalculateCircumcircle(frame[0], frame[1], frame[2], circle))
                    {
                        valid = true;
                        break;
                    }
                }
                if (!valid)
                    frame = {geometry::Point{minX, minY}, geometry::Point{maxX, minY}, geometry::Point{minX, maxY}};
                {
                    std::unique_lock<std::mutex> lock(run->mutex);
                    if (run->wake.wait_until(lock, start + std::chrono::milliseconds(500 * (i + 1)),
                        [&run] { return run->cancelled; }))
                        return;
                    run->frames[i] = frame;
                }
                // Only a plain HWND is passed across threads. All MFC/GDI stays in the UI thread.
                if (!::PostMessage(target, WM_RANDOM_MOVE_POINT, generation, i))
                    return;
            }
        });
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

LRESULT CCircumcircleDlg::OnRandomMove(WPARAM generation, LPARAM frameIndex)
{
    if (!m_randomRun || generation != m_generation || frameIndex < 0 || frameIndex >= 10
        || frameIndex != m_randomMoves)
        return 0;
    Frame frame;
    {
        std::lock_guard<std::mutex> lock(m_randomRun->mutex);
        frame = m_randomRun->frames[static_cast<size_t>(frameIndex)];
    }
    ++m_randomMoves;
    m_canvas.SetPoints(frame);
    UpdateProgressUI();
    if (m_randomMoves == 10)
    {
        StopRandomMovement();
        UpdatePointCoordinateUI();
    }
    return 0;
}

void CCircumcircleDlg::OnOK()
{
    OnRadiusKillFocus();
    OnThicknessKillFocus();
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
    m_ready = false;
    CDialogEx::OnDestroy();
}
