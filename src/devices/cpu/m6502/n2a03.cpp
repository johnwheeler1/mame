// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    n2a03.cpp

    6502, NES variant

***************************************************************************/

#include "emu.h"
#include "n2a03.h"
#include "n2a03d.h"

DEFINE_DEVICE_TYPE(N2A03_CORE, n2a03_core_device, "n2a03_core", "Ricoh N2A03 core") // needed for some VT systems with XOP instead of standard APU
DEFINE_DEVICE_TYPE(N2A03,      n2a03_device,      "n2a03",      "Ricoh N2A03")      // earliest version, found in punchout, spnchout, dkong3, VS. systems, and some early Famicoms
DEFINE_DEVICE_TYPE(N2A03G,     n2a03g_device,     "n2a03g",     "Ricoh N2A03G")     // later revision, found in front-loader NES

uint8_t n2a03_device::psg1_4014_r()
{
	return m_apu->read(0x14);
}

uint8_t n2a03_device::psg1_4015_r()
{
	return m_apu->read(0x15);
}

void n2a03_device::psg1_4015_w(uint8_t data)
{
	m_apu->write(0x15, data);
}

void n2a03_device::psg1_4017_w(uint8_t data)
{
	m_apu->write(0x17, data);
}


// on various drivers          output port 0x4014 is used for external hardware   (not used by APU?)
//                       input/output port 0x4016  ^                              (not used by APU?)
//                       input        port 0x4017  ^                              ( APU_IRQCTRL )
// is there a fall through where every write is seen by other hw, or do these addresses really not touch the APU?? APU_IRQCTRL can definitely be written by can it be read back?

void n2a03_device::n2a03_map(address_map &map)
{
	map(0x4000, 0x4013).rw("nesapu", FUNC(nesapu_device::read), FUNC(nesapu_device::write));
	map(0x4014, 0x4014).r(FUNC(n2a03_device::psg1_4014_r)); // .w(FUNC(nesapu_device::sprite_dma_0_w));
	map(0x4015, 0x4015).rw(FUNC(n2a03_device::psg1_4015_r), FUNC(n2a03_device::psg1_4015_w)); /* PSG status / first control register */
	//map(0x4016, 0x4016).rw(FUNC(n2a03_device::vsnes_in0_r), FUNC(n2a03_device::vsnes_in0_w));
	map(0x4017, 0x4017) /*.r(FUNC(n2a03_device::vsnes_in1_r))*/ .w(FUNC(n2a03_device::psg1_4017_w));
}



n2a03_core_device::n2a03_core_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
	: m6502_device(mconfig, type, tag, owner, clock)
{
}

n2a03_core_device::n2a03_core_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: n2a03_core_device(mconfig, N2A03_CORE, tag, owner, clock)
{
}



n2a03_device::n2a03_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
	: n2a03_core_device(mconfig, type, tag, owner, clock)
	, device_mixer_interface(mconfig, *this, 1)
	, m_apu(*this, "nesapu")
{
	program_config.m_internal_map = address_map_constructor(FUNC(n2a03_device::n2a03_map), this);
}

n2a03_device::n2a03_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: n2a03_device(mconfig, N2A03, tag, owner, clock)
{
}

n2a03g_device::n2a03g_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: n2a03_device(mconfig, N2A03G, tag, owner, clock)
{
}



std::unique_ptr<util::disasm_interface> n2a03_core_device::create_disassembler()
{
	return std::make_unique<n2a03_disassembler>();
}

WRITE_LINE_MEMBER(n2a03_device::apu_irq)
{
	// games relying on the APU_IRQ don't seem to work anyway? (nes software list : timelord, mig29sf, firehawk)
	set_input_line(N2A03_APU_IRQ_LINE, state ? ASSERT_LINE : CLEAR_LINE);
}

uint8_t n2a03_device::apu_read_mem(offs_t offset)
{
	return mintf->program.read_byte(offset);
}

void n2a03_device::device_add_mconfig(machine_config &config)
{
	APU_2A03(config, m_apu, DERIVED_CLOCK(1,1));
	m_apu->irq().set(FUNC(n2a03_device::apu_irq));
	m_apu->mem_read().set(FUNC(n2a03_device::apu_read_mem));
	m_apu->add_route(ALL_OUTPUTS, *this, 1.0, AUTO_ALLOC_INPUT, 0);
}

void n2a03g_device::device_add_mconfig(machine_config &config)
{
	NES_APU(config, m_apu, DERIVED_CLOCK(1,1));
	m_apu->irq().set(FUNC(n2a03g_device::apu_irq));
	m_apu->mem_read().set(FUNC(n2a03g_device::apu_read_mem));
	m_apu->add_route(ALL_OUTPUTS, *this, 1.0, AUTO_ALLOC_INPUT, 0);
}


#include "cpu/m6502/n2a03.hxx"
