///////////////////////////////////////////////////////////////////////////////
// Name:        src/aui/framemanager.cpp
// Purpose:     wxaui: wx advanced user interface - docking window manager
// Author:      Benjamin I. Williams
// Modified by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Created:     2005-05-17
// Copyright:   (C) Copyright 2005-2006, Kirix Corporation, All Rights Reserved
// Licence:     wxWindows Library Licence, Version 3.1
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_AUI

#include "wx/aui/framemanager.h"
#include "wx/aui/dockart.h"
#include "wx/aui/floatpane.h"
#include "wx/aui/tabmdi.h"
#include "wx/aui/tabart.h"
#include "wx/aui/auibar.h"
#include "wx/aui/auibook.h"
#include "wx/mdi.h"

#ifndef WX_PRECOMP
    #include "wx/panel.h"
    #include "wx/settings.h"
    #include "wx/app.h"
    #include "wx/dcclient.h"
    #include "wx/dcscreen.h"
    #include "wx/dcmemory.h"
    #include "wx/toolbar.h"
    #include "wx/image.h"
    #include "wx/statusbr.h"
#endif

WX_CHECK_BUILD_OPTIONS("wxAUI")

#include "wx/arrimpl.cpp"
WX_DECLARE_OBJARRAY(wxRect, wxAuiRectArray);
WX_DEFINE_OBJARRAY(wxAuiRectArray)
WX_DEFINE_OBJARRAY(wxAuiDockUIPartArray)
WX_DEFINE_OBJARRAY(wxAuiDockInfoArray)
WX_DEFINE_OBJARRAY(wxAuiPaneButtonArray)
WX_DEFINE_OBJARRAY(wxAuiPaneInfoArray)

WX_DEFINE_OBJARRAY(wxAuiTabContainerPointerArray)
WX_DEFINE_OBJARRAY(wxAuiSizerItemPointerArray)

wxAuiPaneInfo wxAuiNullPaneInfo;
wxAuiDockInfo wxAuiNullDockInfo;
wxDEFINE_EVENT( wxEVT_AUI_PANE_BUTTON, wxAuiManagerEvent );
wxDEFINE_EVENT( wxEVT_AUI_PANE_CLOSE, wxAuiManagerEvent );
wxDEFINE_EVENT( wxEVT_AUI_PANE_MAXIMIZE, wxAuiManagerEvent );
wxDEFINE_EVENT( wxEVT_AUI_PANE_RESTORE, wxAuiManagerEvent );
wxDEFINE_EVENT( wxEVT_AUI_PANE_ACTIVATED, wxAuiManagerEvent );
wxDEFINE_EVENT( wxEVT_AUI_PANE_DOCK_OVER, wxAuiManagerEvent );
wxDEFINE_EVENT( wxEVT_AUI_RENDER, wxAuiManagerEvent );
wxDEFINE_EVENT( wxEVT_AUI_FIND_MANAGER, wxAuiManagerEvent );
wxDEFINE_EVENT( wxEVT_AUI_ALLOW_DND, wxAuiManagerEvent );

#ifdef __WXMAC__
    // a few defines to avoid nameclashes
    #define __MAC_OS_X_MEMORY_MANAGER_CLEAN__ 1
    #define __AIFF__
    #include "wx/osx/private.h"
#endif

#ifdef __WXMSW__
    #include "wx/msw/wrapwin.h"
    #include "wx/msw/private.h"
    #include "wx/msw/dc.h"
#endif

wxIMPLEMENT_DYNAMIC_CLASS(wxAuiManagerEvent, wxEvent);
wxIMPLEMENT_CLASS(wxAuiManager, wxEvtHandler);

// the utility function ShowWnd() is the same as show,
// except it handles wxAuiMDIChildFrame windows as well,
// as the Show() method on this class is "unplugged"
static void ShowWnd(wxWindow* wnd, bool show)
{
#if wxUSE_MDI
    if (wxDynamicCast(wnd,wxAuiMDIChildFrame))
    {
        wxAuiMDIChildFrame* cf = (wxAuiMDIChildFrame*)wnd;
        cf->DoShow(show);
        cf->ApplyMDIChildFrameRect();
    }
    else
#endif
    {
        wnd->Show(show);
    }
}

// Determine if a close button should appear on the tab for the passed pane
bool TabHasCloseButton(unsigned int flags,wxAuiPaneInfo& page)
{
    if ((flags & wxAUI_MGR_NB_CLOSE_ON_ALL_TABS) != 0 || ((flags & wxAUI_MGR_NB_CLOSE_ON_ACTIVE_TAB) != 0 && page.HasFlag(wxAuiPaneInfo::optionActiveNotebook)))
        return true;
    return false;
}

const int auiToolBarLayer = 10;
//fixme: (MJM) This should be set by the art somewhere not hardcoded, temporary hardcoding while we iron out some issues with the tab art providers.
const int notebookTabHeight = 42;


#ifndef __WXGTK20__


class wxPseudoTransparentFrame : public wxFrame
{
public:
    wxPseudoTransparentFrame(wxWindow* parent = NULL,
                wxWindowID id = wxID_ANY,
                const wxString& title = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString &name = wxT("frame"))
                    : wxFrame(parent, id, title, pos, size, style | wxFRAME_SHAPED, name)
    {
        SetBackgroundStyle(wxBG_STYLE_CUSTOM);
        m_amount=0;
        m_maxWidth=0;
        m_maxHeight=0;
        m_lastWidth=0;
        m_lastHeight=0;
#ifdef __WXGTK__
        m_canSetShape = false; // have to wait for window create event on GTK
#else
        m_canSetShape = true;
#endif
        m_region = wxRegion(0, 0, 0, 0);
        SetTransparent(0);
    }

    virtual bool SetTransparent(wxByte alpha) wxOVERRIDE
    {
        if (m_canSetShape)
        {
            int w=100; // some defaults
            int h=100;
            GetClientSize(&w, &h);

            m_maxWidth = w;
            m_maxHeight = h;
            m_amount = alpha;
            m_region.Clear();
//            m_region.Union(0, 0, 1, m_maxWidth);
            if (m_amount)
            {
                for (int y=0; y<m_maxHeight; y++)
                {
                    // Reverse the order of the bottom 4 bits
                    int j=((y&8)?1:0)|((y&4)?2:0)|((y&2)?4:0)|((y&1)?8:0);
                    if ((j*16+8)<m_amount)
                        m_region.Union(0, y, m_maxWidth, 1);
                }
            }
            SetShape(m_region);
            Refresh();
        }
        return true;
    }

    void OnPaint(wxPaintEvent& WXUNUSED(event))
    {
        wxPaintDC dc(this);

        if (m_region.IsEmpty())
            return;

#ifdef __WXMAC__
        dc.SetBrush(wxColour(128, 192, 255));
#else
        dc.SetBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_ACTIVECAPTION));
#endif
        dc.SetPen(*wxTRANSPARENT_PEN);

        wxRegionIterator upd(GetUpdateRegion()); // get the update rect list

        while (upd)
        {
            wxRect rect(upd.GetRect());
            dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);

            ++upd;
        }
    }

#ifdef __WXGTK__
    void OnWindowCreate(wxWindowCreateEvent& WXUNUSED(event))
    {
        m_canSetShape=true;
        SetTransparent(0);
    }
#endif

    void OnSize(wxSizeEvent& event)
    {
        // We sometimes get surplus size events
        if ((event.GetSize().GetWidth() == m_lastWidth) &&
            (event.GetSize().GetHeight() == m_lastHeight))
        {
            event.Skip();
            return;
        }
        m_lastWidth = event.GetSize().GetWidth();
        m_lastHeight = event.GetSize().GetHeight();

        SetTransparent(m_amount);
        m_region.Intersect(0, 0, event.GetSize().GetWidth(),
                           event.GetSize().GetHeight());
        SetShape(m_region);
        Refresh();
        event.Skip();
    }

private:
    wxByte m_amount;
    int m_maxWidth;
    int m_maxHeight;
    bool m_canSetShape;
    int m_lastWidth,m_lastHeight;

    wxRegion m_region;

    wxDECLARE_DYNAMIC_CLASS(wxPseudoTransparentFrame);
    wxDECLARE_EVENT_TABLE();
};


wxIMPLEMENT_DYNAMIC_CLASS(wxPseudoTransparentFrame, wxFrame);

wxBEGIN_EVENT_TABLE(wxPseudoTransparentFrame, wxFrame)
    EVT_PAINT(wxPseudoTransparentFrame::OnPaint)
    EVT_SIZE(wxPseudoTransparentFrame::OnSize)
#ifdef __WXGTK__
    EVT_WINDOW_CREATE(wxPseudoTransparentFrame::OnWindowCreate)
#endif
wxEND_EVENT_TABLE()


#else
  // __WXGTK20__

#include <gtk/gtk.h>
#include "wx/gtk/private/gtk2-compat.h"

static void
gtk_pseudo_window_realized_callback( GtkWidget *m_widget, void *WXUNUSED(win) )
{
        wxSize disp = wxGetDisplaySize();
        int amount = 128;
        wxRegion region;
        for (int y=0; y<disp.y; y++)
                {
                    // Reverse the order of the bottom 4 bits
                    int j=((y&8)?1:0)|((y&4)?2:0)|((y&2)?4:0)|((y&1)?8:0);
                    if ((j*16+8)<amount)
                        region.Union(0, y, disp.x, 1);
                }
        gdk_window_shape_combine_region(gtk_widget_get_window(m_widget), region.GetRegion(), 0, 0);
}


class wxPseudoTransparentFrame: public wxFrame
{
public:
    wxPseudoTransparentFrame(wxWindow* parent = NULL,
                wxWindowID id = wxID_ANY,
                const wxString& title = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString &name = wxT("frame"))
    {
         if (!CreateBase( parent, id, pos, size, style, wxDefaultValidator, name ))
            return;

        m_title = title;

        m_widget = gtk_window_new( GTK_WINDOW_POPUP );
        g_object_ref(m_widget);

        if (parent) parent->AddChild(this);

        g_signal_connect( m_widget, "realize",
                      G_CALLBACK (gtk_pseudo_window_realized_callback), this );

        GdkColor col;
        col.red = 128 * 256;
        col.green = 192 * 256;
        col.blue = 255 * 256;
        gtk_widget_modify_bg( m_widget, GTK_STATE_NORMAL, &col );
    }

    bool SetTransparent(wxByte WXUNUSED(alpha)) wxOVERRIDE
    {
        return true;
    }

protected:
    virtual void DoSetSizeHints( int minW, int minH,
                                 int maxW, int maxH,
                                 int incW, int incH) wxOVERRIDE
    {
        // the real wxFrame method doesn't work for us because we're not really
        // a top level window so skip it
        wxWindow::DoSetSizeHints(minW, minH, maxW, maxH, incW, incH);
    }

private:
    wxDECLARE_DYNAMIC_CLASS(wxPseudoTransparentFrame);
};

wxIMPLEMENT_DYNAMIC_CLASS(wxPseudoTransparentFrame, wxFrame);

#endif
 // __WXGTK20__



// -- static utility functions --

static wxBitmap wxPaneCreateStippleBitmap()
{
    unsigned char data[] = { 0,0,0,192,192,192, 192,192,192,0,0,0 };
    wxImage img(2,2,data,true);
    return wxBitmap(img);
}

static void DrawResizeHint(wxDC& dc, const wxRect& rect)
{
    wxBitmap stipple = wxPaneCreateStippleBitmap();
    wxBrush brush(stipple);
    dc.SetBrush(brush);
#ifdef __WXMSW__
    wxMSWDCImpl *impl = (wxMSWDCImpl*) dc.GetImpl();
    PatBlt(GetHdcOf(*impl), rect.GetX(), rect.GetY(), rect.GetWidth(), rect.GetHeight(), PATINVERT);
#else
    dc.SetPen(*wxTRANSPARENT_PEN);

    dc.SetLogicalFunction(wxXOR);
    dc.DrawRectangle(rect);
#endif
}



// CopyDocksAndPanes() - this utility function creates copies of
// the dock and pane info.  wxAuiDockInfo's usually contain pointers
// to wxAuiPaneInfo classes, thus this function is necessary to reliably
// reconstruct that relationship in the new dock info and pane info arrays

static void CopyDocksAndPanes(wxAuiDockInfoArray& dest_docks,
                              wxAuiPaneInfoArray& dest_panes,
                              const wxAuiDockInfoArray& src_docks,
                              const wxAuiPaneInfoArray& src_panes)
{
    dest_docks = src_docks;
    dest_panes = src_panes;
    int i, j, k, dock_count, pc1, pc2;
    for (i = 0, dock_count = dest_docks.GetCount(); i < dock_count; ++i)
    {
        wxAuiDockInfo& dock = dest_docks.Item(i);
        for (j = 0, pc1 = dock.panes.GetCount(); j < pc1; ++j)
            for (k = 0, pc2 = src_panes.GetCount(); k < pc2; ++k)
                if (dock.panes.Item(j) == &src_panes.Item(k))
                    dock.panes.Item(j) = &dest_panes.Item(k);
    }
}

// GetMaxLayer() is an internal function which returns
// the highest layer inside the specified dock
static int GetMaxLayer(const wxAuiDockInfoArray& docks,
                       int dock_direction)
{
    int i, dock_count, max_layer = 0;
    for (i = 0, dock_count = docks.GetCount(); i < dock_count; ++i)
    {
        wxAuiDockInfo& dock = docks.Item(i);
        if (dock.dock_direction == dock_direction &&
            dock.dock_layer > max_layer && !dock.fixed)
                max_layer = dock.dock_layer;
    }
    return max_layer;
}


// GetMaxRow() is an internal function which returns
// the highest layer inside the specified dock
static int GetMaxRow(const wxAuiPaneInfoArray& panes, int direction, int layer)
{
    int i, pane_count, max_row = 0;
    for (i = 0, pane_count = panes.GetCount(); i < pane_count; ++i)
    {
        wxAuiPaneInfo& pane = panes.Item(i);
        if (pane.dock_direction == direction &&
            pane.dock_layer == layer &&
            pane.dock_row > max_row)
                max_row = pane.dock_row;
    }
    return max_row;
}



// DoInsertDockLayer() is an internal function that inserts a new dock
// layer by incrementing all existing dock layer values by one
void DoInsertDockLayer(wxAuiPaneInfoArray& panes,
                              int dock_direction,
                              int dock_layer)
{
    int i, pane_count;
    for (i = 0, pane_count = panes.GetCount(); i < pane_count; ++i)
    {
        wxAuiPaneInfo& pane = panes.Item(i);
        if (!pane.IsFloating() &&
            pane.dock_direction == dock_direction &&
            pane.dock_layer >= dock_layer)
                pane.dock_layer++;
    }
}

// DoInsertDockLayer() is an internal function that inserts a new dock
// row by incrementing all existing dock row values by one
void DoInsertDockRow(wxAuiPaneInfoArray& panes,
                            int dock_direction,
                            int dock_layer,
                            int dock_row)
{
    int i, pane_count;
    for (i = 0, pane_count = panes.GetCount(); i < pane_count; ++i)
    {
        wxAuiPaneInfo& pane = panes.Item(i);
        if (!pane.IsFloating() &&
            pane.dock_direction == dock_direction &&
            pane.dock_layer == dock_layer &&
            pane.dock_row >= dock_row)
                pane.dock_row++;
    }
}

// DoInsertDockLayer() is an internal function that inserts a space for
// another dock pane by incrementing all existing dock row values by one
void DoInsertPane(wxAuiPaneInfoArray& panes,
                         int dock_direction,
                         int dock_layer,
                         int dock_row,
                         int dock_pos)
{
    int i, pane_count;
    for (i = 0, pane_count = panes.GetCount(); i < pane_count; ++i)
    {
        wxAuiPaneInfo& pane = panes.Item(i);
        if (!pane.IsFloating() &&
            pane.dock_direction == dock_direction &&
            pane.dock_layer == dock_layer &&
            pane.dock_row == dock_row &&
            pane.dock_pos >= dock_pos)
                pane.dock_pos++;
    }
}

// DoInsertPage() is an internal function that inserts a space for
// another notebook page by incrementing all existing page values by one
void DoInsertPage(wxAuiPaneInfoArray& panes, int dockDirection, int dockLayer, int dockRow, int dockPos, int dockPage)
{
    int i, paneCount;
    for (i = 0, paneCount = panes.GetCount(); i < paneCount; ++i)
    {
        wxAuiPaneInfo& pane = panes.Item(i);
        if (!pane.IsFloating() &&
            pane.GetDirection() == dockDirection &&
            pane.GetLayer() == dockLayer &&
            pane.GetRow() == dockRow &&
            pane.GetPosition() == dockPos &&
            pane.GetPage() >= dockPage)
            {
                pane.Page(pane.GetPage()+1);
            }
    }
}

// FindDocks() is an internal function that returns a list of docks which meet
// the specified conditions in the parameters and returns a sorted array
// (sorted by layer and then row)
static void FindDocks(wxAuiDockInfoArray& docks,
                      int dock_direction,
                      int dock_layer,
                      int dock_row,
                      wxAuiDockInfoPtrArray& arr)
{
    int begin_layer = dock_layer;
    int end_layer = dock_layer;
    int begin_row = dock_row;
    int end_row = dock_row;
    int dock_count = docks.GetCount();
    int layer, row, i, max_row = 0, max_layer = 0;

    // discover the maximum dock layer and the max row
    for (i = 0; i < dock_count; ++i)
    {
        max_row = wxMax(max_row, docks.Item(i).dock_row);
        max_layer = wxMax(max_layer, docks.Item(i).dock_layer);
    }

    // if no dock layer was specified, search all dock layers
    if (dock_layer == -1)
    {
        begin_layer = 0;
        end_layer = max_layer;
    }

    // if no dock row was specified, search all dock row
    if (dock_row == -1)
    {
        begin_row = 0;
        end_row = max_row;
    }

    arr.Clear();

    for (layer = begin_layer; layer <= end_layer; ++layer)
        for (row = begin_row; row <= end_row; ++row)
            for (i = 0; i < dock_count; ++i)
            {
                wxAuiDockInfo& d = docks.Item(i);
                if (dock_direction == -1 || dock_direction == d.dock_direction)
                {
                    if (d.dock_layer == layer && d.dock_row == row)
                        arr.Add(&d);
                }
            }
}

// FindPaneInDock() looks up a specified window pointer inside a dock.
// If found, the corresponding wxAuiPaneInfo pointer is returned, otherwise NULL.
static wxAuiPaneInfo* FindPaneInDock(const wxAuiDockInfo& dock, wxWindow* window)
{
    int i, count = dock.panes.GetCount();
    for (i = 0; i < count; ++i)
    {
        wxAuiPaneInfo* p = dock.panes.Item(i);
        if (p->window == window)
            return p;
    }
    return NULL;
}

// RemovePaneFromDocks() removes a pane window from all docks
// with a possible exception specified by parameter "ex_cept"
static void RemovePaneFromDocks(wxAuiDockInfoArray& docks,
                                wxAuiPaneInfo& pane,
                                wxAuiDockInfo* ex_cept  = NULL  )
{
    int i, dock_count;
    for (i = 0, dock_count = docks.GetCount(); i < dock_count; ++i)
    {
        wxAuiDockInfo& d = docks.Item(i);
        if (&d == ex_cept)
            continue;
        wxAuiPaneInfo* pi = FindPaneInDock(d, pane.window);
        if (pi)
            d.panes.Remove(pi);
    }
}

/*
// This function works fine, and may be used in the future

// RenumberDockRows() takes a dock and assigns sequential numbers
// to existing rows.  Basically it takes out the gaps; so if a
// dock has rows with numbers 0,2,5, they will become 0,1,2
static void RenumberDockRows(wxAuiDockInfoPtrArray& docks)
{
    int i, dock_count;
    for (i = 0, dock_count = docks.GetCount(); i < dock_count; ++i)
    {
        wxAuiDockInfo& dock = *docks.Item(i);
        dock.dock_row = i;

        int j, pane_count;
        for (j = 0, pane_count = dock.panes.GetCount(); j < pane_count; ++j)
            dock.panes.Item(j)->dock_row = i;
    }
}
*/


// SetActivePane() sets the active pane, as well as cycles through
// every other pane and makes sure that all others' active flags
// are turned off
int wxAuiManager::SetActivePane(wxWindow* active_pane)
{
    int i, pane_count;
    wxAuiPaneInfo* active_paneinfo = NULL;
    for (i = 0, pane_count = m_panes.GetCount(); i < pane_count; ++i)
    {
        wxAuiPaneInfo& pane = m_panes.Item(i);
        pane.state &= ~wxAuiPaneInfo::optionActive;
        if (pane.window == active_pane)
        {
            active_paneinfo = &pane;
            wxAuiTabContainer* ctrl;
            int ctrlIndex;
            if(FindTab(pane.GetWindow(), &ctrl, &ctrlIndex))
            {
                ctrl->SetActivePage(pane.GetWindow());
            }

            pane.state |= wxAuiPaneInfo::optionActive;
        }
    }

    // send the 'activated' event after all panes have been updated
    if ( active_paneinfo )
    {
        wxAuiManagerEvent evt(wxEVT_AUI_PANE_ACTIVATED);
        evt.SetManager(this);
        evt.SetPane(active_paneinfo);
        ProcessMgrEvent(evt);
    }

    return 0;
}


// this function is used to sort panes by Direction, Layer, Row, Position and then finally by Page
static int PaneSortFunc(wxAuiPaneInfo** p1, wxAuiPaneInfo** p2)
{
    if((*p1)->GetDirection() < (*p2)->GetDirection())
    {
        return -1;
    }
    else if((*p1)->GetDirection() == (*p2)->GetDirection())
    {
        if((*p1)->GetLayer() < (*p2)->GetLayer())
        {
            return -1;
        }
        else if((*p1)->GetLayer() == (*p2)->GetLayer())
        {
            if((*p1)->GetRow() < (*p2)->GetRow())
            {
                return -1;
            }
            else if((*p1)->GetRow() == (*p2)->GetRow())
            {
                if((*p1)->GetPosition() < (*p2)->GetPosition())
                {
                    return -1;
                }
                else if((*p1)->GetPosition() == (*p2)->GetPosition())
                {
                    if((*p1)->GetPage() < (*p2)->GetPage())
                    {
                        return -1;
                    }
                }
            }
        }
    }
    return 1;
}

// this function is used to sort panes primarily by dock position and secondly by page
static int DockPaneSortFunc(wxAuiPaneInfo** p1, wxAuiPaneInfo** p2)
{
    if((*p1)->GetPosition() < (*p2)->GetPosition())
    {
        return -1;
    }
    else if((*p1)->GetPosition() == (*p2)->GetPosition())
    {
        if((*p1)->GetPage() < (*p2)->GetPage())
        {
            return -1;
        }
    }
    return 1;
}


bool wxAuiPaneInfo::IsValid() const
{
    // Should this RTTI and function call be rewritten as
    // sending a new event type to allow other window types
    // to check the pane settings?
    wxAuiToolBar* toolbar = wxDynamicCast(window, wxAuiToolBar);
    return !toolbar || toolbar->IsPaneValid(*this);
}


wxAuiPaneInfo &wxAuiPaneInfo::MoveOver(const wxAuiPaneInfo &target)
{
if (target.IsValid() && IsValid() && !IsToolbar() && !target.IsToolbar() && IsDockable() && target.IsDocked()) {

    dock_direction = target.dock_direction;
    dock_layer     = target.dock_layer;
    dock_pos       = target.dock_pos;
    dock_row       = target.dock_row;
    m_dock_page    = target.m_dock_page+1;

    Dock();

}

return *this;
}



// -- wxAuiManager class implementation --


wxBEGIN_EVENT_TABLE(wxAuiManager, wxEvtHandler)
    EVT_AUI_PANE_BUTTON(wxAuiManager::OnPaneButton)
    EVT_AUI_RENDER(wxAuiManager::OnRender)
    EVT_PAINT(wxAuiManager::OnPaint)
    EVT_ERASE_BACKGROUND(wxAuiManager::OnEraseBackground)
    EVT_SIZE(wxAuiManager::OnSize)
    EVT_SET_CURSOR(wxAuiManager::OnSetCursor)
    EVT_LEFT_DOWN(wxAuiManager::OnLeftDown)
    EVT_LEFT_UP(wxAuiManager::OnLeftUp)
    EVT_LEFT_DCLICK(wxAuiManager::OnLeftDClick)
    EVT_RIGHT_DOWN(wxAuiManager::OnRightDown)
    EVT_RIGHT_UP(wxAuiManager::OnRightUp)
    EVT_MIDDLE_DOWN(wxAuiManager::OnMiddleDown)
    EVT_MIDDLE_UP(wxAuiManager::OnMiddleUp)
    EVT_MOTION(wxAuiManager::OnMotion)
    EVT_LEAVE_WINDOW(wxAuiManager::OnLeaveWindow)
    EVT_MOUSE_CAPTURE_LOST(wxAuiManager::OnCaptureLost)
    EVT_CHILD_FOCUS(wxAuiManager::OnChildFocus)
    EVT_AUI_FIND_MANAGER(wxAuiManager::OnFindManager)
wxEND_EVENT_TABLE()


wxAuiManager::wxAuiManager(wxWindow* managed_wnd, unsigned int flags)
{
    m_action = actionNone;
    m_lastMouseMove = wxPoint();
    m_hoverButton = NULL;
    m_art = new wxAuiDefaultDockArt;
    m_hintWnd = NULL;
    m_flags = flags;
    m_skipping = false;
    m_hasMaximized = false;
    m_frame = NULL;
    m_dockConstraintX = 0.3;
    m_dockConstraintY = 0.3;
    m_reserved = NULL;
    m_currentDragItem = -1;

    m_tab_art = new wxAuiDefaultTabArt;
    m_doingHintCalculation = false;
    m_actionWindow = NULL;
    m_actionDeadZone = NULL;
    m_actionPart = NULL;
    m_actionWindow = NULL;
    m_hoverButton2 = NULL;

    if (managed_wnd)
    {
        SetManagedWindow(managed_wnd);
    }
}

wxAuiManager::~wxAuiManager()
{
    // NOTE: It's possible that the windows have already been destroyed by the
    // time this dtor is called, so this loop can result in memory access via
    // invalid pointers, resulting in a crash.  So it will be disabled while
    // waiting for a better solution.
#if 0
    for ( size_t i = 0; i < m_panes.size(); i++ )
    {
        wxAuiPaneInfo& pinfo = m_panes[i];
        if (pinfo.window && !pinfo.window->GetParent())
            delete pinfo.window;
    }
#endif

    delete m_art;
    delete m_tab_art;
}

// creates a floating frame for the windows
wxAuiFloatingFrame* wxAuiManager::CreateFloatingFrame(wxWindow* parent,
                                                      const wxAuiPaneInfo& paneInfo)
{
    return new wxAuiFloatingFrame(parent, this, paneInfo);
}

bool wxAuiManager::CanDockPanel(const wxAuiPaneInfo & WXUNUSED(p))
{
    // if a key modifier is pressed while dragging the frame,
    // don't dock the window
    return !(wxGetKeyState(WXK_CONTROL) || wxGetKeyState(WXK_ALT));
}

// GetPane() looks up a wxAuiPaneInfo structure based
// on the supplied window pointer.  Upon failure, GetPane()
// returns an empty wxAuiPaneInfo, a condition which can be checked
// by calling wxAuiPaneInfo::IsOk().
//
// The pane info's structure may then be modified.  Once a pane's
// info is modified, wxAuiManager::Update() must be called to
// realize the changes in the UI.

wxAuiPaneInfo& wxAuiManager::GetPane(wxWindow* window) const
{
    int i, pane_count;
    for (i = 0, pane_count = m_panes.GetCount(); i < pane_count; ++i)
    {
        wxAuiPaneInfo& p = m_panes.Item(i);
        if (p.window == window)
            return p;
    }
    return wxAuiNullPaneInfo;
}

// this version of GetPane() looks up a pane based on a
// 'pane name', see above comment for more info
wxAuiPaneInfo& wxAuiManager::GetPane(const wxString& name) const
{
    int i, pane_count;
    for (i = 0, pane_count = m_panes.GetCount(); i < pane_count; ++i)
    {
        wxAuiPaneInfo& p = m_panes.Item(i);
        if (p.name == name)
            return p;
    }
    return wxAuiNullPaneInfo;
}

// GetAllPanes() returns a reference to all the pane info structures
wxAuiPaneInfoArray& wxAuiManager::GetAllPanes()
{
    return m_panes;
}

