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
#include "obs-controller.h"
#include "macro-helpers.h"
#include <thread>
#ifdef _WIN32
#include <Windows.h>
#define tsleep _sleep
#else
#include <unistd.h>
#define tsleep(x) usleep(x * 1000)
#endif
#include <util/platform.h>

Actions::Actions(MidiHook *_hook) : hook{_hook}
{
	if (_action_map.isEmpty())
		make_map();
}
void Actions::make_map()
{
	_action_map.insert(QString("Set_Current_Scene"), new SetCurrentScene());
	_action_map.insert(QString("Reset_Scene_Item"), new ResetSceneItem());
	_action_map.insert(QString("Toggle_Mute"), new ToggleMute());
	_action_map.insert(QString("Do_Transition"), new TransitionToProgram());
	_action_map.insert(QString("Set_Current_Transition"), new SetCurrentTransition());
	_action_map.insert(QString("Set_Mute"), new SetMute());
	_action_map.insert(QString("Toggle_Start_Stop_Streaming"), new StartStopStreaming());
	_action_map.insert(QString("Set_Preview_Scene"), new SetPreviewScene());
	_action_map.insert(QString("Set_Current_Scene_Collection"), new SetCurrentSceneCollection());
	_action_map.insert(QString("Set_Transition_Duration"), new SetTransitionDuration());
	_action_map.insert(QString("Start_Streaming"), new StartStreaming());
	_action_map.insert(QString("Stop_Streaming"), new StopStreaming());
	_action_map.insert(QString("Start_Recording"), new StartRecording());
	_action_map.insert(QString("Stop_Recording"), new StopRecording());
	_action_map.insert(QString("Start_Replay_Buffer"), new StartReplayBuffer());
	_action_map.insert(QString("Stop_Replay_Buffer"), new StopReplayBuffer());
	_action_map.insert(QString("Set_Volume"), new SetVolume());
	_action_map.insert(QString("Take_Source_Screenshot"), new TakeSourceScreenshot());
	_action_map.insert(QString("Pause_Recording"), new PauseRecording());
	_action_map.insert(QString("Enable_Source_Filter"), new EnableSourceFilter());
	_action_map.insert(QString("Disable_Source_Filter"), new DisableSourceFilter());
	_action_map.insert(QString("Toggle_Start_Stop_Recording"), new StartStopRecording());
	_action_map.insert(QString("Toggle_Start_Stop_Replay_Buffer"), new StartStopReplayBuffer());
	_action_map.insert(QString("Resume_Recording"), new ResumeRecording());
	_action_map.insert(QString("Save_Replay_Buffer"), new SaveReplayBuffer());
	_action_map.insert(QString("Set_Current_Profile"), new SetCurrentProfile());
	_action_map.insert(QString("Toggle_Source_Filter"), new ToggleSourceFilter());
	_action_map.insert(QString("Set_Text_GDIPlus_Text"), new SetTextGDIPlusText());
	_action_map.insert(QString("Set_Browser_Source_URL"), new SetBrowserSourceURL());
	_action_map.insert(QString("Reload_Browser_Source"), new ReloadBrowserSource());
	_action_map.insert(QString("Set_Sync_Offset"), new SetSyncOffset());
	_action_map.insert(QString("Set_Source_Rotation"), new SetSourceRotation());
	_action_map.insert(QString("Set_Source_Position"), new SetSourcePosition());
	_action_map.insert(QString("Set_Gain_Filter"), new SetGainFilter());
	_action_map.insert(QString("Set_Opacity"), new SetOpacity());
	_action_map.insert(QString("Set_Source_Scale"), new SetSourceScale());
	_action_map.insert(QString("Move_T_Bar"), new move_t_bar());
	_action_map.insert(QString("Play_Pause_Media"), new play_pause_media_source());
	_action_map.insert(QString("Studio_Mode"), new toggle_studio_mode());
	_action_map.insert(QString("Reset_Stats"), new reset_stats());
	_action_map.insert(QString("Restart_Media"), new restart_media());
	_action_map.insert(QString("Stop_Media"), new stop_media());
	_action_map.insert(QString("Previous_Media"), new prev_media());
	_action_map.insert(QString("Next_Media"), new next_media());
	_action_map.insert(QString("Toggle_Source_Visibility"), new ToggleSourceVisibility());
	_action_map.insert(QString("Take_Screenshot"), new TakeScreenshot());
	_action_map.insert(QString("Disable_Preview"), new DisablePreview());
	_action_map.insert(QString("Enable_Preview"), new EnablePreview());
	_action_map.insert(QString("Toggle_Fade_Source"), new make_opacity_filter());
	_action_map.insert(QString("Trigger_Hotkey_By_Name"), new TriggerHotkey());
	_action_map.insert(QString("Trigger Hotkey"), new TriggerHotkey());
}

