/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "android/sdk-controller-socket.h"
#include "android/sensors-port.h"
#include "android/hw-sensors.h"
#include "android/utils/debug.h"

#define  E(...)    derror(__VA_ARGS__)
#define  W(...)    dwarning(__VA_ARGS__)
#define  D(...)    VERBOSE_PRINT(sensors_port,__VA_ARGS__)
#define  D_ACTIVE  VERBOSE_CHECK(sensors_port)

#define TRACE_ON    1

#if TRACE_ON
#define  T(...)    VERBOSE_PRINT(sensors_port,__VA_ARGS__)
#else
#define  T(...)
#endif

/* Timeout (millisec) to use when communicating with SDK controller. */
#define SDKCTL_SENSORS_TIMEOUT      3000

/*
 * Queries sent to sensors port of the SDK controller.
 */

/* Queries the port for list of available sensors. */
#define SDKCTL_SENSORS_QUERY_LIST   1

/*
 * Messages sent between the emuator, and  sensors port of the SDK controller.
 */

/* Starts sensor emulation. */
#define SDKCTL_SENSORS_START            1
/* Stops sensor emulation. */
#define SENSOR_SENSORS_STOP             2
/* Enables emulation for a sensor. */
#define SDKCTL_SENSORS_ENABLE           3
/* Disables emulation for a sensor. */
#define SDKCTL_SENSORS_DISABLE          4
/* This message delivers sensor values. */
#define SDKCTL_SENSORS_SENSOR_EVENT     5


/* Describes a sensor on the device.
 * When SDK controller sensors port replies to a "list" query, it replies with
 * a flat buffer containing entries of this type following each other. End of
 * each entry is a zero-terminator for its 'sensor_name' field. The end of the
 * entire list is marked with an entry, containing -1 at its 'sensor_id' field.
 */
typedef struct SensorEntry {
    /* Identifies sensor on the device. Value -1 indicates list terminator,
     * rather than a valid sensor descriptor. */
    int     sensor_id;
    /* Beginning of zero-terminated sensor name. */
    char    sensor_name[1];
} SensorEntry;

/* Describes a sensor in the array of emulated sensors. */
typedef struct SensorDescriptor {
    /* Identifies sensor on the device. */
    int         sensor_id;
    /* Identifies sensor in emulator. */
    int         emulator_id;
    /* Sensor name. */
    char*       sensor_name;
} SensorDescriptor;

/* Sensor event message descriptor.
 * Entries of this type are sent along with SDKCTL_SENSORS_SENSOR_EVENT message
 */
typedef struct SensorEvent {
    /* Identifies a device sensor for which values have been delivered. */
    int     sensor_id;
    /* Sensor values. */
    float   fvalues[3];
} SensorEvent;

/* Sensors port descriptor. */
struct AndroidSensorsPort {
    /* Caller identifier. */
    void*               opaque;
    /* Communication socket. */
    SDKCtlSocket*       sdkctl;
    /* Lists sensors available for emulation. */
    SensorDescriptor**  sensors;
    /* Number of sensors in 'sensors' list. */
    int                 sensors_count;
};

/********************************************************************************
 *                          Sensors port internals
 *******************************************************************************/

/* Checks if sensor descriptor is the terminator.
 * Return:
 *  Boolean, 1 if it is a terminator, 0 if it is not.
 */
static int
_sensor_entry_is_terminator(const SensorEntry* entry)
{
    return entry == NULL || entry->sensor_id == -1;
}

/* Gets next sensor descriptor.
 * Return:
 *  Next sensor desciptor, or NULL if there are no more descriptors in the list.
 */
static const SensorEntry*
_sensor_entry_next(const SensorEntry* entry)
{
    if (!_sensor_entry_is_terminator(entry)) {
        /* Next descriptor begins right after zero-terminator for the sensor_name
         * field of this descriptor. */
        entry = (const SensorEntry*)(entry->sensor_name + strlen(entry->sensor_name) + 1);
        if (!_sensor_entry_is_terminator(entry)) {
            return entry;
        }
    }
    return NULL;
}

