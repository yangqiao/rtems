/**
 * @file
 *
 * @ingroup raspberrypi
 *
 * @brief framebuffer support.
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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>

#include <bsp.h>
#include <bsp/raspberrypi.h>
#include <bsp/mailbox.h>
#include <bsp/vc.h>

#include <rtems.h>
#include <rtems/libio.h>
#include <rtems/fb.h>
#include <rtems/framebuffer.h>
#include <rtems/score/atomic.h>

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768
#define BPP 32
#define BUFFER_SIZE (SCREEN_WIDTH*SCREEN_HEIGHT*BPP/8)

/* flag to limit driver to protect against multiple opens */
static Atomic_Flag driver_mutex;

/*
 * screen information for the driver (fb0).
 */

static struct fb_var_screeninfo fb_var_info = {
  .xres                = SCREEN_WIDTH,
  .yres                = SCREEN_HEIGHT,
  .bits_per_pixel      = BPP
};

static struct fb_fix_screeninfo fb_fix_info = {
  .smem_start          = (void *) NULL,
  .smem_len            = 0,
  .type                = FB_TYPE_PACKED_PIXELS,
  .visual              = FB_VISUAL_TRUECOLOR,
  .line_length         = 0
};

int raspberrypi_get_fix_screen_info( struct fb_fix_screeninfo *info )
{
  *info = fb_fix_info;
  return 0;
}

int raspberrypi_get_var_screen_info( struct fb_var_screeninfo *info )
{
  *info = fb_var_info;
  return 0;
}

static int
find_mode_from_cmdline(void)
{
  const char* opt;
  char* endptr;
  uint32_t width;
  uint32_t height;
  uint32_t bpp;
  opt = rpi_cmdline_arg("--video=");
  if (opt)
  {
      opt += sizeof("--video=")-1;
      width = strtol(opt, &endptr, 10);
      if (*endptr != 'x')
      {
          return -2;
      }
      opt = endptr+1;
      height = strtol(opt, &endptr, 10);
      switch (*endptr)
      {
          case '-':
              opt = endptr+1;
              if (strlen(opt) <= 2)
                  bpp = strtol(opt, &endptr, 10);
              else
              {
                  bpp = strtol(opt, &endptr, 10);
                  if (*endptr != ' ')
                  {
                      return -4;
                  }
              }
          case ' ':
          case 0:
              break;
          default:
              return -3;
      }
  }
  else
    return -1;

  fb_var_info.xres    = width;
  fb_var_info.yres    = height;

  return 0;
}

static int
find_mode_from_vc(void)
{
  bcm2835_get_display_size_entries entries;
  bcm2835_mailbox_get_display_size(&entries);
  unsigned int width = entries.width;
  unsigned int height = entries.height;

  if (width == 0 || height == 0)
  {
    fb_var_info.xres    = SCREEN_WIDTH;
    fb_var_info.yres    = SCREEN_HEIGHT;
  }
  else
  {
    fb_var_info.xres     = width;
    fb_var_info.yres     = height;
  }

  return 0;
}

static bool
hdmi_is_present(void)
{
  bcm2835_get_display_size_entries entries;
  bcm2835_mailbox_get_display_size(&entries);
  if(entries.width == 0x290 && entries.height ==0x1A0 )
  {
    return false;
  }
  else
  {
    return true;
  }
}

int
fb_init(void)
{
  if (fb_fix_info.smem_start != NULL)
  {
    return -2;
  }

  if (hdmi_is_present() == false)
  {
    return -3;
  }

  if (find_mode_from_cmdline())
  {
    if(find_mode_from_vc())
      return -1;
  }

  bcm2835_init_frame_buffer_entries  init_frame_buffer_entries;
  init_frame_buffer_entries.xres = fb_var_info.xres;
  init_frame_buffer_entries.yres = fb_var_info.yres;
  init_frame_buffer_entries.xvirt = fb_var_info.xres;
  init_frame_buffer_entries.yvirt = fb_var_info.yres;
  init_frame_buffer_entries.depth = fb_var_info.bits_per_pixel;
  init_frame_buffer_entries.pixel_order = bcm2835_mailbox_pixel_order_rgb;
  init_frame_buffer_entries.alpha_mode = bcm2835_mailbox_alpha_mode_0_opaque;
  init_frame_buffer_entries.voffset_x = 0;
  init_frame_buffer_entries.voffset_y = 0;
  init_frame_buffer_entries.overscan_left = 0;
  init_frame_buffer_entries.overscan_right = 0;
  init_frame_buffer_entries.overscan_top = 0;
  init_frame_buffer_entries.overscan_bottom = 0;
  bcm2835_mailbox_init_frame_buffer(&init_frame_buffer_entries);

  bcm2835_get_pitch_entries get_pitch_entries;
  bcm2835_mailbox_get_pitch(&get_pitch_entries);

  fb_var_info.xres = init_frame_buffer_entries.xres;
  fb_var_info.yres = init_frame_buffer_entries.yres;
  fb_var_info.bits_per_pixel = init_frame_buffer_entries.depth;
  fb_fix_info.smem_start = init_frame_buffer_entries.base;
  fb_fix_info.smem_len = init_frame_buffer_entries.size;
  fb_fix_info.line_length = get_pitch_entries.pitch;

  return 0;
}


