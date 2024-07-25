/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Romain Bouqueau, Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2000-2024
 *					All rights reserved
 *
 *  This file is part of GPAC / Media Tools sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>
#include <gpac/mpeg4_odf.h>
#include <gpac/maths.h>
#include <gpac/avparse.h>

#ifndef GPAC_DISABLE_OGG
#include <gpac/internal/ogg.h>
#endif

//uncomment/define globally to remove all bitstream parsing logging from code (this will break inspect mode analyze=bs)
//#define GPAC_DISABLE_AVPARSE_LOGS

#ifndef GPAC_DISABLE_AVPARSE_LOGS
void gf_bs_log_idx(GF_BitStream *bs, u32 nBits, const char *fname, s64 val, s32 idx1, s32 idx2, s32 idx3);

#define gf_bs_log(_bs, _nBits, _fname, _val) gf_bs_log_idx(_bs, _nBits, _fname, _val, -1, -1, -1)

static u32 gf_bs_read_int_log_idx3(GF_BitStream *bs, u32 nBits, const char *fname, s32 idx1, s32 idx2, s32 idx3)
{
	u32 val = gf_bs_read_int(bs, nBits);
	gf_bs_log_idx(bs, nBits, fname, val, idx1, idx2, idx3);
	return val;
}

#define gf_bs_read_int_log(_bs, _nBits, _fname) gf_bs_read_int_log_idx3(_bs, _nBits, _fname, -1, -1, -1)
#define gf_bs_read_int_log_idx(_bs, _nBits, _fname, _idx) gf_bs_read_int_log_idx3(_bs, _nBits, _fname, _idx, -1, -1)
#define gf_bs_read_int_log_idx2(_bs, _nBits, _fname, _idx1, _idx2) gf_bs_read_int_log_idx3(_bs, _nBits, _fname, (s32) _idx1, (s32) _idx2, -1)


#else

#define gf_bs_log(_bs, _nBits, _fname, _val)
#define gf_bs_log_idx(_bs, _nBits, _fname, _val, _idx1, _idx2, _idx3)

#define gf_bs_read_int_log(_bs, _nbb, _f) gf_bs_read_int(_bs, _nbb)
#define gf_bs_read_int_log_idx(_bs, _nbb, _f, _idx) gf_bs_read_int(_bs, _nbb)
#define gf_bs_read_int_log_idx2(_bs, _nbb, _f, _idx1, _idx2) gf_bs_read_int(_bs, _nbb)
#define gf_bs_read_int_log_idx3(_bs, _nbb, _f, _idx1, _idx2, _idx3) gf_bs_read_int(_bs, _nbb)

#endif


GF_EXPORT
u8 gf_mp3_version(u32 hdr)
{
	return ((hdr >> 19) & 0x3);
}

GF_EXPORT
const char *gf_mp3_version_name(u32 hdr)
{
	u32 v = gf_mp3_version(hdr);
	switch (v) {
	case 0:
		return "MPEG-2.5";
	case 1:
		return "Reserved";
	case 2:
		return "MPEG-2";
	case 3:
		return "MPEG-1";
	default:
		return "Unknown";
	}
}

#ifndef GPAC_DISABLE_AV_PARSERS

GF_EXPORT
u8 gf_mp3_layer(u32 hdr)
{
	return 4 - (((hdr >> 17) & 0x3));
}

GF_EXPORT
u8 gf_mp3_num_channels(u32 hdr)
{
	if (((hdr >> 6) & 0x3) == 3) return 1;
	return 2;
}

GF_EXPORT
u16 gf_mp3_sampling_rate(u32 hdr)
{
	u16 res;
	/* extract the necessary fields from the MP3 header */
	u8 version = gf_mp3_version(hdr);
	u8 sampleRateIndex = (hdr >> 10) & 0x3;

	switch (sampleRateIndex) {
	case 0:
		res = 44100;
		break;
	case 1:
		res = 48000;
		break;
	case 2:
		res = 32000;
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[MPEG-1/2 Audio] Samplerate index not valid\n"));
		return 0;
	}
	/*reserved or MPEG-1*/
	if (version & 1) return res;

	/*MPEG-2*/
	res /= 2;
	/*MPEG-2.5*/
	if (version == 0) res /= 2;
	return res;
}

GF_EXPORT
u16 gf_mp3_window_size(u32 hdr)
{
	u8 version = gf_mp3_version(hdr);
	u8 layer = gf_mp3_layer(hdr);

	if (layer == 3) {
		if (version == 3) return 1152;
		return 576;
	}
	if (layer == 2) return 1152;
	return 384;
}

GF_EXPORT
u8 gf_mp3_object_type_indication(u32 hdr)
{
	switch (gf_mp3_version(hdr)) {
	case 3:
		return GF_CODECID_MPEG_AUDIO;
	case 2:
	case 0:
		return GF_CODECID_MPEG2_PART3;
	default:
		return 0x00;
	}
}

/*aligned bitrate parsing with libMAD*/

