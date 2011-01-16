/*
   Copyright (C) 2010 by Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#ifndef CCAN_ISCSI_H
#define CCAN_ISCSI_H

struct iscsi_context;
struct sockaddr;


/*
 * Returns the file descriptor that libiscsi uses.
 */
int iscsi_get_fd(struct iscsi_context *iscsi);

/*
 * Returns which events that we need to poll for for the iscsi file descriptor.
 */
int iscsi_which_events(struct iscsi_context *iscsi);

/*
 * Called to process the events when events become available for the iscsi file descriptor.
 */
int iscsi_service(struct iscsi_context *iscsi, int revents);



/*
 * Create a context for an ISCSI session.
 * Initiator_name is the iqn name we want to identify to the target as.
 *
 * Returns:
 *  0: success
 * <0: error
 */
struct iscsi_context *iscsi_create_context(const char *initiator_name);

/*
 * Destroy an existing ISCSI context and tear down any existing connection.
 * Callbacks for any command in flight will be invoked with ISCSI_STATUS_CANCELLED.
 *
 * Returns:
 *  0: success
 * <0: error
 */
int iscsi_destroy_context(struct iscsi_context *iscsi);

/*
 * Set an optional alias name to identify with when connecting to the target
 *
 * Returns:
 *  0: success
 * <0: error
 */
int iscsi_set_alias(struct iscsi_context *iscsi, const char *alias);

/*
 * Set the iqn name of the taqget to login to.
 * The target name must be set before a normal-login can be initiated.
 * Only discovery-logins are possible without setting the target iqn name.
 *
 * Returns:
 *  0: success
 * <0: error
 */
int iscsi_set_targetname(struct iscsi_context *iscsi, const char *targetname);


/* Types of icsi sessions. Discovery sessions are used to query for what targets exist behind
 * the portal connected to. Normal sessions are used to log in and do I/O to the SCSI LUNs
 */
enum iscsi_session_type {ISCSI_SESSION_DISCOVERY=1, ISCSI_SESSION_NORMAL=2};

/*
 * Set the session type for a scsi context.
 * Session type can only be set/changed while the iscsi context is not logged in to
 * a target.
 *
 * Returns:
 *  0: success
 * <0: error
 */
int iscsi_set_session_type(struct iscsi_context *iscsi, enum iscsi_session_type session_type);


/* ISCSI_STATUS_GOOD must map to SCSI_STATUS_GOOD
 * and ISCSI_STATUS_CHECK_CONDITION must map to SCSI_STATUS_CHECK_CONDITION
 */
enum icsi_status { ISCSI_STATUS_GOOD		=0,
     		   ISCSI_STATUS_CHECK_CONDITION	=2,
		   ISCSI_STATUS_CANCELLED	=0x0f000000,
		   ISCSI_STATUS_ERROR		=0x0f000001 };


/*
 * Generic callback for completion of iscsi_*_async().
 * command_data depends on status.
 */
typedef void (*iscsi_command_cb)(struct iscsi_context *iscsi, int status, void *command_data, void *private_data);


/*
 * Asynchronous call to connect a TCP connection to the target-host/port
 *
 * Returns:
 *  0 if the call was initiated and a connection will be attempted. Result of the connection will be reported
 *    through the callback function.
 * <0 if there was an error. The callback function will not be invoked.
 *
 * This command is unique in that the callback can be invoked twice.
 *
 * Callback parameters :
 * status can be either of :
 *    ISCSI_STATUS_GOOD     : Connection was successful. Command_data is NULL.
 *                            In this case the callback will be invoked a second time once the connection
 *                            is torn down.
 *
 *    ISCSI_STATUS_ERROR    : Either failed to establish the connection, or an already established connection
 *                            has failed with an error.
 *
 * The callback will NOT be invoked if the session is explicitly torn down through a call to
 * iscsi_disconnect() or iscsi_destroy_context().
 */
int iscsi_connect_async(struct iscsi_context *iscsi, const char *target, iscsi_command_cb cb, void *private_data);

/*
 * Disconnect a connection to a target.
 * You can not disconnect while being logged in to a target.
 *
 * Returns:
 *  0 disconnect was successful
 * <0 error
 */
int iscsi_disconnect(struct iscsi_context *iscsi);

/*
 * Asynchronous call to perform an ISCSI login.
 *
 * Returns:
 *  0 if the call was initiated and a login will be attempted. Result of the login will be reported
 *    through the callback function.
 * <0 if there was an error. The callback function will not be invoked.
 *
 * Callback parameters :
 * status can be either of :
 *    ISCSI_STATUS_GOOD     : login was successful. Command_data is always NULL.
 *    ISCSI_STATUS_CANCELLED: login was aborted. Command_data is NULL.
 *    ISCSI_STATUS_ERROR    : login failed. Command_data is NULL.
 */
