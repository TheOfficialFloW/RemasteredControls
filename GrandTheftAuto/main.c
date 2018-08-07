/*
  Remastered Controls: GTA
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

PSP_MODULE_INFO("GTARemastered", 0x1007, 1, 0);

int sceKernelQuerySystemCall(void *function);

#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2) & 0x03FFFFFF), a);

#define REDIRECT_FUNCTION(a, f) \
{ \
  u32 func = a; \
  _sw(0x08000000 | (((u32)(f) >> 2) & 0x03FFFFFF), func); \
  _sw(0, func + 4); \
}

enum GtaPad {
  PAD_LX = 1,
  PAD_LY = 2,
  PAD_RX = 3,
  PAD_RY = 4,
  PAD_RTRIGGER = 7,
  PAD_CROSS = 21,
};

static STMOD_HANDLER previous;

static u32 cameraXStub, cameraYStub, aimXStub, aimYStub,
           vcsAccelerationStub, vcsAccelerationNormalStub, lcsAccelerationStub;

static u32 MakeSyscallStub(void *function) {
  SceUID block_id = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER, "", PSP_SMEM_High, 2 * sizeof(u32), NULL);
  u32 stub = (u32)sceKernelGetBlockHeadAddr(block_id);
  _sw(0x03E00008, stub);
  _sw(0x0000000C | (sceKernelQuerySystemCall(function) << 6), stub + 4);
  return stub;
}

short cameraX(short *pad) {
  return pad[PAD_RX];
}

short cameraY(short *pad) {
  return pad[PAD_RY];
}

short aimX(short *pad) {
  return pad[PAD_LX] ? pad[PAD_LX] : pad[PAD_RX];
}

short aimY(short *pad) {
  return pad[PAD_LY] ? pad[PAD_LY] : pad[PAD_RY];
}

short vcsAcceleration(short *pad) {
  if (pad[77] == 0)
    return pad[PAD_RTRIGGER];
  return 0;
}

short vcsAccelerationNormal(short *pad) {
  if (pad[77] == 0)
    return pad[PAD_CROSS];
  return 0;
}

short lcsAcceleration(short *pad) {
  if (pad[67] == 0)
    return pad[PAD_RTRIGGER];
  return 0;
}

static int PatchVCS(u32 addr, u32 text_addr) {
  // Implement right analog stick
  if (_lw(addr + 0x00) == 0x10000006 && _lw(addr + 0x04) == 0xA3A70003) {
    _sw(0xA7A50000 | (_lh(addr + 0x1C)), addr + 0x24);      // sh $a1, X($sp)
    _sw(0x97A50000 | (_lh(addr - 0xC) + 0x2), addr + 0x1C); // lhu $a1, X($sp)
    return 1;
  }

  // Redirect camera movement
  if (_lw(addr + 0x00) == 0x14800036 && _lw(addr + 0x10) == 0x10400016) {
    _sw(0x00000000, addr + 0x00);
    _sw(0x10000016, addr + 0x10);
    MAKE_CALL(addr + 0x8C, cameraXStub);
    _sw(0x00000000, addr + 0x108);
    _sw(0x10000002, addr + 0x118);
    MAKE_CALL(addr + 0x144, cameraYStub);
    return 1;
  }

  // Redirect gun aim movement
  if (_lw(addr + 0x00) == 0x04800040 && _lw(addr + 0x08) == 0x1080003E) {
    MAKE_CALL(addr + 0x50, aimXStub);
    MAKE_CALL(addr + 0x7C, aimXStub);
    MAKE_CALL(addr + 0x8C, aimXStub);
    MAKE_CALL(addr + 0x158, aimYStub);
    MAKE_CALL(addr + 0x1BC, aimYStub);
    return 1;
  }

  // Swap R trigger and cross button
  if (_lw(addr + 0x00) == 0x9485009A && _lw(addr + 0x04) == 0x0005282B &&
      _lw(addr + 0x08) == 0x30A500FF && _lw(addr + 0x0C) == 0x10A00003 &&
      _lw(addr + 0x10) == 0x00000000 && _lw(addr + 0x14) == 0x10000002 &&
      _lw(addr + 0x18) == 0x00001025 && _lw(addr + 0x1C) == 0x8482002A) {
    REDIRECT_FUNCTION(addr + 0x00, vcsAccelerationStub);
    return 1;
  }

  // Use normal button for flying plane
  if (_lw(addr + 0x00) == 0xC60E0780 && _lw(addr + 0x04) == 0x460D6302 &&
      _lw(addr + 0x08) == 0x460D7342) {
    MAKE_CALL(addr + 0x1C, vcsAccelerationNormalStub);
    MAKE_CALL(addr + 0x3D0, vcsAccelerationNormalStub);
    return 1;
  }

  // Use normal button for flying helicoper
  if (_lw(addr + 0x00) == 0x16400018 && _lw(addr + 0xC) == 0x02202025) {
    MAKE_CALL(addr + 0x14, vcsAccelerationNormalStub);
    return 1;
  }

  if (_lw(addr + 0x00) == 0x1480000C && _lw(addr + 0x10) == 0x50A0000A) {
    _sh(PAD_RTRIGGER * 2, addr + 0x20);
    _sh(PAD_RTRIGGER * 2, addr + 0x68);
    _sh(PAD_CROSS * 2, addr + 0x80);
    return 1;
  }

  // Allow using L trigger when walking
  if (_lw(addr + 0x00) == 0x1480000E && _lw(addr + 0x10) == 0x10800008 &&
      _lw(addr + 0x1C) == 0x04800003) {
    _sw(0, addr + 0x10);
    _sw(0, addr + 0x9C);
    return 1;
  }

  // Force L trigger value in the L+camera movement function
  if (_lw(addr + 0x00) == 0x84C7000A) {
    _sw(0x2407FFFF, addr + 0x00);
    return 1;
  }

  return 0;
}

static int PatchLCS(u32 addr, u32 text_addr) {
  // Implement right analog stick
  if (_lw(addr + 0x00) == 0x10000006 && _lw(addr + 0x04) == 0xA3A70013) {
    _sw(0xA7A50000 | (_lh(addr + 0x1C)), addr + 0x24);      // sh $a1, X($sp)
    _sw(0x97A50000 | (_lh(addr - 0xC) + 0x2), addr + 0x1C); // lhu $a1, X($sp)
    return 1;
  }

  // Redirect camera movement
  if (_lw(addr + 0x00) == 0x14800034 && _lw(addr + 0x10) == 0x10400014) {
    _sw(0x00000000, addr + 0x00);
    _sw(0x10000014, addr + 0x10);
    MAKE_CALL(addr + 0x84, cameraXStub);
    _sw(0x00000000, addr + 0x100);
    _sw(0x10000002, addr + 0x110);
    MAKE_CALL(addr + 0x13C, cameraYStub);
    return 1;
  }

  // Redirect gun aim movement
  if (_lw(addr + 0x00) == 0x04800036 && _lw(addr + 0x08) == 0x10800034) {
    MAKE_CALL(addr + 0x3C, aimXStub);
    MAKE_CALL(addr + 0x68, aimXStub);
    MAKE_CALL(addr + 0x78, aimXStub);
    MAKE_CALL(addr + 0x130, aimYStub);
    MAKE_CALL(addr + 0x198, aimYStub);
    return 1;
  }

  // Swap R trigger and cross button
  if (_lw(addr + 0x00) == 0x94850086 && _lw(addr + 0x04) == 0x10A00003 &&
      _lw(addr + 0x08) == 0x00000000 && _lw(addr + 0x0C) == 0x10000002 &&
      _lw(addr + 0x10) == 0x00001025 && _lw(addr + 0x14) == 0x8482002A) {
    REDIRECT_FUNCTION(addr + 0x00, lcsAccelerationStub);
    return 1;
  }

  if (_lw(addr + 0x00) == 0x14A0000E && _lw(addr + 0x10) == 0x50C0000C) {
    _sh(PAD_RTRIGGER * 2, addr + 0x20);
    _sh(PAD_CROSS * 2, addr + 0x50);
    return 1;
  }

  // Allow using L trigger when walking
  if (_lw(addr + 0x00) == 0x14A0000E && _lw(addr + 0x10) == 0x10A00008 &&
      _lw(addr + 0x1C) == 0x04A00003) {
    _sw(0, addr + 0x10);
    _sw(0, addr + 0x74);
    return 1;
  }

  // Force L trigger value in the L+camera movement function
  if (_lw(addr + 0x00) == 0x850A000A) {
    _sw(0x240AFFFF, addr + 0x00);
    return 1;
  }

  return 0;
}

int OnModuleStart(SceModule2 *mod) {
  char *modname = mod->modname;
  u32 text_addr = mod->text_addr;

  int gta_version = -1;

  if (strcmp(modname, "GTA3") == 0) {
    cameraXStub = MakeSyscallStub(cameraX);
    cameraYStub = MakeSyscallStub(cameraY);
    aimXStub = MakeSyscallStub(aimX);
    aimYStub = MakeSyscallStub(aimY);
    vcsAccelerationStub = MakeSyscallStub(vcsAcceleration);
    vcsAccelerationNormalStub = MakeSyscallStub(vcsAccelerationNormal);
    lcsAccelerationStub = MakeSyscallStub(lcsAcceleration);

    u32 i;
    for (i = 0; i < mod->text_size; i += 4) {
      u32 addr = text_addr + i;

      if ((gta_version == -1 || gta_version == 0) && PatchVCS(addr, text_addr)) {
        gta_version = 0;
        continue;
      }

      if ((gta_version == -1 || gta_version == 1) && PatchLCS(addr, text_addr)) {
        gta_version = 1;
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
  previous = sctrlHENSetStartModuleHandler(OnModuleStart);
  return 0;
}
