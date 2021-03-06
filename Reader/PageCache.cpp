#include "stdafx.h"
#include "PageCache.h"
#include <assert.h>

PageCache::PageCache()
    : m_Text(NULL)
    , m_Size(0)
    , m_OnePageLineCount(0)
    , m_CurPageSize(0)
    , m_CurrentLine(0)
    , m_CurrentPos(NULL)
    , m_lineGap(NULL)
    , m_InternalBorder(NULL)
{
    memset(&m_Rect, 0, sizeof(m_Rect));
    memset(&m_PageInfo, 0, sizeof(m_PageInfo));
}


PageCache::~PageCache()
{
    RemoveAllLine(TRUE);
}

void PageCache::SetText(HWND hWnd, TCHAR *text, INT size, INT *pos, INT *lg, INT *ib)
{
    m_Text = text;
    m_Size = size;
    m_CurrentPos = pos;
    m_lineGap = lg;
    m_InternalBorder = ib;
    RemoveAllLine(TRUE);
    InvalidateRect(hWnd, &m_Rect, FALSE);
}

void PageCache::SetRect(RECT *rect)
{
    BOOL bNeedClear = FALSE;
    if (0 == memcmp(rect, &m_Rect, sizeof(RECT)))
        return;

    if (m_Rect.right - m_Rect.left != rect->right - rect->left)
        bNeedClear = TRUE;

    memcpy(&m_Rect, rect, sizeof(RECT));
    if (bNeedClear)
        RemoveAllLine();
}

void PageCache::Reset(HWND hWnd, BOOL redraw)
{
    RemoveAllLine();
    if (redraw)
        InvalidateRect(hWnd, &m_Rect, FALSE);
}

void PageCache::ReDraw(HWND hWnd)
{
    InvalidateRect(hWnd, &m_Rect, FALSE);
}

void PageCache::PageUp(HWND hWnd)
{
    return LineUp(hWnd, m_OnePageLineCount);
}

void PageCache::PageDown(HWND hWnd)
{
    return LineDown(hWnd, m_OnePageLineCount);
}

void PageCache::LineUp(HWND hWnd, INT n)
{
    if (!IsValid())
        return;
    if (n == 0)
        return;
    if (m_PageInfo.line_size <= 0)
        return;
    if ((*m_CurrentPos) == 0) // already at the first line of file
        return;

    m_CurrentLine -= n;
    if (m_CurrentLine < 0 && m_PageInfo.line_info[0].start == 0) // n is out of range
    {
        m_CurrentLine = 0;
    }
    InvalidateRect(hWnd, &m_Rect, FALSE);
}

void PageCache::LineDown(HWND hWnd, INT n)
{
    if (!IsValid())
        return;
    if (n == 0)
        return;
    if ((*m_CurrentPos) + m_CurPageSize == m_Size) // already show the last line of file
        return;
    
    m_CurrentLine += n;
    InvalidateRect(hWnd, &m_Rect, FALSE);
}

void PageCache::DrawPage(HDC hdc)
{
    int i;
    int h;
    line_info_t *line;
    RECT rect;

    if (!IsValid())
        return;

    h = GetLineHeight(hdc);
    m_OnePageLineCount = (m_Rect.bottom - m_Rect.top + (*m_lineGap) - 2 * (*m_InternalBorder)) / h;

    if (m_PageInfo.line_size == 0 || m_CurrentLine < 0 
        || (m_PageInfo.line_info[m_PageInfo.line_size - 1].start + m_PageInfo.line_info[m_PageInfo.line_size - 1].length != m_Size && m_CurrentLine + m_OnePageLineCount > m_PageInfo.line_size))
    {
        LoadPageInfo(hdc, m_Rect.right - m_Rect.left - 2 * (*m_InternalBorder));
        UnitTest1();
    }
    UnitTest2();

    memcpy(&rect, &m_Rect, sizeof(RECT));
    m_CurPageSize = 0;
    rect.left = (*m_InternalBorder);
    rect.top = (*m_InternalBorder);
    for (i = 0; i < m_OnePageLineCount && m_CurrentLine + i < m_PageInfo.line_size; i++)
    {
        line = &m_PageInfo.line_info[m_CurrentLine + i];
        rect.bottom = rect.top + h;
        DrawText(hdc, m_Text + line->start, line->length, &rect, DT_LEFT);
        rect.top += h;
        m_CurPageSize += line->length;
    }
    (*m_CurrentPos) = m_PageInfo.line_info[m_CurrentLine].start;
}

