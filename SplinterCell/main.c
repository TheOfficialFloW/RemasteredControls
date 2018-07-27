/*
  Remastered Controls: Splinter Cell
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
#include <math.h>

#include <systemctrl.h>

PSP_MODULE_INFO("SplinterCellRemastered", 0x1007, 1, 0);

int sceKernelQuerySystemCall(void *function);

#define REDIRECT_FUNCTION(a, f) \
{ \
  u32 func = a; \
  _sw(0x08000000 | (((u32)(f) >> 2) & 0x03FFFFFF), func); \
  _sw(0, func + 4); \
}

#define MAKE_DUMMY_FUNCTION(a, r) \
{ \
  u32 func = a; \
  if (r == 0) { \
    _sw(0x03E00008, func); \
    _sw(0x00001021, func + 4); \
  } else { \
    _sw(0x03E00008, func); \
    _sw(0x24020000 | r, func + 4); \
  } \
}

#define DEADZONE 0.3f

static STMOD_HANDLER previous;

static u32 MakeSyscallStub(void *function) {
  SceUID block_id = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER, "", PSP_SMEM_High, 2 * sizeof(u32), NULL);
  u32 stub = (u32)sceKernelGetBlockHeadAddr(block_id);
  _sw(0x03E00008, stub);
  _sw(0x0000000C | (sceKernelQuerySystemCall(function) << 6), stub + 4);
  return stub;
}

static float getAxis(float axis, float deadzone, float speed) {
  if (fabsf(axis) >= deadzone) {
    if (axis < 0.0f) {
      return speed * ((axis + deadzone) / (1.0f - deadzone));
    } else {
      return speed * ((axis - deadzone) / (1.0f - deadzone));
    }
  }

  return 0.0f;
}

int __0fGUInputKDirectAxis6JEInputKeyfTCPatched(int this, int axis) {
  int k1 = pspSdkSetK1(0);

  int param = _lw(this) + 0x130;
  int arg1 = this + _lh(param);
  int arg2 = _lw(_lw(this + 0x12A0) + 0x34);

  float *(* __0fGUInputMFindAxisNameP6GAActorPCcK)(int arg1, int arg2, const char *name);
  __0fGUInputMFindAxisNameP6GAActorPCcK = (void *)(0x80000000 | _lw(param + 4));
  if (__0fGUInputMFindAxisNameP6GAActorPCcK) {
    SceCtrlData pad;
    sceCtrlPeekBufferPositive(&pad, 1);

    if (axis == 0xE0) {
      float Lx = ((float)pad.Lx - 128.0f) / 128.0f;
      float Rx = ((float)pad.Rsrv[0] - 128.0f) / 128.0f;

      float *aStrafe  = __0fGUInputMFindAxisNameP6GAActorPCcK(arg1, arg2, "aStrafe");
      float *aTurn  = __0fGUInputMFindAxisNameP6GAActorPCcK(arg1, arg2, "aTurn");

      if (aStrafe)
        *aStrafe = getAxis(Lx, DEADZONE, 1.0f);
      if (aTurn)
        *aTurn = getAxis(Rx, DEADZONE, 1.0f);
    } else if (axis == 0xE1) {
      float Ly = -(((float)pad.Ly - 128.0f) / 128.0f);
      float Ry = -(((float)pad.Rsrv[1] - 128.0f) / 128.0f);

      float *aForward  = __0fGUInputMFindAxisNameP6GAActorPCcK(arg1, arg2, "aForward");
      float *aLookUp  = __0fGUInputMFindAxisNameP6GAActorPCcK(arg1, arg2, "aLookUp");

      if (aForward)
        *aForward = getAxis(Ly, DEADZONE, 1.0f);
      if (aLookUp)
        *aLookUp = getAxis(Ry, DEADZONE, -1.0f);
    }
  }

  pspSdkSetK1(k1);
  return 0;
}

int OnModuleStart(SceModule2 *mod) {
  char *modname = mod->modname;
  u32 text_addr = mod->text_addr;

  if (strcmp(modname, "SplinterCellPSP") == 0) {
    u32 i;
    for (i = 0; i < mod->text_size; i += 4) {
      // __0fJAEQInvHUDODisplayCamInfoP6IUECanvasi
      if (_lw(text_addr + i) == 0x27BDFFB0 && _lw(text_addr + i + 4) == 0xC48C03B0) {
        MAKE_DUMMY_FUNCTION(text_addr+i, 0);
      }

      // __0fGUInputKDirectAxis6JEInputKeyfTC
      if (_lw(text_addr + i) == 0x27BDFF70 && _lw(text_addr + i + 4) == 0xAFB00068) {
        REDIRECT_FUNCTION(text_addr+i, MakeSyscallStub(__0fGUInputKDirectAxis6JEInputKeyfTCPatched));
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
  char *filename = sceKernelInitFileName();
  if (!filename || strcmp(filename, "disc0:/PSP_GAME/USRDIR/SYSTEM/SPLINTERCELL.PRX") != 0) {
    return 1;
  }

  sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
  previous = sctrlHENSetStartModuleHandler(OnModuleStart);
  return 0;
}
