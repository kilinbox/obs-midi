/*
obs-midi
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/
#include <functional>
#include <string>
#include <utility>
#include <QtCore/QTime>
#include "utils.h"
#include "midi-agent.h"
#include "obs-midi.h"
#include "events.h"
#include "config.h"
#include "device-manager.h"
#include "macro-helpers.h"
using namespace std;
////////////////
// MIDI AGENT //
////////////////

/// <summary>
/// Creates a new Midi Agent from input and output ports
/// </summary>
/// <param name="in_port">Input Port number of MIDI Device</param>
/// <param name="out_port">Output Port number of MIDI Device</param>
MidiAgent::MidiAgent(const int &in_port, std::optional<int> out_port)
{
	set_input_port(in_port);
	if (out_port)
		set_output_port(out_port.value());
	this->setParent(GetDeviceManager().get());
	set_callbacks();
}
/// <summary>
/// Creates a Midi Agent from saved devices
/// </summary>
/// <param name="midiData"></param>
MidiAgent::MidiAgent(const char *midiData)
{
	// Sets the parent of this instance of MidiAgent to Device Manager
	this->setParent(GetDeviceManager().get());
	// Sets the Midi Callback function
	this->Load(midiData);
	if (is_device_attached(midiData)) {
		set_callbacks();
		if (enabled)
			open_midi_input_port();
		if (bidirectional)
			open_midi_output_port();
	}
}
/// <summary>
///  Sets the callbacks for
/// * Events
/// * MIDI messages
/// * MIDI device errors
/// </summary>
void MidiAgent::set_callbacks()
{
	connect(GetEventsSystem().get(), &Events::obsEvent, this, &MidiAgent::handle_obs_event);
	midiin.set_callback([this](const auto &message) { HandleInput(message, this); });
	midiin.set_error_callback([this](const auto &error_type, const auto &error_message) { HandleError(error_type, error_message, this); });
	midiout.set_error_callback([this](const auto &error_type, const auto &error_message) { HandleError(error_type, error_message, this); });
}
/// <summary>
/// MidiAgent Deconstructor
/// </summary>
MidiAgent::~MidiAgent()
{
	this->disconnect();
	clear_MidiHooks();
	midiin.cancel_callback();
}
/// <summary>
/// Checks wether a device is attached and in the device list;
/// </summary>
/// <param name="incoming_data"></param>
/// <returns></returns>
bool MidiAgent::is_device_attached(const char *incoming_data)
{
	obs_data_t *data = obs_data_create_from_json(incoming_data);
	const int minput_port = DeviceManager().get_input_port_number(obs_data_get_string(data, "name"));
	obs_data_release(data);
	return (minput_port != -1);
}
/// <summary>
/// Loads information from OBS data. (recalled from Config)
/// This will not enable the MidiAgent or open the port. (and shouldn't)
/// </summary>
/// <param name="incoming_data"></param>
void MidiAgent::Load(const char *incoming_data)
{
	obs_data_t *data = obs_data_create_from_json(incoming_data);
	obs_data_set_default_bool(data, "enabled", false);
	obs_data_set_default_bool(data, "bidirectional", false);
	midi_input_name = QString(obs_data_get_string(data, "name"));
	midi_output_name = QString(obs_data_get_string(data, "outname"));
	input_port = DeviceManager().get_input_port_number(midi_input_name);
	output_port = DeviceManager().get_output_port_number(midi_output_name);
	enabled = obs_data_get_bool(data, "enabled");
	bidirectional = obs_data_get_bool(data, "bidirectional");
	obs_data_array_t *hooksData = obs_data_get_array(data, "hooks");
	const size_t hooksCount = obs_data_array_count(hooksData);
	for (size_t i = 0; i < hooksCount; i++) {
		obs_data_t *hookData = obs_data_array_item(hooksData, i);
		auto *mh = new MidiHook(obs_data_get_json(hookData));

		add_MidiHook(std::move(mh));
		obs_data_release(hookData);
	}
	obs_data_array_release(hooksData);
	obs_data_release(data);
}
/// <summary>
/// Sets the input port number and name
/// </summary>
/// <param name="port"></param>
void MidiAgent::set_input_port(const int port)
{
	input_port = port;
	midi_input_name = QString::fromStdString(midiin.get_port_name(port));
}
/// <summary>
/// Sets the output port number and name
/// </summary>
/// <param name="port"></param>
void MidiAgent::set_output_port(const int port)
{
	output_port = port;
	midi_output_name = QString::fromStdString(midiout.get_port_name(port));
}
/// <summary>
/// Opens MIDI input port
/// </summary>
void MidiAgent::open_midi_input_port()
{
	if (!midiin.is_port_open()) {
		try {
			midiin.open_port(input_port);
		} catch (const libremidi::midi_exception &error) {
			blog(LOG_DEBUG, "Midi Error %s", error.what());
		} catch (const libremidi::driver_error &error) {
			blog(LOG_DEBUG, "Midi Driver Error %s", error.what());
		} catch (const libremidi::system_error &error) {
			blog(LOG_DEBUG, "Midi system Error %s", error.what());
		}
		blog(LOG_INFO, "MIDI device connected In: [%d] %s", input_port, midi_input_name.toStdString().c_str());
	}
}
/// <summary>
/// Opens MIDI output port
/// </summary>
void MidiAgent::open_midi_output_port()
{
	if (!midiout.is_port_open()) {
		try {
			midiout.open_port(output_port);
		} catch (const libremidi::midi_exception &error) {
			blog(LOG_DEBUG, "Midi Error %s", error.what());
		} catch (const libremidi::driver_error &error) {
			blog(LOG_DEBUG, "Midi Driver Error %s", error.what());
		} catch (const libremidi::system_error &error) {
			blog(LOG_DEBUG, "Midi system Error %s", error.what());
		}
	}
}
/// <summary>
/// Closes both MIDI input and MIDI output ports
/// </summary>
void MidiAgent::close_both_midi_ports()
{
	close_midi_input_port();
	close_midi_output_port();
}
/// <summary>
/// Closes the connection to the Midi Input Port
/// *Does not cancel callback*
/// </summary>
void MidiAgent::close_midi_input_port()
{
	if (midiin.is_port_open()) {
		midiin.close_port();
	}
}
/// <summary>
/// Closes the connection to the MIDI output port
/// </summary>
void MidiAgent::close_midi_output_port()
{
	if (midiout.is_port_open()) {
		midiout.close_port();
	}
}
/// <summary>
///
/// </summary>
/// <returns></returns>
const QString &MidiAgent::get_midi_input_name() const
{
	return midi_input_name;
}
/// <summary>
///
/// </summary>
/// <returns></returns>
const QString &MidiAgent::get_midi_output_name() const
{
	return midi_output_name;
}
/// <summary>
///
/// </summary>
/// <param name="oname"></param>
void MidiAgent::set_midi_output_name(const QString &oname)
{
	midi_output_name = oname;
}
/// <summary>
///
/// </summary>
/// <param name="state"></param>
/// <returns></returns>
bool MidiAgent::set_bidirectional(const bool &state)
{
	this->bidirectional = state;
	if (!state) {
		if (midiout.is_port_open()) {
			midiout.close_port();
		}
	} else {
		open_midi_output_port();
	}
	GetConfig().get()->Save();
	return state;
}
/// <summary>
///
/// </summary>
/// <returns></returns>
int MidiAgent::GetPort() const
{
	return input_port;
}
/// <summary>
///
/// </summary>
/// <returns></returns>
bool MidiAgent::isEnabled() const
{
	return enabled;
}
/// <summary>
///
/// </summary>
/// <returns></returns>
bool MidiAgent::isBidirectional() const
{
	return bidirectional;
}
/// <summary>
///
/// </summary>
/// <returns></returns>
bool MidiAgent::isConnected() const
{
	return connected;
}
/// <summary>
/// Midi input callback.
/// Extend input handling functionality in the OBSController Class.
/// For OBS action triggers, edit the funcMap instead.
/// </summary>
/// <param name="message"></param>
/// <param name="userData"></param>
void MidiAgent::HandleInput(const libremidi::message &message, void *userData)
{
	auto *self = static_cast<MidiAgent *>(userData);
	if (!self->enabled) {
		return;
	}
	/*************Get Message parts***********/
	self->sending = true;
	auto *x = new MidiMessage();
	x->set_message(message);
	/***** Send Messages to emit function *****/
	x->device_name = self->get_midi_input_name();
	emit self->broadcast_midi_message((MidiMessage)*x);
	/** check if hook exists for this note or cc norc and launch it **/
	self->exe_midi_hook_if_exists(x);
	delete x;
}
/// <summary>
/// Callback function to handle midi errors
/// </summary>
/// <param name="error_type"></param>
/// <param name="error_message"></param>
/// <param name="userData"></param>
void MidiAgent::HandleError(const libremidi::midi_error &error_type, const std::string_view &error_message, void *userData)
{
	blog(LOG_ERROR, "Midi Error: %s", error_message.data());
	UNUSED_PARAMETER(error_type);
	UNUSED_PARAMETER(userData);
}