static
u32 const bitrate_table[5][15] = {
	/* MPEG-1 */
	{	0,  32000,  64000,  96000, 128000, 160000, 192000, 224000,  /* Layer I   */
		256000, 288000, 320000, 352000, 384000, 416000, 448000
	},
	{	0,  32000,  48000,  56000,  64000,  80000,  96000, 112000,  /* Layer II  */
		128000, 160000, 192000, 224000, 256000, 320000, 384000
	},
	{	0,  32000,  40000,  48000,  56000,  64000,  80000,  96000,  /* Layer III */
		112000, 128000, 160000, 192000, 224000, 256000, 320000
	},

	/* MPEG-2 LSF */
	{	0,  32000,  48000,  56000,  64000,  80000,  96000, 112000,  /* Layer I   */
		128000, 144000, 160000, 176000, 192000, 224000, 256000
	},
	{	0,   8000,  16000,  24000,  32000,  40000,  48000,  56000,  /* Layers    */
		64000,  80000,  96000, 112000, 128000, 144000, 160000
	} /* II & III  */
};


u32 gf_mp3_bit_rate(u32 hdr)
{
	u8 version = gf_mp3_version(hdr);
	u8 layer = gf_mp3_layer(hdr);
	u8 bitRateIndex = (hdr >> 12) & 0xF;
	u32 lidx;
	/*MPEG-1*/
	if (version & 1) {
		if (!layer) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[MPEG-1/2 Audio] layer index not valid\n"));
			return 0;
		}
		lidx = layer - 1;
	}
	/*MPEG-2/2.5*/
	else {
		lidx = 3 + (layer >> 1);
	}
	if (lidx>4) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[MPEG-1/2 Audio] layer index not valid\n"));
		return 0;
	}
	if (bitRateIndex>14) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[MPEG-1/2 Audio] Bitrate index not valid\n"));
		return 0;
	}
	return bitrate_table[lidx][bitRateIndex];
}



GF_EXPORT
u16 gf_mp3_frame_size(u32 hdr)
{
	u8 version = gf_mp3_version(hdr);
	u8 layer = gf_mp3_layer(hdr);
	u32 pad = ((hdr >> 9) & 0x1) ? 1 : 0;
	u32 bitrate = gf_mp3_bit_rate(hdr);
	u32 samplerate = gf_mp3_sampling_rate(hdr);

	u32 frameSize = 0;
	if (!samplerate || !bitrate) return 0;

	if (layer == 1) {
		frameSize = ((12 * bitrate / samplerate) + pad) * 4;
	}
	else {
		u32 slots_per_frame = 144;
		if ((layer == 3) && !(version & 1)) slots_per_frame = 72;
		frameSize = (slots_per_frame * bitrate / samplerate) + pad;
	}
	return (u16)frameSize;
}


GF_EXPORT
u32 gf_mp3_get_next_header(FILE* in)
{
	u8 b, state = 0;
	u32 dropped = 0;
	unsigned char bytes[4];
	bytes[0] = bytes[1] = bytes[2] = bytes[3] = 0;

	while (1) {
		if (gf_fread(&b, 1, in) == 0) return 0;

		if (state == 3) {
			bytes[state] = b;
			return GF_4CC((u32)bytes[0], bytes[1], bytes[2], bytes[3]);
		}
		if (state == 2) {
			if (((b & 0xF0) == 0) || ((b & 0xF0) == 0xF0) || ((b & 0x0C) == 0x0C)) {
				if (bytes[1] == 0xFF) state = 1;
				else state = 0;
			}
			else {
				bytes[state] = b;
				state = 3;
			}
		}
		if (state == 1) {
			if (((b & 0xE0) == 0xE0) && ((b & 0x18) != 0x08) && ((b & 0x06) != 0)) {
				bytes[state] = b;
				state = 2;
			}
			else {
				state = 0;
			}
		}

		if (state == 0) {
			if (b == 0xFF) {
				bytes[state] = b;
				state = 1;
			}
			else {
				if ((dropped == 0) && ((b & 0xE0) == 0xE0) && ((b & 0x18) != 0x08) && ((b & 0x06) != 0)) {
					bytes[0] = (u8)0xFF;
					bytes[1] = b;
					state = 2;
				}
				else {
					dropped++;
				}
			}
		}
	}
	return 0;
}

GF_EXPORT
u32 gf_mp3_get_next_header_mem(const u8 *buffer, u32 size, u32 *pos)
{
	u32 cur;
	u8 b, state = 0;
	u32 dropped = 0;
	unsigned char bytes[4];
	bytes[0] = bytes[1] = bytes[2] = bytes[3] = 0;

	cur = 0;
	*pos = 0;
	while (cur < size) {
		b = (u8)buffer[cur];
		cur++;

		if (state == 3) {
			u32 val;
			bytes[state] = b;
			val = GF_4CC((u32)bytes[0], bytes[1], bytes[2], bytes[3]);
			if (gf_mp3_frame_size(val)) {
				*pos = dropped;
				return val;
			}
			state = 0;
			dropped = cur;
		}
		if (state == 2) {
			if (((b & 0xF0) == 0) || ((b & 0xF0) == 0xF0) || ((b & 0x0C) == 0x0C)) {
				if (bytes[1] == 0xFF) {
					state = 1;
					dropped += 1;
				}
				else {
					state = 0;
					dropped = cur;
				}
			}
			else {
				bytes[state] = b;
				state = 3;
			}
		}
		if (state == 1) {
			if (((b & 0xE0) == 0xE0) && ((b & 0x18) != 0x08) && ((b & 0x06) != 0)) {
				bytes[state] = b;
				state = 2;
			}
			else {
				state = 0;
				dropped = cur;
			}
		}

		if (state == 0) {
			if (b == 0xFF) {
				bytes[state] = b;
				state = 1;
			}
			else {
				dropped++;
			}
		}
	}
	return 0;
}

#endif /*GPAC_DISABLE_AV_PARSERS*/
