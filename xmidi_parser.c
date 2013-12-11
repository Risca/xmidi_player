#include "xmidi_parser.h"
#include "event.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define warning(...) fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");
#define ARRAYSIZE(x) ((int)(sizeof(x) / sizeof(x[0])))

static uint16_t read2low(uint8_t** data)
{
	uint8_t* d = *data;
	uint16_t value = (d[1] << 8) | d[0];
	*data = (d + 2);
	return value;
}

static uint32_t read4high(uint8_t** data)
{
	uint8_t* d = *data;
	uint16_t value = (d[0] << 24) | (d[1] << 16) | (d[2] << 8) | (d[3]);
	*data = (d + 4);
	return value;
}

static void write4high(uint8_t** data, uint32_t val)
{
	uint8_t* d = *data;
	*d++ = (val >> 24) & 0xff;
	*d++ = (val >> 16) & 0xff;
	*d++ = (val >> 8) & 0xff;
	*d++ = val & 0xff;
	*data = d;
}

static void write2high(uint8_t** data, uint16_t val)
{
	uint8_t* d = *data;
	*d++ = (val >> 8) & 0xff;
	*d++ = val & 0xff;
	*data = d;
}

//
// PutVLQ
//
// Write a Conventional Variable Length Quantity
// 
// Code adapted from the Exult engine
//
static int putVLQ(uint8_t* dest, uint32_t value)
{
	int buffer;
	int j, i = 1;

	buffer = value & 0x7F;
	while (value >>= 7)
	{
		buffer <<= 8;
		buffer |= ((value & 0x7F) | 0x80);
		i++;
	}
	if (!dest) return i;
	for (j = 0; j < i; j++)
	{
		*dest++ = buffer & 0xFF;
		buffer >>= 8;
	}
	return i;
}

static int put_event(uint8_t* dest, struct EventInfo* info)
{
	int i = 0,j;
	int rc = 0;
	static uint8_t last_event = 0;

	rc = putVLQ (dest, info->delta);
	if (dest) dest += rc;
	i += rc;

	if ((info->event != last_event) || (info->event >= 0xF0))
	{
		if (dest) *dest++ = (info->event);
		i++;
	}
	
	last_event = info->event;
	
	switch (info->event >> 4)
	{
		// 2 bytes data
		// Note off, Note on, Aftertouch, Controller and Pitch Wheel
		case 0x8: case 0x9: case 0xA: case 0xB: case 0xE:
		if (dest)
		{
			*dest++ = (info->basic.param1);
			*dest++ = (info->basic.param2);
		}
		i += 2;
		break;
		

		// 1 bytes data
		// Program Change and Channel Pressure
		case 0xC: case 0xD:
		if (dest) *dest++ = (info->basic.param1);
		i++;
		break;
		

		// Variable length
		// SysEx
		case 0xF:
		if (info->event == 0xFF)
		{
			if (dest) *dest++ = (info->basic.param1);
			i++;
		}

		rc = putVLQ (dest, info->length);
		if (dest) dest += rc;
		i += rc;
		
		if (info->length)
		{
			for (j = 0; j < info->length; j++)
			{
				if (dest) *dest++ = (info->ext.data[j]); 
				i++;
			}
		}

		break;
		

		// Never occur
		default:
		warning("Not supposed to see this");
		break;
	}

	return i;
}

