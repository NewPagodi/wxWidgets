///////////////////////////////////////////////////////////////////////////////
// Name:        src/aui/auibook.cpp
// Purpose:     wxaui: wx advanced user interface - notebook
// Author:      Benjamin I. Williams
// Modified by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Modified by: Jens Lody
// Created:     2006-06-28
// Copyright:   (C) Copyright 2006, Kirix Corporation, All Rights Reserved
//                            2012, Jens Lody for the code related to left
//                                  and right positioning
// Licence:     wxWindows Library Licence, Version 3.1
///////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_AUI

#include "wx/aui/auibook.h"

#ifndef WX_PRECOMP
    #include "wx/settings.h"
    #include "wx/dcclient.h"
    #include "wx/dcmemory.h"
#endif

#include "wx/aui/tabmdi.h"

#ifdef __WXMAC__
#include "wx/osx/private.h"
#endif

#include "wx/arrimpl.cpp"
WX_DEFINE_OBJARRAY(wxAuiNotebookPageArray)
WX_DEFINE_OBJARRAY(wxAuiTabContainerButtonArray)

wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_PAGE_CLOSE, wxAuiNotebookEvent);
wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_PAGE_CLOSED, wxAuiNotebookEvent);
wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_PAGE_CHANGING, wxAuiNotebookEvent);
wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_PAGE_CHANGED, wxAuiNotebookEvent);
wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_BUTTON, wxAuiNotebookEvent);
wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_BEGIN_DRAG, wxAuiNotebookEvent);
wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_END_DRAG, wxAuiNotebookEvent);
wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_CANCEL_DRAG, wxAuiNotebookEvent);
wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_DRAG_MOTION, wxAuiNotebookEvent);
wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_ALLOW_DND, wxAuiNotebookEvent);
wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_BG_DCLICK, wxAuiNotebookEvent);
wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_DRAG_DONE, wxAuiNotebookEvent);
wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_TAB_MIDDLE_UP, wxAuiNotebookEvent);
wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_TAB_MIDDLE_DOWN, wxAuiNotebookEvent);
wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_TAB_RIGHT_UP, wxAuiNotebookEvent);
wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_TAB_RIGHT_DOWN, wxAuiNotebookEvent);
wxDEFINE_EVENT(wxEVT_AUINOTEBOOK_TAB_DCLICK, wxAuiNotebookEvent);

wxIMPLEMENT_CLASS(wxAuiNotebook, wxBookCtrlBase)
wxIMPLEMENT_DYNAMIC_CLASS(wxAuiNotebookEvent, wxBookCtrlEvent)

bool TabHasCloseButton(unsigned int flags,wxAuiNotebookPage& page);


// -- wxAuiTabContainer class implementation --


// wxAuiTabContainer is a class which contains information about each
// tab.  It also can render an entire tab control to a specified DC.
// It's not a window class itself, because this code will be used by
// the wxFrameMananger, where it is disadvantageous to have separate
// windows for each tab control in the case of "docked tabs"

// A derived class, wxAuiTabCtrl, is an actual wxWindow-derived window
// which can be used as a tab control in the normal sense.
wxAuiTabContainer::wxAuiTabContainer(wxAuiTabArt* artProvider,wxAuiManager* mgr)
: m_focus(false)
, m_mgr(mgr)
, m_art(artProvider)
{
    m_tabOffset = 0;
    m_flags = 0;

    AddButton(wxAUI_BUTTON_LEFT, wxLEFT);
    AddButton(wxAUI_BUTTON_RIGHT, wxRIGHT);
    AddButton(wxAUI_BUTTON_WINDOWLIST, wxRIGHT);
    AddButton(wxAUI_BUTTON_CLOSE, wxRIGHT);
}

wxAuiTabContainer::~wxAuiTabContainer()
{
    unsigned int i;
    for(i=0;i<m_pages.GetCount();i++)
    {
        m_pages[i].GetWindow()->Disconnect( wxEVT_KEY_DOWN, wxKeyEventHandler(wxAuiTabContainer::OnChildKeyDown) );
    }
}

void wxAuiTabContainer::SetArtProvider(wxAuiTabArt* art)
{
    m_art = art;

    if (m_art)
    {
        m_art->SetFlags(m_flags);
    }
}

wxAuiTabArt* wxAuiTabContainer::GetArtProvider() const
{
    return m_art;
}

void wxAuiTabContainer::SetFlags(unsigned int flags)
{
    m_flags = flags;

    // check for new close button settings
    RemoveButton(wxAUI_BUTTON_LEFT);
    RemoveButton(wxAUI_BUTTON_RIGHT);
    RemoveButton(wxAUI_BUTTON_UP);
    RemoveButton(wxAUI_BUTTON_DOWN);
    RemoveButton(wxAUI_BUTTON_WINDOWLIST);
    RemoveButton(wxAUI_BUTTON_CLOSE);


    if (IsHorizontal())
    {
        if (flags & wxAUI_MGR_NB_SCROLL_BUTTONS)
        {
            AddButton(wxAUI_BUTTON_LEFT, wxLEFT);
            AddButton(wxAUI_BUTTON_RIGHT, wxRIGHT);
        }

        if (flags & wxAUI_MGR_NB_WINDOWLIST_BUTTON)
        {
            AddButton(wxAUI_BUTTON_WINDOWLIST, wxRIGHT);
        }

        if (flags & wxAUI_MGR_NB_CLOSE_BUTTON)
        {
            AddButton(wxAUI_BUTTON_CLOSE, wxRIGHT);
        }
    }
    else
    {
        if (flags & wxAUI_NB_SCROLL_BUTTONS)
        {
            AddButton(wxAUI_BUTTON_UP, wxUP);
            AddButton(wxAUI_BUTTON_DOWN, wxDOWN);
        }

        if (flags & wxAUI_NB_WINDOWLIST_BUTTON)
        {
            AddButton(wxAUI_BUTTON_WINDOWLIST, wxUP);
        }

        if (flags & wxAUI_NB_CLOSE_BUTTON)
        {
            AddButton(wxAUI_BUTTON_CLOSE, wxUP);
        }
    }
    if (m_art)
    {
        m_art->SetFlags(m_flags);
    }
}

unsigned int wxAuiTabContainer::GetFlags() const
{
    return m_flags;
}

bool wxAuiTabContainer::HasFlag(int flag) const
{
    return (m_flags & flag) != 0;
}


bool wxAuiTabContainer::IsHorizontal() const
{
    return HasFlag(wxAUI_NB_TOP | wxAUI_NB_BOTTOM);
}

void wxAuiTabContainer::SetNormalFont(const wxFont& font)
{
    m_art->SetNormalFont(font);
}

void wxAuiTabContainer::SetSelectedFont(const wxFont& font)
{
    m_art->SetSelectedFont(font);
}

void wxAuiTabContainer::SetMeasuringFont(const wxFont& font)
{
    m_art->SetMeasuringFont(font);
}

void wxAuiTabContainer::SetRect(const wxRect& rect)
{
    m_targetRect = rect;
    m_rect.x = 0;
    m_rect.y = 0;
    m_rect.width = rect.width;
    m_rect.height = rect.height;

    if (m_art)
    {
        m_art->SetSizingInfo(rect.GetSize(), m_pages.GetCount());
    }
}

