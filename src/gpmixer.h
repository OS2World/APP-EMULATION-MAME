#define AUDIO_OK			0x00
#define AUDIO_OK_BUFFER_RESIZED		0x01
#define AUDIO_ERR_ALREADY_INIT		0x80
#define AUDIO_ERR_BAD_INIT_PARAMS	0x81
#define AUDIO_ERR_DEVICE_ERR		0x82
#define AUDIO_ERR_NOT_INITIALIZED	0x83
#define AUDIO_ERR_BAD_CHANNEL		0x84
#define AUDIO_ERR_NO_SUCH_DEVICE	0x85
#define AUDIO_ERR_SAMPLE_NOT_FOUND	0x86
#define AUDIO_ERR_BAD_PARAMS		0x87

#define MMPM_ERR_OPEN_PLAYLIST		0x01
#define MMPM_ERR_SETUP_MMPM		0x02
#define MMPM_ERR_PLAY_MMPM		0x03
#define MMPM_ERR_CLOSE_MMPM		0x04

#define DART_ERR_CREATE_QUEUE		0x01
#define DART_ERR_OPEN_QUEUE		0x02
#define DART_ERR_OPEN_MIXER		0x03
#define DART_ERR_SETUP_MIXER		0x04
#define DART_ERR_ALLOC_BUFFER		0x05
#define DART_ERR_DEALLOC_BUFFER		0x06
#define DART_ERR_CLOSE_MIXER		0x07


struct DEVICE_DRIVER {
	char *name;
	char *description;
	unsigned long suggested_bufsize;
	unsigned long min_bufsize, max_bufsize;
	unsigned long suggested_freq;
	unsigned long min_freq, max_freq;
	char supports_16bit;

	unsigned long last_device_error, extended_status;

	char (*init_function)(unsigned long frequency, unsigned long *bufferlen, 
	    char bitspersample);
	char (*shutdown_function)(void);
};

int AudioGetNumberOfDevices( void );

char AudioGetDeviceDriver( unsigned char driver_number, 
    struct DEVICE_DRIVER *driver);

char AudioGetCurrentDriver( struct DEVICE_DRIVER *driver );

char AudioInitialize( unsigned char driver_number,
    unsigned long frequency, unsigned long bufferlen, char bitspersample, 
    char max_channels );

char AudioClose( void );

char PlaySample( void *data, char channel, 
    unsigned long sample_length, char sample_bit_depth, 
    unsigned long frequency, char volume, char loop, HEV trigger_sem );

char PlaySampleSync( void *data, char channel, 
    unsigned long sample_length, char sample_bit_depth, 
    unsigned long frequency, char volume, char loop, HEV trigger_sem,
    void **sync_handle );

char PlaySampleSyncCleanup( void **sync_handle );

unsigned long AudioGetBufferSize( void );

char AudioSetSampleFrequency( char channel, 
    unsigned long frequency );

char AudioSetSampleVolume( char channel, 
    unsigned char volume );

char StopChannelNow( char channel );

char StopChannelAfterLoop( char channel );

