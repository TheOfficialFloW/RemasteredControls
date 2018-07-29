/*
  Remastered Controls: Prince Of Persia
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

PSP_MODULE_INFO("POPRemastered", 0x1007, 1, 0);

int sceKernelQuerySystemCall(void *function);

#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2) & 0x03FFFFFF), a);

static STMOD_HANDLER previous;

static u32 MakeSyscallStub(void *function) {
  SceUID block_id = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER, "", PSP_SMEM_High, 2 * sizeof(u32), NULL);
  u32 stub = (u32)sceKernelGetBlockHeadAddr(block_id);
  _sw(0x03E00008, stub);
  _sw(0x0000000C | (sceKernelQuerySystemCall(function) << 6), stub + 4);
  return stub;
}

int getInput(float *input) {
  int k1 = pspSdkSetK1(0);

  SceCtrlData pad;
  sceCtrlPeekBufferPositive(&pad, 1);

  input[0] = (float)(pad.Lx - 127) / 128.0f;
  input[1] = (float)(127 - pad.Ly) / 128.0f;
  input[2] = (float)(pad.Rsrv[0] - 127) / 128.0f;
  input[3] = (float)(127 - pad.Rsrv[1]) / 128.0f;

  pspSdkSetK1(k1);
  return 0;
}

int OnModuleStart(SceModule2 *mod) {
  int once = 0;

  if (strcmp(mod->modname, "pop") == 0) {
    u32 i;
    for (i = 0; i < 0x00800000; i += 4) {
      u32 addr = mod->text_addr + i;

      // Get analog input on our own
      if (_lw(addr + 0x00) == 0x3C023C00 && _lw(addr + 0x08) == 0x2484FF81) {
        _sw(0, addr + 0x34);
        _sw(0, addr + 0x4C);

        _sw(0x03A02021, addr + 0x00); // move $a0, $sp
        MAKE_CALL(addr + 0x04, MakeSyscallStub(getInput));
        _sw(0x24840054, addr + 0x08); // addiu $a0, $a0, 0x54

        continue;
      } else if (_lw(addr + 0x00) == 0x3C033C00 && _lw(addr + 0x08) == 0x24A5FF81) {
        _sh(0x5000, addr - 0x16);

        _sw(0, addr + 0x24);
        _sw(0, addr + 0x34);

        _sw(0x03A02021, addr + 0x00); // move $a0, $sp
        MAKE_CALL(addr + 0x04, MakeSyscallStub(getInput));
        _sw(0x24840064, addr + 0x08); // addiu $a0, $a0, 0x64

        continue;
      }

      if ((_lw(addr + 0x00) == 0x5220007A && _lw(addr + 0x04) == 0xC7A20064) ||
          (_lw(addr + 0x00) == 0x5200007A && _lw(addr + 0x04) == 0xC7A20054)) {
        // Handle camera
        _sw(0, addr + 0x00);
        _sw(0, addr + 0x04);

        // Point to Rx
        _sh(_lh(addr + 0x08) + 0x08, addr + 0x08);

        continue;
      }

      if (_lw(addr + 0x00) == 0x10000078 && (_lw(addr + 0x08) == 0xC7A20054 || _lw(addr + 0x08) == 0xC7A20064)) {
        // Handle walking
        _sw(0, addr + 0x00);

        // Disable some writes
        _sw(0, addr + 0x04);
        _sw(0, addr - 0x08);
        // _sw(0, addr - 0x10);
        // _sw(0, addr - 0x18);

        continue;
      }

      // Point to Ry
      if (_lw(addr + 0x00) == 0x46010002 && (_lw(addr + 0x08) == 0xC7A20058 || _lw(addr + 0x08) == 0xC7A20068)) {
        if (!once) {
          _sh(_lh(addr + 0x08) + 0x08, addr + 0x08);
          once = 1;
          continue;
        }
      }

      // Disable some writes
      if ((_lw(addr + 0x00) == 0xC7A1002C || _lw(addr + 0x00) == 0xC7A10034) &&
           _lw(addr + 0x0C) == 0x46011802 && _lw(addr + 0x48) != 0x44800800) {
        // _sw(0, addr + 0x24);
        // _sw(0, addr + 0x2C);
        _sw(0, addr + 0x34);
        _sw(0, addr + 0x3C);
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
