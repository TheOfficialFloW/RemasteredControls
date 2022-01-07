/*
  Remastered Controls: Metal Gear Solid
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

PSP_MODULE_INFO("MGSRemastered", 0x1007, 1, 0);

int sceKernelQuerySystemCall(void *function);

#define MAKE_JUMP(a, f) _sw(0x08000000 | (((u32)(f) & 0x0FFFFFFC) >> 2), a);

#define HIJACK_FUNCTION(a, f, ptr) \
{ \
  u32 _func_ = a; \
  static u32 patch_buffer[3]; \
  _sw(_lw(_func_), (u32)patch_buffer); \
  _sw(_lw(_func_ + 4), (u32)patch_buffer + 8);\
  MAKE_JUMP((u32)patch_buffer + 4, _func_ + 8); \
  _sw(0x08000000 | (((u32)(f) >> 2) & 0x03FFFFFF), _func_); \
  _sw(0, _func_ + 4); \
  ptr = (void *)patch_buffer; \
}

#define EMULATOR_DEVCTL__IS_EMULATOR     0x00000003

static STMOD_HANDLER previous;

static int (* getCamera)(int *input);
static int (* getInputPw)(char *input);
static int (* getInputPo)(char *input);

static u32 MakeSyscallStub(void *function) {
  SceUID block_id = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER, "", PSP_SMEM_High, 2 * sizeof(u32), NULL);
  u32 stub = (u32)sceKernelGetBlockHeadAddr(block_id);
  _sw(0x03E00008, stub);
  _sw(0x0000000C | (sceKernelQuerySystemCall(function) << 6), stub + 4);
  return stub;
}

int getCameraPatched(int *input) {
  int k1 = pspSdkSetK1(0);

  SceCtrlData pad;
  sceCtrlPeekBufferPositive(&pad, 1);

  input[752/4] = pad.Rsrv[0] - 128;
  input[756/4] = pad.Rsrv[1] - 128;

  pspSdkSetK1(k1);
  return getCamera(input);
}

int getInputPwPatched(char *input) {
  int res = getInputPw(input);
  int k1 = pspSdkSetK1(0);

  SceCtrlData pad;
  sceCtrlPeekBufferPositive(&pad, 1);

  if (pad.Rsrv[0] < 64 || pad.Rsrv[0] > 192)
    input[2046] = pad.Rsrv[0] - 128;

  if (pad.Rsrv[1] < 64 || pad.Rsrv[1] > 192)
    input[2047] = pad.Rsrv[1] - 128;

  pspSdkSetK1(k1);
  return res;
}

int getInputPoPatched(char *input) {
  int res = getInputPo(input);
  int k1 = pspSdkSetK1(0);

  SceCtrlData pad;
  sceCtrlPeekBufferPositive(&pad, 1);

  if (pad.Rsrv[0] < 64 || pad.Rsrv[0] > 192)
    input[1842] = pad.Rsrv[0] - 128;

  if (pad.Rsrv[1] < 64 || pad.Rsrv[1] > 192)
    input[1843] = pad.Rsrv[1] - 128;

  pspSdkSetK1(k1);
  return res;
}

void applyPatch(u32 text_addr, u32 text_size, int emulator){
  u32 i;
  for (i = 0; i < text_size; i += 4) {
    u32 addr = text_addr + i;

    int po = (_lw(addr + 0x00) == 0x5040004F && _lw(addr + 0x04) == 0x02402021 && _lw(addr + 0x08) == 0xAE0002F0);
    int pw = (_lw(addr + 0x00) == 0x1040004F && _lw(addr + 0x04) == 0x00000000 && _lw(addr + 0x08) == 0xAE0002F0);
    if (po || pw) {
      // Skip branch
      _sw(0, addr + 0x00);

      // Don't reset camera
      _sw(0, addr + 0x08);
      _sw(0, addr + 0x0C);

      // Redirect camera function
      if (pw) {
        if(emulator == 0){
          HIJACK_FUNCTION(addr - 0x38, MakeSyscallStub(getCameraPatched), getCamera);
        }else{
          HIJACK_FUNCTION(addr - 0x38, getCameraPatched, getCamera);
        }
      } else {
        if(emulator == 0){
          HIJACK_FUNCTION(addr - 0x3C, MakeSyscallStub(getCameraPatched), getCamera);
        }else{
          HIJACK_FUNCTION(addr - 0x3C, getCameraPatched, getCamera);
        }
      }

      continue;
    }

    // Redirect input for camera in aiming mode
    if (_lw(addr + 0x00) == 0x27BDFFD0 && _lw(addr + 0x0C) == 0x00808021 && _lw(addr + 0x28) == 0xE7B40020) {
      if(emulator == 0){
        HIJACK_FUNCTION(addr + 0x00, MakeSyscallStub(getInputPwPatched), getInputPw);
      }else{
        HIJACK_FUNCTION(addr + 0x00, getInputPwPatched, getInputPw);
      }
      continue;
    }
    if (_lw(addr + 0x00) == 0x27BDFFC0 && _lw(addr + 0x0C) == 0x2407FFFF && _lw(addr + 0x38) == 0xE7B40030) {
      if(emulator == 0){
        HIJACK_FUNCTION(addr + 0x00, MakeSyscallStub(getInputPoPatched), getInputPo);
      }else{
        HIJACK_FUNCTION(addr + 0x00, getInputPoPatched, getInputPo);
      }
      continue;
    }
  }

  sceKernelDcacheWritebackAll();
  sceKernelIcacheClearAll();
}

int OnModuleStart(SceModule2 *mod) {
  if (strcmp(mod->modname, "mgp_main") == 0) {
    applyPatch(mod->text_addr, mod->text_size, 0);
  }

  if (!previous)
    return 0;

  return previous(mod);
}

static void CheckModules() {
  SceUID modules[10];
  SceKernelModuleInfo info;
  int i, count = 0;

  if (sceKernelGetModuleIdList(modules, sizeof(modules), &count) >= 0) {
    for (i = 0; i < count; ++i) {
      info.size = sizeof(SceKernelModuleInfo);
      if (sceKernelQueryModuleInfo(modules[i], &info) < 0) {
        continue;
      }
      if (strcmp(info.name, "mgp_main") == 0) {
        applyPatch(info.text_addr, info.text_size, 1);
      }
    }
  }
}

int module_start(SceSize args, void *argp) {
  sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
  if (sceIoDevctl("kemulator:", EMULATOR_DEVCTL__IS_EMULATOR, NULL, 0, NULL, 0) == 0) {
    // Just scan the modules using normal/official syscalls.
    CheckModules();
  } else {
    previous = sctrlHENSetStartModuleHandler(OnModuleStart);
  }
  return 0;
}