bool wxAuiTabContainer::AddPage(wxWindow* page,
                                const wxAuiNotebookPage& info)
{
	info.GetWindow()->Connect( wxEVT_KEY_DOWN, wxKeyEventHandler(wxAuiTabContainer::OnChildKeyDown)  ,NULL,this);

    wxAuiNotebookPage page_info;
    page_info = info;
    page_info.window = page;

    m_pages.Add(page_info);

    // let the art provider know how many pages we have
    if (m_art)
    {
        m_art->SetSizingInfo(m_rect.GetSize(), m_pages.GetCount());
    }

    return true;
}

bool wxAuiTabContainer::InsertPage(wxWindow* page,
                                   const wxAuiNotebookPage& info,
                                   size_t idx)
{
    info.GetWindow()->Connect( wxEVT_KEY_DOWN, wxKeyEventHandler(wxAuiTabContainer::OnChildKeyDown)  ,NULL,this);

    wxAuiNotebookPage page_info;
    page_info = info;
    page_info.window = page;

    if (idx >= m_pages.GetCount())
    {
        page_info.Page(m_pages.size());
        m_pages.Add(page_info);
    }
    else
    {
        m_pages.Insert(page_info, idx);
        for(size_t i=idx; i < m_pages.GetCount(); i++)
            m_pages[i].Page(i);
    }

    // let the art provider know how many pages we have
    if (m_art)
    {
        m_art->SetSizingInfo(m_rect.GetSize(), m_pages.GetCount());
    }

    return true;
}

bool wxAuiTabContainer::MovePage(wxWindow* page,
                                 size_t new_idx)
{
    int idx = GetIdxFromWindow(page);
    if (idx == -1)
        return false;

    // get page entry, make a copy of it
    wxAuiNotebookPage p = GetPage(idx);

    // remove old page entry
    RemovePage(page);

    // insert page where it should be
    InsertPage(page, p, new_idx);

    return true;
}

bool wxAuiTabContainer::RemovePage(wxWindow* wnd)
{
    size_t i, page_count = m_pages.GetCount();
    for (i = 0; i < page_count; ++i)
    {
        wxAuiNotebookPage& page = m_pages.Item(i);
        if (page.window == wnd)
        {
            m_pages.RemoveAt(i);

            // let the art provider know how many pages we have
            if (m_art)
            {
                m_art->SetSizingInfo(m_rect.GetSize(), m_pages.GetCount());
            }

            wnd->Disconnect( wxEVT_KEY_DOWN, wxKeyEventHandler(wxAuiTabContainer::OnChildKeyDown) );
            return true;
        }
    }

    return false;
}

bool wxAuiTabContainer::SetActivePage(wxWindow* wnd)
{
    bool found = false;

    size_t i, page_count = m_pages.GetCount();
    for (i = 0; i < page_count; ++i)
    {
        wxAuiNotebookPage& page = m_pages.Item(i);
        if (page.window == wnd)
        {
            if (page.HasFlag(wxAuiPaneInfo::optionActiveNotebook) && page.GetWindow()->IsShown())
            {
                SetFocus(true);
            }
            else
            {
                page.GetWindow()->SetFocus();
                page.GetWindow()->Show(true);
                page.SetFlag(wxAuiPaneInfo::optionActive,true);
                page.SetFlag(wxAuiPaneInfo::optionActiveNotebook,true);
                found = true;
                MakeTabVisible(i);
            }
        }
        else
        {
            page.GetWindow()->Show(false);
            page.SetFlag(wxAuiPaneInfo::optionActive,false);
            page.SetFlag(wxAuiPaneInfo::optionActiveNotebook,false);
        }
    }

    return found;
}

void wxAuiTabContainer::SetNoneActive()
{
    size_t i, page_count = m_pages.GetCount();
    for (i = 0; i < page_count; ++i)
    {
        wxAuiNotebookPage& page = m_pages.Item(i);
        page.SetFlag(wxAuiPaneInfo::optionActiveNotebook,false);
    }
}

bool wxAuiTabContainer::SetActivePage(size_t page)
{
    if (page >= m_pages.GetCount())
        return false;

    return SetActivePage(m_pages.Item(page).window);
}

int wxAuiTabContainer::GetActivePage() const
{
    size_t i, page_count = m_pages.GetCount();
    for (i = 0; i < page_count; ++i)
    {
        wxAuiNotebookPage& page = m_pages.Item(i);
        if (page.HasFlag(wxAuiPaneInfo::optionActiveNotebook))
            return i;
    }

    return -1;
}

wxWindow* wxAuiTabContainer::GetWindowFromIdx(size_t idx) const
{
    if (idx >= m_pages.GetCount())
        return NULL;

    return m_pages[idx].GetWindow();
}

int wxAuiTabContainer::GetIdxFromWindow(wxWindow* wnd) const
{
    const size_t page_count = m_pages.GetCount();
    for ( size_t i = 0; i < page_count; ++i )
    {
        wxAuiNotebookPage& page = m_pages.Item(i);
        if (page.window == wnd)
            return i;
    }
    return wxNOT_FOUND;
}

wxAuiNotebookPage& wxAuiTabContainer::GetPage(size_t idx)
{
    wxASSERT_MSG(idx < m_pages.GetCount(), wxT("Invalid Page index"));

    return m_pages[idx];
}

const wxAuiNotebookPage& wxAuiTabContainer::GetPage(size_t idx) const
{
    wxASSERT_MSG(idx < m_pages.GetCount(), wxT("Invalid Page index"));

    return m_pages[idx];
}

wxAuiNotebookPageArray& wxAuiTabContainer::GetPages()
{
    return m_pages;
}

size_t wxAuiTabContainer::GetPageCount() const
{
    return m_pages.GetCount();
}

void wxAuiTabContainer::AddButton(int id,
                                  int location,
                                  const wxBitmap& normalBitmap,
                                  const wxBitmap& disabledBitmap)
{
    wxAuiTabContainerButton button;
    button.id = id;
    button.bitmap = normalBitmap;
    button.disBitmap = disabledBitmap;
    button.location = location;
    button.curState = wxAUI_BUTTON_STATE_NORMAL;

    m_buttons.Add(button);
}

void wxAuiTabContainer::RemoveButton(int id)
{
    size_t i, button_count = m_buttons.GetCount();

    for (i = 0; i < button_count; ++i)
    {
        if (m_buttons.Item(i).id == id)
        {
            m_buttons.RemoveAt(i);
            return;
        }
    }
}



size_t wxAuiTabContainer::GetTabOffset() const
{
    return m_tabOffset;
}

void wxAuiTabContainer::SetTabOffset(size_t offset)
{
    m_tabOffset = offset;
}



void wxAuiTabContainer::DrawTabs(wxDC* dc, wxWindow* wnd,const wxRect& rect)
{
    SetRect(rect);
    Render(dc,wnd);
}

void wxAuiTabContainer::CalculateRequiredHeight(wxDC& dc,wxWindow* wnd,int& totalSize,int& visibleSize) const
{
    totalSize=0;
    visibleSize=0;

    size_t i;
    size_t pageCount = m_pages.GetCount();
    for (i = 0; i < pageCount; ++i)
    {
        wxAuiNotebookPage& page = m_pages.Item(i);

        // determine if a close button is on this tab
        bool closeButton = TabHasCloseButton(m_flags,page);
		bool isCompact = page.IsCompactTab() && !page.HasFlag(wxAuiPaneInfo::optionActiveNotebook) && page.GetIcon().IsOk();

        int extent = 0;
        wxSize size = m_art->GetTabSize(dc,
                            wnd,
                            isCompact ? wxString() : page.GetCaption(),
                            page.GetIcon(),
                            page.HasFlag(wxAuiPaneInfo::optionActiveNotebook),
                            closeButton ?
                              wxAUI_BUTTON_STATE_NORMAL :
                              wxAUI_BUTTON_STATE_HIDDEN,
                            &extent);

        if (IsHorizontal())
        {
            totalSize = std::max(totalSize,size.y);
        }
        else
        {
            if (i+1 < pageCount)
                totalSize += extent;
            else
                totalSize += size.y;
        }
        if (i >= m_tabOffset)
        {
            if (IsHorizontal())
            {
                visibleSize = std::max(visibleSize,size.y);
            }
            else
            {
                if (i+1 < pageCount)
                    visibleSize += extent;
                else
                    visibleSize += size.y;
            }
        }
    }
}


