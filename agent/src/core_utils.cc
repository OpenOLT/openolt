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

#include "core_utils.h"

std::string serial_number_to_str(bcmolt_serial_number* serial_number) {
#define SERIAL_NUMBER_SIZE 12
    char buff[SERIAL_NUMBER_SIZE+1];

    sprintf(buff, "%c%c%c%c%1X%1X%1X%1X%1X%1X%1X%1X",
            serial_number->vendor_id.arr[0],
            serial_number->vendor_id.arr[1],
            serial_number->vendor_id.arr[2],
            serial_number->vendor_id.arr[3],
            serial_number->vendor_specific.arr[0]>>4 & 0x0f,
            serial_number->vendor_specific.arr[0] & 0x0f,
            serial_number->vendor_specific.arr[1]>>4 & 0x0f,
            serial_number->vendor_specific.arr[1] & 0x0f,
            serial_number->vendor_specific.arr[2]>>4 & 0x0f,
            serial_number->vendor_specific.arr[2] & 0x0f,
            serial_number->vendor_specific.arr[3]>>4 & 0x0f,
            serial_number->vendor_specific.arr[3] & 0x0f);

    return buff;
}

std::string vendor_specific_to_str(char const * const vendor_specific) {
    char buff[SERIAL_NUMBER_SIZE+1];

    sprintf(buff, "%1X%1X%1X%1X%1X%1X%1X%1X",
            vendor_specific[0]>>4 & 0x0f,
            vendor_specific[0] & 0x0f,
            vendor_specific[1]>>4 & 0x0f,
            vendor_specific[1] & 0x0f,
            vendor_specific[2]>>4 & 0x0f,
            vendor_specific[2] & 0x0f,
            vendor_specific[3]>>4 & 0x0f,
            vendor_specific[3] & 0x0f);

    return buff;
}
/**
* Returns the default NNI (Upstream direction) or PON (Downstream direction) scheduler
* Every NNI port and PON port have default scheduler.
* The NNI0 default scheduler ID is 18432, and NNI1 is 18433 and so on.
* Similarly, PON0 default scheduler ID is 16384. PON1 is 16385 and so on.
*
* @param intf_id NNI or PON interface ID
* @param direction "upstream" or "downstream"
*
* @return default scheduler ID for the given interface.
*/

uint16_t get_dev_id(void) {
    return dev_id;
}

int get_default_tm_sched_id(int intf_id, std::string direction) {
    if (direction.compare(upstream) == 0) {
        return tm_upstream_sched_id_start + intf_id;
    } else if (direction.compare(downstream) == 0) {
        return tm_downstream_sched_id_start + intf_id;
    }
    else {
        OPENOLT_LOG(ERROR, openolt_log_id, "invalid direction - %s\n", direction.c_str());
        return 0;
    }
}

/**
* Gets a unique tm_sched_id for a given intf_id, onu_id, uni_id, gemport_id, direction
* The tm_sched_id is locally cached in a map, so that it can rendered when necessary.
* VOLTHA replays whole configuration on OLT reboot, so caching locally is not a problem.
* Note that tech_profile_id is used to differentiate service schedulers in downstream direction.
*
* @param intf_id NNI or PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param gemport_id GEM Port ID
* @param direction Upstream or downstream
* @param tech_profile_id Technology Profile ID
*
* @return tm_sched_id
*/
uint32_t get_tm_sched_id(int pon_intf_id, int onu_id, int uni_id, std::string direction, int tech_profile_id) {
    sched_map_key_tuple key(pon_intf_id, onu_id, uni_id, direction, tech_profile_id);
    int sched_id = -1;

    std::map<sched_map_key_tuple, int>::const_iterator it = sched_map.find(key);
    if (it != sched_map.end()) {
        sched_id = it->second;
    }
    if (sched_id != -1) {
        return sched_id;
    }

    bcmos_fastlock_lock(&data_lock);
    // Complexity of O(n). Is there better way that can avoid linear search?
    for (sched_id = 0; sched_id < MAX_TM_SCHED_ID; sched_id++) {
        if (tm_sched_bitset[sched_id] == 0) {
            tm_sched_bitset[sched_id] = 1;
            break;
        }
    }
    bcmos_fastlock_unlock(&data_lock, 0);

    if (sched_id < MAX_TM_SCHED_ID) {
        bcmos_fastlock_lock(&data_lock);
        sched_map[key] = sched_id;
        bcmos_fastlock_unlock(&data_lock, 0);
        return sched_id;
    } else {
        return -1;
    }
}

/**
* Free tm_sched_id for a given intf_id, onu_id, uni_id, gemport_id, direction, tech_profile_id
*
* @param intf_id NNI or PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param gemport_id GEM Port ID
* @param direction Upstream or downstream
* @param tech_profile_id Technology Profile ID
*/
void free_tm_sched_id(int pon_intf_id, int onu_id, int uni_id, std::string direction, int tech_profile_id) {
    sched_map_key_tuple key(pon_intf_id, onu_id, uni_id, direction, tech_profile_id);
    std::map<sched_map_key_tuple, int>::const_iterator it;
    bcmos_fastlock_lock(&data_lock);
    it = sched_map.find(key);
    if (it != sched_map.end()) {
        tm_sched_bitset[it->second] = 0;
        sched_map.erase(it);
    }
    bcmos_fastlock_unlock(&data_lock, 0);
}

bool is_tm_sched_id_present(int pon_intf_id, int onu_id, int uni_id, std::string direction, int tech_profile_id) {
    sched_map_key_tuple key(pon_intf_id, onu_id, uni_id, direction, tech_profile_id);
    std::map<sched_map_key_tuple, int>::const_iterator it = sched_map.find(key);
    if (it != sched_map.end()) {
        return true;
    }
    return false;
}

/**
* Check whether given two tm qmp profiles are equal or not
*
* @param tmq_map_profileA <vector> TM QUEUE MAPPING PROFILE
* @param tmq_map_profileB <vector> TM QUEUE MAPPING PROFILE
*
* @return boolean, true if given tmq_map_profiles are equal else false
*/

bool check_tm_qmp_equality(std::vector<uint32_t> tmq_map_profileA, std::vector<uint32_t> tmq_map_profileB) {
    for (uint32_t i = 0; i < TMQ_MAP_PROFILE_SIZE; i++) {
        if (tmq_map_profileA[i] != tmq_map_profileB[i]) {
            return false;
        }
    }
    return true;
}

/**
* Modifies given queues_pbit_map to parsable format
* e.g: Modifes "0b00000101" to "10100000"
*
* @param queues_pbit_map PBIT MAP configured for each GEM in TECH PROFILE
* @param size Queue count
*
* @return string queues_pbit_map
*/
std::string* get_valid_queues_pbit_map(std::string *queues_pbit_map, uint32_t size) {
    for(uint32_t i=0; i < size; i++) {
        /* Deletes 2 characters from index number 0 */
        queues_pbit_map[i].erase(0, 2);
        std::reverse(queues_pbit_map[i].begin(), queues_pbit_map[i].end());
    }
    return queues_pbit_map;
}

/**
* Creates TM QUEUE MAPPING PROFILE for given queues_pbit_map and queues_priority_q
*
* @param queues_pbit_map PBIT MAP configured for each GEM in TECH PROFILE
* @param queues_priority_q PRIORITY_Q configured for each GEM in TECH PROFILE
* @param size Queue count
*
* @return <vector> TM QUEUE MAPPING PROFILE
*/
std::vector<uint32_t> get_tmq_map_profile(std::string *queues_pbit_map, uint32_t *queues_priority_q, uint32_t size) {
    std::vector<uint32_t> tmq_map_profile(8,0);

    for(uint32_t i=0; i < size; i++) {
        for (uint32_t j = 0; j < queues_pbit_map[i].size(); j++) {
            if (queues_pbit_map[i][j]=='1') {
                tmq_map_profile.at(j) = queue_id_list[queues_priority_q[i]];
            }
        }
    }
    return tmq_map_profile;
}