// This version of GetPane() looks up a pane based on its index position.
wxAuiPaneInfo& wxAuiManager::GetPane(size_t paneIndex) const
{
    size_t paneCount = m_panes.GetCount();
    if(paneCount>paneIndex)
    {
        return m_panes[paneIndex];
    }

    wxASSERT_MSG(0, wxT("Invalid pane index passed to wxAuiManager::GetPane"));
    return wxAuiNullPaneInfo;
}

// GetPaneCount() returns the total number of pages managed by the multi-notebook.
size_t wxAuiManager::GetPaneCount() const
{
    return m_panes.GetCount();
}

// FindTab() finds the tab control that currently contains the window as well
// as the index of the window in the tab control.  It returns true if the
// window was found, otherwise false.
bool wxAuiManager::FindTab(wxWindow* page, wxAuiTabContainer** ctrl, int* idx)
{
    int uiPartsCount = m_uiParts.GetCount();
    int i;
    for (i = 0; i < uiPartsCount; i++)
    {
        wxAuiDockUIPart& part = m_uiParts.Item(i);
        if(part.m_tab_container)
        {
            int pageIndex = part.m_tab_container->GetIdxFromWindow(page);
            if(pageIndex != wxNOT_FOUND)
            {
                *ctrl = part.m_tab_container;
                *idx = pageIndex;
                return true;
            }
        }
    }
    return false;
}

// SetFlag() and HasFlag() allow the owner to set/get various
// options which are global to wxAuiManager
bool wxAuiManager::HasFlag(int flag) const
{
    return (m_flags & flag) != 0;
}

void wxAuiManager::SetFlag(int flag, bool optionState)
{
    if (optionState)
        SetFlags(m_flags | flag);
    else
        SetFlags(m_flags & ~flag);
}

wxAuiTabArt* wxAuiManager::GetTabArtProvider() const
{
    return m_tab_art;
}


void wxAuiManager::SetTabArtProvider(wxAuiTabArt* artProvider)
{
    // delete the last art provider, if any
    delete m_tab_art;

    // assign the new art provider
    m_tab_art = artProvider;
}

int wxAuiManager::GetActivePane(wxWindow* focus) const
{
    int i, paneCount;

    // First try to find a pane that has the focus flag and whose window has actual focus.
    // This allows us to pick the right pane in the event that multiple panes have the focus flag set.
    for (i = 0, paneCount = m_panes.GetCount(); i < paneCount; ++i)
    {
        wxAuiPaneInfo& pane = m_panes[i];
        if(pane.HasFlag(wxAuiPaneInfo::optionActive))
        {
            if(!focus || pane.GetWindow()==focus)
            {
                return i;
            }
        }
    }
    // If no panes had the focus flag and actual focus then fall back to returning the first pane with focus flag.
    for (i = 0, paneCount = m_panes.GetCount(); i < paneCount; ++i)
    {
        wxAuiPaneInfo& pane = m_panes[i];
        if(pane.HasFlag(wxAuiPaneInfo::optionActive))
        {
            return i;
        }
    }
    return wxNOT_FOUND;
}

// HitTest() is an internal function which determines
// which UI item the specified coordinates are over
// (x,y) specify a position in client coordinates
wxAuiDockUIPart* wxAuiManager::HitTest(int x, int y)
{
    wxAuiDockUIPart* result = NULL;

    int i, part_count;
    for (i = 0, part_count = m_uiParts.GetCount(); i < part_count; ++i)
    {
        wxAuiDockUIPart* item = &m_uiParts.Item(i);

        // we are not interested in typeDock, because this space
        // isn't used to draw anything, just for measurements;
        // besides, the entire dock area is covered with other
        // rectangles, which we are interested in.
        if (item->type == wxAuiDockUIPart::typeDock)
            continue;

        // if we already have a hit on a more specific item, we are not
        // interested in a pane hit.  If, however, we don't already have
        // a hit, returning a pane hit is necessary for some operations
        if ((item->type == wxAuiDockUIPart::typePane ||
            item->type == wxAuiDockUIPart::typePaneBorder) && result)
            continue;

        // if the point is inside the rectangle, we have a hit
        if (item->rect.Contains(x,y))
            result = item;
    }

    return result;
}


// SetFlags() and GetFlags() allow the owner to set various
// options which are global to wxAuiManager
void wxAuiManager::SetFlags(unsigned int flags)
{
    // find out if we have to call UpdateHintWindowConfig()
    bool update_hint_wnd = false;
    unsigned int hint_mask = wxAUI_MGR_TRANSPARENT_HINT |
                             wxAUI_MGR_VENETIAN_BLINDS_HINT |
                             wxAUI_MGR_RECTANGLE_HINT;
    if ((flags & hint_mask) != (m_flags & hint_mask))
        update_hint_wnd = true;


    // set the new flags
    m_flags = flags;

    if (update_hint_wnd)
    {
        UpdateHintWindowConfig();
    }
}

unsigned int wxAuiManager::GetFlags() const
{
    return m_flags;
}

// Convenience function
static
bool wxAuiManager_HasLiveResize(wxAuiManager& manager)
{
    // With Core Graphics on Mac, it's not possible to show sash feedback,
    // so we'll always use live update instead.
#if defined(__WXMAC__)
    wxUnusedVar(manager);
    return true;
#else
    return (manager.GetFlags() & wxAUI_MGR_LIVE_RESIZE) == wxAUI_MGR_LIVE_RESIZE;
#endif
}

// don't use these anymore as they are deprecated
// use Set/GetManagedFrame() instead
void wxAuiManager::SetFrame(wxFrame* frame)
{
    SetManagedWindow((wxWindow*)frame);
}

wxFrame* wxAuiManager::GetFrame() const
{
    return (wxFrame*)m_frame;
}


// this function will return the aui manager for a given
// window.  The |window| parameter should be any child window
// or grand-child window (and so on) of the frame/window
// managed by wxAuiManager.  The |window| parameter does not
// need to be managed by the manager itself.
wxAuiManager* wxAuiManager::GetManager(wxWindow* window)
{
    wxAuiManagerEvent evt(wxEVT_AUI_FIND_MANAGER);
    evt.SetManager(NULL);
    evt.SetEventObject(window);
    evt.ResumePropagation(wxEVENT_PROPAGATE_MAX);
    if (!window->GetEventHandler()->ProcessEvent(evt))
        return NULL;

    return evt.GetManager();
}


void wxAuiManager::UpdateHintWindowConfig()
{
    // find out if the system can do transparent frames
    bool can_do_transparent = false;

    wxWindow* w = m_frame;
    while (w)
    {
        if (wxDynamicCast(w, wxFrame))
        {
            wxFrame* f = static_cast<wxFrame*>(w);
            can_do_transparent = f->CanSetTransparent();

            break;
        }

        w = w->GetParent();
    }

    // if there is an existing hint window, delete it
    if (m_hintWnd)
    {
        m_hintWnd->Destroy();
        m_hintWnd = NULL;
    }

    m_hintFadeMax = 50;
    m_hintWnd = NULL;

    if ((m_flags & wxAUI_MGR_TRANSPARENT_HINT) && can_do_transparent)
    {
        // Make a window to use for a transparent hint
        #if defined(__WXMSW__) || defined(__WXGTK__)
            m_hintWnd = new wxFrame(m_frame, wxID_ANY, wxEmptyString,
                                     wxDefaultPosition, wxSize(1,1),
                                         wxFRAME_TOOL_WINDOW |
                                         wxFRAME_FLOAT_ON_PARENT |
                                         wxFRAME_NO_TASKBAR |
                                         wxNO_BORDER);

            m_hintWnd->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_ACTIVECAPTION));
        #elif defined(__WXMAC__)
            // Using a miniframe with float and tool styles keeps the parent
            // frame activated and highlighted as such...
            m_hintWnd = new wxMiniFrame(m_frame, wxID_ANY, wxEmptyString,
                                         wxDefaultPosition, wxSize(1,1),
                                         wxFRAME_FLOAT_ON_PARENT
                                         | wxFRAME_TOOL_WINDOW );
            m_hintWnd->Connect(wxEVT_ACTIVATE,
                wxActivateEventHandler(wxAuiManager::OnHintActivate), NULL, this);

            // Can't set the bg colour of a Frame in wxMac
            wxPanel* p = new wxPanel(m_hintWnd);

            // The default wxSYS_COLOUR_ACTIVECAPTION colour is a light silver
            // color that is really hard to see, especially transparent.
            // Until a better system color is decided upon we'll just use
            // blue.
            p->SetBackgroundColour(*wxBLUE);
        #endif

    }
    else
    {
        if ((m_flags & wxAUI_MGR_TRANSPARENT_HINT) != 0 ||
            (m_flags & wxAUI_MGR_VENETIAN_BLINDS_HINT) != 0)
        {
            // system can't support transparent fade, or the venetian
            // blinds effect was explicitly requested
            m_hintWnd = new wxPseudoTransparentFrame(m_frame,
                                                      wxID_ANY,
                                                      wxEmptyString,
                                                      wxDefaultPosition,
                                                      wxSize(1,1),
                                                            wxFRAME_TOOL_WINDOW |
                                                            wxFRAME_FLOAT_ON_PARENT |
                                                            wxFRAME_NO_TASKBAR |
                                                            wxNO_BORDER);
            m_hintFadeMax = 128;
        }
    }
}


// SetManagedWindow() is usually called once when the frame
// manager class is being initialized.  "frame" specifies
// the frame which should be managed by the frame manager
void wxAuiManager::SetManagedWindow(wxWindow* wnd)
{
    wxASSERT_MSG(wnd, wxT("specified window must be non-NULL"));

    m_frame = wnd;
    m_frame->PushEventHandler(this);

#if wxUSE_MDI
    // if the owner is going to manage an MDI parent frame,
    // we need to add the MDI client window as the default
    // center pane

    if (wxDynamicCast(m_frame, wxMDIParentFrame))
    {
        wxMDIParentFrame* mdi_frame = (wxMDIParentFrame*)m_frame;
        wxWindow* client_window = mdi_frame->GetClientWindow();

        wxASSERT_MSG(client_window, wxT("Client window is NULL!"));

        AddPane(client_window,
                wxAuiPaneInfo().Name(wxT("mdiclient")).
                CenterPane().PaneBorder(false));
    }
    else if (wxDynamicCast(m_frame, wxAuiMDIParentFrame))
    {
        wxAuiMDIParentFrame* mdi_frame = (wxAuiMDIParentFrame*)m_frame;
        wxAuiMDIClientWindow* client_window = mdi_frame->GetClientWindow();
        wxASSERT_MSG(client_window, wxT("Client window is NULL!"));

        AddPane(client_window,
                wxAuiPaneInfo().Name(wxT("mdiclient")).
                CenterPane().PaneBorder(false));
    }

#endif

    UpdateHintWindowConfig();
}


// UnInit() must be called, usually in the destructor
// of the frame class.   If it is not called, usually this
// will result in a crash upon program exit
void wxAuiManager::UnInit()
{
    if (m_frame)
    {
        m_frame->RemoveEventHandler(this);
    }
}

// GetManagedWindow() returns the window pointer being managed
wxWindow* wxAuiManager::GetManagedWindow() const
{
    return m_frame;
}

wxAuiDockArt* wxAuiManager::GetArtProvider() const
{
    return m_art;
}

void wxAuiManager::ProcessMgrEvent(wxAuiManagerEvent& event)
{
    // first, give the owner frame a chance to override
    if (m_frame)
    {
        if (m_frame->GetEventHandler()->ProcessEvent(event))
            return;
    }

    ProcessEvent(event);
}

// SetArtProvider() instructs wxAuiManager to use the
// specified art provider for all drawing calls.  This allows
// plugable look-and-feel features.  The pointer that is
// passed to this method subsequently belongs to wxAuiManager,
// and is deleted in the frame manager destructor
void wxAuiManager::SetArtProvider(wxAuiDockArt* art_provider)
{
    // delete the last art provider, if any
    delete m_art;

    // assign the new art provider
    m_art = art_provider;
}


bool wxAuiManager::AddPane(wxWindow* window, const wxAuiPaneInfo& paneInfo)
{
    wxASSERT_MSG(window, wxT("NULL window ptrs are not allowed"));

    // check if the pane has a valid window
    if (!window)
        return false;

    // check if the window is already managed by us
    if (GetPane(paneInfo.window).IsOk())
        return false;

    // check if the pane name already exists, this could reveal a
    // bug in the library user's application
    bool already_exists = false;
    if (!paneInfo.name.empty() && GetPane(paneInfo.name).IsOk())
    {
        wxFAIL_MSG(wxT("A pane with that name already exists in the manager!"));
        already_exists = true;
    }

    // if the new pane is docked then we should undo maximize
    if (paneInfo.IsDocked())
        RestoreMaximizedPane();

    // special case:  wxAuiToolBar style interacts with docking flags
    wxAuiPaneInfo test(paneInfo);
    wxAuiToolBar* toolbar = wxDynamicCast(window, wxAuiToolBar);
    if (toolbar)
    {
        // if pane has default docking flags
        const unsigned int dockMask = wxAuiPaneInfo::optionLeftDockable |
                                        wxAuiPaneInfo::optionRightDockable |
                                        wxAuiPaneInfo::optionTopDockable |
                                        wxAuiPaneInfo::optionBottomDockable;
        const unsigned int defaultDock = wxAuiPaneInfo().
                                            DefaultPane().state & dockMask;
        if ((test.state & dockMask) == defaultDock)
        {
            // set docking flags based on toolbar style
            if (toolbar->GetWindowStyleFlag() & wxAUI_TB_VERTICAL)
            {
                test.TopDockable(false).BottomDockable(false);
            }
            else if (toolbar->GetWindowStyleFlag() & wxAUI_TB_HORIZONTAL)
            {
                test.LeftDockable(false).RightDockable(false);
            }
        }
        else
        {
            // see whether non-default docking flags are valid
            test.window = window;
            if (test.window != window)
                return false;
        }
    }

    m_panes.Add(test);

    wxAuiPaneInfo& pinfo = m_panes.Last();

    // set the pane window
    pinfo.window = window;


    // if the pane's name identifier is blank, create a random string
    if (pinfo.name.empty() || already_exists)
    {
        pinfo.name.Printf(wxT("%08lx%08x%08x%08lx"),
             (unsigned long)(wxPtrToUInt(pinfo.window) & 0xffffffff),
             (unsigned int)time(NULL),
             (unsigned int)clock(),
             (unsigned long)m_panes.GetCount());
    }

    // set initial proportion (if not already set)
    if (pinfo.dock_proportion == 0)
        pinfo.dock_proportion = 100000;

    if (pinfo.HasMaximizeButton())
    {
        wxAuiPaneButton button;
        button.button_id = wxAUI_BUTTON_MAXIMIZE_RESTORE;
        pinfo.buttons.Add(button);
    }

    if (pinfo.HasPinButton())
    {
        wxAuiPaneButton button;
        button.button_id = wxAUI_BUTTON_PIN;
        pinfo.buttons.Add(button);
    }

    if (pinfo.HasCloseButton())
    {
        wxAuiPaneButton button;
        button.button_id = wxAUI_BUTTON_CLOSE;
        pinfo.buttons.Add(button);
    }

    if (pinfo.HasGripper())
    {
        if (wxDynamicCast(pinfo.window, wxAuiToolBar))
        {
            // prevent duplicate gripper -- both wxAuiManager and wxAuiToolBar
            // have a gripper control.  The toolbar's built-in gripper
            // meshes better with the look and feel of the control than ours,
            // so turn wxAuiManager's gripper off, and the toolbar's on.

            wxAuiToolBar* tb = static_cast<wxAuiToolBar*>(pinfo.window);
            pinfo.SetFlag(wxAuiPaneInfo::optionGripper, false);
            tb->SetGripperVisible(true);
        }
    }


    if (pinfo.best_size == wxDefaultSize &&
        pinfo.window)
    {
        pinfo.best_size = pinfo.window->GetClientSize();

#if wxUSE_TOOLBAR
        if (wxDynamicCast(pinfo.window, wxToolBar))
        {
            // GetClientSize() doesn't get the best size for
            // a toolbar under some newer versions of wxWidgets,
            // so use GetBestSize()
            pinfo.best_size = pinfo.window->GetBestSize();
        }
#endif // wxUSE_TOOLBAR

        if (pinfo.min_size != wxDefaultSize)
        {
            if (pinfo.best_size.x < pinfo.min_size.x)
                pinfo.best_size.x = pinfo.min_size.x;
            if (pinfo.best_size.y < pinfo.min_size.y)
                pinfo.best_size.y = pinfo.min_size.y;
        }
    }



    return true;
}

bool wxAuiManager::AddPane(wxWindow* window,
                           int direction,
                           const wxString& caption)
{
    wxAuiPaneInfo pinfo;
    pinfo.Caption(caption);
    switch (direction)
    {
        case wxTOP:    pinfo.Top(); break;
        case wxBOTTOM: pinfo.Bottom(); break;
        case wxLEFT:   pinfo.Left(); break;
        case wxRIGHT:  pinfo.Right(); break;
        case wxCENTER: pinfo.CenterPane(); break;
    }
    return AddPane(window, pinfo);
}

bool wxAuiManager::AddPane(wxWindow* window,
                           const wxAuiPaneInfo& paneInfo,
                           const wxPoint& drop_pos)
{
    if (!AddPane(window, paneInfo))
        return false;

    wxAuiPaneInfo& pane = GetPane(window);

    DoDrop(m_docks, m_panes, pane, drop_pos, wxPoint(0,0));

    return true;
}

bool wxAuiManager::InsertPane(wxWindow* window, const wxAuiPaneInfo& paneInfo,
                                int insert_level)
{
    wxASSERT_MSG(window, wxT("NULL window ptrs are not allowed"));

    // shift the panes around, depending on the insert level
    switch (insert_level)
    {
        case wxAUI_INSERT_PANE:
            DoInsertPane(m_panes,
                 paneInfo.dock_direction,
                 paneInfo.dock_layer,
                 paneInfo.dock_row,
                 paneInfo.dock_pos);
            break;
        case wxAUI_INSERT_ROW:
            DoInsertDockRow(m_panes,
                 paneInfo.dock_direction,
                 paneInfo.dock_layer,
                 paneInfo.dock_row);
            break;
        case wxAUI_INSERT_DOCK:
            DoInsertDockLayer(m_panes,
                 paneInfo.dock_direction,
                 paneInfo.dock_layer);
            break;
    }

    // if the window already exists, we are basically just moving/inserting the
    // existing window.  If it doesn't exist, we need to add it and insert it
    wxAuiPaneInfo& existing_pane = GetPane(window);
    if (!existing_pane.IsOk())
    {
        return AddPane(window, paneInfo);
    }
    else
    {
        if (paneInfo.IsFloating())
        {
            existing_pane.Float();
            if (paneInfo.floating_pos != wxDefaultPosition)
                existing_pane.FloatingPosition(paneInfo.floating_pos);
            if (paneInfo.floating_size != wxDefaultSize)
                existing_pane.FloatingSize(paneInfo.floating_size);
        }
        else
        {
            // if the new pane is docked then we should undo maximize
            RestoreMaximizedPane();

            existing_pane.Direction(paneInfo.dock_direction);
            existing_pane.Layer(paneInfo.dock_layer);
            existing_pane.Row(paneInfo.dock_row);
            existing_pane.Position(paneInfo.dock_pos);
        }
    }

    return true;
}


// DetachPane() removes a pane from the frame manager.  This
// method will not destroy the window that is removed.
bool wxAuiManager::DetachPane(wxWindow* window)
{
    wxASSERT_MSG(window, wxT("NULL window ptrs are not allowed"));

    int i, count;
    for (i = 0, count = m_panes.GetCount(); i < count; ++i)
    {
        wxAuiPaneInfo& p = m_panes.Item(i);
        if (p.window == window)
        {
            if (p.frame)
            {
                // we have a floating frame which is being detached. We need to
                // reparent it to m_frame and destroy the floating frame

                // reduce flicker
                p.window->SetSize(1,1);

                if (p.frame->IsShown())
                    p.frame->Show(false);

                // reparent to m_frame and destroy the pane
                if (m_actionWindow == p.frame)
                {
                    m_actionWindow = NULL;
                }

                p.window->Reparent(m_frame);
                p.frame->SetSizer(NULL);
                p.frame->Destroy();
                p.frame = NULL;
            }

            // make sure there are no references to this pane in our uiparts,
            // just in case the caller doesn't call Update() immediately after
            // the DetachPane() call.  This prevets obscure crashes which would
            // happen at window repaint if the caller forgets to call Update()
            int pi, part_count;
            for (pi = 0, part_count = (int)m_uiParts.GetCount(); pi < part_count; ++pi)
            {
                wxAuiDockUIPart& part = m_uiParts.Item(pi);
                if (part.pane == &p)
                {
                    m_uiParts.RemoveAt(pi);
                    part_count--;
                    pi--;
                    continue;
                }

                if (part.m_tab_container)
                    part.m_tab_container->RemovePage(window);

            }

            m_panes.RemoveAt(i);
            return true;
        }
    }
    return false;
}

// ClosePane() destroys or hides the pane depending on its flags
bool wxAuiManager::ClosePane(wxAuiPaneInfo& paneInfo)
{
    // If we are a wxAuiNotebook then we must fire off a EVT_AUINOTEBOOK_PAGE_CLOSE event and give the user an opportunity to veto it.
    wxAuiNotebook *notebook = wxDynamicCast(GetManagedWindow(),wxAuiNotebook);

    if (notebook)
    {
        wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_PAGE_CLOSE, GetManagedWindow()->GetId());
        e.SetSelection(GetAllPanes().Index(paneInfo));
        e.SetEventObject(GetManagedWindow());
        GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
        if (!e.IsAllowed())
            return false;
    }


    // fire pane close event, allow opportunity for users to veto close
    wxAuiManagerEvent e(wxEVT_AUI_PANE_CLOSE);
    e.SetPane(&paneInfo);
    ProcessMgrEvent(e);
    if (e.GetVeto())
        return false;


    // If the pane is the active page of a notebook, activate the next page, or the previous one if no next exists
    if (paneInfo.HasFlag(wxAuiPaneInfo::optionActiveNotebook))
    {
        wxAuiTabContainer* ctrl;
        int                ctrlIndex;

        if(FindTab(paneInfo.GetWindow(), &ctrl, &ctrlIndex))
        {
            size_t n_pages   = ctrl->GetPageCount();

            if (n_pages > 1)
            {
                size_t cur_page = (size_t)ctrlIndex;
                size_t new_page = cur_page == n_pages-1 ? cur_page-1 : cur_page+1;
                ctrl->SetActivePage(new_page);
            }
        }

    }


    // if we were maximized, restore
    if (paneInfo.IsMaximized())
    {
        RestorePane(paneInfo);
    }

    // first, hide the window
    if (paneInfo.window && paneInfo.window->IsShown())
    {
        paneInfo.window->Show(false);
    }

    // make sure that we are the parent of this window
    if (paneInfo.window && paneInfo.window->GetParent() != m_frame)
    {
        paneInfo.window->Reparent(m_frame);
    }

    // if we have a frame, destroy it
    if (paneInfo.frame)
    {
        paneInfo.frame->Destroy();
        paneInfo.frame = NULL;
    }

    // now we need to either destroy or hide the pane
    if (paneInfo.HasDestroyOnClose())
    {
        wxWindow * window = paneInfo.window;
        DetachPane(window);
        if (window)
        {
            window->Destroy();
        }
    }
    else
    {
        paneInfo.Hide();
    }

    // If we are a wxAuiNotebook then we must fire off a EVT_AUINOTEBOOK_PAGE_CLOSED event to notify user of change.
    if (notebook)
    {
        wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_PAGE_CLOSED, GetManagedWindow()->GetId());
        e.SetSelection(GetAllPanes().Index(paneInfo));
        e.SetEventObject(GetManagedWindow());
        GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
    }

    return true;
}

void wxAuiManager::MaximizePane(wxAuiPaneInfo& paneInfo)
{
    int i, pane_count;

    // un-maximize and hide all other panes
    for (i = 0, pane_count = m_panes.GetCount(); i < pane_count; ++i)
    {
        wxAuiPaneInfo& p = m_panes.Item(i);
        if (!p.IsToolbar() && !p.IsFloating())
        {
            p.Restore();

            // save hidden state
            p.SetFlag(wxAuiPaneInfo::savedHiddenState,
                      p.HasFlag(wxAuiPaneInfo::optionHidden));
			p.SetFlag(wxAuiPaneInfo::savedActiveNotebookState,
                      p.HasFlag(wxAuiPaneInfo::optionActiveNotebook));

            // hide the pane, because only the newly
            // maximized pane should show
            p.Hide();
        }
    }

    // mark ourselves maximized
    paneInfo.Maximize();
    paneInfo.Show();
    m_hasMaximized = true;

    // last, show the window
    if (paneInfo.window && !paneInfo.window->IsShown())
    {
        paneInfo.window->Show(true);
    }
}

void wxAuiManager::RestorePane(wxAuiPaneInfo& paneInfo)
{
    int i, pane_count;

    // restore all the panes
    for (i = 0, pane_count = m_panes.GetCount(); i < pane_count; ++i)
    {
        wxAuiPaneInfo& p = m_panes.Item(i);
        if (!p.IsToolbar() && !p.IsFloating())
        {
            p.SetFlag(wxAuiPaneInfo::optionHidden,
                      p.HasFlag(wxAuiPaneInfo::savedHiddenState));
			p.SetFlag(wxAuiPaneInfo::optionActiveNotebook,
                      p.HasFlag(wxAuiPaneInfo::savedActiveNotebookState));
        }
    }

    // mark ourselves non-maximized
    paneInfo.Restore();
    m_hasMaximized = false;

    // last, show the window
    if (paneInfo.window && !paneInfo.window->IsShown())
    {
        paneInfo.window->Show(true);
    }
}

void wxAuiManager::RestoreMaximizedPane()
{
    int i, pane_count;

    // restore all the panes
    for (i = 0, pane_count = m_panes.GetCount(); i < pane_count; ++i)
    {
        wxAuiPaneInfo& p = m_panes.Item(i);
        if (p.IsMaximized())
        {
            RestorePane(p);
            break;
        }
    }
}

// EscapeDelimiters() changes ";" into "\;" and "|" into "\|"
// in the input string.  This is an internal functions which is
// used for saving perspectives
static wxString EscapeDelimiters(const wxString& s)
{
    wxString result;
    result.Alloc(s.length());
    const wxChar* ch = s.c_str();
    while (*ch)
    {
        if (*ch == wxT(';') || *ch == wxT('|'))
            result += wxT('\\');
        result += *ch;
        ++ch;
    }
    return result;
}

wxString wxAuiPaneInfo::GetInfo() const
{
    wxString result = wxT("name=");
    result += EscapeDelimiters(name);
    result += wxT(";");

    result += wxT("caption=");
    result += EscapeDelimiters(caption);
    result += wxT(";");

    result += wxT("tooltip=");
    result += EscapeDelimiters(m_tooltip);
    result += wxT(";");

    result += wxString::Format(wxT("state=%u;"), state);
    result += wxString::Format(wxT("dir=%d;"), dock_direction);
    result += wxString::Format(wxT("layer=%d;"), dock_layer);
    result += wxString::Format(wxT("row=%d;"), dock_row);
    result += wxString::Format(wxT("pos=%d;"), dock_pos);
    result += wxString::Format(wxT("prop=%d;"), dock_proportion);
    result += wxString::Format(wxT("page=%d;"), m_dock_page);
    result += wxString::Format(wxT("bestw=%d;"), best_size.x);
    result += wxString::Format(wxT("besth=%d;"), best_size.y);
    result += wxString::Format(wxT("minw=%d;"), min_size.x);
    result += wxString::Format(wxT("minh=%d;"), min_size.y);
    result += wxString::Format(wxT("maxw=%d;"), max_size.x);
    result += wxString::Format(wxT("maxh=%d;"), max_size.y);
    result += wxString::Format(wxT("floatx=%d;"), floating_pos.x);
    result += wxString::Format(wxT("floaty=%d;"), floating_pos.y);
    result += wxString::Format(wxT("floatw=%d;"), floating_size.x);
    result += wxString::Format(wxT("floath=%d;"), floating_size.y);
    return result;
}
wxString wxAuiManager::SavePaneInfo(wxAuiPaneInfo& pane)
{
    return pane.GetInfo();
}

