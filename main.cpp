#define _USE_MATH_DEFINES
#include <math.h>
#include <fftw3.h>
#include <iostream>
#include <bits/stdc++.h>
#include <algorithm>
#include <vector>

#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#include <sys/socket.h>
#include <linux/un.h>
#include <netinet/in.h>
#include <sys/file.h>

int input_size;
int output_size;

double *input_buffer;
fftw_complex *output_buffer;

fftw_plan plan;

snd_pcm_uframes_t buffer_frames = 44100;
unsigned int rate = 44100;
std::vector<int> buffer;
int buffer_length = 0;
snd_pcm_t *capture_handle;
snd_pcm_hw_params_t *hw_params;
snd_pcm_format_t format = SND_PCM_FORMAT_S32_LE;
int channels = 2;

#define SOCKET_PATH "/tmp/FFsockeT"
#define LOCK_PATH "/tmp/FFsockeT.lock"
static struct sockaddr_un address, client_address;
static int socket_fd;
static int client_fd;

// Thank you Daniel J for the pointers and borrowed code from https://github.com/daniel-j/unicorn-fft/

// from impulse. I have no idea how these numbers were generated
// https://github.com/ianhalpern/Impulse/blob/master/src/Impulse.c#L30
static const long fft_max[] = { 12317168L, 7693595L, 5863615L, 4082974L, 5836037L, 4550263L, 3377914L, 3085778L, 3636534L, 3751823L, 2660548L, 3313252L, 2698853L, 2186441L, 1697466L, 1960070L, 1286950L, 1252382L, 1313726L, 1140443L, 1345589L, 1269153L, 897605L, 900408L, 892528L, 587972L, 662925L, 668177L, 686784L, 656330L, 1580286L, 785491L, 761213L, 730185L, 851753L, 927848L, 891221L, 634291L, 833909L, 646617L, 804409L, 1015627L, 671714L, 813811L, 689614L, 727079L, 853936L, 819333L, 679111L, 730295L, 836287L, 1602396L, 990827L, 773609L, 733606L, 638993L, 604530L, 573002L, 634570L, 1015040L, 679452L, 672091L, 880370L, 1140558L, 1593324L, 686787L, 781368L, 605261L, 1190262L, 525205L, 393080L, 409546L, 436431L, 723744L, 765299L, 393927L, 322105L, 478074L, 458596L, 512763L, 381303L, 671156L, 1177206L, 476813L, 366285L, 436008L, 361763L, 252316L, 204433L, 291331L, 296950L, 329226L, 319209L, 258334L, 388701L, 543025L, 396709L, 296099L, 190213L, 167976L, 138928L, 116720L, 163538L, 331761L, 133932L, 187456L, 530630L, 131474L, 84888L, 82081L, 122379L, 82914L, 75510L, 62669L, 73492L, 68775L, 57121L, 94098L, 68262L, 68307L, 48801L, 46864L, 61480L, 46607L, 45974L, 45819L, 45306L, 45110L, 45175L, 44969L, 44615L, 44440L, 44066L, 43600L, 57117L, 43332L, 59980L, 55319L, 54385L, 81768L, 51165L, 54785L, 73248L, 52494L, 57252L, 61869L, 65900L, 75893L, 65152L, 108009L, 421578L, 152611L, 135307L, 254745L, 132834L, 169101L, 137571L, 141159L, 142151L, 211389L, 267869L, 367730L, 256726L, 185238L, 251197L, 204304L, 284443L, 258223L, 158730L, 228565L, 375950L, 294535L, 288708L, 351054L, 694353L, 477275L, 270576L, 426544L, 362456L, 441219L, 313264L, 300050L, 421051L, 414769L, 244296L, 292822L, 262203L, 418025L, 579471L, 418584L, 419449L, 405345L, 739170L, 488163L, 376361L, 339649L, 313814L, 430849L, 275287L, 382918L, 297214L, 286238L, 367684L, 303578L, 516246L, 654782L, 353370L, 417745L, 392892L, 418934L, 475608L, 284765L, 260639L, 288961L, 301438L, 301305L, 329190L, 252484L, 272364L, 261562L, 208419L, 203045L, 229716L, 191240L, 328251L, 267655L, 322116L, 509542L, 498288L, 341654L, 346341L, 451042L, 452194L, 467716L, 447635L, 644331L, 1231811L, 1181923L, 1043922L, 681166L, 1078456L, 1088757L, 1221378L, 1358397L, 1817252L, 1255182L, 1410357L, 2264454L, 1880361L, 1630934L, 1147988L, 1919954L, 1624734L, 1373554L, 1865118L, 2431931L, 2431931L };

void fft_setup(){
	input_size = buffer_frames;
	output_size = input_size / 2 + 1;

	input_buffer = (double*)(fftw_malloc(input_size * sizeof(double)));
	output_buffer = (fftw_complex*)(fftw_malloc(output_size * sizeof(fftw_complex)));

	plan = fftw_plan_dft_r2c_1d(input_size, input_buffer, output_buffer, FFTW_ESTIMATE);
}

int snd_fail(int err, const char *msg) {
	std::cout << msg << " " << snd_strerror(err) << std::endl;
	return err;
}