/**
* Gets corresponding tm_qmp_id for a given tmq_map_profile
*
* @param <vector> TM QUEUE MAPPING PROFILE
*
* @return tm_qmp_id
*/
int get_tm_qmp_id(std::vector<uint32_t> tmq_map_profile) {
    int tm_qmp_id = -1;

    std::map<int, std::vector < uint32_t > >::const_iterator it = qmp_id_to_qmp_map.begin();
    while(it != qmp_id_to_qmp_map.end()) {
        if(check_tm_qmp_equality(tmq_map_profile, it->second)) {
            tm_qmp_id = it->first;
            break;
        }
        it++;
    }
    return tm_qmp_id;
}

/**
* Updates sched_qmp_id_map with given sched_id, pon_intf_id, onu_id, uni_id, tm_qmp_id
*
* @param upstream/downstream sched_id
* @param PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param tm_qmp_id TM QUEUE MAPPING PROFILE ID
*/
void update_sched_qmp_id_map(uint32_t sched_id,uint32_t pon_intf_id, uint32_t onu_id, \
                             uint32_t uni_id, int tm_qmp_id) {
   bcmos_fastlock_lock(&data_lock);
   sched_qmp_id_map_key_tuple key(sched_id, pon_intf_id, onu_id, uni_id);
   sched_qmp_id_map.insert(make_pair(key, tm_qmp_id));
   bcmos_fastlock_unlock(&data_lock, 0);
}

/**
* Gets corresponding tm_qmp_id for a given sched_id, pon_intf_id, onu_id, uni_id
*
* @param upstream/downstream sched_id
* @param PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
*
* @return tm_qmp_id
*/
int get_tm_qmp_id(uint32_t sched_id,uint32_t pon_intf_id, uint32_t onu_id, uint32_t uni_id) {
    sched_qmp_id_map_key_tuple key(sched_id, pon_intf_id, onu_id, uni_id);
    int tm_qmp_id = -1;

    std::map<sched_qmp_id_map_key_tuple, int>::const_iterator it = sched_qmp_id_map.find(key);
    if (it != sched_qmp_id_map.end()) {
        tm_qmp_id = it->second;
    }
    return tm_qmp_id;
}

/**
* Gets a unique tm_qmp_id for a given tmq_map_profile
* The tm_qmp_id is locally cached in a map, so that it can be rendered when necessary.
* VOLTHA replays whole configuration on OLT reboot, so caching locally is not a problem
*
* @param upstream/downstream sched_id
* @param PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param <vector> TM QUEUE MAPPING PROFILE
*
* @return tm_qmp_id
*/
int get_tm_qmp_id(uint32_t sched_id,uint32_t pon_intf_id, uint32_t onu_id, uint32_t uni_id, \
                  std::vector<uint32_t> tmq_map_profile) {
    int tm_qmp_id;

    bcmos_fastlock_lock(&data_lock);
    /* Complexity of O(n). Is there better way that can avoid linear search? */
    for (tm_qmp_id = 0; tm_qmp_id < MAX_TM_QMP_ID; tm_qmp_id++) {
        if (tm_qmp_bitset[tm_qmp_id] == 0) {
            tm_qmp_bitset[tm_qmp_id] = 1;
            break;
        }
    }
    bcmos_fastlock_unlock(&data_lock, 0);

    if (tm_qmp_id < MAX_TM_QMP_ID) {
        bcmos_fastlock_lock(&data_lock);
        qmp_id_to_qmp_map.insert(make_pair(tm_qmp_id, tmq_map_profile));
        bcmos_fastlock_unlock(&data_lock, 0);
        update_sched_qmp_id_map(sched_id, pon_intf_id, onu_id, uni_id, tm_qmp_id);
        return tm_qmp_id;
    } else {
        return -1;
    }
}

/**
* Free tm_qmp_id for a given sched_id, pon_intf_id, onu_id, uni_id
*
* @param upstream/downstream sched_id
* @param PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param tm_qmp_id TM QUEUE MAPPING PROFILE ID
*
* @return boolean, true if no more reference for TM QMP else false
*/
bool free_tm_qmp_id(uint32_t sched_id,uint32_t pon_intf_id, uint32_t onu_id, \
                    uint32_t uni_id, int tm_qmp_id) {
    bool result;
    sched_qmp_id_map_key_tuple key(sched_id, pon_intf_id, onu_id, uni_id);
    std::map<sched_qmp_id_map_key_tuple, int>::const_iterator it = sched_qmp_id_map.find(key);
    bcmos_fastlock_lock(&data_lock);
    if (it != sched_qmp_id_map.end()) {
        sched_qmp_id_map.erase(it);
    }
    bcmos_fastlock_unlock(&data_lock, 0);

    uint32_t tm_qmp_ref_count = 0;
    std::map<sched_qmp_id_map_key_tuple, int>::const_iterator it2 = sched_qmp_id_map.begin();
    while(it2 != sched_qmp_id_map.end()) {
        if(it2->second == tm_qmp_id) {
            tm_qmp_ref_count++;
        }
        it2++;
    }

    if (tm_qmp_ref_count == 0) {
        std::map<int, std::vector < uint32_t > >::const_iterator it3 = qmp_id_to_qmp_map.find(tm_qmp_id);
        if (it3 != qmp_id_to_qmp_map.end()) {
            bcmos_fastlock_lock(&data_lock);
            tm_qmp_bitset[tm_qmp_id] = 0;
            qmp_id_to_qmp_map.erase(it3);
            bcmos_fastlock_unlock(&data_lock, 0);
            OPENOLT_LOG(INFO, openolt_log_id, "Reference count for tm qmp profile id %d is : %d. So clearing it\n", \
                        tm_qmp_id, tm_qmp_ref_count);
            result = true;
        }
    } else {
        OPENOLT_LOG(INFO, openolt_log_id, "Reference count for tm qmp profile id %d is : %d. So not clearing it\n", \
                    tm_qmp_id, tm_qmp_ref_count);
        result = false;
    }
    return result;
}

/* ACL ID is a shared resource, caller of this function has to ensure atomicity using locks
   Gets free ACL ID if available, else -1. */
int get_acl_id() {
    int acl_id;

    /* Complexity of O(n). Is there better way that can avoid linear search? */
    for (acl_id = 0; acl_id < MAX_ACL_ID; acl_id++) {
        if (acl_id_bitset[acl_id] == 0) {
            acl_id_bitset[acl_id] = 1;
            break;
        }
    }
    if (acl_id < MAX_ACL_ID) {
        return acl_id ;
    } else {
        return -1;
    }
}

/* ACL ID is a shared resource, caller of this function has to ensure atomicity using locks
   Frees up the ACL ID. */
void free_acl_id (int acl_id) {
    if (acl_id < MAX_ACL_ID) {
        acl_id_bitset[acl_id] = 0;
    }
}

/**
* Returns qos type as string
*
* @param qos_type bcmolt_egress_qos_type enum
*/
std::string get_qos_type_as_string(bcmolt_egress_qos_type qos_type) {
    switch (qos_type)
    {
        case BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE: return "FIXED_QUEUE";
        case BCMOLT_EGRESS_QOS_TYPE_TC_TO_QUEUE: return "TC_TO_QUEUE";
        case BCMOLT_EGRESS_QOS_TYPE_PBIT_TO_TC: return "PBIT_TO_TC";
        case BCMOLT_EGRESS_QOS_TYPE_NONE: return "NONE";
        case BCMOLT_EGRESS_QOS_TYPE_PRIORITY_TO_QUEUE: return "PRIORITY_TO_QUEUE";
        default: OPENOLT_LOG(ERROR, openolt_log_id, "qos-type-not-supported %d\n", qos_type);
                 return "qos-type-not-supported";
    }
}