/* Gets number of entries in the list. */
static int
_sensor_entry_list_size(const SensorEntry* entry) {
    int ret = 0;
    while (!_sensor_entry_is_terminator(entry)) {
        ret++;
        entry = _sensor_entry_next(entry);
    }
    return ret;
}

/* Discards sensors saved in AndroidSensorsPort's array. */
static void
_sensors_port_discard_sensors(AndroidSensorsPort* asp)
{
    if (asp->sensors != NULL) {
        int n;
        for (n = 0; n < asp->sensors_count; n++) {
            if (asp->sensors[n] != NULL) {
                free(asp->sensors[n]->sensor_name);
                AFREE(asp->sensors[n]);
            }
        }
        free(asp->sensors);
        asp->sensors = NULL;
    }
    asp->sensors_count = 0;
}


/* Destroys and frees the descriptor. */
static void
_sensors_port_free(AndroidSensorsPort* asp)
{
    if (asp != NULL) {
        _sensors_port_discard_sensors(asp);
        if (asp->sdkctl != NULL) {
            sdkctl_socket_release(asp->sdkctl);
        }
        AFREE(asp);
    }
}

/* Parses flat sensor list, and saves its entries into 'sensors' array filed of
 * the AndroidSensorsPort descriptor. */
static void
_sensors_port_save_sensors(AndroidSensorsPort* asp, const SensorEntry* list)
{
    const int count = _sensor_entry_list_size(list);
    if (count != 0) {
        int n;
        /* Allocate array for sensor descriptors. */
        asp->sensors = malloc(sizeof(SensorDescriptor*) * count);

        /* Iterate through the flat sensor list, filling up array of emulated
         * sensors. */
        const SensorEntry* entry = _sensor_entry_is_terminator(list) ? NULL : list;
        for (n = 0; n < count &&  entry != NULL; n++) {
            /* Get emulator-side ID for the sensor. < 0 value indicates that
             * sensor is not supported by the emulator. */
            const int emulator_id =
                android_sensors_get_id_from_name((char*)entry->sensor_name);
            if (emulator_id >= 0) {
                SensorDescriptor* desc;
                ANEW0(desc);
                desc->emulator_id   = emulator_id;
                desc->sensor_id     = entry->sensor_id;
                desc->sensor_name   = ASTRDUP(entry->sensor_name);

                asp->sensors[asp->sensors_count++] = desc;
                D("Sensors: Emulated sensor '%s': Device id = %d, Emulator id = %d",
                  desc->sensor_name, desc->sensor_id, desc->emulator_id);
            } else {
                D("Sensors: Sensor '%s' is not support by emulator",
                  entry->sensor_name);
            }
            entry = _sensor_entry_next(entry);
        }
        D("Sensors: Emulating %d sensors", asp->sensors_count);
    }
}

/* Finds sensor descriptor for an SDK controller-side ID. */
static const SensorDescriptor*
_sensor_from_sdkctl_id(AndroidSensorsPort* asp, int id)
{
    int n;
    for (n = 0; n < asp->sensors_count; n++) {
        if (asp->sensors[n]->sensor_id == id) {
            return asp->sensors[n];
        }
    }
    return NULL;
}

/* Initiates sensor emulation.
 * Param:
 *  asp - Android sensors port instance returned from sensors_port_create.
 * Return:
 *  Zero on success, failure otherwise.
 */
static void
_sensors_port_start(AndroidSensorsPort* asp)
{
    int n;

    if (!sdkctl_socket_is_port_ready(asp->sdkctl)) {
        /* SDK controller side is not ready for emulation. Retreat... */
        D("Sensors: SDK controller side is not ready for emulation.");
        return;
    }

    /* Disable all sensors, and reenable only those that are emulated by
     * hardware. */
    sensors_port_disable_sensor(asp, "all");

    /* Walk throuh the list of enabled sensors enabling them on the device. */
    for (n = 0; n < asp->sensors_count; n++) {
        if (android_sensors_get_sensor_status(asp->sensors[n]->emulator_id) == 1) {
            /* Reenable emulation for this sensor. */
            sensors_port_enable_sensor(asp, asp->sensors[n]->sensor_name);
            D("Sensors: Sensor '%s' is enabled on SDK controller.",
              asp->sensors[n]->sensor_name);
        }
    }

    /* Start the emulation. */
    SDKCtlMessage* const msg =
        sdkctl_message_send(asp->sdkctl, SDKCTL_SENSORS_START, NULL, 0);
    sdkctl_message_release(msg);

    D("Sensors: Emulation has been started.");
}

