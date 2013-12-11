#include <inttypes.h>

struct XMIDI_info {
	uint8_t num_tracks;
	uint8_t* tracks[120]; // Maximum 120 tracks
};

uint32_t convert_to_midi(uint8_t* data, uint32_t size, uint8_t** dest);
int read_XMIDI_header(uint8_t* data, uint32_t size, struct XMIDI_info* info);
