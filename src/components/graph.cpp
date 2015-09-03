// Copyright 2015 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/graph.h"

#include <cstdio>
#include "components/services.h"
#include "event/graph.h"

FPL_ENTITY_DEFINE_COMPONENT(fpl::fpl_project::GraphComponent,
                            fpl::fpl_project::GraphData)

namespace fpl {
namespace fpl_project {

void GraphComponent::RegisterListener(int event_id, entity::EntityRef* entity,
                                      event::NodeEventListener* listener) {
  auto graph_data = Data<GraphData>(*entity);
  if (!graph_data) {
    graph_data = AddEntity(*entity);
  }
  graph_data->broadcaster.RegisterListener(event_id, listener);
}

void GraphComponent::Init() {
  ServicesComponent* services =
      entity_manager_->GetComponent<ServicesComponent>();
  event_system_ = services->event_system();
  graph_dictionary_ = services->graph_dictionary();
}

void GraphComponent::AddFromRawData(entity::EntityRef& entity,
                                    const void* raw_data) {
  GraphData* graph_data = AddEntity(entity);
  auto graph_def = static_cast<const GraphDef*>(raw_data);
  auto filename_list = graph_def->filename_list();
  if (filename_list) {
    for (size_t i = 0; i < filename_list->size(); ++i) {
      auto filename = filename_list->Get(i);
      graph_data->graphs.push_back(SerializableGraphState());
      graph_data->graphs.back().filename = filename->c_str();
    }
  }
}

void GraphComponent::PostLoadFixup() {
  auto services_component = entity_manager_->GetComponent<ServicesComponent>();
  for (auto iter = component_data_.begin(); iter != component_data_.end();
       ++iter) {
    entity::EntityRef& entity = iter->entity;
    GraphData* graph_data = Data<GraphData>(entity);
    for (auto graph_iter = graph_data->graphs.begin();
         graph_iter != graph_data->graphs.end(); ++graph_iter) {
      event::Graph* graph = LoadGraph(event_system_, graph_dictionary_,
                                      graph_iter->filename.c_str(),
                                      services_component->audio_engine());
      graph_iter->graph_state.Initialize(graph);
    }
  }
}

entity::ComponentInterface::RawDataUniquePtr GraphComponent::ExportRawData(
    const entity::EntityRef& entity) const {
  const GraphData* data = GetComponentData(entity);
  if (data == nullptr) return nullptr;

  flatbuffers::FlatBufferBuilder fbb;
  std::vector<flatbuffers::Offset<flatbuffers::String>> filenames_vector;
  for (auto iter = data->graphs.begin(); iter != data->graphs.end(); ++iter) {
    filenames_vector.push_back(fbb.CreateString(iter->filename));
  }
  GraphDefBuilder builder(fbb);
  if (filenames_vector.size() > 0) {
    builder.add_filename_list(fbb.CreateVector(filenames_vector));
  }
  fbb.Finish(builder.Finish());
  return fbb.ReleaseBufferPointer();
}

void GraphComponent::UpdateAllEntities(entity::WorldTime /*delta_time*/) {
  for (auto iter = component_data_.begin(); iter != component_data_.end();
       ++iter) {
    entity::EntityRef& entity = iter->entity;
    GraphData* graph_data = Data<GraphData>(entity);
    for (auto graph_iter = graph_data->graphs.begin();
         graph_iter != graph_data->graphs.end(); ++graph_iter) {
      graph_iter->graph_state.Execute();
    }
  }
}

}  // fpl_project
}  // fpl
