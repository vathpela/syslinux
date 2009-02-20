/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Erwan Velu - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * -----------------------------------------------------------------------
*/

#include "hdt-menu.h"



int start_menu_mode(char *version_string) {
  struct s_hdt_menu hdt_menu;
  struct s_hardware hardware;

  /* Cleaning structures */
  init_hardware(&hardware);
  memset(&hdt_menu,0,sizeof (hdt_menu));

  /* Detect every kind of hardware */
  detect_hardware(&hardware);

  /* Setup the menu system*/
  setup_menu(version_string);

  /* Compute all sub menus */
  compute_submenus(&hdt_menu, &hardware);

  /* Compute main menu */
  compute_main_menu(&hdt_menu,&hardware);

#ifdef WITH_MENU_DISPLAY
  t_menuitem * curr;
  char cmd[160];

  printf("Starting Menu (%d menus)\n",hdt_menu.total_menu_count);
  curr=showmenus(hdt_menu.main_menu.menu);
  /* When we exit the menu, do we have something to do */
  if (curr) {
        /* When want to execute something */
        if (curr->action == OPT_RUN)
        {
            strcpy(cmd,curr->data);

            /* Use specific syslinux call if needed */
            if (issyslinux())
               runsyslinuxcmd(cmd);
            else csprint(cmd,0x07);
            return 1; // Should not happen when run from SYSLINUX
        }
  }
#endif
  return 0;
}

/* In the menu system, what to do on keyboard timeout */
TIMEOUTCODE ontimeout()
{
         // beep();
            return CODE_WAIT;
}

/* Keyboard handler for the menu system */
void keys_handler(t_menusystem *ms, t_menuitem *mi,unsigned int scancode)
{
   char nc;

   if ((scancode >> 8) == F1) { // If scancode of F1
      runhelpsystem(mi->helpid);
   }
   // If user hit TAB, and item is an "executable" item
   // and user has privileges to edit it, edit it in place.
   if (((scancode & 0xFF) == 0x09) && (mi->action == OPT_RUN)) {
//(isallowed(username,"editcmd") || isallowed(username,"root"))) {
     nc = getnumcols();
     // User typed TAB and has permissions to edit command line
     gotoxy(EDITPROMPT,1,ms->menupage);
     csprint("Command line:",0x07);
     editstring(mi->data,ACTIONLEN);
     gotoxy(EDITPROMPT,1,ms->menupage);
     cprint(' ',0x07,nc-1,ms->menupage);
   }
}

/* Setup the Menu system*/
void setup_menu(char *version) {
   /* Creating the menu */
  init_menusystem(version);
  set_window_size(0,0,24,80);

 // Register the menusystem handler
 // reg_handler(HDLR_SCREEN,&msys_handler);
  reg_handler(HDLR_KEYS,&keys_handler);

  // Register the ontimeout handler, with a time out of 10 seconds
  reg_ontimeout(ontimeout,1000,0);
}

/* Compute Main' Submenus*/
void compute_submenus(struct s_hdt_menu *hdt_menu, struct s_hardware *hardware) {
 /* Compute this menus if a DMI table exist */
  if (hardware->is_dmi_valid) {
    compute_motherboard(&(hdt_menu->mobo_menu),&(hardware->dmi));
    compute_chassis(&(hdt_menu->chassis_menu),&(hardware->dmi));
    compute_system(&(hdt_menu->system_menu),&(hardware->dmi));
    compute_memory(hdt_menu,&(hardware->dmi));
    compute_bios(&(hdt_menu->bios_menu),&(hardware->dmi));
    compute_battery(&(hdt_menu->battery_menu),&(hardware->dmi));
  }

  compute_processor(&(hdt_menu->cpu_menu),hardware);
  compute_disks(hdt_menu,hardware->disk_info);
#ifdef WITH_PCI
  compute_PCI(hdt_menu,hardware);
  compute_kernel(&(hdt_menu->kernel_menu),hardware);
#endif
  compute_syslinuxmenu(&(hdt_menu->syslinux_menu));
  compute_aboutmenu(&(hdt_menu->about_menu));
}

