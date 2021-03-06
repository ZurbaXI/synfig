/* === S Y N F I G ========================================================= */
/*!	\file mainwindow.cpp
**	\brief MainWindow
**
**	$Id$
**
**	\legal
**	......... ... 2013 Ivan Mahonin
**
**	This package is free software; you can redistribute it and/or
**	modify it under the terms of the GNU General Public License as
**	published by the Free Software Foundation; either version 2 of
**	the License, or (at your option) any later version.
**
**	This package is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**	General Public License for more details.
**	\endlegal
*/
/* ========================================================================= */

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mainwindow.h"
#include "canvasview.h"
#include "docks/dockable.h"
#include "docks/dockbook.h"
#include "docks/dockmanager.h"
#include "docks/dockdroparea.h"

#include <synfigapp/main.h>

#include <gtkmm/menubar.h>
#include <gtkmm/box.h>

#include <gtkmm/inputdialog.h>

#endif

/* === U S I N G =========================================================== */

using namespace std;
using namespace etl;
using namespace synfig;
using namespace studio;

/* === M A C R O S ========================================================= */

#define GRAB_HINT_DATA(y)	{ \
		String x; \
		if(synfigapp::Main::settings().get_value(String("pref.")+y+"_hints",x)) \
		{ \
			set_type_hint((Gdk::WindowTypeHint)atoi(x.c_str()));	\
		} \
	}

/* === G L O B A L S ======================================================= */

/* === P R O C E D U R E S ================================================= */

/* === M E T H O D S ======================================================= */

MainWindow::MainWindow()
{
	set_default_size(600, 400);
	toggling_show_menubar = App::enable_mainwin_menubar;
	toggling_show_toolbar = App::enable_mainwin_toolbar;

	main_dock_book_ = manage(new DockBook());
	main_dock_book_->allow_empty = true;
	main_dock_book_->show();

	class Bin : public Gtk::Bin {
	public:
		Bin() { }
	protected:
		void on_size_allocate(Gtk::Allocation &allocation) {
			Gtk::Bin::on_size_allocate(allocation);
			if (get_child() != NULL)
				get_child()->size_allocate(allocation);
		}
		void on_size_request(Gtk::Requisition *requisition) {
			Gtk::Bin::on_size_request(requisition);
			if (get_child() != NULL && requisition != NULL)
				*requisition = get_child()->size_request();
		}
	};

	bin_ = manage((Gtk::Bin*)new Bin());
	bin_->add(*main_dock_book_);
	bin_->show();

	Gtk::VBox *vbox = manage(new Gtk::VBox());

	Gtk::Widget* menubar = App::ui_manager()->get_widget("/menubar-main");
	if (menubar != NULL)
	{
		vbox->pack_start(*menubar, false, false, 0);
	}

	Gtk::Widget* toolbar = App::ui_manager()->get_widget("/toolbar-main");
	if (toolbar != NULL)
	{
		Gtk::IconSize iconsize = Gtk::IconSize::from_name("synfig-small_icon_16x16");
		toolbar->set_property("toolbar-style", Gtk::TOOLBAR_ICONS);
		toolbar->set_property("icon-size", iconsize);
		vbox->pack_start(*toolbar, false, false, 0);
	}

	vbox->pack_end(*bin_, true, true, 0);
	vbox->show();
	if(!App::enable_mainwin_menubar) menubar->hide();
	if(!App::enable_mainwin_toolbar) toolbar->hide();

	add(*vbox);

	add_accel_group(App::ui_manager()->get_accel_group());

	init_menus();
	window_action_group = Gtk::ActionGroup::create("mainwindow-recentfiles");
	App::ui_manager()->insert_action_group(window_action_group);

	App::signal_recent_files_changed().connect(
		sigc::mem_fun(*this, &MainWindow::on_recent_files_changed) );

	signal_delete_event().connect(
		sigc::ptr_fun(App::shutdown_request) );

	App::dock_manager->signal_dockable_registered().connect(
		sigc::mem_fun(*this,&MainWindow::on_dockable_registered) );

	App::dock_manager->signal_dockable_unregistered().connect(
		sigc::mem_fun(*this,&MainWindow::on_dockable_unregistered) );

	GRAB_HINT_DATA("mainwindow");
}

MainWindow::~MainWindow(){ }