Actions *Actions::make_action(QString action, MidiHook *h)
{
	Actions *act = _action_map[action];
	act->set_hook(h);
	return act;
}
Actions *Actions::make_action(QString action)
{
	Actions *act = _action_map[action];
	return act;
}
QString Actions::get_action_string()
{
	return QString(Utils::translate_action_string(hook->action))
		.append(" using ")
		.append(this->hook->message_type)
		.append(" ")
		.append(QString::number(this->hook->norc));
}
////////////////////
// BUTTON ACTIONS //
////////////////////
/*
 * Sets the currently active scene
 */
void SetCurrentScene::execute()
{
	const OBSSourceAutoRelease source = obs_get_source_by_name(hook->scene.toUtf8());
	obs_frontend_set_current_scene(source);
}
/**
 * Sets the scene in preview. Must be in Studio mode or will throw error
 */
void SetPreviewScene::execute()
{
	if (!obs_frontend_preview_program_mode_active()) {
		blog(LOG_INFO, "Can Not Set Preview scene -- studio mode not enabled");
	}
	const OBSScene scene = Utils::GetSceneFromNameOrCurrent(hook->scene);
	if (!scene) {
		blog(LOG_DEBUG, "specified scene doesn't exist");
	}
	obs_source_t *source = obs_scene_get_source(scene);
	obs_frontend_set_current_preview_scene(source);
}
QString SetPreviewScene::get_action_string()
{
	return QString(Utils::translate_action_string(hook->action))
		.append(" using ")
		.append(this->hook->message_type)
		.append(" ")
		.append(QString::number(this->hook->norc));
}
void DisablePreview::execute()
{
	obs_queue_task(
		OBS_TASK_UI,
		[](void *param) {
			if (obs_frontend_preview_enabled()) {
				obs_frontend_set_preview_enabled(false);
			}
			(void)param;
		},
		nullptr, true);
}
void EnablePreview::execute()
{
	obs_queue_task(
		OBS_TASK_UI,
		[](void *param) {
			obs_frontend_set_preview_enabled(true);
			(void)param;
		},
		nullptr, true);
}
/**
 * Change the active scene collection.
 */
void SetCurrentSceneCollection::execute()
{
	// TODO : Check if specified profile exists and if changing is allowed
	obs_frontend_set_current_scene_collection(hook->scene_collection.toUtf8());
}
/**
 * Reset a scene item.
 */
void ResetSceneItem::execute()
{
	const OBSScene scene = Utils::GetSceneFromNameOrCurrent(hook->scene);
	if (!scene) {
		throw("requested scene doesn't exist");
	}
	const OBSSceneItemAutoRelease sceneItem = Utils::GetSceneItemFromName(scene, hook->source);
	if (!sceneItem) {
		throw("specified scene item doesn't exist");
	}
	const OBSSourceAutoRelease sceneItemSource = obs_sceneitem_get_source(sceneItem);
	const OBSDataAutoRelease settings = obs_source_get_settings(sceneItemSource);

	obs_source_update(sceneItemSource, settings);
}
/**
 * Transitions the currently previewed scene to the main output.
 */
/**
 * Transitions the currently previewed scene to the main output using specified transition.
 * transitionDuration is optional. (milliseconds)
 */
