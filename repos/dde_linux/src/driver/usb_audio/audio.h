/*
 * \brief  C-API for audio (work in progress)
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \date   2022-05-31
 */

/*
 * Copyright (C) 2022-26 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <genode_c_api/base.h>

#ifdef __cplusplus
extern "C" {
#endif

struct genode_audio_packet
{
	void         *data;
	unsigned long samples;
	unsigned      sample_bits;
	unsigned      sample_phys;
};

enum Device_mode {
	INTERNAL,
	EXTERNAL,
	DEFAULT = EXTERNAL,
};

enum Ctrl_type
{
	CTRL_INVALID, CTRL_BOOL, CTRL_INTEGER, CTRL_ENUMERATED
};


struct genode_mixer_control
{
	enum Ctrl_type type;
	char const *type_label;
	unsigned    id;
	unsigned    value_count;
	unsigned    values[2];
	char const *name;
	unsigned    min, max;
	unsigned    enum_count;
	char      **enum_strings;
};


struct genode_mixer_controls
{
	unsigned count;
	struct genode_mixer_control control[64];
};


struct genode_device
{
	bool  valid;
	char *direction;
	char *node;
	char *name;
};


struct genode_devices
{
	struct genode_device device[64][2];
};


struct genode_routing
{
	char playback[16];
	char mic_headset[16];
	char mic_internal[16];

	unsigned capture_samplerate;
	unsigned capture_samplebits;
	unsigned capture_channels;
	unsigned playback_samplerate;
	unsigned playback_samplebits;
	unsigned playback_channels;

	unsigned speaker_external_index;
	unsigned speaker_internal_index;
	unsigned mic_external_index;
	unsigned mic_internal_index;
};


void genode_audio_init(struct genode_env *env_ptr,
                       struct genode_allocator *alloc_ptr);

struct genode_audio_packet genode_audio_record(unsigned, unsigned, unsigned);
void   genode_audio_play(struct genode_audio_packet packet);

int genode_audio_device_avail(char const*);

unsigned long genode_audio_capture_samples_per_period(void);
void          genode_audio_set_capture_channels(unsigned);
unsigned      genode_audio_get_capture_channels(void);
void          genode_audio_set_capture_samplerate(unsigned);
unsigned      genode_audio_get_capture_samplerate(void);
void          genode_audio_set_capture_samplebits(unsigned);
unsigned      genode_audio_get_capture_samplebits(void);

unsigned long genode_audio_playback_samples_per_period(void);
void          genode_audio_set_playback_samplerate(unsigned);
unsigned      genode_audio_get_playback_samplerate(void);
void          genode_audio_set_playback_samplebits(unsigned);
unsigned      genode_audio_get_playback_samplebits(void);
void          genode_audio_set_playback_channels(unsigned);
unsigned      genode_audio_get_playback_channels(void);

void genode_mixer_report_controls(struct genode_mixer_controls *controls);
void genode_mixer_update_controls(struct genode_mixer_controls *controls, bool force);
bool genode_mixer_update(void);
enum Device_mode genode_speaker_mode(void);
enum Device_mode genode_microphone_mode(void);
bool genode_query_routing(struct genode_routing *routing, short, short);

void genode_devices_report(struct genode_devices *devices);

#ifdef __cplusplus
}
#endif

