/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Clipboard Virtual Channel Unit Tests
 *
 * Copyright 2011 Vic Lee
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/chanman.h>
#include <freerdp/utils/event.h>
#include <freerdp/utils/hexdump.h>

#include "test_cliprdr.h"

int init_cliprdr_suite(void)
{
	freerdp_chanman_global_init();
	return 0;
}

int clean_cliprdr_suite(void)
{
	freerdp_chanman_global_uninit();
	return 0;
}

int add_cliprdr_suite(void)
{
	add_test_suite(cliprdr);

	add_test_function(cliprdr);

	return 0;
}

static const uint8 test_clip_caps_data[] =
{
	"\x07\x00\x00\x00\x10\x00\x00\x00\x01\x00\x00\x00\x01\x00\x0C\x00"
	"\x02\x00\x00\x00\x0E\x00\x00\x00"
};

static const uint8 test_monitor_ready_data[] =
{
	"\x01\x00\x00\x00\x00\x00\x00\x00"
};

static const uint8 test_format_list_data[] =
{
	"\x02\x00\x00\x00\x48\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\xd0\x00\x00"
	"\x48\x00\x54\x00\x4D\x00\x4C\x00\x20\x00\x46\x00\x6F\x00\x72\x00"
	"\x6D\x00\x61\x00\x74\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
};

static const uint8 test_format_list_response_data[] =
{
	"\x03\x00\x01\x00\x00\x00\x00\x00"
};

static int test_rdp_channel_data(rdpInst* inst, int chan_id, char* data, int data_size)
{
	printf("chan_id %d data_size %d\n", chan_id, data_size);
	/*freerdp_hexdump(data, data_size);*/
}

static int event_processed;

static void event_process_callback(FRDP_EVENT* event)
{
	printf("Event %d processed.\n", event->event_type);
	event_processed = 1;
}

void test_cliprdr(void)
{
	rdpChanMan* chan_man;
	rdpSettings settings = { 0 };
	rdpInst inst = { 0 };
	FRDP_EVENT* event;
	FRDP_CB_FORMAT_LIST_EVENT* format_list_event;
	int i;

	settings.hostname = "testhost";
	inst.settings = &settings;
	inst.rdp_channel_data = test_rdp_channel_data;

	chan_man = freerdp_chanman_new();

	freerdp_chanman_load_plugin(chan_man, &settings, "../channels/cliprdr/cliprdr.so", NULL);
	freerdp_chanman_pre_connect(chan_man, &inst);
	freerdp_chanman_post_connect(chan_man, &inst);

	/* server sends cliprdr capabilities and monitor ready PDU */
	freerdp_chanman_data(&inst, 0, (char*)test_clip_caps_data, sizeof(test_clip_caps_data) - 1,
		CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(test_clip_caps_data) - 1);

	freerdp_chanman_data(&inst, 0, (char*)test_monitor_ready_data, sizeof(test_monitor_ready_data) - 1,
		CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(test_monitor_ready_data) - 1);

	/* cliprdr sends clipboard_sync event to UI */
	while ((event = freerdp_chanman_pop_event(chan_man)) == NULL)
	{
		freerdp_chanman_check_fds(chan_man, &inst);
	}
	printf("Got event %d\n", event->event_type);
	CU_ASSERT(event->event_type == FRDP_EVENT_TYPE_CB_SYNC);
	freerdp_event_free(event);

	/* UI sends format_list event to cliprdr */
	event = freerdp_event_new(FRDP_EVENT_TYPE_CB_FORMAT_LIST, event_process_callback, NULL);
	format_list_event = (FRDP_CB_FORMAT_LIST_EVENT*)event;
	format_list_event->num_formats = 2;
	format_list_event->formats = (uint32*)xmalloc(sizeof(uint32) * 2);
	format_list_event->formats[0] = CB_FORMAT_TEXT;
	format_list_event->formats[1] = CB_FORMAT_HTML;
	event_processed = 0;
	freerdp_chanman_send_event(chan_man, "cliprdr", event);

	/* cliprdr sends format list PDU to server */
	while (!event_processed)
	{
		freerdp_chanman_check_fds(chan_man, &inst);
	}

	/* server sends format list response PDU to cliprdr */
	freerdp_chanman_data(&inst, 0, (char*)test_format_list_response_data, sizeof(test_format_list_response_data) - 1,
		CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(test_format_list_response_data) - 1);

	/* server sends format list PDU to cliprdr */
	freerdp_chanman_data(&inst, 0, (char*)test_format_list_data, sizeof(test_format_list_data) - 1,
		CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(test_format_list_data) - 1);

	/* cliprdr sends format_list event to UI */
	while ((event = freerdp_chanman_pop_event(chan_man)) == NULL)
	{
		freerdp_chanman_check_fds(chan_man, &inst);
	}
	printf("Got event %d\n", event->event_type);
	CU_ASSERT(event->event_type == FRDP_EVENT_TYPE_CB_FORMAT_LIST);
	if (event->event_type == FRDP_EVENT_TYPE_CB_FORMAT_LIST)
	{
		format_list_event = (FRDP_CB_FORMAT_LIST_EVENT*)event;
		for (i = 0; i < format_list_event->num_formats; i++)
			printf("Format: 0x%X\n", format_list_event->formats[i]);
	}
	freerdp_event_free(event);

	freerdp_chanman_close(chan_man, &inst);
	freerdp_chanman_free(chan_man);
}
