/*
 * \brief  Genode Audio out/in session + C-API implementation
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \date   2022-05-31
 */

/*
 * Copyright (C) 2022-2026 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/session_label.h>
#include <os/reporter.h>
#include <play_session/connection.h>
#include <record_session/connection.h>
#include <root/component.h>

#include "audio.h"

using namespace Genode;

static char const *not_avail_string = "N/A";

/*************************
 ** Record/Play session **
 *************************/

struct Stereo_output : Noncopyable
{
	static constexpr unsigned CHANNELS = 2u;
	static constexpr unsigned MAX_SAMPLES = 4096u /* ~85ms @ 48kHz */;

	Env &_env;

	Record::Connection _left  { _env, "left"  };
	Record::Connection _right { _env, "right" };

	struct Recording : private Noncopyable
	{
		bool depleted = false;

		/* 16 bit per sample, interleaved left and right */
		union {
			signed char data3[MAX_SAMPLES*CHANNELS*3];
			int16_t     data2[MAX_SAMPLES*CHANNELS];
			int32_t     data4[MAX_SAMPLES*CHANNELS];
		};

		void clear()
		{
			Genode::bzero(data4, sizeof(data4));
		}

		void from_record_sessions(Record::Num_samples num_samples,
		                          unsigned sample_bits,
		                          unsigned sample_phys,
		                          Record::Connection &left, Record::Connection &right)
		{
			using Samples_ptr = Record::Connection::Samples_ptr;

			auto clamped = [&] (float v)
			{
				return (v >  1.0) ?  1.0
				     : (v < -1.0) ? -1.0
				     :  v;
			};

			if (sample_bits == 16) {
				auto float_to_s16 = [&] (float v) { return int16_t(clamped(v)*((1u<<15)-1)); };

				left.record(num_samples,
					[&] (Record::Time_window const tw, Samples_ptr const &samples) {
						depleted = false;

						for (unsigned i = 0; i < num_samples.value(); i++)
							data2[i*CHANNELS] = float_to_s16(samples.start[i]);

						right.record_at(tw, num_samples,
							[&] (Samples_ptr const &samples) {
								for (unsigned i = 0; i < num_samples.value(); i++)
									data2[i*CHANNELS + 1] = float_to_s16(samples.start[i]);
							});
					},
					[&] {
						depleted = true;
						clear();
					}
				);
			} else

			if (sample_bits == 24 && sample_phys == 3) {
				auto float_to_s32 = [&] (float v) { return int32_t(clamped(v)*((1u<<31)-1)); };

				left.record(num_samples,
					[&] (Record::Time_window const tw, Samples_ptr const &samples) {
						depleted = false;

						for (unsigned i = 0; i < num_samples.value(); i++) {
							unsigned const index = i * 3;
							int32_t const left = float_to_s32(samples.start[i]);
							unsigned char const left_0 = (unsigned char)(left >>  8) & 0xff;
							unsigned char const left_1 = (unsigned char)(left >> 16) & 0xff;
							unsigned char const left_2 = (unsigned char)(left >> 24) & 0xff;

							data3[index*CHANNELS+0] = left_0;
							data3[index*CHANNELS+1] = left_1;
							data3[index*CHANNELS+2] = left_2;
						}

						right.record_at(tw, num_samples,
							[&] (Samples_ptr const &samples) {
								for (unsigned i = 0; i < num_samples.value(); i++) {
									unsigned const index = i * 3;
									int32_t const right = float_to_s32(samples.start[i]);
									unsigned char const right_0 = (unsigned char)(right >>  8) & 0xff;
									unsigned char const right_1 = (unsigned char)(right >> 16) & 0xff;
									unsigned char const right_2 = (unsigned char)(right >> 24) & 0xff;

									data3[index*CHANNELS+((CHANNELS >1) * 3)+0] = right_0;
									data3[index*CHANNELS+((CHANNELS >1) * 3)+1] = right_1;
									data3[index*CHANNELS+((CHANNELS >1) * 3)+2] = right_2;
								}
							});
					},
					[&] {
						depleted = true;
						clear();
					}
				);
			} else

			if (sample_bits == 32) {
				auto float_to_s32 = [&] (float v) { return int32_t(clamped(v)*((1u<<31)-1)); };

				left.record(num_samples,
					[&] (Record::Time_window const tw, Samples_ptr const &samples) {
						depleted = false;

						for (unsigned i = 0; i < num_samples.value(); i++)
							data4[i*CHANNELS] = float_to_s32(samples.start[i]);

						right.record_at(tw, num_samples,
							[&] (Samples_ptr const &samples) {
								for (unsigned i = 0; i < num_samples.value(); i++)
									data4[i*CHANNELS + 1] = float_to_s32(samples.start[i]);
							});
					},
					[&] {
						depleted = true;
						clear();
					}
				);
			}
		}
	};

