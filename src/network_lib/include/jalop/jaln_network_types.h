/**
 * @file jaln_network_types.h
 *
 * Public types of the JALoP Network Library.
 *
 * @section LICENSE
 *
 * Source code in 3rd-party is licensed and owned by their respective
 * copyright holders.
 *
 * All other source code is copyright Tresys Technology and licensed as below.
 *
 * Copyright (c) 2012 Tresys Technology LLC, Columbia, Maryland, USA
 *
 * This software was developed by Tresys Technology LLC
 * with U.S. Government sponsorship.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _JALN_NETWORK_TYPES_H_
#define _JALN_NETWORK_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <jalop/jal_status.h>
#include <stdint.h>

/** Integer version of JALoP network protocol */
#define JALN_JALOP_VERSION_ONE 1
#define JALN_JALOP_VERSION_TWO 2

/**
 * Enum used to distinguish between record types
 */
enum jaln_record_type {
	/** Indicates a Journal Record */
	JALN_RTYPE_JOURNAL = 1 << 0,
	/** Indicates an Audit Record */
	JALN_RTYPE_AUDIT = 1 << 1,
	/** Indicates a Log Record */
	JALN_RTYPE_LOG = 1 << 2,
};

/** Defined bit value for all possible records types */
#define JALN_RTYPE_ALL (JALN_RTYPE_JOURNAL | JALN_RTYPE_AUDIT | JALN_RTYPE_LOG)
/**
 * Structure to encompass MIME headers
 */
struct jaln_mime_header {
	/** The name of this header */
	char *name;
	/** The value of this header */
	char *value;
	/** The next header in the list */
	struct jaln_mime_header *next;
};

/**
 * Create a jaln_mime_header list.
 *  
 * @param[in,out] headers The structure will contain the list of
 *       headers. This will be set to NULL.
 *  
 * @return a newly created jaln_mime_header_list
 */
struct jaln_mime_header *jaln_mime_header_create(struct jaln_mime_header **headers);

/**
 * Destroy a list of MIME headers
 *
 * @param[in,out] headers The headers to free. This will be set to NULL.
 */
void jaln_mime_header_destroy(struct jaln_mime_header **headers);

/**
 * Information about a connected JALoP Channel
 */
struct jaln_channel_info {
	/** The hostname of the remote peer (if available) */
	char *hostname;
	/** The IP address in dotted decimal notation (i.e. "192.168.1.6") */
	char *addr;
	/** The selected XML compression, "XML", "EXI", "deflate" etc */
	char *compression;
	/** The selected digest method, "sha256", "sha512", etc */
	char *digest_method;
	/** The type of JAL records exchanged on this channel */
	enum jaln_record_type type;
};

/**
 * Provides some metadata about a specific JAL record.
 */
struct jaln_record_info {
	/** The type of this record (journal, audit, or log) */
	enum jaln_record_type type;
	/** The nonce of this record */
	char *nonce;
	/** The length of the system metadata */
	uint64_t sys_meta_len;
	/** The length of the application metadata */
	uint64_t app_meta_len;
	/** The length of the payload (raw journal, audit, or log data) */
	uint64_t payload_len;
};

/**
 * Enum used to indicate the publishing mode (Archive or Live)
 */
enum jaln_publish_mode {
	/** Publisher should send all data, including historical data  */
	JALN_ARCHIVE_MODE,
	/** Publisher should send only new data as of the time of the Subscribe request */
	JALN_LIVE_MODE,
	/** The mode has not been initialized */
	JALN_UNKNOWN_MODE,
};

/**
 * Enum used to indicate the 'status' of a digest
 */
enum jaln_digest_status {
	/** Indicates the digest calculated by both peers is the same */
	JALN_DIGEST_STATUS_CONFIRMED,
	/** Indicates the digest calculated by both peers is not the same */
	JALN_DIGEST_STATUS_INVALID,
	/** Indicates the nonce was not recognized */
	JALN_DIGEST_STATUS_UNKNOWN,
};

/**
 * Enum used to indicate the role a particular peer is supposed to fill.
 * @see #jaln_connect_ack
 */
enum jaln_role {
	/**
	 * The role is has not been determined yet.
	 */
	 JALN_ROLE_UNSET = 0,
	/**
	 * The peer should act as a subscriber. They are expected to send only
	 * 'subscribe', 'journal-resume', 'digest', and 'sync' messages.
	 *
	 * The must be prepared to handle '*-record', and 'digest-response'
	 * messages.
	 */
	JALN_ROLE_SUBSCRIBER,
	/**
	 * The peer should act as a publisher. They are expected to send only
	 * '*-record', and 'digest-response'
	 *
	 * They must be prepared to handle 'subscribe', 'journal-recover',
	 * 'digest', and 'sync' messages.
	 */
	JALN_ROLE_PUBLISHER
};

/**
 * This represents the data that is sent as part of a 'connect-ack' message.
 */
