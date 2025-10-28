#include "Explorer.h"


void AddressBar::FrameLayout(Rect& r) {
	// TODO
}

void AddressBar::FrameAddSize(Size& sz) {
	sz.cy += 30;
}

Explorer::Explorer()
{
	AddFrame(menu);
	AddFrame(toolbar);
	AddFrame(addr);
	AddFrame(status);
	
	toolbar.Set(THISBACK(ToolMenu));
	menu.Set(THISBACK(Menu));
	
	Add(main_hsplit.SizePos());
	
	main_hsplit.Horz();
	main_hsplit.Add(navigator_placeholder);
	main_hsplit.Add(dual_view);
	main_hsplit.SetPos(2000);
	
	dual_view.Horz();
	dual_view.Add(main_left_placeholder);
	dual_view.Add(main_right_placeholder);
	
	
}

void Explorer::ToolMenu(Bar& bar) {
	
}

void Explorer::Menu(Bar& bar) {
	
	
	bar.Sub(t_("File"), [this](Bar& bar) {
		bar.Add(t_("Exit"), [this]{this->Close();});
	});
	
	bar.Sub(t_("Edit"), [this](Bar& bar) {
		// TODO something like in windows me explorer
	});
	
	bar.Sub(t_("View"), [this](Bar& bar) {
		// TODO something like in windows me explorer
	});
	
	bar.Sub(t_("Favorites"), [this](Bar& bar) {
		// TODO something like in windows me explorer
	});
	
	bar.Sub(t_("Tools"), [this](Bar& bar) {
		// TODO something like in windows me explorer
	});
	
	bar.Sub(t_("Left"), [this](Bar& bar) {
		// TODO something like in commander style program for left area
	});
	
	bar.Sub(t_("Right"), [this](Bar& bar) {
		// TODO something like in commander style program for right area
	});
	
	bar.Sub(t_("Help"), [this](Bar& bar) {
		// TODO something like in windows me explorer
	});
	
}