void wxAuiTabContainer::CalculateRequiredWidth(wxDC& dc,wxWindow* wnd,int& totalSize,int& visibleSize) const
{
    totalSize=0;
    visibleSize=0;

    size_t i;
    size_t pageCount = m_pages.GetCount();
    for (i = 0; i < pageCount; ++i)
    {
        wxAuiNotebookPage& page = m_pages.Item(i);

        // determine if a close button is on this tab
        bool closeButton = TabHasCloseButton(m_flags,page);
		bool isCompact = page.IsCompactTab() && !page.HasFlag(wxAuiPaneInfo::optionActiveNotebook) && page.GetIcon().IsOk();

        int extent = 0;
        wxSize size = m_art->GetTabSize(dc,
                            wnd,
                            isCompact ? wxString() : page.GetCaption(),
                            page.GetIcon(),
                            page.HasFlag(wxAuiPaneInfo::optionActiveNotebook),
                            closeButton ?
                              wxAUI_BUTTON_STATE_NORMAL :
                              wxAUI_BUTTON_STATE_HIDDEN,
                            &extent);

        if (IsHorizontal())
        {
            if (i+1 < pageCount)
                totalSize += extent;
            else
                totalSize += size.x;
        }
        else
        {
             totalSize = std::max(totalSize,size.x);
        }

        if (i >= m_tabOffset)
        {
            if (IsHorizontal())
            {
                if (i+1 < pageCount)
                    visibleSize += extent;
                else
                    visibleSize += size.x;
            }
            else
            {
                visibleSize = std::max(visibleSize,size.x);
            }
        }
    }
}

