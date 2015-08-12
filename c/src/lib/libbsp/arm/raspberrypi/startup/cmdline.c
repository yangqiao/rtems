/**
 * @file
 *
 * @ingroup raspberrypi
 *
 * @brief mailbox support.
 */
/*
 * Copyright (c) 2015 Yang Qiao
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *
 *  http://www.rtems.org/license/LICENSE
 *
 */

#include <bsp.h>
#include <bsp/vc.h>

#define MAX_CMDLINE_LENGTH 1024
static char* _rpi_cmdline;
static bcm2835_get_cmdline_entries get_cmdline_entries;

void rpi_init_cmdline(void)
{
  bcm2835_get_cmdline_entries get_cmdline_entries;
  bcm2835_mailbox_get_cmdline(&get_cmdline_entries);
  _rpi_cmdline = get_cmdline_entries.cmdline;
}

const char* rpi_cmdline(void)
{
  return _rpi_cmdline;
}

const char* rpi_cmdline_arg(const char* arg)
{
  return strstr (rpi_cmdline (), arg);
}
