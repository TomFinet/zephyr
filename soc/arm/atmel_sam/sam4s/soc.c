/*
 * Copyright (c) 2013-2015 Wind River Systems, Inc.
 * Copyright (c) 2016 Intel Corporation.
 * Copyright (c) 2017 Justin Watson
 * Copyright (c) 2023 Gerson Fernando Budke <nandojve@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Atmel SAM4S MCU series initialization code
 *
 * This module provides routines to initialize and support board-level hardware
 * for the Atmel SAM4S series processor.
 */

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <soc.h>
#include <zephyr/arch/arm/aarch32/cortex_m/cmsis.h>

/**
 * @brief Setup various clock on SoC at boot time.
 *
 * Setup the SoC clocks according to section 28.12 in datasheet.
 *
 * Setup Slow, Main, PLLA, Processor and Master clocks during the device boot.
 * It is assumed that the relevant registers are at their reset value.
 */
static ALWAYS_INLINE void clock_init(void)
{
	uint32_t reg_val;

#ifdef CONFIG_SOC_ATMEL_SAM4S_EXT_SLCK
	/* Switch slow clock to the external 32 KHz crystal oscillator. */
	SUPC->SUPC_CR = SUPC_CR_KEY_PASSWD | SUPC_CR_XTALSEL_CRYSTAL_SEL;

	/* Wait for oscillator to be stabilized. */
	while (!(SUPC->SUPC_SR & SUPC_SR_OSCSEL)) {
		;
	}

#endif /* CONFIG_SOC_ATMEL_SAM4S_EXT_SLCK */

#ifdef CONFIG_SOC_ATMEL_SAM4S_EXT_MAINCK
	/*
	 * Setup main external crystal oscillator.
	 */

	/* Start the external crystal oscillator. */
	PMC->CKGR_MOR = CKGR_MOR_KEY_PASSWD
			/* Fast RC oscillator frequency is at 4 MHz. */
			| CKGR_MOR_MOSCRCF_4_MHz
			/*
			 * We select maximum setup time. While start up time
			 * could be shortened this optimization is not deemed
			 * critical right now.
			 */
			| CKGR_MOR_MOSCXTST(0xFFu)
			/* RC oscillator must stay on. */
			| CKGR_MOR_MOSCRCEN
			| CKGR_MOR_MOSCXTEN;

	/* Wait for oscillator to be stabilized. */
	while (!(PMC->PMC_SR & PMC_SR_MOSCXTS)) {
		;
	}

	/* Select the external crystal oscillator as the main clock source. */
	PMC->CKGR_MOR = CKGR_MOR_KEY_PASSWD
			| CKGR_MOR_MOSCRCF_4_MHz
			| CKGR_MOR_MOSCRCEN
			| CKGR_MOR_MOSCXTEN
			| CKGR_MOR_MOSCXTST(0xFFu)
			| CKGR_MOR_MOSCSEL;

	/* Wait for external oscillator to be selected. */
	while (!(PMC->PMC_SR & PMC_SR_MOSCSELS)) {
		;
	}

	/* Turn off RC oscillator, not used any longer, to save power */
	PMC->CKGR_MOR = CKGR_MOR_KEY_PASSWD
			| CKGR_MOR_MOSCSEL
			| CKGR_MOR_MOSCXTST(0xFFu)
			| CKGR_MOR_MOSCXTEN;

	/* Wait for the RC oscillator to be turned off. */
	while (PMC->PMC_SR & PMC_SR_MOSCRCS) {
		;
	}

#ifdef CONFIG_SOC_ATMEL_SAM4S_WAIT_MODE
	/*
	 * Instruct CPU to enter Wait mode instead of Sleep mode to
	 * keep Processor Clock (HCLK) and thus be able to debug
	 * CPU using JTAG.
	 */
	PMC->PMC_FSMR |= PMC_FSMR_LPM;
#endif
#else
	/* Setup main fast RC oscillator. */

	/*
	 * NOTE: MOSCRCF must be changed only if MOSCRCS is set in the PMC_SR
	 * register, should normally be the case.
	 */
	while (!(PMC->PMC_SR & PMC_SR_MOSCRCS)) {
		;
	}

	/* Set main fast RC oscillator to 12 MHz. */
	PMC->CKGR_MOR = CKGR_MOR_KEY_PASSWD
			| CKGR_MOR_MOSCRCF_12_MHz
			| CKGR_MOR_MOSCRCEN;

	/* Wait for RC oscillator to stabilize. */
	while (!(PMC->PMC_SR & PMC_SR_MOSCRCS)) {
		;
	}
#endif /* CONFIG_SOC_ATMEL_SAM4S_EXT_MAINCK */

	/*
	 * Setup PLLA
	 */

	/* Switch MCK (Master Clock) to the main clock first. */
	reg_val = PMC->PMC_MCKR & ~PMC_MCKR_CSS_Msk;
	PMC->PMC_MCKR = reg_val | PMC_MCKR_CSS_MAIN_CLK;

	/* Wait for clock selection to complete. */
	while (!(PMC->PMC_SR & PMC_SR_MCKRDY)) {
		;
	}

	/* Setup PLLA. */
	PMC->CKGR_PLLAR = CKGR_PLLAR_ONE
			  | CKGR_PLLAR_MULA(CONFIG_SOC_ATMEL_SAM4S_PLLA_MULA)
			  | CKGR_PLLAR_PLLACOUNT(0x3Fu)
			  | CKGR_PLLAR_DIVA(CONFIG_SOC_ATMEL_SAM4S_PLLA_DIVA);

	/*
	 * NOTE: Both MULA and DIVA must be set to a value greater than 0 or
	 * otherwise PLL will be disabled. In this case we would get stuck in
	 * the following loop.
	 */

	/* Wait for PLL lock. */
	while (!(PMC->PMC_SR & PMC_SR_LOCKA)) {
		;
	}

	/*
	 * Final setup of the Master Clock
	 */

	/*
	 * NOTE: PMC_MCKR must not be programmed in a single write operation.
	 * If CSS or PRES are modified we must wait for MCKRDY bit to be
	 * set again.
	 */

	/* Setup prescaler - PLLA Clock / Processor Clock (HCLK). */
	reg_val = PMC->PMC_MCKR & ~PMC_MCKR_PRES_Msk;
	PMC->PMC_MCKR = reg_val | PMC_MCKR_PRES_CLK_1;

	/* Wait for Master Clock setup to complete */
	while (!(PMC->PMC_SR & PMC_SR_MCKRDY)) {
		;
	}

	/* Finally select PLL as Master Clock source. */
	reg_val = PMC->PMC_MCKR & ~PMC_MCKR_CSS_Msk;
	PMC->PMC_MCKR = reg_val | PMC_MCKR_CSS_PLLA_CLK;

	/* Wait for Master Clock setup to complete. */
	while (!(PMC->PMC_SR & PMC_SR_MCKRDY)) {
		;
	}
}

void z_arm_platform_init(void)
{
	/*
	 * Set FWS (Flash Wait State) value before increasing Master Clock
	 * (MCK) frequency. Look at table 44.73 in the SAM4S datasheet.
	 * This is set to the highest number of read cycles because it won't
	 * hurt lower clock frequencies. However, a high frequency with too
	 * few read cycles could cause flash read problems. FWS 5 (6 cycles)
	 * is the safe setting for all of this SoCs usable frequencies.
	 * TODO: Add code to handle SAM4SD devices that have 2 EFCs.
	 */
	EFC0->EEFC_FMR = EEFC_FMR_FWS(5);

	/* Setup system clocks. */
	clock_init();
}