// Render() renders the tab catalog to the specified DC
// It is a virtual function and can be overridden to
// provide custom drawing capabilities
void wxAuiTabContainer::Render(wxDC* raw_dc, wxWindow* wnd)
{
    if (!raw_dc || !raw_dc->IsOk() || m_rect.IsEmpty())
        return;

    wxMemoryDC dc;

    // use the same layout direction as the window DC uses to ensure that the
    // text is rendered correctly
    dc.SetLayoutDirection(raw_dc->GetLayoutDirection());

    wxBitmap bmp;
    size_t i;
    size_t page_count = m_pages.GetCount();
    size_t button_count = m_buttons.GetCount();

    // create off-screen bitmap
    bmp.Create(m_rect.GetWidth(), m_rect.GetHeight());
    dc.SelectObject(bmp);

    if (!dc.IsOk())
        return;

    // draw background
    m_art->DrawBackground(dc, wnd, m_rect);

    // first we create all buttons on the right/bottom, except the forward arrows
    // so we can determine, whether we need the backward- and forward-arrows

    // draw buttons
    int backwButtonsSize = 0;
    int forwButtonsSize = 0;


    int locationBtnRightTop;
    int locationBtnLeftBottom;
    int idBtnLeftBottom;
    int idBtnArrowRightUp;
    int testSize;

    if (IsHorizontal())
    {
        locationBtnRightTop = wxRIGHT;
        locationBtnLeftBottom = wxLEFT;
        idBtnLeftBottom = wxAUI_BUTTON_LEFT;
        idBtnArrowRightUp = wxAUI_BUTTON_RIGHT;
        testSize = m_rect.GetWidth();
    }
    else
    {
        locationBtnRightTop = wxUP;
        locationBtnLeftBottom = wxDOWN;
        idBtnLeftBottom = wxAUI_BUTTON_DOWN;
        idBtnArrowRightUp = wxAUI_BUTTON_UP;
        testSize = m_rect.GetHeight();
    }


    // draw the forward buttons on the top
    for (i = button_count; i > 0 ; --i)
    {
        wxAuiTabContainerButton& button = m_buttons.Item(i - 1);
        if (button.location != locationBtnRightTop)
            continue;
        if (button.curState & wxAUI_BUTTON_STATE_HIDDEN)
            continue;
        if (button.id == idBtnArrowRightUp)
            continue;

        wxRect button_rect = m_rect;
        button_rect.SetX(1);
        button_rect.SetY(1);

        m_art->DrawButton(dc,
                          wnd,
                          button_rect,
                          button.id,
                          button.curState,
                          locationBtnRightTop,
                          &button.rect);

        if (IsHorizontal())
        {
            forwButtonsSize += button.rect.GetWidth();
        }
        else
        {
            backwButtonsSize = wxMax(backwButtonsSize, button.rect.GetHeight());
        }

    }

    // find out if size of tabs is larger than can be
    // afforded on screen
    int totalSize = 0;
    int visibleSize = 0;
    int btn_size;
    if(IsHorizontal())
    {
        CalculateRequiredWidth(dc,wnd,totalSize,visibleSize);
        btn_size = forwButtonsSize;
    }
    else
    {
        CalculateRequiredHeight(dc,wnd,totalSize,visibleSize);
        btn_size = backwButtonsSize;
    }
    // ensure, we show as max tabs as possible
    while (m_tabOffset > 0 && IsTabVisible(page_count-1, m_tabOffset-1, &dc, wnd) )
            --m_tabOffset;

    // show up/down buttons
    for (i = 0; i < button_count; ++i)
    {
        wxAuiTabContainerButton& button = m_buttons.Item(i);
        if (button.id == idBtnArrowRightUp || button.id == idBtnLeftBottom)
        {
            if (totalSize > (testSize - btn_size - m_art->GetIndentSize()) || m_tabOffset != 0)
                button.curState &= ~wxAUI_BUTTON_STATE_HIDDEN;
            else
                button.curState |= wxAUI_BUTTON_STATE_HIDDEN;
        }
    }

    // determine whether left/up button should be enabled
    for (i = 0; i < button_count; ++i)
    {
        wxAuiTabContainerButton& button = m_buttons.Item(i);
        if (button.id == wxAUI_BUTTON_LEFT || button.id == wxAUI_BUTTON_UP)
        {
            if (m_tabOffset == 0)
                button.curState |= wxAUI_BUTTON_STATE_DISABLED;
            else
                button.curState &= ~wxAUI_BUTTON_STATE_DISABLED;
        }
    }

    // now draw forward arrows, keep the rect where it should be drawn, so we are
    // able to redraw it, if it should be enabled. We can only determine this, if it is actually drawn
    // because we do not know it's exact size
    wxRect forw_btn_rect;

    for (i = button_count; i > 0; --i)
    {
        wxAuiTabContainerButton& button = m_buttons.Item(i - 1);

        if (button.location != locationBtnRightTop)
            continue;
        if (button.id != idBtnArrowRightUp)
            continue;
        if (button.curState & wxAUI_BUTTON_STATE_HIDDEN)
            continue;

        wxRect button_rect = m_rect;
        button_rect.SetY(1);

        if (IsHorizontal())
        {
            forw_btn_rect = button_rect;
        }

        m_art->DrawButton(dc,
                          wnd,
                          button_rect,
                          button.id,
                          button.curState,
                          locationBtnRightTop,
                          &button.rect);

        if (IsHorizontal())
        {
            forwButtonsSize += button.rect.GetWidth();
        }
        else
        {
            backwButtonsSize = wxMax(backwButtonsSize, button.rect.GetHeight());
        }
    }



    // draw the buttons on the bottom side

    for (i = button_count; i > 0 ; --i)
    {
        wxAuiTabContainerButton& button = m_buttons.Item(i - 1);

        if (button.location != locationBtnLeftBottom)
            continue;
        if (button.curState & wxAUI_BUTTON_STATE_HIDDEN)
            continue;

        wxRect button_rect = m_rect;
        button_rect.SetX(1);
        button_rect.SetY(1);

        if (!IsHorizontal() && button.id == wxAUI_BUTTON_DOWN)
        {
            forw_btn_rect = button_rect;
        }

        m_art->DrawButton(dc,
                          wnd,
                          button_rect,
                          button.id,
                          button.curState,
                          locationBtnLeftBottom,
                          &button.rect);

        if (IsHorizontal())
        {
            backwButtonsSize += button.rect.GetWidth();
        }
        else
        {
            forwButtonsSize = wxMax(forwButtonsSize, button.rect.GetHeight());
        }
    }

    // this is not a typo, we use the size we determined when drawing the top (aka backw) buttons
    int offset = backwButtonsSize;

    if (offset == 0)
        offset += m_art->GetIndentSize();


    // prepare the tab-close-button array
    // make sure tab button entries which aren't used are marked as hidden
    for (i = page_count; i < m_tabCloseButtons.GetCount(); ++i)
        m_tabCloseButtons.Item(i).curState = wxAUI_BUTTON_STATE_HIDDEN;

    // make sure there are enough tab button entries to accommodate all tabs
    while (m_tabCloseButtons.GetCount() < page_count)
    {
        wxAuiTabContainerButton tempbtn;
        tempbtn.id = wxAUI_BUTTON_CLOSE;
        tempbtn.location = wxCENTER;
        tempbtn.curState = wxAUI_BUTTON_STATE_HIDDEN;
        m_tabCloseButtons.Add(tempbtn);
    }


    // buttons before the tab offset must be set to hidden
    for (i = 0; i < m_tabOffset; ++i)
    {
        m_tabCloseButtons.Item(i).curState = wxAUI_BUTTON_STATE_HIDDEN;
    }

    // determine whether right/down button should be enabled
    for (i = 0; i < button_count; ++i)
    {
        wxAuiTabContainerButton& button = m_buttons.Item(i);
        if (button.id == wxAUI_BUTTON_RIGHT || button.id == wxAUI_BUTTON_DOWN)
        {
            if (visibleSize < testSize - (backwButtonsSize + forwButtonsSize))
                button.curState |= wxAUI_BUTTON_STATE_DISABLED;
            else
                button.curState &= ~wxAUI_BUTTON_STATE_DISABLED;

            m_art->DrawButton(dc,
                              wnd,
                              forw_btn_rect,
                              button.id,
                              button.curState,
                              button.location,
                              &button.rect);
        }
    }

    // draw the tabs

    size_t active = 999;
    int active_offset = 0;
    wxRect active_rect;

    int extent = 0;
    wxRect rect = m_rect;
    if (IsHorizontal())
    {
        rect.y = 0;
        rect.height = m_rect.height;
    }
    else
    {
        rect.x = 0;
        rect.width = m_rect.width;
    }

    for (i = m_tabOffset; i < page_count; ++i)
    {
        wxAuiNotebookPage& page = m_pages.Item(i);
        wxAuiTabContainerButton& tab_button = m_tabCloseButtons.Item(i);


        if (TabHasCloseButton(m_flags,page))
        {
            if (tab_button.curState == wxAUI_BUTTON_STATE_HIDDEN)
            {
                tab_button.id = wxAUI_BUTTON_CLOSE;
                tab_button.curState = wxAUI_BUTTON_STATE_NORMAL;
                tab_button.location = wxCENTER;
            }
        }
        else
        {
            tab_button.curState = wxAUI_BUTTON_STATE_HIDDEN;
        }

        if (IsHorizontal())
        {
            rect.x = offset;
            rect.width = m_rect.width - forwButtonsSize - offset - 2;
            if (rect.width <= 0)
                break;
        }
        else
        {
             rect.y = offset;
             rect.height = m_rect.height - forwButtonsSize - offset - 2;
             if (rect.height <= 0)
                 break;
        }

        wxRect page_rect = page.GetRect();
        m_art->DrawTab(dc,
                       wnd,
                       page,
                       rect,
                       tab_button.curState,
                       HasFocus(),
                       &page_rect,
                       &tab_button.rect,
                       &extent);
        page.Rect(page_rect);

        if (page.HasFlag(wxAuiPaneInfo::optionActiveNotebook))
        {
            active = i;
            active_offset = offset;
            active_rect = rect;
        }

        offset += extent;
    }


    // make sure to deactivate buttons which are off the screen to the right
    for (++i; i < m_tabCloseButtons.GetCount(); ++i)
    {
        m_tabCloseButtons.Item(i).curState = wxAUI_BUTTON_STATE_HIDDEN;
    }


    // draw the active tab again so it stands in the foreground
    if (active >= m_tabOffset && active < m_pages.GetCount())
    {
        wxAuiNotebookPage& page = m_pages.Item(active);

        wxAuiTabContainerButton& tab_button = m_tabCloseButtons.Item(active);

        rect.x = active_offset;
        
        wxRect page_rect = page.GetRect();
        m_art->DrawTab(dc,
                       wnd,
                       page,
                       active_rect,
                       tab_button.curState,
                       HasFocus(),
                       &page_rect,
                       &tab_button.rect,
                       &extent);
        page.Rect(page_rect);
    }


    raw_dc->Blit(m_targetRect.x, m_targetRect.y,
                 m_targetRect.GetWidth(), m_targetRect.GetHeight(),
                 &dc, 0, 0);
}

