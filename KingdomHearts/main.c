/*
  Remastered Controls: Kingdom Hearts
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

PSP_MODULE_INFO("KHBBSRemastered", 0x1007, 1, 0);

int sceKernelQuerySystemCall(void *function);

#define MAKE_SYSCALL(a, n) _sw(0x0000000C | (n << 6), a);

static STMOD_HANDLER previous;

float leftAnalogX() {
  SceCtrlData pad;

  int k1 = pspSdkSetK1(0);
  sceCtrlPeekBufferPositive(&pad, 1);
  pspSdkSetK1(k1);

  return (float)(pad.Rsrv[0] - 128) / 128.0f;
}

float leftAnalogY() {
  SceCtrlData pad;

  int k1 = pspSdkSetK1(0);
  sceCtrlPeekBufferPositive(&pad, 1);
  pspSdkSetK1(k1);

  return (float)(pad.Rsrv[1] - 128) / 128.0f;
}

int OnModuleStart(SceModule2 *mod) {
  if (strcmp(mod->modname, "MainApp") == 0) {
    u32 i;
    for (i = 0; i < mod->text_size; i += 4) {
      u32 addr = mod->text_addr + i;

      // Fake L trigger as held
      if (_lw(addr + 0x00) == 0x30840004 && _lw(addr + 0x04) == 0x508000BA) {
        _sw(0, addr + 0x04);
        continue;
      }

      if (_lw(addr + 0x00) == 0xAFBF0008 && _lw(addr + 0x04) == 0x1C80000B) {
        // Fake control type B
        _sw(0, addr + 0x04);

        // Redirect left analog stick to right analog stick
        if (_lw(addr + 0x08) == 0x46007506) {
          u32 syscall = sceKernelQuerySystemCall(leftAnalogX);
          _sw(_lw(addr + 0x18), addr + 0x14);
          _sw(_lw(addr + 0x58), addr + 0x54);
          MAKE_SYSCALL(addr + 0x18, syscall);
          MAKE_SYSCALL(addr + 0x58, syscall);
        } else if (_lw(addr + 0x08) == 0x4480A000) {
          u32 syscall = sceKernelQuerySystemCall(leftAnalogY);
          _sw(_lw(addr + 0x18), addr + 0x14);
          _sw(_lw(addr + 0x58), addr + 0x54);
          MAKE_SYSCALL(addr + 0x18, syscall);
          MAKE_SYSCALL(addr + 0x58, syscall);
        }

        continue;
      }
    }

    sceKernelDcacheWritebackAll();
    sceKernelIcacheClearAll();
  }

  if (!previous)
    return 0;

  return previous(mod);
}

int module_start(SceSize args, void *argp) {
  sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
  previous = sctrlHENSetStartModuleHandler(OnModuleStart);
  return 0;
}
