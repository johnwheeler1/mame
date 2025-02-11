// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Vas Crabb
//============================================================
//
//  debugwin.c - Win32 debug window handling
//
//============================================================

#include "emu.h"
#include "debug_module.h"
#include "modules/osdmodule.h"

#if defined(OSD_WINDOWS) /*|| defined(SDLMAME_WIN32)*/

#include "win/debugwin.h"

#include "win/consolewininfo.h"
#include "win/debugwininfo.h"
#include "win/disasmwininfo.h"
#include "win/logwininfo.h"
#include "win/memorywininfo.h"
#include "win/pointswininfo.h"
#include "win/uimetrics.h"

#include "config.h"
#include "debugger.h"
#include "debug/debugcpu.h"

#include "util/xmlfile.h"

#include "window.h"
#include "../input/input_common.h"
#include "../input/input_windows.h"


class debugger_windows : public osd_module, public debug_module, protected debugger_windows_interface
{
public:
	debugger_windows() :
		osd_module(OSD_DEBUG_PROVIDER, "windows"),
		debug_module(),
		m_machine(nullptr),
		m_metrics(),
		m_waiting_for_debugger(false),
		m_window_list(),
		m_main_console(nullptr),
		m_config(),
		m_save_windows(true)
	{
	}

	virtual ~debugger_windows() { }

	virtual int init(const osd_options &options) override { return 0; }
	virtual void exit() override;

	virtual void init_debugger(running_machine &machine) override;
	virtual void wait_for_debugger(device_t &device, bool firststop) override;
	virtual void debugger_update() override;

protected:
	virtual running_machine &machine() const override { return *m_machine; }

	virtual ui_metrics &metrics() const override { return *m_metrics; }
	virtual void set_color_theme(int index) override;
	virtual bool get_save_window_arrangement() const override { return m_save_windows; }
	virtual void set_save_window_arrangement(bool save) override { m_save_windows = save; }

	virtual bool const &waiting_for_debugger() const override { return m_waiting_for_debugger; }
	virtual bool seq_pressed() const override;

	virtual void create_memory_window() override { create_window<memorywin_info>(); }
	virtual void create_disasm_window() override { create_window<disasmwin_info>(); }
	virtual void create_log_window() override { create_window<logwin_info>(); }
	virtual void create_points_window() override { create_window<pointswin_info>(); }
	virtual void remove_window(debugwin_info &info) override;

	virtual void show_all() override;
	virtual void hide_all() override;

private:
	template <typename T> T *create_window();

	void config_load(config_type cfgtype, config_level cfglevel, util::xml::data_node const *parentnode);
	void config_save(config_type cfgtype, util::xml::data_node *parentnode);

	void load_configuration(util::xml::data_node const &parentnode);

	running_machine             *m_machine;
	std::unique_ptr<ui_metrics>    m_metrics;
	bool                        m_waiting_for_debugger;
	std::vector<std::unique_ptr<debugwin_info>>  m_window_list;
	consolewin_info             *m_main_console;

	util::xml::file::ptr m_config;
	bool m_save_windows;
};


void debugger_windows::exit()
{
	// loop over windows and free them
	while (!m_window_list.empty())
		m_window_list.front()->destroy();

	m_main_console = nullptr;
	m_metrics.reset();
	m_machine = nullptr;
}


void debugger_windows::init_debugger(running_machine &machine)
{
	m_machine = &machine;
	m_metrics = std::make_unique<ui_metrics>(downcast<osd_options &>(m_machine->options()));
	machine.configuration().config_register(
			"debugger",
			configuration_manager::load_delegate(&debugger_windows::config_load, this),
			configuration_manager::save_delegate(&debugger_windows::config_save, this));
}


