/*
  Remastered Controls: Warriors
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

#include <stdio.h>
#include <string.h>

#include <systemctrl.h>

PSP_MODULE_INFO("WarriorsRemastered", 0x1007, 1, 0);

static STMOD_HANDLER previous;

int OnModuleStart(SceModule2 *mod) {
  if (strcmp(mod->modname, "WARR") == 0) {
    u32 i;
    for (i = 0; i < mod->text_size; i += 4) {
      u32 addr = mod->text_addr + i;

      if (_lw(addr + 0x00) == 0x12600017) {
        _sw(0xA450001C, addr + 0x00); // sh $s0, 28($v0)
        _sw(0x93A6001B, addr + 0x04); // lbu $a2, 27($sp)
        _sw(0x93A5001A, addr + 0x08); // lbu $a1, 26($sp)
        _sw(0x10000014, addr + 0x0C); // b 0x14
        _sw(0xA226001B, addr + 0x6C); // sb $a2, 27($s1)
        _sw(0xA225001A, addr + 0x8C); // sb $a1, 26($s1)
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
  previous = sctrlHENSetStartModuleHandler(OnModuleStart);
  return 0;
}