void TransitionToProgram::execute()
{
	if (state::transitioning)
		return;
	state()._CurrentTransitionDuration = obs_frontend_get_transition_duration();
	obs_source_t *transition = obs_frontend_get_current_transition();
	QString scenename;
	/**
	 * If Transition from hook is not Current Transition, and if it is not an empty Value, then set current transition
	 */
	if ((hook->transition != "Current Transition") && !hook->transition.isEmpty() && !hook->transition.isNull()) {
		Utils::SetTransitionByName(hook->transition);
		state()._TransitionWasCalled = true;
	}
	if ((hook->scene != "Preview Scene") && !hook->scene.isEmpty() && !hook->scene.isNull()) {
		state()._TransitionWasCalled = true;
	}
	if (hook->scene == "Preview Scene") {
		obs_source_t *source = obs_frontend_get_current_scene();
		hook->scene = QString(obs_source_get_name(source));
		state()._TransitionWasCalled = true;
	}
	if (hook->int_override && *hook->int_override > 0) {
		obs_frontend_set_transition_duration(*hook->int_override);
		state()._TransitionWasCalled = true;
	}
	(obs_frontend_preview_program_mode_active()) ? obs_frontend_preview_program_trigger_transition() : SetCurrentScene().execute();

	state()._CurrentTransition = QString(obs_source_get_name(transition));

	obs_source_release(transition);
}

QGridLayout *TransitionToProgram::set_widgets()
{


	auto scenelist = Utils::get_scene_names();
	scenelist.prepend("Preview Scene");
	scene = Utils::make_combo(scenelist);
	auto transition_list = Utils::GetTransitionsList();
	transition_list.prepend("Current Transition");
	transition = Utils::make_combo(transition_list);
	enable_duration = new QCheckBox("Enable");
	duration = new QSpinBox();
	duration->setValue(300);
	duration->setMaximum(100000);
	duration->setMinimum(0);
	duration->setSuffix(" ms");
	duration->setEnabled(false);

	auto lay = new QGridLayout();
	lay->addWidget(new QLabel("Scene *"), 0, 0);
	lay->addWidget(scene, 0, 1);
	lay->addWidget(new QLabel("Transition *"), 1, 0);
	lay->addWidget(transition, 1, 1);
	lay->addWidget(new QLabel("Duration *"), 2, 0);
	lay->addWidget(enable_duration, 2, 1);
	lay->addWidget(duration, 2, 2);
	lay->setAlignment(Qt::AlignTop);

	return lay;
}
/**
 * Set the active transition.
 */
void SetCurrentTransition::execute()
{
	Utils::SetTransitionByName(hook->transition);
}
/**
 * Set the duration of the currently active transition
 */
void SetTransitionDuration::execute()
{
	obs_frontend_set_transition_duration(*hook->duration);
}
void SetSourceVisibility::execute()
{
	obs_sceneitem_set_visible(Utils::GetSceneItemFromName(Utils::GetSceneFromNameOrCurrent(hook->scene), hook->source), *hook->value);
}
/**
 *
 * Toggles the source's visibility
 * seems to stop audio from playing as well
 *
 */
void ToggleSourceVisibility::execute()
{
	const auto scene = Utils::GetSceneItemFromName(Utils::GetSceneFromNameOrCurrent(hook->scene), hook->source);
	if (obs_sceneitem_visible(scene)) {
		obs_sceneitem_set_visible(scene, false);
	} else {
		obs_sceneitem_set_visible(scene, true);
	}
}
	/**
 * Inverts the mute status of a specified source.
 */
void ToggleMute::execute()
{
	if (hook->audio_source.isEmpty()) {
		throw("sourceName is empty");
	}
	obs_source *source = obs_get_source_by_name(hook->audio_source.qtocs());
	if (!source) {
		throw("sourceName not found");
	}
	obs_source_set_muted(source, !obs_source_muted(source));
}
/**
 * Sets the mute status of a specified source.
 */
