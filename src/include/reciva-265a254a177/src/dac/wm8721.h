/* sound/arm/wm8721.h
 *
 * Wolfson WM8721 codec driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

struct wm8721;
struct wm8721_cfg;

extern void *wm8721_attach(struct snd_card *card,
					      struct device *dev,
					      struct wm8721_cfg *cfg);

extern void wm8721_detach(void *pw);

extern int wm8721_prepare(void *pw,
			       struct snd_pcm_substream *substream,
			       struct snd_pcm_runtime *runtime);