/// <summary>
/// Returns a QVector containing all Midi Hooks for this device
/// </summary>
/// <returns>QVector<MidiHook*></returns>
QVector<MidiHook *> MidiAgent::GetMidiHooks() const
{
	return midiHooks;
}
/// <summary>
/// Returns a MidiHook* if Message Type, NORC and Channel are found
/// </summary>
/// <param name="message">MidiMessage</param>
/// <returns>MidiHook*</returns>
MidiHook *MidiAgent::get_midi_hook_if_exists(MidiMessage *message)
{
	for (auto midiHook : this->midiHooks) {
		if (midiHook->message_type == message->message_type && midiHook->norc == message->NORC && midiHook->channel == message->channel) {
			if (midiHook->value_as_filter) {
				if (message->value == midiHook->value)
					return midiHook;
			} else
				return midiHook;
		}
	}
	return nullptr;
}
/// <summary>
/// Executes a MidiHook* if Message Type, NORC and Channel are found
/// </summary>
/// <param name="message">MidiMessage</param>
/// <returns>MidiHook*</returns>
void MidiAgent::exe_midi_hook_if_exists(MidiMessage *message)
{
	for (auto midiHook : this->midiHooks) {
		if (midiHook->message_type == message->message_type && midiHook->norc == message->NORC && midiHook->channel == message->channel) {
			if (midiHook->value_as_filter) {
				if (message->value == *midiHook->value)
					midiHook->EXE();
			} else {
				midiHook->value.emplace(message->value);
				midiHook->EXE();
			}
		}
	}
}
void MidiAgent::add_MidiHook(MidiHook *hook)
{
	// Add a new MidiHook
	midiHooks.push_back(hook);
}
/// <summary>
/// Sets wether or not this Midi Agent is enabled
/// </summary>
/// <param name="state">Enable State</param>
void MidiAgent::set_enabled(const bool &state)
{
	this->enabled = state;
	if (state)
		open_midi_input_port();
	else
		close_midi_input_port();
	GetConfig().get()->Save();
}
/// <summary>
/// Replaces current MidiHooks
/// </summary>
/// <param name="mh">Midi Hooks </param>
void MidiAgent::set_midi_hooks(QVector<MidiHook *> mh)
{
	midiHooks = std::move(mh);
}
/// <summary>
/// Remove a midi hook
/// *This does not remove from config unless saved afterwards*
/// </summary>
/// <param name="hook"></param>
void MidiAgent::remove_MidiHook(MidiHook *hook)
{
	// Remove a MidiHook
	if (midiHooks.contains(hook)) {
		midiHooks.removeOne(hook);
		delete (hook);
	}
}
void MidiAgent::edit_midi_hook(MidiHook *old_hook, MidiHook *new_hook)
{
	remove_MidiHook(old_hook);
	add_MidiHook(new_hook);
}
/// <summary>
/// Clears all the MidiHooks for this device.
/// *This does not delete hooks from config unless saved afterwards*
/// </summary>
void MidiAgent::clear_MidiHooks()
{
	for (auto hook:midiHooks) {
		delete hook;
	}
	midiHooks.clear();
}
/// <summary>
/// Get this MidiAgent state as OBS Data. (includes midi hooks)
/// *This is needed to Serialize the state in the config.*
/// https://obsproject.com/docs/reference-settings.html
/// </summary>
/// <returns>QString (OBSData Json string)</returns>
QString MidiAgent::GetData()
{
	blog(LOG_DEBUG, "MA::GetData");
	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "name", midi_input_name.toStdString().c_str());
	obs_data_set_string(data, "outname", midi_output_name.toStdString().c_str());
	obs_data_set_bool(data, "enabled", enabled);
	obs_data_set_bool(data, "bidirectional", bidirectional);
	obs_data_array_t *arrayData = obs_data_array_create();
	for (auto midiHook : midiHooks) {
		obs_data_t *hookData = obs_data_create_from_json(midiHook->GetData().toStdString().c_str());
		obs_data_array_push_back(arrayData, hookData);
		obs_data_release(hookData);
	}
	obs_data_set_array(data, "hooks", arrayData);
	QString return_data(obs_data_get_json(data));
	obs_data_array_release(arrayData);
	obs_data_release(data);
	return return_data;
}
/// <summary>
/// Get Midi Hook, For use with events
/// </summary>
/// <param name="event"></param>
/// <returns>MidiHook *</returns>
MidiHook *MidiAgent::get_midi_hook_if_exists(const RpcEvent &event) const
{
	for (auto hook: this->midiHooks) {
		bool found = false;
		switch (ActionsClass::string_to_action(Utils::untranslate(hook->action))) {
		case ActionsClass::Actions::Set_Volume:
			found = (hook->audio_source == QString(obs_data_get_string(event.additionalFields(), "sourceName")) &&
				 event.updateType() == "SourceVolumeChanged");
			break;
		case ActionsClass::Actions::Toggle_Mute:
			found = (hook->audio_source == QString(obs_data_get_string(event.additionalFields(), "sourceName")) &&
				 event.updateType() == "SourceMuteStateChanged");
			break;
		case ActionsClass::Actions::Do_Transition:
		case ActionsClass::Actions::Set_Preview_Scene:
		case ActionsClass::Actions::Set_Current_Scene:
			found = (hook->scene == QString(obs_data_get_string(event.additionalFields(), "scene-name")));
			break;
		case ActionsClass::Actions::Toggle_Start_Stop_Recording:
		case ActionsClass::Actions::Start_Recording:
		case ActionsClass::Actions::Stop_Recording:
			found = ((event.updateType() == "RecordingStarted") || (event.updateType() == "RecordingStopped") ||
				 (event.updateType() == "RecordingStopping"));
			break;
		case ActionsClass::Actions::Toggle_Start_Stop_Streaming:
		case ActionsClass::Actions::Start_Streaming:
		case ActionsClass::Actions::Stop_Streaming:
			found = ((event.updateType() == "StreamStarted") || (event.updateType() == "StreamStopped") ||
				 (event.updateType() == "StreamStopping"));
			break;
		case ActionsClass::Actions::Disable_Preview:

			break;
		case ActionsClass::Actions::Disable_Source_Filter:

			break;

		case ActionsClass::Actions::Enable_Preview:

			break;
		case ActionsClass::Actions::Enable_Source_Filter:

			break;
		case ActionsClass::Actions::Move_T_Bar:

			break;
		case ActionsClass::Actions::Next_Media:

			break;
		case ActionsClass::Actions::Pause_Recording:

			break;
		case ActionsClass::Actions::Play_Pause_Media:

			break;
		case ActionsClass::Actions::Previous_Media:

			break;
		case ActionsClass::Actions::Reload_Browser_Source:

			break;
		case ActionsClass::Actions::Reset_Scene_Item:

			break;
		case ActionsClass::Actions::Reset_Stats:

			break;
		case ActionsClass::Actions::Restart_Media:

			break;
		case ActionsClass::Actions::Resume_Recording:

			break;
		case ActionsClass::Actions::Save_Replay_Buffer:

			break;
		case ActionsClass::Actions::Scrub_Media:

			break;
		case ActionsClass::Actions::Set_Audio_Monitor_Type:

			break;
		case ActionsClass::Actions::Set_Browser_Source_URL:

			break;
		case ActionsClass::Actions::Set_Current_Profile:

			break;
		case ActionsClass::Actions::Set_Current_Scene_Collection:

			break;
		case ActionsClass::Actions::Set_Current_Transition:

			break;
		case ActionsClass::Actions::Set_Gain_Filter:

			break;
		case ActionsClass::Actions::Set_Media_Time:

			break;
		case ActionsClass::Actions::Set_Mute:

			break;
		case ActionsClass::Actions::Set_Opacity:

			break;
		case ActionsClass::Actions::Set_Scene_Item_Crop:

			break;
		case ActionsClass::Actions::Set_Scene_Item_Position:

			break;
		case ActionsClass::Actions::Set_Scene_Item_Render:

			break;
		case ActionsClass::Actions::Set_Scene_Item_Transform:

			break;
		case ActionsClass::Actions::Set_Scene_Transition_Override:

			break;
		case ActionsClass::Actions::Set_Source_Filter_Visibility:

			break;
		case ActionsClass::Actions::Set_Source_Name:

			break;
		case ActionsClass::Actions::Set_Source_Position:

			break;
		case ActionsClass::Actions::Set_Source_Rotation:

			break;
		case ActionsClass::Actions::Set_Source_Scale:

			break;
		case ActionsClass::Actions::Set_Source_Settings:

			break;
		case ActionsClass::Actions::Set_Sync_Offset:

			break;
		case ActionsClass::Actions::Set_Text_GDIPlus_Text:

			break;
		case ActionsClass::Actions::Set_Transition_Duration:

			break;
		case ActionsClass::Actions::Start_Replay_Buffer:

			break;
		case ActionsClass::Actions::Stop_Media:

			break;
		case ActionsClass::Actions::Stop_Replay_Buffer:

			break;
		case ActionsClass::Actions::Studio_Mode:

			break;
		case ActionsClass::Actions::Take_Screenshot:

			break;
		case ActionsClass::Actions::Take_Source_Screenshot:

			break;
		case ActionsClass::Actions::Toggle_Source_Filter:

			break;
		case ActionsClass::Actions::Toggle_Source_Visibility:

			break;
		case ActionsClass::Actions::Toggle_Start_Stop_Replay_Buffer:

			break;
		case ActionsClass::Actions::Unpause_Recording:

			break;
		}
		if (found)
			return hook;
	}
	return NULL;
}
/*Handle OBS events*/
void MidiAgent::handle_obs_event(const RpcEvent &event)
{
	blog(LOG_DEBUG, "OBS Event : %s \n AD: %s", event.updateType().toStdString().c_str(), obs_data_get_json(event.additionalFields()));
	if (event.updateType() == "FinishedLoading") {
		loading = false;
		return;
	}
	if (loading)
		return;
	MidiHook *hook = get_midi_hook_if_exists(event);

	/// <summary>
	/// 	ON EVENT TYPE Find matching hook, pull data from that hook, and do thing.
	/// </summary>
	/// <param name="event"></param>
	if (hook != NULL) {
		MidiMessage *message = hook->get_message_from_hook();
		switch (Events::string_to_event(event.updateType())) {
		case Events::event_type::SourceVolumeChanged:
			Macro::set_volume(this, message, obs_data_get_double(event.additionalFields(), "volume"));
			break;
		case Events::event_type::SwitchScenes:
			Macro::swap_buttons(this, message, state::previous_scene_norc, hook->norc);
			state::previous_scene_norc = hook->norc;
			blog(LOG_DEBUG, "Switch Scenes %s", obs_data_get_string(event.additionalFields(), "scene-name"));
			break;
		case Events::event_type::PreviewSceneChanged:
			Macro::swap_buttons(this, message, state::previous_preview_scene_norc, hook->norc);
			state::previous_preview_scene_norc = hook->norc;
			blog(LOG_DEBUG, "Scene Preview Changed");
			break;
		case Events::event_type::SourceMuteStateChanged:
			Macro::set_on_off(this, message, !obs_data_get_bool(event.additionalFields(), "muted"));
			break;
		case Events::event_type::StreamStarted:
			Macro::set_on_off(this, message, true);
			break;
		case Events::event_type::StreamStarting:
			Macro::set_on_off(this, message, false);
			break;
		case Events::event_type::StreamStopped:
			Macro::set_on_off(this, message, false);
			break;
		case Events::event_type::StreamStopping:
			Macro::set_on_off(this, message, false);
			break;
		case Events::event_type::RecordingStarted:
			Macro::set_on_off(this, message, true);
			break;
		case Events::event_type::RecordingStarting:
			Macro::set_on_off(this, message, false);
			break;
		case Events::event_type::RecordingStopping:
			Macro::set_on_off(this, message, true);
			break;
		case Events::event_type::RecordingStopped:
			Macro::set_on_off(this, message, false);
			break;
		case Events::event_type::SceneChanged:
			Macro::swap_buttons(this, message, state::previous_scene_norc, hook->norc);
			state::previous_scene_norc = hook->norc;
			blog(LOG_DEBUG, "Scene Changed");
			break;
		}

		delete (message);
	} else {
		/// <summary>
		/// Events that dont need a hook
		/// </summary>
		/// <param name="event"></param>
		switch (Events::string_to_event(event.updateType())) {
		case Events::event_type::LoadingFinished:
			startup();
			break;
		case Events::event_type::SourceRenamed:
			rename_source(event);
			break;
		case Events::event_type::Exiting:
			state::closing = true;
			break;
		case Events::event_type::SourceRemoved:
			remove_source(event);
			break;
		case Events::event_type::ProfileChanged:
			GetDeviceManager().get()->reload();
			break;
		case Events::event_type::SceneCollectionChanged:
			GetDeviceManager().get()->reload();
			break;
		case Events::event_type::TransitionBegin:
			break;
		}
	}
}
/// <summary>
/// Find all hooks that have name, and remove hook
/// </summary>
/// <param name="event">Incoming RpcEvent</param>
void MidiAgent::remove_source(const RpcEvent &event)
{
	if (state::closing)
		return;

	const QString from = obs_data_get_string(event.additionalFields(), "sourceName");
	for (auto midiHook : this->midiHooks) {
		if (midiHook->source == from) {
			this->remove_MidiHook(midiHook);
			GetConfig().get()->Save();
		}
	}
	GetConfig()->Save();
}
/// <summary>
/// Find all hooks that have name, and replace name
/// </summary>
/// <param name="event">incoming RpcEvent</param>
void MidiAgent::rename_source(const RpcEvent &event)
{
	blog(LOG_DEBUG, "Rename source %s to %s", obs_data_get_string(event.additionalFields(), "previousName"),
	     obs_data_get_string(event.additionalFields(), "newName"));
	const QString from = obs_data_get_string(event.additionalFields(), "previousName");
	for (auto midiHook : this->midiHooks) {
		if (midiHook->scene == from) {
			midiHook->scene = obs_data_get_string(event.additionalFields(), "newName");
			GetConfig().get()->Save();
		} else if (midiHook->source == from) {
			midiHook->source = obs_data_get_string(event.additionalFields(), "newName");
			GetConfig().get()->Save();
		}
	}
}
/// <summary>
/// Sends message to midi Devices
/// </summary>
/// <param name="message">MidiMessage to send</param>
void MidiAgent::send_message_to_midi_device(const MidiMessage &message)
{
	if (message.message_type != "none") {
		std::unique_ptr<libremidi::message> hello = std::make_unique<libremidi::message>();
		if (message.message_type == "Control Change") {
			this->midiout.send_message(hello->control_change(message.channel, message.NORC, message.value));
		} else if (message.message_type == "Note On") {
			this->midiout.send_message(hello->note_on(message.channel, message.NORC, message.value));
		} else if (message.message_type == "Note Off") {
			this->midiout.send_message(hello->note_off(message.channel, message.NORC, message.value));
		}
	}
}
/// <summary>
/// Sends Message to Midi device
/// </summary>
/// <param name="bytes">Midi Message in Bytes</param>
void MidiAgent::send_bytes(unsigned char bytes)
{
	midiout.send_message((unsigned char *)bytes, sizeof(bytes));
}
/// <summary>
/// Gets all audio sources, and iterates over them to set the volumes
/// Used to set volume sliders on startup
/// </summary>
void MidiAgent::set_current_volumes()
{
	const auto volumelist = Utils::GetAudioSourceNames();
	for (int i = 0; i < volumelist.count(); i++) {

		const auto source = obs_get_source_by_name(volumelist.at(i).toStdString().c_str());
		const auto vol = obs_source_get_volume(source);
		obs_data_t *additional = obs_data_create();
		obs_data_set_string(additional, "sourceName", volumelist.at(i).toStdString().c_str());

		auto *event = new RpcEvent(QString("SourceVolumeChanged"), NULL, NULL, additional);
		MidiHook *hook = get_midi_hook_if_exists((RpcEvent)*event);
		obs_source_release(source);
		obs_data_release(additional);
		if (hook == nullptr)
			return;
		blog(LOG_DEBUG, "Get Volume %s is %i", volumelist.at(i).toStdString().c_str(), Utils::mapper2(vol));
		Macro::set_volume(this, hook->get_message_from_hook(), vol);
	}
}
/// <summary>
/// Used to explicitely set midi device state on program startup
/// Some actions happen automatically due to Events from obs being sent during startup
/// </summary>
void MidiAgent::startup()
{
	// set_current_scene();
	set_current_volumes();
}
