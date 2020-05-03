/*
  Remastered Controls: Tony Hawk
  Copyright (C) 2020, TheFloW

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

PSP_MODULE_INFO("TonyHawkRemastered", 0x1007, 1, 0);

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

int getInput(u8 *input, SceCtrlData *pad) {
  input[4] = 255 - pad->Rsrv[0];
  input[5] = 255 - pad->Rsrv[1];
  input[6] = pad->Lx;
  input[7] = pad->Ly;
  return 0;
}

int OnModuleStart(SceModule2 *mod) {
  char *modname = mod->modname;

  if (strcmp(modname, "hawkpsp") == 0) {
    u32 i;
    for (i = 0; i < mod->text_size; i += 4) {
      u32 addr = mod->text_addr + i;

      // Tony Hawk's Project 8
      if (_lw(addr) == 0xA206001C && _lw(addr + 0x04) == 0x34040080) {
        _sw(0x26040018, addr + 0x00); // addiu $a0, $s0, 24
        MAKE_CALL(addr + 0x04, MakeSyscallStub(getInput));
        _sw(0x27A50010, addr + 0x08); // addiu $a1, $sp, 16
        _sw(0, addr + 0x0C);
        _sw(0, addr + 0x14);
        break;
      }
/*
      // Tony Hawk's Underground 2: Remix
      if (_lw(addr) == 0xA204001C && _lw(addr + 0x04) == 0x93A40019) {
        _sw(0x26040018, addr + 0x00); // addiu $a0, $s0, 24
        MAKE_CALL(addr + 0x04, MakeSyscallStub(getInput));
        _sw(0x27A50010, addr + 0x08); // addiu $a1, $sp, 16
        _sw(0, addr + 0x0C);
        _sw(0, addr + 0x10);
        break;
      }
*/
    }

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