	Recording _recording { };

	genode_audio_packet record_packet(unsigned samples,
	                                  unsigned sample_bits,
	                                  unsigned sample_phys)
	{
		_recording.from_record_sessions({ samples }, sample_bits, sample_phys, _left, _right);
		return _recording.depleted ?
		       genode_audio_packet { nullptr, 0, sample_bits, 2 } :
		       /* use biggest union member */
		       genode_audio_packet { _recording.data4, samples, sample_bits, 2 };
	}

	Stereo_output(Env &env) : _env(env) { }
};


struct Stereo_input : Noncopyable
{
	Env &_env;

	Play::Connection _left  { _env, "mic_left"  };
	Play::Connection _right { _env, "mic_right" };

	struct Frame { float left, right; };

	void _for_each_frame(genode_audio_packet &packet, int channels, auto const &fn) const
	{
		/* convert two channel interleaved to two channel float */
		if (packet.sample_bits == 16) {
			float const scale = 1.0f / (1u << 15);
			for (unsigned i = 0; i < packet.samples; i++) {
				auto left  = float(((short *)packet.data)[i*channels + 0]);
				auto right = float(((short *)packet.data)[i*channels + (channels > 1)]);
				fn(Frame { .left  = scale * left, .right = scale * right });
			}
		}
		if (packet.sample_bits == 24 && packet.sample_phys == 3) {
			char const *data_ptr = (char *)packet.data;
			float const scale = 1.0f / (1u << 31);
			for (unsigned i = 0; i < packet.samples; i++) {
				unsigned const index = i * 3;
				unsigned char left_0  = data_ptr[index * channels + 0];
				unsigned char left_1  = data_ptr[index * channels + 1];
				unsigned char left_2  = data_ptr[index * channels + 2];

				unsigned char right_0 = data_ptr[index * channels + ((channels > 1) * 3) + 0];
				unsigned char right_1 = data_ptr[index * channels + ((channels > 1) * 3) + 1];
				unsigned char right_2 = data_ptr[index * channels + ((channels > 1) * 3) + 2];

				unsigned const uleft  =  (left_0 << 0) |  (left_1 << 8) |  (left_2 << 16);
				unsigned const uright = (right_0 << 0) | (right_1 << 8) | (right_2 << 16);

				int32_t const left  = uleft  << 8;
				int32_t const right = uright << 8;

				fn(Frame { .left  = scale * float(left), .right = scale * float(right) });
			}
		}
		if (packet.sample_bits == 32) {
			float const scale = 1.0f / (1u << 31);
			for (unsigned i = 0; i < packet.samples; i++) {
				auto left  = float(((int32_t*)packet.data)[i*channels + 0]);
				auto right = float(((int32_t*)packet.data)[i*channels + (channels > 1)]);
				fn(Frame { .left = scale * left, .right = scale * right });
			}
		}
	}

	Play::Time_window _time_window { };