INT PageCache::GetCurPageSize(void)
{
    return m_CurPageSize;
}

INT PageCache::GetOnePageLineCount(void)
{
    return m_OnePageLineCount;
}

LONG PageCache::GetLineHeight(HDC hdc)
{
    SIZE sz = { 0 };
    GetTextExtentPoint32(hdc, _T("AaBbYyZz"), 8, &sz);
    return sz.cy + (*m_lineGap);
}

INT PageCache::GetCahceUnitSize(HDC hdc)
{
    SIZE sz = { 0 };
    INT w, h;
    GetTextExtentPoint32(hdc, _T("."), 1, &sz);
    w = (m_Rect.right - m_Rect.left) / sz.cx;
    h = (m_Rect.bottom - m_Rect.top) / sz.cy;
    return w * h;
}

void PageCache::LoadPageInfo(HDC hdc, INT maxw)
{
    INT MAX_FIND_SIZE = GetCahceUnitSize(hdc);
    INT pos1, pos2, pos3, pos4;
    INT i;
    INT start;
    INT length;
    LONG width;
    INT index;
    SIZE sz = { 0 };
    BOOL flag = TRUE;
    //MAX_FIND_SIZE = 1503;
    // pageup/lineup:         [pos1, pos2)
    // already in cache page: [pos2, pos3)
    // pagedown/linedown:     [pos3, pos4)

    // set startpos
    if (m_PageInfo.line_size > 0)
        pos2 = m_PageInfo.line_info[0].start;
    else
        pos2 = (*m_CurrentPos);
    pos1 = pos2 <= MAX_FIND_SIZE ? 0 : pos2 - MAX_FIND_SIZE;

    if (m_PageInfo.line_size > 0)
        pos3 = m_PageInfo.line_info[m_PageInfo.line_size - 1].start + m_PageInfo.line_info[m_PageInfo.line_size - 1].length;
    else
        pos3 = (*m_CurrentPos);
    pos4 = pos3 + MAX_FIND_SIZE >= m_Size ? m_Size : pos3 + MAX_FIND_SIZE;

    if (m_CurrentLine < 0)
    {
        flag = FALSE;

        // [pos1, pos2)
        start = pos1;
        length = 0;
        width = 0;
        index = 0;
        for (i = pos1; i < pos2; i++)
        {
            // new line
#if 0 // because do FormatText
            if (m_Text[i] == 0x0D && m_Text[i + 1] == 0x0A) // the last char in data is 0x00, So it won��t cross the memory.
            {
                i++;
                length++;
            }
#endif
            if (m_Text[i] == 0x0A)
            {
                length++;
                AddLine(start, length, index++);
                start = i + 1;
                length = 0;
                width = 0;
                continue;
            }

            // calc char width
            GetTextExtentPoint32(hdc, &m_Text[i], 1, &sz);
            width += sz.cx;
            if (width > maxw)
            {
                AddLine(start, length, index++);
                start = i;
                length = 1;
                width = sz.cx;
                continue;
            }
            length++;
        }
        if (width > 0 && width <= maxw)
        {
            AddLine(start, length, index++);
        }

        // fixed bug
        if (m_CurrentLine < 0 && m_PageInfo.line_info[0].start == 0)
        {
            m_CurrentLine = 0;
        }
    }

    if (flag)
    {
        // [pos3, pos4)
        start = pos3;
        length = 0;
        width = 0;
        index = 0;
        for (i = pos3; i < pos4; i++)
        {
            // new line
#if 0 // because do FormatText
            if (m_Text[i] == 0x0D && m_Text[i + 1] == 0x0A) // the last char in data is 0x00, So it won��t cross the memory.
            {
                i++;
                length++;
            }
#endif
            if (m_Text[i] == 0x0A)
            {
                length++;
                AddLine(start, length);
                start = i + 1;
                length = 0;
                width = 0;
                if (m_CurrentLine + m_OnePageLineCount <= m_PageInfo.line_size)
                    break;
                continue;
            }

            // calc char width
            GetTextExtentPoint32(hdc, &m_Text[i], 1, &sz);
            width += sz.cx;
            if (width > maxw)
            {
                AddLine(start, length);
                start = i;
                length = 1;
                width = sz.cx;
#if FAST_MODEL
                if (m_CurrentLine + m_OnePageLineCount <= m_PageInfo.line_size)
                    break;
#endif
                continue;
            }
            length++;
        }
        if (pos4 == m_Size)
        {
            if (width > 0 && width <= maxw)
            {
                AddLine(start, length);
            }
        }
        else
        {
            // Discard dirty line
        }
    }
}

