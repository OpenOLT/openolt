/*
 * Copyright 2018-present Open Networking Foundation

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>

#include "Queue.h"
#include <iostream>
#include <sstream>

#include "core.h"
#include "state.h"

State state;

void* RunSim(void *) {

    state.activate();

    while (!state.is_connected()) {
        sleep(5);
    }

    // Send Olt up indication
    {
        openolt::Indication ind;
        openolt::OltIndication* olt_ind = new openolt::OltIndication;
        olt_ind->set_oper_state("up");
        ind.set_allocated_olt_ind(olt_ind);
        std::cout << "olt indication, oper_state:" << ind.olt_ind().oper_state() << std::endl;
        oltIndQ.push(ind);
    }

    // TODO - Add interface and onu indication events
}

Status Enable_(int argc, char *argv[]) {
    pthread_t simThread;

    pthread_create(&simThread, NULL, RunSim, NULL);
    return Status::OK;
}

Status Disable_() {
    return Status::OK;
}

Status Reenable_() {
    return Status::OK;
}

Status EnablePonIf_(uint32_t intf_id) {
    return Status::OK;
}

Status DisableUplinkIf_(uint32_t intf_id) {
    return Status::OK;
}

Status EnableUplinkIf_(uint32_t intf_id) {
    return Status::OK;
}

Status DisablePonIf_(uint32_t intf_id) {
    return Status::OK;
}

Status ActivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific, uint32_t pir) {
    return Status::OK;
}

Status DeactivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific) {
    return Status::OK;
}

Status DeleteOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific) {
    return Status::OK;;
}

Status OmciMsgOut_(uint32_t intf_id, uint32_t onu_id, const std::string pkt) {
    return Status::OK;
}

Status OnuPacketOut_(uint32_t intf_id, uint32_t onu_id, uint32_t port_no, const std::string pkt) {
    return Status::OK;
}

Status UplinkPacketOut_(uint32_t intf_id, const std::string pkt) {
    return Status::OK;
}

Status FlowAdd_(int32_t access_intf_id, int32_t onu_id, int32_t uni_id, uint32_t port_no,
                uint32_t flow_id, const std::string flow_type,
                int32_t alloc_id, int32_t network_intf_id,
                int32_t gemport_id, const ::openolt::Classifier& classifier,
                const ::openolt::Action& action, int32_t priority_value, uint64_t cookie) {
    return Status::OK;
}

Status SchedAdd_(int intf_id, int onu_id, int agg_port_id) {
    return Status::OK;
}

Status SchedRemove_(int intf_id, int onu_id, int agg_port_id) {
    return Status::OK;
}

Status FlowRemove_(uint32_t flow_id, const std::string flow_type) {
    return Status::OK;
}

void stats_collection() {
}

Status GetDeviceInfo_(openolt::DeviceInfo* deviceInfo) {
    return Status::OK;
}