int mic_setup(){
	int err;

	if ((err = snd_pcm_open(&capture_handle, "hw:adau7002", SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		return snd_fail(err, "Cannot open audio device.");
	}

	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		return snd_fail(err, "Cannot allocate hardware paramaters.");
	}

	if ((err = snd_pcm_hw_params_any(capture_handle, hw_params)) < 0) {
		return snd_fail(err, "Cannot initialize hardware parameters.");
	}

	if ((err = snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		return snd_fail(err, "Cannot set access type.");
	}

	if ((err = snd_pcm_hw_params_set_format(capture_handle, hw_params, format)) < 0) {
		return snd_fail(err, "Cannot set format.");
	}

	if ((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, channels)) < 0) {
		return snd_fail(err, "Cannot set channel count.");
	}

	if ((err = snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &rate, NULL)) < 0) {
		return snd_fail(err, "Cannot set sample rate.");
	}

	if ((err = snd_pcm_hw_params_set_period_size_near(capture_handle, hw_params, &buffer_frames, NULL)) < 0) {
		return snd_fail(err, "Cannot set period size.");
	}

	if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0) {
		return snd_fail(err, "Cannot set hardware parameters.");
	}

	snd_pcm_hw_params_free(hw_params);

	if ((err = snd_pcm_prepare(capture_handle)) < 0) {
		return snd_fail(err, "Cannot prepare audio interface.");
	}

	/*if ((err = snd_pcm_start(capture_handle)) < 0) {
		return snd_fail(err, "Cannot start audio interface.");
	}*/

	buffer_length = buffer_frames * snd_pcm_format_width(format) / sizeof(char) * channels;
	buffer.resize(buffer_length);

	return 0;
}

void mic_cleanup() {
	snd_pcm_drain(capture_handle);
	snd_pcm_close(capture_handle);
}

void fft_transform(int *data_buffer, int fft_bin_count){
	// Prepare the input buffer, converting uint to double
	for (size_t index = 0; index < buffer.size(); index+=2){
		input_buffer[index / 2] = double(buffer[index]) / INT_MAX;
	}
	fftw_execute(plan);
	unsigned int groupsize = 5;
	for(int n = 0; n < fft_bin_count; n++){
		unsigned int x;
		double y = 0;
		for(int x = 0; x < groupsize; x++){	
			double r = (double) sqrt( pow( output_buffer[ (groupsize*n) + x ][ 0 ], 2 ) 
				 + pow( output_buffer[ (groupsize*n) + x ][ 1 ], 2 ) )
			 	 / (double)fft_max[ (groupsize*n) + x ];
			
			if(r < 0){r = 0;}
			if(r > 1.0){r = 1.0;}
			if(r > y){y = r;}
		}
		data_buffer[n] = (unsigned int)(y * 65535);
	}
}

int socket_setup(){
	int err;
	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (socket_fd < 0){
		std::cout << "socket() failed." << std::endl;
		return -1;
	}

	memset(&address, 0, sizeof(struct sockaddr_un));
	address.sun_family = AF_UNIX;
	snprintf(address.sun_path, UNIX_PATH_MAX, SOCKET_PATH);

	int lock_fd = open(LOCK_PATH, O_RDONLY | O_CREAT, 0600);
	if (lock_fd == -1){
		std::cout << "Unable to open " << LOCK_PATH << "." << std::endl;
		return -1;
	}

	int ret = flock(lock_fd, LOCK_EX | LOCK_NB);
	if (ret != 0){
		std::cout << "Unable to get lock. Another FFsocketT daemon running?" << std::endl;
		return -1;
	}

	unlink(SOCKET_PATH);

	if ((err = bind(socket_fd, (struct sockaddr *)&address, sizeof(struct sockaddr_un))) != 0){
		std::cout << "bind() failed. (" << err << ")" << std::endl;
		return -1;
	}

	if ((err = listen(socket_fd, 5)) != 0) {
		std::cout << "listen() failed. (" << err << ")" << std::endl;
		return -1;
	}

	std::cout << "Waiting for connection..." << std::endl;

	socklen_t client_len = sizeof(client_address);
	client_fd = accept(socket_fd, (struct sockaddr *)&client_address, &client_len);
	std::cout << "Connection accepted!" << std::endl;

	/*if ((err = connect(socket_fd, (struct sockaddr *)&address, sizeof(address))) < 0){
		std::cout << "connect() failed. (" << err << ")" << std::endl;
		return -1;
	}*/

	return 0;
}

void socket_cleanup() {
	std::cout << "Cleaning up socket." << std::endl;
	close(client_fd);
	close(socket_fd);
}

int main(){
	int err;
	int fft_bin_count = 19;
	int packet_size = fft_bin_count + 1;
	int data[packet_size];

	signal(SIGPIPE, SIG_IGN);

	data[0] = fft_bin_count;
	std::cout << "FFsockeT daemon starting" << std::endl;
	socket_setup();
	if ((err = mic_setup()) < 0) {
		return 1;
	}
	fft_setup();
	while(1){
		if ((err = snd_pcm_readi(capture_handle, buffer.data(), buffer_frames)) != buffer_frames){
			std::cout << "Audio read failed." << snd_strerror(err) << std::endl;
			if (err == -EPIPE){ // Handle underrun
				snd_pcm_prepare(capture_handle);
			}
			else {
				return err;
			}
		}
		fft_transform(&data[0], fft_bin_count);
		std::cout << "Writing " << (sizeof(int) * packet_size) << "bytes!" << std::endl;
		if ((err = write(client_fd, (const void *) &data, sizeof(int) * packet_size)) < 0) {
			close(client_fd);
			std::cout << "Error writing to socket. (" << err << ")" << std::endl;
			if (err == -1){
				std::cout << "Waiting for connection..." << std::endl;
				socklen_t client_len = sizeof(client_address);
				client_fd = accept(socket_fd, (struct sockaddr *)&client_address, &client_len); 
				std::cout << "Connection accepted!" << std::endl;
				snd_pcm_drain(capture_handle);
			}
			else {
				std::cout << "Unhandled write error. Bailing out!" << std::endl;
				return err;
			}
		}
		//std::cout << (reinterpret_cast<int32_t*>(buffer))[0] << std::endl;
	}
	std::cout << "Cleaning up!" << std::endl;
	mic_cleanup();
	socket_cleanup();
	return 0;
}
