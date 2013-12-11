#include "event.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define warning(...) fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");
#define ARRAYSIZE(x) ((int)(sizeof(x) / sizeof(x[0])))

// This is a special XMIDI variable length quantity
//
// Adapted from the ScummVM project
static uint32_t readVLQ2(uint8_t** data)
{
	uint8_t* pos = *data;
	uint32_t value = 0;
	while (!(pos[0] & 0x80)) {
		value += *pos++;
	}
	*data = pos;
	return value;
}

// This is the conventional (i.e. SMF) variable length quantity
//
// Adapted from the ScummVM project
static uint32_t readVLQ(uint8_t** data) {
	uint8_t* d = *data;
	uint8_t str;
	uint32_t value = 0;
	int i;

	for (i = 0; i < 4; ++i) {
		str = *d++;
		value = (value << 7) | (str & 0x7F);
		if (!(str & 0x80))
			break;
	}
	*data = d;
	return value;
}

struct CachedEvent {
	struct EventInfo* eventInfo;
	uint32_t time;
	struct CachedEvent* next;
};
struct CachedEvent* cached_events = NULL;

static void save_event(struct EventInfo* info, uint32_t current_time)
{
	uint32_t delta = info->length;
	struct CachedEvent *prev, *next, *temp;

	temp = malloc(sizeof(struct CachedEvent));
	temp->eventInfo = info;
	temp->time = current_time + delta;
	temp->next = NULL;

	printf("Saving event to be stopped at %2X\n", temp->time);

	if (!cached_events) {
		cached_events = temp;
	}
	else {
		prev = NULL;
		next = cached_events;

		/* Find the proper time slot */
		while (next && next->time < current_time + delta) {
			prev = next;
			next = next->next;
		}

		if (!next) {
			prev->next = temp;
		}
		else {
			if (prev) {
				temp->next = prev->next;
				prev->next = temp;
			}
			else {
				temp->next = cached_events;
				cached_events = temp;
			}
		}
	}
}

struct EventInfo* pop_cached_event(uint32_t current_time, uint32_t delta)
{
	struct EventInfo* info = NULL;
	struct CachedEvent* old;

	if (cached_events && cached_events->time < current_time + delta) {
		info = cached_events->eventInfo;
		info->delta = cached_events->time - current_time;
		old = cached_events;
		cached_events = cached_events->next;
		free(old);
	}
	
	return info;
}