// Is the tab visible?
bool wxAuiTabContainer::IsTabVisible(int tabPage, int tabOffset, wxDC* dc, wxWindow* wnd)
{
    if (!dc || !dc->IsOk())
        return false;

    size_t i;
    size_t page_count = m_pages.GetCount();
    size_t button_count = m_buttons.GetCount();

    // Hasn't been rendered yet; assume it's visible
    if (m_tabCloseButtons.GetCount() < page_count)
        return true;

    // First check if both buttons are disabled - if so, there's no need to
    // check further for visibility.
    int arrowButtonVisibleCount = 0;
    for (i = 0; i < button_count; ++i)
    {
        wxAuiTabContainerButton& button = m_buttons.Item(i);
        if (button.id == wxAUI_BUTTON_LEFT ||
            button.id == wxAUI_BUTTON_RIGHT ||
            button.id == wxAUI_BUTTON_UP ||
            button.id == wxAUI_BUTTON_DOWN)
        {
            if ((button.curState & wxAUI_BUTTON_STATE_HIDDEN) == 0)
                arrowButtonVisibleCount ++;
        }
    }

    // Tab must be visible
    if (arrowButtonVisibleCount == 0)
        return true;

    // If tab is less than the given offset, it must be invisible by definition
    if (tabPage < tabOffset)
        return false;

    // draw buttons
    int backwButtonsSize = 0;
    int forwButtonsSize = 0;

    int offset = 0;

    // calculate size of the buttons on the right/bottom side
    if (IsHorizontal())
    {
        offset = m_rect.x + m_rect.width;
        for (i = 0; i < button_count; ++i)
        {
            wxAuiTabContainerButton& button = m_buttons.Item(button_count - i - 1);
            if (button.location != wxRIGHT)
                continue;
            if (button.curState & wxAUI_BUTTON_STATE_HIDDEN)
                continue;

            offset -= button.rect.GetWidth();
            forwButtonsSize += button.rect.GetWidth();
        }
    }
    else // must be wxAUI_NB_TOP or wxAUI_NB_BOTTOM
    {
        for (i = 0; i < button_count; ++i)
        {
            wxAuiTabContainerButton& button = m_buttons.Item(button_count - i - 1);

            if (button.location != wxDOWN)
                continue;
            if (button.curState & wxAUI_BUTTON_STATE_HIDDEN)
                continue;

            offset = wxMax(offset, button.rect.GetHeight());
            forwButtonsSize = offset;
        }
    }

    offset = 0;

    int location_btn_left_top;
    int testSize;

    if (IsHorizontal())
    {
        location_btn_left_top = wxLEFT;
        testSize = m_rect.GetWidth();
    }
    else
    {
        location_btn_left_top = wxUP;
        testSize = m_rect.GetHeight();
    }


    for (i = 0; i < button_count; ++i)
    {
        wxAuiTabContainerButton& button = m_buttons.Item(button_count - i - 1);

        if (button.location != location_btn_left_top)
            continue;
        if (button.curState & wxAUI_BUTTON_STATE_HIDDEN)
            continue;

        if (IsHorizontal())
        {
            offset += button.rect.GetWidth();
            backwButtonsSize += button.rect.GetWidth();
        }
        else
        {
            offset = wxMax(offset, button.rect.GetHeight());
            backwButtonsSize = offset;
        }
    }

    offset = backwButtonsSize;

    // calculate size of the buttons on the left/top side
    if (offset == 0)
        offset += m_art->GetIndentSize();

    // See if the given page is visible at the given tab offset (effectively scroll position)
    for (i = tabOffset; i < page_count; ++i)
    {
        wxAuiNotebookPage& page = m_pages.Item(i);
        wxAuiTabContainerButton& tab_button = m_tabCloseButtons.Item(i);
		bool isCompact = page.IsCompactTab() && !page.HasFlag(wxAuiPaneInfo::optionActiveNotebook) && page.GetIcon().IsOk();

        if ((testSize - forwButtonsSize - offset - 2) <= 0)
            return false; // haven't found the tab, and we've run out of space, so return false

        int extent = 0;
        wxSize size = m_art->GetTabSize(*dc,
                            wnd,
                            isCompact ? wxString() : page.GetCaption(),
                            page.GetIcon(),
                            page.HasFlag(wxAuiPaneInfo::optionActiveNotebook),
                            tab_button.curState,
                            &extent);

        offset += extent;

        if (i == (size_t) tabPage)
        {
            // If not all of the tab is visible, and supposing there's space to display it all,
            // we could do better so we return false.
            if (((testSize - forwButtonsSize - offset - 2) <= 0) && ((testSize - forwButtonsSize - backwButtonsSize) > extent))
                return false;
            else
                return true;
        }
    }

    // Shouldn't really get here, but if it does, assume the tab is visible to prevent
    // further looping in calling code.
    return true;
}

// Make the tab visible if it wasn't already
void wxAuiTabContainer::MakeTabVisible(int tabPage)
{
    wxWindow* win = m_pages.Item(tabPage).GetWindow();
    wxClientDC dc(win);
    if (!IsTabVisible(tabPage, GetTabOffset(), & dc, win))
    {
        int i;
        for (i = 0; i < (int) m_pages.GetCount(); i++)
        {
            if (IsTabVisible(tabPage, i, & dc, win))
            {
                SetTabOffset(i);
                win->Refresh();
                return;
            }
        }
    }
}

// TabHitTest() tests if a tab was hit, passing the window pointer
// back if that condition was fulfilled.  The function returns
// true if a tab was hit, otherwise false
bool wxAuiTabContainer::TabHitTest(int x, int y, wxWindow** hit) const
{
    //Remap the x and y to our internal rect
    x += m_rect.x-m_targetRect.x;
    y += m_rect.y-m_targetRect.y;

    wxRect rect = m_rect;
    if (m_flags & (wxAUI_NB_LEFT | wxAUI_NB_RIGHT))
    {
        int bottomSize = 0;
        for (size_t i = m_buttons.GetCount(); i > 0 ; --i)
        {
            wxAuiTabContainerButton& button = m_buttons.Item(i - 1);
            if (button.location == wxDOWN)
            {
                // if we have hit one of the bottom buttons step out, or we can not click it
                if (button.rect.Contains(x,y))
                {
                    bottomSize = 0;
                    break;
                }
                bottomSize = wxMax(bottomSize, button.rect.GetHeight());
            }
        }
        if (bottomSize)
           rect.height -= (bottomSize + 2);
    }

    if (!rect.Contains(x,y))
        return false;

    wxAuiTabContainerButton* btn = NULL;
    if (ButtonHitTest(x, y, &btn) && !(btn->curState & wxAUI_BUTTON_STATE_DISABLED))
    {
        if (m_buttons.Index(*btn) != wxNOT_FOUND)
            return false;
    }

    size_t i, page_count = m_pages.GetCount();

    for (i = m_tabOffset; i < page_count; ++i)
    {
        wxAuiNotebookPage& page = m_pages.Item(i);
        if (page.rect.Contains(x,y))
        {
            if (hit)
                *hit = page.window;
            return true;
        }
    }

    return false;
}

// ButtonHitTest() tests if a button was hit. The function returns
// true if a button was hit, otherwise false
bool wxAuiTabContainer::ButtonHitTest(int x, int y,
                                      wxAuiTabContainerButton** hit) const
{
    //Remap the x and y to our internal rect
    x += m_rect.x-m_targetRect.x;
    y += m_rect.y-m_targetRect.y;

    wxRect rect = m_rect;
    if (m_flags & (wxAUI_NB_LEFT | wxAUI_NB_RIGHT))
    {
        int bottomSize = 0;
        for (size_t i = m_buttons.GetCount(); i > 0 ; --i)
        {
            wxAuiTabContainerButton& button = m_buttons.Item(i - 1);
            if (button.location == wxDOWN)
            {
                // if we have hit one of the bottom buttons step out, or we can not click it
                if (button.rect.Contains(x,y))
                {
                    bottomSize = 0;
                    break;
                }
                bottomSize = wxMax(bottomSize, button.rect.GetHeight());
            }
        }
        if (bottomSize)
            rect.height -= (bottomSize + 2);
    }

    if (!rect.Contains(x,y))
        return false;

    size_t i, button_count;


    button_count = m_buttons.GetCount();
    for (i = 0; i < button_count; ++i)
    {
        wxAuiTabContainerButton& button = m_buttons.Item(i);
        if (button.rect.Contains(x,y) &&
            !(button.curState & (wxAUI_BUTTON_STATE_HIDDEN |
                                   wxAUI_BUTTON_STATE_DISABLED)))
        {
            if (hit)
                *hit = &button;
            return true;
        }
    }

    button_count = m_tabCloseButtons.GetCount();
    for (i = 0; i < button_count; ++i)
    {
        wxAuiTabContainerButton& button = m_tabCloseButtons.Item(i);
        if (button.rect.Contains(x,y) &&
            !(button.curState & (wxAUI_BUTTON_STATE_HIDDEN |
                                   wxAUI_BUTTON_STATE_DISABLED)))
        {
            if (hit)
                *hit = &button;
            return true;
        }
    }

    return false;
}