/********************************************************************************
 *                          Sensors port callbacks
 *******************************************************************************/

/* Completion for the "list" query. */
static AsyncIOAction
_on_sensor_list_query(void* query_opaque,
                      SDKCtlQuery* query,
                      AsyncIOState status)
{
    AndroidSensorsPort* const asp = (AndroidSensorsPort*)(query_opaque);
    if (status != ASIO_STATE_SUCCEEDED) {
        /* We don't really care about failures at this point. They will
         * eventually surface up in another place. */
        return ASIO_ACTION_DONE;
    }

    /* Parse query response which is a flat list of SensorEntry entries. */
    const SensorEntry* const list =
        (const SensorEntry*)sdkctl_query_get_buffer_out(query);
    D("Sensors: Sensor list received with %d sensors.",
      _sensor_entry_list_size(list));
    _sensors_port_save_sensors(asp, list);

    /* At this point we are ready to statr sensor emulation. */
    _sensors_port_start(asp);

    return ASIO_ACTION_DONE;
}

/* A callback that is invoked on sensor events.
 * Param:
 *  asp - AndroidSensorsPort instance.
 *  event - Sensor event.
 */
static void
_on_sensor_event(AndroidSensorsPort* asp, const SensorEvent* event)
{
    /* Find corresponding server descriptor. */
    const SensorDescriptor* const desc =
        _sensor_from_sdkctl_id(asp, event->sensor_id);
    if (desc != NULL) {
        T("Sensors: %s -> %f, %f, %f", desc->sensor_name,
          event->fvalues[0], event->fvalues[1],
          event->fvalues[2]);
        /* Fire up sensor change in the guest. */
        android_sensors_set(desc->emulator_id, event->fvalues[0],
                            event->fvalues[1], event->fvalues[2]);
    } else {
        W("Sensors: No descriptor for sensor %d", event->sensor_id);
    }
}

/* A callback that is invoked on SDK controller socket connection events. */
static AsyncIOAction
_on_sensors_socket_connection(void* client_opaque,
                             SDKCtlSocket* sdkctl,
                             AsyncIOState status)
{
    AndroidSensorsPort* const asp = (AndroidSensorsPort*)client_opaque;
    if (status == ASIO_STATE_FAILED) {
        /* Disconnection could mean that user is swapping devices. New device may
         * have different set of sensors, so we need to re-query sensor list on
         * reconnection. */
        _sensors_port_discard_sensors(asp);

        /* Reconnect (after timeout delay) on failures */
        if (sdkctl_socket_is_handshake_ok(sdkctl)) {
            sdkctl_socket_reconnect(sdkctl, SDKCTL_DEFAULT_TCP_PORT,
                                    SDKCTL_SENSORS_TIMEOUT);
        }
    }
    return ASIO_ACTION_DONE;
}