/**
* Gets/Updates qos type for given pon_intf_id, onu_id, uni_id
*
* @param PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param queue_size TrafficQueues Size
*
* @return qos_type
*/
bcmolt_egress_qos_type get_qos_type(uint32_t pon_intf_id, uint32_t onu_id, uint32_t uni_id, uint32_t queue_size) {
    qos_type_map_key_tuple key(pon_intf_id, onu_id, uni_id);
    bcmolt_egress_qos_type egress_qos_type = BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE;
    std::string qos_string;

    std::map<qos_type_map_key_tuple, bcmolt_egress_qos_type>::const_iterator it = qos_type_map.find(key);
    if (it != qos_type_map.end()) {
        egress_qos_type = it->second;
        qos_string = get_qos_type_as_string(egress_qos_type);
        OPENOLT_LOG(INFO, openolt_log_id, "Qos-type for subscriber connected to pon_intf_id %d, onu_id %d and uni_id %d is %s\n", \
                    pon_intf_id, onu_id, uni_id, qos_string.c_str());
    }
    else {
        /* QOS Type has been pre-defined as Fixed Queue but it will be updated based on number of GEMPORTS
           associated for a given subscriber. If GEM count = 1 for a given subscriber, qos_type will be Fixed Queue
           else Priority to Queue */
        egress_qos_type = (queue_size > 1) ? \
            BCMOLT_EGRESS_QOS_TYPE_PRIORITY_TO_QUEUE : BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE;
        bcmos_fastlock_lock(&data_lock);
        qos_type_map.insert(make_pair(key, egress_qos_type));
        bcmos_fastlock_unlock(&data_lock, 0);
        qos_string = get_qos_type_as_string(egress_qos_type);
        OPENOLT_LOG(INFO, openolt_log_id, "Qos-type for subscriber connected to pon_intf_id %d, onu_id %d and uni_id %d is %s\n", \
                    pon_intf_id, onu_id, uni_id, qos_string.c_str());
    }
    return egress_qos_type;
}

/**
* Clears qos type for given pon_intf_id, onu_id, uni_id
*
* @param PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
*/
void clear_qos_type(uint32_t pon_intf_id, uint32_t onu_id, uint32_t uni_id) {
    qos_type_map_key_tuple key(pon_intf_id, onu_id, uni_id);
    std::map<qos_type_map_key_tuple, bcmolt_egress_qos_type>::const_iterator it = qos_type_map.find(key);
    bcmos_fastlock_lock(&data_lock);
    if (it != qos_type_map.end()) {
        qos_type_map.erase(it);
        OPENOLT_LOG(INFO, openolt_log_id, "Cleared Qos-type for subscriber connected to pon_intf_id %d, onu_id %d and uni_id %d\n", \
                    pon_intf_id, onu_id, uni_id);
    }
    bcmos_fastlock_unlock(&data_lock, 0);
}

/**
* Returns Scheduler/Queue direction as string
*
* @param direction as specified in tech_profile.proto
*/
std::string GetDirection(int direction) {
    switch (direction)
    {
        case tech_profile::Direction::UPSTREAM: return upstream;
        case tech_profile::Direction::DOWNSTREAM: return downstream;
        default: OPENOLT_LOG(ERROR, openolt_log_id, "direction-not-supported %d\n", direction);
                 return "direction-not-supported";
    }
}

// This method handles waiting for AllocObject configuration.
// Returns error if the AllocObject is not in the appropriate state based on action requested.
bcmos_errno wait_for_alloc_action(uint32_t intf_id, uint32_t alloc_id, AllocCfgAction action) {
    Queue<alloc_cfg_complete_result> cfg_result;
    alloc_cfg_compltd_key k(intf_id, alloc_id);
    alloc_cfg_compltd_map[k] =  &cfg_result;
    bcmos_errno err = BCM_ERR_OK;

    // Try to pop the result from BAL with a timeout of ALLOC_CFG_COMPLETE_WAIT_TIMEOUT ms
    std::pair<alloc_cfg_complete_result, bool> result = cfg_result.pop(ALLOC_CFG_COMPLETE_WAIT_TIMEOUT);
    if (result.second == false) {
        OPENOLT_LOG(ERROR, openolt_log_id, "timeout waiting for alloc cfg complete indication intf_id %d, alloc_id %d\n",
                    intf_id, alloc_id);
        // Invalidate the queue pointer.
        bcmos_fastlock_lock(&alloc_cfg_wait_lock);
        alloc_cfg_compltd_map[k] = NULL;
        bcmos_fastlock_unlock(&alloc_cfg_wait_lock, 0);
        err = BCM_ERR_INTERNAL;
    }
    else if (result.first.status == ALLOC_CFG_STATUS_FAIL) {
        OPENOLT_LOG(ERROR, openolt_log_id, "error processing alloc cfg request intf_id %d, alloc_id %d\n",
                    intf_id, alloc_id);
        err = BCM_ERR_INTERNAL;
    }

    if (err == BCM_ERR_OK) {
        if (action == ALLOC_OBJECT_CREATE) {
            if (result.first.state != ALLOC_OBJECT_STATE_ACTIVE) {
                OPENOLT_LOG(ERROR, openolt_log_id, "alloc object not in active state intf_id %d, alloc_id %d alloc_obj_state %d\n",
                            intf_id, alloc_id, result.first.state);
               err = BCM_ERR_INTERNAL;
            } else {
                OPENOLT_LOG(INFO, openolt_log_id, "Create upstream bandwidth allocation success, intf_id %d, alloc_id %d\n",
                            intf_id, alloc_id);
            }
        } else { // ALLOC_OBJECT_DELETE
              if (result.first.state != ALLOC_OBJECT_STATE_NOT_CONFIGURED) {
                  OPENOLT_LOG(ERROR, openolt_log_id, "alloc object is not reset intf_id %d, alloc_id %d alloc_obj_state %d\n",
                              intf_id, alloc_id, result.first.state);
                  err = BCM_ERR_INTERNAL;
              } else {
                  OPENOLT_LOG(INFO, openolt_log_id, "Remove alloc object success, intf_id %d, alloc_id %d\n",
                              intf_id, alloc_id);
              }
        }
    }

    // Remove entry from map
    bcmos_fastlock_lock(&alloc_cfg_wait_lock);
    alloc_cfg_compltd_map.erase(k);
    bcmos_fastlock_unlock(&alloc_cfg_wait_lock, 0);
    return err;
}

// This method handles waiting for OnuDeactivate Completed Indication
bcmos_errno wait_for_onu_deactivate_complete(uint32_t intf_id, uint32_t onu_id) {
    Queue<onu_deactivate_complete_result> deact_result;
    onu_deact_compltd_key k(intf_id, onu_id);
    onu_deact_compltd_map[k] =  &deact_result;
    bcmos_errno err = BCM_ERR_OK;

    // Try to pop the result from BAL with a timeout of ONU_DEACTIVATE_COMPLETE_WAIT_TIMEOUT ms
    std::pair<onu_deactivate_complete_result, bool> result = deact_result.pop(ONU_DEACTIVATE_COMPLETE_WAIT_TIMEOUT);
    if (result.second == false) {
        OPENOLT_LOG(ERROR, openolt_log_id, "timeout waiting for onu deactivate complete indication intf_id %d, onu_id %d\n",
                    intf_id, onu_id);
        // Invalidate the queue pointer.
        bcmos_fastlock_lock(&onu_deactivate_wait_lock);
        onu_deact_compltd_map[k] = NULL;
        bcmos_fastlock_unlock(&onu_deactivate_wait_lock, 0);
        err = BCM_ERR_INTERNAL;
    }
    else if (result.first.result == BCMOLT_RESULT_FAIL) {
        OPENOLT_LOG(ERROR, openolt_log_id, "error processing onu deactivate request intf_id %d, onu_id %d, fail_reason %d\n",
                    intf_id, onu_id, result.first.reason);
        err = BCM_ERR_INTERNAL;
    } else if (result.first.result == BCMOLT_RESULT_SUCCESS) {
        OPENOLT_LOG(INFO, openolt_log_id, "success processing onu deactivate request intf_id %d, onu_id %d\n",
                    intf_id, onu_id);
    }

    // Remove entry from map
    bcmos_fastlock_lock(&onu_deactivate_wait_lock);
    onu_deact_compltd_map.erase(k);
    bcmos_fastlock_unlock(&onu_deactivate_wait_lock, 0);

    return err;
}