void
MainWindow::save_all()
{
	std::list<etl::handle<Instance> >::iterator iter;
	for(iter=App::instance_list.begin();iter!=App::instance_list.end();iter++)
		(*iter)->save();
}

void
MainWindow::show_dialog_input()
{
	App::dialog_input->present();
}

void
MainWindow::init_menus()
{
	Glib::RefPtr<Gtk::ActionGroup> action_group = Gtk::ActionGroup::create("mainwindow");

	// file
	action_group->add( Gtk::Action::create("new", Gtk::Stock::NEW),
		sigc::hide_return(sigc::ptr_fun(&studio::App::new_instance))
	);
	action_group->add( Gtk::Action::create("open", Gtk::Stock::OPEN, _("Open...")),
		sigc::hide_return(sigc::bind(sigc::ptr_fun(&studio::App::dialog_open), ""))
	);
	action_group->add( Gtk::Action::create("save-all", Gtk::StockID("synfig-saveall")),
		sigc::ptr_fun(save_all)
	);
	action_group->add( Gtk::Action::create("quit", Gtk::StockID("gtk-quit"), _("Quit")),
		sigc::hide_return(sigc::ptr_fun(&studio::App::quit))
	);

	// Edit menu
	action_group->add( Gtk::Action::create("input-devices", _("Input Devices...")),
		sigc::ptr_fun(&MainWindow::show_dialog_input)
	);
	action_group->add( Gtk::Action::create("setup", _("Preferences...")),
		sigc::ptr_fun(&studio::App::show_setup)
	);

	// View menu
	//Glib::RefPtr<Gtk::ToggleAction> action;
	toggle_menubar = Gtk::ToggleAction::create("toggle-mainwin-menubar", _("Show Menubar"));
	toggle_menubar->set_active(toggling_show_menubar);
	action_group->add(toggle_menubar, sigc::mem_fun(*this, &studio::MainWindow::toggle_show_menubar));

	toggle_toolbar = Gtk::ToggleAction::create("toggle-mainwin-toolbar", _("Show Toolbar"));
	toggle_toolbar->set_active(toggling_show_toolbar);
	action_group->add(toggle_toolbar, sigc::mem_fun(*this, &studio::MainWindow::toggle_show_toolbar));

	// pre defined workspace (window ui layout)
	action_group->add( Gtk::Action::create("workspace-compositing", _("Compositing")),
		sigc::ptr_fun(App::set_workspace_compositing)
	);
	action_group->add( Gtk::Action::create("workspace-animating", _("Animating")),
		sigc::ptr_fun(App::set_workspace_animating)
	);
	action_group->add( Gtk::Action::create("workspace-default", _("Default")),
		sigc::ptr_fun(App::set_workspace_default)
	);

	// help
	#define URL(action_name,title,url) \
		action_group->add( Gtk::Action::create(action_name, title), \
			sigc::bind(sigc::ptr_fun(&studio::App::open_url),url))
	#define WIKI(action_name,title,page) \
		URL(action_name,title, "http://synfig.org/wiki" + String(page))
	#define SITE(action_name,title,page) \
		URL(action_name,title, "http://synfig.org/cms" + String(page))

	action_group->add( Gtk::Action::create("help", Gtk::Stock::HELP),
		sigc::ptr_fun(studio::App::dialog_help)
	);

	// TRANSLATORS:         | Help menu entry:              | A wiki page:          |
	WIKI("help-tutorials",	_("Tutorials"),					_("/Category:Tutorials"));
	WIKI("help-reference",	_("Reference"),					_("/Category:Reference"));
	WIKI("help-faq",		_("Frequently Asked Questions"),_("/FAQ")				);
	SITE("help-support",	_("Get Support"),				_("/en/support")		);

	action_group->add( Gtk::Action::create(
			"help-about", Gtk::StockID("synfig-about"), _("About Synfig Studio")),
		sigc::ptr_fun(studio::App::dialog_about)
	);

	// TODO: open recent
	//filemenu->items().push_back(Gtk::Menu_Helpers::MenuElem(_("Open Recent"),*recent_files_menu));

	App::ui_manager()->insert_action_group(action_group);
}


void
MainWindow::toggle_show_toolbar()
{
	Gtk::Widget* toolbar = App::ui_manager()->get_widget("/toolbar-main");

	if(toggling_show_toolbar)
	{
		toolbar->hide();
		toggling_show_toolbar = false;
	}
	else
	{
		toolbar->show();
		toggling_show_toolbar = true;
	}
	App::enable_mainwin_toolbar = toggling_show_toolbar;
}


