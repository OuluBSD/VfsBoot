#ifndef _Explorer_Explorer_h
#define _Explorer_Explorer_h

#include <CtrlLib/CtrlLib.h>

using namespace Upp;

#define IMAGEFILE <Explorer/Explorer.iml>
#include <Draw/iml_header.h>

struct AddressBar : public CtrlFrame {
	
	void FrameLayout(Rect& r) override;
	void FrameAddSize(Size& sz) override;
};

class Explorer : public TopWindow {
	ToolBar toolbar;
	MenuBar menu;
	StatusBar status;
	AddressBar addr;
	
	Splitter main_hsplit;
	Splitter dual_view;
	
	ParentCtrl navigator_placeholder;
	ParentCtrl main_left_placeholder;
	ParentCtrl main_right_placeholder;
	
public:
	typedef Explorer CLASSNAME;
	Explorer();
	
	void Menu(Bar& bar);
	void ToolMenu(Bar& bar);
	
	
};

#endif