char* openolt_read_sysinfo(const char* field_name, char* field_val)
{
   FILE *fp;
   /* Prepare the command*/
   char command[150];

   snprintf(command, sizeof command, "bash -l -c \"onlpdump -s\" | perl -ne 'print $1 if /%s: (\\S+)/'", field_name);
   /* Open the command for reading. */
   fp = popen(command, "r");
   if (fp == NULL) {
       /*The client has to check for a Null field value in this case*/
       OPENOLT_LOG(INFO, openolt_log_id,  "Failed to query the %s\n", field_name);
       return field_val;
   }

   /*Read the field value*/
   if (fp) {
       uint8_t ret;
       ret = fread(field_val, OPENOLT_FIELD_LEN, 1, fp);
       if (ret >= OPENOLT_FIELD_LEN)
           OPENOLT_LOG(INFO, openolt_log_id,  "Read data length %u\n", ret);
       pclose(fp);
   }
   return field_val;
}

Status pushOltOperInd(uint32_t intf_id, const char *type, const char *state)
{
    openolt::Indication ind;
    openolt::IntfOperIndication* intf_oper_ind = new openolt::IntfOperIndication;

    intf_oper_ind->set_type(type);
    intf_oper_ind->set_intf_id(intf_id);
    intf_oper_ind->set_oper_state(state);
    ind.set_allocated_intf_oper_ind(intf_oper_ind);
    oltIndQ.push(ind);
    return Status::OK;
}

#define CLI_HOST_PROMPT_FORMAT "BCM.%u> "

/* Build CLI prompt */
void openolt_cli_get_prompt_cb(bcmcli_session *session, char *buf, uint32_t max_len)
{
    snprintf(buf, max_len, CLI_HOST_PROMPT_FORMAT, dev_id);
}

int _bal_apiend_cli_thread_handler(long data)
{
    char init_string[]="\n";
    bcmcli_session *sess = current_session;
    bcmos_task_parm bal_cli_task_p_dummy;

    /* Switch to interactive mode if not stopped in the init script */
    if (!bcmcli_is_stopped(sess)) {
        /* Force a CLI command prompt
         * The string passed into the parse function
         * must be modifiable, so a string constant like
         * bcmcli_parse(current_session, "\n") will not
         * work.
         */
        bcmcli_parse(sess, init_string);

        /* Process user input until EOF or quit command */
        bcmcli_driver(sess);
    }
    OPENOLT_LOG(INFO, openolt_log_id, "BAL API End CLI terminated\n");

    /* Cleanup */
    bcmcli_session_close(current_session);
    bcmcli_token_destroy(NULL);
    return 0;
}

/* Init API CLI commands for the current device */
bcmos_errno bcm_openolt_api_cli_init(bcmcli_entry *parent_dir, bcmcli_session *session)
{
    bcmos_errno rc;

    api_parent_dir = parent_dir;

    rc = bcm_api_cli_set_commands(session);

#ifdef BCM_SUBSYSTEM_HOST
    /* Subscribe for device change indication */
    rc = rc ? rc : bcmolt_olt_sel_ind_register(_api_cli_olt_change_ind);
#endif

    return rc;
}

bcmos_errno bcm_cli_quit(bcmcli_session *session, const bcmcli_cmd_parm parm[], uint16_t n_parms)
{
    bcmcli_stop(session);
    bcmcli_session_print(session, "CLI terminated by 'Quit' command\n");
    status_bcm_cli_quit = BCMOS_TRUE;

    return BCM_ERR_OK;
}

int get_status_bcm_cli_quit(void) {
     return status_bcm_cli_quit;
}

bcmos_errno bcmolt_apiend_cli_init() {
    bcmos_errno ret;
    bcmos_task_parm bal_cli_task_p = {};
    bcmos_task_parm bal_cli_task_p_dummy;

    /** before creating the task, check if it is already created by the other half of BAL i.e. Core side */
    if (BCM_ERR_OK != bcmos_task_query(&bal_cli_thread, &bal_cli_task_p_dummy)) {
        /* Create BAL CLI thread */
        bal_cli_task_p.name = bal_cli_thread_name;
        bal_cli_task_p.handler = _bal_apiend_cli_thread_handler;
        bal_cli_task_p.priority = TASK_PRIORITY_CLI;

        ret = bcmos_task_create(&bal_cli_thread, &bal_cli_task_p);
        if (BCM_ERR_OK != ret) {
            bcmos_printf("Couldn't create BAL API end CLI thread\n");
            return ret;
        }
    }
}

bcmos_errno get_onu_status(bcmolt_interface pon_ni, int onu_id, bcmolt_onu_state *onu_state) {
    bcmos_errno err;
    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    onu_key.pon_ni = pon_ni;
    onu_key.onu_id = onu_id;

    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    BCMOLT_FIELD_SET_PRESENT(&onu_cfg.data, onu_cfg_data, onu_state);
    BCMOLT_FIELD_SET_PRESENT(&onu_cfg.data, onu_cfg_data, itu);
    #ifdef TEST_MODE
    // It is impossible to mock the setting of onu_cfg.data.onu_state because
    // the actual bcmolt_cfg_get passes the address of onu_cfg.hdr and we cannot
    // set the onu_cfg.data.onu_state. So a new stub function is created and address
    // of onu_cfg is passed. This is one-of case where we need to add test specific
    // code in production code.
    err = bcmolt_cfg_get__onu_state_stub(dev_id, &onu_cfg);
    #else
    err = bcmolt_cfg_get(dev_id, &onu_cfg.hdr);
    #endif
    *onu_state = onu_cfg.data.onu_state;
    return err;
}

bcmos_errno get_pon_interface_status(bcmolt_interface pon_ni, bcmolt_interface_state *state, bcmolt_status *los_status) {
    bcmos_errno err;
    bcmolt_pon_interface_key pon_key;
    bcmolt_pon_interface_cfg pon_cfg;
    pon_key.pon_ni = pon_ni;

    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, pon_key);
    BCMOLT_FIELD_SET_PRESENT(&pon_cfg.data, pon_interface_cfg_data, state);
    BCMOLT_FIELD_SET_PRESENT(&pon_cfg.data, pon_interface_cfg_data, los_status);
    BCMOLT_FIELD_SET_PRESENT(&pon_cfg.data, pon_interface_cfg_data, itu);
    #ifdef TEST_MODE
    // It is impossible to mock the setting of pon_cfg.data.state because
    // the actual bcmolt_cfg_get passes the address of pon_cfg.hdr and we cannot
    // set the pon_cfg.data.state. So a new stub function is created and address
    // of pon_cfg is passed. This is one-of case where we need to add test specific
    // code in production code.
    err = bcmolt_cfg_get__pon_intf_stub(dev_id, &pon_cfg);
    #else
    err = bcmolt_cfg_get(dev_id, &pon_cfg.hdr);
    #endif
    *state = pon_cfg.data.state;
    *los_status = pon_cfg.data.los_status;
    return err;
}

