/*
  Remastered Controls: Tomb Raider
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

PSP_MODULE_INFO("TombRaiderRemastered", 0x1007, 1, 0);

int sceKernelQuerySystemCall(void *function);

#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2) & 0x03FFFFFF), a);
#define EXTRACT_CALL(x) ((((u32)_lw((u32)x)) & ~0x0C000000) << 2)

static STMOD_HANDLER previous;
static int (* getInput)(int port, float *input);

static u32 MakeSyscallStub(void *function) {
  SceUID block_id = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER, "", PSP_SMEM_High, 2 * sizeof(u32), NULL);
  u32 stub = (u32)sceKernelGetBlockHeadAddr(block_id);
  _sw(0x03E00008, stub);
  _sw(0x0000000C | (sceKernelQuerySystemCall(function) << 6), stub + 4);
  return stub;
}

int getInputPatched(int port, float *input) {
  int k1 = pspSdkSetK1(0);
  int res = getInput(port, input);

  SceCtrlData pad;
  sceCtrlPeekBufferPositive(&pad, 1);

  input[1] = (float)(pad.Lx - 128);
  input[2] = (float)(pad.Ly - 128);
  input[3] = (float)(pad.Rsrv[0] - 128);
  input[4] = (float)(pad.Rsrv[1] - 128);

  pspSdkSetK1(k1);
  return res;
}

int OnModuleStart(SceModule2 *mod) {
  char *modname = mod->modname;
  u32 text_addr = mod->text_addr;

  if (strcmp(modname, "tr7") == 0) {
    u32 i;
    for (i = 0; i < mod->text_size; i += 4) {
      if (_lw(text_addr + i) == 0x10400015) {
        getInput = (void *)(0x80000000 | EXTRACT_CALL(text_addr + i - 8));
        MAKE_CALL(text_addr + i - 8, MakeSyscallStub(getInputPatched));
        break;
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