void PageCache::AddLine(INT start, INT length, INT pos)
{
    const int UNIT_SIZE = 1024;

    if (!m_PageInfo.line_info)
    {
        m_PageInfo.line_size = 0;
        m_PageInfo.alloc_size = UNIT_SIZE;
        m_PageInfo.line_info = (line_info_t *)malloc(m_PageInfo.alloc_size * sizeof(line_info_t));
    }
    if (m_PageInfo.line_size >= m_PageInfo.alloc_size)
    {
        m_PageInfo.alloc_size += UNIT_SIZE;
        m_PageInfo.line_info = (line_info_t *)realloc(m_PageInfo.line_info, m_PageInfo.alloc_size * sizeof(line_info_t));
    }
    if (pos == -1)
    {
        m_PageInfo.line_info[m_PageInfo.line_size].start = start;
        m_PageInfo.line_info[m_PageInfo.line_size].length = length;
    }
    else
    {
        memcpy(&m_PageInfo.line_info[pos + 1], &m_PageInfo.line_info[pos], sizeof(line_info_t) * (m_PageInfo.line_size - pos));
        m_PageInfo.line_info[pos].start = start;
        m_PageInfo.line_info[pos].length = length;
        // set currentline
        m_CurrentLine++;
    }
    m_PageInfo.line_size++;
}

void PageCache::RemoveAllLine(BOOL freemem)
{
    if (freemem)
    {
        if (m_PageInfo.line_info)
            free(m_PageInfo.line_info);
        memset(&m_PageInfo, 0, sizeof(m_PageInfo));
    }
    else
    {
        m_PageInfo.line_size = 0;
    }
    m_CurrentLine = 0;
}

BOOL PageCache::IsValid(void)
{
    if (!m_Text || m_Size == 0)
        return FALSE;
    if (m_Rect.right - m_Rect.left == 0)
        return FALSE;
    if (m_Rect.bottom - m_Rect.top == 0)
        return FALSE;
    return TRUE;
}

void PageCache::UnitTest1(void)
{
#if TEST_MODEL
    assert(m_CurrentLine >= 0 && m_CurrentLine < m_PageInfo.line_size);
    if (m_CurrentLine + m_OnePageLineCount > m_PageInfo.line_size)
    {
        assert(m_PageInfo.line_info[m_PageInfo.line_size - 1].start + m_PageInfo.line_info[m_PageInfo.line_size - 1].length == m_Size);
    }
#endif
}

void PageCache::UnitTest2(void)
{
#if TEST_MODEL
    int i, v1, v2, v3;
    TCHAR *buf = NULL;
    for (i = 0; i < m_PageInfo.line_size; i++)
    {
        v1 = m_PageInfo.line_info[i].start;
        v2 = m_PageInfo.line_info[i].length;
        assert(v1 >= 0 && v2 > 0 && v1 + v2 <= m_Size);
        if (v1 + v2 == m_Size)
        {
            assert(i == m_PageInfo.line_size - 1);
        }
        if (i < m_PageInfo.line_size - 1)
        {
            v3 = m_PageInfo.line_info[i+1].start;
            assert(v3 > 0 && v1 + v2 == v3);
        }
    }
#endif
}