void debugger_windows::wait_for_debugger(device_t &device, bool firststop)
{
	// create a console window
	if (!m_main_console)
		m_main_console = create_window<consolewin_info>();

	// update the views in the console to reflect the current CPU
	if (m_main_console)
		m_main_console->set_cpu(device);

	// when we are first stopped, adjust focus to us
	if (firststop && m_main_console)
	{
		if (m_config)
		{
			for (util::xml::data_node const *node = m_config->get_first_child(); node; node = node->get_next_sibling())
				load_configuration(*node);
			m_config.reset();
		}
		m_main_console->set_foreground();
		if (winwindow_has_focus())
			m_main_console->set_default_focus();
	}

	// make sure the debug windows are visible
	m_waiting_for_debugger = true;
	show_all();

	// run input polling to ensure that our status is in sync
	downcast<windows_osd_interface&>(machine().osd()).poll_input(*m_machine);

	// get and process messages
	MSG message;
	GetMessage(&message, nullptr, 0, 0);

	switch (message.message)
	{
	// check for F10 -- we need to capture that ourselves
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		if (message.wParam == VK_F4 && message.message == WM_SYSKEYDOWN)
			SendMessage(GetAncestor(GetFocus(), GA_ROOT), WM_CLOSE, 0, 0);
		if (message.wParam == VK_F10)
			SendMessage(GetAncestor(GetFocus(), GA_ROOT), (message.message == WM_SYSKEYDOWN) ? WM_KEYDOWN : WM_KEYUP, message.wParam, message.lParam);
		break;

	// process everything else
	default:
		winwindow_dispatch_message(*m_machine, &message);
		break;
	}

	// mark the debugger as active
	m_waiting_for_debugger = false;
}


void debugger_windows::debugger_update()
{
	// if we're running live, do some checks
	if (!winwindow_has_focus() && m_machine && !m_machine->debugger().cpu().is_stopped() && (m_machine->phase() == machine_phase::RUNNING))
	{
		// see if the interrupt key is pressed and break if it is
		if (seq_pressed())
		{
			HWND const focuswnd = GetFocus();

			m_machine->debugger().debug_break();

			// if we were focused on some window's edit box, reset it to default
			for (auto &info : m_window_list)
				info->restore_field(focuswnd);
		}
	}
}


void debugger_windows::set_color_theme(int index)
{
	m_metrics->set_color_theme(index);
	for (auto const &window : m_window_list)
		window->redraw();
}


bool debugger_windows::seq_pressed() const
{
	input_seq const &seq = m_machine->ioport().type_seq(IPT_UI_DEBUG_BREAK);
	bool result = false;
	bool invert = false;
	bool first = true;

	// iterate over all of the codes
	for (int codenum = 0, length = seq.length(); codenum < length; codenum++)
	{
		input_code code = seq[codenum];

		if (code == input_seq::not_code)
		{
			// handle NOT
			invert = true;
		}
		else if (code == input_seq::or_code || code == input_seq::end_code)
		{
			// handle OR and END

			// if we have a positive result from the previous set, we're done
			if (result || code == input_seq::end_code)
				break;

			// otherwise, reset our state
			result = false;
			invert = false;
			first = true;
		}
		else
		{
			// handle everything else as a series of ANDs
			int const vkey = keyboard_trans_table::instance().vkey_for_mame_code(code);
			bool const pressed = (vkey != 0) && ((GetAsyncKeyState(vkey) & 0x8000) != 0);

			if (first)          // if this is the first in the sequence, result is set equal
				result = pressed ^ invert;
			else if (result)    // further values are ANDed
				result = result && (pressed ^ invert);

			// no longer first, and clear the invert flag
			first = invert = false;
		}
	}

	// return the result if we queried at least one switch
	return result;
}


void debugger_windows::remove_window(debugwin_info &info)
{
	for (auto it = m_window_list.begin(); it != m_window_list.end(); ++it)
		if (it->get() == &info) {
			m_window_list.erase(it);
			return;
		}
}


void debugger_windows::show_all()
{
	for (auto &info : m_window_list)
		info->show();
}


