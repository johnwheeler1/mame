// license:BSD-3-Clause
// copyright-holders:Andrew Gardner
//============================================================
//
//  debugqt.c - SDL/QT debug window handling
//
//  SDLMAME by Olivier Galibert and R. Belmont
//
//============================================================

#include "debug_module.h"
#include "modules/osdmodule.h"

#if (USE_QTDEBUG)

#include <vector>

#include <QtWidgets/QApplication>
#include <QtCore/QAbstractEventDispatcher>
#include <QtCore/QAbstractNativeEventFilter>

#include "emu.h"
#include "config.h"
#include "debugger.h"
#include "modules/lib/osdobj_common.h"

#include "qt/logwindow.h"
#include "qt/mainwindow.h"
#include "qt/dasmwindow.h"
#include "qt/memorywindow.h"
#include "qt/breakpointswindow.h"
#include "qt/deviceswindow.h"
#include "qt/deviceinformationwindow.h"

#include "util/xmlfile.h"


class debug_qt : public osd_module, public debug_module
#if defined(_WIN32) && !defined(SDLMAME_WIN32)
, public QAbstractNativeEventFilter
#endif
{
public:
	debug_qt() : osd_module(OSD_DEBUG_PROVIDER, "qt"), debug_module(), m_machine(nullptr)
	{
	}

	virtual ~debug_qt() { }

	virtual int init(const osd_options &options) { return 0; }
	virtual void exit() { }

	virtual void init_debugger(running_machine &machine);
	virtual void wait_for_debugger(device_t &device, bool firststop);
	virtual void debugger_update();
#if defined(_WIN32) && !defined(SDLMAME_WIN32)
	virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *) Q_DECL_OVERRIDE;
#endif

private:
	void configuration_save(config_type which_type, util::xml::data_node *parentnode);
	void gather_save_configurations();

	running_machine *m_machine;
	util::xml::file::ptr m_config;
};


namespace {

//============================================================
//  "Global" variables to make QT happy
//============================================================

int qtArgc = 1;
char qtArg0[] = "mame";
char *qtArgv[] = { qtArg0, nullptr };

bool oneShot = true;
MainWindow *mainQtWindow = nullptr;

//============================================================
//  XML configuration save/load
//============================================================

// Global variable used to feed the xml configuration callbacks
std::vector<std::unique_ptr<WindowQtConfig> > xmlConfigurations;


void xml_configuration_load(running_machine &machine, config_type cfg_type, config_level cfg_level, util::xml::data_node const *parentnode)
{
	// We only care about system configuration files
	if ((cfg_type != config_type::SYSTEM) || !parentnode)
		return;

	xmlConfigurations.clear();

	// Configuration load
	util::xml::data_node const *wnode = nullptr;
	for (wnode = parentnode->get_child(osd::debugger::NODE_WINDOW); wnode; wnode = wnode->get_next_sibling(osd::debugger::NODE_WINDOW))
	{
		switch (wnode->get_attribute_int(osd::debugger::ATTR_WINDOW_TYPE, -1))
		{
			case osd::debugger::WINDOW_TYPE_CONSOLE:            xmlConfigurations.push_back(std::make_unique<MainWindowQtConfig>()); break;
			case osd::debugger::WINDOW_TYPE_MEMORY_VIEWER:      xmlConfigurations.push_back(std::make_unique<MemoryWindowQtConfig>()); break;
			case osd::debugger::WINDOW_TYPE_DISASSEMBLY_VIEWER: xmlConfigurations.push_back(std::make_unique<DasmWindowQtConfig>()); break;
			case osd::debugger::WINDOW_TYPE_ERROR_LOG_VIEWER:   xmlConfigurations.push_back(std::make_unique<LogWindowQtConfig>()); break;
			case osd::debugger::WINDOW_TYPE_POINTS_VIEWER:      xmlConfigurations.push_back(std::make_unique<BreakpointsWindowQtConfig>()); break;
			case osd::debugger::WINDOW_TYPE_DEVICES_VIEWER:     xmlConfigurations.push_back(std::make_unique<DevicesWindowQtConfig>()); break;
			case osd::debugger::WINDOW_TYPE_DEVICE_INFO_VIEWER: xmlConfigurations.push_back(std::make_unique<DeviceInformationWindowQtConfig>()); break;
			default: continue;
		}
		xmlConfigurations.back()->recoverFromXmlNode(*wnode);
	}
}


//============================================================
//  Utilities
//============================================================

void load_and_clear_main_window_config(std::vector<std::unique_ptr<WindowQtConfig> > &configList)
{
	for (int i = 0; i < configList.size(); i++)
	{
		WindowQtConfig &config = *configList[i];
		if (config.m_type == osd::debugger::WINDOW_TYPE_CONSOLE)
		{
			config.applyToQWidget(mainQtWindow);
			configList.erase(configList.begin() + i);
			break;
		}
	}
}


void setup_additional_startup_windows(running_machine &machine, std::vector<std::unique_ptr<WindowQtConfig> > &configList)
{
	for (int i = 0; i < configList.size(); i++)
	{
		WindowQtConfig &config = *configList[i];

		WindowQt *foo = nullptr;
		switch (config.m_type)
		{
		case osd::debugger::WINDOW_TYPE_MEMORY_VIEWER:
			foo = new MemoryWindow(machine); break;
		case osd::debugger::WINDOW_TYPE_DISASSEMBLY_VIEWER:
			foo = new DasmWindow(machine); break;
		case osd::debugger::WINDOW_TYPE_ERROR_LOG_VIEWER:
			foo = new LogWindow(machine); break;
		case osd::debugger::WINDOW_TYPE_POINTS_VIEWER:
			foo = new BreakpointsWindow(machine); break;
		case osd::debugger::WINDOW_TYPE_DEVICES_VIEWER:
			foo = new DevicesWindow(machine); break;
		case osd::debugger::WINDOW_TYPE_DEVICE_INFO_VIEWER:
			foo = new DeviceInformationWindow(machine); break;
		default:
			break;
		}
		config.applyToQWidget(foo);
		foo->show();
	}
}


void bring_main_window_to_front()
{
	foreach (QWidget *widget, QApplication::topLevelWidgets())
	{
		if (dynamic_cast<MainWindow *>(widget))
		{
			widget->activateWindow();
			widget->raise();
		}
	}
}

} // anonymous namespace