/* A callback that is invoked on SDK controller port connection events. */
static void
_on_sensors_port_connection(void* client_opaque,
                           SDKCtlSocket* sdkctl,
                           SdkCtlPortStatus status)
{
    AndroidSensorsPort* const asp = (AndroidSensorsPort*)client_opaque;
    switch (status) {
        case SDKCTL_PORT_CONNECTED: {
            D("Sensors: SDK Controller is connected.");
            /* Query list of available sensors. */
            SDKCtlQuery* const query =
                sdkctl_query_build_and_send(asp->sdkctl, SDKCTL_SENSORS_QUERY_LIST,
                                            0, NULL, NULL, NULL,
                                            _on_sensor_list_query, asp,
                                            SDKCTL_SENSORS_TIMEOUT);
            sdkctl_query_release(query);
            break;
        }

        case SDKCTL_PORT_DISCONNECTED:
            _sensors_port_discard_sensors(asp);
            D("Sensors: SDK Controller is disconnected.");
            break;

        case SDKCTL_PORT_ENABLED:
            _sensors_port_start(asp);
            D("Sensors: SDK Controller is enabled.");
            break;

        case SDKCTL_PORT_DISABLED:
            D("Sensors: SDK Controller is disabled.");
            break;

        case SDKCTL_HANDSHAKE_CONNECTED:
            D("Sensors: SDK Controller has succeeded handshake, and port is connected.");
            break;

        case SDKCTL_HANDSHAKE_NO_PORT:
            D("Sensors: SDK Controller has succeeded handshake, and port is not connected.");
            break;

        case SDKCTL_HANDSHAKE_DUP:
            E("Sensors: SDK Controller has failed the handshake due to port duplication.");
            sdkctl_socket_disconnect(sdkctl);
            break;

        case SDKCTL_HANDSHAKE_UNKNOWN_QUERY:
            E("Sensors: SDK Controller has failed the handshake due to unknown query.");
            sdkctl_socket_disconnect(sdkctl);
            break;

        case SDKCTL_HANDSHAKE_UNKNOWN_RESPONSE:
        default:
            E("Sensors: Handshake has failed due to unknown reasons.");
            sdkctl_socket_disconnect(sdkctl);
            break;
    }
}

/* A callback that is invoked when a message is received from SDK controller. */
static void
_on_sensors_message(void* client_opaque,
                   SDKCtlSocket* sdkctl,
                   SDKCtlMessage* message,
                   int msg_type,
                   void* msg_data,
                   int msg_size)
{
    AndroidSensorsPort* const asp = (AndroidSensorsPort*)client_opaque;
    switch (msg_type) {
        case SDKCTL_SENSORS_SENSOR_EVENT:
            _on_sensor_event(asp, (const SensorEvent*)msg_data);
            break;

        default:
            E("Sensors: Unknown message type %d", msg_type);
            break;
    }
}

/********************************************************************************
 *                          Sensors port API
 *******************************************************************************/

AndroidSensorsPort*
sensors_port_create(void* opaque)
{
    AndroidSensorsPort* asp;

    ANEW0(asp);
    asp->opaque = opaque;
    asp->sensors = NULL;
    asp->sensors_count = 0;
    asp->sdkctl = sdkctl_socket_new(SDKCTL_SENSORS_TIMEOUT, "sensors",
                                    _on_sensors_socket_connection,
                                    _on_sensors_port_connection,
                                    _on_sensors_message, asp);
    sdkctl_init_recycler(asp->sdkctl, 76, 8);
    sdkctl_socket_connect(asp->sdkctl, SDKCTL_DEFAULT_TCP_PORT,
                          SDKCTL_SENSORS_TIMEOUT);
    return asp;
}

void
sensors_port_destroy(AndroidSensorsPort* asp)
{
    if (asp->sdkctl != NULL) {
        sdkctl_socket_disconnect(asp->sdkctl);
    }
    _sensors_port_free(asp);
}

int
sensors_port_enable_sensor(AndroidSensorsPort* asp, const char* name)
{
    if (asp->sdkctl != NULL && sdkctl_socket_is_port_ready(asp->sdkctl)) {
        SDKCtlMessage* const msg = sdkctl_message_send(asp->sdkctl,
                                                       SDKCTL_SENSORS_ENABLE,
                                                       name, strlen(name));
        sdkctl_message_release(msg);
        return 0;
    } else {
        return -1;
    }
}

int
sensors_port_disable_sensor(AndroidSensorsPort* asp, const char* name)
{
    if (asp->sdkctl != NULL && sdkctl_socket_is_port_ready(asp->sdkctl)) {
        SDKCtlMessage* const msg = sdkctl_message_send(asp->sdkctl,
                                                       SDKCTL_SENSORS_DISABLE,
                                                       name, strlen(name));
        sdkctl_message_release(msg);
        return 0;
    } else {
        return -1;
    }
}