void compute_main_menu(struct s_hdt_menu *hdt_menu,struct s_hardware *hardware) {

  /* Let's count the number of menu we have */
  hdt_menu->total_menu_count=0;
  hdt_menu->main_menu.items_count=0;

  hdt_menu->main_menu.menu = add_menu(" Main Menu ",-1);
  set_item_options(-1,24);

#ifdef WITH_PCI
  add_item("PCI <D>evices","PCI Devices Menu",OPT_SUBMENU,NULL,hdt_menu->pci_menu.menu);
  hdt_menu->main_menu.items_count++;
  hdt_menu->total_menu_count+=hdt_menu->pci_menu.items_count;
#endif
  if (hdt_menu->disk_menu.items_count>0) {
     add_item("<D>isks","Disks Menu",OPT_SUBMENU,NULL,hdt_menu->disk_menu.menu);
     hdt_menu->main_menu.items_count++;
     hdt_menu->total_menu_count+=hdt_menu->disk_menu.items_count;
  }

  if (hdt_menu->memory_menu.items_count>0) {
     add_item("<M>emory Modules","Memory Modules Menu",OPT_SUBMENU,NULL,hdt_menu->memory_menu.menu);
     hdt_menu->main_menu.items_count++;
     hdt_menu->total_menu_count+=hdt_menu->memory_menu.items_count;
  }
  add_item("<P>rocessor","Main Processor Menu",OPT_SUBMENU,NULL,hdt_menu->cpu_menu.menu);
  hdt_menu->main_menu.items_count++;

if (hardware->is_dmi_valid) {
  add_item("M<o>therboard","Motherboard Menu",OPT_SUBMENU,NULL,hdt_menu->mobo_menu.menu);
  hdt_menu->main_menu.items_count++;
  add_item("<B>ios","Bios Menu",OPT_SUBMENU,NULL,hdt_menu->bios_menu.menu);
  hdt_menu->main_menu.items_count++;
  add_item("<C>hassis","Chassis Menu",OPT_SUBMENU,NULL,hdt_menu->chassis_menu.menu);
  hdt_menu->main_menu.items_count++;
  add_item("<S>ystem","System Menu",OPT_SUBMENU,NULL,hdt_menu->system_menu.menu);
  hdt_menu->main_menu.items_count++;
  add_item("Ba<t>tery","Battery Menu",OPT_SUBMENU,NULL,hdt_menu->battery_menu.menu);
  hdt_menu->main_menu.items_count++;
}
  add_item("","",OPT_SEP,"",0);
#ifdef WITH_PCI
  add_item("<K>ernel Modules","Kernel Modules Menu",OPT_SUBMENU,NULL,hdt_menu->kernel_menu.menu);
  hdt_menu->main_menu.items_count++;
#endif
  add_item("<S>yslinux","Syslinux Information Menu",OPT_SUBMENU,NULL,hdt_menu->syslinux_menu.menu);
  hdt_menu->main_menu.items_count++;
  add_item("<A>bout","About Menu",OPT_SUBMENU,NULL,hdt_menu->about_menu.menu);
  hdt_menu->main_menu.items_count++;

  hdt_menu->total_menu_count+=hdt_menu->main_menu.items_count;
}

void detect_hardware(struct s_hardware *hardware) {
  printf("CPU: Detecting\n");
  cpu_detect(hardware);

  printf("DISKS: Detecting\n");
  detect_disks(hardware);

  printf("DMI: Detecting Table\n");
  if (detect_dmi(hardware) == -ENODMITABLE ) {
   printf("DMI: ERROR ! Table not found ! \n");
   printf("DMI: Many hardware components will not be detected ! \n");
  } else {
   printf("DMI: Table found ! (version %d.%d)\n",hardware->dmi.dmitable.major_version,hardware->dmi.dmitable.minor_version);
  }
#ifdef WITH_PCI
  detect_pci(hardware);
#endif
}