int iscsi_login_async(struct iscsi_context *iscsi, iscsi_command_cb cb, void *private_data);


/*
 * Asynchronous call to perform an ISCSI logout.
 *
 * Returns:
 *  0 if the call was initiated and a logout will be attempted. Result of the logout will be reported
 *    through the callback function.
 * <0 if there was an error. The callback function will not be invoked.
 *
 * Callback parameters :
 * status can be either of :
 *    ISCSI_STATUS_GOOD     : logout was successful. Command_data is always NULL.
 *    ISCSI_STATUS_CANCELLED: logout was aborted. Command_data is NULL.
 */
int iscsi_logout_async(struct iscsi_context *iscsi, iscsi_command_cb cb, void *private_data);


/*
 * Asynchronous call to perform an ISCSI discovery.
 *
 * discoveries can only be done on connected and logged in discovery sessions.
 *
 * Returns:
 *  0 if the call was initiated and a discovery  will be attempted. Result of the logout will be reported
 *    through the callback function.
 * <0 if there was an error. The callback function will not be invoked.
 *
 * Callback parameters :
 * status can be either of :
 *    ISCSI_STATUS_GOOD     : Discovery was successful. Command_data is a pointer to a
 *                            iscsi_discovery_address list of structures.
 *                            This list of structures is only valid for the duration of the
 *                            callback and all data will be freed once the callback returns.
 *    ISCSI_STATUS_CANCELLED: Discovery was aborted. Command_data is NULL.
 */
int iscsi_discovery_async(struct iscsi_context *iscsi, iscsi_command_cb cb, void *private_data);

struct iscsi_discovery_address {
       struct iscsi_discovery_address *next;
       const char *target_name;
       const char *target_address;
};

/*
 * Asynchronous call to perform an ISCSI NOP-OUT call
 *
 * Returns:
 *  0 if the call was initiated and a nop-out will be attempted. Result will be reported
 *    through the callback function.
 * <0 if there was an error. The callback function will not be invoked.
 *
 * Callback parameters :
 * status can be either of :
 *    ISCSI_STATUS_GOOD     : NOP-OUT was successful and the server responded with a NOP-IN
 *                            callback_data is a iscsi_data structure containing the data returned from
 *                            the server.
 *    ISCSI_STATUS_CANCELLED: Discovery was aborted. Command_data is NULL.
 */
int iscsi_nop_out_async(struct iscsi_context *iscsi, iscsi_command_cb cb, unsigned char *data, int len, void *private_data);


/* These are the possible status values for the callbacks for scsi commands.
 * The content of command_data depends on the status type.
 *
 * status :
 *   ISCSI_STATUS_GOOD the scsi command completed successfullt on the target.
 *   If this scsi command returns DATA-IN, that data is stored in an scsi_task structure
 *   returned in the command_data parameter. This buffer will be automatically freed once the callback
 *   returns.
 *
 *   ISCSI_STATUS_CHECK_CONDITION the scsi command failed with a scsi sense.
 *   Command_data contains a struct scsi_task. When the callback returns, this buffer
 *   will automatically become freed.
 *
 *   ISCSI_STATUS_CANCELLED the scsi command was aborted. Command_data is NULL.
 *
 *   ISCSI_STATUS_ERROR the command failed. Command_data is NULL.
 */



struct iscsi_data {
       int size;
       unsigned char *data;
};


/*
 * Async commands for SCSI
 */
int iscsi_reportluns_async(struct iscsi_context *iscsi, iscsi_command_cb cb, int report_type, int alloc_len, void *private_data);
int iscsi_testunitready_async(struct iscsi_context *iscsi, int lun, iscsi_command_cb cb, void *private_data);
int iscsi_inquiry_async(struct iscsi_context *iscsi, int lun, iscsi_command_cb cb, int evpd, int page_code, int maxsize, void *private_data);
int iscsi_readcapacity10_async(struct iscsi_context *iscsi, int lun, iscsi_command_cb cb, int lba, int pmi, void *private_data);
int iscsi_read10_async(struct iscsi_context *iscsi, int lun, iscsi_command_cb cb, int lba, int datalen, int blocksize, void *private_data);
int iscsi_write10_async(struct iscsi_context *iscsi, int lun, iscsi_command_cb cb, unsigned char *data, int datalen, int lba, int fua, int fuanv, int blocksize, void *private_data);
int iscsi_modesense6_async(struct iscsi_context *iscsi, int lun, iscsi_command_cb cb, int dbd, int pc, int page_code, int sub_page_code, unsigned char alloc_len, void *private_data);


#endif /* CCAN_ISCSI_H */