void wxAuiPaneInfo::LoadInfo(wxString& info)
{
    // replace escaped characters so we can
    // split up the string easily
    info.Replace(wxT("\\|"), wxT("\a"));
    info.Replace(wxT("\\;"), wxT("\b"));

    while(1)
    {
        wxString val_part = info.BeforeFirst(wxT(';'));
        info = info.AfterFirst(wxT(';'));
        wxString val_name = val_part.BeforeFirst(wxT('='));
        wxString value = val_part.AfterFirst(wxT('='));
        val_name.MakeLower();
        val_name.Trim(true);
        val_name.Trim(false);
        value.Trim(true);
        value.Trim(false);

        if (val_name.empty())
            break;

        if (val_name == wxT("name"))
            name = value;
        else if (val_name == wxT("caption"))
            caption = value;
        else if (val_name == wxT("tooltip"))
            m_tooltip = value;
        else if (val_name == wxT("state"))
            state = (unsigned int)wxAtoi(value.c_str());
        else if (val_name == wxT("dir"))
            dock_direction = wxAtoi(value.c_str());
        else if (val_name == wxT("layer"))
            dock_layer = wxAtoi(value.c_str());
        else if (val_name == wxT("row"))
            dock_row = wxAtoi(value.c_str());
        else if (val_name == wxT("pos"))
            dock_pos = wxAtoi(value.c_str());
        else if (val_name == wxT("prop"))
            dock_proportion = wxAtoi(value.c_str());
        else if (val_name == wxT("page"))
            m_dock_page = wxAtoi(value.c_str());
        else if (val_name == wxT("bestw"))
            best_size.x = wxAtoi(value.c_str());
        else if (val_name == wxT("besth"))
            best_size.y = wxAtoi(value.c_str());
        else if (val_name == wxT("minw"))
            min_size.x = wxAtoi(value.c_str());
        else if (val_name == wxT("minh"))
            min_size.y = wxAtoi(value.c_str());
        else if (val_name == wxT("maxw"))
            max_size.x = wxAtoi(value.c_str());
        else if (val_name == wxT("maxh"))
            max_size.y = wxAtoi(value.c_str());
        else if (val_name == wxT("floatx"))
            floating_pos.x = wxAtoi(value.c_str());
        else if (val_name == wxT("floaty"))
            floating_pos.y = wxAtoi(value.c_str());
        else if (val_name == wxT("floatw"))
            floating_size.x = wxAtoi(value.c_str());
        else if (val_name == wxT("floath"))
            floating_size.y = wxAtoi(value.c_str());
        else {
            wxFAIL_MSG(wxT("Bad Perspective String"));
        }
    }

    // replace escaped characters so we can
    // split up the string easily
    name.Replace(wxT("\a"), wxT("|"));
    name.Replace(wxT("\b"), wxT(";"));
    caption.Replace(wxT("\a"), wxT("|"));
    caption.Replace(wxT("\b"), wxT(";"));
    m_tooltip.Replace(wxT("\a"), wxT("|"));
    m_tooltip.Replace(wxT("\b"), wxT(";"));
    info.Replace(wxT("\a"), wxT("|"));
    info.Replace(wxT("\b"), wxT(";"));

    // sanitize values
    SetFlag(optionActive, false);
}

// Load a "pane" with the pane infor settings in panePart
void wxAuiManager::LoadPaneInfo(wxString panePart, wxAuiPaneInfo &pane)
{
    pane.LoadInfo(panePart);
    return;
}


// SavePerspective() saves all pane information as a single string.
// This string may later be fed into LoadPerspective() to restore
// all pane settings.  This save and load mechanism allows an
// exact pane configuration to be saved and restored at a later time

wxString wxAuiManager::SavePerspective()
{
    wxString result;
    result.Alloc(500);
    result = wxT("layout2|");

    int pane_i, pane_count = m_panes.GetCount();
    for (pane_i = 0; pane_i < pane_count; ++pane_i)
    {
        wxAuiPaneInfo& pane = m_panes.Item(pane_i);
        result += SavePaneInfo(pane)+wxT("|");
    }

    int dock_i, dock_count = m_docks.GetCount();
    for (dock_i = 0; dock_i < dock_count; ++dock_i)
    {
        wxAuiDockInfo& dock = m_docks.Item(dock_i);

        result += wxString::Format(wxT("dock_size(%d,%d,%d)=%d|"),
                                   dock.dock_direction, dock.dock_layer,
                                   dock.dock_row, dock.size);
    }

    return result;
}

// LoadPerspective() loads a layout which was saved with SavePerspective()
// If the "update" flag parameter is true, the GUI will immediately be updated

bool wxAuiManager::LoadPerspective(const wxString& layout, bool update)
{
    wxString input = layout;
    wxString part;

    // check layout string version
    //    'layout1' = wxAUI 0.9.0 - wxAUI 0.9.2
    //    'layout2' = wxAUI 0.9.2 (wxWidgets 2.8)
    part = input.BeforeFirst(wxT('|'));
    input = input.AfterFirst(wxT('|'));
    part.Trim(true);
    part.Trim(false);
    if (part != wxT("layout2"))
        return false;

    // Mark all panes currently managed as hidden. Also, dock all panes that are dockable.
    int pane_i, pane_count = m_panes.GetCount();
    for (pane_i = 0; pane_i < pane_count; ++pane_i)
    {
        wxAuiPaneInfo& p = m_panes.Item(pane_i);
        if(p.IsDockable())
            p.Dock();
        p.Hide();
    }

    // clear out the dock array; this will be reconstructed
    m_docks.Clear();

    // replace escaped characters so we can
    // split up the string easily
    input.Replace(wxT("\\|"), wxT("\a"));
    input.Replace(wxT("\\;"), wxT("\b"));

    m_hasMaximized = false;
    while (1)
    {
        wxAuiPaneInfo pane;

        wxString pane_part = input.BeforeFirst(wxT('|'));
        input = input.AfterFirst(wxT('|'));
        pane_part.Trim(true);

        // if the string is empty, we're done parsing
        if (pane_part.empty())
            break;

        if (pane_part.Left(9) == wxT("dock_size"))
        {
            wxString val_name = pane_part.BeforeFirst(wxT('='));
            wxString value = pane_part.AfterFirst(wxT('='));

            long dir, layer, row, size;
            wxString piece = val_name.AfterFirst(wxT('('));
            piece = piece.BeforeLast(wxT(')'));
            piece.BeforeFirst(wxT(',')).ToLong(&dir);
            piece = piece.AfterFirst(wxT(','));
            piece.BeforeFirst(wxT(',')).ToLong(&layer);
            piece.AfterFirst(wxT(',')).ToLong(&row);
            value.ToLong(&size);

            wxAuiDockInfo dock;
            dock.dock_direction = dir;
            dock.dock_layer = layer;
            dock.dock_row = row;
            dock.size = size;
            m_docks.Add(dock);
            continue;
        }

        // Undo our escaping as LoadPaneInfo needs to take an unescaped
        // name so it can be called by external callers
        pane_part.Replace(wxT("\a"), wxT("|"));
        pane_part.Replace(wxT("\b"), wxT(";"));

        LoadPaneInfo(pane_part, pane);

        if ( pane.IsMaximized() )
            m_hasMaximized = true;

        wxAuiPaneInfo& p = GetPane(pane.name);
        if (!p.IsOk())
        {
            // the pane window couldn't be found
            // in the existing layout -- skip it
            continue;
        }

        p.SafeSet(pane);
    }

    if (update)
        Update();

    return true;
}

void wxAuiManager::GetPanePositionsAndSizes(wxAuiDockInfo& dock,
                                            wxArrayInt& positions,
                                            wxArrayInt& sizes)
{
    int caption_size = m_art->GetMetric(wxAUI_DOCKART_CAPTION_SIZE);
    int pane_borderSize = m_art->GetMetric(wxAUI_DOCKART_PANE_BORDER_SIZE);
    int gripperSize = m_art->GetMetric(wxAUI_DOCKART_GRIPPER_SIZE);

    positions.Empty();
    sizes.Empty();

    int offset, action_pane = -1;
    int pane_i, pane_count = dock.panes.GetCount();

    // find the pane marked as our action pane
    for (pane_i = 0; pane_i < pane_count; ++pane_i)
    {
        wxAuiPaneInfo& pane = *(dock.panes.Item(pane_i));

        if (pane.HasFlag(wxAuiPaneInfo::actionPane))
        {
            wxASSERT_MSG(action_pane==-1, wxT("Too many fixed action panes"));
            action_pane = pane_i;
        }
    }

    // set up each panes default position, and
    // determine the size (width or height, depending
    // on the dock's orientation) of each pane
    for (pane_i = 0; pane_i < pane_count; ++pane_i)
    {
        wxAuiPaneInfo& pane = *(dock.panes.Item(pane_i));
        positions.Add(pane.dock_pos);
        int size = 0;

        if (pane.HasBorder())
            size += (pane_borderSize*2);

        if (dock.IsHorizontal())
        {
            if (pane.HasGripper() && !pane.HasGripperTop())
                size += gripperSize;
            size += pane.best_size.x;
        }
        else
        {
            if (pane.HasGripper() && pane.HasGripperTop())
                size += gripperSize;

            if (pane.HasCaption())
                size += caption_size;
            size += pane.best_size.y;
        }

        sizes.Add(size);
    }

    // if there is no action pane, just return the default
    // positions (as specified in pane.pane_pos)
    if (action_pane == -1)
        return;

    offset = 0;
    for (pane_i = action_pane-1; pane_i >= 0; --pane_i)
    {
        int amount = positions[pane_i+1] - (positions[pane_i] + sizes[pane_i]);

        if (amount >= 0)
            offset += amount;
        else
            positions[pane_i] -= -amount;

        offset += sizes[pane_i];
    }

    // if the dock mode is fixed, make sure none of the panes
    // overlap; we will bump panes that overlap
    offset = 0;
    for (pane_i = action_pane; pane_i < pane_count; ++pane_i)
    {
        int amount = positions[pane_i] - offset;
        if (amount >= 0)
            offset += amount;
        else
            positions[pane_i] += -amount;

        offset += sizes[pane_i];
    }
}


void wxAuiManager::LayoutAddPane(wxSizer* cont,
                                 wxAuiDockInfo& dock,
                                 wxAuiPaneInfo& pane,
                                 wxAuiDockUIPartArray& uiparts,
                                 bool spacer_only,
                                 bool allowtitlebar)
{
    wxAuiDockUIPart part;
    wxSizerItem* sizer_item;

    int caption_size = m_art->GetMetric(wxAUI_DOCKART_CAPTION_SIZE);
    int gripperSize = m_art->GetMetric(wxAUI_DOCKART_GRIPPER_SIZE);
    int pane_borderSize = m_art->GetMetric(wxAUI_DOCKART_PANE_BORDER_SIZE);
    int pane_button_size = m_art->GetMetric(wxAUI_DOCKART_PANE_BUTTON_SIZE);

    // find out the orientation of the item (orientation for panes
    // is the same as the dock's orientation)
    int orientation;
    if (dock.IsHorizontal())
        orientation = wxHORIZONTAL;
    else
        orientation = wxVERTICAL;

    // this variable will store the proportion
    // value that the pane will receive
    int pane_proportion = pane.dock_proportion;

    wxBoxSizer* horz_pane_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* vert_pane_sizer = new wxBoxSizer(wxVERTICAL);

    if (allowtitlebar && pane.HasGripper())
    {
        if (pane.HasGripperTop())
            sizer_item = vert_pane_sizer ->Add(1, gripperSize, 0, wxEXPAND);
        else
            sizer_item = horz_pane_sizer ->Add(gripperSize, 1, 0, wxEXPAND);

        part.type = wxAuiDockUIPart::typeGripper;
        part.dock = &dock;
        part.pane = &pane;
        part.button = NULL;
        part.orientation = orientation;
        part.cont_sizer = horz_pane_sizer;
        part.sizer_item = sizer_item;
        part.m_tab_container = NULL;
        uiparts.Add(part);
    }

    if (allowtitlebar && pane.HasCaption())
    {
        // create the caption sizer
        wxBoxSizer* caption_sizer = new wxBoxSizer(wxHORIZONTAL);

        sizer_item = caption_sizer->Add(1, caption_size, 1, wxEXPAND);

        part.type = wxAuiDockUIPart::typeCaption;
        part.dock = &dock;
        part.pane = &pane;
        part.button = NULL;
        part.orientation = orientation;
        part.cont_sizer = vert_pane_sizer;
        part.sizer_item = sizer_item;
        part.m_tab_container = NULL;
        int caption_part_idx = uiparts.GetCount();
        uiparts.Add(part);

        // add pane buttons to the caption
        int i, button_count;
        for (i = 0, button_count = pane.buttons.GetCount();
             i < button_count; ++i)
        {
            wxAuiPaneButton& button = pane.buttons.Item(i);

            sizer_item = caption_sizer->Add(pane_button_size,
                                            caption_size,
                                            0, wxEXPAND);

            part.type = wxAuiDockUIPart::typePaneButton;
            part.dock = &dock;
            part.pane = &pane;
            part.button = &button;
            part.orientation = orientation;
            part.cont_sizer = caption_sizer;
            part.sizer_item = sizer_item;
            part.m_tab_container = NULL;
            uiparts.Add(part);
        }

        // if we have buttons, add a little space to the right
        // of them to ease visual crowding
        if (button_count >= 1)
        {
            caption_sizer->Add(3,1);
        }

        // add the caption sizer
        sizer_item = vert_pane_sizer->Add(caption_sizer, 0, wxEXPAND);

        uiparts.Item(caption_part_idx).sizer_item = sizer_item;
    }

    // add the pane window itself
    if (spacer_only)
    {
        sizer_item = vert_pane_sizer->Add(1, 1, 1, wxEXPAND);
    }
    else
    {
        sizer_item = vert_pane_sizer->Add(pane.window, 1, wxEXPAND);
        // Don't do this because it breaks the pane size in floating windows
        // BIW: Right now commenting this out is causing problems with
        // an mdi client window as the center pane.
        vert_pane_sizer->SetItemMinSize(pane.window, 1, 1);
    }

    part.type = wxAuiDockUIPart::typePane;
    part.dock = &dock;
    part.pane = &pane;
    part.button = NULL;
    part.orientation = orientation;
    part.cont_sizer = vert_pane_sizer;
    part.sizer_item = sizer_item;
    part.m_tab_container = NULL;
    uiparts.Add(part);


    // determine if the pane should have a minimum size; if the pane is
    // non-resizable (fixed) then we must set a minimum size. Alternatively,
    // if the pane.min_size is set, we must use that value as well

    wxSize min_size = pane.min_size;
    if (pane.IsFixed())
    {
        if (min_size == wxDefaultSize)
        {
            min_size = pane.best_size;
            pane_proportion = 0;
        }
    }

    if (min_size != wxDefaultSize)
    {
        vert_pane_sizer->SetItemMinSize(
                        vert_pane_sizer->GetChildren().GetCount()-1,
                        min_size.x, min_size.y);
    }


    // add the vertical sizer (caption, pane window) to the
    // horizontal sizer (gripper, vertical sizer)
    horz_pane_sizer->Add(vert_pane_sizer, 1, wxEXPAND);

    // finally, add the pane sizer to the dock sizer

    if (pane.HasBorder())
    {
        int border_flags = wxALL;
        if(!allowtitlebar)
        {
            if (m_tab_art->m_flags & wxAUI_NB_TOP)
            {
                border_flags &= ~wxTOP;
            }
            if (m_tab_art->m_flags & wxAUI_NB_BOTTOM)
            {
                border_flags &= ~wxBOTTOM;
            }
            if (m_tab_art->m_flags & wxAUI_NB_LEFT)
            {
                border_flags &= ~wxLEFT;
            }
            if (m_tab_art->m_flags & wxAUI_NB_RIGHT)
            {
                border_flags &= ~wxRIGHT;
            }
            pane_borderSize = m_tab_art->GetAdditionalBorderSpace(pane.GetWindow());
        }
        // allowing space for the pane's border
        sizer_item = cont->Add(horz_pane_sizer, pane_proportion,
                               wxEXPAND | border_flags, pane_borderSize);

        part.type = wxAuiDockUIPart::typePaneBorder;
        part.dock = &dock;
        part.pane = &pane;
        part.button = NULL;
        part.orientation = orientation;
        part.cont_sizer = cont;
        part.sizer_item = sizer_item;
        part.m_tab_container = NULL;
        uiparts.Add(part);
    }
    else
    {
        sizer_item = cont->Add(horz_pane_sizer, pane_proportion, wxEXPAND);
    }
}

int wxAuiManager::GetNotebookFlags()
{
    int flags=0;
    if(HasFlag(wxAUI_MGR_NB_BOTTOM))
    {
        flags |= wxAUI_MGR_NB_BOTTOM;
    }
    else if(HasFlag(wxAUI_MGR_NB_LEFT))
    {
        flags |= wxAUI_MGR_NB_LEFT;
    }
    else if(HasFlag(wxAUI_MGR_NB_RIGHT))
    {
        flags |= wxAUI_MGR_NB_RIGHT;
    }
    else
    {
        flags |= wxAUI_MGR_NB_TOP;
    }

    if(HasFlag(wxAUI_MGR_NB_WINDOWLIST_BUTTON))
    {
        flags |= wxAUI_MGR_NB_WINDOWLIST_BUTTON;
    }

    if(HasFlag(wxAUI_MGR_NB_SCROLL_BUTTONS))
    {
        flags |= wxAUI_MGR_NB_SCROLL_BUTTONS;
    }

    if(HasFlag(wxAUI_MGR_NB_TAB_FIXED_WIDTH))
    {
        flags |= wxAUI_MGR_NB_TAB_FIXED_WIDTH;
    }


    if(HasFlag(wxAUI_MGR_NB_CLOSE_BUTTON))
    {
        flags |= wxAUI_MGR_NB_CLOSE_BUTTON;
    }
    else if(HasFlag(wxAUI_MGR_NB_CLOSE_ON_ACTIVE_TAB))
    {
        flags |= wxAUI_MGR_NB_CLOSE_ON_ACTIVE_TAB;
    }
    else if(HasFlag(wxAUI_MGR_NB_CLOSE_ON_ALL_TABS))
    {
        flags |= wxAUI_MGR_NB_CLOSE_ON_ALL_TABS;
    }

    return flags;
}


void wxAuiManager::LayoutAddNotebook(wxAuiTabArt* tabArt,wxAuiTabContainer* notebookContainer,wxSizer* notebookSizer,wxAuiDockUIPart& part,wxAuiDockInfo& dock,wxAuiDockUIPartArray& uiparts,wxAuiTabContainerPointerArray& tabContainerRecalcList,wxAuiSizerItemPointerArray& tabContainerRecalcSizers,wxAuiPaneInfo* pane,int orient)
{
    wxSizerItem* sizerItem;

    wxSize tabSize = tabArt->GetBestTabSize(m_frame, notebookContainer->GetPages(), wxDefaultSize);

    if(orient==wxHORIZONTAL)
    {
        sizerItem = notebookSizer->Add(m_art->GetMetric(wxAUI_DOCKART_SASH_SIZE), tabSize.y, 0, wxEXPAND);
    }
    else
    {
        sizerItem = notebookSizer->Add(tabSize.x, m_art->GetMetric(wxAUI_DOCKART_SASH_SIZE), 0, wxEXPAND);
    }

    tabContainerRecalcList.Add(notebookContainer);
    tabContainerRecalcSizers.Add(sizerItem);

    part.type = wxAuiDockUIPart::typePaneTab;
    part.dock = &dock;
    part.pane = pane;
    part.button = NULL;
    part.orientation = orient;
    part.cont_sizer = notebookSizer;
    part.sizer_item = sizerItem;
    part.m_tab_container = notebookContainer;
    uiparts.Add(part);
}

// This method is used to find out if it is allowed to set a pane as a new tab in a notebook
// The default behaviour of this method is set so it's backward compatible with wxWidgets 2.8 to 3.0
// It sends a EVT_AUI_PANE_DOCK_OVER that may be Vetoed() / Allowed() to override the default behaviour.
bool wxAuiManager::CanDockOver(wxAuiPaneInfo const &pane, wxAuiPaneInfo const &covered_pane)
{
    bool can_dock_over = false;

    // Basically, we can create a tab if a pane exactly overlaps another one
    if ( pane.GetPosition() == covered_pane.GetPosition() )
    {
        wxAuiPaneInfo evt_pane(pane); // we use a copy of the pane to forbid accidentally modifying the underlying one from "user space"
        wxAuiPaneInfo evt_covered_pane(covered_pane);

        wxAuiManagerEvent evt(wxEVT_AUI_PANE_DOCK_OVER);
        evt.SetManager(this);
        evt.SetPane(&evt_pane);
        evt.SetTargetPane(&evt_covered_pane);
        evt.SetCanVeto(true);
        evt.Veto( !HasFlag(wxAUI_MGR_NB_ALLOW_NOTEBOOKS) );

        GetManagedWindow()->ProcessWindowEvent(evt);

        can_dock_over = evt.IsAllowed();

    }

    return can_dock_over;
}

// This method tells if a pane must be set into a notebook, even if it's alone
bool wxAuiManager::MustDockInNotebook(const wxAuiPaneInfo &pane) const
{
    return !(pane.IsFloating() || pane.IsMaximized()) && pane.IsAlwaysDockInNotebook();
}