void wxAuiTabContainer::OnChildKeyDown(wxKeyEvent& evt)
{
    evt.Skip();
    if (HasFocus())
    {
        if (evt.GetKeyCode() == WXK_LEFT)
        {
            SetActivePage(GetActivePage()-1);
            SetFocus(true);
            evt.Skip(false);

            m_mgr->Update();
        }
        else if (evt.GetKeyCode() == WXK_RIGHT)
        {
            SetActivePage(GetActivePage()+1);
            SetFocus(true);
            evt.Skip(false);

            m_mgr->Update();
        }
    }
    return;
}







// extern functions from framemanager.cpp
extern void DoInsertDockLayer(wxAuiPaneInfoArray&,int,int);
extern void DoInsertDockRow(wxAuiPaneInfoArray&,int,int,int);
extern void DoInsertPane(wxAuiPaneInfoArray&,int,int,int,int);
extern void DoInsertPage(wxAuiPaneInfoArray&,int,int,int,int,int);


wxBEGIN_EVENT_TABLE(wxAuiNotebook, wxControl)
EVT_SIZE(wxAuiNotebook::OnSize)
EVT_AUI_ALLOW_DND(wxAuiNotebook::OnPaneDrop)
wxEND_EVENT_TABLE()

// -- wxAuiNotebook class implementation --
bool wxAuiNotebook::Create(wxWindow* parent,
                           wxWindowID id,
                           const wxPoint& pos,
                           const wxSize& size,
                           long style)
{
    if(!wxControl::Create(parent, id, pos, size, style))
        return false;

    Init(style);
    return true;
}

// Init contains common initialization
// code called by all constructors
void wxAuiNotebook::Init(long style)
{
    SetName(wxT("wxAuiNotebook"));
    SetArtProvider(new wxAuiDefaultTabArt);

    m_mgr.SetManagedWindow(this);
    m_mgr.SetDockSizeConstraint(1.0, 1.0); // no dock size constraint
    SetWindowStyleFlag(style);

}

wxAuiNotebook::~wxAuiNotebook()
{
    // Indicate we're deleting pages
    SendDestroyEvent();

    while ( GetPageCount() > 0 )
        DeletePage(0);

    m_mgr.UnInit();
}

void wxAuiNotebook::SetArtProvider(wxAuiTabArt* art)
{
    m_mgr.SetTabArtProvider(art);
    UpdateTabCtrlSize();
}

// SetTabCtrlHeight() is the highest-level override of the
// tab height.  A call to this function effectively enforces a
// specified tab ctrl height, overriding all other considerations,
// such as text or bitmap height.  It overrides any call to
// SetUniformBitmapSize().  Specifying a height of -1 reverts
// any previous call and returns to the default behaviour

void wxAuiNotebook::SetTabCtrlHeight(int height)
{
    GetArtProvider()->SetTabCtrlHeight(height);
}

void wxAuiNotebook::SetTabCtrlWidth(int width)
{
    GetArtProvider()->SetTabCtrlWidth(width);
}


// SetUniformBitmapSize() ensures that all tabs will have
// the same height, even if some tabs don't have bitmaps
// Passing wxDefaultSize to this function will instruct
// the control to use dynamic tab height-- so when a tab
// with a large bitmap is added, the tab ctrl's height will
// automatically increase to accommodate the bitmap

void wxAuiNotebook::SetUniformBitmapSize(const wxSize& size)
{
    GetArtProvider()->SetUniformBitmapSize(size);
}

// UpdateTabCtrlSize() does the actual tab resizing. It's meant
// to be used internally
bool wxAuiNotebook::UpdateTabCtrlSize()
{
    // get the tab ctrl height we will use
    wxSize size = CalculateTabCtrlSize();

    // if the tab control height needs to change, update
    // all of our tab controls with the new height
    if (m_mgr.GetTabArtProvider()->GetRequestedSize() == size)
        return false;


    // Are we supposed to update from the 'size' or what?

    return true;
}

wxSize wxAuiNotebook::CalculateTabCtrlSize()
{
    // find out new best tab size
    wxAuiNotebookPageArray allPanes;
    unsigned int i;
    for(i=0;i<m_mgr.GetPaneCount();i++)
    {
        allPanes.Add(m_mgr.GetPane(i));
    }

    wxSize tab_size = m_mgr.GetTabArtProvider()->GetBestTabSize((wxWindow*)this, allPanes,
        m_mgr.GetTabArtProvider()->GetRequiredBitmapSize());
    // if a fixed tab ctrl height is specified,
    // just use that instead of calculating a
    // tab height
    if (m_mgr.GetTabArtProvider()->GetRequestedSize().y != -1)
        tab_size.y = m_mgr.GetTabArtProvider()->GetRequestedSize().y;

    return tab_size;
}


wxAuiTabArt* wxAuiNotebook::GetArtProvider() const
{
    return m_mgr.GetTabArtProvider();
}

void wxAuiNotebook::SetWindowStyleFlag(long style)
{
    //Necessary for backwards compatibility with old wxAuiNotebook control - if we don't strip these flags then we end up with an ugly border around notebooks (where there was none before) in old code.
    style &= ~wxBORDER_MASK;
    style |= wxBORDER_NONE;
    style |= wxWANTS_CHARS;
    style |= wxAUI_MGR_NB_ALLOW_NOTEBOOKS;

    wxControl::SetWindowStyleFlag(style);

    // copy notebook dedicated style flags to the onboard manager flags
    m_mgr.SetFlags( style & 0xFFFF );

    // split is done by allowing the pane normal docking
    // if the flag changes, we have to redo the layout
    bool allow_docking = m_mgr.HasFlag(wxAUI_MGR_NB_TAB_SPLIT);
    for(size_t i=0; i<m_mgr.GetAllPanes().size() ;i++)
        m_mgr.GetAllPanes()[i].Dockable(allow_docking);

    m_mgr.Update();
}


bool wxAuiNotebook::AddPage(wxWindow* page,
                            const wxString& caption,
                            bool select,
                            const wxBitmap& bitmap)
{
    wxASSERT_MSG(page, wxT("page pointer must be non-NULL"));
    if (!page)
        return false;

    return InsertPage(m_mgr.GetAllPanes().size(),page,caption,select,bitmap);
}

bool wxAuiNotebook::InsertPage(size_t page_idx,
                               wxWindow* page,
                               const wxString& caption,
                               bool select,
                               const wxBitmap& bitmap)
{
    wxASSERT_MSG(page, wxT("page pointer must be non-NULL"));
    if (!page)
        return false;

    // Shift other panes so that this one can go in between them if necessary
    DoInsertPage(m_mgr.GetAllPanes(),1,0,1,0,page_idx);

    m_mgr.AddPane(page, wxAuiPaneInfo().Centre().Layer(0).Position(0).Caption(caption).Floatable(false).Movable().Page(page_idx).Icon(bitmap).Dockable(m_mgr.HasFlag(wxAUI_NB_TAB_SPLIT)).CloseButton(false).AlwaysDockInNotebook());

    // Change the selection if explicitly requested, or if the page is the first one; for the later case, it ensures that there is a current page in the notebook
    if (select || GetPageCount() == 1)
        SetSelection(page_idx);

    m_mgr.Update();

    return true;
}