void SetMute::execute()
{
	if (hook->source.isEmpty()) {
		throw("sourceName is empty");
	}
	const OBSSourceAutoRelease source = obs_get_source_by_name(hook->source.toUtf8());
	if (!source) {
		throw("specified source doesn't exist");
	}
	obs_source_set_muted(source, *hook->value);
}
QString SetMute::get_action_string()
{
	return QString(Utils::translate_action_string(hook->action))
		.append(" with ")
		.append(this->hook->message_type)
		.append(" ")
		.append(QString::number(this->hook->norc));
}
/**
 * Toggle streaming on or off.
 */
void StartStopStreaming::execute()
{
	if (obs_frontend_streaming_active())
		obs_frontend_streaming_stop();
	else
		obs_frontend_streaming_start();
}
/**
 * Start streaming.
 */
void StartStreaming::execute()
{
	if (!obs_frontend_streaming_active()) {
		obs_frontend_streaming_start();
	}
}
/**
 * Stop streaming.
 */
void StopStreaming::execute()
{
	if (obs_frontend_streaming_active()) {
		obs_frontend_streaming_stop();
	}
}
/**
 * Toggle recording on or off.
 */
void StartStopRecording::execute()
{
	(obs_frontend_recording_active() ? obs_frontend_recording_stop() : obs_frontend_recording_start());
}
/**
 * Start recording.
 */
void StartRecording::execute()
{
	if (!obs_frontend_recording_active()) {
		obs_frontend_recording_start();
	}
}
/**
 * Stop recording.
 */
void StopRecording::execute()
{
	if (obs_frontend_recording_active()) {
		obs_frontend_recording_stop();
	}
}
/**
 * Pause the current recording.
 */
void PauseRecording::execute()
{
	if (obs_frontend_recording_active()) {
		obs_frontend_recording_pause(true);
	}
}
/**
 * Resume/unpause the current recording (if paused).
 */
void ResumeRecording::execute()
{
	if (obs_frontend_recording_active()) {
		obs_frontend_recording_pause(false);
	}
}
/**
 * Toggle the Replay Buffer on/off.
 */
void StartStopReplayBuffer::execute()
{
	if (!Utils::ReplayBufferEnabled()) {
		Utils::alert_popup("replay buffer disabled in settings");
		return;
	}
	if (obs_frontend_replay_buffer_active()) {
		obs_frontend_replay_buffer_stop();
	} else {
		Utils::StartReplayBuffer();
	}
}
/**
 * Start recording into the Replay Buffer.
 * Will throw an error if "Save Replay Buffer" hotkey is not set in OBS' settings.
 * Setting this hotkey is mandatory, even when triggering saves only
 * through obs-midi.
 */
void StartReplayBuffer::execute()
{
	if (!Utils::ReplayBufferEnabled()) {
		Utils::alert_popup("replay buffer disabled in settings");
		return;
	}
	if (!obs_frontend_replay_buffer_active()) {
		Utils::StartReplayBuffer();
	}
}
/**
 * Stop recording into the Replay Buffer.
 */
void StopReplayBuffer::execute()
{
	if (!Utils::ReplayBufferEnabled()) {
		Utils::alert_popup("replay buffer disabled in settings");
		return;
	}
	if (obs_frontend_replay_buffer_active()) {
		obs_frontend_replay_buffer_stop();
	}
}
/**
 * Flush and save the contents of the Replay Buffer to disk. This is
 * basically the same as triggering the "Save Replay Buffer" hotkey.
 * Will return an `error` if the Replay Buffer is not active.
 */