/*
 * fbds device driver initialize entry point.
 */

rtems_device_driver
frame_buffer_initialize (rtems_device_major_number major,
                 rtems_device_minor_number minor, void *arg)
{
  rtems_status_code status;

  /* register the devices */
  status = rtems_io_register_name (FRAMEBUFFER_DEVICE_0_NAME, major, 0);
  if (status != RTEMS_SUCCESSFUL) {
    printk ("[!] error registering framebuffer\n");
    rtems_fatal_error_occurred (status);
  }
  _Atomic_Flag_clear(&driver_mutex, ATOMIC_ORDER_RELEASE);
  return RTEMS_SUCCESSFUL;
}

/*
 * fbds device driver open operation.
 */

rtems_device_driver
frame_buffer_open (rtems_device_major_number major,
           rtems_device_minor_number minor, void *arg)
{
  if (_Atomic_Flag_test_and_set(&driver_mutex, ATOMIC_ORDER_ACQUIRE) != 0 ) {
    printk( "FB_CIRRUS could not lock driver_mutex\n" );
    return RTEMS_UNSATISFIED;
  }

  memset ((void *)fb_fix_info.smem_start, 0, fb_fix_info.smem_len);
  return RTEMS_SUCCESSFUL;
}

/*
 * fbds device driver close operation.
 */

rtems_device_driver
frame_buffer_close (rtems_device_major_number major,
            rtems_device_minor_number minor, void *arg)
{
  /* restore previous state.  for VGA this means return to text mode.
   * leave out if graphics hardware has been initialized in
   * frame_buffer_initialize() */
  _Atomic_Flag_clear(&driver_mutex, ATOMIC_ORDER_RELEASE);
  memset ((void *)fb_fix_info.smem_start, 0, fb_fix_info.smem_len);
  return RTEMS_SUCCESSFUL;
}

/*
 * fbds device driver read operation.
 */

rtems_device_driver
frame_buffer_read (rtems_device_major_number major,
           rtems_device_minor_number minor, void *arg)
{
  rtems_libio_rw_args_t *rw_args = (rtems_libio_rw_args_t *)arg;
  rw_args->bytes_moved =
    ((rw_args->offset + rw_args->count) > fb_fix_info.smem_len ) ?
    (fb_fix_info.smem_len - rw_args->offset) :rw_args->count;
  memcpy( rw_args->buffer,
          (const void *) (fb_fix_info.smem_start + rw_args->offset),
          rw_args->bytes_moved);
  return RTEMS_SUCCESSFUL;
}

/*
 * fbds device driver write operation.
 */

rtems_device_driver
frame_buffer_write (rtems_device_major_number major,
            rtems_device_minor_number minor, void *arg)
{
  rtems_libio_rw_args_t *rw_args = (rtems_libio_rw_args_t *)arg;
  rw_args->bytes_moved =
    ((rw_args->offset + rw_args->count) > fb_fix_info.smem_len ) ?
    (fb_fix_info.smem_len - rw_args->offset) : rw_args->count;
  memcpy( (void *) (fb_fix_info.smem_start + rw_args->offset),
          rw_args->buffer,
          rw_args->bytes_moved);
  return RTEMS_SUCCESSFUL;
}

/*
 * ioctl entry point.
 */

rtems_device_driver
frame_buffer_control (rtems_device_major_number major,
              rtems_device_minor_number minor, void *arg)
{
  rtems_libio_ioctl_args_t *args = arg;

  /* XXX check minor */

  switch (args->command) {
  case FBIOGET_VSCREENINFO:
    memcpy (args->buffer, &fb_var_info, sizeof (fb_var_info));
    args->ioctl_return = 0;
    break;
  case FBIOGET_FSCREENINFO:
    memcpy (args->buffer, &fb_fix_info, sizeof (fb_fix_info));
    args->ioctl_return = 0;
    break;
  case FBIOGETCMAP:
    args->ioctl_return = 0;
    break;
  case FBIOPUTCMAP:
    args->ioctl_return = 0;
    break;

  default:
    args->ioctl_return = 0;
    break;

  }
  return RTEMS_SUCCESSFUL;
}