// DeletePage() removes a tab from the multi-notebook,
// and destroys the window as well
bool wxAuiNotebook::DeletePage(size_t page_idx)
{
    if(page_idx>=m_mgr.GetPaneCount())
    {
        wxASSERT_MSG(0, wxT("Invalid page index passed to wxAuiNotebook::DeletePage"));
        return false;
    }

    wxWindow* wnd = m_mgr.GetPane(page_idx).GetWindow();
    m_mgr.DetachPane(wnd);
#if wxUSE_MDI
    // actually destroy the window now
    if (wxDynamicCast(wnd,wxAuiMDIChildFrame))
    {
        // delete the child frame with pending delete, as is
        // customary with frame windows
        if (!wxPendingDelete.Member(wnd))
            wxPendingDelete.Append(wnd);
    }
    else
#endif
    {
        wnd->Destroy();
    }
    m_mgr.Update();
    return true;
}



// RemovePage() removes a tab from the multi-notebook,
// but does not destroy the window
bool wxAuiNotebook::RemovePage(size_t page_idx)
{
    if(page_idx>=m_mgr.GetPaneCount())
    {
        // set new active pane unless we're being destroyed anyhow
        wxASSERT_MSG(0, wxT("Invalid page index passed to wxAuiNotebook::RemovePage"));
        return false;
    }

    m_mgr.DetachPane(m_mgr.GetPane(page_idx).GetWindow());
    m_mgr.Update();
    return true;
}

// GetPageIndex() returns the index of the page, or wxNOT_FOUND if the
// page could not be located in the notebook
int wxAuiNotebook::GetPageIndex(wxWindow* page_wnd)
{
    return m_mgr.GetAllPanes().Index(m_mgr.GetPane(page_wnd));
}



// SetPageText() changes the tab caption of the specified page
bool wxAuiNotebook::SetPageText(size_t page_idx, const wxString& text)
{
    if(page_idx>=m_mgr.GetPaneCount())
    {
        wxASSERT_MSG(0, wxT("Invalid page index passed to wxAuiNotebook::SetPageText"));
        return false;
    }

    m_mgr.GetPane(page_idx).Caption(text);
    m_mgr.Update();
    return true;
}

// returns the page caption
wxString wxAuiNotebook::GetPageText(size_t page_idx) const
{
    if(page_idx>=m_mgr.GetPaneCount())
    {
        wxASSERT_MSG(0, wxT("Invalid page index passed to wxAuiNotebook::GetPageText"));
        return wxEmptyString;
    }

    return m_mgr.GetPane(page_idx).GetCaption();
}

bool wxAuiNotebook::SetPageToolTip(size_t page_idx, const wxString& text)
{
    if (page_idx >= m_mgr.GetPaneCount())
    {
        wxASSERT_MSG(0, wxT("Invalid page index passed to wxAuiNotebook::SetPageToolTip"));
        return false;
    }

    // update tooltip info for pane.
    m_mgr.GetPane(page_idx).ToolTip(text);
    m_mgr.Update();

    // NB: we don't update the tooltip if it is already being displayed, it
    //     typically never happens, no need to code that
    return true;
}

wxString wxAuiNotebook::GetPageToolTip(size_t page_idx) const
{
    if (page_idx >= m_mgr.GetPaneCount())
    {
        wxASSERT_MSG(0, wxT("Invalid page index passed to wxAuiNotebook::GetPageToolTip"));
        return wxT("");
    }

    return m_mgr.GetPane(page_idx).GetToolTip();;
}


bool wxAuiNotebook::SetPageBitmap(size_t page_idx, const wxBitmap& bitmap)
{
    if(page_idx>=m_mgr.GetPaneCount())
    {
        wxASSERT_MSG(0, wxT("Invalid page index passed to wxAuiNotebook::SetPageBitmap"));
        return false;
    }

    m_mgr.GetPane(page_idx).Icon(bitmap);
    m_mgr.Update();
    return true;
}

// returns the page bitmap
wxBitmap wxAuiNotebook::GetPageBitmap(size_t page_idx) const
{
    if(page_idx>=m_mgr.GetPaneCount())
    {
        wxASSERT_MSG(0, wxT("Invalid page index passed to wxAuiNotebook::GetPageBitmap"));
        return wxNullBitmap;
    }

    return m_mgr.GetPane(page_idx).GetIcon();
}

// GetSelection() returns the index of the currently active page
int wxAuiNotebook::GetSelection() const
{
    return m_mgr.GetActivePane(FindFocus());
}

// SetSelection() sets the currently active page and emit changed event
int wxAuiNotebook::SetSelection(size_t new_page)
{
    return DoModifySelection(new_page, true);
}


// GetPageCount() returns the total number of
// pages managed by the multi-notebook
size_t wxAuiNotebook::GetPageCount() const
{
    return m_mgr.GetPaneCount();
}

// GetPage() returns the wxWindow pointer of the
// specified page
wxWindow* wxAuiNotebook::GetPage(size_t page_idx) const
{
    return m_mgr.GetPane(page_idx).GetWindow();
}

// GetActiveTabCtrl() returns the active tab control.  It is
// called to determine which control gets new windows being added
wxAuiTabContainer* wxAuiNotebook::GetActiveTabCtrl()
{
    unsigned int sel_pane_index = GetSelection();
    if (sel_pane_index >= 0 && sel_pane_index < m_mgr.GetPaneCount())
    {
        wxAuiTabContainer* ctrl;
        int idx;

        // find the tab ctrl with the current page
        if (m_mgr.FindTab(m_mgr.GetPane(sel_pane_index).GetWindow(), &ctrl, &idx))
        {
            return ctrl;
        }
    }

    // no current page, just find the first tab ctrl
    wxAuiPaneInfoArray& all_panes = m_mgr.GetAllPanes();
    size_t i, pane_count = all_panes.GetCount();
    for (i = 0; i < pane_count; ++i)
    {
        wxAuiPaneInfo& pane = all_panes.Item(i);
        if (pane.GetName() == wxT("dummy"))
            continue;

        wxAuiTabContainer* ctrl;
        int idx;

        // Return the tab ctrl for this pane, if one exists.
        if (m_mgr.FindTab(pane.GetWindow(), &ctrl, &idx))
        {
            return ctrl;
        }
    }

    return NULL;
}

void wxAuiNotebook::OnTabCancelDrag(wxAuiNotebookEvent& /* evt */)
{
    //fixme: (MJM) merge - this has been broken in the merge and needs to be re-implemented
}

void wxAuiNotebook::OnPaneDrop(wxAuiManagerEvent& evt)
{
    // Fire our own notebook specific event in place of normal manager one.
    // This is for backwards compatibility with older notebook code, before the aui manager handled this.
    wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_ALLOW_DND, m_mgr.GetManagedWindow()->GetId());
    int selIndex=m_mgr.GetAllPanes().Index(*evt.GetPane());
    e.SetSelection(selIndex);
    e.SetOldSelection(selIndex);
    e.SetEventObject(this);
    e.SetDragSource(this);
    e.Veto(); // dropping must be explicitly approved by control owner

    ProcessEvent(e);

    evt.Veto(!e.IsAllowed());
}

