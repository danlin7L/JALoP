/**
 * @file jalls_handle_journal.cpp This file contains functions to handle a journal
 * to the jal local store.
 *
 * @section LICENSE
 *
 * Source code in 3rd-party is licensed and owned by their respective
 * copyright holders.
 *
 * All other source code is copyright Tresys Technology and licensed as below.
 *
 * Copyright (c) 2012-2013 Tresys Technology LLC, Columbia, Maryland, USA
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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <jalop/jal_status.h>
#include <jalop/jal_digest.h>

#include "jal_alloc.h"

#include "jaldb_context.hpp"
#include "jaldb_segment.h"
#include "jaldb_utils.h"

#include "jalls_context.h"
#include "jalls_handle_journal.hpp"
#include "jalls_handler.h"
#include "jalls_msg.h"
#include "jalls_record_utils.h"
#include "jaldb_record_xml.h"

#define JALLS_JOURNAL_BUF_LEN 8192

extern "C" int jalls_handle_journal(struct jalls_thread_context *thread_ctx, uint64_t data_len, uint64_t meta_len)
{

	if (!thread_ctx || !(thread_ctx->ctx)) {
		return -1; //should never happen.
	}

	struct jaldb_record *rec = NULL;

	int db_payload_fd = -1;
	char *db_payload_path = NULL;
	enum jal_status jal_err;
	enum jaldb_status db_err;

	uint8_t *app_meta_buf = NULL;
	int err;
	int ret = -1;

	int debug = thread_ctx->ctx->debug;

	uint64_t bytes_remaining;

	struct jal_digest_ctx *digest_ctx = NULL;
	uint8_t *digest = NULL;
	uint8_t *payload_digest = NULL;
	int payload_digest_len = 0;
	char *payload_alg = NULL;
	uint8_t *app_meta_digest = NULL;
	int app_meta_digest_len = 0;
	char *app_meta_alg = NULL;

	void *sha256_instance = NULL;

	char *nonce = NULL;
	
	RSA *signing_key = NULL;

	if (thread_ctx->ctx->sign_sys_meta) {
		signing_key = thread_ctx->signing_key;
	}

	//get a file from the db layer to write the journal data to.
	uuid_t uuid;
	uuid_generate(uuid);
	db_err = jaldb_create_file(thread_ctx->db_ctx->journal_root, &db_payload_path,
			&db_payload_fd, uuid, JALDB_RTYPE_JOURNAL, JALDB_DTYPE_PAYLOAD);
	if (db_err != JALDB_OK) {
		if (debug) {
			fprintf(stderr, "could not create a file to store journal data\n");
		}
		goto err_out;
	}

	//get the payload, write it to the db file.
	//digests the payload as well
	char data_buf[JALLS_JOURNAL_BUF_LEN];
	memset(data_buf, 0, JALLS_JOURNAL_BUF_LEN);

	struct iovec iov[1];
	iov[0].iov_base = data_buf;
	if (data_len > JALLS_JOURNAL_BUF_LEN) {
			iov[0].iov_len = JALLS_JOURNAL_BUF_LEN;
	} else {
			iov[0].iov_len = data_len;
	}

	struct msghdr msgh;
	memset(&msgh, 0, sizeof(msgh));

	msgh.msg_iov = iov;
	msgh.msg_iovlen = 1;

	ssize_t bytes_received;

	bytes_remaining = data_len;
	int bytes_written;

	digest_ctx = jal_digest_ctx_create(JAL_DIGEST_ALGORITHM_SHA256);
	sha256_instance = digest_ctx->create();
	digest = (uint8_t *)jal_malloc(digest_ctx->len);
	jal_err = digest_ctx->init(sha256_instance);
	if(jal_err != JAL_OK) {
		if (debug) {
			fprintf(stderr, "could not init sha256 digest context\n");
		}
		goto err_out;
	}

	bytes_received = jalls_recvmsg_helper(thread_ctx->fd, &msgh, debug);
	while (bytes_received > 0 && bytes_remaining >= (uint64_t)bytes_received) {
		bytes_remaining -= (uint64_t)bytes_received;
		jal_err = digest_ctx->update(sha256_instance, (uint8_t *)data_buf, bytes_received);
		if (jal_err != JAL_OK) {
			if (debug) {
				fprintf(stderr, "could not digest the journal data\n");
			}
			goto err_out;
		}
		bytes_written = write(db_payload_fd, data_buf, bytes_received);
		if (bytes_written < 0) {
			if (debug) {
				fprintf(stderr, "could not write journal to file\n");
			}
			goto err_out;
		}
		if (bytes_remaining == 0) {
				break;
		}
		iov[0].iov_len = (bytes_remaining < JALLS_JOURNAL_BUF_LEN) ? bytes_remaining : JALLS_JOURNAL_BUF_LEN;
		bytes_received = jalls_recvmsg_helper(thread_ctx->fd, &msgh, debug);
	}
	if (bytes_received < 0) {
		if (debug) {
			fprintf(stderr, "could not receive journal data\n");
		}
		goto err_out;
	}
	size_t digest_length;
	digest_length = digest_ctx->len;
	jal_err = digest_ctx->final(sha256_instance, digest, &digest_length);
	if(jal_err != JAL_OK) {
		if (debug) {
			fprintf(stderr, "could not digest the journal\n");
		}
		goto err_out;
	}

	//get first break string
	err = jalls_handle_break(thread_ctx->fd);
	if (err < 0) {
		if (debug) {
			fprintf(stderr, "could not receive first BREAK\n");
		}
		goto err_out;
	}

	//get the app_metadata
	if (meta_len) {
		err = jalls_handle_app_meta(&app_meta_buf, meta_len, thread_ctx->fd, debug);
		if (err < 0) {
			if (debug) {
				fprintf(stderr, "could not receive the application metadata\n");
			}
			goto err_out;
		}
	}

	//get second break string
	err = jalls_handle_break(thread_ctx->fd);
	if (err < 0) {
		if (debug) {
			fprintf(stderr, "could not receive second BREAK\n");
		}
		goto err_out;
	}

	err = jalls_create_record(JALDB_RTYPE_JOURNAL, thread_ctx, &rec);
	if (err < 0) {
		if (debug) {
			fprintf(stderr, "failed to create record struct\n");
		}
		goto err_out;
	}

	if (meta_len) {
		rec->app_meta = jaldb_create_segment();
		rec->app_meta->length = meta_len;
		rec->app_meta->payload = app_meta_buf;
		rec->app_meta->on_disk = 0;
		app_meta_buf = NULL;
	}

	rec->payload = jaldb_create_segment();
	rec->payload->length = data_len;
	rec->payload->payload = (uint8_t*)db_payload_path;
	rec->payload->on_disk = 1;
	rec->payload->fd = db_payload_fd;
	db_payload_path = NULL;

	// Needed to generate system metadata
	rec->source = jal_strdup("localhost");

	if (thread_ctx->ctx->manifest_sys_meta) {
		if (rec->payload) {
			if (rec->payload->on_disk) {
				err = jal_digest_fd(digest_ctx, rec->payload->fd, &payload_digest);
			} else {
				err = jal_digest_buffer(digest_ctx, rec->payload->payload, rec->payload->length, &payload_digest);
			}
			if (JAL_OK != err) {
				if (debug) {
					fprintf(stderr, "Failed to calculate digest for record payload\n");
				}
				goto err_out;
			}
			payload_digest_len = digest_ctx->len;
			payload_alg = jal_strdup(digest_ctx->algorithm_uri); 
		}

		if (rec->app_meta) {
			err = jal_digest_buffer(digest_ctx, rec->app_meta->payload, rec->app_meta->length, &app_meta_digest);
			if (JAL_OK != err) {
				if (debug) {
					fprintf(stderr, "Failed to calculate digest for record Application Metadata\n");
				}
				goto err_out;
			}
			app_meta_digest_len = digest_ctx->len;
			app_meta_alg = jal_strdup(digest_ctx->algorithm_uri); 
		}

	}

	rec->sys_meta = jaldb_create_segment();
	db_err = jaldb_record_to_system_metadata_doc(rec,
						signing_key,
						app_meta_digest,
						app_meta_digest_len,
						app_meta_alg,
						payload_digest,
						payload_digest_len,
						payload_alg,
						(char **) &(rec->sys_meta->payload),
						&(rec->sys_meta->length));
	if (JALDB_OK != db_err) {
		if (debug) {
			fprintf(stderr, "Failed to generate system metadata for record\n");
		}
		goto err_out;
	}

	db_err = jaldb_insert_record(thread_ctx->db_ctx, rec, 1, &nonce);
	free(nonce);
	nonce = NULL;

	if (JALDB_OK != db_err) {
		fprintf(stderr, "could not insert journal record into database\n");
		switch (db_err) {
			case JALDB_E_REJECT:
				fprintf(stderr, "record was too large and was rejected\n");
				break;
			case JALDB_E_INTERNAL_ERROR:
				ret = JALDB_E_INTERNAL_ERROR;
				fprintf(stderr, "Internal database error occurred\n");
				break;
			default:
				break;
		}
		goto err_out;
	}
	ret = 0;

err_out:
	if (digest_ctx) {
		digest_ctx->destroy(sha256_instance);
		jal_digest_ctx_destroy(&digest_ctx);
	}
	free(digest);
	free(app_meta_buf);
	jaldb_destroy_record(&rec);
	free(app_meta_digest);
	free(app_meta_alg);
	free(payload_digest);
	free(payload_alg);
	return ret;
}
