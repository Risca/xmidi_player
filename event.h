#ifndef EVENT_H
#define EVENT_H
#include <inttypes.h>


/**
 * Provides comprehensive information on the next event in the MIDI stream.
 * An EventInfo struct is instantiated by format-specific implementations
 * of MidiParser::parseNextEvent() each time another event is needed.
 *
 * Code adapted from the ScummVM project
 */
struct EventInfo {
	uint8_t * start; ///< Position in the MIDI stream where the event starts.
	              ///< For delta-based MIDI streams (e.g. SMF and XMIDI), this points to the delta.
	uint32_t delta; ///< The number of ticks after the previous event that this event should occur.
	uint8_t event; ///< Upper 4 bits are the command code, lower 4 bits are the MIDI channel.
	              ///< For META, event == 0xFF. For SysEx, event == 0xF0.
	union {
		struct {
			uint8_t param1; ///< The first parameter in a simple MIDI message.
			uint8_t param2; ///< The second parameter in a simple MIDI message.
		} basic;
		struct {
			uint8_t   type; ///< For META events, this indicates the META type.
			uint8_t * data; ///< For META and SysEx events, this points to the start of the data.
		} ext;
	};
	uint32_t length; ///< For META and SysEx blocks, this indicates the length of the data.
	               ///< For Note On events, a non-zero value indicates that no Note Off event
	               ///< will occur, and the MidiParser will have to generate one itself.
	               ///< For all other events, this value should always be zero.
};

int read_event_info(uint8_t* data, struct EventInfo* info, uint32_t current_time);

/* Returns an EventInfo struct if there is a cached event that should be
 * played between current_time and current_time + delta. The cached event
 * is removed from the internal list of cached events! */
struct EventInfo* pop_cached_event(uint32_t current_time, uint32_t delta);
#endif
