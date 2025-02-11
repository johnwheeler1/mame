// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Vas Crabb
//============================================================
//
//  memoryviewinfo.c - Win32 debug window handling
//
//============================================================

#include "emu.h"
#include "memoryviewinfo.h"

#include "util/xmlfile.h"

#include "strconv.h"


memoryview_info::memoryview_info(debugger_windows_interface &debugger, debugwin_info &owner, HWND parent) :
	debugview_info(debugger, owner, parent, DVT_MEMORY)
{
}


memoryview_info::~memoryview_info()
{
}


char const *memoryview_info::expression() const
{
	return view<debug_view_memory>()->expression();
}


debug_view_memory::data_format memoryview_info::data_format() const
{
	return view<debug_view_memory>()->get_data_format();
}


uint32_t memoryview_info::chunks_per_row() const
{
	return view<debug_view_memory>()->chunks_per_row();
}


bool memoryview_info::reverse() const
{
	return view<debug_view_memory>()->reverse();
}


bool memoryview_info::physical() const
{
	return view<debug_view_memory>()->physical();
}

int memoryview_info::address_radix() const
{
	return view<debug_view_memory>()->address_radix();
}


void memoryview_info::set_expression(const std::string &string)
{
	view<debug_view_memory>()->set_expression(string);
}

void memoryview_info::set_data_format(debug_view_memory::data_format dataformat)
{
	view<debug_view_memory>()->set_data_format(dataformat);
}

void memoryview_info::set_chunks_per_row(uint32_t rowchunks)
{
	view<debug_view_memory>()->set_chunks_per_row(rowchunks);
}

void memoryview_info::set_reverse(bool reverse)
{
	view<debug_view_memory>()->set_reverse(reverse);
}

void memoryview_info::set_physical(bool physical)
{
	view<debug_view_memory>()->set_physical(physical);
}

void memoryview_info::set_address_radix(int radix)
{
	view<debug_view_memory>()->set_address_radix(radix);
}


void memoryview_info::restore_configuration_from_node(util::xml::data_node const &node)
{
	debug_view_memory &memview(*view<debug_view_memory>());
	memview.set_reverse(0 != node.get_attribute_int(osd::debugger::ATTR_WINDOW_MEMORY_REVERSE_COLUMNS, memview.reverse() ? 1 : 0));
	memview.set_physical(0 != node.get_attribute_int(osd::debugger::ATTR_WINDOW_MEMORY_ADDRESS_MODE, memview.physical() ? 1 : 0));
	memview.set_address_radix(node.get_attribute_int(osd::debugger::ATTR_WINDOW_MEMORY_ADDRESS_RADIX, memview.address_radix()));
	memview.set_data_format(debug_view_memory::data_format(node.get_attribute_int(osd::debugger::ATTR_WINDOW_MEMORY_DATA_FORMAT, int(memview.get_data_format()))));
	memview.set_chunks_per_row(node.get_attribute_int(osd::debugger::ATTR_WINDOW_MEMORY_ROW_CHUNKS, memview.chunks_per_row()));

	debugview_info::restore_configuration_from_node(node);
}


void memoryview_info::save_configuration_to_node(util::xml::data_node &node)
{
	debugview_info::save_configuration_to_node(node);

	debug_view_memory &memview(*view<debug_view_memory>());
	node.set_attribute_int(osd::debugger::ATTR_WINDOW_MEMORY_REVERSE_COLUMNS, memview.reverse() ? 1 : 0);
	node.set_attribute_int(osd::debugger::ATTR_WINDOW_MEMORY_ADDRESS_MODE, memview.physical() ? 1 : 0);
	node.set_attribute_int(osd::debugger::ATTR_WINDOW_MEMORY_ADDRESS_RADIX, memview.address_radix());
	node.set_attribute_int(osd::debugger::ATTR_WINDOW_MEMORY_DATA_FORMAT, int(memview.get_data_format()));
	node.set_attribute_int(osd::debugger::ATTR_WINDOW_MEMORY_ROW_CHUNKS, memview.chunks_per_row());
}


void memoryview_info::add_items_to_context_menu(HMENU menu)
{
	debugview_info::add_items_to_context_menu(menu);

	AppendMenu(menu, MF_DISABLED | MF_SEPARATOR, 0, TEXT(""));
	AppendMenu(menu, MF_GRAYED, ID_CONTEXT_LAST_PC, TEXT("Last PC"));
}

void memoryview_info::update_context_menu(HMENU menu)
{
	debugview_info::update_context_menu(menu);

	bool enable = false;
	debug_view_memory &memview(*view<debug_view_memory>());
	debug_view_memory_source const &source = downcast<debug_view_memory_source const &>(*memview.source());
	address_space *const space = source.space();
	if (space)
	{
		if (memview.cursor_visible())
		{
			// get the last known PC to write to this memory location
			debug_view_xy const pos = memview.cursor_position();
			offs_t const address = space->byte_to_address(memview.addressAtCursorPosition(pos));
			offs_t a = address & space->logaddrmask();
			if (!space->device().memory().translate(space->spacenum(), TRANSLATE_READ_DEBUG, a))
			{
				m_lastpc = "Bad address";
			}
			else
			{
				uint64_t memval = space->unmap();
				auto dis = space->device().machine().disable_side_effects();
				switch (space->data_width())
				{
				case  8: memval = space->read_byte(a);            break;
				case 16: memval = space->read_word_unaligned(a);  break;
				case 32: memval = space->read_dword_unaligned(a); break;
				case 64: memval = space->read_qword_unaligned(a); break;
				}

				offs_t const pc = source.device()->debug()->track_mem_pc_from_space_address_data(
						space->spacenum(),
						address,
						memval);
				if (pc != offs_t(-1))
				{
					if (space->is_octal())
						m_lastpc = util::string_format("Address %o written at PC=%o", address, pc);
					else
						m_lastpc = util::string_format("Address %x written at PC=%x", address, pc);
					enable = true;
				}
				else
				{
					m_lastpc = "Unknown PC";
				}
			}
		}
		else
		{
			m_lastpc = "No address";
		}
	}
	else
	{
		m_lastpc = "Not an address space";
	}
	ModifyMenuW(menu, ID_CONTEXT_LAST_PC, MF_BYCOMMAND, ID_CONTEXT_LAST_PC, osd::text::to_wstring(m_lastpc).c_str());
	EnableMenuItem(menu, ID_CONTEXT_LAST_PC, MF_BYCOMMAND | (enable ? MF_ENABLED : MF_GRAYED));
}

void memoryview_info::handle_context_menu(unsigned command)
{
	switch (command)
	{
	case ID_CONTEXT_LAST_PC:
		// TODO: copy to clipboard
		return;

	default:
		debugview_info::handle_context_menu(command);
	};

}
