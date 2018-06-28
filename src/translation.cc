#include "translation.h"

int interface_key_to_port_no(bcmbal_interface_key key) {
    if (key.intf_type == BCMBAL_INTF_TYPE_NNI) {
        return 128 + key.intf_id;
    }
    if (key.intf_type == BCMBAL_INTF_TYPE_PON) {
        return (0x2 << 28) + 1;
    }
    return key.intf_id;
}

std::string alarm_status_to_string(bcmbal_alarm_status status) {
    switch (status) {
        case BCMBAL_ALARM_STATUS_OFF:
            return "off";
        case BCMBAL_ALARM_STATUS_ON:
            return "on";
        case BCMBAL_ALARM_STATUS_NO__CHANGE:
            return "no_change";
    }
    return "unknown";
}
