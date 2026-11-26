// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <cedit.h>
#include <dm.h>
#include <env.h>
#include <expo.h>
#include <mapmem.h>
#include <dm/ofnode.h>
#include <test/cedit-test.h>
#include <test/ut.h>
#include <test/video.h>
#include "bootstd_common.h"
#include "expo_common.h"
#include "../../boot/scene_internal.h"

/* Check the cedit command */
static int cedit_base(struct unit_test_state *uts)
{
	extern struct expo *cur_exp;
	struct scene_obj_menu *menu;
	struct scene_obj_txt *txt;
	struct expo *exp;
	struct scene *scn;

	ut_assertok(run_command("cedit load hostfs - cedit.dtb", 0));

	/*
	 * ^N  Move down to second menu
	 * ^M  Open menu
	 * ^N  Move down to second item
	 * ^M  Select item
	 * \e  Quit
	 *
	 * cedit_run() returns -EACCESS so this command returns CMD_RET_FAILURE
	 */
	console_in_puts("\x0e\x0d\x0e\x0d\e");
	ut_asserteq(1, run_command("cedit run", 0));

	exp = cur_exp;
	scn = expo_lookup_scene_id(exp, exp->scene_id);
	ut_assertnonnull(scn);

	menu = scene_obj_find(scn, scn->highlight_id, SCENEOBJT_NONE);
	ut_assertnonnull(menu);

	txt = scene_obj_find(scn, menu->title_id, SCENEOBJT_NONE);
	ut_assertnonnull(txt);
	ut_asserteq_str("AC Power", expo_get_str(exp, txt->gen.str_id));

	ut_asserteq(ID_AC_ON, menu->cur_item_id);

	return 0;
}
BOOTSTD_TEST(cedit_base, UTF_CONSOLE);

/* Check that the usage message is shown, with its first and last lines */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("cedit - Configuration editor");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline(
		"cedit load <interface> <dev[:part]> <filename>   - load config editor");
	ut_assert_skip_to_line(
		"cedit run                                        - run config editor");
	ut_assert_console_end();

	return 0;
}

