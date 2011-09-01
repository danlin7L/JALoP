/**
 * @file jalp_digest_internal.h This file defines functions for calculating
 * digests.
 *
 * @section LICENSE
 *
 * Source code in 3rd-party is licensed and owned by their respective
 * copyright holders.
 *
 * All other source code is copyright Tresys Technology and licensed as below.
 *
 * Copyright (c) 2011 Tresys Technology LLC, Columbia, Maryland, USA
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

#include <unistd.h>
#include <errno.h>
#include <jalop/jal_status.h>
#include <jalop/jal_digest.h>
#include "jal_error_callback_internal.h"
#include "jal_alloc.h"
#include "jalp_digest_internal.h"

#define DIGEST_BUF_SIZE 8192

enum jal_status jalp_digest_buffer(struct jal_digest_ctx *digest_ctx,
		const uint8_t *data, size_t len, uint8_t **digest)
{
	if(!data || !digest || *digest) {
		return JAL_E_INVAL;
	}

	if (!jal_digest_ctx_is_valid(digest_ctx)) {
		return JAL_E_INVAL;
	}

	void *instance = digest_ctx->create();
	if(!instance) {
		jal_error_handler(JAL_E_NO_MEM);
	}
	*digest = jal_malloc(digest_ctx->len);

	enum jal_status ret = JAL_E_INVAL;
	ret = digest_ctx->init(instance);
	if(ret != JAL_OK) {
		goto err_out;
	}

	ret = digest_ctx->update(instance, data, len);
	if(ret != JAL_OK) {
		goto err_out;
	}

	size_t digest_length = digest_ctx->len;
	ret = digest_ctx->final(instance, *digest, &digest_length);
	if(ret != JAL_OK) {
		goto err_out;
	}

	digest_ctx->destroy(instance);
	return JAL_OK;

err_out:
	digest_ctx->destroy(instance);
	free(*digest);
	*digest = NULL;
	return ret;

}

enum jal_status jalp_digest_fd(struct jal_digest_ctx *digest_ctx,
                int fd, uint8_t **digest)
{
	if(!digest || *digest || fd < 0) {
		return JAL_E_INVAL;
	}

	if(!jal_digest_ctx_is_valid(digest_ctx)) {
		return JAL_E_INVAL;
	}

	enum jal_status ret;
	*digest = jal_malloc(digest_ctx->len);

	void *instance = digest_ctx->create();
	if (!instance) {
		jal_error_handler(JAL_E_NO_MEM);
	}

	void *buf = jal_malloc(DIGEST_BUF_SIZE);

	ret = digest_ctx->init(instance);
	if(ret != JAL_OK) {
		goto err_out;
	}


	int bytes_read;
	int seek_ret = lseek(fd, 0, SEEK_SET);
	if (seek_ret == -1) {
		ret = JAL_E_FILE_IO;
		goto err_out;
	}


	while ((bytes_read = read(fd, buf, DIGEST_BUF_SIZE)) > 0) {
		ret = digest_ctx->update(instance, buf, bytes_read);
		if(ret != JAL_OK) {
			goto err_out;
		}
	}
	if (bytes_read == -1) {
		ret = JAL_E_FILE_IO;
		goto err_out;
	}

	size_t digest_length = digest_ctx->len;
	ret = digest_ctx->final(instance, *digest, &digest_length);
	if(ret != JAL_OK) {
		goto err_out;
	}

	free(buf);
	digest_ctx->destroy(instance);
	return JAL_OK;

err_out:
	free(buf);
	digest_ctx->destroy(instance);
	free(*digest);
	*digest = NULL;
	return ret;

}