void wxAuiManager::LayoutAddDock(wxSizer* cont,
                                 wxAuiDockInfo& dock,
                                 wxAuiDockUIPartArray& uiparts,
                                 bool spacer_only)
{
    wxSizerItem* sizer_item;
    wxAuiDockUIPart part;

    int sashSize = m_art->GetMetric(wxAUI_DOCKART_SASH_SIZE);
    int orientation = dock.IsHorizontal() ? wxHORIZONTAL : wxVERTICAL;

    // resizable bottom and right docks have a sash before them
    if (!m_hasMaximized && !dock.fixed && (dock.dock_direction == wxAUI_DOCK_BOTTOM ||
                        dock.dock_direction == wxAUI_DOCK_RIGHT))
    {
        sizer_item = cont->Add(sashSize, sashSize, 0, wxEXPAND);

        part.type = wxAuiDockUIPart::typeDockSizer;
        part.orientation = orientation;
        part.dock = &dock;
        part.pane = NULL;
        part.button = NULL;
        part.cont_sizer = cont;
        part.sizer_item = sizer_item;
        part.m_tab_container = NULL;
        uiparts.Add(part);
    }

    // create the sizer for the dock
    wxSizer* dock_sizer = new wxBoxSizer(orientation);

    // add each pane to the dock
    bool has_maximized_pane = false;
    int pane_i, pane_count = dock.panes.GetCount();

    //For left/right notebooks (with horizontal tabs) we need to first know all tabs in notebook before we can calculate the width of the tab sizer, so we store them and then do the calculation at the end
    wxAuiTabContainerPointerArray tabContainerRecalcList;
    wxAuiSizerItemPointerArray tabContainerRecalcSizers;

    if (dock.fixed)
    {
        wxArrayInt pane_positions, pane_sizes;

        // figure out the real pane positions we will
        // use, without modifying the each pane's pane_pos member
        GetPanePositionsAndSizes(dock, pane_positions, pane_sizes);

        int offset = 0;

        // Variables to keep track of the first page of a notebook we encounter so that we can add further panes
        // to the same notebook.
        wxAuiPaneInfo* firstPaneInNotebook = NULL;
        wxAuiTabContainer* notebookContainer = NULL;
        wxSizer* notebookSizer = NULL;
        // Variable to keep track of whether an active page has been set for the current notebook, so that if we pass
        // an entire notebook without encountering an active page we can set the first page as the active one.
        bool activenotebookpagefound = false;

        for (pane_i = 0; pane_i < pane_count; ++pane_i)
        {
            wxAuiPaneInfo& pane = *(dock.panes.Item(pane_i));
            int pane_pos = pane_positions.Item(pane_i);

            if (pane.IsMaximized())
                has_maximized_pane = true;

            if (firstPaneInNotebook && CanDockOver(pane, *firstPaneInNotebook))
            {
                // This page is part of an existing notebook so add it to the container.
                // If it is the active page then it is visible, otherwise hide it.
                notebookContainer->AddPage(pane);
                if(pane.HasFlag(wxAuiPaneInfo::optionActiveNotebook))
                {
                    if(!activenotebookpagefound)
                    {
                        // Don't ever hide or show a window during hint calculation as this can affect display of windows other than the hint one.
                        if(!m_doingHintCalculation)
                            ShowWnd(pane.GetWindow(),true);
                        activenotebookpagefound = true;
                    }
                    else
                    {
                        // Only hide the window if it belongs to us.
                        // It might not belong to us if we are in the middle of a drop calculation for
                        // a floating frame, hiding it in this case would make the floating frame blank.
                        if(pane.GetWindow()->GetParent()==m_frame)
                        {
                            // Don't ever hide or show a window during hint calculation as this can affect display of windows other than the hint one.
                            if(!m_doingHintCalculation)
                                ShowWnd(pane.GetWindow(),false);

                            // We have something inconsistent: 2 active pages in the same notebook
                            // we just clear the flag to avoid having strange layouts
                            pane.SetFlag(wxAuiPaneInfo::optionActiveNotebook, false);
                        }
                        // Add a debug warning?
                    }
                }
                else
                {
                    // Only hide the window if it belongs to us.
                    // It might not belong to us if we are in the middle of a drop calculation for
                    // a floating frame, hiding it in this case would make the floating frame blank.
                    if(pane.GetWindow()->GetParent()==m_frame)
                    {
                        // Don't ever hide or show a window during hint calculation as this can affect display of windows other than the hint one.
                        if(!m_doingHintCalculation)
                            ShowWnd(pane.GetWindow(),false);
                    }
                }

                // If we are only doing a drop calculation then we only want the first pane in the notebook to be added to the sizer.
                // This is because we are essentially ignoring hidden/visible state - so adding more than one pane means that the space gets divided up evenly between pages by the sizer.
                // We don't want this because the hint rect should take up the whole sizer.
                // The hint code needs to find a pane named __HINT__ in our notebook to know we contain the hint so ensure the first pane has __HINT__ set as the name.
                if(m_doingHintCalculation&&pane.GetName()==L"__HINT__")
                {
                    if(part.pane)
                    {
                        part.pane->Name(L"__HINT__");
                    }
                }
                if(spacer_only)
                    continue;
            }
            else
            {
                if (firstPaneInNotebook)
                {
                    // Right/Bottom notebook
                    if(HasFlag(wxAUI_MGR_NB_RIGHT))
                    {
                        LayoutAddNotebook(m_tab_art,notebookContainer,notebookSizer,part,dock,uiparts,tabContainerRecalcList,tabContainerRecalcSizers,firstPaneInNotebook,wxVERTICAL);
                    }
                    else if(HasFlag(wxAUI_MGR_NB_BOTTOM))
                    {
                        LayoutAddNotebook(m_tab_art,notebookContainer,notebookSizer,part,dock,uiparts,tabContainerRecalcList,tabContainerRecalcSizers,firstPaneInNotebook,wxHORIZONTAL);
                    }

                    if(!activenotebookpagefound)
                    {
                        // The previous page was a notebook that did not have an active page and we are not part of,
                        // set the first page in that notebook to be the active page.
                        // Don't ever hide or show a window during hint calculation as this can affect display of windows other than the hint one.
                        if(!m_doingHintCalculation)
                        {
                            firstPaneInNotebook->SetFlag(wxAuiPaneInfo::optionActiveNotebook,true);
                            ShowWnd(firstPaneInNotebook->GetWindow(),true);
                            activenotebookpagefound=true;
                        }
                    }
                }

                // If the next pane has the same position as us then we are the first page in a notebook.
                // Create a new notebook container and add it as a part.
                if( MustDockInNotebook(pane) || (pane_i<pane_count-1 && CanDockOver(*dock.panes.Item(pane_i+1), pane)) )
                {
                    firstPaneInNotebook = &pane;

                    notebookContainer =  new wxAuiTabContainer(m_tab_art,this);
                    // Left/Right notebook
                    if(HasFlag(wxAUI_MGR_NB_LEFT)||HasFlag(wxAUI_MGR_NB_RIGHT))
                    {
                        notebookSizer = new wxBoxSizer(wxHORIZONTAL);
                    }
                    else
                    {
                        notebookSizer = new wxBoxSizer(wxVERTICAL);
                    }
                    notebookSizer = new wxBoxSizer(wxVERTICAL);
                    dock_sizer->Add(notebookSizer, pane.GetProportion(), wxEXPAND);
                    int flags = GetNotebookFlags();
                    notebookContainer->SetFlags(flags);
                    notebookContainer->AddPage(pane);

                    // Top/Left Notebook
                    if(HasFlag(wxAUI_MGR_NB_LEFT))
                    {
                        LayoutAddNotebook(m_tab_art,notebookContainer,notebookSizer,part,dock,uiparts,tabContainerRecalcList,tabContainerRecalcSizers,firstPaneInNotebook,wxVERTICAL);
                    }
                    else if(!HasFlag(wxAUI_MGR_NB_RIGHT)&&!HasFlag(wxAUI_MGR_NB_BOTTOM))
                    {
                        LayoutAddNotebook(m_tab_art,notebookContainer,notebookSizer,part,dock,uiparts,tabContainerRecalcList,tabContainerRecalcSizers,firstPaneInNotebook,wxHORIZONTAL);
                    }

                    if(pane.HasFlag(wxAuiPaneInfo::optionActiveNotebook))
                    {
                        // Don't ever hide or show a window during hint calculation as this can affect display of windows other than the hint one.
                        if(!m_doingHintCalculation)
                            ShowWnd(pane.GetWindow(),true);
                        activenotebookpagefound = true;
                    }
                    else
                    {
                        activenotebookpagefound = false;
                        // Only hide the window if it belongs to us.
                        // It might not belong to us if we are in the middle of a drop calculation for
                        // a floating frame, hiding it in this case would make the floating frame blank.
                        if(pane.GetWindow()->GetParent()==m_frame)
                        {
                            // Don't ever hide or show a window during hint calculation as this can affect display of windows other than the hint one.
                            if(!m_doingHintCalculation)
                                ShowWnd(pane.GetWindow(),false);
                        }
                    }
                }
                else
                {
                    // We are a normal pane not part of a notebook so set the notebook tracking variables to NULL.
                    firstPaneInNotebook = NULL;
                    notebookContainer = NULL;
                    notebookSizer = NULL;

                    // Show the pane if necessary (all invisible panes have already been hidden)
                    if (!m_doingHintCalculation && pane.IsShown() && !pane.GetWindow()->IsShown())
                        ShowWnd(pane.GetWindow(), true);
                }
            }

            int amount = pane_pos - offset;
            if (amount > 0)
            {
                if (dock.IsVertical())
                    sizer_item = dock_sizer->Add(1, amount, 0, wxEXPAND);
                else
                    sizer_item = dock_sizer->Add(amount, 1, 0, wxEXPAND);

                part.type = wxAuiDockUIPart::typeBackground;
                part.dock = &dock;
                part.pane = NULL;
                part.button = NULL;
                part.orientation = (orientation==wxHORIZONTAL) ? wxVERTICAL:wxHORIZONTAL;
                part.cont_sizer = dock_sizer;
                part.sizer_item = sizer_item;
                part.m_tab_container = NULL;
                uiparts.Add(part);

                offset += amount;
            }

            LayoutAddPane(notebookSizer?notebookSizer:dock_sizer, dock, pane, uiparts, spacer_only, firstPaneInNotebook==NULL);

            offset += pane_sizes.Item(pane_i);
        }
        // Done adding panes, if the last pane we added was part of a notebook which didn't have an active page set
        // then set the first pange in that notebook to be the active page.
        if(firstPaneInNotebook)
        {
            // Right/Bottom notebook
            if(HasFlag(wxAUI_MGR_NB_RIGHT))
            {
                LayoutAddNotebook(m_tab_art,notebookContainer,notebookSizer,part,dock,uiparts,tabContainerRecalcList,tabContainerRecalcSizers,firstPaneInNotebook,wxVERTICAL);
            }
            else if(HasFlag(wxAUI_MGR_NB_BOTTOM))
            {
                LayoutAddNotebook(m_tab_art,notebookContainer,notebookSizer,part,dock,uiparts,tabContainerRecalcList,tabContainerRecalcSizers,firstPaneInNotebook,wxHORIZONTAL);
            }


            if(!activenotebookpagefound && firstPaneInNotebook)
            {
                // Don't ever hide or show a window during hint calculation as this can affect display of windows other than the hint one.
                if(!m_doingHintCalculation)
                {
                    firstPaneInNotebook->SetFlag(wxAuiPaneInfo::optionActiveNotebook,true);
                    ShowWnd(firstPaneInNotebook->GetWindow(),true);
                    activenotebookpagefound=true;
                }
            }
        }

        // at the end add a very small stretchable background area
        sizer_item = dock_sizer->Add(0,0, 1, wxEXPAND);

        part.type = wxAuiDockUIPart::typeBackground;
        part.dock = &dock;
        part.pane = NULL;
        part.button = NULL;
        part.orientation = orientation;
        part.cont_sizer = dock_sizer;
        part.sizer_item = sizer_item;
        part.m_tab_container = NULL;
        uiparts.Add(part);
    }
    else
    {
        // Variables to keep track of the first page of a notebook we encounter so that we can add further panes
        // to the same notebook.
        wxAuiPaneInfo* firstPaneInNotebook = NULL;
        wxAuiTabContainer* notebookContainer = NULL;
        wxSizer* notebookSizer = NULL;
        // Variable to keep track of whether an active page has been set for the current notebook, so that if we pass
        // an entire notebook without encountering an active page we can set the first page as the active one.
        bool activenotebookpagefound = false;

        for (pane_i = 0; pane_i < pane_count; ++pane_i)
        {
            wxAuiPaneInfo& pane = *(dock.panes.Item(pane_i));

            if (pane.IsMaximized())
                has_maximized_pane = true;

            if(firstPaneInNotebook && CanDockOver(pane, *firstPaneInNotebook))
            {
                // This page is part of an existing notebook so add it to the container.
                // If it is the active page then it is visible, otherwise hide it.
                notebookContainer->AddPage(pane);

                if(pane.HasFlag(wxAuiPaneInfo::optionActiveNotebook))
                {
                    if(!activenotebookpagefound)
                    {
                        // Don't ever hide or show a window during hint calculation as this can affect display of windows other than the hint one.
                        if(!m_doingHintCalculation)
                            ShowWnd(pane.GetWindow(),true);
                        activenotebookpagefound = true;
                    }
                    //else
                        // Add a debug warning
                }
                else
                {
                    // Only hide the window if it belongs to us.
                    // It might not belong to us if we are in the middle of a drop calculation for
                    // a floating frame, hiding it in this case would make the floating frame blank.
                    if(pane.GetWindow()->GetParent()==m_frame)
                    {
                        // Don't ever hide or show a window during hint calculation as this can affect display of windows other than the hint one.
                        if(!m_doingHintCalculation)
                            ShowWnd(pane.GetWindow(),false);
                    }
                }

                // If we are only doing a drop calculation then we only want the first pane in the notebook to be added to the sizer.
                // This is because we are essentially ignoring hidden/visible state - so adding more than one pane means that the space gets divided up evenly between pages by the sizer.
                // We don't want this because the hint rect should take up the whole sizer.
                // The hint code needs to find a pane named __HINT__ in our notebook to know we contain the hint so ensure the first pane has __HINT__ set as the name.
                if(m_doingHintCalculation&&pane.GetName()==L"__HINT__")
                {
                    if(part.pane)
                    {
                        part.pane->Name(L"__HINT__");
                    }
                }
                if(spacer_only)
                    continue;
            }
            else
            {
                if(firstPaneInNotebook)
                {
                    // Right/Bottom notebook
                    if(HasFlag(wxAUI_MGR_NB_RIGHT))
                    {
                        LayoutAddNotebook(m_tab_art,notebookContainer,notebookSizer,part,dock,uiparts,tabContainerRecalcList,tabContainerRecalcSizers,firstPaneInNotebook,wxVERTICAL);
                    }
                    else if(HasFlag(wxAUI_MGR_NB_BOTTOM))
                    {
                        LayoutAddNotebook(m_tab_art,notebookContainer,notebookSizer,part,dock,uiparts,tabContainerRecalcList,tabContainerRecalcSizers,firstPaneInNotebook,wxHORIZONTAL);
                    }

                    if(!activenotebookpagefound)
                    {
                        // The previous page was a notebook that did not have an active page and we are not part of,
                        // set the first page in that notebook to be the active page.
                        // Don't ever hide or show a window during hint calculation as this can affect display of windows other than the hint one.
                        if(!m_doingHintCalculation)
                        {
                            firstPaneInNotebook->SetFlag(wxAuiPaneInfo::optionActiveNotebook,true);
                            ShowWnd(firstPaneInNotebook->GetWindow(),true);
                            activenotebookpagefound=true;
                        }
                    }
                }

                // if this is not the first pane being added,
                // we need to add a pane sizer
                if (!m_hasMaximized && pane_i > 0)
                {
                    sizer_item = dock_sizer->Add(sashSize, sashSize, 0, wxEXPAND);

                    part.type = wxAuiDockUIPart::typePaneSizer;
                    part.dock = &dock;
                    part.pane = dock.panes.Item(pane_i-1);
                    part.button = NULL;
                    part.orientation = (orientation==wxHORIZONTAL) ? wxVERTICAL:wxHORIZONTAL;
                    part.cont_sizer = dock_sizer;
                    part.sizer_item = sizer_item;
                    part.m_tab_container = NULL;
                    uiparts.Add(part);
                }

                // If the next pane has the same position as us then we are the first page in a notebook.
                // Create a new notebook container and add it as a part.
                if(MustDockInNotebook(pane) || (pane_i<pane_count-1 && CanDockOver(*dock.panes.Item(pane_i+1), pane)) )
                {
                    firstPaneInNotebook = &pane;
                    notebookContainer =  new wxAuiTabContainer(m_tab_art,this);
                    // Left/Right notebook
                    if(HasFlag(wxAUI_MGR_NB_LEFT)||HasFlag(wxAUI_MGR_NB_RIGHT))
                    {
                        notebookSizer = new wxBoxSizer(wxHORIZONTAL);
                    }
                    else
                    {
                        notebookSizer = new wxBoxSizer(wxVERTICAL);
                    }
                    dock_sizer->Add(notebookSizer, pane.GetProportion(), wxEXPAND);
                    int flags= GetNotebookFlags();
                    notebookContainer->SetFlags(flags);
                    notebookContainer->AddPage(pane);

                    // Left/Top Notebook
                    if(HasFlag(wxAUI_MGR_NB_LEFT))
                    {
                        LayoutAddNotebook(m_tab_art,notebookContainer,notebookSizer,part,dock,uiparts,tabContainerRecalcList,tabContainerRecalcSizers,firstPaneInNotebook,wxVERTICAL);
                    }
                    else if(!HasFlag(wxAUI_MGR_NB_RIGHT)&&!HasFlag(wxAUI_MGR_NB_BOTTOM))
                    {
                        LayoutAddNotebook(m_tab_art,notebookContainer,notebookSizer,part,dock,uiparts,tabContainerRecalcList,tabContainerRecalcSizers,firstPaneInNotebook,wxHORIZONTAL);
                    }

                    if(pane.HasFlag(wxAuiPaneInfo::optionActiveNotebook))
                    {
                        // Don't ever hide or show a window during hint calculation as this can affect display of windows other than the hint one.
                        if(!m_doingHintCalculation)
                            ShowWnd(pane.GetWindow(),true);
                        activenotebookpagefound = true;
                    }
                    else
                    {
                        activenotebookpagefound = false;
                        // Only hide the window if it belongs to us.
                        // It might not belong to us if we are in the middle of a drop calculation for
                        // a floating frame, hiding it in this case would make the floating frame blank.
                        if(pane.GetWindow()->GetParent()==m_frame)
                        {
                            // Don't ever hide or show a window during hint calculation as this can affect display of windows other than the hint one.
                            if(!m_doingHintCalculation)
                                ShowWnd(pane.GetWindow(),false);
                        }
                    }
                }
                else
                {
                    // We are a normal pane not part of a notebook so set the notebook tracking variables to NULL.
                    firstPaneInNotebook = NULL;
                    notebookContainer = NULL;
                    notebookSizer = NULL;

                    // Show the pane if necessary (all invisible panes have already been hidden)
                    if (!m_doingHintCalculation && pane.IsShown() && !pane.GetWindow()->IsShown())
                        ShowWnd(pane.GetWindow(), true);

                }
            }

            // Add the pane itself to the layout, if it is part of a notebook then don't allow it to have a title bar.
            LayoutAddPane(notebookSizer?notebookSizer:dock_sizer, dock, pane, uiparts, spacer_only, firstPaneInNotebook==NULL);
        }
        // Done adding panes, if the last pane we added was part of a notebook which didn't have an active page set
        // then set the first pange in that notebook to be the active page.
        if(firstPaneInNotebook)
        {
            // Right/Bottom notebook
            if(HasFlag(wxAUI_MGR_NB_RIGHT))
            {
                LayoutAddNotebook(m_tab_art,notebookContainer,notebookSizer,part,dock,uiparts,tabContainerRecalcList,tabContainerRecalcSizers,firstPaneInNotebook,wxVERTICAL);
            }
            else if(HasFlag(wxAUI_MGR_NB_BOTTOM))
            {
                LayoutAddNotebook(m_tab_art,notebookContainer,notebookSizer,part,dock,uiparts,tabContainerRecalcList,tabContainerRecalcSizers,firstPaneInNotebook,wxHORIZONTAL);
            }


            if(!activenotebookpagefound)
            {
                // The previous page was a notebook that did not have an active page and we are not part of,
                // set the first page in that notebook to be the active page.
                // Don't ever hide or show a window during hint calculation as this can affect display of windows other than the hint one.
                if(!m_doingHintCalculation)
                {
                    firstPaneInNotebook->SetFlag(wxAuiPaneInfo::optionActiveNotebook,true);
                    ShowWnd(firstPaneInNotebook->GetWindow(),true);
                    activenotebookpagefound=true;
                }
            }
        }
    }

    unsigned int recalcTabSizeIndex;
    for(recalcTabSizeIndex=0; recalcTabSizeIndex < tabContainerRecalcList.size(); recalcTabSizeIndex++)
    {
        wxSize tabSize = m_tab_art->GetBestTabSize(m_frame, tabContainerRecalcList[recalcTabSizeIndex]->GetPages(), wxDefaultSize);
        tabContainerRecalcSizers[recalcTabSizeIndex]->SetMinSize(tabSize);
    }

    if (dock.dock_direction == wxAUI_DOCK_CENTER || has_maximized_pane)
        sizer_item = cont->Add(dock_sizer, 1, wxEXPAND);
    else
        sizer_item = cont->Add(dock_sizer, 0, wxEXPAND);

    part.type = wxAuiDockUIPart::typeDock;
    part.dock = &dock;
    part.pane = NULL;
    part.button = NULL;
    part.orientation = orientation;
    part.cont_sizer = cont;
    part.sizer_item = sizer_item;
    part.m_tab_container = NULL;
    uiparts.Add(part);

    if (dock.IsHorizontal())
        cont->SetItemMinSize(dock_sizer, 0, dock.size);
    else
        cont->SetItemMinSize(dock_sizer, dock.size, 0);

    //  top and left docks have a sash after them
    if (!m_hasMaximized &&
        !dock.fixed &&
          (dock.dock_direction == wxAUI_DOCK_TOP ||
           dock.dock_direction == wxAUI_DOCK_LEFT))
    {
        sizer_item = cont->Add(sashSize, sashSize, 0, wxEXPAND);

        part.type = wxAuiDockUIPart::typeDockSizer;
        part.dock = &dock;
        part.pane = NULL;
        part.button = NULL;
        part.orientation = orientation;
        part.cont_sizer = cont;
        part.sizer_item = sizer_item;
        part.m_tab_container = NULL;
        uiparts.Add(part);
    }
}

wxSizer* wxAuiManager::LayoutAll(wxAuiPaneInfoArray& panes,
                                 wxAuiDockInfoArray& docks,
                                 wxAuiDockUIPartArray& uiparts,
                                 bool spacer_only)
{
    wxBoxSizer* container = new wxBoxSizer(wxVERTICAL);

    int pane_borderSize = m_art->GetMetric(wxAUI_DOCKART_PANE_BORDER_SIZE);
    int caption_size = m_art->GetMetric(wxAUI_DOCKART_CAPTION_SIZE);
    wxSize cli_size = m_frame->GetClientSize();
    int i, dock_count, pane_count;


    // empty all docks out
    for (i = 0, dock_count = docks.GetCount(); i < dock_count; ++i)
    {
        wxAuiDockInfo& dock = docks.Item(i);

        // empty out all panes, as they will be readded below
        dock.panes.Empty();

        if (dock.fixed)
        {
            // always reset fixed docks' sizes, because
            // the contained windows may have been resized
            dock.size = 0;
        }
    }


    // iterate through all known panes, filing each
    // of them into the appropriate dock. If the
    // pane does not exist in the dock, add it
    for (i = 0, pane_count = panes.GetCount(); i < pane_count; ++i)
    {
        wxAuiPaneInfo& p = panes.Item(i);

        // find any docks with the same dock direction, dock layer, and
        // dock row as the pane we are working on
        wxAuiDockInfo* dock;
        wxAuiDockInfoPtrArray arr;
        FindDocks(docks, p.dock_direction, p.dock_layer, p.dock_row, arr);

        if (arr.GetCount() > 0)
        {
            // found the right dock
            dock = arr.Item(0);
        }
        else
        {
            // dock was not found, so we need to create a new one
            wxAuiDockInfo d;
            d.dock_direction = p.dock_direction;
            d.dock_layer = p.dock_layer;
            d.dock_row = p.dock_row;
            docks.Add(d);
            dock = &docks.Last();
        }


        if (p.IsDocked() && p.IsShown())
        {
            // remove the pane from any existing docks except this one
            RemovePaneFromDocks(docks, p, dock);

            // pane needs to be added to the dock,
            // if it doesn't already exist
            if (!FindPaneInDock(*dock, p.window))
                dock->panes.Add(&p);
        }
        else
        {
            // remove the pane from any existing docks
            RemovePaneFromDocks(docks, p);
        }

    }

    // remove any empty docks
    for (i = docks.GetCount()-1; i >= 0; --i)
    {
        if (docks.Item(i).panes.GetCount() == 0)
            docks.RemoveAt(i);
    }

    // configure the docks further
    for (i = 0, dock_count = docks.GetCount(); i < dock_count; ++i)
    {
        wxAuiDockInfo& dock = docks.Item(i);
        int j, dock_pane_count = dock.panes.GetCount();

        // sort the dock pane array by the pane's
        // dock position (dock_pos), in ascending order
        dock.panes.Sort(DockPaneSortFunc);

        // for newly created docks, set up their initial size
        if (dock.size == 0)
        {
            int size = 0;

            for (j = 0; j < dock_pane_count; ++j)
            {
                wxAuiPaneInfo& pane = *dock.panes.Item(j);
                wxSize pane_size = pane.best_size;
                if (pane_size == wxDefaultSize)
                    pane_size = pane.min_size;
                if (pane_size == wxDefaultSize)
                    pane_size = pane.window->GetSize();

                if (dock.IsHorizontal())
                    size = wxMax(pane_size.y, size);
                else
                    size = wxMax(pane_size.x, size);
            }

            // add space for the border (two times), but only
            // if at least one pane inside the dock has a pane border
            for (j = 0; j < dock_pane_count; ++j)
            {
                if (dock.panes.Item(j)->HasBorder())
                {
                    size += (pane_borderSize*2);
                    break;
                }
            }

            // if pane is on the top or bottom, add the caption height,
            // but only if at least one pane inside the dock has a caption
            if (dock.IsHorizontal())
            {
                for (j = 0; j < dock_pane_count; ++j)
                {
                    if (dock.panes.Item(j)->HasCaption())
                    {
                        size += caption_size;
                        break;
                    }
                }
            }


            // new dock's size may not be more than the dock constraint
            // parameter specifies.  See SetDockSizeConstraint()

            int max_dock_x_size = (int)(m_dockConstraintX * ((double)cli_size.x));
            int max_dock_y_size = (int)(m_dockConstraintY * ((double)cli_size.y));

            if (dock.IsHorizontal())
                size = wxMin(size, max_dock_y_size);
            else
                size = wxMin(size, max_dock_x_size);

            // absolute minimum size for a dock is 10 pixels
            if (size < 10)
                size = 10;

            dock.size = size;
        }


        // determine the dock's minimum size
        bool plus_border = false;
        bool plus_caption = false;
        int dock_min_size = 0;
        for (j = 0; j < dock_pane_count; ++j)
        {
            wxAuiPaneInfo& pane = *dock.panes.Item(j);
            if (pane.min_size != wxDefaultSize)
            {
                if (pane.HasBorder())
                    plus_border = true;
                if (pane.HasCaption())
                    plus_caption = true;
                if (dock.IsHorizontal())
                {
                    if (pane.min_size.y > dock_min_size)
                        dock_min_size = pane.min_size.y;
                }
                else
                {
                    if (pane.min_size.x > dock_min_size)
                        dock_min_size = pane.min_size.x;
                }
            }
        }

        if (plus_border)
            dock_min_size += (pane_borderSize*2);
        if (plus_caption && dock.IsHorizontal())
            dock_min_size += (caption_size);

        dock.min_size = dock_min_size;


        // if the pane's current size is less than its
        // minimum, increase the dock's size to its minimum
        if (dock.size < dock.min_size)
            dock.size = dock.min_size;


        // determine the dock's mode (fixed or proportional);
        // determine whether the dock has only toolbars
        bool action_pane_marked = false;
        dock.fixed = true;
        dock.toolbar = true;
        for (j = 0; j < dock_pane_count; ++j)
        {
            wxAuiPaneInfo& pane = *dock.panes.Item(j);
            if (!pane.IsFixed())
                dock.fixed = false;
            if (!pane.IsToolbar())
                dock.toolbar = false;
            if (pane.HasFlag(wxAuiPaneInfo::optionDockFixed))
                dock.fixed = true;
            if (pane.HasFlag(wxAuiPaneInfo::actionPane))
                action_pane_marked = true;
        }


        // if the dock mode is proportional and not fixed-pixel,
        // reassign the dock_pos to the sequential 0, 1, 2, 3;
        // e.g. remove gaps like 1, 2, 30, 500
        // If panes currently share a position they must continue to do so
        // e.g. 1, 5, 5, 9 -> 0, 1, 1, 2
        if (!dock.fixed)
        {
            int i = -1;
            int lastposition = -1;
            for (j = 0; j < dock_pane_count; ++j)
            {
                wxAuiPaneInfo& pane = *dock.panes.Item(j);
                if(pane.GetPosition()!=lastposition)
                    ++i;
                lastposition=pane.GetPosition();
                pane.Position(i);
            }
        }

        // if the dock mode is fixed, and none of the panes
        // are being moved right now, make sure the panes
        // do not overlap each other.  If they do, we will
        // adjust the positions of the panes
        // If panes share an identical position they must continue to do so as they are then part of a notebook,
        // excepted if they are toolbars (toolbars cannot be part of notebooks)
        if (dock.fixed && !action_pane_marked)
        {
            wxArrayInt pane_positions, pane_sizes;
            GetPanePositionsAndSizes(dock, pane_positions, pane_sizes);

            int offset = 0;
            int lastposition=-1;
            for (j = 0; j < dock_pane_count; ++j)
            {
                wxAuiPaneInfo& pane = *(dock.panes.Item(j));
                if(!pane.IsToolbar() && pane.GetPosition()==lastposition)
                {
                    pane.Position(dock.panes.Item(j-1)->GetPosition());
                    continue;
                }
                lastposition = pane.GetPosition();
                pane.Position(pane_positions[j]);

                int amount = pane.dock_pos - offset;
                if (amount >= 0)
                    offset += amount;
                else
                    pane.dock_pos += -amount;

                offset += pane_sizes[j];
            }
        }
    }

    // discover the maximum dock layer
    int max_layer = 0;
    for (i = 0; i < dock_count; ++i)
        max_layer = wxMax(max_layer, docks.Item(i).dock_layer);


    // clear out uiparts
    uiparts.Empty();

    // create a bunch of box sizers,
    // from the innermost level outwards.
    wxSizer* cont = NULL;
    wxSizer* middle = NULL;
    int layer = 0;
    int row, row_count;

    for (layer = 0; layer <= max_layer; ++layer)
    {
        wxAuiDockInfoPtrArray arr;

        // find any docks in this layer
        FindDocks(docks, -1, layer, -1, arr);

        // if there aren't any, skip to the next layer
        if (arr.IsEmpty())
            continue;

        wxSizer* old_cont = cont;

        // create a container which will hold this layer's
        // docks (top, bottom, left, right)
        cont = new wxBoxSizer(wxVERTICAL);


        // find any top docks in this layer
        FindDocks(docks, wxAUI_DOCK_TOP, layer, -1, arr);
        if (!arr.IsEmpty())
        {
            for (row = 0, row_count = arr.GetCount(); row < row_count; ++row)
                LayoutAddDock(cont, *arr.Item(row), uiparts, spacer_only);
        }


        // fill out the middle layer (which consists
        // of left docks, content area and right docks)

        middle = new wxBoxSizer(wxHORIZONTAL);

        // find any left docks in this layer
        FindDocks(docks, wxAUI_DOCK_LEFT, layer, -1, arr);
        if (!arr.IsEmpty())
        {
            for (row = 0, row_count = arr.GetCount(); row < row_count; ++row)
                LayoutAddDock(middle, *arr.Item(row), uiparts, spacer_only);
        }

        // add content dock (or previous layer's sizer
        // to the middle
        if (!old_cont)
        {
            // find any center docks
            FindDocks(docks, wxAUI_DOCK_CENTER, -1, -1, arr);
            if (!arr.IsEmpty())
            {
                for (row = 0,row_count = arr.GetCount(); row<row_count; ++row)
                   LayoutAddDock(middle, *arr.Item(row), uiparts, spacer_only);
            }
            else if (!m_hasMaximized)
            {
                // there are no center docks, add a background area
                wxSizerItem* sizer_item = middle->Add(1,1, 1, wxEXPAND);
                wxAuiDockUIPart part;
                part.type = wxAuiDockUIPart::typeBackground;
                part.pane = NULL;
                part.dock = NULL;
                part.button = NULL;
                part.cont_sizer = middle;
                part.sizer_item = sizer_item;
                part.m_tab_container = NULL;
                uiparts.Add(part);
            }
        }
        else
        {
            middle->Add(old_cont, 1, wxEXPAND);
        }

        // find any right docks in this layer
        FindDocks(docks, wxAUI_DOCK_RIGHT, layer, -1, arr);
        if (!arr.IsEmpty())
        {
            for (row = arr.GetCount()-1; row >= 0; --row)
                LayoutAddDock(middle, *arr.Item(row), uiparts, spacer_only);
        }

        if (middle->GetChildren().GetCount() > 0)
            cont->Add(middle, 1, wxEXPAND);
             else
            delete middle;



        // find any bottom docks in this layer
        FindDocks(docks, wxAUI_DOCK_BOTTOM, layer, -1, arr);
        if (!arr.IsEmpty())
        {
            for (row = arr.GetCount()-1; row >= 0; --row)
                LayoutAddDock(cont, *arr.Item(row), uiparts, spacer_only);
        }

    }

    if (!cont)
    {
        // no sizer available, because there are no docks,
        // therefore we will create a simple background area
        cont = new wxBoxSizer(wxVERTICAL);
        wxSizerItem* sizer_item = cont->Add(1,1, 1, wxEXPAND);
        wxAuiDockUIPart part;
        part.type = wxAuiDockUIPart::typeBackground;
        part.pane = NULL;
        part.dock = NULL;
        part.button = NULL;
        part.cont_sizer = middle;
        part.sizer_item = sizer_item;
        part.m_tab_container = NULL;
        uiparts.Add(part);
    }

    container->Add(cont, 1, wxEXPAND);
    return container;
}


// SetDockSizeConstraint() allows the dock constraints to be set.  For example,
// specifying values of 0.5, 0.5 will mean that upon dock creation, a dock may
// not be larger than half of the window's size

void wxAuiManager::SetDockSizeConstraint(double width_pct, double height_pct)
{
    m_dockConstraintX = wxMax(0.0, wxMin(1.0, width_pct));
    m_dockConstraintY = wxMax(0.0, wxMin(1.0, height_pct));
}