void wxAuiNotebook::Split(size_t page, int direction)
{
    // If splitting is not allowed then simply return
    if(!m_mgr.HasFlag(wxAUI_NB_TAB_SPLIT))
    {
        return;
    }

    wxAuiPaneInfoArray& panes = m_mgr.GetAllPanes();
    unsigned int pageCount = panes.size();
    if(pageCount>page)
    {
        switch(direction)
        {
            case wxTOP: direction = wxAUI_DOCK_TOP; break;
            case wxBOTTOM: direction = wxAUI_DOCK_BOTTOM; break;
            case wxLEFT: direction = wxAUI_DOCK_LEFT; break;
            case wxRIGHT: direction = wxAUI_DOCK_RIGHT; break;
            default: wxASSERT_MSG(0, wxT("Invalid direction passed to wxAuiNotebook::Split")); return;
        }

        panes[page].Direction(direction);

        DoInsertDockLayer(panes,direction,panes[page].GetLayer());

        m_mgr.SetActivePane(panes[page].GetWindow());

        m_mgr.Update();
        return;
    }
    wxASSERT_MSG(0, wxT("Invalid page index passed to wxAuiNotebook::Split"));
}

// Sets the normal font
void wxAuiNotebook::SetNormalFont(const wxFont& font)
{
    GetArtProvider()->SetNormalFont(font);
}

// Sets the selected tab font
void wxAuiNotebook::SetSelectedFont(const wxFont& font)
{
    GetArtProvider()->SetSelectedFont(font);
}

// Sets the measuring font
void wxAuiNotebook::SetMeasuringFont(const wxFont& font)
{
    GetArtProvider()->SetMeasuringFont(font);
}

// Sets the tab font
bool wxAuiNotebook::SetFont(const wxFont& font)
{
    wxControl::SetFont(font);

    wxFont normalFont(font);
    wxFont selectedFont(normalFont);
    selectedFont.SetWeight(wxFONTWEIGHT_BOLD);

    SetNormalFont(normalFont);
    SetSelectedFont(selectedFont);
    SetMeasuringFont(selectedFont);

    return true;
}

// Gets the tab control height
int wxAuiNotebook::GetTabCtrlHeight() const
{
    return GetArtProvider()->m_tabCtrlHeight;
}

// Gets the tab control width
int wxAuiNotebook::GetTabCtrlWidth() const
{
    return GetArtProvider()->m_tabCtrlWidth;
}

// Gets the height of the notebook for a given page height
int wxAuiNotebook::GetHeightForPageHeight(int pageHeight)
{
    UpdateTabCtrlSize();

    int tabCtrlHeight = GetTabCtrlHeight();
    int decorHeight = 2;
    return tabCtrlHeight + pageHeight + decorHeight;
}

// Gets the width of the notebook for a given page width
int wxAuiNotebook::GetWidthForPageWidth(int pageWidth)
{
    UpdateTabCtrlSize();

    int tabCtrlWidth = GetTabCtrlWidth();
    int decorWidth = 2;
    return tabCtrlWidth + pageWidth + decorWidth;
}

// Advances the selection, generation page selection events
void wxAuiNotebook::AdvanceSelection(bool forward)
{
    if (GetPageCount() <= 1)
        return;

    int currentSelection = GetSelection();

    if (forward)
    {
        if (currentSelection == (int) (GetPageCount() - 1))
            return;
        else if (currentSelection == -1)
            currentSelection = 0;
        else
            currentSelection ++;
    }
    else
    {
        if (currentSelection <= 0)
            return;
        else
            currentSelection --;
    }

    SetSelection(currentSelection);
}

// Shows the window menu
bool wxAuiNotebook::ShowWindowMenu()
{
    wxAuiTabContainer* tabCtrl = GetActiveTabCtrl();

    int idx = tabCtrl->GetArtProvider()->ShowDropDown(this, tabCtrl->GetPages(), tabCtrl->GetActivePage());

    if (idx != -1)
    {
        wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_PAGE_CHANGING, GetId());
        e.SetSelection(idx);
        e.SetOldSelection(tabCtrl->GetActivePage());
        e.SetEventObject(this);
        GetEventHandler()->ProcessEvent(e);

        return true;
    }
    else
        return false;
}

void wxAuiNotebook::DoThaw()
{
    //fixme: (MJM) This has been broken in the merge and needs to be re-implemented.
    //DoSizing();

    wxBookCtrlBase::DoThaw();
}

void wxAuiNotebook::SetPageSize (const wxSize& WXUNUSED(size))
{
    wxFAIL_MSG("Not implemented for wxAuiNotebook");
}

int wxAuiNotebook::HitTest (const wxPoint& WXUNUSED(pt), long* WXUNUSED(flags)) const
{
    wxFAIL_MSG("Not implemented for wxAuiNotebook");
    return wxNOT_FOUND;
}

int wxAuiNotebook::GetPageImage(size_t WXUNUSED(n)) const
{
    wxFAIL_MSG("Not implemented for wxAuiNotebook");
    return -1;
}

bool wxAuiNotebook::SetPageImage(size_t n, int imageId)
{
    return SetPageBitmap(n, GetImageList()->GetBitmap(imageId));
}

wxWindow* wxAuiNotebook::GetCurrentPage () const
{
    const int sel = GetSelection();

    return sel == wxNOT_FOUND ? NULL : GetPage(sel);
}

int wxAuiNotebook::ChangeSelection(size_t n)
{
    return DoModifySelection(n, false);
}

bool wxAuiNotebook::AddPage(wxWindow *page, const wxString &text, bool select, 
                            int imageId)
{
    if(HasImageList()) 
    {
        return AddPage(page, text, select, GetImageList()->GetBitmap(imageId));
    }
    else
    {
        return AddPage(page, text, select, wxNullBitmap);
    }
}

bool wxAuiNotebook::DeleteAllPages()
{
    size_t count = GetPageCount();
    for(size_t i = 0; i < count; i++)
    {
        DeletePage(0);
    }
    return true;
}

bool wxAuiNotebook::InsertPage(size_t index, wxWindow *page, 
                               const wxString &text, bool select, 
                               int imageId)
{
    if(HasImageList())
    {
        return InsertPage(index, page, text, select, 
                          GetImageList()->GetBitmap(imageId));
    }
    else
    {
        return InsertPage(index, page, text, select, wxNullBitmap);
    }
}

int wxAuiNotebook::DoModifySelection(size_t n, bool events)
{
    // don't change the page unless necessary;
    // however, clicking again on a tab should give it the focus.
    if ((int)n == GetSelection())
    {
        if(!GetCurrentPage()->HasFocus())
            GetCurrentPage()->SetFocus();

        return n;
    }

    bool vetoed = false;

    wxAuiNotebookEvent evt(wxEVT_AUINOTEBOOK_PAGE_CHANGING, m_windowId);

    if(events)
    {
        evt.SetSelection(n);
        evt.SetOldSelection(GetSelection());
        evt.SetEventObject(this);
        GetEventHandler()->ProcessEvent(evt);
        vetoed = !evt.IsAllowed();
    }

    if (!vetoed)
    {
        wxWindow* wnd = m_mgr.GetPane(n).GetWindow();
        m_mgr.SetActivePane(wnd);
        m_mgr.Update();

        // program allows the page change
        if(events)
        {
            evt.SetEventType(wxEVT_AUINOTEBOOK_PAGE_CHANGED);
            (void)GetEventHandler()->ProcessEvent(evt);
        }
    }

    return GetSelection();
}


#endif // wxUSE_AUI
