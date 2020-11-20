/*
 * Copyright 2013 Google Inc. All Rights Reserved.
 * Copyright 2019 n-Track S.r.l. All Rights Reserved.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include "audio_module.h"

#include "internal/audio_module_internal.h"

void audioroute_configure(void *handle, audio_module_process_t process, initialize_processing_t initialize_processing, void *context, audioroute_engine **e) {
  audio_module_runner *amr = (audio_module_runner *) handle;
  amr->process = process;
  amr->initialize_processing=initialize_processing;
  amr->context = context;
  if(e) {
    *e = new audioroute_engine();
    (*e)->shm_ptr = amr->shm_ptr;
  }
}

void audioroute_configure(void *handle, audio_module_process_t process, initialize_processing_t initialize_processing, void *context)
{
    audioroute_configure(handle, process, initialize_processing, context, NULL);
}