/* Check the cedit command's usage message */
static int cedit_usage(struct unit_test_state *uts)
{
	/* no sub-command */
	ut_asserteq(1, run_command("cedit", 0));
	ut_assertok(check_usage(uts));

	/* a sub-command which does not exist */
	ut_asserteq(1, run_command("cedit fred", 0));
	ut_assertok(check_usage(uts));

	/* too few arguments for load */
	ut_asserteq(1, run_command("cedit load hostfs -", 0));
	ut_assertok(check_usage(uts));

	/* too many arguments for dump */
	ut_asserteq(1, run_command("cedit dump extra", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
BOOTSTD_TEST(cedit_usage, UTF_CONSOLE);

/* Check the sub-commands which need an editor to have been loaded */
static int cedit_no_expo(struct unit_test_state *uts)
{
	extern struct expo *cur_exp;
	struct expo *old = cur_exp;

	cur_exp = NULL;

	ut_asserteq(1, run_command("cedit run", 0));
	ut_assert_nextline("No expo loaded");

	ut_asserteq(1, run_command("cedit read_fdt hostfs - settings.dtb", 0));
	ut_assert_nextline("No expo loaded");

	ut_asserteq(1, run_command("cedit write_fdt hostfs - settings.dtb", 0));
	ut_assert_nextline("No expo loaded");

	ut_asserteq(1, run_command("cedit read_env", 0));
	ut_assert_nextline("No expo loaded");

	ut_asserteq(1, run_command("cedit write_env", 0));
	ut_assert_nextline("No expo loaded");

	ut_asserteq(1, run_command("cedit read_cmos", 0));
	ut_assert_nextline("No expo loaded");

	ut_asserteq(1, run_command("cedit write_cmos", 0));
	ut_assert_nextline("No expo loaded");

	if (IS_ENABLED(CONFIG_CMD_CEDIT_DUMP)) {
		ut_asserteq(1, run_command("cedit dump", 0));
		ut_assert_nextline("No expo loaded");
	}

	ut_assert_console_end();
	cur_exp = old;

	return 0;
}
BOOTSTD_TEST(cedit_no_expo, UTF_CONSOLE);

/* Check the cedit load error paths */
static int cedit_fail(struct unit_test_state *uts)
{
	extern struct expo *cur_exp;
	struct udevice *dev;
	struct scene *scn;

	/* a file which is not there */
	ut_asserteq(1, run_command("cedit load hostfs - nothere.dtb", 0));
	ut_assert_nextline("File not found");
	ut_assert_console_end();

	/*
	 * write out the settings, then try to load them as a description: the
	 * file is a valid devicetree but has no expo in it
	 */
	ut_assertok(run_command("cedit load hostfs - cedit.dtb", 0));
	ut_assertok(uclass_first_device_err(UCLASS_VIDEO, &dev));
	ut_asserteq(ID_SCENE1, cedit_prepare(cur_exp, dev, &scn));
	ut_assertok(run_command("cedit write_fdt hostfs - settings.dtb", 0));

	ut_asserteq(1, run_command("cedit load hostfs - settings.dtb", 0));

	/* skip the log line from expo_build() */
	ut_assert_skip_to_line("Failed to build expo: -22: Invalid argument");
	ut_assert_console_end();

	/* the editor which failed to load is still the current one */
	ut_assertnonnull(cur_exp);

	return 0;
}
BOOTSTD_TEST(cedit_fail, UTF_CONSOLE);

/* Check the cedit dump command */
static int cedit_dump(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_CMD_CEDIT_DUMP))
		return -EAGAIN;

	ut_assertok(run_command("cedit load hostfs - cedit.dtb", 0));
	ut_assertok(run_command("cedit dump", 0));

	ut_assert_nextline("Expo: name 'name'");
	ut_assert_skip_to_line("Scenes:");
	ut_assert_nextline("  Scene 6: name 'main'");
	ut_assert_skip_to_line("    Object 8 (cpu-speed): type menu");
	ut_assert_skip_to_line(
		"        Item a: name 'cpu-speed.item-0' key_id 0 label_id 19 desc_id 0 preview_id 0");
	ut_assert_skip_to_line("    Object 12 (machine-name.edit): type text");
	ut_assert_skip_to_line("        str ''");

	/* the dump ends with a blank line of its own */
	console_record_reset();

	return 0;
}
BOOTSTD_TEST(cedit_dump, UTF_CONSOLE);

/* Check the cedit write_fdt and read_fdt commands */
static int cedit_fdt(struct unit_test_state *uts)
{
	struct scene_obj_textline *tline;
	struct video_priv *vid_priv;
	extern struct expo *cur_exp;
	struct scene_obj_menu *menu;
	struct udevice *dev;
	ulong addr = 0x1000;
	struct ofprop prop;
	struct scene *scn;
	oftree tree;
	ofnode node;
	char *str;
	void *fdt;
	int i;

	ut_assertok(uclass_first_device_err(UCLASS_VIDEO, &dev));
	vid_priv = dev_get_uclass_priv(dev);

	ut_assertok(run_command("cedit load hostfs - cedit.dtb", 0));

	ut_asserteq(ID_SCENE1, cedit_prepare(cur_exp, dev, &scn));

	/* get a menu to fiddle with */
	menu = scene_obj_find(scn, ID_CPU_SPEED, SCENEOBJT_MENU);
	ut_assertnonnull(menu);
	menu->cur_item_id = ID_CPU_SPEED_2;

	/* get a textline to fiddle with too */
	tline = scene_obj_find(scn, ID_MACHINE_NAME, SCENEOBJT_TEXTLINE);
	ut_assertnonnull(tline);
	str = abuf_data(&tline->tin.buf);
	strcpy(str, "my-machine");

	ut_assertok(run_command("cedit write_fdt hostfs - settings.dtb", 0));
	ut_assertok(run_commandf("load hostfs - %lx settings.dtb", addr));
	ut_assert_nextlinen("1024 bytes read");

	fdt = map_sysmem(addr, 1024);
	tree = oftree_from_fdt(fdt);
	node = ofnode_find_subnode(oftree_root(tree), CEDIT_NODE_NAME);
	ut_assert(ofnode_valid(node));

	ut_asserteq(ID_CPU_SPEED_2,
		    ofnode_read_u32_default(node, "cpu-speed", 0));
	ut_asserteq(3,
		    ofnode_read_u32_default(node, "cpu-speed-value", 0));
	ut_asserteq_str("2.5 GHz", ofnode_read_string(node, "cpu-speed-str"));
	ut_asserteq_str("my-machine", ofnode_read_string(node, "machine-name"));

	/* There should only be 7 properties */
	for (i = 0, ofnode_first_property(node, &prop); ofprop_valid(&prop);
	     i++, ofnode_next_property(&prop))
		;
	ut_asserteq(7, i);

	ut_assert_console_end();

	/* reset the expo */
	menu->cur_item_id = ID_CPU_SPEED_1;
	*str = '\0';

	/* load in the settings and make sure they update */
	ut_assertok(run_command("cedit read_fdt hostfs - settings.dtb", 0));
	ut_asserteq(ID_CPU_SPEED_2, menu->cur_item_id);
	ut_asserteq_str("my-machine", ofnode_read_string(node, "machine-name"));

	ut_assertnonnull(menu);
	ut_assert_console_end();

	return 0;
}
BOOTSTD_TEST(cedit_fdt, UTF_CONSOLE);

/* Check the cedit write_env and read_env commands */
static int cedit_env(struct unit_test_state *uts)
{
	struct scene_obj_textline *tline;
	struct video_priv *vid_priv;
	extern struct expo *cur_exp;
	struct scene_obj_menu *menu;
	struct udevice *dev;
	struct scene *scn;
	char *str;

	ut_assertok(run_command("cedit load hostfs - cedit.dtb", 0));

	ut_assertok(uclass_first_device_err(UCLASS_VIDEO, &dev));
	vid_priv = dev_get_uclass_priv(dev);

	ut_asserteq(ID_SCENE1, cedit_prepare(cur_exp, dev, &scn));

	/* get a menu to fiddle with */
	menu = scene_obj_find(scn, ID_CPU_SPEED, SCENEOBJT_MENU);
	ut_assertnonnull(menu);
	menu->cur_item_id = ID_CPU_SPEED_2;

	/* get a textline to fiddle with too */
	tline = scene_obj_find(scn, ID_MACHINE_NAME, SCENEOBJT_TEXTLINE);
	ut_assertnonnull(tline);
	str = abuf_data(&tline->tin.buf);
	strcpy(str, "my-machine");

	ut_assertok(run_command("cedit write_env -v", 0));
	ut_assert_nextlinen("c.cpu-speed=11");
	ut_assert_nextlinen("c.cpu-speed-str=2.5 GHz");
	ut_assert_nextlinen("c.cpu-speed-value=3");
	ut_assert_nextlinen("c.power-loss=14");
	ut_assert_nextlinen("c.power-loss-str=Always Off");
	ut_assert_nextlinen("c.power-loss-value=0");
	ut_assert_nextlinen("c.machine-name=my-machine");
	ut_assert_console_end();

	ut_asserteq(11, env_get_ulong("c.cpu-speed", 10, 0));
	ut_asserteq_str("2.5 GHz", env_get("c.cpu-speed-str"));
	ut_asserteq_str("my-machine", env_get("c.machine-name"));

	/* reset the expo */
	menu->cur_item_id = ID_CPU_SPEED_1;
	*str = '\0';

	ut_assertok(run_command("cedit read_env -v", 0));
	ut_assert_nextlinen("c.cpu-speed=11");
	ut_assert_nextlinen("c.power-loss=14");
	ut_assert_nextlinen("c.machine-name=my-machine");
	ut_assert_console_end();

	ut_asserteq(ID_CPU_SPEED_2, menu->cur_item_id);
	ut_asserteq_str("my-machine", env_get("c.machine-name"));

	return 0;
}
BOOTSTD_TEST(cedit_env, UTF_CONSOLE);

/* Check the cedit write_cmos and read_cmos commands */
static int cedit_cmos(struct unit_test_state *uts)
{
	struct scene_obj_menu *menu, *menu2;
	struct video_priv *vid_priv;
	extern struct expo *cur_exp;
	struct udevice *dev;
	struct scene *scn;

	ut_assertok(run_command("cedit load hostfs - cedit.dtb", 0));

	ut_assertok(uclass_first_device_err(UCLASS_VIDEO, &dev));
	vid_priv = dev_get_uclass_priv(dev);
	ut_asserteq(ID_SCENE1, cedit_prepare(cur_exp, dev, &scn));

	/* get the menus to fiddle with */
	menu = scene_obj_find(scn, ID_CPU_SPEED, SCENEOBJT_MENU);
	ut_assertnonnull(menu);
	menu->cur_item_id = ID_CPU_SPEED_2;

	menu2 = scene_obj_find(scn, ID_POWER_LOSS, SCENEOBJT_MENU);
	ut_assertnonnull(menu2);
	menu2->cur_item_id = ID_AC_MEMORY;

	ut_assertok(run_command("cedit write_cmos -v", 0));
	ut_assert_nextlinen("Write 2 bytes from offset 80 to 84");
	ut_assert_console_end();

	/* reset the expo */
	menu->cur_item_id = ID_CPU_SPEED_1;
	menu2->cur_item_id = ID_AC_OFF;

	ut_assertok(run_command("cedit read_cmos -v", 0));
	ut_assert_nextlinen("Read 2 bytes from offset 80 to 84");
	ut_assert_console_end();

	ut_asserteq(ID_CPU_SPEED_2, menu->cur_item_id);
	ut_asserteq(ID_AC_MEMORY, menu2->cur_item_id);

	/* the same device, this time named on the command line */
	ut_assertok(run_command("cedit write_cmos -v rtc@43", 0));
	ut_assert_nextlinen("Write 2 bytes from offset 80 to 84");
	ut_assertok(run_command("cedit read_cmos -v rtc@43", 0));
	ut_assert_nextlinen("Read 2 bytes from offset 80 to 84");
	ut_assert_console_end();

	/* a device which does not exist */
	ut_asserteq(1, run_command("cedit read_cmos nope", 0));
	ut_assert_nextline("Failed to get RTC device: -19: No such device");
	ut_asserteq(1, run_command("cedit write_cmos nope", 0));
	ut_assert_nextline("Failed to get RTC device: -19: No such device");
	ut_assert_console_end();

	return 0;
}
BOOTSTD_TEST(cedit_cmos, UTF_CONSOLE);

/* Check the cedit displays correctly */
static int cedit_render(struct unit_test_state *uts)
{
	struct scene_obj_menu *menu;
	struct video_priv *vid_priv;
	extern struct expo *cur_exp;
	struct expo_action evt;
	struct expo_action act;
	struct udevice *dev, *con;
	struct expo_theme *theme;
	struct stdio_dev *sdev;
	struct scene *scn;
	struct expo *exp;
	int i;

	ut_assertok(run_command("cedit load hostfs - cedit.dtb", 0));

	exp = cur_exp;
	sdev = stdio_get_by_name("vidconsole");
	ut_assertnonnull(sdev);
	con = sdev->priv;

	theme = &exp->theme;
	theme->menuitem_gap_y = 2;
	theme->menu_inset = 2;
	/* theme->menu_title_margin_x is 0 so menu labels shouldn't line up */
	theme->textline_label_margin_x = 10;

	dev = dev_get_parent(con);
	vid_priv = dev_get_uclass_priv(dev);
	ut_asserteq(ID_SCENE1, cedit_prepare(exp, dev, &scn));

	menu = scene_obj_find(scn, ID_POWER_LOSS, SCENEOBJT_MENU);
	ut_assertnonnull(menu);
	ut_asserteq(ID_AC_OFF, menu->cur_item_id);

	ut_assertok(expo_render(exp));
	ut_asserteq(4926, video_compress_fb(uts, dev, false));
	ut_assertok(video_check_copy_fb(uts, dev));

	/* move to the second menu */
	act.type = EXPOACT_POINT_OBJ;
	act.select.id = ID_POWER_LOSS;
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_assertok(expo_render(exp));
	ut_asserteq(5000, video_compress_fb(uts, dev, false));

	/* open the menu */
	act.type = EXPOACT_OPEN;
	act.select.id = ID_POWER_LOSS;
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_assertok(expo_render(exp));
	ut_asserteq(5390, video_compress_fb(uts, dev, false));

	/* close the menu */
	act.type = EXPOACT_CLOSE;
	act.select.id = ID_POWER_LOSS;
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_assertok(expo_render(exp));
	ut_asserteq(5000, video_compress_fb(uts, dev, false));

	/* open the menu again to check it looks the same */
	act.type = EXPOACT_OPEN;
	act.select.id = ID_POWER_LOSS;
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_assertok(expo_render(exp));
	ut_asserteq(5390, video_compress_fb(uts, dev, false));

	/* close the menu */
	act.type = EXPOACT_CLOSE;
	act.select.id = ID_POWER_LOSS;
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_assertok(expo_render(exp));
	ut_asserteq(5000, video_compress_fb(uts, dev, false));

	act.type = EXPOACT_OPEN;
	act.select.id = ID_POWER_LOSS;
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_assertok(expo_render(exp));
	ut_asserteq(5390, video_compress_fb(uts, dev, false));

	act.type = EXPOACT_POINT_ITEM;
	act.select.id = ID_AC_ON;
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_assertok(expo_render(exp));
	ut_asserteq(5363, video_compress_fb(uts, dev, false));

	/* select it */
	act.type = EXPOACT_SELECT;
	act.select.id = ID_AC_ON;
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_assertok(expo_render(exp));
	ut_asserteq(4964, video_compress_fb(uts, dev, false));

	ut_asserteq(ID_AC_ON, menu->cur_item_id);

	/* move to the line-edit field */
	act.type = EXPOACT_POINT_OBJ;
	act.select.id = ID_MACHINE_NAME;
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_assertok(expo_render(exp));
	ut_asserteq(4868, video_compress_fb(uts, dev, false));

	/* open it */
	act.type = EXPOACT_OPEN;
	act.select.id = ID_MACHINE_NAME;
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_assertok(expo_render(exp));
	ut_asserteq(4871, video_compress_fb(uts, dev, false));

	/*
	 * Send some keypresses. Note that the console must be enabled so that
	 * the characters actually reach the putc_xy() in console_truetype,
	 * since in scene_textline_send_key(), the textline restores the
	 * vidconsole state, outputs the character and then saves the state
	 * again. If the character is never output, then the state won't be
	 * updated and the textline will be inconsistent.
	 *
	 * This is why this test enables UTF_NO_SILENT
	 */
	for (i = 'a'; i < 'd'; i++)
		ut_assertok(scene_send_key(scn, i, &evt));
	ut_assertok(cedit_arange(exp, vid_priv, scn->id));
	ut_assertok(expo_render(exp));
	ut_asserteq(5076, video_compress_fb(uts, dev, false));

	expo_destroy(exp);
	cur_exp = NULL;

	return 0;
}
BOOTSTD_TEST(cedit_render, UTF_DM | UTF_SCAN_FDT | UTF_NO_SILENT);

/* Check the cedit displays textlines correctly */
static int cedit_render_textline(struct unit_test_state *uts)
{
	struct scene_obj_textline *tline;
	struct video_priv *vid_priv;
	extern struct expo *cur_exp;
	struct expo_action evt;
	struct expo_action act;
	struct scene_obj *edit;
	struct udevice *dev, *con;
	struct stdio_dev *sdev;
	struct scene *scn;
	struct expo *exp;
	char *str;
	int i;

	ut_assertok(run_command("cedit load hostfs - cedit.dtb", 0));

	exp = cur_exp;
	sdev = stdio_get_by_name("vidconsole");
	ut_assertnonnull(sdev);
	con = sdev->priv;

	dev = dev_get_parent(con);
	vid_priv = dev_get_uclass_priv(dev);
	ut_asserteq(ID_SCENE1, cedit_prepare(exp, dev, &scn));

	/* set up an initial value for the textline */
	tline = scene_obj_find(scn, ID_MACHINE_NAME, SCENEOBJT_TEXTLINE);
	ut_assertnonnull(tline);
	str = abuf_data(&tline->tin.buf);
	strcpy(str, "my-machine");
	ut_asserteq(20, tline->pos);

	ut_assertok(expo_render(exp));
	ut_asserteq(5344, video_compress_fb(uts, dev, false));
	ut_assertok(video_check_copy_fb(uts, dev));

	edit = scene_obj_find(scn, ID_MACHINE_NAME_EDIT, SCENEOBJT_TEXT);
	ut_assert(edit);

	/* try the password flag */
	edit->flags |= SCENEOF_PASSWORD;
	ut_assertok(expo_render(exp));
	ut_asserteq(5135, video_compress_fb(uts, dev, false));
	ut_assertok(video_check_copy_fb(uts, dev));
	edit->flags &= ~SCENEOF_PASSWORD;

	/* move to the line-edit field */
	act.type = EXPOACT_POINT_OBJ;
	act.select.id = ID_MACHINE_NAME;
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_assertok(expo_render(exp));
	ut_asserteq(5408, video_compress_fb(uts, dev, false));

	/* open it */
	act.type = EXPOACT_OPEN;
	act.select.id = ID_MACHINE_NAME;
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_assertok(expo_render(exp));
	ut_asserteq(5291, video_compress_fb(uts, dev, false));

	/* delete some characters */
	ut_unsilence_console(uts);
	for (i = 0; i < 3; i++)
		ut_assertok(scene_send_key(scn, '\b', &evt));
	ut_silence_console(uts);
	ut_asserteq_str("my-mach", str);

	ut_assertok(cedit_arange(exp, vid_priv, scn->id));
	ut_assertok(expo_render(exp));
	ut_asserteq(5209, video_compress_fb(uts, dev, false));

	expo_destroy(exp);
	cur_exp = NULL;

	return 0;
}
BOOTSTD_TEST(cedit_render_textline, UTF_DM | UTF_SCAN_FDT | UTF_NO_SILENT);

/* Check the cedit is arranged correctly */
static int cedit_position(struct unit_test_state *uts)
{
	struct scene_obj_textline *tline;
	struct scene_obj_menu *menu;
	struct video_priv *vid_priv;
	extern struct expo *cur_exp;
	const uint label_width = 96;
	struct scene_menitem *item;
	struct udevice *dev, *con;
	struct expo_theme *theme;
	struct scene_obj *obj;
	struct stdio_dev *sdev;
	struct scene *scn;
	struct expo *exp;
	uint x_offset;

	ut_assertok(run_command("cedit load hostfs - cedit.dtb", 0));
	exp = cur_exp;

	sdev = stdio_get_by_name("vidconsole");
	ut_assertnonnull(sdev);
	con = sdev->priv;

	theme = &exp->theme;
	theme->menuitem_gap_y = 2;
	theme->menu_inset = 2;
	theme->textline_label_margin_x = 10;
	theme->menu_title_margin_x = 10;

	dev = dev_get_parent(con);
	vid_priv = dev_get_uclass_priv(dev);
	ut_asserteq(ID_SCENE1, cedit_prepare(exp, dev, &scn));

	// expo_dump(exp);
	menu = scene_obj_find(scn, ID_CPU_SPEED, SCENEOBJT_MENU);
	ut_assertnonnull(menu);
	obj = &menu->obj;
	ut_asserteq(50, obj->bbox.x0);
	ut_asserteq(100, obj->bbox.y0);
	ut_asserteq(50 + 160, obj->bbox.x1);
	ut_asserteq(18 + 100, obj->bbox.y1);

	obj = scene_obj_find(scn, menu->title_id, SCENEOBJT_TEXT);
	ut_assertnonnull(obj);
	ut_asserteq(50, obj->bbox.x0);
	ut_asserteq(100, obj->bbox.y0);
	ut_asserteq(50 + 75, obj->bbox.x1);
	ut_asserteq(18 + 100, obj->bbox.y1);

	item = scene_menuitem_find_seq(menu, 0);
	ut_assertnonnull(item);

	x_offset = label_width + theme->textline_label_margin_x;
	obj = scene_obj_find(scn, item->label_id, SCENEOBJT_TEXT);
	ut_assertnonnull(obj);
	ut_asserteq(42, obj->dims.x);
	ut_asserteq(18, obj->dims.y);

	ut_asserteq(0, obj->ofs.xofs);
	ut_asserteq(0, obj->ofs.yofs);

	ut_asserteq(50 + x_offset, obj->bbox.x0);
	ut_asserteq(100, obj->bbox.y0);
	ut_asserteq(50 + x_offset + 54, obj->bbox.x1);
	ut_asserteq(18 + 100, obj->bbox.y1);

	/* all items should have the same, so check them */
	item = scene_menuitem_find_seq(menu, 1);
	ut_assertnonnull(item);
	ut_asserteq(50 + x_offset + 54, obj->bbox.x1);

	item = scene_menuitem_find_seq(menu, 2);
	ut_assertnonnull(item);
	ut_asserteq(50 + x_offset + 54, obj->bbox.x1);

	menu = scene_obj_find(scn, ID_POWER_LOSS, SCENEOBJT_MENU);
	ut_assertnonnull(menu);
	ut_asserteq(ID_AC_OFF, menu->cur_item_id);

	obj = &menu->obj;
	ut_asserteq(50, obj->bbox.x0);
	ut_asserteq(120, obj->bbox.y0);
	ut_asserteq(50 + 160, obj->bbox.x1);
	ut_asserteq(18 + 120, obj->bbox.y1);

	tline = scene_obj_find(scn, ID_MACHINE_NAME, SCENEOBJT_TEXTLINE);
	ut_assertnonnull(menu);

	obj = &tline->obj;
	ut_asserteq(381, obj->dims.x);
	ut_asserteq(18, obj->dims.y);

	ut_asserteq(50, obj->bbox.x0);
	ut_asserteq(140, obj->bbox.y0);
	ut_asserteq(50 + 381, obj->bbox.x1);
	ut_asserteq(140 + 18, obj->bbox.y1);

	obj = scene_obj_find(scn, ID_MACHINE_NAME_EDIT, SCENEOBJT_TEXT);
	ut_assertnonnull(obj);
	ut_asserteq(275, obj->dims.x);
	ut_asserteq(18, obj->dims.y);

	ut_asserteq(50 + x_offset, obj->bbox.x0);
	ut_asserteq(140, obj->bbox.y0);
	ut_asserteq(50 + x_offset + obj->dims.x, obj->bbox.x1);
	ut_asserteq(140 + 18, obj->bbox.y1);

	return 0;
}
BOOTSTD_TEST(cedit_position, UTF_DM | UTF_SCAN_FDT);

/* Check the cedit handles mouse clicks correctly */
static int cedit_mouse(struct unit_test_state *uts)
{
	struct scene_obj_menu *speed, *loss;
	struct scene_obj_textline *mach;
	struct video_priv *vid_priv;
	extern struct expo *cur_exp;
	struct scene_menitem *item;
	struct udevice *dev, *con;
	struct expo_action act;
	struct stdio_dev *sdev;
	struct scene *scn;
	struct expo *exp;

	ut_assertok(run_command("cedit load hostfs - cedit.dtb", 0));

	exp = cur_exp;
	sdev = stdio_get_by_name("vidconsole");
	ut_assertnonnull(sdev);
	con = sdev->priv;

	dev = dev_get_parent(con);
	vid_priv = dev_get_uclass_priv(dev);
	ut_asserteq(ID_SCENE1, cedit_prepare(exp, dev, &scn));

	speed = scene_obj_find(scn, ID_CPU_SPEED, SCENEOBJT_NONE);
	ut_assertnonnull(speed);
	ut_asserteq(ID_CPU_SPEED, scn->highlight_id);

	loss = scene_obj_find(scn, ID_POWER_LOSS, SCENEOBJT_NONE);
	ut_assertnonnull(loss);

	mach = scene_obj_find(scn, ID_MACHINE_NAME, SCENEOBJT_NONE);
	ut_assertnonnull(mach);

	item = scene_menuitem_find_seq(speed, 0);

	/* click on the title of the CPU speed (should do nothing) */
	ut_assertok(click_check(uts, scn, speed->title_id, EXPOACT_NONE, &act));

	/* click on CPU speed to open it */
	ut_assertok(click_check(uts, scn, item->label_id, EXPOACT_OPEN, &act));
	ut_asserteq(ID_CPU_SPEED, act.select.id);
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_asserteq(SCENEOF_OPEN | SCENEOF_SIZE_VALID | SCENEOF_DIRTY,
		    speed->obj.flags);

	/* click outside the label to close the menu */
	ut_assertok(scene_send_click(scn, 10, 10, &act));
	ut_asserteq(EXPOACT_CLOSE, act.type);
	ut_asserteq(ID_CPU_SPEED, act.select.id);
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_asserteq(SCENEOF_SIZE_VALID | SCENEOF_DIRTY, speed->obj.flags);

	/* click on CPU speed to open it again */
	ut_assertok(click_check(uts, scn, item->label_id, EXPOACT_OPEN, &act));
	ut_asserteq(ID_CPU_SPEED, act.select.id);
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_asserteq(SCENEOF_OPEN | SCENEOF_SIZE_VALID | SCENEOF_DIRTY,
		    speed->obj.flags);

	/* click on the second item (1.5 GHz) */
	item = scene_menuitem_find_seq(speed, 1);
	ut_assertnonnull(item);
	ut_asserteq(ID_CPU_SPEED_2, item->id);
	ut_assertok(click_check(uts, scn, item->label_id,
				EXPOACT_POINT_CLOSE, &act));
	ut_asserteq(ID_CPU_SPEED_2, act.select.id);
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));

	/* verify that the second item is now selected and menu is closed */
	ut_asserteq(ID_CPU_SPEED_2, speed->cur_item_id);
	ut_asserteq(SCENEOF_SIZE_VALID | SCENEOF_DIRTY, speed->obj.flags);
	ut_asserteq(ID_CPU_SPEED, scn->highlight_id);

	/* click on the power loss menu to open it */
	item = scene_menuitem_find_seq(loss, 0);
	ut_assertnonnull(item);
	ut_assertok(click_check(uts, scn, item->label_id, EXPOACT_POINT_OPEN,
				&act));
	ut_asserteq(ID_POWER_LOSS, act.select.id);
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_asserteq(SCENEOF_OPEN | SCENEOF_SIZE_VALID | SCENEOF_DIRTY,
		    loss->obj.flags);

	/* click on CPU speed to open it again */
	item = scene_menuitem_find_seq(speed, 0);
	ut_assertok(click_check(uts, scn, item->label_id, EXPOACT_REPOINT_OPEN,
				&act));
	ut_asserteq(ID_CPU_SPEED, act.select.id);
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_asserteq(SCENEOF_OPEN | SCENEOF_SIZE_VALID | SCENEOF_DIRTY,
		    speed->obj.flags);

	/* click on the textline */
	ut_assertok(click_check(uts, scn, mach->tin.edit_id,
				EXPOACT_REPOINT_OPEN, &act));
	ut_asserteq(ID_MACHINE_NAME, act.select.id);
	ut_asserteq(ID_CPU_SPEED, act.select.prev_id);
	ut_assertok(cedit_do_action(exp, scn, vid_priv, &act));
	ut_asserteq(ID_CPU_SPEED_2, speed->cur_item_id);
	ut_asserteq(ID_MACHINE_NAME, scn->highlight_id);
	ut_asserteq(SCENEOF_SIZE_VALID | SCENEOF_DIRTY, loss->obj.flags);
	ut_asserteq(SCENEOF_OPEN | SCENEOF_SIZE_VALID | SCENEOF_DIRTY,
		    mach->obj.flags);

	return 0;
}
BOOTSTD_TEST(cedit_mouse, UTF_DM | UTF_SCAN_FDT);