/* Same as bcmolt_cfg_get but with added logic of retrying the API
   in case of some specific failures like timeout or object not yet ready
*/
bcmos_errno bcmolt_cfg_get_mult_retry(bcmolt_oltid olt, bcmolt_cfg *cfg) {
    bcmos_errno err;
    uint32_t current_try = 0;

    while (current_try < MAX_BAL_API_RETRY_COUNT) {
        err = bcmolt_cfg_get(olt, cfg);
        current_try++;

        if (err == BCM_ERR_STATE || err == BCM_ERR_TIMEOUT) {
            OPENOLT_LOG(WARNING, openolt_log_id, "bcmolt_cfg_get: err = %s\n", bcmos_strerror(err));
            bcmos_usleep(BAL_API_RETRY_TIME_IN_USECS);
            continue;
        }
        else {
           break;
        }
    }

    if (err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "bcmolt_cfg_get tried (%d) times with retry time(%d usecs) err = %s\n",
                           current_try,
                           BAL_API_RETRY_TIME_IN_USECS,
                           bcmos_strerror(err));
    }
    return err;
}


unsigned NumNniIf_() {return num_of_nni_ports;}
unsigned NumPonIf_() {return num_of_pon_ports;}

bcmos_errno get_nni_interface_status(bcmolt_interface id, bcmolt_interface_state *state) {
    bcmos_errno err;
    bcmolt_nni_interface_key nni_key;
    bcmolt_nni_interface_cfg nni_cfg;
    nni_key.id = id;

    BCMOLT_CFG_INIT(&nni_cfg, nni_interface, nni_key);
    BCMOLT_FIELD_SET_PRESENT(&nni_cfg.data, nni_interface_cfg_data, state);
    #ifdef TEST_MODE
    // It is impossible to mock the setting of nni_cfg.data.state because
    // the actual bcmolt_cfg_get passes the address of nni_cfg.hdr and we cannot
    // set the nni_cfg.data.state. So a new stub function is created and address
    // of nni_cfg is passed. This is one-of case where we need to add test specific
    // code in production code.
    err = bcmolt_cfg_get__nni_intf_stub(dev_id, &nni_cfg);
    #else
    err = bcmolt_cfg_get(dev_id, &nni_cfg.hdr);
    #endif
    *state = nni_cfg.data.state;
    return err;
}

Status install_gem_port(int32_t intf_id, int32_t onu_id, int32_t gemport_id) {
    bcmos_errno err;
    bcmolt_itupon_gem_cfg cfg; /* declare main API struct */
    bcmolt_itupon_gem_key key = {}; /* declare key */
    bcmolt_gem_port_configuration configuration = {};

    key.pon_ni = intf_id;
    key.gem_port_id = gemport_id;

    BCMOLT_CFG_INIT(&cfg, itupon_gem, key);

    bcmolt_gem_port_direction configuration_direction;
    configuration_direction = BCMOLT_GEM_PORT_DIRECTION_BIDIRECTIONAL;
    BCMOLT_FIELD_SET(&configuration, gem_port_configuration, direction, configuration_direction);

    bcmolt_gem_port_type configuration_type;
    configuration_type = BCMOLT_GEM_PORT_TYPE_UNICAST;
    BCMOLT_FIELD_SET(&configuration, gem_port_configuration, type, configuration_type);

    BCMOLT_FIELD_SET(&cfg.data, itupon_gem_cfg_data, configuration, configuration);

    BCMOLT_FIELD_SET(&cfg.data, itupon_gem_cfg_data, onu_id, onu_id);

    bcmolt_control_state encryption_mode;
    encryption_mode = BCMOLT_CONTROL_STATE_DISABLE;
    BCMOLT_FIELD_SET(&cfg.data, itupon_gem_cfg_data, encryption_mode, encryption_mode);

    bcmolt_us_gem_port_destination upstream_destination_queue;
    upstream_destination_queue = BCMOLT_US_GEM_PORT_DESTINATION_DATA;
    BCMOLT_FIELD_SET(&cfg.data, itupon_gem_cfg_data, upstream_destination_queue, upstream_destination_queue);

    bcmolt_control_state control;
    control = BCMOLT_CONTROL_STATE_ENABLE;
    BCMOLT_FIELD_SET(&cfg.data, itupon_gem_cfg_data, control, control);

    err = bcmolt_cfg_set(dev_id, &cfg.hdr);
    if(err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "failed to install gem_port = %d\n", gemport_id);
        return bcm_to_grpc_err(err, "Access_Control set ITU PON Gem port failed");
    }

    OPENOLT_LOG(INFO, openolt_log_id, "gem port installed successfully = %d\n", gemport_id);

    return Status::OK;
}

Status remove_gem_port(int32_t intf_id, int32_t gemport_id) {
    bcmolt_itupon_gem_cfg gem_cfg;
    bcmolt_itupon_gem_key key = {
        .pon_ni = (bcmolt_interface)intf_id,
        .gem_port_id = (bcmolt_gem_port_id)gemport_id
    };
    bcmos_errno err;

    BCMOLT_CFG_INIT(&gem_cfg, itupon_gem, key);
    err = bcmolt_cfg_clear(dev_id, &gem_cfg.hdr);
    if (err != BCM_ERR_OK)
    {
        OPENOLT_LOG(ERROR, openolt_log_id, "failed to remove gem_port = %d err=%s\n", gemport_id, gem_cfg.hdr.hdr.err_text);
        return bcm_to_grpc_err(err, "Access_Control clear ITU PON Gem port failed");
    }

    OPENOLT_LOG(INFO, openolt_log_id, "gem port removed successfully = %d\n", gemport_id);

    return Status::OK;
}

Status update_acl_interface(int32_t intf_id, bcmolt_interface_type intf_type, uint32_t access_control_id,
                bcmolt_members_update_command acl_cmd) {
    bcmos_errno err;
    bcmolt_access_control_interfaces_update oper; /* declare main API struct */
    bcmolt_access_control_key acl_key = {}; /* declare key */
    bcmolt_intf_ref interface_ref_list_elem = {};
    bcmolt_interface_type interface_ref_list_elem_intf_type;
    bcmolt_interface_id interface_ref_list_elem_intf_id;
    bcmolt_intf_ref_list_u8 interface_ref_list = {};

    if (acl_cmd != BCMOLT_MEMBERS_UPDATE_COMMAND_ADD && acl_cmd != BCMOLT_MEMBERS_UPDATE_COMMAND_REMOVE) {
        OPENOLT_LOG(ERROR, openolt_log_id, "acl cmd = %d not supported currently\n", acl_cmd);
        return bcm_to_grpc_err(BCM_ERR_PARM, "unsupported acl cmd");
    }
    interface_ref_list.arr = (bcmolt_intf_ref*)bcmos_calloc(sizeof(bcmolt_intf_ref)*1);

    if (interface_ref_list.arr == NULL)
         return bcm_to_grpc_err(BCM_ERR_PARM, "allocate interface_ref_list failed");
    OPENOLT_LOG(INFO, openolt_log_id, "update acl interface received for intf_id = %d, intf_type = %s, acl_id = %d, acl_cmd = %s\n",
            intf_id, intf_type == BCMOLT_INTERFACE_TYPE_PON? "pon": "nni", access_control_id,
            acl_cmd == BCMOLT_MEMBERS_UPDATE_COMMAND_ADD? "add": "remove");

    acl_key.id = access_control_id;

    /* Initialize the API struct. */
    BCMOLT_OPER_INIT(&oper, access_control, interfaces_update, acl_key);

    bcmolt_members_update_command command;
    command = acl_cmd;
    BCMOLT_FIELD_SET(&oper.data, access_control_interfaces_update_data, command, command);

    interface_ref_list_elem_intf_type = intf_type;
    BCMOLT_FIELD_SET(&interface_ref_list_elem, intf_ref, intf_type, interface_ref_list_elem_intf_type);

    interface_ref_list_elem_intf_id = intf_id;
    BCMOLT_FIELD_SET(&interface_ref_list_elem, intf_ref, intf_id, interface_ref_list_elem_intf_id);

    interface_ref_list.len = 1;
    BCMOLT_ARRAY_ELEM_SET(&interface_ref_list, 0, interface_ref_list_elem);

    BCMOLT_FIELD_SET(&oper.data, access_control_interfaces_update_data, interface_ref_list, interface_ref_list);

    err =  bcmolt_oper_submit(dev_id, &oper.hdr);
    if (err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "update acl interface fail for intf_id = %d, intf_type = %s, acl_id = %d, acl_cmd = %s\n",
            intf_id, intf_type == BCMOLT_INTERFACE_TYPE_PON? "pon": "nni", access_control_id,
            acl_cmd == BCMOLT_MEMBERS_UPDATE_COMMAND_ADD? "add": "remove");
        return bcm_to_grpc_err(err, "Access_Control submit interface failed");
    }

    bcmos_free(interface_ref_list.arr);
    OPENOLT_LOG(INFO, openolt_log_id, "update acl interface success for intf_id = %d, intf_type = %s, acl_id = %d, acl_cmd = %s\n",
            intf_id, intf_type == BCMOLT_INTERFACE_TYPE_PON? "pon": "nni", access_control_id,
            acl_cmd == BCMOLT_MEMBERS_UPDATE_COMMAND_ADD? "add": "remove");

    return Status::OK;
}