static int convert_to_mtrk(uint8_t* data, uint32_t size, uint8_t* dest)
{
#if 1
	int time = 0;
	int lasttime = 0;
	int rc;
//	XMidiEvent	*event;
	uint32_t 	i = 8;
	uint32_t 	j;
	uint8_t*	size_pos = NULL;
	uint8_t*	data_end = data + size;
	struct XMIDI_info xmidi_info;
	struct EventInfo info;
	struct EventInfo* cached_info;

	if (dest)
	{
		*dest++ =('M');
		*dest++ =('T');
		*dest++ =('r');
		*dest++ =('k');

		size_pos = dest;
		dest += 4;
	}

	rc = read_XMIDI_header(data, size, &xmidi_info);
	if (!rc) {
		warning("Failed to read XMIDI header");
		return 0;
	}

	data = xmidi_info.tracks[0];

	while (data < data_end)
	{
		printf("=======================================================================\n");
		// We don't write the end of stream marker here, we'll do it later
		if (data[0] == 0xFF && data[1] == 0x2f) {
			printf("Got EOX\n");
//			lasttime = event->time;
			continue;
		}

		rc = read_event_info(data, &info, time);
		if (!rc) {
			warning("Failed to read event info %ld bytes from the end!", data_end - data);
			return 0;
		}
		data += rc;

#if 1
		cached_info = pop_cached_event(time, info.delta);
		while (cached_info) {
			printf("Injecting event %2X at time %2X\n", cached_info->event, time);
			rc = put_event(dest, cached_info);
			if (!rc) {
				warning("Failed to save injected event!");
				return 0;
			}
			if (dest) dest += rc;
			i += rc;
			time += cached_info->delta;
			info.delta -= cached_info->delta;
			free(cached_info);
			cached_info = pop_cached_event(time, info.delta);
		}
#endif

		printf("Saving event %02X\n", info.event);
		rc = put_event(dest, &info);
		if (!rc) {
			warning("Failed to save event!");
			return 0;
		}
		if (dest) dest += rc;
		i += rc;
		time += info.delta;
		if (info.event == 0xFF && info.ext.type == 0x2F) {
			printf("GOT EOX\n");
			data = data_end;
		}
	}

	// Write out end of stream marker
	if (lasttime > time) {
		rc = putVLQ (dest, lasttime-time);
		if (dest) dest += rc;
		i += rc;
	}
	else {
		rc = putVLQ (dest, 0);
		if (dest) dest += rc;
		i += rc;
	}
	if (dest) {
		*dest++ = (0xFF);
		*dest++ = (0x2F);
	}
	rc = putVLQ (dest, 0);
	i += 2+rc;

	if (dest)
	{
		dest += rc;
//		int cur_pos = dest->getPos();
//		dest->seek (size_pos);
//		dest->write4high (i-8);
		write4high(&size_pos, i-8);
//		dest->seek (cur_pos);
	}
	return i;
#else
	return 0;
#endif
}

uint32_t convert_to_midi(uint8_t* data, uint32_t size, uint8_t** dest)
{
	int len;
	uint8_t* d,* start;

	if (!dest)
		return 0;

	/* Do a dry run first so we know how much memory to allocate */
	len = convert_to_mtrk (data, size, NULL);
	if (!len) {
		warning("Failed dummy conversion!");
		return 0;
	}

	printf("Allocating %d bytes of memory\n", len);
	d = malloc(len + 14);
	if (!d) {
		perror("Could not allocate memory");
		return 0;
	}
	start = d;

	*d++ = ('M');
	*d++ = ('T');
	*d++ = ('h');
	*d++ = ('d');
	
	write4high (&d, 6);

	write2high (&d, 0);
	write2high (&d, 1);
	write2high (&d, 60);	// The PPQN

	len = convert_to_mtrk(data, size, d);
	if (!len) {
		warning("Failed to convert");
		free(d);
		return 0;
	}

	*dest = start;

	return len + 14;
}

/* Code adapted from the ScummVM project, which originally adapted it from the
 * Exult engine */
