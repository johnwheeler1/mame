// license:BSD-3-Clause
// copyright-holders:Miodrag Milanovic
/***************************************************************************

        Slot device

***************************************************************************/

#include "emu.h"
#include "emuopts.h"

#include "corestr.h"
#include "zippath.h"


device_slot_interface::device_slot_interface(const machine_config &mconfig, device_t &device) :
	device_interface(device, "slot"),
	m_default_clock(DERIVED_CLOCK(1, 1)),
	m_default_option(nullptr),
	m_fixed(false),
	m_card_device(nullptr)
{
}

device_slot_interface::~device_slot_interface()
{
}


device_slot_interface::slot_option::slot_option(const char *name, const device_type &devtype, bool selectable) :
	m_name(name),
	m_devtype(devtype),
	m_selectable(selectable),
	m_default_bios(nullptr),
	m_machine_config(nullptr),
	m_input_device_defaults(nullptr),
	m_clock(0)
{
}


void device_slot_interface::interface_validity_check(validity_checker &valid) const
{
	if (m_default_option && (m_options.find(m_default_option) == m_options.end()))
		osd_printf_error("Default option '%s' does not correspond to any configured option\n", m_default_option);
}


device_slot_interface::slot_option &device_slot_interface::option_add(const char *name, const device_type &devtype)
{
	if (!name || !*name)
		throw emu_fatalerror("slot '%s' attempt to add option without name\n", device().tag());

	const slot_option *const existing = option(name);
	if (existing)
		throw emu_fatalerror("slot '%s' duplicate option '%s'\n", device().tag(), name);

	return m_options.emplace(name, std::make_unique<slot_option>(name, devtype, true)).first->second->clock(m_default_clock);
}


device_slot_interface::slot_option &device_slot_interface::option_add_internal(const char *name, const device_type &devtype)
{
	if (!name || !*name)
		throw emu_fatalerror("slot '%s' attempt to add option without name\n", device().tag());

	const slot_option *const existing = option(name);
	if (existing)
		throw emu_fatalerror("slot '%s' duplicate option '%s'\n", device().tag(), name);

	return m_options.emplace(name, std::make_unique<slot_option>(name, devtype, false)).first->second->clock(m_default_clock);
}


device_slot_interface::slot_option &device_slot_interface::option_replace(const char *name, const device_type &devtype)
{
	if (!name || !*name)
		throw emu_fatalerror("slot '%s' attempt to replace option without name\n", device().tag());

	auto search = m_options.find(name);
	if (search == m_options.end())
		throw emu_fatalerror("slot '%s' attempt to replace nonexistent option '%s'\n", device().tag(), name);

	return (search->second = std::make_unique<slot_option>(name, devtype, true))->clock(m_default_clock);
}


device_slot_interface::slot_option &device_slot_interface::option_replace_internal(const char *name, const device_type &devtype)
{
	if (!name || !*name)
		throw emu_fatalerror("slot '%s' attempt to replace option without name\n", device().tag());

	auto search = m_options.find(name);
	if (search == m_options.end())
		throw emu_fatalerror("slot '%s' attempt to replace nonexistent option '%s'\n", device().tag(), name);

	return (search->second = std::make_unique<slot_option>(name, devtype, false))->clock(m_default_clock);
}


void device_slot_interface::option_remove(const char *name)
{
	if (!name || !*name)
		throw emu_fatalerror("slot '%s' attempt to remove option without name\n", device().tag());

	if (m_options.erase(name) == 0)
		throw emu_fatalerror("slot '%s' attempt to remove nonexistent option '%s'\n", device().tag(), name);
}


device_slot_interface::slot_option *device_slot_interface::config_option(const char *name)
{
	auto const search = m_options.find(name);
	if (search != m_options.end())
		return search->second.get();

	throw emu_fatalerror("slot '%s' has no option '%s'\n", device().tag(), name);
}


bool device_slot_interface::has_selectable_options() const
{
	if (!fixed())
	{
		for (auto &option : option_list())
			if (option.second->selectable())
				return true;
	}
	return false;
}


const device_slot_interface::slot_option *device_slot_interface::option(const char *name) const
{
	if (name)
	{
		auto const search = m_options.find(name);
		if (search != m_options.end())
			return search->second.get();
	}
	return nullptr;
}


get_default_card_software_hook::get_default_card_software_hook(const std::string &path, std::function<bool(util::core_file &, std::string&)> &&get_hashfile_extrainfo)
	: m_get_hashfile_extrainfo(std::move(get_hashfile_extrainfo))
	, m_called_get_hashfile_extrainfo(false)
	, m_has_hash_extrainfo(false)
{
	if (!path.empty())
	{
		std::string revised_path;
		util::zippath_fopen(path, OPEN_FLAG_READ, m_image_file, revised_path);
		if (m_image_file)
			m_file_type = core_filename_extract_extension(revised_path, true);
	}
}

bool get_default_card_software_hook::hashfile_extrainfo(std::string &extrainfo)
{
	if (!m_called_get_hashfile_extrainfo)
	{
		if (m_get_hashfile_extrainfo)
			m_has_hash_extrainfo = m_get_hashfile_extrainfo(*image_file(), m_hash_extrainfo);
		m_called_get_hashfile_extrainfo = true;
	}
	extrainfo = m_hash_extrainfo;
	return m_has_hash_extrainfo;
}

bool get_default_card_software_hook::is_filetype(std::string_view candidate_filetype) const
{
	return util::streqlower(m_file_type, candidate_filetype);
}