void SaveReplayBuffer::execute()
{
	if (!Utils::ReplayBufferEnabled()) {
		Utils::alert_popup("replay buffer disabled in settings");
		return;
	}
	if (!obs_frontend_replay_buffer_active()) {
		Utils::alert_popup("replay buffer not active");
		return;
	}
	const OBSOutputAutoRelease replayOutput = obs_frontend_get_replay_buffer_output();
	calldata_t cd = {0};
	proc_handler_t *ph = obs_output_get_proc_handler(replayOutput);
	proc_handler_call(ph, "save", &cd);
	calldata_free(&cd);
}
void SetCurrentProfile::execute()
{
	if (hook->profile.isEmpty()) {
		throw("profile name is empty");
	}
	// TODO : check if profile exists
	obs_frontend_set_current_profile(hook->profile.toUtf8());
}
void SetTextGDIPlusText::execute() {}
void SetBrowserSourceURL::execute()
{
	const OBSSourceAutoRelease source = obs_get_source_by_name(hook->source.toUtf8());
	const QString sourceId = obs_source_get_id(source);
	if (sourceId != "browser_source" && sourceId != "linuxbrowser-source") {
		return blog(LOG_DEBUG, "Not a browser Source");
	}
	const OBSDataAutoRelease settings = obs_source_get_settings(source);
	obs_data_set_string(settings, "url", hook->string_override.toUtf8());
	obs_source_update(source, settings);
}
void ReloadBrowserSource::execute()
{
	const OBSSourceAutoRelease source = obs_get_source_by_name(hook->source.toUtf8());
	obs_properties_t *sourceProperties = obs_source_properties(source);
	obs_property_t *property = obs_properties_get(sourceProperties, "refreshnocache");
	obs_property_button_clicked(property, source); // This returns a boolean but we ignore it because the browser plugin always returns `false`.
	obs_properties_destroy(sourceProperties);
}
void TakeScreenshot::execute()
{
	obs_frontend_take_screenshot();
}
void TakeSourceScreenshot::execute()
{
	const OBSSourceAutoRelease source = obs_get_source_by_name(hook->scene.toUtf8());
	obs_frontend_take_source_screenshot(source);
}
void EnableSourceFilter::execute()
{
	const OBSSourceAutoRelease source = obs_get_source_by_name(hook->source.toUtf8());
	const OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, hook->filter.toUtf8());
	obs_source_set_enabled(filter, true);
}
void DisableSourceFilter::execute()
{
	const OBSSourceAutoRelease source = obs_get_source_by_name(hook->source.toUtf8());
	const OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, hook->filter.toUtf8());
	obs_source_set_enabled(filter, false);
}
void ToggleSourceFilter::execute()
{
	const OBSSourceAutoRelease source = obs_get_source_by_name(hook->source.toUtf8());
	const OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, hook->filter.toUtf8());
	if (obs_source_enabled(filter)) {
		obs_source_set_enabled(filter, false);
	} else {
		obs_source_set_enabled(filter, true);
	}
}
void TriggerHotkey::execute()
{
	obs_hotkey_t *obsHotkey = Utils::get_obs_hotkey_by_name(hook->hotkey);
	if (!obsHotkey) {
		blog(LOG_ERROR, "ERROR: Triggered hotkey <%s> was not found", hook->hotkey.qtocs());
		return;
	}
	obs_hotkey_trigger_routed_callback(obs_hotkey_get_id(obsHotkey), true);
}

QString TriggerHotkey::get_action_string()
{
	return QString("Trigger Hotkey")
		.append(" ")
		.append(obs_hotkey_get_description(Utils::get_obs_hotkey_by_name(hook->hotkey)))
		.append(" using ")
		.append(hook->message_type)
		.append(" ")
		.append(QString::number(this->hook->norc));
}

////////////////
// CC ACTIONS //
////////////////
void SetVolume::execute()
{
	const OBSSourceAutoRelease obsSource = obs_get_source_by_name(hook->audio_source.toUtf8());
	obs_source_set_volume(obsSource, pow(Utils::mapper(*hook->value), 3.0));
}
QString SetVolume::get_action_string()
{
	return QString(Utils::translate_action_string(hook->action))
		.append(" of ")
		.append(hook->audio_source)
		.append(" using ")
		.append(this->hook->message_type)
		.append(" ")
		.append(QString::number(this->hook->norc));
}
/**
 * Set the audio sync offset of a specified source.
 */