int read_event_info(uint8_t* data, struct EventInfo* info, uint32_t current_time)
{
	struct EventInfo* injectedEvent;
	info->start = data;
	info->delta = readVLQ2(&data);
	info->event = *data++;

	/* Advance current time here, but not yet in the main conversion loop.
	 * This is so that cached events can still be injected correctly */
	current_time += info->delta;
	
	printf("%02X: Parsing event %02X\n", current_time, info->event);
	switch (info->event >> 4) {
	case 0x9: // Note On
		info->basic.param1 = *(data++);
		info->basic.param2 = *(data++);
		info->length = readVLQ(&data);
		if (info->basic.param2 == 0) {
			info->event = (info->event & 0x0F) | 0x80;
			info->length = 0;
		}
		else {
			printf("Found Note On with duration %X. Saving a Note Off for later\n", info->length);
			injectedEvent = malloc(sizeof(struct EventInfo));
			injectedEvent->event = 0x80 | info->event & 0x0f;
			injectedEvent->basic.param1 = info->basic.param1;
			injectedEvent->basic.param2 = info->basic.param2;
			injectedEvent->length = info->length;
			save_event(injectedEvent, current_time);
		}
		break;

	case 0xC:
	case 0xD:
		info->basic.param1 = *(data++);
		info->basic.param2 = 0;
		break;

	case 0x8:
	case 0xA:
	case 0xE:
		info->basic.param1 = *(data++);
		info->basic.param2 = *(data++);
		break;

	case 0xB:
		info->basic.param1 = *(data++);
		info->basic.param2 = *(data++);

		// This isn't a full XMIDI implementation, but it should
		// hopefully be "good enough" for most things.

		switch (info->basic.param1) {
		// Simplified XMIDI looping.
		case 0x74: {	// XMIDI_CONTROLLER_FOR_LOOP
#if 0 // TODO
				uint8_t *pos = data;
				if (_loopCount < ARRAYSIZE(_loop) - 1)
					_loopCount++;
				else
					warning("XMIDI: Exceeding maximum loop count %d", ARRAYSIZE(_loop));

				_loop[_loopCount].pos = pos;
				_loop[_loopCount].repeat = info->basic.param2;
#endif
				break;
			}

		case 0x75:	// XMIDI_CONTORLLER_NEXT_BREAK
#if 0 // TODO
			if (_loopCount >= 0) {
				if (info->basic.param2 < 64) {
					// End the current loop.
					_loopCount--;
				} else {
					// Repeat 0 means "loop forever".
					if (_loop[_loopCount].repeat) {
						if (--_loop[_loopCount].repeat == 0)
							_loopCount--;
						else
							data = _loop[_loopCount].pos;
					} else {
						data = _loop[_loopCount].pos;
					}
				}
			}
#endif
			break;

		case 0x77:	// XMIDI_CONTROLLER_CALLBACK_TRIG
#if 0 // TODO
			if (_callbackProc)
				_callbackProc(info->basic.param2, _callbackData);
#endif
			break;

		case 0x6e:	// XMIDI_CONTROLLER_CHAN_LOCK
		case 0x6f:	// XMIDI_CONTROLLER_CHAN_LOCK_PROT
		case 0x70:	// XMIDI_CONTROLLER_VOICE_PROT
		case 0x71:	// XMIDI_CONTROLLER_TIMBRE_PROT
		case 0x72:	// XMIDI_CONTROLLER_BANK_CHANGE
		case 0x73:	// XMIDI_CONTROLLER_IND_CTRL_PREFIX
		case 0x76:	// XMIDI_CONTROLLER_CLEAR_BB_COUNT
		case 0x78:	// XMIDI_CONTROLLER_SEQ_BRANCH_INDEX
		default:
			if (info->basic.param1 >= 0x6e && info->basic.param1 <= 0x78) {
				warning("Unsupported XMIDI controller %d (0x%2x)",
					info->basic.param1, info->basic.param1);
			}
		}

		// Should we really keep passing the XMIDI controller events to
		// the MIDI driver, or should we turn them into some kind of
		// NOP events? (Dummy meta events, perhaps?) Ah well, it has
		// worked so far, so it shouldn't cause any damage...

		break;

	case 0xF: // Meta or SysEx event
		switch (info->event & 0x0F) {
		case 0x2: // Song Position Pointer
			info->basic.param1 = *(data++);
			info->basic.param2 = *(data++);
			break;

		case 0x3: // Song Select
			info->basic.param1 = *(data++);
			info->basic.param2 = 0;
			break;

		case 0x6:
		case 0x8:
		case 0xA:
		case 0xB:
		case 0xC:
		case 0xE:
			info->basic.param1 = info->basic.param2 = 0;
			break;

		case 0x0: // SysEx
			info->length = readVLQ(&data);
			info->ext.data = data;
			data += info->length;
			break;

		case 0xF: // META event
			info->ext.type = *(data++);
			info->length = readVLQ(&data);
			info->ext.data = data;
			data += info->length;
			if (info->ext.type == 0x51 && info->length == 3) {
				// Tempo event. We want to make these constant 500,000.
				info->ext.data[0] = 0x07;
				info->ext.data[1] = 0xA1;
				info->ext.data[2] = 0x20;
			}
			break;

		default:
			warning("MidiParser_XMIDI::parseNextEvent: Unsupported event code %x (delta: %X)", info->event, info->delta);
			return 0;
		}
	}

	return (data - info->start);
}