Status install_acl(const acl_classifier_key acl_key) {
    bcmos_errno err;
    bcmolt_access_control_cfg cfg;
    bcmolt_access_control_key key = { };
    bcmolt_classifier c_val = { };
    // hardcode the action for now.
    bcmolt_access_control_fwd_action_type action_type = BCMOLT_ACCESS_CONTROL_FWD_ACTION_TYPE_TRAP_TO_HOST;

    int acl_id = get_acl_id();
    if (acl_id < 0) {
        OPENOLT_LOG(ERROR, openolt_log_id, "exhausted acl_id for eth_type = %d, ip_proto = %d, src_port = %d, dst_port = %d\n",
                acl_key.ether_type, acl_key.ip_proto, acl_key.src_port, acl_key.dst_port);
        return bcm_to_grpc_err(BCM_ERR_INTERNAL, "exhausted acl id");
    }

    key.id = acl_id;
    /* config access control instance */
    BCMOLT_CFG_INIT(&cfg, access_control, key);
    if (acl_key.ether_type > 0) {
        OPENOLT_LOG(DEBUG, openolt_log_id, "Access_Control classify ether_type 0x%04x\n", acl_key.ether_type);
        BCMOLT_FIELD_SET(&c_val, classifier, ether_type, acl_key.ether_type);
    }

    if (acl_key.ip_proto > 0) {
        OPENOLT_LOG(DEBUG, openolt_log_id, "Access_Control classify ip_proto %d\n", acl_key.ip_proto);
        BCMOLT_FIELD_SET(&c_val, classifier, ip_proto, acl_key.ip_proto);
    }

    if (acl_key.dst_port > 0) {
        OPENOLT_LOG(DEBUG, openolt_log_id, "Access_Control classify dst_port %d\n", acl_key.dst_port);
        BCMOLT_FIELD_SET(&c_val, classifier, dst_port, acl_key.dst_port);
    }

    if (acl_key.src_port > 0) {
        OPENOLT_LOG(DEBUG, openolt_log_id, "Access_Control classify src_port %d\n", acl_key.src_port);
        BCMOLT_FIELD_SET(&c_val, classifier, src_port, acl_key.src_port);
    }

    BCMOLT_MSG_FIELD_SET(&cfg, classifier, c_val);
    BCMOLT_MSG_FIELD_SET(&cfg, priority, 10000);
    BCMOLT_MSG_FIELD_SET(&cfg, statistics_control, BCMOLT_CONTROL_STATE_ENABLE);

    BCMOLT_MSG_FIELD_SET(&cfg, forwarding_action.action, action_type);

    err = bcmolt_cfg_set(dev_id, &cfg.hdr);
    if (err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Access_Control set configuration failed, Error %d\n", err);
        // Free the acl_id
        free_acl_id(acl_id);
        return bcm_to_grpc_err(err, "Access_Control set configuration failed");
    }

    ACL_LOG(INFO, "ACL add ok", err);

    // Update the map that we have installed an acl for the given classfier.
    acl_classifier_to_acl_id_map[acl_key] = acl_id;
    return Status::OK;
}

Status remove_acl(int acl_id) {
    bcmos_errno err;
    bcmolt_access_control_cfg cfg; /* declare main API struct */
    bcmolt_access_control_key key = {}; /* declare key */

    key.id = acl_id;

    /* Initialize the API struct. */
    BCMOLT_CFG_INIT(&cfg, access_control, key);
    BCMOLT_FIELD_SET_PRESENT(&cfg.data, access_control_cfg_data, state);
    err = bcmolt_cfg_get(dev_id, &cfg.hdr);
    if (err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Access_Control get state failed\n");
        return bcm_to_grpc_err(err, "Access_Control get state failed");
    }

    if (cfg.data.state == BCMOLT_CONFIG_STATE_CONFIGURED) {
        key.id = acl_id;
        /* Initialize the API struct. */
        BCMOLT_CFG_INIT(&cfg, access_control, key);

        err = bcmolt_cfg_clear(dev_id, &cfg.hdr);
        if (err != BCM_ERR_OK) {
            // Should we free acl_id here ? We should ideally never land here..
            OPENOLT_LOG(ERROR, openolt_log_id, "Error %d while removing Access_Control rule ID %d\n",
                err, acl_id);
            return Status(grpc::StatusCode::INTERNAL, "Failed to remove Access_Control");
        }
    }

    // Free up acl_id
    free_acl_id(acl_id);

    OPENOLT_LOG(INFO, openolt_log_id, "acl removed successfully %d\n", acl_id);

    return Status::OK;
}

// Formulates ACL Classifier Key based on the following fields
// a. ether_type b. ip_proto c. src_port d. dst_port
// If any of the field is not available it is populated as -1.
void formulate_acl_classifier_key(acl_classifier_key *key, const ::openolt::Classifier& classifier) {

        // TODO: Is 0 a valid value for any of the following classifiers?
        // because in the that case, the 'if' check would fail and -1 would be filled as value.
        //
        if (classifier.eth_type()) {
            OPENOLT_LOG(DEBUG, openolt_log_id, "classify ether_type 0x%04x\n", classifier.eth_type());
            key->ether_type = classifier.eth_type();
        } else key->ether_type = -1;

        if (classifier.ip_proto()) {
            OPENOLT_LOG(DEBUG, openolt_log_id, "classify ip_proto %d\n", classifier.ip_proto());
            key->ip_proto = classifier.ip_proto();
        } else key->ip_proto = -1;


        if (classifier.src_port()) {
            OPENOLT_LOG(DEBUG, openolt_log_id, "classify src_port %d\n", classifier.src_port());
            key->src_port = classifier.src_port();
        } else key->src_port = -1;


        if (classifier.dst_port()) {
            OPENOLT_LOG(DEBUG, openolt_log_id, "classify dst_port %d\n", classifier.dst_port());
            key->dst_port = classifier.dst_port();
        } else key->dst_port = -1;
}