struct jaln_connect_ack {
	/** The hostname (if available) of the remote peer. */
	char *hostname;
	/** The IP address in dotted decimal notation (i.e. "192.168.1.1"). */
	char *addr;
	/**
	 * The version of JALoP that the peers are using to communicate.
	 */
	int jaln_version;
	/**
	 * The JALoP user agent string (if any). This is the user agent of the
	 * receiver of the 'connect' (sender of 'connect-ack') message.
	 */
	char *jaln_agent;
	/**
	 * Indicates which role this side of the connection is expected to
	 * play.
	 */
	enum jaln_role mode;
	/**
	 * This list contains any extra headers, not processed by the JNL. It
	 * only contains additional headers not included the JALoP spec.
	 */
	struct jaln_mime_header *headers;
};

/**
 * This represents the data that is sent as part of a 'connect' message.
 */
struct jaln_connect_request {
	/** The hostname of the remote peer */
	char *hostname;
	/** The address of the remote peer */
	char *addr;
	/** Information about the connection request; */
	struct jaln_channel_info *ch_info;
	/** The requested type of data to transfer using this channel. */
	enum jaln_record_type type;
	/** The version of JALoP that the peers are using to communicate. */
	int jaln_version;
	/**
	 * The proposed compressions the sender of this 'connect' message is will
	 * to use.
	 */
	char **compressions;
	/** The number of compressions in the array. */
	int cmp_cnt;
	/** The proposed digest methods. */
	char **digests;
	/** The number of digests in the array. */
	int dgst_cnt;
	/**
	 * The role as sent by the remote peer. Note that when the peer sends a
	 * 'connect' message with the role set to JALN_ROLE_SUBSCRIBE, it is
	 * indicating that it plans on acting as a subscriber. Conversely, when
	 * the role is JALN_ROLE_PUBLISH, it indicates the peer plans on acting
	 * as a publisher.
	 */
	enum jaln_role role;
	/**
 	 * The mode sent by the remote peer.  Can be either JALN_LIVE_MODE or
 	 * JALN_ARCHIVE_MODE.  In archive mode, the publisher should send all
 	 * data.  In live mode, the publisher should send only new data.
 	 */
	enum jaln_publish_mode mode;
	/**
	 * The jal user agent string (if any). This is the user agent of the
	 * sender of the 'connect' message.
	 */
	char *jaln_agent;
};

/**
 * The JNL fills out the #jaln_connect_nack and passes it to the application
 * when the peer sends a 'connect-nack' message.
 */
struct jaln_connect_nack {
	/** Information about the channel. */
	struct jaln_channel_info *ch_info;
	/** Array of failure reasons given by the remote peer. */
	char **error_list;
	/** Number of elements in the \p error_list */
	int error_cnt;
	/**
	 * Any additional headers. This list will not contain the headers for
	 * the errors in #error_list.
	 */
	struct jaln_mime_header *headers;
};

/**
 * @struct jaln_payload_feeder
 * The jaln_payload_feeder is used to send bytes of the payload (journal,
 * audit, or log data) to the remote peer.
 */
struct jaln_payload_feeder {
	/**
	 * An application may set this to anything they like. It will be passed
	 * as the \p feeder_data parameter of #get_bytes.
	 *
	 * the JNL will call release_payload_feeder when it is done with a
	 * particular instance to give the application a chance to release any
	 * data associated with the feeder.
	 */
	void *feeder_data;
	/**
	 * The JNL calls this when it needs to read more bytes of the payload
	 * (raw journal, audit, or log data).
	 *
	 * @param[in] offset The offset, in bytes, to start reading from.
	 * @param[in] buffer The buffer to fill with data.
	 * @param[in,out] size The number of bytes available in the buffer.
	 * Applications must set this to the actual number of bytes read.
	 * @param[in] feeder_data the application defined feeder_data pointer of this struct.
	 *
	 * @return JAL_OK to continue, some other value to stop sending data.
	 */
	enum jal_status (*get_bytes)(const uint64_t offset,
			   uint8_t * const buffer,
			   uint64_t *size,
			   void *feeder_data);
};

/**
 * Used to indicate whether a connection should be accepted or rejected
 */
enum jaln_connect_error {
	JALN_CE_ACCEPT = 0,
	JALN_CE_UNSUPPORTED_VERSION      = 1 << 0,
	JALN_CE_UNSUPPORTED_COMPRESSION  = 1 << 1,
	JALN_CE_UNSUPPORTED_DIGEST       = 1 << 2,
	JALN_CE_UNSUPPORTED_MODE         = 1 << 3,
	JALN_CE_UNAUTHORIZED_MODE        = 1 << 4,
};

struct jaln_connection;


/**
 * This handle maintains the data associated with HTTP connections used
 * to transmit/receive JALoP messages. For each type of data (journal, audit
 * or log) exchanged with a particular remote.
 */
typedef struct jaln_session_t jaln_session;

/**
 * This holds global data such as base publisher
 * callbacks, and channel creation handlers.
 */
typedef struct jaln_context_t jaln_context;

#ifdef __cplusplus
}
#endif

#endif // _JALN_NETWORK_TYPES_H_