void wxAuiManager::GetDockSizeConstraint(double* width_pct, double* height_pct) const
{
    if (width_pct)
        *width_pct = m_dockConstraintX;

    if (height_pct)
        *height_pct = m_dockConstraintY;
}


// Used inside Update to store the tab offset of previous notebooks so that they can be restored if a notebook
// with the same hash still exists. This prevents tab offsets from resetting every time we drag a pane for example.
WX_DECLARE_STRING_HASH_MAP( int, NotebookOffsetHash );

// Update() updates the layout.  Whenever changes are made to
// one or more panes, this function should be called.  It is the
// external entry point for running the layout engine.

void wxAuiManager::Update()
{
    m_hoverButton = NULL;
    m_actionPart = NULL;
    delete m_actionDeadZone;
    m_actionDeadZone = NULL;

    // Ensure pane array is always sorted before doing layout - otherwise we can have odd results
    m_panes.Sort(PaneSortFunc);

    wxSizer* sizer;
    int i, pane_count = m_panes.GetCount();


    // destroy floating panes which have been
    // redocked or are becoming non-floating
    for (i = 0; i < pane_count; ++i)
    {
        wxAuiPaneInfo& p = m_panes.Item(i);

        if (!p.IsFloating() && p.frame)
        {
            // because the pane is no longer in a floating, we need to
            // reparent it to m_frame and destroy the floating frame

            // reduce flicker
            p.window->SetSize(1,1);


            // the following block is a workaround for bug #1531361
            // (see wxWidgets sourceforge page).  On wxGTK (only), when
            // a frame is shown/hidden, a move event unfortunately
            // also gets fired.  Because we may be dragging around
            // a pane, we need to cancel that action here to prevent
            // a spurious crash.
            if (m_actionWindow == p.frame)
            {
                if (wxWindow::GetCapture() == m_frame)
                    m_frame->ReleaseMouse();
                m_action = actionNone;
                m_actionWindow = NULL;
            }

            // hide the frame
            if (p.frame->IsShown())
                p.frame->Show(false);

            // reparent to m_frame and destroy the pane
            if (m_actionWindow == p.frame)
            {
                m_actionWindow = NULL;
            }

            p.window->Reparent(m_frame);
            p.frame->SetSizer(NULL);
            p.frame->Destroy();
            p.frame = NULL;
        }
    }


    // delete old sizer first
    m_frame->SetSizer(NULL);


    // hide or show panes as necessary,
    // and float panes as necessary
    for (i = 0; i < pane_count; ++i)
    {
        wxAuiPaneInfo& p = m_panes.Item(i);

        if (p.IsFloating())
        {
            if (p.frame == NULL)
            {
                // we need to create a frame for this
                // pane, which has recently been floated
                wxAuiFloatingFrame* frame = CreateFloatingFrame(m_frame, p);

                // on MSW and Mac, if the owner desires transparent dragging, and
                // the dragging is happening right now, then the floating
                // window should have this style by default
                if (m_action == actionDragFloatingPane &&
                    (m_flags & wxAUI_MGR_TRANSPARENT_DRAG))
                        frame->SetTransparent(150);

                frame->SetPaneWindow(p);
                if(p.GetDirection()==wxAUI_DOCK_CENTER)
                    p.CaptionVisible(true);
                p.frame = frame;

                if (p.IsShown() && !frame->IsShown())
                    frame->Show();
            }
            else
            {
                // frame already exists, make sure its position
                // and size reflect the information in wxAuiPaneInfo
                if ((p.frame->GetPosition() != p.floating_pos) || (p.frame->GetSize() != p.floating_size))
                {
                    p.frame->SetSize(p.floating_pos.x, p.floating_pos.y,
                                     p.floating_size.x, p.floating_size.y,
                                     wxSIZE_USE_EXISTING);
                /*
                    p.frame->SetSize(p.floating_pos.x, p.floating_pos.y,
                                     wxDefaultCoord, wxDefaultCoord,
                                     wxSIZE_USE_EXISTING);
                    //p.frame->Move(p.floating_pos.x, p.floating_pos.y);
                */
                }

                if (p.GetFrame()->IsShown() != p.IsShown())
                    ShowWnd(p.GetFrame(),p.IsShown());

                // update whether the pane is resizable or not
                long style = p.frame->GetWindowStyleFlag();
                if (p.IsFixed())
                    style &= ~wxRESIZE_BORDER;
                else
                    style |= wxRESIZE_BORDER;
                p.frame->SetWindowStyleFlag(style);

                if (p.frame->GetLabel() != p.caption)
                    p.frame->SetLabel(p.caption);

            }
        }
        else
        {
            if (p.window->IsShown() != p.IsShown())
            {
                // Hide windows we are sure are hidden
                // Other panes will be processed later
                if(!p.IsShown())
                    ShowWnd(p.GetWindow(),false);

            }
        }

    }
    // We have to do the hiding and showing of panes before we call LayoutAll
    // As LayoutAll may want to hide frames even though they are technically "visible"
    // If they are in a notebook.

    // cache the offset positions for any notebooks we have, so that if we are just resizing a dock for example our notebook tabs don't jump around.
    NotebookOffsetHash cachednotebookoffsets;
    // cache the notebook that has the current focus so that it can keep the focus when doing a resize for example.
    wxString focusnotebook = wxT("");
    int uiPartsCount = m_uiParts.GetCount();
    for (i = 0; i < uiPartsCount; i++)
    {
        wxAuiDockUIPart& part = m_uiParts.Item(i);
        if(part.m_tab_container && part.m_tab_container->GetPageCount() > 0)
        {
            wxAuiPaneInfo &pane = part.m_tab_container->GetPage(0);
            wxString notebookpositionhash;
            notebookpositionhash << wxString::Format(wxT("%d;"), pane.GetDirection());
            notebookpositionhash << wxString::Format(wxT("%d;"), pane.GetPosition());
            notebookpositionhash << wxString::Format(wxT("%d;"), pane.GetRow());
            notebookpositionhash << wxString::Format(wxT("%d;"), pane.GetLayer());
            cachednotebookoffsets[notebookpositionhash] = part.m_tab_container->GetTabOffset();
            if(part.m_tab_container->HasFocus())
            {
                focusnotebook = notebookpositionhash;
            }
        }
    }


    // create a layout for all of the panes
    sizer = LayoutAll(m_panes, m_docks, m_uiParts, false);

    // restore the offset positions for any notebooks we have, so that if we are just resizing a dock for example our notebook tabs don't jump around.
    uiPartsCount = m_uiParts.GetCount();
    for (i = 0; i < uiPartsCount; i++)
    {
        wxAuiDockUIPart& part = m_uiParts.Item(i);
        if(part.m_tab_container && part.m_tab_container->GetPageCount() > 0)
        {
            wxAuiPaneInfo &pane = part.m_tab_container->GetPage(0);
            wxString notebookpositionhash;
            notebookpositionhash << wxString::Format(wxT("%d;"), pane.GetDirection());
            notebookpositionhash << wxString::Format(wxT("%d;"), pane.GetPosition());
            notebookpositionhash << wxString::Format(wxT("%d;"), pane.GetRow());
            notebookpositionhash << wxString::Format(wxT("%d;"), pane.GetLayer());
            if(cachednotebookoffsets.find(notebookpositionhash)!=cachednotebookoffsets.end())
            {
                int oldtaboffset = cachednotebookoffsets[notebookpositionhash];
                int numtabs = part.m_tab_container->GetPageCount();
                int newtaboffset;

                // If we have removed tabs then the offset might be greater then the number of tabs in which case we set it to the number of tabs
                // Otherwise we set it to the old offset
                if(oldtaboffset>numtabs)
                    newtaboffset = numtabs;
                else
                    newtaboffset = oldtaboffset;

                part.m_tab_container->SetTabOffset(newtaboffset);

                // restore the focus to the focused notebook, if there was one
                if(focusnotebook==notebookpositionhash)
                {
                    part.m_tab_container->SetFocus(true);
                }
            }
        }
    }

    // keep track of the old window rectangles so we can
    // refresh those windows whose rect has changed
    wxAuiRectArray old_pane_rects;
    for (i = 0; i < pane_count; ++i)
    {
        wxRect r;
        wxAuiPaneInfo& p = m_panes.Item(i);

        if (p.window && p.IsShown() && p.IsDocked())
            r = p.rect;

        old_pane_rects.Add(r);
    }




    // apply the new sizer
    m_frame->SetSizer(sizer);
    m_frame->SetAutoLayout(false);
    DoFrameLayout();



    // now that the frame layout is done, we need to check
    // the new pane rectangles against the old rectangles that
    // we saved a few lines above here.  If the rectangles have
    // changed, the corresponding panes must also be updated
    for (i = 0; i < pane_count; ++i)
    {
        wxAuiPaneInfo& p = m_panes.Item(i);
        if (p.window && p.window->IsShown() && p.IsDocked())
        {
            if (p.rect != old_pane_rects[i])
            {
                p.window->Refresh();
                p.window->Update();
            }
        }
    }


    Repaint();

    // set frame's minimum size

/*
    // N.B. More work needs to be done on frame minimum sizes;
    // this is some interesting code that imposes the minimum size,
    // but we may want to include a more flexible mechanism or
    // options for multiple minimum-size modes, e.g. strict or lax
    wxSize min_size = sizer->GetMinSize();
    wxSize frame_size = m_frame->GetSize();
    wxSize client_size = m_frame->GetClientSize();

    wxSize minframe_size(min_size.x+frame_size.x-client_size.x,
                         min_size.y+frame_size.y-client_size.y );

    m_frame->SetMinSize(minframe_size);

    if (frame_size.x < minframe_size.x ||
        frame_size.y < minframe_size.y)
            sizer->Fit(m_frame);
*/
}


// DoFrameLayout() is an internal function which invokes wxSizer::Layout
// on the frame's main sizer, then measures all the various UI items
// and updates their internal rectangles.  This should always be called
// instead of calling m_frame->Layout() directly

void wxAuiManager::DoFrameLayout()
{
    m_frame->Layout();

    int i, part_count;
    for (i = 0, part_count = m_uiParts.GetCount(); i < part_count; ++i)
    {
        wxAuiDockUIPart& part = m_uiParts.Item(i);

        // get the rectangle of the UI part
        // originally, this code looked like this:
        //    part.rect = wxRect(part.sizer_item->GetPosition(),
        //                       part.sizer_item->GetSize());
        // this worked quite well, with one exception: the mdi
        // client window had a "deferred" size variable
        // that returned the wrong size.  It looks like
        // a bug in wx, because the former size of the window
        // was being returned.  So, we will retrieve the part's
        // rectangle via other means


        part.rect = part.sizer_item->GetRect();
        int flag = part.sizer_item->GetFlag();
        int border = part.sizer_item->GetBorder();
        if (flag & wxTOP)
        {
            part.rect.y -= border;
            part.rect.height += border;
        }
        if (flag & wxLEFT)
        {
            part.rect.x -= border;
            part.rect.width += border;
        }
        if (flag & wxBOTTOM)
            part.rect.height += border;
        if (flag & wxRIGHT)
            part.rect.width += border;


        if (part.type == wxAuiDockUIPart::typeDock)
            part.dock->rect = part.rect;
        if (part.type == wxAuiDockUIPart::typePane)
            part.pane->rect = part.rect;
    }
}

// GetPanePart() looks up the pane the pane border UI part (or the regular
// pane part if there is no border). This allows the caller to get the exact
// rectangle of the pane in question, including decorations like
// caption and border (if any).

wxAuiDockUIPart* wxAuiManager::GetPanePart(wxWindow* wnd)
{
    int i, part_count;
    for (i = 0, part_count = m_uiParts.GetCount(); i < part_count; ++i)
    {
        wxAuiDockUIPart& part = m_uiParts.Item(i);
        if (part.type == wxAuiDockUIPart::typePaneBorder &&
            part.pane && part.pane->window == wnd)
                return &part;
    }
    for (i = 0, part_count = m_uiParts.GetCount(); i < part_count; ++i)
    {
        wxAuiDockUIPart& part = m_uiParts.Item(i);
        if (part.type == wxAuiDockUIPart::typePane &&
            part.pane && part.pane->window == wnd)
                return &part;
    }
    return NULL;
}



// GetDockPixelOffset() is an internal function which returns
// a dock's offset in pixels from the left side of the window
// (for horizontal docks) or from the top of the window (for
// vertical docks).  This value is necessary for calculating
// pixel-pane/toolbar offsets when they are dragged.

int wxAuiManager::GetDockPixelOffset(wxAuiPaneInfo& test)
{
    // the only way to accurately calculate the dock's
    // offset is to actually run a theoretical layout

    int i, part_count, dock_count;
    wxAuiDockInfoArray docks;
    wxAuiPaneInfoArray panes;
    wxAuiDockUIPartArray uiparts;
    CopyDocksAndPanes(docks, panes, m_docks, m_panes);
    panes.Add(test);

    wxSizer* sizer = LayoutAll(panes, docks, uiparts, true);
    wxSize client_size = m_frame->GetClientSize();
    sizer->SetDimension(0, 0, client_size.x, client_size.y);
    sizer->Layout();

    for (i = 0, part_count = uiparts.GetCount(); i < part_count; ++i)
    {
        wxAuiDockUIPart& part = uiparts.Item(i);
        part.rect = wxRect(part.sizer_item->GetPosition(),
                           part.sizer_item->GetSize());
        if (part.type == wxAuiDockUIPart::typeDock)
            part.dock->rect = part.rect;
    }

    delete sizer;

    for (i = 0, dock_count = docks.GetCount(); i < dock_count; ++i)
    {
        wxAuiDockInfo& dock = docks.Item(i);
        if (test.dock_direction == dock.dock_direction &&
            test.dock_layer==dock.dock_layer && test.dock_row==dock.dock_row)
        {
            if (dock.IsVertical())
                return dock.rect.y;
            else
                return dock.rect.x;
        }
    }

    return 0;
}



// ProcessDockResult() is a utility function used by DoDrop() - it checks
// if a dock operation is allowed, the new dock position is copied into
// the target info.  If the operation was allowed, the function returns true.

bool wxAuiManager::ProcessDockResult(wxAuiPaneInfo& target,
                                     const wxAuiPaneInfo& new_pos)
{
    bool allowed = false;
    switch (new_pos.dock_direction)
    {
        case wxAUI_DOCK_TOP:    allowed = target.IsTopDockable();    break;
        case wxAUI_DOCK_BOTTOM: allowed = target.IsBottomDockable(); break;
        case wxAUI_DOCK_LEFT:   allowed = target.IsLeftDockable();   break;
        case wxAUI_DOCK_RIGHT:  allowed = target.IsRightDockable();  break;
        case wxAUI_DOCK_CENTER: allowed = target.IsCenterDockable(); break;
    }

    if (!allowed && target.GetPage() != new_pos.GetPage() && HasFlag(wxAUI_MGR_NB_TAB_MOVE))
        allowed = true;

    if (allowed)
    {
        target = new_pos;
        // Should this RTTI and function call be rewritten as
        // sending a new event type to allow other window types
        // to vary size based on dock location?
        wxAuiToolBar* toolbar = wxDynamicCast(target.window, wxAuiToolBar);
        if (toolbar)
        {
            wxSize hintSize = toolbar->GetHintSize(target.dock_direction);
            if (target.best_size != hintSize)
            {
                target.best_size = hintSize;
                target.floating_size = wxDefaultSize;
            }
        }
    }

    return allowed;
}


// DoDrop() is an important function.  It basically takes a mouse position,
// and determines where the pane's new position would be.  If the pane is to be
// dropped, it performs the drop operation using the specified dock and pane
// arrays.  By specifying copied dock and pane arrays when calling, a "what-if"
// scenario can be performed, giving precise coordinates for drop hints.
// If, however, wxAuiManager:m_docks and wxAuiManager::m_panes are specified
// as parameters, the changes will be made to the main state arrays

const int auiInsertRowPixels = 10;
const int auiNewRowPixels = 40;
const int auiLayerInsertPixels = 40;
const int auiLayerInsertOffset = 5;

bool wxAuiManager::DoDrop(wxAuiDockInfoArray& docks,
                          wxAuiPaneInfoArray& panes,
                          wxAuiPaneInfo& target,
                          const wxPoint& pt,
                          const wxPoint& offset)
{
    wxSize cli_size = m_frame->GetClientSize();

    wxAuiPaneInfo drop = target;


    // The result should always be shown
    drop.Show();

    wxAuiDockUIPart* part = HitTest(pt.x, pt.y);

    // Check to see if the pane has been dragged outside of the window
    // (or near to the outside of the window), if so, dock it along the edge
    // (Don't do this if busy dragging in a notebook)
    if(!part||part->m_tab_container==NULL)
    {
        int layerInsertOffset = auiLayerInsertOffset;
        if (drop.IsToolbar())
             layerInsertOffset = 0;


        if (pt.x < layerInsertOffset && pt.x > layerInsertOffset-auiLayerInsertPixels && pt.y > 0 && pt.y < cli_size.y)
        {
            int newLayer = wxMax(wxMax(GetMaxLayer(docks, wxAUI_DOCK_LEFT), GetMaxLayer(docks, wxAUI_DOCK_BOTTOM)), GetMaxLayer(docks, wxAUI_DOCK_TOP)) + 1;

            if (drop.IsToolbar())
                newLayer = auiToolBarLayer;

            drop.Dock().Left().Layer(newLayer).Row(0).Position(pt.y - GetDockPixelOffset(drop) - offset.y);
            return ProcessDockResult(target, drop);
        }
        else if (pt.y < layerInsertOffset && pt.y > layerInsertOffset-auiLayerInsertPixels && pt.x > 0 && pt.x < cli_size.x)
        {
            int newLayer = wxMax(wxMax(GetMaxLayer(docks, wxAUI_DOCK_TOP), GetMaxLayer(docks, wxAUI_DOCK_LEFT)), GetMaxLayer(docks, wxAUI_DOCK_RIGHT)) + 1;

            if (drop.IsToolbar())
                newLayer = auiToolBarLayer;

            drop.Dock().Top().Layer(newLayer).Row(0).Position(pt.x - GetDockPixelOffset(drop) - offset.x);
            return ProcessDockResult(target, drop);
        }
        else if (pt.x >= cli_size.x - layerInsertOffset && pt.x < cli_size.x - layerInsertOffset + auiLayerInsertPixels && pt.y > 0 && pt.y < cli_size.y)
        {
            int newLayer = wxMax(wxMax(GetMaxLayer(docks, wxAUI_DOCK_RIGHT), GetMaxLayer(docks, wxAUI_DOCK_TOP)), GetMaxLayer(docks, wxAUI_DOCK_BOTTOM)) + 1;

            if (drop.IsToolbar())
                newLayer = auiToolBarLayer;

            drop.Dock().Right().Layer(newLayer).Row(0).Position(pt.y - GetDockPixelOffset(drop) - offset.y);
            return ProcessDockResult(target, drop);
        }
        else if (pt.y >= cli_size.y - layerInsertOffset && pt.y < cli_size.y - layerInsertOffset + auiLayerInsertPixels && pt.x > 0 && pt.x < cli_size.x)
        {
            int newLayer = wxMax( wxMax( GetMaxLayer(docks, wxAUI_DOCK_BOTTOM), GetMaxLayer(docks, wxAUI_DOCK_LEFT)), GetMaxLayer(docks, wxAUI_DOCK_RIGHT)) + 1;

            if (drop.IsToolbar())
                newLayer = auiToolBarLayer;

            drop.Dock().Bottom().Layer(newLayer).Row(0).Position(pt.x - GetDockPixelOffset(drop) - offset.x);
            return ProcessDockResult(target, drop);
        }

    }


    if (drop.IsToolbar())
    {
        if (!part || !part->dock)
            return false;

        // calculate the offset from where the dock begins
        // to the point where the user dropped the pane
        int dock_drop_offset = 0;
        if (part->dock->IsHorizontal())
            dock_drop_offset = pt.x - part->dock->rect.x - offset.x;
        else
            dock_drop_offset = pt.y - part->dock->rect.y - offset.y;


        // toolbars may only be moved in and to fixed-pane docks,
        // otherwise we will try to float the pane.  Also, the pane
        // should float if being dragged over center pane windows
        if (!part->dock->fixed || part->dock->dock_direction == wxAUI_DOCK_CENTER ||
            pt.x >= cli_size.x || pt.x <= 0 || pt.y >= cli_size.y || pt.y <= 0)
        {
            if (m_lastRect.IsEmpty() || m_lastRect.Contains(pt.x, pt.y ))
            {
                m_skipping = true;
            }
            else
            {
                if ((m_flags & wxAUI_MGR_ALLOW_FLOATING) && drop.IsFloatable())
                {
                    drop.Float();
                }

                m_skipping = false;

                return ProcessDockResult(target, drop);
            }

            drop.Position(pt.x - GetDockPixelOffset(drop) - offset.x);

            return ProcessDockResult(target, drop);
        }

        m_skipping = false;

        m_lastRect = part->dock->rect;
        m_lastRect.Inflate( 15, 15 );

        drop.Dock().
             Direction(part->dock->dock_direction).
             Layer(part->dock->dock_layer).
             Row(part->dock->dock_row).
             Position(dock_drop_offset);

        if ((
            ((pt.y < part->dock->rect.y + 1) && part->dock->IsHorizontal()) ||
            ((pt.x < part->dock->rect.x + 1) && part->dock->IsVertical())
            ) && part->dock->panes.GetCount() > 1)
        {
            if ((part->dock->dock_direction == wxAUI_DOCK_TOP) ||
                (part->dock->dock_direction == wxAUI_DOCK_LEFT))
            {
                int row = drop.dock_row;
                DoInsertDockRow(panes, part->dock->dock_direction,
                                part->dock->dock_layer,
                                part->dock->dock_row);
                drop.dock_row = row;
            }
            else
            {
                DoInsertDockRow(panes, part->dock->dock_direction,
                                part->dock->dock_layer,
                                part->dock->dock_row+1);
                drop.dock_row = part->dock->dock_row+1;
            }
        }

        if ((
            ((pt.y > part->dock->rect.y + part->dock->rect.height - 2 ) && part->dock->IsHorizontal()) ||
            ((pt.x > part->dock->rect.x + part->dock->rect.width - 2 ) && part->dock->IsVertical())
            ) && part->dock->panes.GetCount() > 1)
        {
            if ((part->dock->dock_direction == wxAUI_DOCK_TOP) ||
                (part->dock->dock_direction == wxAUI_DOCK_LEFT))
            {
                DoInsertDockRow(panes, part->dock->dock_direction,
                                part->dock->dock_layer,
                                part->dock->dock_row+1);
                drop.dock_row = part->dock->dock_row+1;
            }
            else
            {
                int row = drop.dock_row;
                DoInsertDockRow(panes, part->dock->dock_direction,
                                part->dock->dock_layer,
                                part->dock->dock_row);
                drop.dock_row = row;
            }
        }

        return ProcessDockResult(target, drop);
    }




    if (!part)
        return false;



    if (part->type == wxAuiDockUIPart::typePaneTab)
    {
        // Figure out if the pane is already part of the notebook, which will happen when dragging a tab in a notebook.
        bool isAlreadyInNotebook = part->m_tab_container->GetPages().Index(&target) != wxNOT_FOUND;

        // Check if tab moving is allowed
        if(isAlreadyInNotebook && !HasFlag(wxAUI_MGR_NB_TAB_MOVE))
        {
            return false;
        }

        wxAuiPaneInfo* hitPane=NULL;
        int page=-1;
        // If we are above a tab then insert before it, otherwise insert at the end
        if(!part->m_tab_container->TabHitTest(pt.x,pt.y,&hitPane))
        {
            if( part->m_tab_container->TabHitTest(pt.x+10,pt.y,&hitPane) || part->m_tab_container->TabHitTest(pt.x+10,pt.y+10,&hitPane) || part->m_tab_container->TabHitTest(pt.x,pt.y+10,&hitPane) )
            {
                //Before first tab - don't do anything
                return false;
            }
            else
            {
                // Insert at end.
                hitPane = &part->m_tab_container->GetPage(part->m_tab_container->GetPageCount()-1);
                page = hitPane->GetPage()+1;
            }
        }
        else
        {
            // Insert after pane we are hovering over.
            if (isAlreadyInNotebook) {
                if( drop.GetPage()<hitPane->GetPage() )
                {
                    page = hitPane->GetPage()+1;
                }
                else
                {
                    // Insert before pane we are hovering over.
                    page = hitPane->GetPage();
                }
            }
        }

        DoInsertPage(panes, hitPane->GetDirection(), hitPane->GetLayer(), hitPane->GetRow(), hitPane->GetPosition(), page);
        drop.Dock().Direction(hitPane->GetDirection()).Layer(hitPane->GetLayer()).Row(hitPane->GetRow()).Position(hitPane->GetPosition()).Page(page);
        return ProcessDockResult(target, drop);
    }
    else if(part->type == wxAuiDockUIPart::typeCaption)
    {
        drop.Dock().Direction(part->pane->GetDirection()).Layer(part->pane->GetLayer()).Row(part->pane->GetRow()).Position(part->pane->GetPosition()).Page(part->pane->GetPage()+1);
        return ProcessDockResult(target, drop);
    }
    else if (part->type == wxAuiDockUIPart::typePaneBorder || part->type == wxAuiDockUIPart::typeGripper || part->type == wxAuiDockUIPart::typePaneButton || part->type == wxAuiDockUIPart::typePane || part->type == wxAuiDockUIPart::typePaneSizer || part->type == wxAuiDockUIPart::typeDockSizer || part->type == wxAuiDockUIPart::typeBackground)
    {
        if (part->type == wxAuiDockUIPart::typeDockSizer)
        {
            if (part->dock->panes.GetCount() != 1)
                return false;
            part = GetPanePart(part->dock->panes.Item(0)->window);
            if (!part)
                return false;
        }



        // If a normal frame is being dragged over a toolbar, insert it
        // along the edge under the toolbar, but over all other panes.
        // (this could be done much better, but somehow factoring this
        // calculation with the one at the beginning of this function)
        if (part->dock && part->dock->toolbar)
        {
            int layer = 0;

            switch (part->dock->dock_direction)
            {
                case wxAUI_DOCK_LEFT:
                    layer = wxMax(wxMax(GetMaxLayer(docks, wxAUI_DOCK_LEFT),
                                      GetMaxLayer(docks, wxAUI_DOCK_BOTTOM)),
                                      GetMaxLayer(docks, wxAUI_DOCK_TOP));
                    break;
                case wxAUI_DOCK_TOP:
                    layer = wxMax(wxMax(GetMaxLayer(docks, wxAUI_DOCK_TOP),
                                      GetMaxLayer(docks, wxAUI_DOCK_LEFT)),
                                      GetMaxLayer(docks, wxAUI_DOCK_RIGHT));
                    break;
                case wxAUI_DOCK_RIGHT:
                    layer = wxMax(wxMax(GetMaxLayer(docks, wxAUI_DOCK_RIGHT),
                                      GetMaxLayer(docks, wxAUI_DOCK_TOP)),
                                      GetMaxLayer(docks, wxAUI_DOCK_BOTTOM));
                    break;
                case wxAUI_DOCK_BOTTOM:
                    layer = wxMax(wxMax(GetMaxLayer(docks, wxAUI_DOCK_BOTTOM),
                                      GetMaxLayer(docks, wxAUI_DOCK_LEFT)),
                                      GetMaxLayer(docks, wxAUI_DOCK_RIGHT));
                    break;
            }

            DoInsertDockRow(panes, part->dock->dock_direction,
                            layer, 0);
            drop.Dock().
                 Direction(part->dock->dock_direction).
                 Layer(layer).Row(0).Position(0);
            return ProcessDockResult(target, drop);
        }


        if (!part->pane) {
            // drop on background, i.e. on the center
            drop.Dock().Direction(wxAUI_DOCK_CENTER).Layer(0).Row(0).Position(0);
            return ProcessDockResult(target, drop);
        }

        part = GetPanePart(part->pane->window);
        if (!part)
            return false;

        bool insert_dock_row = false;
        int insert_row = part->pane->dock_row;
        int insert_dir = part->pane->dock_direction;
        int insert_layer = part->pane->dock_layer;

        switch (part->pane->dock_direction)
        {
            case wxAUI_DOCK_TOP:
                if (pt.y >= part->rect.y &&
                    pt.y < part->rect.y+auiInsertRowPixels)
                        insert_dock_row = true;
                break;
            case wxAUI_DOCK_BOTTOM:
                if (pt.y > part->rect.y+part->rect.height-auiInsertRowPixels &&
                    pt.y <= part->rect.y + part->rect.height)
                        insert_dock_row = true;
                break;
            case wxAUI_DOCK_LEFT:
                if (pt.x >= part->rect.x &&
                    pt.x < part->rect.x+auiInsertRowPixels)
                        insert_dock_row = true;
                break;
            case wxAUI_DOCK_RIGHT:
                if (pt.x > part->rect.x+part->rect.width-auiInsertRowPixels &&
                    pt.x <= part->rect.x+part->rect.width)
                        insert_dock_row = true;
                break;
            case wxAUI_DOCK_CENTER:
            {
                // "new row pixels" will be set to the default, but
                // must never exceed 20% of the window size
                int new_row_pixels_x = auiNewRowPixels;
                int new_row_pixels_y = auiNewRowPixels;

                if (new_row_pixels_x > (part->rect.width*20)/100)
                    new_row_pixels_x = (part->rect.width*20)/100;

                if (new_row_pixels_y > (part->rect.height*20)/100)
                    new_row_pixels_y = (part->rect.height*20)/100;


                // determine if the mouse pointer is in a location that
                // will cause a new row to be inserted.  The hot spot positions
                // are along the borders of the center pane

                insert_layer = 0;
                insert_dock_row = true;
                const wxRect& pr = part->rect;
                if (pt.x >= pr.x && pt.x < pr.x + new_row_pixels_x)
                    insert_dir = wxAUI_DOCK_LEFT;
                else if (pt.y >= pr.y && pt.y < pr.y + new_row_pixels_y)
                    insert_dir = wxAUI_DOCK_TOP;
                else if (pt.x >= pr.x + pr.width - new_row_pixels_x &&
                         pt.x < pr.x + pr.width)
                    insert_dir = wxAUI_DOCK_RIGHT;
                else if (pt.y >= pr.y+ pr.height - new_row_pixels_y &&
                         pt.y < pr.y + pr.height)
                    insert_dir = wxAUI_DOCK_BOTTOM;
                else
                    insert_dock_row = false;

                insert_row = GetMaxRow(panes, insert_dir, insert_layer) + 1;
            }
        }

        if (insert_dock_row)
        {
            DoInsertDockRow(panes, insert_dir, insert_layer, insert_row);
            drop.Dock().Direction(insert_dir).
                        Layer(insert_layer).
                        Row(insert_row).
                        Position(0);
            return ProcessDockResult(target, drop);
        }

        // determine the mouse offset and the pane size, both in the
        // direction of the dock itself, and perpendicular to the dock

        int mouseOffset, size;

        if (part->orientation == wxVERTICAL)
        {
            mouseOffset = pt.y - part->rect.y;
            size = part->rect.GetHeight();
        }
        else
        {
            mouseOffset = pt.x - part->rect.x;
            size = part->rect.GetWidth();
        }

        int drop_position = part->pane->dock_pos;

        // if we are in the top/left part of the pane,
        // insert the pane before the pane being hovered over
        if (mouseOffset <= size/2)
        {
            drop_position = part->pane->dock_pos;
            DoInsertPane(panes,
                         part->pane->dock_direction,
                         part->pane->dock_layer,
                         part->pane->dock_row,
                         part->pane->dock_pos);
        }

        // if we are in the bottom/right part of the pane,
        // insert the pane before the pane being hovered over
        if (mouseOffset > size/2)
        {
            drop_position = part->pane->dock_pos+1;
            DoInsertPane(panes,
                         part->pane->dock_direction,
                         part->pane->dock_layer,
                         part->pane->dock_row,
                         part->pane->dock_pos+1);
        }

        drop.Dock().
             Direction(part->dock->dock_direction).
             Layer(part->dock->dock_layer).
             Row(part->dock->dock_row).
             Position(drop_position);
        return ProcessDockResult(target, drop);
    }

    return false;
}


