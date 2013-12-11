#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>

#include <semaphore.h>

#include "xmidi_parser.h"

static uint32_t get_file_size(FILE* fp)
{
	uint32_t orig = ftell(fp);
	uint32_t size;

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, orig, SEEK_SET);

	return size;
}

void init_SDL()
{
	/* We're going to be requesting certain things from our audio
	   device, so we set them up beforehand */
	int audio_rate = 22050;
	Uint16 audio_format = AUDIO_S16; /* 16-bit stereo */
	int audio_channels = 2;
	int audio_buffers = 4096;
	
	SDL_Init(SDL_INIT_AUDIO);
	
	/* This is where we open up our audio device.  Mix_OpenAudio takes
	   as its parameters the audio format we'd /like/ to have. */
	if(Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers)) {
		printf("Unable to open audio!\n");
		exit(1);
	}
}

sem_t stop_semaphore;
void musicDone()
{
	sem_post(&stop_semaphore);
}

int main(int argc, char* argv[]) {
	FILE* fp,* out_fp;
	uint32_t size;
	int rc;
	uint8_t* data,* out_data;
	size_t bytes_read;
	struct XMIDI_info info;
	SDL_RWops *rw;
	Mix_Music* music;
	
	if (argc < 2) {
		printf("%s <xmi file>\n", argv[0]);
		return EXIT_FAILURE;
	}

	fp = fopen(argv[1], "rb");
	if (!fp) {
		perror("Failed to open file");
		return EXIT_FAILURE;
	}

	size = get_file_size(fp);
	if (!size) {
		printf("Failed to get size of file\n");
		return EXIT_FAILURE;
	}

	data = malloc(size);
	if (!data) {
		perror("Failed to allocate memory");
		fclose(fp);
		return EXIT_FAILURE;
	}

	bytes_read = fread(data, 1, size, fp);
	if (bytes_read != size) {
		perror("Failed to read all data");
		goto err_free;
	}

	size = convert_to_midi(data, size, &out_data);
	if (!size)
		goto err_free;

	sem_init(&stop_semaphore, 0, 0);
	init_SDL();
	rw = SDL_RWFromMem(out_data, size);
	music = Mix_LoadMUS_RW(rw);
	Mix_PlayMusic(music, 0);
	Mix_HookMusicFinished(musicDone);

	sem_wait(&stop_semaphore);

	/* This is the cleaning up part */
	Mix_CloseAudio();
	SDL_Quit();
	free(data);
	free(out_data);
	fclose(fp);
	return EXIT_SUCCESS;

err_free:
	free(data);
	fclose(fp);
	return EXIT_FAILURE;
}