void SetSyncOffset::execute()
{
	const OBSSourceAutoRelease source = obs_get_source_by_name(hook->source.toUtf8());
	obs_source_set_sync_offset(source, *hook->value);
}
void SetSourcePosition::execute() {}
void SetSourceRotation::execute()
{
	obs_scene_t *scene = Utils::GetSceneFromNameOrCurrent(hook->scene);
	obs_sceneitem_t *item = Utils::GetSceneItemFromName(scene, hook->source);
	uint32_t current = obs_sceneitem_get_alignment(item);
	obs_sceneitem_set_alignment(item, OBS_ALIGN_CENTER);
	const float rotation = Utils::map_to_range((hook->range_min) ? *hook->range_min : 0, (hook->range_max) ? *hook->range_max : 360, *hook->value);
	obs_sceneitem_set_rot(item, rotation);
}
void SetSourceScale::execute()
{
	obs_scene_t *scene = Utils::GetSceneFromNameOrCurrent(hook->scene);
	obs_sceneitem_t *item = Utils::GetSceneItemFromName(scene, hook->source);
	uint32_t current = obs_sceneitem_get_alignment(item);
	obs_sceneitem_set_alignment(item, OBS_ALIGN_CENTER);
	obs_sceneitem_set_bounds_type(item, obs_bounds_type::OBS_BOUNDS_NONE);
	vec2 *scale = new vec2();
	vec2_set(scale, Utils::map_to_range(0, (hook->range_min) ? *hook->range_min : 1, *hook->value),
		 Utils::map_to_range(0, (hook->range_max) ? *hook->range_max : 1, *hook->value));
	obs_sceneitem_set_scale(item, scale);
	delete (scale);
}
void SetGainFilter::execute() {}
void SetOpacity::execute() {}
void move_t_bar::execute()
{
	if (obs_frontend_preview_program_mode_active()) {
		obs_frontend_set_tbar_position(Utils::t_bar_mapper(*hook->value));
		obs_frontend_release_tbar();
	}
}
void play_pause_media_source::execute()
{
	const OBSSourceAutoRelease source = obs_get_source_by_name(hook->media_source.toUtf8());
	switch (obs_source_media_get_state(source)) {
	case obs_media_state::OBS_MEDIA_STATE_PAUSED:
		obs_source_media_play_pause(source, false);
		break;
	case obs_media_state::OBS_MEDIA_STATE_PLAYING:
		obs_source_media_play_pause(source, true);
		break;
	case obs_media_state::OBS_MEDIA_STATE_ENDED:
		obs_source_media_restart(source);
		break;
	case OBS_MEDIA_STATE_NONE:
		break;
	case OBS_MEDIA_STATE_OPENING:
		break;
	case OBS_MEDIA_STATE_BUFFERING:
		break;
	case OBS_MEDIA_STATE_STOPPED:
		break;
	case OBS_MEDIA_STATE_ERROR:
		break;
	}
}
// TODO:: Fix this
void toggle_studio_mode::execute()
{
	obs_queue_task(
		OBS_TASK_UI,
		[](void *param) {
			obs_frontend_set_preview_program_mode(!obs_frontend_preview_program_mode_active());

			UNUSED_PARAMETER(param);
		},
		nullptr, true);
}
void reset_stats::execute() {}
void restart_media::execute()
{
	const OBSSourceAutoRelease source = obs_get_source_by_name(hook->media_source.toUtf8());
	obs_source_media_restart(source);
}
void play_media::execute()
{
	const OBSSourceAutoRelease source = obs_get_source_by_name(hook->media_source.toUtf8());
	obs_source_media_play_pause(source, false);
}
void stop_media::execute()
{
	const OBSSourceAutoRelease source = obs_get_source_by_name(hook->media_source.toUtf8());
	obs_source_media_stop(source);
}
void next_media::execute()
{
	const OBSSourceAutoRelease source = obs_get_source_by_name(hook->media_source.toUtf8());
	obs_source_media_next(source);
}
void prev_media::execute()
{
	const OBSSourceAutoRelease source = obs_get_source_by_name(hook->media_source.toUtf8());
	obs_source_media_previous(source);
}

float time_to_sleep(float duration)
{
	return duration / 2000;
}

