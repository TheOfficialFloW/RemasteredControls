/*
  Remastered Controls: Resistance
  Copyright (C) 2018, TheFloW

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>

#include <stdio.h>
#include <string.h>

#include <systemctrl.h>

PSP_MODULE_INFO("ResistanceRemastered", 0x1007, 1, 0);

#define FAKE_DEVNAME      "usbpspcm0:"
#define FAKE_UID          0x12345678

#define PS3_CTRL_LEFT     0x8000
#define PS3_CTRL_DOWN     0x4000
#define PS3_CTRL_RIGHT    0x2000
#define PS3_CTRL_UP       0x1000
#define PS3_CTRL_START    0x0800
#define PS3_CTRL_R3       0x0400
#define PS3_CTRL_L3       0x0200
#define PS3_CTRL_SELECT   0x0100
#define PS3_CTRL_SQUARE   0x0080
#define PS3_CTRL_CROSS    0x0040
#define PS3_CTRL_CIRCLE   0x0020
#define PS3_CTRL_TRIANGLE 0x0010
#define PS3_CTRL_R1       0x0008
#define PS3_CTRL_L1       0x0004
#define PS3_CTRL_R2       0x0002
#define PS3_CTRL_L2       0x0001

static STMOD_HANDLER previous;
static SceCtrlData pad;
static int init_mode = 0;

static u16 convertButtons(u32 psp_buttons) {
  u16 ps3_buttons = 0;

  if (psp_buttons & PSP_CTRL_LEFT)
    ps3_buttons |= PS3_CTRL_LEFT | PS3_CTRL_L2; // Remap weapon select

  if (psp_buttons & PSP_CTRL_DOWN)
    ps3_buttons |= PS3_CTRL_DOWN | PS3_CTRL_R3; // Remap aim

  if (psp_buttons & PSP_CTRL_RIGHT)
    ps3_buttons |= PS3_CTRL_RIGHT | PS3_CTRL_R2; // Remap weapon select

  if (psp_buttons & PSP_CTRL_UP)
    ps3_buttons |= PS3_CTRL_UP;

  if (psp_buttons & PSP_CTRL_START)
    ps3_buttons |= PS3_CTRL_START;

  if (psp_buttons & PSP_CTRL_SELECT)
    ps3_buttons |= PS3_CTRL_SELECT;

  if (psp_buttons & PSP_CTRL_SQUARE)
    ps3_buttons |= PS3_CTRL_SQUARE;

  if (psp_buttons & PSP_CTRL_CROSS)
    ps3_buttons |= PS3_CTRL_CROSS;

  if (psp_buttons & PSP_CTRL_CIRCLE)
    ps3_buttons |= PS3_CTRL_CIRCLE;

  if (psp_buttons & PSP_CTRL_TRIANGLE)
    ps3_buttons |= PS3_CTRL_TRIANGLE;

  if (psp_buttons & PSP_CTRL_RTRIGGER)
    ps3_buttons |= PS3_CTRL_R1;

  if (psp_buttons & PSP_CTRL_LTRIGGER)
    ps3_buttons |= PS3_CTRL_L1;

  return ps3_buttons;
}

static int sceCtrlReadBufferPositivePatched(SceCtrlData *pad_data, int count) {
  int res = sceCtrlReadBufferPositive(pad_data, count);
  int k1 = pspSdkSetK1(0);

  if (init_mode == 2) {
    memcpy(&pad, pad_data, sizeof(SceCtrlData));
    pad_data->Buttons = 0;
    pad_data->Lx = 128;
    pad_data->Ly = 128;
    pad_data->Rsrv[0] = 128;
    pad_data->Rsrv[1] = 128;
  }

  pspSdkSetK1(k1);
  return res;
}

static int sceIoReadPatched(SceUID fd, void *data, SceSize size) {
  int k1 = pspSdkSetK1(0);

  // Check if it's the fake UID
  if (fd == FAKE_UID) {
    // Wait to relieve thread, since sceIoRead on usbpspcm0: is actually a blocking function
    sceDisplayWaitVblankStart();

    int len = 0;

    if (init_mode == 0) {
      // Activate Resistance Plus
      sprintf(data, "%1d%1d", 1, 1);
      len = 3;
      init_mode++;
    } else if (init_mode == 1) {
      // Activate infected mode if desired
      SceIoStat stat;
      memset(&stat, 0, sizeof(SceIoStat));
      int infected_mode = sceIoGetstat("ms0:/seplugins/resistance_infected.bin", &stat) >= 0;
      sprintf(data, "%1d%1d", 2, infected_mode);
      len = 3;
      init_mode++;
    } else {
      // Fake PS3 controls
      sprintf(data, "%1d%04x%02x%02x%02x%02x", 0, convertButtons(pad.Buttons), pad.Rsrv[0], pad.Rsrv[1], pad.Lx, pad.Ly);
      len = 14;
    }

    pspSdkSetK1(k1);
    return len;
  }

  pspSdkSetK1(k1);
  return sceIoRead(fd, data, size);
}

static SceUID sceIoOpenPatched(const char *file, int flags, SceMode mode) {
  int k1 = pspSdkSetK1(0);

  // Fake UID
  if (strcmp(file, FAKE_DEVNAME) == 0) {
    pspSdkSetK1(k1);
    return FAKE_UID;
  }

  pspSdkSetK1(k1);
  return sceIoOpen(file, flags, mode);
}

static int sceIoWritePatched(SceUID fd, const void *data, SceSize size) {
  int k1 = pspSdkSetK1(0);

  // Fake success
  if (fd == FAKE_UID) {
    pspSdkSetK1(k1);
    return size;
  }

  pspSdkSetK1(k1);
  return sceIoWrite(fd, data, size);
}

static int sceIoClosePatched(SceUID fd) {
  int k1 = pspSdkSetK1(0);

  // Fake success
  if (fd == FAKE_UID) {
    pspSdkSetK1(k1);
    return 0;
  }

  pspSdkSetK1(k1);
  return sceIoClose(fd);
}

static int sceIoDevctlPatched(const char *dev, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen) {
  int k1 = pspSdkSetK1(0);

  if (cmd == 0x03415001) { // Fake connection for register
    u32 conn[2];
    conn[0] = 0;
    conn[1] = 0x81;
    int res = sceKernelStartThread(*(u32 *)indata, sizeof(conn), &conn);
    pspSdkSetK1(k1);
    return res;
  } else if (cmd == 0x03415002) { // Fake success for unregister
    pspSdkSetK1(k1);
    return 0;
  } else if (cmd == 0x03435005) { // Fake devname for bind
    strcpy(outdata, FAKE_DEVNAME);
    pspSdkSetK1(k1);
    return 0;
  }

  pspSdkSetK1(k1);
  return sceIoDevctl(dev, cmd, indata, inlen, outdata, outlen);
}

static int sceUsbStartPatched(const char *driverName, int size, void *args) {
  return 0;
}

static int sceUsbStopPatched(const char *driverName, int size, void *args) {
  return 0;
}

static int sceUsbActivatePatched(u32 pid) {
  return 0;
}

static int sceUsbDeactivatePatched(u32 pid) {
  return 0;
}

int OnModuleStart(SceModule2 *mod) {
  if (strcmp(mod->modname, "Resistance") == 0) {
    // Redirect ctrl function to dummy pad input
    sctrlHENPatchSyscall(FindProc("sceController_Service", "sceCtrl", 0x1F803938), sceCtrlReadBufferPositivePatched);

    // Redirect IO functions to fake usbpspcm0: communication
    sctrlHENPatchSyscall(FindProc("sceIOFileManager", "IoFileMgrForUser", 0x109F50BC), sceIoOpenPatched);
    sctrlHENPatchSyscall(FindProc("sceIOFileManager", "IoFileMgrForUser", 0x6A638D83), sceIoReadPatched);
    sctrlHENPatchSyscall(FindProc("sceIOFileManager", "IoFileMgrForUser", 0x42EC03AC), sceIoWritePatched);
    sctrlHENPatchSyscall(FindProc("sceIOFileManager", "IoFileMgrForUser", 0x810C4BC3), sceIoClosePatched);
    sctrlHENPatchSyscall(FindProc("sceIOFileManager", "IoFileMgrForUser", 0x54F5FB11), sceIoDevctlPatched);

    // Redirect USB functions to fake success
    sctrlHENPatchSyscall(FindProc("sceUSB_Driver", "sceUsb", 0xAE5DE6AF), sceUsbStartPatched);
    sctrlHENPatchSyscall(FindProc("sceUSB_Driver", "sceUsb", 0xC2464FA0), sceUsbStopPatched);
    sctrlHENPatchSyscall(FindProc("sceUSB_Driver", "sceUsb", 0x586DB82C), sceUsbActivatePatched);
    sctrlHENPatchSyscall(FindProc("sceUSB_Driver", "sceUsb", 0xC572A9C8), sceUsbDeactivatePatched);

    // Clear caches
    sceKernelDcacheWritebackAll();
    sceKernelIcacheClearAll();
  }

  if (!previous)
    return 0;

  return previous(mod);
}

int module_start(SceSize args, void *argp) {
  previous = sctrlHENSetStartModuleHandler(OnModuleStart);
  return 0;
}