void
MainWindow::toggle_show_menubar()
{
	Gtk::Widget* menubar = App::ui_manager()->get_widget("/menubar-main");

	if(toggling_show_menubar)
	{
		menubar->hide();
		toggling_show_menubar = false;
	}
	else
	{
		menubar->show();
		toggling_show_menubar = true;
	}
	App::enable_mainwin_menubar = toggling_show_menubar;
}


void
MainWindow::on_recent_files_changed()
{
	Glib::RefPtr<Gtk::ActionGroup> action_group = Gtk::ActionGroup::create("mainwindow-recentfiles");

	int index = 0;
	std::string menu_items;
	for(list<string>::const_iterator i=App::get_recent_files().begin();i!=App::get_recent_files().end();i++)
	{
		std::string raw = basename(*i);
		std::string quoted;
		size_t pos = 0, last_pos = 0;

		// replace _ in filenames by __ or it won't show up in the menu
		for (pos = last_pos = 0; (pos = raw.find('_', pos)) != string::npos; last_pos = pos)
			quoted += raw.substr(last_pos, ++pos - last_pos) + '_';
		quoted += raw.substr(last_pos);

		std::string action_name = strprintf("file-recent-%d", index++);
		menu_items += "<menuitem action='" + action_name +"' />";

		action_group->add( Gtk::Action::create(action_name, quoted),
			sigc::hide_return(sigc::bind(sigc::ptr_fun(&App::open),*i))
		);
	}

	std::string ui_info =
		"<menu action='menu-file'><menu action='menu-open-recent'>"
	  + menu_items
	  + "</menu></menu>";
	std::string ui_info_popup =
		"<ui><popup action='menu-main'>" + ui_info + "</popup></ui>";
	std::string ui_info_menubar =
		"<ui><menubar action='menubar-main'>" + ui_info + "</menubar></ui>";

	App::ui_manager()->insert_action_group(action_group);
	App::ui_manager()->add_ui_from_string(ui_info_popup);
	App::ui_manager()->add_ui_from_string(ui_info_menubar);
}

void
MainWindow::on_dockable_registered(Dockable* dockable)
{

	// replace _ in panel names (filenames) by __ or it won't show up in the menu,
	// this block code is just a copy from MainWindow::on_recent_files_changed().
	std::string raw = dockable->get_local_name();
	std::string quoted;
	size_t pos = 0, last_pos = 0;
	for (pos = last_pos = 0; (pos = raw.find('_', pos)) != string::npos; last_pos = pos)
		quoted += raw.substr(last_pos, ++pos - last_pos) + '_';
	quoted += raw.substr(last_pos);

	window_action_group->add( Gtk::Action::create("panel-" + dockable->get_name(), quoted),
		sigc::mem_fun(*dockable, &Dockable::present)
	);

	std::string ui_info =
		"<menu action='menu-window'>"
	    "<menuitem action='panel-" + dockable->get_name() + "' />"
	    "</menu>";
	std::string ui_info_popup =
		"<ui><popup action='menu-main'>" + ui_info + "</popup></ui>";
	std::string ui_info_menubar =
		"<ui><menubar action='menubar-main'>" + ui_info + "</menubar></ui>";

	Gtk::UIManager::ui_merge_id merge_id_popup = App::ui_manager()->add_ui_from_string(ui_info_popup);
	Gtk::UIManager::ui_merge_id merge_id_menubar = App::ui_manager()->add_ui_from_string(ui_info_menubar);

	// record CanvasView toolbar and popup id's
	CanvasView *canvas_view = dynamic_cast<CanvasView*>(dockable);
	if(canvas_view)
	{
		canvas_view->set_popup_id(merge_id_popup);
		canvas_view->set_toolbar_id(merge_id_menubar);
	}
}

void
MainWindow::on_dockable_unregistered(Dockable* dockable)
{
	// remove the document from the menus
	CanvasView *canvas_view = dynamic_cast<CanvasView*>(dockable);
	if(canvas_view)
	{
		App::ui_manager()->remove_ui(canvas_view->get_popup_id());
		App::ui_manager()->remove_ui(canvas_view->get_toolbar_id());
	}
}
/* === E N T R Y P O I N T ================================================= */