// Alert other manager of the drop and let it try handle it.
bool wxAuiManager::DoDropExternal(wxAuiManager* otherMgr, wxWindow* otherWnd, wxAuiPaneInfo& drop, const wxPoint& screenPt, const wxPoint& actionOffset)
{
    if(HasFlag(wxAUI_MGR_ALLOW_EXTERNAL_MOVE))
    {
        wxAuiManagerEvent e(wxEVT_AUI_ALLOW_DND);
        e.SetManager(this);
        e.SetPane(&drop);
        e.Veto(); // dropping must be explicitly approved by control owner
        otherWnd->GetEventHandler()->ProcessEvent(e);
        if(e.GetVeto())
        {
            // no answer or negative answer
            return false;
        }

        wxAuiPaneInfo dropExternal=drop;
        dropExternal.SetFlag(wxAuiPaneInfo::optionActive,false);
        dropExternal.SetFlag(wxAuiPaneInfo::optionActiveNotebook,false);

        wxPoint clientPt = otherMgr->GetManagedWindow()->ScreenToClient(screenPt);
        // Figure out if, and where, the other manager would place the pane if dropped.
        if(otherMgr->DoDrop(otherMgr->m_docks, otherMgr->m_panes, dropExternal, clientPt, actionOffset))
        {
            // If the other manager accepted the drop then remove the pane from this manager and do the drop into the other manager.
            DetachPane(drop.GetWindow());
            drop.GetWindow()->Reparent(otherWnd);
            otherMgr->AddPane(dropExternal.GetWindow(),dropExternal);
            return true;
        }
        else
        {
            otherMgr->DetachPane(drop.GetWindow());
            return false;
        }
    }
    return false;
}

void wxAuiManager::OnHintFadeTimer(wxTimerEvent& WXUNUSED(event))
{
    if (!m_hintWnd || m_hintFadeAmt >= m_hintFadeMax)
    {
        m_hintFadeTimer.Stop();
        Disconnect(m_hintFadeTimer.GetId(), wxEVT_TIMER,
                   wxTimerEventHandler(wxAuiManager::OnHintFadeTimer));
        return;
    }

    m_hintFadeAmt += 4;
    m_hintWnd->SetTransparent(m_hintFadeAmt);
}

void wxAuiManager::ShowHint(const wxRect& rect)
{
    if (m_hintWnd)
    {
        // if the hint rect is the same as last time, don't do anything
        if (m_lastHint == rect)
            return;
        m_lastHint = rect;

        m_hintFadeAmt = m_hintFadeMax;

        if ((m_flags & wxAUI_MGR_HINT_FADE)
            && !((wxDynamicCast(m_hintWnd, wxPseudoTransparentFrame)) &&
                 (m_flags & wxAUI_MGR_NO_VENETIAN_BLINDS_FADE))
            )
            m_hintFadeAmt = 0;

        m_hintWnd->SetSize(rect);
        m_hintWnd->SetTransparent(m_hintFadeAmt);

        if (!m_hintWnd->IsShown())
            m_hintWnd->Show();

        // if we are dragging a floating pane, set the focus
        // back to that floating pane (otherwise it becomes unfocused)
        if (m_action == actionDragFloatingPane && m_actionWindow)
            m_actionWindow->SetFocus();

        m_hintWnd->Raise();


        if (m_hintFadeAmt != m_hintFadeMax) //  Only fade if we need to
        {
            // start fade in timer
            m_hintFadeTimer.SetOwner(this);
            m_hintFadeTimer.Start(5);
            Connect(m_hintFadeTimer.GetId(), wxEVT_TIMER,
                    wxTimerEventHandler(wxAuiManager::OnHintFadeTimer));
        }
    }
    else  // Not using a transparent hint window...
    {
        if (!(m_flags & wxAUI_MGR_RECTANGLE_HINT))
            return;

        if (m_lastHint != rect)
        {
            // remove the last hint rectangle
            m_lastHint = rect;
            m_frame->Refresh();
            m_frame->Update();
        }

        wxScreenDC screendc;
        wxRegion clip(1, 1, 10000, 10000);

        // clip all floating windows, so we don't draw over them
        int i, pane_count;
        for (i = 0, pane_count = m_panes.GetCount(); i < pane_count; ++i)
        {
            wxAuiPaneInfo& pane = m_panes.Item(i);

            if (pane.IsFloating() &&
                    pane.frame &&
                        pane.frame->IsShown())
            {
                wxRect r = pane.frame->GetRect();
#ifdef __WXGTK__
                // wxGTK returns the client size, not the whole frame size
                r.width += 15;
                r.height += 35;
                r.Inflate(5);
#endif

                clip.Subtract(r);
            }
        }

        // As we can only hide the hint by redrawing the managed window, we
        // need to clip the region to the managed window too or we get
        // nasty redrawn problems.
        clip.Intersect(m_frame->GetRect());

        screendc.SetDeviceClippingRegion(clip);

        wxBitmap stipple = wxPaneCreateStippleBitmap();
        wxBrush brush(stipple);
        screendc.SetBrush(brush);
        screendc.SetPen(*wxTRANSPARENT_PEN);

        screendc.DrawRectangle(rect.x, rect.y, 5, rect.height);
        screendc.DrawRectangle(rect.x+5, rect.y, rect.width-10, 5);
        screendc.DrawRectangle(rect.x+rect.width-5, rect.y, 5, rect.height);
        screendc.DrawRectangle(rect.x+5, rect.y+rect.height-5, rect.width-10, 5);
    }
}

void wxAuiManager::HideHint()
{
    // hides a transparent window hint, if there is one
    if (m_hintWnd)
    {
        if (m_hintWnd->IsShown())
            m_hintWnd->Show(false);
        m_hintWnd->SetTransparent(0);
        m_hintFadeTimer.Stop();
        // In case this is called while a hint fade is going, we need to
        // disconnect the event handler.
        Disconnect(m_hintFadeTimer.GetId(), wxEVT_TIMER,
                   wxTimerEventHandler(wxAuiManager::OnHintFadeTimer));
        m_lastHint = wxRect();
        return;
    }

    // hides a painted hint by redrawing the frame window
    if (!m_lastHint.IsEmpty())
    {
        m_frame->Refresh();
        m_frame->Update();
        m_lastHint = wxRect();
    }
}

void wxAuiManager::OnHintActivate(wxActivateEvent& WXUNUSED(event))
{
    // Do nothing so this event isn't handled in the base handlers.

    // Letting the hint window activate without this handler can lead to
    // weird behaviour on Mac where the menu is switched out to the top
    // window's menu in MDI applications when it shouldn't be. So since
    // we don't want user interaction with the hint window anyway, we just
    // prevent it from activating here.
}



void wxAuiManager::StartPaneDrag(wxWindow* pane_window,
                                 const wxPoint& offset)
{
    wxAuiPaneInfo& pane = GetPane(pane_window);
    if (!pane.IsOk())
        return;

    if (pane.IsToolbar())
    {
        m_action = actionDragToolbarPane;
    }
    else
    {
        m_action = actionDragFloatingPane;
    }

    m_actionWindow = pane_window;
    m_actionOffset = offset;
    m_frame->CaptureMouse();

    if (pane.frame)
    {
        wxRect window_rect = pane.frame->GetRect();
        wxRect client_rect = pane.frame->GetClientRect();
        wxPoint client_pt = pane.frame->ClientToScreen(client_rect.GetTopLeft());
        wxPoint origin_pt = client_pt - window_rect.GetTopLeft();
        m_actionOffset += origin_pt;
    }
}


// CalculateHintRect() calculates the drop hint rectangle.  The method
// first calls DoDrop() to determine the exact position the pane would
// be at were if dropped.  If the pane would indeed become docked at the
// specified drop point, the rectangle hint will be returned in
// screen coordinates.  Otherwise, an empty rectangle is returned.
// |pane_window| is the window pointer of the pane being dragged, |pt| is
// the mouse position, in client coordinates.  |offset| describes the offset
// that the mouse is from the upper-left corner of the item being dragged

wxRect wxAuiManager::CalculateHintRect(wxWindow* pane_window,
                                       const wxPoint& pt,
                                       const wxPoint& offset)
{
    wxRect rect;
    m_doingHintCalculation = true;

    // we need to paint a hint rectangle; to find out the exact hint rectangle,
    // we will create a new temporary layout and then measure the resulting
    // rectangle; we will create a copy of the docking structures (m_dock)
    // so that we don't modify the real thing on screen

    int i, pane_count, part_count;
    wxAuiDockInfoArray docks;
    wxAuiPaneInfoArray panes;
    wxAuiDockUIPartArray uiparts;
    wxAuiPaneInfo hint = GetPane(pane_window);
    hint.name = wxT("__HINT__");
    hint.PaneBorder(true);
    hint.Show();

    if (!hint.IsOk())
    {
        m_doingHintCalculation = false;
        return rect;
    }

    CopyDocksAndPanes(docks, panes, m_docks, m_panes);

    // remove any pane already there which bears the same window;
    // this happens when you are moving a pane around in a dock
    for (i = 0, pane_count = panes.GetCount(); i < pane_count; ++i)
    {
        if (panes.Item(i).window == pane_window)
        {
            RemovePaneFromDocks(docks, panes.Item(i));
            panes.RemoveAt(i);
            break;
        }
    }

    // find out where the new pane would be
    if (!DoDrop(docks, panes, hint, pt, offset))
    {
        m_doingHintCalculation = false;
        return rect;
    }

    panes.Add(hint);

    wxSizer* sizer = LayoutAll(panes, docks, uiparts, true);
    wxSize client_size = m_frame->GetClientSize();
    sizer->SetDimension(0, 0, client_size.x, client_size.y);
    sizer->Layout();

    // First check if the hint is somewhere in a notebook, if it is then take the hint rect from the active notebook page.
    bool hint_found = false;
#if 0
    for (i = 0, part_count = uiparts.GetCount(); i < part_count && !hint_found; ++i)
    {
        wxAuiDockUIPart& part = uiparts.Item(i);
        if (part.type == wxAuiDockUIPart::typePaneTab)
        {
            wxAuiPaneInfoPtrArray& pages = part.m_tab_container->GetPages();
            int pageCount = pages.GetCount();
            int j = 0;
            for (j = 0; j < pageCount && !hint_found;j++)
            {
                if(pages[j]->GetName() == wxT("__HINT__"))
                {
                    hint_found = true;

                    int activePage=part.m_tab_container->GetActivePage();
                    // It is possible in some instances (when forming a new notebook via drag) - that no page is yet active, if this is the case act as if the first one is active.
                    if(activePage==-1)
                        activePage=0;

                    rect = part.m_tab_container->GetPage(activePage).GetWindow()->GetRect();

                    // If we are a new notebook then there are no tabs yet on the page, so the hint will be wrong.
                    // We need to add the height/width of the tabs
                    // TODO: Future - we could probably get 'fancy' here and actually draw some tab outlines in as part of the hint if we wanted (instead of just increas.ing the rectangle size)
                    if(pageCount==2)
                    {
                        //fixme: Need a better way to do this.
                        rect.y-=notebookTabHeight/2;
                        rect.height+=notebookTabHeight/2;
                    }
                    break;
                }
            }
        }
    }

#endif

    // If it is not in the notebook then take the hint from the pane border.
    if (rect.IsEmpty())
    {
        for (i = 0, part_count = uiparts.GetCount(), hint_found=false;
            i < part_count && !hint_found; ++i)
        {
            wxAuiDockUIPart& part = uiparts.Item(i);

            if (part.type == wxAuiDockUIPart::typePaneBorder &&
            part.pane && part.pane->GetName() == wxT("__HINT__"))
            {
                rect = wxRect(part.sizer_item->GetPosition(),
                          part.sizer_item->GetSize());
                hint_found = true;
            }
        }
    }

    delete sizer;

    if (rect.IsEmpty())
    {
        m_doingHintCalculation = false;
        return rect;
    }


    if ( wxDynamicCast(pane_window->GetParent(), wxAuiNotebook) ) {

        // If the dragged pane is part of a wxAuiNotebook, we have to better show
        // the split direction, so truncate the hint rectangle in the opposite direction to it's own direction

        int hw = wxMax(rect.GetWidth()/2,1);
        int hh = wxMax(rect.GetHeight()/2,1);
        switch(hint.GetDirection()) {

            case wxAUI_DOCK_BOTTOM: rect.SetTop(rect.GetTop()+rect.GetHeight()-hh);
            case wxAUI_DOCK_TOP   : rect.SetHeight(hh); break;

            case wxAUI_DOCK_RIGHT : rect.SetLeft(rect.GetLeft()+rect.GetWidth()-hw);
            case wxAUI_DOCK_LEFT  : rect.SetWidth(hw); break;

        }

    }


    // actually show the hint rectangle on the screen
    m_frame->ClientToScreen(&rect.x, &rect.y);

    if ( m_frame->GetLayoutDirection() == wxLayout_RightToLeft )
    {
        // Mirror rectangle in RTL mode
        rect.x -= rect.GetWidth();
    }

    m_doingHintCalculation = false;
    return rect;
}

// DrawHintRect() calculates the hint rectangle by calling
// CalculateHintRect().  If there is a rectangle, it shows it
// by calling ShowHint(), otherwise it hides any hint
// rectangle currently shown
void wxAuiManager::DrawHintRect(wxWindow* pane_window,
                                const wxPoint& pt,
                                const wxPoint& offset)
{
    //Special case: if we allow external drops to windows managed by other managers then handle this first before all other cases.
    if(HasFlag(wxAUI_MGR_ALLOW_EXTERNAL_MOVE))
    {
        wxAuiDockUIPart* part = HitTest(pt.x, pt.y);
        if(!part)
        {
            wxPoint screenPt = ::wxGetMousePosition();
            wxWindow* targetCtrl = ::wxFindWindowAtPoint(screenPt);

            // Make sure we have a target ctrl (it can be NULL if e.g. drop is outside of program window) - if we don't have one we don't draw a hint here.
            if(!targetCtrl)
                return;

            // If we are on top of a hint window then quickly hide the hint window and get the window that is underneath it.
            if(targetCtrl==m_hintWnd)
            {
                m_hintWnd->Hide();
                targetCtrl = ::wxFindWindowAtPoint(screenPt);
                m_hintWnd->Show();
            }

            // Find the manager this window belongs to (if it does belong to one)
            wxAuiManager* otherMgr = NULL;
            while(targetCtrl)
            {
                if(!wxDynamicCast(targetCtrl,wxAuiFloatingFrame))
                {
                    if(targetCtrl->GetEventHandler() && wxDynamicCast(targetCtrl->GetEventHandler(),wxAuiManager))
                    {
                        otherMgr = ((wxAuiManager*)targetCtrl->GetEventHandler());
                        break;
                    }
                }
                targetCtrl = targetCtrl->GetParent();
            }
            // Alert other manager of the drop and have it show hint.
            if(otherMgr && otherMgr != this)
            {
                // find out from the destination control if it's ok to drop this tab here.
                if(wxDynamicCast(targetCtrl,wxAuiNotebook))
                {
                    wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_ALLOW_DND, GetManagedWindow()->GetId());
                    int selIndex=GetAllPanes().Index(GetPane(pane_window));
                    e.SetSelection(selIndex);
                    e.SetOldSelection(selIndex);
                    e.SetEventObject((wxAuiNotebook*)GetManagedWindow());
                    e.SetDragSource((wxAuiNotebook*)GetManagedWindow());
                    e.Veto(); // dropping must be explicitly approved by control owner

                    targetCtrl->GetEventHandler()->ProcessEvent(e);

                    if (!e.IsAllowed())
                    {
                        // no answer or negative answer
                        HideHint();
                        return;
                    }
                }
                else
                {
                    wxAuiManagerEvent e(wxEVT_AUI_ALLOW_DND);
                    e.SetManager(this);
                    e.SetPane(&GetPane(pane_window));
                    e.Veto(); // dropping must be explicitly approved by control owner
                    targetCtrl->GetEventHandler()->ProcessEvent(e);
                    if(e.GetVeto())
                    {
                        // no answer or negative answer
                        HideHint();
                        return;
                    }
                }

                otherMgr->AddPane(pane_window);
                wxPoint clientPt = otherMgr->GetManagedWindow()->ScreenToClient(screenPt);
                wxRect hint_rect = otherMgr->CalculateHintRect(pane_window, clientPt, offset);
                otherMgr->DetachPane(pane_window);
                if(hint_rect.IsEmpty())
                {
                    HideHint();
                }
                else
                {
                    ShowHint(hint_rect);
                }
                return;
            }
        }
    }

    //Normal case: calculate and display hint for a drop in our own window.
    wxRect rect = CalculateHintRect(pane_window, pt, offset);
    if (rect.IsEmpty())
    {
        HideHint();
    }
    else
    {
        ShowHint(rect);
    }
}

void wxAuiManager::OnFloatingPaneMoveStart(wxWindow* wnd)
{
    // try to find the pane
    wxAuiPaneInfo& pane = GetPane(wnd);
    wxASSERT_MSG(pane.IsOk(), wxT("Pane window not found"));

    if(!pane.frame)
        return;

    if (m_flags & wxAUI_MGR_TRANSPARENT_DRAG)
        pane.frame->SetTransparent(150);
}

void wxAuiManager::OnFloatingPaneMoving(wxWindow* wnd, wxDirection dir)
{
    // try to find the pane
    wxAuiPaneInfo& pane = GetPane(wnd);
    wxASSERT_MSG(pane.IsOk(), wxT("Pane window not found"));

    wxPoint pt = ::wxGetMousePosition();

#if 0
    // Adapt pt to direction
    if (dir == wxNORTH)
    {
        // move to pane's upper border
        wxPoint pos( 0,0 );
        pos = wnd->ClientToScreen( pos );
        pt.y = pos.y;
        // and some more pixels for the title bar
        pt.y -= 5;
    }
    else if (dir == wxWEST)
    {
        // move to pane's left border
        wxPoint pos( 0,0 );
        pos = wnd->ClientToScreen( pos );
        pt.x = pos.x;
    }
    else if (dir == wxEAST)
    {
        // move to pane's right border
        wxPoint pos( wnd->GetSize().x, 0 );
        pos = wnd->ClientToScreen( pos );
        pt.x = pos.x;
    }
    else if (dir == wxSOUTH)
    {
        // move to pane's bottom border
        wxPoint pos( 0, wnd->GetSize().y );
        pos = wnd->ClientToScreen( pos );
        pt.y = pos.y;
    }
#else
    wxUnusedVar(dir);
#endif

    wxPoint client_pt = m_frame->ScreenToClient(pt);

    // calculate the offset from the upper left-hand corner
    // of the frame to the mouse pointer
    wxPoint frame_pos = pane.frame->GetPosition();
    wxPoint action_offset(pt.x-frame_pos.x, pt.y-frame_pos.y);

    // no hint for toolbar floating windows
    if (pane.IsToolbar() && m_action == actionDragFloatingPane)
    {
        wxAuiDockInfoArray docks;
        wxAuiPaneInfoArray panes;
        wxAuiDockUIPartArray uiparts;
        wxAuiPaneInfo hint = pane;

        CopyDocksAndPanes(docks, panes, m_docks, m_panes);

        // find out where the new pane would be
        if (!DoDrop(docks, panes, hint, client_pt))
            return;
        if (hint.IsFloating())
            return;

        pane = hint;
        m_action = actionDragToolbarPane;
        m_actionWindow = pane.window;

        Update();

        return;
    }


    // if a key modifier is pressed while dragging the frame,
    // don't dock the window
    if (!CanDockPanel(pane))
    {
        HideHint();
        return;
    }


    DrawHintRect(wnd, client_pt, action_offset);

#ifdef __WXGTK__
    // this cleans up some screen artifacts that are caused on GTK because
    // we aren't getting the exact size of the window (see comment
    // in DrawHintRect)
    //Refresh();
#endif


    // reduces flicker
    m_frame->Update();
}

void wxAuiManager::OnFloatingPaneMoved(wxWindow* wnd, wxDirection dir)
{
    // try to find the pane
    wxAuiPaneInfo& pane = GetPane(wnd);
    wxASSERT_MSG(pane.IsOk(), wxT("Pane window not found"));

    wxPoint pt = ::wxGetMousePosition();

#if 0
    // Adapt pt to direction
    if (dir == wxNORTH)
    {
        // move to pane's upper border
        wxPoint pos( 0,0 );
        pos = wnd->ClientToScreen( pos );
        pt.y = pos.y;
        // and some more pixels for the title bar
        pt.y -= 10;
    }
    else if (dir == wxWEST)
    {
        // move to pane's left border
        wxPoint pos( 0,0 );
        pos = wnd->ClientToScreen( pos );
        pt.x = pos.x;
    }
    else if (dir == wxEAST)
    {
        // move to pane's right border
        wxPoint pos( wnd->GetSize().x, 0 );
        pos = wnd->ClientToScreen( pos );
        pt.x = pos.x;
    }
    else if (dir == wxSOUTH)
    {
        // move to pane's bottom border
        wxPoint pos( 0, wnd->GetSize().y );
        pos = wnd->ClientToScreen( pos );
        pt.y = pos.y;
    }
#else
    wxUnusedVar(dir);
#endif

    wxPoint client_pt = m_frame->ScreenToClient(pt);

    // calculate the offset from the upper left-hand corner
    // of the frame to the mouse pointer
    wxPoint frame_pos = pane.frame->GetPosition();
    wxPoint action_offset(pt.x-frame_pos.x, pt.y-frame_pos.y);

    // if a key modifier is pressed while dragging the frame,
    // don't dock the window
    if (CanDockPanel(pane))
    {
        // do the drop calculation
        DoDrop(m_docks, m_panes, pane, client_pt, action_offset);
    }

    // if the pane is still floating, update its floating
    // position (that we store)
    if (pane.IsFloating())
    {
        pane.floating_pos = pane.frame->GetPosition();

        if (m_flags & wxAUI_MGR_TRANSPARENT_DRAG)
            pane.frame->SetTransparent(255);
    }
    else if (m_hasMaximized)
    {
        RestoreMaximizedPane();
    }

    // Update the layout to realize new position and e.g. form notebooks if needed.
    Update();

    HideHint();

    // If a notebook formed we may have lost our active status so set it again.
    SetActivePane(pane.GetWindow());

    // Allow the updated layout an opportunity to recalculate/update the pane positions.
    DoFrameLayout();

    // Make changes visible to user.
    Repaint();
}

void wxAuiManager::OnFloatingPaneResized(wxWindow* wnd, const wxRect& rect)
{
    // try to find the pane
    wxAuiPaneInfo& pane = GetPane(wnd);
    wxASSERT_MSG(pane.IsOk(), wxT("Pane window not found"));

    pane.FloatingSize(rect.GetWidth(), rect.GetHeight());

    // the top-left position may change as well as the size
    pane.FloatingPosition(rect.x, rect.y);
}


void wxAuiManager::OnFloatingPaneClosed(wxWindow* wnd, wxCloseEvent& evt)
{
    // try to find the pane
    wxAuiPaneInfo& pane = GetPane(wnd);
    wxASSERT_MSG(pane.IsOk(), wxT("Pane window not found"));

    if (!ClosePane(pane))
		evt.Veto();
}



void wxAuiManager::OnFloatingPaneActivated(wxWindow* wnd)
{
    if (GetPane(wnd).IsOk())
    {
        SetActivePane(wnd);
        Repaint();
    }
}

// OnRender() draws all of the pane captions, sashes,
// backgrounds, captions, grippers, pane borders and buttons.
// It renders the entire user interface.

void wxAuiManager::OnRender(wxAuiManagerEvent& evt)
{
    // if the frame is about to be deleted, don't bother
    if (!m_frame || wxPendingDelete.Member(m_frame))
        return;

    wxDC* dc = evt.GetDC();

#ifdef __WXMAC__
    dc->Clear() ;
#endif
    int i, part_count;
    for (i = 0, part_count = m_uiParts.GetCount();
         i < part_count; ++i)
    {
        wxAuiDockUIPart& part = m_uiParts.Item(i);

        // don't draw hidden pane items or items that aren't windows
        if (part.sizer_item &&
                ((!part.sizer_item->IsWindow() &&
                  !part.sizer_item->IsSpacer() &&
                  !part.sizer_item->IsSizer()) ||
                  !part.sizer_item->IsShown()))
            continue;

        switch (part.type)
        {
            case wxAuiDockUIPart::typePaneTab:
                part.m_tab_container->DrawTabs(dc, m_frame, part.rect);
                break;
            case wxAuiDockUIPart::typeDockSizer:
            case wxAuiDockUIPart::typePaneSizer:
                m_art->DrawSash(*dc, m_frame, part.orientation, part.rect);
                break;
            case wxAuiDockUIPart::typeBackground:
                m_art->DrawBackground(*dc, m_frame, part.orientation, part.rect);
                break;
            case wxAuiDockUIPart::typeCaption:
                m_art->DrawCaption(*dc, m_frame, part.pane->caption, part.rect, *part.pane);
                break;
            case wxAuiDockUIPart::typeGripper:
                m_art->DrawGripper(*dc, m_frame, part.rect, *part.pane);
                break;
            case wxAuiDockUIPart::typePaneBorder:
                m_art->DrawBorder(*dc, m_frame, part.rect, *part.pane);
                break;
            case wxAuiDockUIPart::typePaneButton:
                m_art->DrawPaneButton(*dc, m_frame, part.button->button_id,
                        wxAUI_BUTTON_STATE_NORMAL, part.rect, *part.pane);
                break;
        }
    }
}

 wxAuiDockUIPart::~wxAuiDockUIPart()
    {
        if(m_tab_container)
        {
            delete m_tab_container;
            m_tab_container = NULL;
        }
    }

