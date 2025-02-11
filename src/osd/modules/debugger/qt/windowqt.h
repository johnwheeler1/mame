// license:BSD-3-Clause
// copyright-holders:Andrew Gardner
#ifndef MAME_DEBUGGER_QT_WINDOWQT_H
#define MAME_DEBUGGER_QT_WINDOWQT_H

#include "../xmlconfig.h"

#include "debugger.h"

#include <QtWidgets/QMainWindow>


//============================================================
//  The Qt window that everyone derives from.
//============================================================
class WindowQt : public QMainWindow
{
	Q_OBJECT

public:
	WindowQt(running_machine &machine, QWidget *parent = nullptr);
	virtual ~WindowQt();

	// The interface to an all-window refresh
	void refreshAll() { s_refreshAll = true; }
	bool wantsRefresh() { return s_refreshAll; }
	void clearRefreshFlag() { s_refreshAll = false; }

	void hideAll() { s_hideAll = true; }
	bool wantsHide() { return s_hideAll; }
	void clearHideFlag() { s_hideAll = false; }

	void saveConfiguration(util::xml::data_node &parentnode);

protected slots:
	void debugActOpenMemory();
	void debugActOpenDasm();
	void debugActOpenLog();
	void debugActOpenPoints();
	void debugActOpenDevices();
	void debugActRun();
	void debugActRunAndHide();
	void debugActRunToNextCpu();
	void debugActRunNextInt();
	void debugActRunNextVBlank();
	void debugActStepInto();
	void debugActStepOver();
	void debugActStepOut();
	void debugActSoftReset();
	void debugActHardReset();
	virtual void debugActClose();
	void debugActQuit();


protected:
	virtual void saveConfigurationToNode(util::xml::data_node &node);

	running_machine &m_machine;

	static bool s_refreshAll;
	static bool s_hideAll;
};


//=========================================================================
//  A way to store the configuration of a window long enough to read/write.
//=========================================================================
class WindowQtConfig
{
public:
	WindowQtConfig(int type) :
		m_type(type),
		m_size(800, 600),
		m_position(120, 120)
	{}
	virtual ~WindowQtConfig() {}

	// Settings
	int m_type;
	QPoint m_size;
	QPoint m_position;

	virtual void applyToQWidget(QWidget *widget);
	virtual void recoverFromXmlNode(util::xml::data_node const &node);
};


#endif // MAME_DEBUGGER_QT_WINDOWQT_H