	void play_packet(genode_audio_packet &packet, int channels)
	{
		Play::Duration const duration_us { 10'000 };
		_time_window = _left.schedule_and_enqueue(_time_window, duration_us,
			[&] (auto &submit) {
				_for_each_frame(packet, channels, [&] (Frame const frame) {
					submit(frame.left); }); });

		_right.enqueue(_time_window,
			[&] (auto &submit) {
				_for_each_frame(packet, channels, [&] (Frame const frame) {
					submit(frame.right); }); });
	}

	Stereo_input(Env &env) : _env(env) { }
};


struct Audio
{
	Env       &env;
	Allocator &alloc;

	Constructible <Expanding_reporter>  mixer{ };

	Reporter devices { env, "devices" };

	Attached_rom_dataspace config { env, "config" };
	Signal_handler<Audio>  config_handler { env.ep(), *this,
		&Audio::config_update };

	bool mixer_update { false };
	Device_mode speaker_mode = Device_mode::DEFAULT;
	Device_mode microphone_mode = Device_mode::DEFAULT;

	Stereo_output stereo_output { env };
	Stereo_input  stereo_input  { env };

	unsigned _capture_samplerate  { 48000u };
	unsigned _capture_samplebits  { 0u };
	unsigned _capture_channels    { 2u };
	unsigned _playback_samplerate { 48000u };
	unsigned _playback_samplebits { 0u };
	unsigned _playback_channels   { 2u };

	Audio(Env &env, Allocator &alloc)
	: env(env), alloc(alloc)
	{
		config.sigh(config_handler);
		config_update();

	}

	void config_update()
	{
		config.update();

		bool mixer_report = config.node().attribute_value("report_mixer", false);

		if (mixer_report && !mixer.constructed())
			mixer.construct(env, "mixer", "mixer", Expanding_reporter::Initial_buffer_size { 8192 } );
		else if (!mixer_report && mixer.constructed())
			mixer.destruct();

		mixer_update = config.node().has_sub_node("control");

		typedef String<9> Mode;
		Mode speaker    = config.node().attribute_value("speaker_priority", Mode("internal"));
		Mode microphone = config.node().attribute_value("mic_priority"    , Mode("internal"));

		speaker_mode    = (speaker    == "internal") ? INTERNAL : EXTERNAL;
		microphone_mode = (microphone == "internal") ? INTERNAL : EXTERNAL;

		devices.enabled(config.node().attribute_value("report_devices", false));
	}

	struct genode_audio_packet record_packet(unsigned samples,
	                                         unsigned sample_bits,
	                                         unsigned sample_phys)
	{
		return stereo_output.record_packet(samples, sample_bits, sample_phys);
	}

	void play_packet(struct genode_audio_packet &packet)
	{
		stereo_input.play_packet(packet, _capture_channels);
	}

	void samplerate(int capture, unsigned samplerate)
	{
		/* XXX fallback to default */

		if (capture) _capture_samplerate  = samplerate;
		else         _playback_samplerate = samplerate;
	}

	void samplebits(int capture, unsigned samplebits)
	{
		/* XXX fallback to default */

		if (capture) _capture_samplebits  = samplebits;
		else         _playback_samplebits = samplebits;
	}

	unsigned samplerate(int capture) const
	{
		return capture ? _capture_samplerate
		               : _playback_samplerate;
	}

	unsigned samplebits(int capture) const
	{
		return capture ? _capture_samplebits
		               : _playback_samplebits;
	}

	void channels(int capture, unsigned channels)
	{
		/* XXX fallback to default */

		if (capture) _capture_channels  = channels;
		else         _playback_channels = channels;
	}

	unsigned channels(int capture) const
	{
		return capture ? _capture_channels
		               : _playback_channels;
	}
};


Audio &_audio(Audio *audio = nullptr)
{
	static Audio &_audio = *audio;
	return _audio;
}


/**
 * C-interface
 */

extern "C" void genode_audio_init(struct genode_env *env_ptr,
                                  struct genode_allocator *alloc_ptr)
{
	Env *env = static_cast<Env*>(env_ptr);
	Allocator *alloc = static_cast<Allocator*>(alloc_ptr);