// Render() fire a render event, which is normally handled by
// wxAuiManager::OnRender().  This allows the render function to
// be overridden via the render event.  This can be useful for paintin
// custom graphics in the main window. Default behaviour can be
// invoked in the overridden function by calling OnRender()

void wxAuiManager::Render(wxDC* dc)
{
    wxAuiManagerEvent e(wxEVT_AUI_RENDER);
    e.SetManager(this);
    e.SetDC(dc);
    ProcessMgrEvent(e);
}

void wxAuiManager::Repaint(wxDC* dc)
{
#ifdef __WXMAC__ 
    if ( dc == NULL )
    {
        m_frame->Refresh() ;
        m_frame->Update() ;
        return ;
    }
#endif
    int w, h;
    m_frame->GetClientSize(&w, &h);

    // figure out which dc to use; if one
    // has been specified, use it, otherwise
    // make a client dc
    wxClientDC* client_dc = NULL;
    if (!dc)
    {
        client_dc = new wxClientDC(m_frame);
        dc = client_dc;
    }

    // if the frame has a toolbar, the client area
    // origin will not be (0,0).
    wxPoint pt = m_frame->GetClientAreaOrigin();
    if (pt.x != 0 || pt.y != 0)
        dc->SetDeviceOrigin(pt.x, pt.y);

    // render all the items
    Render(dc);

    // if we created a client_dc, delete it
    if (client_dc)
        delete client_dc;
}

void wxAuiManager::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(m_frame);
    Repaint(&dc);
}

void wxAuiManager::OnEraseBackground(wxEraseEvent& event)
{
#ifdef __WXMAC__
    event.Skip() ;
#else
    wxUnusedVar(event);
#endif
}

void wxAuiManager::OnSize(wxSizeEvent& event)
{
    if (m_frame)
    {
        DoFrameLayout();
        Repaint();

#if wxUSE_MDI
        if (wxDynamicCast(m_frame, wxMDIParentFrame))
        {
            // for MDI parent frames, this event must not
            // be "skipped".  In other words, the parent frame
            // must not be allowed to resize the client window
            // after we are finished processing sizing changes
            return;
        }
#endif
    }
    event.Skip();
}

void wxAuiManager::OnFindManager(wxAuiManagerEvent& evt)
{

    // get the searched window
    wxWindow *target = wxDynamicCast(evt.GetEventObject(),wxWindow);

    // get the window we are managing, if none, return NULL
    wxWindow* window = GetManagedWindow();
    if (!window || !target)
    {
        evt.SetManager(NULL);
        return;
    }

    // if we are managing a child frame, get the 'real' manager
    if (wxDynamicCast(window, wxAuiFloatingFrame))
    {
        wxAuiFloatingFrame* float_frame = static_cast<wxAuiFloatingFrame*>(window);
        evt.SetManager(float_frame->GetOwnerManager());
        return;
    }

    // if our managed window is the target, we should pass the event up to an upper manager
    if (target == window) {
        evt.Skip();
        return;
    }

    // return pointer to ourself
    evt.SetManager(this);
}

void wxAuiManager::OnSetCursor(wxSetCursorEvent& event)
{
    // determine cursor
    wxAuiDockUIPart* part = HitTest(event.GetX(), event.GetY());
    wxCursor cursor = wxNullCursor;

    if (part)
    {
        if (part->type == wxAuiDockUIPart::typeDockSizer ||
            part->type == wxAuiDockUIPart::typePaneSizer)
        {
            // a dock may not be resized if it has a single
            // pane which is not resizable
            if (part->type == wxAuiDockUIPart::typeDockSizer && part->dock &&
                part->dock->panes.GetCount() == 1 &&
                part->dock->panes.Item(0)->IsFixed())
                    return;

            // panes that may not be resized do not get a sizing cursor
            if (part->pane && part->pane->IsFixed())
                return;

            if (part->orientation == wxVERTICAL)
                cursor = wxCursor(wxCURSOR_SIZEWE);
            else
                cursor = wxCursor(wxCURSOR_SIZENS);
        }
        else if (part->type == wxAuiDockUIPart::typeGripper)
        {
            cursor = wxCursor(wxCURSOR_SIZING);
        }
    }

    event.SetCursor(cursor);
}



void wxAuiManager::UpdateButtonOnScreen(wxAuiDockUIPart* button_ui_part,
                                        const wxMouseEvent& event)
{
    wxAuiDockUIPart* hit_test = HitTest(event.GetX(), event.GetY());
    if (!hit_test || !button_ui_part)
        return;

    int state = wxAUI_BUTTON_STATE_NORMAL;

    if (hit_test == button_ui_part)
    {
        if (event.LeftIsDown())
            state = wxAUI_BUTTON_STATE_PRESSED;
        else
            state = wxAUI_BUTTON_STATE_HOVER;
    }
    else
    {
        if (event.LeftIsDown())
            state = wxAUI_BUTTON_STATE_HOVER;
    }

    // now repaint the button with hover state
    wxClientDC cdc(m_frame);

    // if the frame has a toolbar, the client area
    // origin will not be (0,0).
    wxPoint pt = m_frame->GetClientAreaOrigin();
    if (pt.x != 0 || pt.y != 0)
        cdc.SetDeviceOrigin(pt.x, pt.y);

    if (hit_test->pane)
    {
        m_art->DrawPaneButton(cdc, m_frame,
                  button_ui_part->button->button_id,
                  state,
                  button_ui_part->rect,
                  *hit_test->pane);
    }
}

void wxAuiManager::OnLeftDClick(wxMouseEvent& event)
{
    wxAuiDockUIPart* part = HitTest(event.GetX(), event.GetY());
    if(part)
    {
        if(part->type == wxAuiDockUIPart::typePaneTab)
        {
            wxAuiPaneInfo* hitPane;
            wxAuiTabContainerButton* hitbutton;
            if(!part->m_tab_container->ButtonHitTest(event.m_x,event.m_y,&hitbutton)) {

                if (part->m_tab_container->TabHitTest(event.m_x,event.m_y,&hitPane))
                {
                    wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_TAB_DCLICK, GetManagedWindow()->GetId());
                    e.SetEventObject(GetManagedWindow());
                    e.SetSelection(GetAllPanes().Index(*hitPane));
                    GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
                } else {
                    wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_BG_DCLICK, GetManagedWindow()->GetId());
                    e.SetEventObject(GetManagedWindow());
                    GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
                }

            }
        } else if (part->type == wxAuiDockUIPart::typeCaption) {

			wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_TAB_DCLICK, GetManagedWindow()->GetId());
			e.SetEventObject(GetManagedWindow());
			e.SetSelection(GetAllPanes().Index(*(part->pane)));
			GetManagedWindow()->GetEventHandler()->ProcessEvent(e);

		}
    }
}

void wxAuiManager::OnLeftDown(wxMouseEvent& event)
{
    m_currentDragItem = -1;

    wxAuiDockUIPart* part = HitTest(event.GetX(), event.GetY());
    if (part)
    {
        if (part->type == wxAuiDockUIPart::typeDockSizer ||
            part->type == wxAuiDockUIPart::typePaneSizer)
        {
            // Removing this restriction so that a centre pane can be resized
            //if (part->dock && part->dock->dock_direction == wxAUI_DOCK_CENTER)
            //    return;

            // a dock may not be resized if it has a single
            // pane which is not resizable
            if (part->type == wxAuiDockUIPart::typeDockSizer && part->dock &&
                part->dock->panes.GetCount() == 1 &&
                part->dock->panes.Item(0)->IsFixed())
                    return;

            // panes that may not be resized should be ignored here
            if (part->pane && part->pane->IsFixed())
                return;

            m_action = actionResize;
            m_actionPart = part;
            m_actionHintRect = wxRect();
            m_actionStart = wxPoint(event.m_x, event.m_y);
            m_actionOffset = wxPoint(event.m_x - part->rect.x,
                                      event.m_y - part->rect.y);
            m_frame->CaptureMouse();
        }
        else if (part->type == wxAuiDockUIPart::typePaneButton)
        {
            m_action = actionClickButton;
            m_actionPart = part;
            m_actionStart = wxPoint(event.m_x, event.m_y);
            m_frame->CaptureMouse();

            UpdateButtonOnScreen(part, event);
        }
        else if (part->type == wxAuiDockUIPart::typeCaption ||
                  part->type == wxAuiDockUIPart::typeGripper)
        {
            // if we are managing a wxAuiFloatingFrame window, then
            // we are an embedded wxAuiManager inside the wxAuiFloatingFrame.
            // We want to initiate a toolbar drag in our owner manager
            wxWindow* managed_wnd = GetManagedWindow();

            if (part->pane &&
                part->pane->window &&
                managed_wnd &&
                wxDynamicCast(managed_wnd, wxAuiFloatingFrame))
            {
                wxAuiFloatingFrame* floating_frame = (wxAuiFloatingFrame*)managed_wnd;
                wxAuiManager* owner_mgr = floating_frame->GetOwnerManager();
                owner_mgr->StartPaneDrag(part->pane->window,
                                             wxPoint(event.m_x - part->rect.x,
                                                     event.m_y - part->rect.y));
                return;
            }

            SetActivePane(part->pane->GetWindow());
            if (GetFlags() & wxAUI_MGR_ALLOW_ACTIVE_PANE)
                Repaint();

            m_action = actionClickCaption;
            m_actionPart = part;
            m_actionStart = wxPoint(event.m_x, event.m_y);
            m_actionOffset = wxPoint(event.m_x - part->rect.x,
                                      event.m_y - part->rect.y);
            m_frame->CaptureMouse();
        }
        else if(part->type == wxAuiDockUIPart::typePaneTab)
        {
            wxAuiPaneInfo* hitPane;
            if(part->m_tab_container->ButtonHitTest(event.m_x,event.m_y,&m_hoverButton2))
            {
                m_action = actionClickButton;
                m_actionPart = part;
                m_actionStart = wxPoint(event.m_x, event.m_y);
                m_frame->CaptureMouse();
                part->pane = &part->m_tab_container->GetPage(part->m_tab_container->GetActivePage());

                m_hoverButton2->curState = wxAUI_BUTTON_STATE_PRESSED;
                Repaint();
            }
            else if(part->m_tab_container->TabHitTest(event.m_x,event.m_y,&hitPane))
            {
                int oldActivePaneIndex=GetActivePane(NULL);
                int newActivePaneIndex=GetAllPanes().Index(*hitPane);

                // If we are a wxAuiNotebook then we must fire off a wxEVT_AUINOTEBOOK_PAGE_CHANGING event and give the user an opportunity to veto it.
                bool vetoed=false;
                if(wxDynamicCast(GetManagedWindow(),wxAuiNotebook))
                {
                    wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_PAGE_CHANGING, GetManagedWindow()->GetId());
                    e.SetSelection(newActivePaneIndex);
                    e.SetOldSelection(oldActivePaneIndex);
                    e.SetEventObject(GetManagedWindow());
                    GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
                    vetoed = !e.IsAllowed();
                }

                // Either no event sent or event not vetoed - either way go ahead and do the change
                if(!vetoed)
                {
                    SetActivePane(hitPane->GetWindow());

                    m_actionOffset = wxPoint(event.m_x-hitPane->GetRect().x,event.m_y-part->rect.y);

                    m_action = actionClickCaption;
                    m_actionPart = part;
                    m_actionPart->pane = hitPane;
                    m_actionStart = wxPoint(event.m_x, event.m_y);
                    m_frame->CaptureMouse();

                    DoFrameLayout();
                    Repaint();

                    // If we are a wxAuiNotebook then we must fire off a wxEVT_AUINOTEBOOK_PAGE_CHANGED event.
                    if(wxDynamicCast(GetManagedWindow(),wxAuiNotebook))
                    {
                        wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_PAGE_CHANGED, GetManagedWindow()->GetId());
                        e.SetSelection(newActivePaneIndex);
                        e.SetOldSelection(oldActivePaneIndex);
                        e.SetEventObject(GetManagedWindow());
                        GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
                    }
                }
            }
        }
#ifdef __WXMAC__
        else
        {
            event.Skip();
        }
#endif
    }
#ifdef __WXMAC__
    else
    {
        event.Skip();
    }
#else
    event.Skip();
#endif
}

/// Ends a resize action, or for live update, resizes the sash
bool wxAuiManager::DoEndResizeAction(wxMouseEvent& event)
{
    // resize the dock or the pane
    if (m_actionPart && m_actionPart->type==wxAuiDockUIPart::typeDockSizer)
    {
        // first, we must calculate the maximum size the dock may be
        int sashSize = m_art->GetMetric(wxAUI_DOCKART_SASH_SIZE);

        int used_width = 0, used_height = 0;

        wxSize client_size = m_frame->GetClientSize();

        size_t dock_i, dock_count = m_docks.GetCount();
        for (dock_i = 0; dock_i < dock_count; ++dock_i)
        {
            wxAuiDockInfo& dock = m_docks.Item(dock_i);
            if (dock.dock_direction == wxAUI_DOCK_TOP ||
                dock.dock_direction == wxAUI_DOCK_BOTTOM)
            {
                used_height += dock.size;
            }
            if (dock.dock_direction == wxAUI_DOCK_LEFT ||
                dock.dock_direction == wxAUI_DOCK_RIGHT)
            {
                used_width += dock.size;
            }
            if (dock.resizable)
                used_width += sashSize;
        }


        int available_width = client_size.GetWidth() - used_width;
        int available_height = client_size.GetHeight() - used_height;


#if wxUSE_STATUSBAR
        // if there's a status control, the available
        // height decreases accordingly
        if (m_frame && wxDynamicCast(m_frame, wxFrame))
        {
            wxFrame* frame = static_cast<wxFrame*>(m_frame);
            wxStatusBar* status = frame->GetStatusBar();
            if (status)
            {
                wxSize status_client_size = status->GetClientSize();
                available_height -= status_client_size.GetHeight();
            }
        }
#endif

        wxRect& rect = m_actionPart->dock->rect;

        wxPoint new_pos(event.m_x - m_actionOffset.x,
            event.m_y - m_actionOffset.y);
        int new_size, old_size = m_actionPart->dock->size;

        switch (m_actionPart->dock->dock_direction)
        {
        case wxAUI_DOCK_LEFT:
            new_size = new_pos.x - rect.x;
            if (new_size-old_size > available_width)
                new_size = old_size+available_width;
            m_actionPart->dock->size = new_size;
            break;
        case wxAUI_DOCK_TOP:
            new_size = new_pos.y - rect.y;
            if (new_size-old_size > available_height)
                new_size = old_size+available_height;
            m_actionPart->dock->size = new_size;
            break;
        case wxAUI_DOCK_RIGHT:
            new_size = rect.x + rect.width - new_pos.x -
                       m_actionPart->rect.GetWidth();
            if (new_size-old_size > available_width)
                new_size = old_size+available_width;
            m_actionPart->dock->size = new_size;
            break;
        case wxAUI_DOCK_BOTTOM:
            new_size = rect.y + rect.height -
                new_pos.y - m_actionPart->rect.GetHeight();
            if (new_size-old_size > available_height)
                new_size = old_size+available_height;
            m_actionPart->dock->size = new_size;
            break;
        }

        Update();
        Repaint(NULL);
    }
    else if (m_actionPart &&
        m_actionPart->type == wxAuiDockUIPart::typePaneSizer)
    {
        wxAuiDockInfo& dock = *m_actionPart->dock;
        wxAuiPaneInfo* pane = m_actionPart->pane;

        // If the pane is part of a notebook then its window might not currently be the visible one within the notebook
        // So we must then find which window in the notebook is the visible one and use the pane for that instead.
        if(!pane->GetWindow()->IsShown())
        {
            int i;
            int dock_pane_count = dock.panes.GetCount();
            // First find where in the panes array our current pane is
            for (i = 0; i < dock_pane_count; ++i)
            {
                wxAuiPaneInfo& p = *dock.panes.Item(i);
                if (p.GetWindow() == pane->GetWindow())
                {
                    // Our current pane will be the last pane in the notebook,
                    // so now search backwards through the notebook till we find the visible pane.
                    int j;
                    for (j = i - 1; j >= 0; --j)
                    {
                        if(dock.panes.Item(j)->GetWindow()->IsShown() && dock.panes.Item(j)->GetPosition() == pane->GetPosition())
                        {
                            pane = dock.panes.Item(j);
                            break;
                        }
                    }
                    break;
                }
            }
        }

        int total_proportion = 0;
        int dock_pixels = 0;
        int new_pixsize = 0;

        int caption_size = m_art->GetMetric(wxAUI_DOCKART_CAPTION_SIZE);
        int pane_borderSize = m_art->GetMetric(wxAUI_DOCKART_PANE_BORDER_SIZE);
        int sashSize = m_art->GetMetric(wxAUI_DOCKART_SASH_SIZE);

        wxPoint new_pos(event.m_x - m_actionOffset.x,
            event.m_y - m_actionOffset.y);

        // determine the pane rectangle by getting the pane part
        wxAuiDockUIPart* pane_part = GetPanePart(pane->window);
        wxASSERT_MSG(pane_part,
            wxT("Pane border part not found -- shouldn't happen"));

        // determine the new pixel size that the user wants;
        // this will help us recalculate the pane's proportion
        if (dock.IsHorizontal())
            new_pixsize = new_pos.x - pane_part->rect.x;
        else
            new_pixsize = new_pos.y - pane_part->rect.y;

        // determine the size of the dock, based on orientation
        if (dock.IsHorizontal())
            dock_pixels = dock.rect.GetWidth();
        else
            dock_pixels = dock.rect.GetHeight();

        // determine the total proportion of all resizable panes,
        // and the total size of the dock minus the size of all
        // the fixed panes
        int i, dock_pane_count = dock.panes.GetCount();
        int pane_position = -1;
        for (i = 0; i < dock_pane_count; ++i)
        {
            wxAuiPaneInfo& p = *dock.panes.Item(i);
            if (p.window == pane->window)
                pane_position = i;

            // Don't include all the panes from a notebook in the calculation, only the first one.
            if(i==0 || !AreInSameNotebook(p, *dock.panes.Item(i-1)))
            {
                // If this pane is part of a notebook then subtract the height of the notebook tabs as well.
                if(i<dock_pane_count-1 && AreInSameNotebook(p, *dock.panes.Item(i+1)))
                {
                    dock_pixels-=notebookTabHeight;
                }
                // while we're at it, subtract the pane sash
                // width from the dock width, because this would
                // skew our proportion calculations
                if (i > 0)
                    dock_pixels -= sashSize;

                // also, the whole size (including decorations) of
                // all fixed panes must also be subtracted, because they
                // are not part of the proportion calculation
                if (p.IsFixed())
                {
                    if (dock.IsHorizontal())
                        dock_pixels -= p.GetBestSize().x;
                    else
                        dock_pixels -= p.GetBestSize().y;
                }
                else
                {
                    total_proportion += p.GetProportion();
                }
            }
        }

        // new size can never be more than the number of dock pixels
        if (new_pixsize > dock_pixels)
            new_pixsize = dock_pixels;


        // find a pane in our dock to 'steal' space from or to 'give'
        // space to -- this is essentially what is done when a pane is
        // resized; the pane should usually be the first non-fixed pane
        // to the right of the action pane
        int borrow_pane = -1;
        for (i = pane_position+1; i < dock_pane_count; ++i)
        {
            wxAuiPaneInfo& p = *dock.panes.Item(i);
            // We must not select a pane from the same notebook as the 'lender' to 'steal' space from. (If the 'lender' is in a notebook)
            if (!p.IsFixed() && !AreInSameNotebook(p, *dock.panes.Item(i-1)))
            {
                borrow_pane = i;
                break;
            }
        }


        // demand that the pane being resized is found in this dock
        // (this assert really never should be raised)
        wxASSERT_MSG(pane_position != -1, wxT("Pane not found in dock"));

        // prevent division by zero
        if (dock_pixels == 0 || total_proportion == 0 || borrow_pane == -1)
        {
            m_action = actionNone;
            return false;
        }

        // calculate the new proportion of the pane
        int new_proportion = (new_pixsize*total_proportion)/dock_pixels;

        // default minimum size
        int min_size = 0;

        // check against the pane's minimum size, if specified. please note
        // that this is not enough to ensure that the minimum size will
        // not be violated, because the whole frame might later be shrunk,
        // causing the size of the pane to violate its minimum size
        if (pane->min_size.IsFullySpecified())
        {
            min_size = 0;

            if (pane->HasBorder())
                min_size += (pane_borderSize*2);

            // calculate minimum size with decorations (border,caption)
            if (pane_part->orientation == wxVERTICAL)
            {
                min_size += pane->min_size.y;
                if (pane->HasCaption())
                    min_size += caption_size;
            }
            else
            {
                min_size += pane->min_size.x;
            }
        }


        // for some reason, an arithmetic error somewhere is causing
        // the proportion calculations to always be off by 1 pixel;
        // for now we will add the 1 pixel on, but we really should
        // determine what's causing this.
        min_size++;

        int min_proportion = (min_size*total_proportion)/dock_pixels;

        if (new_proportion < min_proportion)
            new_proportion = min_proportion;



        int prop_diff = new_proportion - pane->dock_proportion;

        // borrow the space from our neighbor pane to the
        // right or bottom (depending on orientation);
        // also make sure we don't make the neighbor too small
        int prop_borrow = dock.panes.Item(borrow_pane)->dock_proportion;

        if (prop_borrow - prop_diff < 0)
        {
            // borrowing from other pane would make it too small,
            // so cancel the resize operation
            prop_borrow = min_proportion;
        }
         else
        {
            prop_borrow -= prop_diff;
        }


        dock.panes.Item(borrow_pane)->dock_proportion = prop_borrow;
        // If the above proportion change was to a pane in a notebook then all other panes in the notebook
        // must have the same change, so we do this below.
        for(i=borrow_pane-1;i>0;i--)
        {
            if(AreInSameNotebook(*dock.panes.Item(borrow_pane), *dock.panes.Item(i)))
            {
                dock.panes.Item(i)->Proportion(prop_borrow);
            }
            else
            {
                break;
            }
        }
        for(i=borrow_pane+1;i<dock_pane_count;i++)
        {
            if(AreInSameNotebook(*dock.panes.Item(borrow_pane), *dock.panes.Item(i)))
            {
                dock.panes.Item(i)->Proportion(prop_borrow);
            }
            else
            {
                break;
            }
        }
        pane->Proportion(new_proportion);
        // If the above proportion change was to a pane in a notebook then all other panes in the notebook
        // must have the same change, so we do this below.
        for(i=pane_position-1;i>0;i--)
        {
            if(AreInSameNotebook(*pane, *dock.panes.Item(i)))
            {
                dock.panes.Item(i)->Proportion(new_proportion);
            }
            else
            {
                break;
            }
        }
        for(i=pane_position+1;i<dock_pane_count;i++)
        {
            if(AreInSameNotebook(*pane, *dock.panes.Item(i)))
            {
                dock.panes.Item(i)->Proportion(new_proportion);
            }
            else
            {
                break;
            }
        }

        // repaint
        Update();
        Repaint(NULL);
    }

    return true;
}

