/*
 *  Force feedback support for Logitech Flight System G940
 *
 *  Copyright (c) 2009 Gary Stein <LordCnidarian@gmail.com>
 *  Copyright (c) 2019 Chris Boyle
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


#include <linux/input.h>
#include <linux/hid.h>

#include "hid-lg.h"

/* Ensure we remember to swap bytes (there's no sle16) */
typedef __s16 __bitwise lg3_s16;

static inline lg3_s16 lg3ff_cpu_to_sle16(s16 val)
{
	return (__force lg3_s16)cpu_to_le16(val);
}

struct hid_lg3ff_axis {
	lg3_s16	constant_force;  /* can cancel autocenter on relevant side */
	u8	_padding0;  /* extra byte of strength? no apparent effect */
	/* how far towards center does the effect keep pushing:
	 * 0   = no autocenter, up to:
	 * 127 = push immediately on any deflection
	 * <0  = repel center
	 */
	s8	autocenter_strength;
	/* how hard does autocenter push */
	s8	autocenter_force;
	/* damping with force of autocenter_force (see also damper_*) */
	s8	autocenter_damping;
	lg3_s16	spring_deadzone_neg;  /* for offset center, set these equal */
	lg3_s16	spring_deadzone_pos;
	s8	spring_coeff_neg;  /* <0 repels center */
	s8	spring_coeff_pos;
	lg3_s16	spring_saturation;
	u8	_padding1[8];  /* [4-8]: a different way of autocentering? */
	s8	damper_coeff_neg;
	s8	damper_coeff_pos;
	lg3_s16	damper_saturation;
	u8	_padding2[4];  /* seems to do the same as damper*? */
} __packed;

struct hid_lg3ff_report {
	struct hid_lg3ff_axis x;
	struct hid_lg3ff_axis y;
	u8	_padding[3];
} __packed;

#define FF_REPORT_ID 2

static void hig_lg3ff_send(struct input_dev *idev,
			   struct hid_lg3ff_report *raw_rep)
{
	struct hid_device *hid = input_get_drvdata(idev);
	struct hid_report *hid_rep = hid->report_enum[HID_OUTPUT_REPORT]
					 .report_id_hash[FF_REPORT_ID];
	int i;

	/* We can be called while atomic (via hid_lg3ff_play) and must queue;
	 * there's nowhere to enqueue a raw report, so populate a hid_report.
	 */
	for (i = 0; i < sizeof(*raw_rep); i++)
		hid_rep->field[0]->value[i] = ((u8 *)raw_rep)[i];
	hid_hw_request(hid, hid_rep, HID_REQ_SET_REPORT);
}

static int hid_lg3ff_play(struct input_dev *dev, void *data,
			 struct ff_effect *effect)
{
	struct hid_lg3ff_report report = {0};
	s16 x, y;

	switch (effect->type) {
	case FF_CONSTANT:
/*
 * Already clamped in ff_memless
 * 0 is center (different then other logitech)
 */
		x = -effect->u.ramp.start_level << 8;
		y = -effect->u.ramp.end_level << 8;

/*
 * Sign backwards from other Force3d pro
 * which get recast here in two's complement 8 bits
 */
		report.x.constant_force = lg3ff_cpu_to_sle16(x);
		report.y.constant_force = lg3ff_cpu_to_sle16(y);
		hig_lg3ff_send(dev, &report);
		break;
	}
	return 0;
}
static void hid_lg3ff_set_autocenter(struct input_dev *dev, u16 magnitude)
{
	struct hid_lg3ff_report report = {0};

	/* negative means repel from center, so scale to 0-127 */
	s8 mag_scaled = magnitude >> 9;

	report.x.autocenter_strength = 127;
	report.x.autocenter_force = mag_scaled;
	report.y.autocenter_strength = 127;
	report.y.autocenter_force = mag_scaled;
	hig_lg3ff_send(dev, &report);
}


static const signed short ff3_joystick_ac[] = {
	FF_CONSTANT,
	FF_AUTOCENTER,
	-1
};

int lg3ff_init(struct hid_device *hid)
{
	struct hid_input *hidinput = list_entry(hid->inputs.next, struct hid_input, list);
	struct input_dev *dev = hidinput->input;
	const signed short *ff_bits = ff3_joystick_ac;
	int error;
	int i;

	/* Check that the report looks ok */
	BUILD_BUG_ON(sizeof(struct hid_lg3ff_report) != 63);  /* excl. id */
	if (!hid_validate_values(hid, HID_OUTPUT_REPORT, FF_REPORT_ID, 0,
				 sizeof(struct hid_lg3ff_report)))
		return -ENODEV;

	/* Assume single fixed device G940 */
	for (i = 0; ff_bits[i] >= 0; i++)
		set_bit(ff_bits[i], dev->ffbit);

	error = input_ff_create_memless(dev, NULL, hid_lg3ff_play);
	if (error)
		return error;

	if (test_bit(FF_AUTOCENTER, dev->ffbit)) {
		dev->ff->set_autocenter = hid_lg3ff_set_autocenter;
		hid_lg3ff_set_autocenter(dev, 0);
	}

	hid_info(hid, "Force feedback for Logitech Flight System G940 by Gary Stein <LordCnidarian@gmail.com>\n");
	return 0;
}