//============================================================
//  Core functionality
//============================================================

#if defined(_WIN32) && !defined(SDLMAME_WIN32)
bool winwindow_qt_filter(void *message);

bool debug_qt::nativeEventFilter(const QByteArray &eventType, void *message, long *)
{
	winwindow_qt_filter(message);
	return false;
}
#endif

void debug_qt::init_debugger(running_machine &machine)
{
	if (!qApp)
	{
		// If you're starting from scratch, create a new qApp
		new QApplication(qtArgc, qtArgv);
#if defined(_WIN32) && !defined(SDLMAME_WIN32)
		QAbstractEventDispatcher::instance()->installNativeEventFilter(this);
#endif
	}
	else
	{
		// If you've done a hard reset, clear out existing widgets & get ready for re-init
		foreach (QWidget *widget, QApplication::topLevelWidgets())
		{
			if (!widget->isWindow() || widget->windowType() != Qt::Window)
				continue;
			delete widget;
		}
		oneShot = true;
	}

	m_machine = &machine;

	// Setup the configuration XML saving and loading
	machine.configuration().config_register("debugger",
			configuration_manager::load_delegate(&xml_configuration_load, &machine),
			configuration_manager::save_delegate(&debug_qt::configuration_save, this));
}


//============================================================
//  Core functionality
//============================================================

#if defined(SDLMAME_UNIX) || defined(SDLMAME_WIN32)
extern int sdl_entered_debugger;
#elif defined(_WIN32)
void winwindow_update_cursor_state(running_machine &machine);
#endif

void debug_qt::wait_for_debugger(device_t &device, bool firststop)
{
#if defined(SDLMAME_UNIX) || defined(SDLMAME_WIN32)
	sdl_entered_debugger = 1;
#endif

	// Dialog initialization
	if (oneShot)
	{
		mainQtWindow = new MainWindow(*m_machine);
		load_and_clear_main_window_config(xmlConfigurations);
		setup_additional_startup_windows(*m_machine, xmlConfigurations);
		mainQtWindow->show();
		oneShot = false;
	}

	// Ensure all top level widgets are visible & bring main window to front
	foreach (QWidget *widget, QApplication::topLevelWidgets())
	{
		if (!widget->isWindow() || widget->windowType() != Qt::Window)
			continue;
		widget->show();
	}

	if (firststop)
		bring_main_window_to_front();

	// Set the main window to display the proper cpu
	mainQtWindow->setProcessor(&device);

	// Run our own QT event loop
	osd_sleep(osd_ticks_per_second() / 1000 * 50);
	qApp->processEvents(QEventLoop::AllEvents, 1);

	// Refresh everyone if requested
	if (mainQtWindow->wantsRefresh())
	{
		QWidgetList allWidgets = qApp->allWidgets();
		for (int i = 0; i < allWidgets.length(); i++)
			allWidgets[i]->update();
		mainQtWindow->clearRefreshFlag();
	}

	// Hide all top level widgets if requested
	if (mainQtWindow->wantsHide())
	{
		foreach (QWidget *widget, QApplication::topLevelWidgets())
		{
			if (!widget->isWindow() || widget->windowType() != Qt::Window)
				continue;
			widget->hide();
		}
		mainQtWindow->clearHideFlag();
	}

	// Exit if the machine has been instructed to do so (scheduled event == exit || hard_reset)
	if (m_machine->scheduled_event_pending())
	{
		// Keep a list of windows we want to save.
		// We need to do this here because by the time xml_configuration_save gets called
		// all the QT windows are already gone.
		gather_save_configurations();
	}

#if defined(_WIN32) && !defined(SDLMAME_WIN32)
	winwindow_update_cursor_state(*m_machine); // make sure the cursor isn't hidden while in debugger
#endif
}


//============================================================
//  Available for video.*
//============================================================

void debug_qt::debugger_update()
{
	qApp->processEvents(QEventLoop::AllEvents, 1);
}


void debug_qt::configuration_save(config_type which_type, util::xml::data_node *parentnode)
{
	// We only save system configuration for now
	if ((config_type::SYSTEM == which_type) && parentnode && m_config)
	{
		for (util::xml::data_node const *node = m_config->get_first_child(); node; node = node->get_next_sibling())
			node->copy_into(*parentnode);
		m_config.reset();
	}
}


void debug_qt::gather_save_configurations()
{
	m_config = util::xml::file::create();

	// Loop over all the open windows
	foreach (QWidget *widget, QApplication::topLevelWidgets())
	{
		if (!widget->isVisible())
			continue;

		if (!widget->isWindow() || widget->windowType() != Qt::Window)
			continue;

		WindowQt *const win = dynamic_cast<WindowQt *>(widget);
		if (win)
			win->saveConfiguration(*m_config);
	}
}

#else // USE_QTDEBUG

MODULE_NOT_SUPPORTED(debug_qt, OSD_DEBUG_PROVIDER, "qt")

#endif

MODULE_DEFINITION(DEBUG_QT, debug_qt)