void fade_in_scene_item(MidiHook *hook)
{
	try {
		std::thread th{[=]() {
			obs_data_t *data = obs_data_create();
			obs_data_set_double(data, "opacity", 0);
			OBSSourceAutoRelease filter = obs_source_create_private("color_filter", "ColorFilter", data);

			obs_scene_t *scene = Utils::GetSceneFromNameOrCurrent(hook->scene);
			obs_sceneitem_t *item = Utils::GetSceneItemFromName(scene, hook->source);
			obs_source_t *source = obs_sceneitem_get_source(item);
			float i = 0;
			float tts = time_to_sleep((hook->int_override) ? (float)*hook->int_override : 500.0f);
			obs_source_filter_add(source, filter);
			obs_sceneitem_set_visible(item, true);

			while (i <= 100) {

				obs_data_set_double(data, "opacity", i);
				obs_source_update(filter, data);
				i = i + 0.05f;

				tsleep((int)tts);
			}

			obs_source_filter_remove(source, filter);

			obs_data_release(data);
		}};
		th.detach();
	} catch (std::exception &e) {
		blog(LOG_DEBUG, "Fade error %s", e.what());
	}
}

void fade_out_scene_item(MidiHook *hook)
{
	try {

		std::thread th{[=]() {
			obs_data_t *data = obs_data_create();
			obs_data_set_double(data, "opacity", 100);
			OBSSourceAutoRelease filter = obs_source_create_private("color_filter", "ColorFilter", data);

			obs_scene_t *scene = Utils::GetSceneFromNameOrCurrent(hook->scene);
			obs_sceneitem_t *item = Utils::GetSceneItemFromName(scene, hook->source);
			obs_source_t *source = obs_sceneitem_get_source(item);
			float i = 100;
			float tts = time_to_sleep((hook->int_override) ? (float)*hook->int_override : 500.0f);
			obs_source_filter_add(source, filter);
			while (i >= 0) {
				obs_data_set_double(data, "opacity", i);
				obs_source_update(filter, data);
				i = i - 0.05f;
				tsleep(tts);
			}
			obs_sceneitem_set_visible(item, false);

			obs_source_filter_remove(source, filter);

			obs_data_release(data);
		}};
		th.detach();
	} catch (std::exception &e) {
		blog(LOG_DEBUG, "Fade error %s", e.what());
	}
}
void make_opacity_filter::execute()
{
	obs_scene_t *scene = Utils::GetSceneFromNameOrCurrent(hook->scene);
	obs_sceneitem_t *item = Utils::GetSceneItemFromName(scene, hook->source);
	(obs_sceneitem_visible(item)) ? fade_out_scene_item(hook) : fade_in_scene_item(hook);
}

QGridLayout *MediaActions::set_widgets()
{
	cb_media_source = Utils::make_combo(Utils::GetMediaSourceNames());
	auto lay = new QGridLayout();
	lay->addWidget(Utils::make_label("Media Source"),0,0,1,1);
	lay->addWidget(cb_media_source,0,1,1,2);
	lay->setAlignment(Qt::AlignTop);
	return lay;
}

QGridLayout *SourceActions::set_widgets()
{
	cb_scene = Utils::make_combo(Utils::get_scene_names());
	cb_source = Utils::make_combo(Utils::get_source_names(cb_scene->currentText()));
	connect(cb_scene, &QComboBox::currentTextChanged, this, &SourceActions::onSceneTextChanged);
	auto lay = new QGridLayout();
	lay->addWidget(new QLabel("Scene"), 0, 0);
	lay->addWidget(cb_scene, 0, 1);
	lay->addWidget(new QLabel("Source"), 1, 0);
	lay->addWidget(cb_source, 1, 1);
	lay->setAlignment(Qt::AlignTop);
	return lay;
	
}
void SourceActions::onSceneTextChanged(QString _scene)
{
	cb_source->clear();
	cb_source->addItems(Utils::get_source_names(_scene));
}

QGridLayout *ItemActions::set_widgets()
{
	return nullptr;
}

QGridLayout *AudioActions::set_widgets()
{
	cb_source = Utils::make_combo(Utils::GetAudioSourceNames());
	auto lay = new QGridLayout();
	lay->addWidget(Utils::make_label("Audio Source"), 0, 0, 1, 1);
	lay->addWidget(cb_source, 0, 1, 1, 2);
	lay->setAlignment(Qt::AlignTop);
	return lay;
}