	static Audio audio { *env, *alloc };
	_audio(&audio);
}


extern "C" void genode_audio_set_capture_channels(unsigned num_ch) {
	_audio().channels(1, num_ch); }


extern "C" unsigned genode_audio_get_capture_channels() {
	return _audio().channels(1); }

extern "C" void genode_audio_set_capture_samplerate(unsigned rate) {
	_audio().samplerate(1, rate); }


extern "C" unsigned genode_audio_get_capture_samplerate() {
	return _audio().samplerate(1); }


extern "C" void genode_audio_set_capture_samplebits(unsigned rate) {
	_audio().samplebits(1, rate); }


extern "C" unsigned genode_audio_get_capture_samplebits() {
	return _audio().samplebits(1); }


extern "C" unsigned long genode_audio_capture_samples_per_period() {
	return _audio().samplerate(1) / 100; }


extern "C" void genode_audio_set_playback_samplerate(unsigned rate) {
	_audio().samplerate(0, rate); }


extern "C" unsigned genode_audio_get_playback_samplerate() {
	return _audio().samplerate(0); }


extern "C" void genode_audio_set_playback_samplebits(unsigned rate) {
	_audio().samplebits(0, rate); }


extern "C" unsigned genode_audio_get_playback_samplebits() {
	return _audio().samplebits(0); }


extern "C" unsigned long genode_audio_playback_samples_per_period() {
	return _audio().samplerate(0) / 100; }


extern "C" void genode_audio_set_playback_channels(unsigned num_ch) {
	_audio().channels(0, num_ch); }


extern "C" unsigned genode_audio_get_playback_channels() {
	return _audio().channels(0); }


extern "C" struct genode_audio_packet genode_audio_record(unsigned samples,
                                                          unsigned sample_bits,
                                                          unsigned sample_phys) {
	return _audio().record_packet(samples, sample_bits, sample_phys); }


extern "C" void genode_audio_play(genode_audio_packet packet) {
	_audio().play_packet(packet); }


int genode_audio_device_avail(char const *device_name) {
	return !(strcmp(not_avail_string, device_name) == 0); }


/*
 * mixer report
 */
static void report_bool(Generator &g, genode_mixer_control *control)
{
	for (unsigned i = 0; i < control->value_count; i++) {
		g.node("control", [&]() {
			g.attribute("name",    control->name);
			g.attribute("id",      control->id);
			g.attribute("channel", i);
			g.attribute("type",    control->type_label);
			g.attribute("value",   (bool)control->values[i]);
		});
	}
}


static void report_integer(Generator &g, genode_mixer_control *control)
{
	for (unsigned i = 0; i < control->value_count; i++) {
		g.node("control", [&]() {
			g.attribute("name",    control->name);
			g.attribute("id",      control->id);
			g.attribute("channel", i);
			g.attribute("type",    control->type_label);
			g.attribute("value",   control->values[i]);
			g.attribute("min",     control->min);
			g.attribute("max",     control->max);
		});
	}
}


static void report_enum(Generator &g, genode_mixer_control *control)
{
	g.node("control", [&]() {
		g.attribute("name", control->name);
		g.attribute("id",   control->id);
		g.attribute("type", control->type_label);
		g.attribute("selected", control->values[0]);

		for (unsigned i = 0; i < control->enum_count; i++) {
			g.node("value", [&]() {
				g.attribute("name", control->enum_strings[i]);
				g.attribute("id", i);
			});
		}
	});
};


extern "C" bool genode_mixer_update(void)
{
	return _audio().mixer_update;
}

extern "C" enum Device_mode genode_speaker_mode(void)
{
	return _audio().speaker_mode;
}

extern "C" enum Device_mode genode_microphone_mode(void)
{
	return _audio().microphone_mode;
}


typedef String<5> Type;

static Ctrl_type genode_control_type(Type const &type)
{
	if (type == "bool") return CTRL_BOOL;
	if (type == "int")  return CTRL_INTEGER;
	if (type == "enum") return CTRL_ENUMERATED;
	return CTRL_INVALID;
}


struct genode_mixer_control *mixer_control(struct genode_mixer_controls *controls,
                                           unsigned const id, unsigned const max)
{
	for (unsigned i = 0; i < controls->count; i++) {
		if (controls->control[i].id == id) return &controls->control[i];
	}

	if (controls->count >= max) return nullptr;

	genode_mixer_control *control =  &controls->control[controls->count];
	controls->count++;

	control->values[0] = ~0u;
	control->values[1] = ~0u;

	return control;
}


extern "C" void genode_mixer_update_controls(struct genode_mixer_controls *controls, bool force)
{
	unsigned const max = controls->count;
	controls->count = 0;

	if (!_audio().mixer_update && !force) return;

	bool profile_selected = false;

	_audio().config.node().for_each_sub_node("profile", [&] (Node const &node) {

		if (profile_selected)
			return;

		profile_selected = true;

		node.for_each_sub_node("control", [&] (Node const &node) {

			unsigned id = node.attribute_value("id", ~0u);
			if (id == ~0u) return;

			Type const name = node.attribute_value("type", Type());
			Ctrl_type  type = genode_control_type(name);
			if (type == CTRL_INVALID) return;

			unsigned channel = node.attribute_value("channel", ~0u);
			if (type != CTRL_ENUMERATED && channel > 1) return;

			unsigned selected = node.attribute_value("selected", ~0u);
			if (type == CTRL_ENUMERATED && selected == ~0u) return;

			struct genode_mixer_control *control = mixer_control(controls, id, max);
			if (control == nullptr) return;

			control->id   = id;
			control->type = type;

			switch (type) {
			case CTRL_BOOL:
			{
				bool value = node.attribute_value("value", false);
				control->values[channel] = value ? 1 : 0;
				break;
			}
			case CTRL_INTEGER:
			{
				unsigned value = node.attribute_value("value", ~0u);
				control->values[channel] = value;
				break;
			}
			case CTRL_ENUMERATED:
			{
				control->values[0] = selected;
				break;
			}
			case CTRL_INVALID: break;
			}
		});
	});

	if (!profile_selected) {
		warning("no profile specified");
		return;
	}

	_audio().mixer_update = false;
}


extern "C" void genode_mixer_report_controls(struct genode_mixer_controls *controls)
{
	if (!_audio().mixer.constructed()) return;

	Expanding_reporter &mixer = *_audio().mixer;

	mixer.generate([&] (Generator &g) {
		for (unsigned i = 0; i < controls->count; i++) {
			switch (controls->control[i].type) {
			case CTRL_BOOL:       report_bool(g, &controls->control[i]);    break;
			case CTRL_INTEGER:    report_integer(g, &controls->control[i]); break;
			case CTRL_ENUMERATED: report_enum(g, &controls->control[i]);    break;
			default: continue;
			}
		}
	});
}


extern "C" void genode_devices_report(struct genode_devices *devices)
{
	Reporter &reporter = _audio().devices;

	if (reporter.enabled() == false) return;

	Reporter::Result const result =
 		reporter.generate([&](Generator &g) {
			for (unsigned i = 0; i < 64; i++)
				for (unsigned j = 0; j < 2; j++) {
					genode_device *device = &devices->device[i][j];
					if (device->valid == false) continue;
					g.node("device", [&]() {
						g.attribute("number", i);
						g.attribute("direction", device->direction);
						g.attribute("node", device->node);
						g.attribute("name", device->name);
					});
			}
		});

	if (result == Buffer_error::EXCEEDED)
		warning("\"devices\" report exceeds maximum size");
}


extern "C" bool genode_query_routing(struct genode_routing *routing,
                                     short vendor_id, short product_id)
{
	if (!routing)
		return false;

	bool result = false;

	bool profile_selected = false;

	bool const strict_profile_check = vendor_id && product_id;

	_audio().config.node().for_each_sub_node("profile", [&] (Node const &node) {

		if (profile_selected)
			return;

		unsigned const profil_vendor_id  = node.attribute_value("vendor_id", 0u);
		unsigned const profil_product_id = node.attribute_value("product_id", 0u);

		if (strict_profile_check && (!profil_vendor_id || !profil_product_id))
			return;

		if (strict_profile_check && ((short)profil_vendor_id != vendor_id || (short)profil_product_id != product_id))
			return;

		profile_selected = true;

		node.with_sub_node("devices",
			[&] (Node const &rnode) {

				using Name = Genode::String<32>;
				Name const profile_name = node.attribute_value("name", Name());

				log("Use devices information from profile '", profile_name, "' for device ",
					Hex(vendor_id, Hex::PREFIX, Hex::PAD), ":",
					Hex(product_id, Hex::PREFIX, Hex::PAD));

				using Device_name = String<16>;

				Device_name const playback =
					rnode.attribute_value("playback", Device_name(not_avail_string));
				Device_name const mic_headset =
					rnode.attribute_value("mic_headset", Device_name(not_avail_string));
				Device_name const mic_internal =
					rnode.attribute_value("mic_internal", Device_name(not_avail_string));

				unsigned const speaker_external_id =
					rnode.attribute_value("speaker_external_id", 0u);
				unsigned const speaker_internal_id =
					rnode.attribute_value("speaker_internal_id", 0u);
				unsigned const mic_external_id =
					rnode.attribute_value("mic_external_id", 0u);
				unsigned const mic_internal_id =
					rnode.attribute_value("mic_internal_id", 0u);

				memcpy(routing->playback, playback.string(), sizeof(routing->playback));
				memcpy(routing->mic_headset, mic_headset.string(), sizeof(routing->mic_headset));
				memcpy(routing->mic_internal, mic_internal.string(), sizeof(routing->mic_internal));

				routing->speaker_external_index = speaker_external_id;
				routing->speaker_internal_index = speaker_internal_id;
				routing->mic_external_index = mic_external_id;
				routing->mic_internal_index = mic_internal_id;

				result = true;
			},
			[&] { error("no devices specified"); });

		/* FIXME make invalid by default to force explicit configuration */
		routing->capture_samplerate = 48'000u;
		routing->capture_samplebits = 0u;
		routing->capture_channels   = 2u;
		routing->playback_samplerate = 48'000u;
		routing->playback_samplebits = 0u;
		routing->playback_channels   = 2u;

		node.with_optional_sub_node("capture", [&] (Node const &node) {
			unsigned const capture_samplerate =
				node.attribute_value("samplerate", routing->capture_samplerate);
			routing->capture_samplerate = capture_samplerate;
			unsigned const capture_samplebits =
				node.attribute_value("samplebits", routing->capture_samplebits);
			routing->capture_samplebits = capture_samplebits;
			unsigned const capture_channels =
				node.attribute_value("channels", routing->capture_channels);
			routing->capture_channels = capture_channels;
		});

		node.with_optional_sub_node("playback", [&] (Node const &node) {
			unsigned const playback_samplerate =
				node.attribute_value("samplerate", routing->playback_samplerate);
			routing->playback_samplerate = playback_samplerate;
			unsigned const playback_samplebits =
				node.attribute_value("samplebits", routing->playback_samplebits);
			routing->playback_samplebits = playback_samplebits;
			unsigned const playback_channels =
				node.attribute_value("channels", routing->playback_channels);
			routing->playback_channels = playback_channels;
		});
	});

	if (!profile_selected) {
		warning("no matching profile found for device ",
		        Hex(vendor_id, Hex::PREFIX, Hex::PAD), ":",
		        Hex(product_id, Hex::PREFIX, Hex::PAD));
		return false;
	}

	return result;
}