Status handle_acl_rule_install(int32_t onu_id, uint32_t flow_id,
                               const std::string flow_type, int32_t access_intf_id,
                               int32_t network_intf_id, int32_t gemport_id,
                               const ::openolt::Classifier& classifier) {
    int acl_id;
    int32_t intf_id = flow_type.compare(upstream) == 0? access_intf_id: network_intf_id;
    const std::string intf_type = flow_type.compare(upstream) == 0? "pon": "nni";
    bcmolt_interface_type olt_if_type = intf_type == "pon"? BCMOLT_INTERFACE_TYPE_PON: BCMOLT_INTERFACE_TYPE_NNI;

    Status resp;

    // few map keys we are going to use later.
    flow_id_flow_direction fl_id_fl_dir(flow_id, flow_type);
    gem_id_intf_id gem_intf(gemport_id, access_intf_id);
    acl_classifier_key acl_key;
    formulate_acl_classifier_key(&acl_key, classifier);
    const acl_classifier_key acl_key_const = {.ether_type=acl_key.ether_type, .ip_proto=acl_key.ip_proto,
        .src_port=acl_key.src_port, .dst_port=acl_key.dst_port};

    bcmos_fastlock_lock(&data_lock);

    // Check if the acl is already installed
    if (acl_classifier_to_acl_id_map.count(acl_key_const) > 0) {
        // retreive the acl_id
        acl_id = acl_classifier_to_acl_id_map[acl_key_const];
        acl_id_gem_id_intf_id ac_id_gm_id_if_id(acl_id, gemport_id, intf_id);
        if (flow_to_acl_map.count(fl_id_fl_dir)) {
            // coult happen if same trap flow is received again
            OPENOLT_LOG(INFO, openolt_log_id, "flow and related acl already handled, nothing more to do\n");
            bcmos_fastlock_unlock(&data_lock, 0);
            return Status::OK;
        }

        OPENOLT_LOG(INFO, openolt_log_id, "Acl for flow_id=%u with eth_type = %d, ip_proto = %d, src_port = %d, dst_port = %d already installed with acl id = %u\n",
                flow_id, acl_key.ether_type, acl_key.ip_proto, acl_key.src_port, acl_key.dst_port, acl_id);

        // The acl_ref_cnt is needed to know how many flows refer an ACL.
        // When the flow is removed, we decrement the reference count.
        // When the reference count becomes 0, we remove the ACL.
        if (acl_ref_cnt.count(acl_id) > 0) {
            acl_ref_cnt[acl_id] ++;
        } else {
            // We should ideally not land here. The acl_ref_cnt should have been
            // initialized the first time acl was installed.
            acl_ref_cnt[acl_id] = 1;
        }

    } else {
        resp = install_acl(acl_key_const);
        if (!resp.ok()) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Acl for flow_id=%u with eth_type = %d, ip_proto = %d, src_port = %d, dst_port = %d failed\n",
                    flow_id, acl_key_const.ether_type, acl_key_const.ip_proto, acl_key_const.src_port, acl_key_const.dst_port);
            bcmos_fastlock_unlock(&data_lock, 0);
            return resp;
        }

        acl_id = acl_classifier_to_acl_id_map[acl_key_const];

        // Initialize the acl reference count
        acl_ref_cnt[acl_id] = 1;

        OPENOLT_LOG(INFO, openolt_log_id, "acl add success for flow_id=%u with acl_id=%d\n", flow_id, acl_id);
    }

    // Register the interface for the given acl
    acl_id_intf_id_intf_type ac_id_inf_id_inf_type(acl_id, intf_id, intf_type);
    // This is needed to keep a track of which interface (pon/nni) has registered for an ACL.
    // If it is registered, how many flows refer to it.
    if (intf_acl_registration_ref_cnt.count(ac_id_inf_id_inf_type) > 0) {
        intf_acl_registration_ref_cnt[ac_id_inf_id_inf_type]++;
    } else {
        // The given interface is not registered for the ACL. We need to do it now.
        resp = update_acl_interface(intf_id, olt_if_type, acl_id, BCMOLT_MEMBERS_UPDATE_COMMAND_ADD);
        if (!resp.ok()){
            OPENOLT_LOG(ERROR, openolt_log_id, "failed to update acl interfaces intf_id=%d, intf_type=%s, acl_id=%d", intf_id, intf_type.c_str(), acl_id);
            // TODO: Ideally we should return error from hear and clean up other other stateful
            // counters we creaed earlier. Will leave it out for now.
        } 
        intf_acl_registration_ref_cnt[ac_id_inf_id_inf_type] = 1;
    }


    // Install the gem port if needed.
    if (gemport_id > 0 && access_intf_id >= 0) {
        if (gem_ref_cnt.count(gem_intf) > 0) {
            // The gem port is already installed
            // Increment the ref counter indicating number of flows referencing this gem port
            gem_ref_cnt[gem_intf]++;
            OPENOLT_LOG(DEBUG, openolt_log_id, "increment gem_ref_cnt in acl handler, ref_cnt=%d\n", gem_ref_cnt[gem_intf]);

        } else {
            // We should ideally never land here. The gem port should have been created the
            // first time ACL was installed.
            // Install the gem port
            Status resp = install_gem_port(access_intf_id, onu_id, gemport_id);
            if (!resp.ok()) {
                // TODO: We might need to reverse all previous data, but leave it out for now.
                OPENOLT_LOG(ERROR, openolt_log_id, "failed to install the gemport=%d for acl_id=%d, intf_id=%d\n", gemport_id, acl_id, access_intf_id);
                bcmos_fastlock_unlock(&data_lock, 0);
                return resp;
            }
            // Initialize the refence count for the gemport.
            gem_ref_cnt[gem_intf] = 1;
            OPENOLT_LOG(DEBUG, openolt_log_id, "intialized gem ref count in acl handler\n");
        }
    } else {
        OPENOLT_LOG(DEBUG, openolt_log_id, "not incrementing gem_ref_cnt in acl handler flow_id=%d, gemport_id=%d, intf_id=%d\n", flow_id, gemport_id, access_intf_id);
    }

    // Update the flow_to_acl_map
    // This info is needed during flow remove. We need to which ACL ID and GEM PORT ID
    // the flow was referring to.
    // After retrieving the ACL ID and GEM PORT ID, we decrement the corresponding
    // reference counters for those ACL ID and GEMPORT ID.
    acl_id_gem_id_intf_id ac_id_gm_id_if_id(acl_id, gemport_id, intf_id);
    flow_to_acl_map[fl_id_fl_dir] = ac_id_gm_id_if_id;

    bcmos_fastlock_unlock(&data_lock, 0);

    return Status::OK;
}

void clear_gem_port(int gemport_id, int access_intf_id) {
    gem_id_intf_id gem_intf(gemport_id, access_intf_id);
    if (gemport_id > 0 && access_intf_id >= 0 && gem_ref_cnt.count(gem_intf) > 0) {
        OPENOLT_LOG(DEBUG, openolt_log_id, "decrementing gem_ref_cnt gemport_id=%d access_intf_id=%d\n", gemport_id, access_intf_id);
        gem_ref_cnt[gem_intf]--;
        if (gem_ref_cnt[gem_intf] == 0) {
            // For datapath flow this may not be necessary (to be verified)
            remove_gem_port(access_intf_id, gemport_id);
            gem_ref_cnt.erase(gem_intf);
            OPENOLT_LOG(DEBUG, openolt_log_id, "removing gem_ref_cnt entry gemport_id=%d access_intf_id=%d\n", gemport_id, access_intf_id);
        } else {
            OPENOLT_LOG(DEBUG, openolt_log_id, "gem_ref_cnt  not zero yet gemport_id=%d access_intf_id=%d\n", gemport_id, access_intf_id);
        }
    } else {
        OPENOLT_LOG(DEBUG, openolt_log_id, "not decrementing gem_ref_cnt gemport_id=%d access_intf_id=%d\n", gemport_id, access_intf_id);
    }
}