int read_XMIDI_header(uint8_t* data, uint32_t size, struct XMIDI_info* info)
{
	uint32_t i = 0;
	uint8_t *start;
	uint32_t len;
	uint32_t chunkLen;
	char buf[32];

//	_loopCount = -1;

	uint8_t *pos = data;

	if (!memcmp(pos, "FORM", 4)) {
		pos += 4;

		// Read length of
		len = read4high(&pos);
		start = pos;

		// XDIRless XMIDI, we can handle them here.
		if (!memcmp(pos, "XMID", 4)) {
			warning("XMIDI doesn't have XDIR");
			pos += 4;
			info->num_tracks = 1;
		} else if (memcmp(pos, "XDIR", 4)) {
			// Not an XMIDI that we recognize
			warning("Expected 'XDIR' but found '%c%c%c%c'", pos[0], pos[1], pos[2], pos[3]);
			return 0;
		} else {
			// Seems Valid
			pos += 4;
			info->num_tracks = 0;

			for (i = 4; i < len; i++) {
				// Read 4 bytes of type
				memcpy(buf, pos, 4);
				pos += 4;

				// Read length of chunk
				chunkLen = read4high(&pos);

				// Add eight bytes
				i += 8;

				if (memcmp(buf, "INFO", 4) == 0) {
					// Must be at least 2 bytes long
					if (chunkLen < 2) {
						warning("Invalid chunk length %d for 'INFO' block", (int)chunkLen);
						return 0;
					}

					info->num_tracks = (uint8_t)read2low(&pos);
					pos += 2;

					if (chunkLen > 2) {
						warning("Chunk length %d is greater than 2", (int)chunkLen);
						//pos += chunkLen - 2;
					}
					break;
				}

				// Must align
				pos += (chunkLen + 1) & ~1;
				i += (chunkLen + 1) & ~1;
			}

			// Didn't get to fill the header
			if (info->num_tracks == 0) {
				warning("Didn't find a valid track count");
				return 0;
			}

			// Ok now to start part 2
			// Goto the right place
			pos = start + ((len + 1) & ~1);

			if (memcmp(pos, "CAT ", 4)) {
				// Not an XMID
				warning("Expected 'CAT ' but found '%c%c%c%c'", pos[0], pos[1], pos[2], pos[3]);
				return 0;
			}
			pos += 4;

			// Now read length of this track
			len = read4high(&pos);

			if (memcmp(pos, "XMID", 4)) {
				// Not an XMID
				warning("Expected 'XMID' but found '%c%c%c%c'", pos[0], pos[1], pos[2], pos[3]);
				return 0;
			}
			pos += 4;

		}

		// Ok it's an XMIDI.
		// We're going to identify and store the location for each track.
		if (info->num_tracks > ARRAYSIZE(info->tracks)) {
			warning("Can only handle %d tracks but was handed %d", (int)ARRAYSIZE(info->tracks), (int)info->num_tracks);
			return 0;
		}

		int tracksRead = 0;
		while (tracksRead < info->num_tracks) {
			if (!memcmp(pos, "FORM", 4)) {
				// Skip this plus the 4 bytes after it.
				pos += 8;
			} else if (!memcmp(pos, "XMID", 4)) {
				// Skip this.
				pos += 4;
			} else if (!memcmp(pos, "TIMB", 4)) {
				// Custom timbres?
				// We don't support them.
				// Read the length, skip it, and hope there was nothing there.
				pos += 4;
				len = read4high(&pos);
				pos += (len + 1) & ~1;
			} else if (!memcmp(pos, "EVNT", 4)) {
				// Ahh! What we're looking for at last.
				info->tracks[tracksRead] = pos + 8; // Skip the EVNT and length bytes
				pos += 4;
				len = read4high(&pos);
				pos += (len + 1) & ~1;
				++tracksRead;
			} else {
				warning("Hit invalid block '%c%c%c%c' while scanning for track locations", pos[0], pos[1], pos[2], pos[3]);
				return 0;
			}
		}

		// If we got this far, we successfully established
		// the locations for each of our tracks.
		// Note that we assume the original data passed in
		// will persist beyond this call, i.e. we do NOT
		// copy the data to our own buffer. Take warning....
//		_ppqn = 60;
//		resetTracking();
//		setTempo(500000);
//		setTrack(0);
		return ~0;
	}

	return 0;
}