void debugger_windows::hide_all()
{
	SetForegroundWindow(std::static_pointer_cast<win_window_info>(osd_common_t::s_window_list.front())->platform_window());
	for (auto &info : m_window_list)
		info->hide();
}


template <typename T>
T *debugger_windows::create_window()
{
	// allocate memory
	std::unique_ptr<T> info = std::make_unique<T>(static_cast<debugger_windows_interface &>(*this));
	if (info->is_valid())
	{
		T &result(*info);
		m_window_list.push_back(std::move(info));
		return &result;
	}
	return nullptr;
}


void debugger_windows::config_load(config_type cfgtype, config_level cfglevel, util::xml::data_node const *parentnode)
{
	if (parentnode)
	{
		if (config_type::DEFAULT == cfgtype)
		{
			m_save_windows = 0 != parentnode->get_attribute_int(osd::debugger::ATTR_DEBUGGER_SAVE_WINDOWS, m_save_windows ? 1 : 0);
			util::xml::data_node const *const colors = parentnode->get_child(osd::debugger::NODE_COLORS);
			if (colors)
				m_metrics->set_color_theme(colors->get_attribute_int(osd::debugger::ATTR_COLORS_THEME, m_metrics->get_color_theme()));
		}
		else if (config_type::SYSTEM == cfgtype)
		{
			if (m_main_console)
			{
				load_configuration(*parentnode);
			}
			else
			{
				if (!m_config)
					m_config = util::xml::file::create();
				parentnode->copy_into(*m_config);
			}
		}
	}
}


void debugger_windows::config_save(config_type cfgtype, util::xml::data_node *parentnode)
{
	if (config_type::DEFAULT == cfgtype)
	{
		parentnode->set_attribute_int(osd::debugger::ATTR_DEBUGGER_SAVE_WINDOWS, m_save_windows ? 1 : 0);
		util::xml::data_node *const colors = parentnode->add_child(osd::debugger::NODE_COLORS, nullptr);
		if (colors)
			colors->set_attribute_int(osd::debugger::ATTR_COLORS_THEME, m_metrics->get_color_theme());
	}
	else if (m_save_windows && (config_type::SYSTEM == cfgtype))
	{
		for (auto &info : m_window_list)
			info->save_configuration(*parentnode);
	}
}


void debugger_windows::load_configuration(util::xml::data_node const &parentnode)
{
	for (util::xml::data_node const *node = parentnode.get_child(osd::debugger::NODE_WINDOW); node; node = node->get_next_sibling(osd::debugger::NODE_WINDOW))
	{
		debugwin_info *win = nullptr;
		switch (node->get_attribute_int(osd::debugger::ATTR_WINDOW_TYPE, -1))
		{
		case osd::debugger::WINDOW_TYPE_CONSOLE:
			m_main_console->restore_configuration_from_node(*node);
			break;
		case osd::debugger::WINDOW_TYPE_MEMORY_VIEWER:
			win = create_window<memorywin_info>();
			break;
		case osd::debugger::WINDOW_TYPE_DISASSEMBLY_VIEWER:
			win = create_window<disasmwin_info>();
			break;
		case osd::debugger::WINDOW_TYPE_ERROR_LOG_VIEWER:
			win = create_window<logwin_info>();
			break;
		case osd::debugger::WINDOW_TYPE_POINTS_VIEWER:
			win = create_window<pointswin_info>();
			break;
		case osd::debugger::WINDOW_TYPE_DEVICES_VIEWER:
			// not supported
			break;
		case osd::debugger::WINDOW_TYPE_DEVICE_INFO_VIEWER:
			// not supported
			break;
		default:
			break;
		}
		if (win)
			win->restore_configuration_from_node(*node);
	}
}


#else // not Windows
MODULE_NOT_SUPPORTED(debugger_windows, OSD_DEBUG_PROVIDER, "windows")
#endif

MODULE_DEFINITION(DEBUG_WINDOWS, debugger_windows)