Status handle_acl_rule_cleanup(int16_t acl_id, int32_t gemport_id, int32_t intf_id, const std::string flow_type) {
    const std::string intf_type= flow_type.compare(upstream) == 0 ? "pon": "nni";
    acl_id_intf_id_intf_type ac_id_inf_id_inf_type(acl_id, intf_id, intf_type);
    intf_acl_registration_ref_cnt[ac_id_inf_id_inf_type]--;
    if (intf_acl_registration_ref_cnt[ac_id_inf_id_inf_type] == 0) {
        bcmolt_interface_type olt_if_type = intf_type == "pon"? BCMOLT_INTERFACE_TYPE_PON: BCMOLT_INTERFACE_TYPE_NNI;
        Status resp = update_acl_interface(intf_id, olt_if_type, acl_id, BCMOLT_MEMBERS_UPDATE_COMMAND_REMOVE);
        if (!resp.ok()){
            OPENOLT_LOG(ERROR, openolt_log_id, "failed to update acl interfaces intf_id=%d, intf_type=%s, acl_id=%d", intf_id, intf_type.c_str(), acl_id);
        }
        intf_acl_registration_ref_cnt.erase(ac_id_inf_id_inf_type);
    }

    acl_ref_cnt[acl_id]--;
    if (acl_ref_cnt[acl_id] == 0) {
        remove_acl(acl_id);
        acl_ref_cnt.erase(acl_id);
        // Iterate acl_classifier_to_acl_id_map and delete classifier the key corresponding to acl_id
        std::map<acl_classifier_key, uint16_t>::iterator it;
        for (it=acl_classifier_to_acl_id_map.begin(); it!=acl_classifier_to_acl_id_map.end(); ++it)  {
            if (it->second == acl_id) {
                OPENOLT_LOG(INFO, openolt_log_id, "cleared classifier key corresponding to acl_id = %d\n", acl_id);
                acl_classifier_to_acl_id_map.erase(it->first);
                break;
            }
        }
    }

    clear_gem_port(gemport_id, intf_id);

    return Status::OK;
}

Status check_bal_ready() {
    bcmos_errno err;
    int maxTrials = 30;
    bcmolt_olt_cfg olt_cfg = { };
    bcmolt_olt_key olt_key = { };

    BCMOLT_CFG_INIT(&olt_cfg, olt, olt_key);
    BCMOLT_MSG_FIELD_GET(&olt_cfg, bal_state);

    while (olt_cfg.data.bal_state != BCMOLT_BAL_STATE_BAL_AND_SWITCH_READY) {
        if (--maxTrials == 0)
            return grpc::Status(grpc::StatusCode::UNAVAILABLE, "check bal ready failed");
        sleep(5);
        #ifdef TEST_MODE
        // It is impossible to mock the setting of olt_cfg.data.bal_state because
        // the actual bcmolt_cfg_get passes the address of olt_cfg.hdr and we cannot
        // set the olt_cfg.data.bal_state. So a new stub function is created and address
        // of olt_cfg is passed. This is one-of case where we need to add test specific
        // code in production code.
        if (bcmolt_cfg_get__bal_state_stub(dev_id, &olt_cfg)) {
        #else
        if (bcmolt_cfg_get(dev_id, &olt_cfg.hdr)) {
        #endif
            continue;
        }
        else
            OPENOLT_LOG(INFO, openolt_log_id, "waiting for BAL ready ...\n");
    }

    OPENOLT_LOG(INFO, openolt_log_id, "BAL is ready\n");
    return Status::OK;
}

Status check_connection() {
    int maxTrials = 60;
    while (!bcmolt_api_conn_mgr_is_connected(dev_id)) {
        sleep(1);
        if (--maxTrials == 0)
            return grpc::Status(grpc::StatusCode::UNAVAILABLE, "check connection failed");
        else
            OPENOLT_LOG(INFO, openolt_log_id, "waiting for daemon connection ...\n");
    }
    OPENOLT_LOG(INFO, openolt_log_id, "daemon is connected\n");
    return Status::OK;
}

std::string get_ip_address(const char* nw_intf){
    std::string ipAddress = "0.0.0.0";
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *temp_addr = NULL;
    int success = 0;
   /* retrieve the current interfaces - returns 0 on success */
    success = getifaddrs(&interfaces);
    if (success == 0) {
        /* Loop through linked list of interfaces */
        temp_addr = interfaces;
        while(temp_addr != NULL) {
            if(temp_addr->ifa_addr->sa_family == AF_INET) {
                /* Check if interface given present in OLT, if yes return its IP Address */
                if(strcmp(temp_addr->ifa_name, nw_intf) == 0){
                    ipAddress=inet_ntoa(((struct sockaddr_in*)temp_addr->ifa_addr)->sin_addr);
                    break;
                }
            }
            temp_addr = temp_addr->ifa_next;
        }
    }
    /* Free memory */
    freeifaddrs(interfaces);
    return ipAddress;
}

bcmos_errno getOnuMaxLogicalDistance(uint32_t intf_id, uint32_t *mld) {
    bcmos_errno err = BCM_ERR_OK;
    bcmolt_pon_distance pon_distance = {};
    bcmolt_pon_interface_cfg pon_cfg; /* declare main API struct */
    bcmolt_pon_interface_key key = {}; /* declare key */

    key.pon_ni = intf_id;

    if (!state.is_activated()) {
        OPENOLT_LOG(ERROR, openolt_log_id, "ONU maximum logical distance is not available since OLT is not activated yet\n");
        return BCM_ERR_STATE;
    }

    /* Initialize the API struct. */
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, key);
        BCMOLT_FIELD_SET_PRESENT(&pon_distance, pon_distance, max_log_distance);
    BCMOLT_FIELD_SET(&pon_cfg.data, pon_interface_cfg_data, pon_distance, pon_distance);
    #ifdef TEST_MODE
    // It is impossible to mock the setting of pon_cfg.data.state because
    // the actual bcmolt_cfg_get passes the address of pon_cfg.hdr and we cannot
    // set the pon_cfg.data.state. So a new stub function is created and address
    // of pon_cfg is passed. This is one-of case where we need to add test specific
    // code in production code.
    err = bcmolt_cfg_get__pon_intf_stub(dev_id, &pon_cfg);
    #else
    err = bcmolt_cfg_get(dev_id, &pon_cfg.hdr);
    #endif
        if (err != BCM_ERR_OK) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to retrieve ONU maximum logical distance for PON %d, err = %s (%d)\n", intf_id, bcmos_strerror(err), err);
            return err;
        }
        *mld = pon_distance.max_log_distance;

    return BCM_ERR_OK;
}

/**
* Gets mac address based on interface name.
*
* @param intf_name interface name
* @param mac_address  mac address field
* @param max_size_of_mac_address max sixe of the mac_address
* @return mac_address value in case of success or return NULL in case of failure.
*/

char* get_intf_mac(const char* intf_name, char* mac_address,  unsigned int max_size_of_mac_address){
    int fd;
    struct ifreq ifr;
    char *mac;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if ( fd == -1) {
        OPENOLT_LOG(ERROR, openolt_log_id, "failed to get mac, could not create file descriptor");
        return NULL;
    }

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy((char *)ifr.ifr_name , (const char *)intf_name , IFNAMSIZ-1);
    if( ioctl(fd, SIOCGIFHWADDR, &ifr) == -1)
    {
        OPENOLT_LOG(ERROR, openolt_log_id, "failed to get mac, ioctl failed and returned err");
        close(fd);
        return NULL;
    }

    close(fd);
    mac = (char *)ifr.ifr_hwaddr.sa_data;

    // formatted mac address
    snprintf(mac_address, max_size_of_mac_address, (const char *)"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", (unsigned char)mac[0], (unsigned char)mac[1], (unsigned char)mac[2], (unsigned char)mac[3], (unsigned char)mac[4], (unsigned char)mac[5]);

    return mac_address;
}