void wxAuiManager::OnLeftUp(wxMouseEvent& event)
{
    if (m_action == actionResize)
    {
        m_frame->ReleaseMouse();

        if (!wxAuiManager_HasLiveResize(*this))
        {
            // get rid of the hint rectangle
            wxScreenDC dc;
            DrawResizeHint(dc, m_actionHintRect);
        }
        if (m_currentDragItem != -1 && wxAuiManager_HasLiveResize(*this))
            m_actionPart = & (m_uiParts.Item(m_currentDragItem));

        DoEndResizeAction(event);

        m_currentDragItem = -1;

    }
    else if (m_action == actionClickButton)
    {
        m_hoverButton = NULL;
        m_frame->ReleaseMouse();
        m_hoverButton = NULL;

        if (m_actionPart)
        {
            wxAuiPaneInfo& pane = *m_actionPart->pane;

            bool passHitTest=false;
            int buttonid=0;
            if(m_actionPart->type == wxAuiDockUIPart::typePaneTab)
            {
                if (m_hoverButton2) { // Enter only if the pointer has not already fall off the action button

                    wxAuiTabContainerButton* hitbutton;
                    if(m_actionPart->m_tab_container->ButtonHitTest(event.m_x,event.m_y,&hitbutton))
                    {
                        passHitTest=( (hitbutton==m_hoverButton2) && (m_hoverButton2->curState==wxAUI_BUTTON_STATE_PRESSED) );
                    }

                    buttonid=m_hoverButton2->id;
                    m_hoverButton2->curState = wxAUI_BUTTON_STATE_NORMAL;
                    m_hoverButton2=NULL;

                }
            }
            else
            {
                UpdateButtonOnScreen(m_actionPart, event);
                passHitTest = (m_actionPart == HitTest(event.GetX(), event.GetY()));
                buttonid=m_actionPart->button->button_id;
            }
            if (passHitTest)
            {
                // If we are a wxAuiNotebook then we must fire off a wxEVT_AUINOTEBOOK_BUTTON event to notify user of change.
                if(wxDynamicCast(GetManagedWindow(),wxAuiNotebook))
                {
                    wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_BUTTON, GetManagedWindow()->GetId());
                    e.SetSelection(GetAllPanes().Index(pane));
                    e.SetInt(buttonid);
                    e.SetEventObject(GetManagedWindow());
                    GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
                }

                // fire button-click event
                wxAuiManagerEvent e(wxEVT_AUI_PANE_BUTTON);
                e.SetManager(this);

                e.SetPane(&pane);
                e.SetButton(buttonid);
                ProcessMgrEvent(e);
            }
            else
            {
                Update();
            }
        }
    }
    else if (m_action == actionClickCaption)
    {
        m_frame->ReleaseMouse();
    }
    else if (m_action == actionDragFloatingPane)
    {
        // If we are a wxAuiNotebook then we must fire off a wxEVT_AUINOTEBOOK_DRAG_DONE event to notify user of change.
        if(wxDynamicCast(GetManagedWindow(),wxAuiNotebook))
        {
            wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_DRAG_DONE, GetManagedWindow()->GetId());
            e.SetSelection(GetAllPanes().Index(GetPane(m_actionWindow)));
            e.SetEventObject(GetManagedWindow());
            GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
        }

        m_frame->ReleaseMouse();

        // If we are a wxAuiNotebook then we must fire off a wxEVT_AUINOTEBOOK_END_DRAG event to notify user of change.
        if(wxDynamicCast(GetManagedWindow(),wxAuiNotebook))
        {
            wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_END_DRAG, GetManagedWindow()->GetId());
            e.SetSelection(GetAllPanes().Index(GetPane(m_actionWindow)));
            e.SetEventObject(GetManagedWindow());
            GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
        }
    }
    else if (m_action == actionDragMovablePane)
    {
        // If we are a wxAuiNotebook then we must fire off a wxEVT_AUINOTEBOOK_DRAG_DONE event to notify user of change.
        if(wxDynamicCast(GetManagedWindow(),wxAuiNotebook))
        {
            wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_DRAG_DONE, GetManagedWindow()->GetId());
            e.SetSelection(GetAllPanes().Index(GetPane(m_actionWindow)));
            e.SetEventObject(GetManagedWindow());
            GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
        }

        // Try to find the pane.
        wxAuiPaneInfo& pane = GetPane(m_actionWindow);
        wxASSERT_MSG(pane.IsOk(), wxT("Pane window not found"));

        // Hide the hint as it is no longer needed.
        HideHint();

        // Move the pane to new position.
        wxPoint screenPt = ::wxGetMousePosition();
        wxPoint clientPt = m_frame->ScreenToClient(screenPt);

        wxWindow* targetCtrl = ::wxFindWindowAtPoint(screenPt);

        // Make sure we have a target ctrl (it can be NULL if e.g. drop is outside of program window)
        if(targetCtrl)
        {
                // If we are on top of a hint window then quickly hide the hint window and get the window that is underneath it.
                if(targetCtrl==m_hintWnd)
                {
                    m_hintWnd->Hide();
                    targetCtrl = ::wxFindWindowAtPoint(screenPt);
                    m_hintWnd->Show();
                }
                // Find the manager this window belongs to (if it does belong to one)
                wxAuiManager* otherMgr = NULL;
                while(targetCtrl)
                {
                    if(!wxDynamicCast(targetCtrl,wxAuiFloatingFrame))
                    {
                        if(targetCtrl->GetEventHandler() && wxDynamicCast(targetCtrl->GetEventHandler(),wxAuiManager))
                        {
                            otherMgr = ((wxAuiManager*)targetCtrl->GetEventHandler());
                            break;
                        }
                    }
                    targetCtrl = targetCtrl->GetParent();
                }

                bool didDrop=false;

                //Store paneWindow now as the below code block can invalidate pane
                wxWindow* paneWindow=pane.GetWindow();

                // Alert other manager of the drop and have it show hint.
                if(otherMgr)
                {
                    if(otherMgr != this)
                    {
                        didDrop = DoDropExternal(otherMgr, targetCtrl, pane, screenPt, wxPoint(0,0));

                        //Update ourselves here, the next block of code will update the target control
                        if(didDrop)
                        {
                            Update();
                            DoFrameLayout();
                            Repaint();
                        }
                    }
                    else
                    {
                        didDrop = DoDrop(m_docks, m_panes, pane, clientPt, wxPoint(0,0));
                    }
                }

                //Warning! pane can be invalidated by the Update in above code block, so should not be used from this point onwards.
                if(didDrop)
                {
                    // Try reduce flicker, Update() calls Repaint() and then we set the active pane and Repaint() again, so use Freeze()/Thaw() to try avoid the double Repaint()
                    otherMgr->GetManagedWindow()->Freeze();

                    // Update the layout to realize new position and e.g. form notebooks if needed.
                    otherMgr->Update();

                    // If a notebook formed we may have lost our active status so set it again.
                    otherMgr->SetActivePane(paneWindow);

                    // Allow the updated layout an opportunity to recalculate/update the pane positions.
                    otherMgr->DoFrameLayout();

                    // Make changes visible to user.
                    otherMgr->Repaint();
                    otherMgr->GetManagedWindow()->Thaw();
                }
        }



        // Cancel the action and release the mouse.
        m_action = actionNone;
        m_frame->ReleaseMouse();
        m_actionWindow = NULL;
        delete m_actionDeadZone;
        m_actionDeadZone = NULL;

        // If we are a wxAuiNotebook then we must fire off a wxEVT_AUINOTEBOOK_END_DRAG event to notify user of change.
        if(wxDynamicCast(GetManagedWindow(),wxAuiNotebook))
        {
            wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_END_DRAG, GetManagedWindow()->GetId());
            e.SetSelection(GetAllPanes().Index(GetPane(m_actionWindow)));
            e.SetEventObject(GetManagedWindow());
            GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
        }
    }
    else if (m_action == actionDragToolbarPane)
    {
        m_frame->ReleaseMouse();

        wxAuiPaneInfo& pane = GetPane(m_actionWindow);
        wxASSERT_MSG(pane.IsOk(), wxT("Pane window not found"));

        // save the new positions
        wxAuiDockInfoPtrArray docks;
        FindDocks(m_docks, pane.dock_direction,
                  pane.dock_layer, pane.dock_row, docks);
        if (docks.GetCount() == 1)
        {
            wxAuiDockInfo& dock = *docks.Item(0);

            wxArrayInt pane_positions, pane_sizes;
            GetPanePositionsAndSizes(dock, pane_positions, pane_sizes);

            int i, dock_pane_count = dock.panes.GetCount();
            for (i = 0; i < dock_pane_count; ++i)
                dock.panes.Item(i)->dock_pos = pane_positions[i];
        }

        pane.state &= ~wxAuiPaneInfo::actionPane;
        Update();
    }
    else
    {
        event.Skip();
    }

    m_action = actionNone;
    m_lastMouseMove = wxPoint(); // see comment in OnMotion()
    delete m_actionDeadZone;
    m_actionDeadZone = NULL;
}

void wxAuiManager::OnRightDown(wxMouseEvent& evt)
{
    wxAuiDockUIPart* part = HitTest(evt.GetX(), evt.GetY());
    if(part)
    {
        if(part->type == wxAuiDockUIPart::typePaneTab)
        {
            wxAuiPaneInfo* hitPane;
            if(part->m_tab_container->TabHitTest(evt.m_x,evt.m_y,&hitPane))
            {
                wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_TAB_RIGHT_DOWN, GetManagedWindow()->GetId());
                e.SetEventObject(GetManagedWindow());
                e.SetSelection(GetAllPanes().Index(*hitPane));
                GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
            }
        }
    }
}

void wxAuiManager::OnRightUp(wxMouseEvent& evt)
{
    wxAuiDockUIPart* part = HitTest(evt.GetX(), evt.GetY());
    if(part)
    {
        if(part->type == wxAuiDockUIPart::typePaneTab)
        {
            wxAuiPaneInfo* hitPane;
            if(part->m_tab_container->TabHitTest(evt.m_x,evt.m_y,&hitPane))
            {
                wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_TAB_RIGHT_UP, GetManagedWindow()->GetId());
                e.SetEventObject(GetManagedWindow());
                e.SetSelection(GetAllPanes().Index(*hitPane));
                GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
            }
        }
    }
}

void wxAuiManager::OnMiddleDown(wxMouseEvent& evt)
{
    wxAuiDockUIPart* part = HitTest(evt.GetX(), evt.GetY());
    if(part)
    {
        if(part->type == wxAuiDockUIPart::typePaneTab)
        {
            wxAuiPaneInfo* hitPane;
            if(part->m_tab_container->TabHitTest(evt.m_x,evt.m_y,&hitPane))
            {
                wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_TAB_MIDDLE_DOWN, GetManagedWindow()->GetId());
                e.SetEventObject(GetManagedWindow());
                e.SetSelection(GetAllPanes().Index(*hitPane));
                GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
            }
        }
    }
}

void wxAuiManager::OnMiddleUp(wxMouseEvent& evt)
{

    // if the wxAUI_NB_MIDDLE_CLICK_CLOSE is specified, middle
    // click should act like a tab close action.  However, first
    // give the owner an opportunity to handle the middle up event
    // for custom action

    wxAuiDockUIPart* part = HitTest(evt.GetX(), evt.GetY());
    if(!part||!part->pane)
    {
        evt.Skip();
        return;
    }
    if (part && part->type == wxAuiDockUIPart::typePaneTab)
    {
        wxAuiPaneInfo* hitPane=NULL;
        if(part->m_tab_container->TabHitTest(evt.GetX(),evt.GetY(),&hitPane))
        {
            part->pane = hitPane;
        }
    }

    // check if we are supposed to close on middle-up
    if (!HasFlag(wxAUI_MGR_MIDDLE_CLICK_CLOSE) == 0)
        return;


    // If we are a wxAuiNotebook then we must fire off a wxEVT_AUINOTEBOOK_TAB_MIDDLE_UP event and give the user an opportunity to veto it.
    {
        wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_TAB_MIDDLE_UP, GetManagedWindow()->GetId());
        e.SetSelection(GetAllPanes().Index(*part->pane));
        e.SetEventObject(GetManagedWindow());
        if (GetManagedWindow()->GetEventHandler()->ProcessEvent(e))
            return;
        if (!e.IsAllowed())
            return;
    }

    // simulate the user pressing the close button on the tab
    {
        wxAuiManagerEvent e(wxEVT_AUI_PANE_BUTTON);
        e.SetEventObject(this);
        e.SetPane(part->pane);
        e.SetButton(wxAUI_BUTTON_CLOSE);
        OnPaneButton(e);
    }
}


void wxAuiManager::ShowToolTip(wxAuiPaneInfo* pane)
{
    #if wxUSE_TOOLTIPS
    wxString toolTip(pane->GetToolTip());
    if(toolTip==wxT(""))
    {
        HideToolTip();
    }
    else
    {
        // If the text changes, set it else, keep old, to avoid
        // 'moving tooltip' effect
        if(m_frame->GetToolTipText() != toolTip)
            m_frame->SetToolTip(toolTip);
    }
    #endif
}

void wxAuiManager::HideToolTip()
{
    #if wxUSE_TOOLTIPS
    m_frame->UnsetToolTip();
    #endif
}

void wxAuiManager::OnMotion(wxMouseEvent& event)
{
    // sometimes when Update() is called from inside this method,
    // a spurious mouse move event is generated; this check will make
    // sure that only real mouse moves will get anywhere in this method;
    // this appears to be a bug somewhere, and I don't know where the
    // mouse move event is being generated.  only verified on MSW

    wxPoint mouse_pos = event.GetPosition();
    if (m_lastMouseMove == mouse_pos)
        return;
    m_lastMouseMove = mouse_pos;

    if(m_actionDeadZone)
    {
        if(m_actionDeadZone->Contains(mouse_pos))
        {
            return;
        }
        else
        {
            delete m_actionDeadZone;
            m_actionDeadZone = NULL;
        }
    }


    if (m_action == actionResize)
    {
        HideToolTip();

        // It's necessary to reset m_actionPart since it destroyed
        // by the Update within DoEndResizeAction.
        if (m_currentDragItem != -1)
            m_actionPart = & (m_uiParts.Item(m_currentDragItem));
        else
            m_currentDragItem = m_uiParts.Index(* m_actionPart);

        if (m_actionPart)
        {
            wxPoint pos = m_actionPart->rect.GetPosition();
            if (m_actionPart->orientation == wxHORIZONTAL)
                pos.y = wxMax(0, event.m_y - m_actionOffset.y);
            else
                pos.x = wxMax(0, event.m_x - m_actionOffset.x);

            if (wxAuiManager_HasLiveResize(*this))
            {
                bool capture = m_frame->HasCapture();
                if (capture)
                    m_frame->ReleaseMouse();
                DoEndResizeAction(event);
                if (capture)
                    m_frame->CaptureMouse();
            }
            else
            {
                wxRect rect(m_frame->ClientToScreen(pos),
                    m_actionPart->rect.GetSize());
                wxScreenDC dc;

                if (!m_actionHintRect.IsEmpty())
                {
                    // remove old resize hint
                    DrawResizeHint(dc, m_actionHintRect);
                    m_actionHintRect = wxRect();
                }

                // draw new resize hint, if it's inside the managed frame
                wxRect frameScreenRect = m_frame->GetScreenRect();
                if (frameScreenRect.Contains(rect))
                {
                    DrawResizeHint(dc, rect);
                    m_actionHintRect = rect;
                }
            }
        }
    }
    else if (m_action == actionClickCaption)
    {
        HideToolTip();

        int drag_x_threshold = wxSystemSettings::GetMetric(wxSYS_DRAG_X);
        int drag_y_threshold = wxSystemSettings::GetMetric(wxSYS_DRAG_Y);

        // caption has been clicked.  we need to check if the mouse
        // is now being dragged. if it is, we need to change the
        // mouse action to 'drag'
        if (m_actionPart &&
            (abs(event.m_x - m_actionStart.x) > drag_x_threshold ||
             abs(event.m_y - m_actionStart.y) > drag_y_threshold))
        {
            wxAuiPaneInfo* paneInfo = m_actionPart->pane;

            if (!paneInfo->IsToolbar())
            {
                // If we are a wxAuiNotebook then we must fire off a wxEVT_AUINOTEBOOK_BEGIN_DRAG event to notify user of change.
                if(wxDynamicCast(GetManagedWindow(),wxAuiNotebook))
                {
                    wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_BEGIN_DRAG, GetManagedWindow()->GetId());
                    e.SetSelection(GetAllPanes().Index(*paneInfo));
                    e.SetEventObject(GetManagedWindow());
                    GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
                }

                // If it is a notebook pane then we don't want to float it unless we have left the notebook tab bar.
                // Instead we want to move it around within the notebook.
                if(m_actionPart->type==wxAuiDockUIPart::typePaneTab)
                {
                    wxAuiDockUIPart* hitpart = HitTest(event.GetX(), event.GetY());
                    if(m_actionPart==hitpart)
                    {
                        wxAuiPaneInfo comp = *paneInfo;
                        DoDrop(m_docks, m_panes, *paneInfo, mouse_pos, wxPoint(0,0));
                        // Only call update if the pane has actually moved within the notebook to prevent flickering.
                        if(comp.GetPage() != paneInfo->GetPage())
                        {
                            Update();
                            Repaint();
                            m_actionPart = HitTest(event.GetX(), event.GetY());
                            m_actionPart->pane = paneInfo;
                            // Reset position of 'action start' otherwise we run the possibility of tabs 'jumping'
                            mouse_pos.x=event.m_x;
                            mouse_pos.y=event.m_y;
                            // If tabs are differently sized then we need to set a 'dead' zone in order to prevent the tab immediately switching around again on next mouse move event
                            if(!HasFlag(wxAUI_MGR_NB_TAB_FIXED_WIDTH))
                            {
                                wxAuiPaneInfo*     hitPane   = NULL;
                                wxAuiTabContainer *container = m_actionPart->m_tab_container;
                                container->TabHitTest(event.GetX(), event.GetY(),&hitPane);
                                if( hitPane && hitPane->GetPage() != paneInfo->GetPage() )
                                {
                                    // The dead zone is set to the area of the tab that has just exchanged it's position with the dragged one
                                    wxRect deadZone = hitPane->GetRect();
                                    // The pane rectangle is the relative position of the tab area in the tab container, so we have to offset it
                                    deadZone.Offset(container->GetRect().GetTopLeft());
                                    // We finally reduce it to the zone between the current mouse position and the dragged tab (with a small margin)
                                    if (container->IsHorizontal()) {
                                        if (hitPane->GetPage() > paneInfo->GetPage())
                                            deadZone.SetRight(mouse_pos.x+5);
                                        else
                                            deadZone.SetLeft(mouse_pos.x-5);
                                    } else {
                                        if (hitPane->GetPage() > paneInfo->GetPage())
                                            deadZone.SetBottom(mouse_pos.y+5);
                                        else
                                            deadZone.SetTop(mouse_pos.y-5);
                                    }

                                    m_actionDeadZone = new wxRect(deadZone);
                                }
                            }
                        }
                        return;
                    }
                }

                if (HasFlag(wxAUI_MGR_ALLOW_FLOATING) && paneInfo->IsFloatable())
                {
                    m_action = actionDragFloatingPane;

                    // set initial float position
                    wxPoint pt = m_frame->ClientToScreen(event.GetPosition());
                    paneInfo->floating_pos = wxPoint(pt.x - m_actionOffset.x,
                                                      pt.y - m_actionOffset.y);

                    if(m_actionPart->type == wxAuiDockUIPart::typePaneTab)
                    {
                        m_actionPart->m_tab_container->RemovePage(paneInfo->GetWindow());
                    }
                    // float the window
                    if (paneInfo->IsMaximized())
                        RestorePane(*paneInfo);
                    paneInfo->Float();
                    Update();

                    m_actionWindow = paneInfo->frame;

                    // action offset is used here to make it feel "natural" to the user
                    // to drag a docked pane and suddenly have it become a floating frame.
                    // Sometimes, however, the offset where the user clicked on the docked
                    // caption is bigger than the width of the floating frame itself, so
                    // in that case we need to set the action offset to a sensible value
                    wxSize frame_size = m_actionWindow->GetSize();
                    if (frame_size.x <= m_actionOffset.x)
                        m_actionOffset.x = 30;
                }
                else if(paneInfo->IsMovable())
                {
                    m_action = actionDragMovablePane;
                    m_actionWindow = paneInfo->GetWindow();
                }
            }
            else
            {
                m_action = actionDragToolbarPane;
                m_actionWindow = paneInfo->window;
            }
        }
    }
    else if (m_action == actionDragFloatingPane)
    {
        HideToolTip();

        if (m_actionWindow)
        {
            // We can't move the child window so we need to get the frame that
            // we want to be really moving. This is probably not the best place
            // to do this but at least it fixes the bug (#13177) for now.
            if (!wxDynamicCast(m_actionWindow, wxAuiFloatingFrame))
            {
                wxAuiPaneInfo& pane = GetPane(m_actionWindow);
                m_actionWindow = pane.frame;
            }

            wxPoint pt = m_frame->ClientToScreen(event.GetPosition());
            m_actionWindow->Move(pt.x - m_actionOffset.x,
                                pt.y - m_actionOffset.y);

            // If we are a wxAuiNotebook then we must fire off a wxEVT_AUINOTEBOOK_DRAG_MOTION event to notify user of change.
            if(wxDynamicCast(GetManagedWindow(),wxAuiNotebook))
            {
                wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_DRAG_MOTION, GetManagedWindow()->GetId());
                e.SetSelection(GetAllPanes().Index(GetPane(m_actionWindow)));
                e.SetEventObject(GetManagedWindow());
                GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
            }
        }
    }
    else if (m_action == actionDragMovablePane)
    {
        HideToolTip();

        // If the movable pane is part of a notebook and the mouse has moved back over the notebook tabs then we set the
        // event type back to a caption click so that we can move the actual tab around instead of drawing hints.
        if(m_actionPart && m_actionPart->type==wxAuiDockUIPart::typePaneTab)
        {
            wxAuiDockUIPart* hitpart = HitTest(event.GetX(), event.GetY());
            if(m_actionPart==hitpart)
            {
                HideHint();
                m_action = actionClickCaption;
                m_actionWindow = NULL;
                return;
            }
        }

        // Try to find the pane.
        wxAuiPaneInfo& pane = GetPane(m_actionWindow);
        wxASSERT_MSG(pane.IsOk(), wxT("Pane window not found"));

        // Draw a hint for where the window will be moved.
        wxPoint pt = event.GetPosition();
        DrawHintRect(m_actionWindow, pt, wxPoint(0,0));

        // Reduces flicker.
        m_frame->Update();

        // If we are a wxAuiNotebook then we must fire off a wxEVT_AUINOTEBOOK_DRAG_MOTION event to notify user of change.
        if(wxDynamicCast(GetManagedWindow(),wxAuiNotebook))
        {
            wxAuiNotebookEvent e(wxEVT_AUINOTEBOOK_DRAG_MOTION, GetManagedWindow()->GetId());
            e.SetSelection(GetAllPanes().Index(GetPane(m_actionWindow)));
            e.SetEventObject(GetManagedWindow());
            GetManagedWindow()->GetEventHandler()->ProcessEvent(e);
        }
    }
    else if (m_action == actionDragToolbarPane)
    {
        HideToolTip();

        wxAuiPaneInfo& pane = GetPane(m_actionWindow);
        wxASSERT_MSG(pane.IsOk(), wxT("Pane window not found"));

        pane.SetFlag(wxAuiPaneInfo::actionPane, true);

        wxPoint point = event.GetPosition();
        DoDrop(m_docks, m_panes, pane, point, m_actionOffset);

        // if DoDrop() decided to float the pane, set up
        // the floating pane's initial position
        if (pane.IsFloating())
        {
            wxPoint pt = m_frame->ClientToScreen(event.GetPosition());
            pane.floating_pos = wxPoint(pt.x - m_actionOffset.x,
                                        pt.y - m_actionOffset.y);
        }

        // this will do the actual move operation;
        // in the case that the pane has been floated,
        // this call will create the floating pane
        // and do the reparenting
        Update();

        // if the pane has been floated, change the mouse
        // action actionDragFloatingPane so that subsequent
        // EVT_MOTION() events will move the floating pane
        if (pane.IsFloating())
        {
            pane.state &= ~wxAuiPaneInfo::actionPane;
            m_action = actionDragFloatingPane;
            m_actionWindow = pane.frame;
        }
    }
    else
    {
        wxAuiDockUIPart* part = HitTest(event.GetX(), event.GetY());
        if (part)
        {
            //For caption(s) and tab(s) set tooltip if one is present, for everything else cancel any existing tooltip.
            if(part->type == wxAuiDockUIPart::typePaneTab)
            {
                wxAuiPaneInfo* hitPane=NULL;
                if(part->m_tab_container->TabHitTest(event.GetX(), event.GetY(), &hitPane))
                {
                    ShowToolTip(hitPane);
                }
                else
                {
                    HideToolTip();
                }

                wxAuiTabContainerButton* hitbutton;
                if(part->m_tab_container->ButtonHitTest(event.m_x,event.m_y,&hitbutton))
                {
                    // make the old button normal
                    if (m_hoverButton)
                    {
                        UpdateButtonOnScreen(m_hoverButton, event);
                        Repaint();
                    }
                    if(m_hoverButton2 != hitbutton)
                    {
                        if (m_hoverButton2)
                        {
                            m_hoverButton2->curState = wxAUI_BUTTON_STATE_NORMAL;
                            Repaint();
                        }

                        m_hoverButton2 = hitbutton;
                        m_hoverButton2->curState = wxAUI_BUTTON_STATE_HOVER;
                        Repaint();
                    }

                    return;
                }
            }
            else if(part->type == wxAuiDockUIPart::typeCaption)
            {
                ShowToolTip(part->pane);
            }
            else
            {
                HideToolTip();

                //Handle other part types as appropriate.
                if(part->type == wxAuiDockUIPart::typePaneButton)
                {
                    if (m_hoverButton2)
                    {
                        m_hoverButton2->curState = wxAUI_BUTTON_STATE_NORMAL;
                        Repaint();
                    }
                    if (part != m_hoverButton)
                    {
                        // make the old button normal
                        if (m_hoverButton)
                        {
                            UpdateButtonOnScreen(m_hoverButton, event);
                            Repaint();
                        }

                        // mouse is over a button, so repaint the
                        // button in hover mode
                        UpdateButtonOnScreen(part, event);
                        m_hoverButton = part;
                    }
                    return;
                }
            }
        }
        else
        {
            HideToolTip();
        }

        if (m_hoverButton||m_hoverButton2)
        {
            m_hoverButton = NULL;
            if(m_hoverButton2)
            {
                m_hoverButton2->curState = wxAUI_BUTTON_STATE_NORMAL;
            }
            Repaint();
            m_hoverButton2 = NULL;
        }
        else
        {
            event.Skip();
        }
    }
}

void wxAuiManager::OnLeaveWindow(wxMouseEvent& event)
{
    HideToolTip();

    if (m_hoverButton || m_hoverButton2)
    {
		if (m_hoverButton) {
			UpdateButtonOnScreen(m_hoverButton, event);
			m_hoverButton = NULL;
		}

		if (m_hoverButton2) {
			m_hoverButton2->curState = wxAUI_BUTTON_STATE_NORMAL;
			m_hoverButton2 = NULL;
		}

        Repaint();
    }
}

void wxAuiManager::OnCaptureLost(wxMouseCaptureLostEvent& WXUNUSED(event))
{
    HideToolTip();

    // cancel the operation in progress, if any
    if ( m_action != actionNone )
    {
        m_action = actionNone;
        HideHint();
        delete m_actionDeadZone;
        m_actionDeadZone = NULL;
    }
}

void wxAuiManager::OnChildFocus(wxChildFocusEvent& event)
{
    bool refresh = false;
    // Ensure none of the notebook controls have focus set
    unsigned int i;
    for (i = 0; i < m_uiParts.GetCount(); i++)
    {
        wxAuiDockUIPart& part = m_uiParts.Item(i);
        if(part.m_tab_container)
        {
            if(part.m_tab_container->HasFocus())
            {
                part.m_tab_container->SetFocus(false);
                refresh = true;
            }
        }
    }

    // when a child pane has its focus set, we should change the
    // pane's active state to reflect this. (this is only true if
    // active panes are allowed by the owner)

    wxAuiPaneInfo& pane = GetPane(event.GetWindow());
    if (pane.IsOk() && !pane.HasFlag(wxAuiPaneInfo::optionActive))
    {
        SetActivePane(event.GetWindow());
        if (HasFlag(wxAUI_MGR_ALLOW_ACTIVE_PANE))
            refresh = true;
    }

    if (refresh)
    {
        m_frame->Refresh();
    }

    event.Skip();
}


// OnPaneButton() is an event handler that is called
// when a pane button has been pressed.
void wxAuiManager::OnPaneButton(wxAuiManagerEvent& evt)
{
    wxASSERT_MSG(evt.pane, wxT("Pane Info passed to wxAuiManager::OnPaneButton must be non-null"));

    wxAuiPaneInfo& pane = *(evt.pane);

    if (evt.button == wxAUI_BUTTON_CLOSE)
    {
        wxAuiPaneInfo* closepane = evt.pane;

        // If the close button is on a notebook tab then we want to close that tab and not the one from the event.
        wxPoint pt = m_actionStart;
        wxAuiDockUIPart* part = HitTest(pt.x, pt.y);
        if (part && part->type == wxAuiDockUIPart::typePaneTab)
        {
            wxAuiPaneInfo* hitPane=NULL;
            if(part->m_tab_container->TabHitTest(pt.x,pt.y,&hitPane))
            {
                closepane = hitPane;
            }
        }

        if(ClosePane(*closepane))
            Update();
    }
    else if (evt.button == wxAUI_BUTTON_MAXIMIZE_RESTORE && !pane.IsMaximized())
    {
        // fire pane maximize event
        wxAuiManagerEvent e(wxEVT_AUI_PANE_MAXIMIZE);
        e.SetManager(this);
        e.SetPane(evt.pane);
        ProcessMgrEvent(e);

        if (!e.GetVeto())
        {
            MaximizePane(pane);
            Update();
        }
    }
    else if (evt.button == wxAUI_BUTTON_MAXIMIZE_RESTORE && pane.IsMaximized())
    {
        // fire pane restore event
        wxAuiManagerEvent e(wxEVT_AUI_PANE_RESTORE);
        e.SetManager(this);
        e.SetPane(evt.pane);
        ProcessMgrEvent(e);

        if (!e.GetVeto())
        {
            RestorePane(pane);
            Update();
        }
    }
    else if (evt.button == wxAUI_BUTTON_PIN &&
                (m_flags & wxAUI_MGR_ALLOW_FLOATING) && pane.IsFloatable())
    {
        if (pane.IsMaximized())
        {
            // If the pane is maximized, the original state must be restored
            // before trying to float the pane, otherwise the other panels
            // wouldn't appear correctly when it becomes floating.
            wxAuiManagerEvent e(wxEVT_AUI_PANE_RESTORE);
            e.SetManager(this);
            e.SetPane(evt.pane);
            ProcessMgrEvent(e);

            if (e.GetVeto())
            {
                // If it can't be restored, it can't be floated neither.
                return;
            }

            RestorePane(pane);
        }

        pane.Float();
        Update();
    }
    else if (evt.button == wxAUI_BUTTON_LEFT || evt.button == wxAUI_BUTTON_UP)
    {
        m_actionPart->m_tab_container->SetTabOffset(m_actionPart->m_tab_container->GetTabOffset()-1);
        Repaint();
    }
    else if (evt.button == wxAUI_BUTTON_RIGHT || evt.button == wxAUI_BUTTON_DOWN)
    {
        m_actionPart->m_tab_container->SetTabOffset(m_actionPart->m_tab_container->GetTabOffset()+1);
        Repaint();
    }
    else if (evt.button == wxAUI_BUTTON_WINDOWLIST)
    {
        int selection = m_actionPart->m_tab_container->GetArtProvider()->ShowDropDown(m_frame, m_actionPart->m_tab_container->GetPages(), m_actionPart->m_tab_container->GetActivePage());
        if (selection != -1)
        {
            m_actionPart->m_tab_container->SetActivePage(selection);
        }
        DoFrameLayout();
        Repaint();
    }
}


bool wxAuiManager::AreInSameNotebook(const wxAuiPaneInfo &pane0, const wxAuiPaneInfo &pane1)
{
	wxAuiTabContainer* tc0 = NULL;
	wxAuiTabContainer* tc1 = NULL;
	int trash;

	return FindTab(pane0.GetWindow(), &tc0, &trash) && FindTab(pane1.GetWindow(), &tc1, &trash) && tc0 == tc1;
}


#endif // wxUSE_AUI
