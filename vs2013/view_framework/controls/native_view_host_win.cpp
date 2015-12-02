
#include "native_view_host_win.h"

#include "base/logging.h"

#include "../focus/focus_manager.h"
#include "../widget/root_view.h"
#include "../widget/widget.h"
#include "native_view_host.h"

namespace view
{

    ////////////////////////////////////////////////////////////////////////////////
    // NativeViewHostWin, public:

    NativeViewHostWin::NativeViewHostWin(NativeViewHost* host)
        : host_(host),
        installed_clip_(false) {}

    NativeViewHostWin::~NativeViewHostWin() {}

    ////////////////////////////////////////////////////////////////////////////////
    // NativeViewHostWin, NativeViewHostWrapper implementation:
    void NativeViewHostWin::NativeViewAttached()
    {
        DCHECK(host_->native_view())
            << "Impossible detatched tab case; See crbug.com/6316";

        // First hide the new window. We don't want anything to draw (like sub-hwnd
        // borders), when we change the parent below.
        ShowWindow(host_->native_view(), SW_HIDE);

        // Need to set the HWND's parent before changing its size to avoid flashing.
        SetParent(host_->native_view(), host_->GetWidget()->GetNativeView());
        host_->Layout();
        // Notify children that parent changed, so they could adjust the focus
        std::vector<RootView*> root_views;
        Widget::FindAllRootViews(host_->native_view(), &root_views);
        for(std::vector<RootView*>::iterator it=root_views.begin();
            it<root_views.end(); ++it)
        {
            (*it)->NotifyNativeViewHierarchyChanged(true,
                host_->GetWidget()->GetNativeView());
        }
    }

    void NativeViewHostWin::NativeViewDetaching(bool destroyed)
    {
        if(!destroyed && installed_clip_)
        {
            UninstallClip();
        }
        installed_clip_ = false;
        // Notify children that parent is removed
        std::vector<RootView*> root_views;
        Widget::FindAllRootViews(host_->native_view(), &root_views);
        for(std::vector<RootView*>::iterator it=root_views.begin();
            it<root_views.end(); ++it)
        {
            (*it)->NotifyNativeViewHierarchyChanged(false,
                host_->GetWidget()->GetNativeView());
        }
    }

    void NativeViewHostWin::AddedToWidget()
    {
        if(!IsWindow(host_->native_view()))
        {
            return;
        }
        HWND parent_hwnd = GetParent(host_->native_view());
        HWND widget_hwnd = host_->GetWidget()->GetNativeView();
        if(parent_hwnd != widget_hwnd)
        {
            SetParent(host_->native_view(), widget_hwnd);
        }
        if(host_->IsVisibleInRootView())
        {
            ShowWindow(host_->native_view(), SW_SHOW);
        }
        else
        {
            ShowWindow(host_->native_view(), SW_HIDE);
        }
        host_->Layout();
    }

    void NativeViewHostWin::RemovedFromWidget()
    {
        if(!IsWindow(host_->native_view()))
        {
            return;
        }
        ShowWindow(host_->native_view(), SW_HIDE);
        SetParent(host_->native_view(), NULL);
    }

    void NativeViewHostWin::InstallClip(int x, int y, int w, int h)
    {
        HRGN clip_region = CreateRectRgn(x, y, x+w, y+h);
        // NOTE: SetWindowRgn owns the region (as well as the deleting the
        // current region), as such we don't delete the old region.
        SetWindowRgn(host_->native_view(), clip_region, TRUE);
        installed_clip_ = true;
    }

    bool NativeViewHostWin::HasInstalledClip()
    {
        return installed_clip_;
    }

    void NativeViewHostWin::UninstallClip()
    {
        SetWindowRgn(host_->native_view(), 0, TRUE);
        installed_clip_ = false;
    }

    void NativeViewHostWin::ShowWidget(int x, int y, int w, int h)
    {
        UINT swp_flags = SWP_DEFERERASE |
            SWP_NOACTIVATE |
            SWP_NOCOPYBITS |
            SWP_NOOWNERZORDER |
            SWP_NOZORDER;
        // Only send the SHOWWINDOW flag if we're invisible, to avoid flashing.
        if(!IsWindowVisible(host_->native_view()))
        {
            swp_flags = (swp_flags | SWP_SHOWWINDOW) & ~SWP_NOREDRAW;
        }

        if(host_->fast_resize())
        {
            // In a fast resize, we move the window and clip it with SetWindowRgn.
            RECT win_rect;
            GetWindowRect(host_->native_view(), &win_rect);
            gfx::Rect rect(win_rect);
            SetWindowPos(host_->native_view(), 0, x, y, rect.width(),
                rect.height(), swp_flags);

            InstallClip(0, 0, w, h);
        }
        else
        {
            SetWindowPos(host_->native_view(), 0, x, y, w, h, swp_flags);
        }
    }

    void NativeViewHostWin::HideWidget()
    {
        if(!IsWindowVisible(host_->native_view()))
        {
            return; // Currently not visible, nothing to do.
        }

        // The window is currently visible, but its clipped by another view. Hide
        // it.
        SetWindowPos(host_->native_view(), 0, 0, 0, 0, 0,
            SWP_HIDEWINDOW|SWP_NOSIZE|SWP_NOMOVE|SWP_NOZORDER|
            SWP_NOREDRAW|SWP_NOOWNERZORDER);
    }

    void NativeViewHostWin::SetFocus()
    {
        ::SetFocus(host_->native_view());
    }

    ////////////////////////////////////////////////////////////////////////////////
    // NativeViewHostWrapper, public:

    // static
    NativeViewHostWrapper* NativeViewHostWrapper::CreateWrapper(
        NativeViewHost* host)
    {
        return new NativeViewHostWin(host);
    }

} //namespace view