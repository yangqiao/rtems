/*
 * Copyright (c) 2015 Yang Qiao
 * based on work by:
 * Copyright (c) 2013 Hesham AL-Matary.
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rtems.org/license/LICENSE.
 */

#define ARM_CP15_TEXT_SECTION BSP_START_TEXT_SECTION

#include <bsp/start.h>
#include <bsp/arm-cp15-start.h>
#include <bsp/linker-symbols.h>
#include <bsp/mm.h>
#include <rtems/fb.h>

extern int raspberrypi_get_var_screen_info( struct fb_var_screeninfo *info );

BSP_START_TEXT_SECTION void bsp_memory_management_initialize(void)
{
  uint32_t ctrl = arm_cp15_get_control();

  ctrl |= ARM_CP15_CTRL_AFE | ARM_CP15_CTRL_S | ARM_CP15_CTRL_XP;

  struct fb_fix_screeninfo fb_fix_info;
  raspberrypi_get_fix_screen_info(&fb_fix_info);

  arm_cp15_start_setup_translation_table_and_enable_mmu_and_cache(
    ctrl,
    (uint32_t *) bsp_translation_table_base,
    ARM_MMU_DEFAULT_CLIENT_DOMAIN,
    &arm_cp15_start_mmu_config_table[0],
    arm_cp15_start_mmu_config_table_size
  );

  arm_cp15_set_translation_table_entries(fb_fix_info.smem_start,
                                        fb_fix_info.smem_start +
                                        fb_fix_info.smem_len,
                                        ARMV7_MMU_DATA_READ_WRITE_CACHED);
}
