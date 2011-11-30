/* Reciva Audio Mute */


#ifndef LINUX_RECIVA_AUDIO_MUTE_H
#define LINUX_RECIVA_AUDIO_MUTE_H


extern void reciva_audio_mute(int on);
extern void reciva_register_mute_function(void (*fn)(int));
extern void reciva_unregister_mute_function(void (*fn)(int));

/* IOCTL Stuff */
#define AUDIO_MUTE_IOCTL_BASE  'A'
#define IOC_AUDIO_MUTE           _IOW(AUDIO_MUTE_IOCTL_BASE, 0, int)

#endif
