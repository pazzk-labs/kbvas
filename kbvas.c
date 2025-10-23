/*
 * SPDX-FileCopyrightText: 2025 권경환 Kyunghwan Kwon <k@pazzk.net>
 *
 * SPDX-License-Identifier: MIT
 */

#include "kbvas.h"

#include <stdlib.h>
#include <string.h>

#include "libmcu/base64.h"

#if !defined(MIN)
#define MIN(a, b)			(((a) > (b))? (b) : (a))
#endif

#if !defined(KBVAS_DEBUG)
#define KBVAS_DEBUG(...)
#endif
#if !defined(KBVAS_INFO)
#define KBVAS_INFO(...)
#endif
#if !defined(KBVAS_ERROR)
#define KBVAS_ERROR(...)
#endif

#define MIN_TLV_LEN			6

enum data_type {
	TYPE_TIMESTAMP		= 0xA1,
	TYPE_VIN		= 0xA2,
	TYPE_SOC		= 0xA3,
	TYPE_SOH		= 0xA4,
	TYPE_BPA		= 0xA5,
	TYPE_BPV		= 0xA6,
	TYPE_BSV		= 0xA7,
	TYPE_BMT		= 0xA8,
	TYPE_SESSION_DURATION	= 0xB1,
	TYPE_BATTERY_ID		= 0xB2,
	TYPE_BSV_MIN_MAX	= 0xB7,
	TYPE_BMT_MIN_MAX	= 0xB8,
	TYPE_COUNTER		= 0xC1,
	TYPE_ENCRYPTED_VIN	= 0xC2,
};

struct tlv {
	uint8_t type;
	uint16_t length;
	const uint8_t *value;
};

struct kbvas {
	struct kbvas_backend_api *backend;
	void *backend_ctx;
	kbvas_batch_count_t batch_count;

	kbvas_batch_callback_t batch_cb;
	void *batch_cb_ctx;
};

static size_t parse_tlv(struct tlv *tlv, const uint8_t *data, size_t datasize)
{
	size_t bytes_parsed = 0;
	size_t expected_len;

	memset(tlv, 0, sizeof(*tlv));
	tlv->type = data[0];

	switch (tlv->type) {
	case TYPE_BSV:
		if (datasize < 4) {
			break;
		}
		tlv->length = (uint16_t)((uint16_t)data[1] << 8 | data[2]);
		expected_len = (size_t)tlv->length + 3;
		if (datasize >= expected_len) {
			tlv->value = &data[3];
			bytes_parsed = expected_len;
		}
		break;
	case TYPE_TIMESTAMP: /* fall through */
	case TYPE_VIN: /* fall through */
	case TYPE_SOC: /* fall through */
	case TYPE_SOH: /* fall through */
	case TYPE_BPA: /* fall through */
	case TYPE_BPV: /* fall through */
	case TYPE_BMT:
		if (datasize < 3) {
			break;
		}
		tlv->length = data[1];
		expected_len = (size_t)tlv->length + 2;
		if (datasize >= expected_len) {
			tlv->value = &data[2];
			bytes_parsed = expected_len;
		}
		break;
	default:
		break;
	}

	return bytes_parsed;
}

static kbvas_error_t parse_battery(const struct tlv *tlv,
		struct kbvas_entry *info)
{
	switch (tlv->type) {
	case TYPE_TIMESTAMP:
		if (tlv->length != 4) {
			return KBVAS_ERROR_INVALID_FORMAT;
		}
		info->timestamp = (time_t)((uint32_t)tlv->value[0] << 24 |
				(uint32_t)tlv->value[1] << 16 |
				(uint32_t)tlv->value[2] << 8 |
				(uint32_t)tlv->value[3]);
		break;
#if defined(KBVAS_USE_RAW_ENCODING)
	case TYPE_VIN:
		if (!tlv->length) {
			return KBVAS_ERROR_INVALID_FORMAT;
		}
		memcpy(info->data.vin, tlv->value,
				MIN(tlv->length, sizeof(info->data.vin)));
		break;
	case TYPE_SOC:
		if (tlv->length != 1) {
			return KBVAS_ERROR_INVALID_FORMAT;
		}
		info->data.soc = tlv->value[0];
		break;
	case TYPE_SOH:
		if (tlv->length != 1) {
			return KBVAS_ERROR_INVALID_FORMAT;
		}
		info->data.soh = tlv->value[0];
		break;
	case TYPE_BPA:
		if (tlv->length != 2) {
			return KBVAS_ERROR_INVALID_FORMAT;
		}
		info->data.bpa = (uint16_t)((uint16_t)tlv->value[0] << 8
				| tlv->value[1]);
		break;
	case TYPE_BPV:
		if (tlv->length != 2) {
			return KBVAS_ERROR_INVALID_FORMAT;
		}
		info->data.bpv = (uint16_t)((uint16_t)tlv->value[0] << 8
				| tlv->value[1]);
		break;
	case TYPE_BSV:
		if (!tlv->length) {
			return KBVAS_ERROR_INVALID_FORMAT;
		}
		info->data.bsv_count = tlv->length;
		memcpy(info->data.bsv, tlv->value,
				MIN(tlv->length, sizeof(info->data.bsv)));
		break;
	case TYPE_BMT:
		if (!tlv->length) {
			return KBVAS_ERROR_INVALID_FORMAT;
		}
		info->data.bmt_count = (uint8_t)tlv->length;
		memcpy(info->data.bmt, tlv->value,
				MIN(tlv->length, sizeof(info->data.bmt)));
		break;
#else /* KBVAS_USE_BASE64 */
	case TYPE_VIN: /* fall through */
	case TYPE_SOC: /* fall through */
	case TYPE_SOH: /* fall through */
	case TYPE_BPA: /* fall through */
	case TYPE_BPV: /* fall through */
	case TYPE_BSV: /* fall through */
	case TYPE_BMT:
		/* skip other types when using base64 encoding */
		break;
#endif
	default:
		KBVAS_ERROR("Unknown TLV type: 0x%02X", tlv->type);
		return KBVAS_ERROR_INVALID_TYPE;
	}

	return KBVAS_ERROR_NONE;
}

static kbvas_error_t process_tlv(const uint8_t *tlv, size_t tlv_len,
		struct kbvas_entry *info)
{
	struct tlv item;
	size_t bytes_parsed = 0;

	for (size_t i = 0; i < tlv_len; i += bytes_parsed) {
		if ((bytes_parsed = parse_tlv(&item, &tlv[i], tlv_len - i)) == 0) {
			KBVAS_ERROR("Failed to parse TLV");
			return KBVAS_ERROR_INVALID_FORMAT;
		}

		if (parse_battery(&item, info) != KBVAS_ERROR_NONE) {
			KBVAS_ERROR("Failed to parse battery info");
			return KBVAS_ERROR_INVALID_TYPE;
		}

		KBVAS_DEBUG("TLV type: 0x%02X, length: %d, value %p(%lu)",
				item.type, item.length, item.value,
				(uintptr_t)item.value - (uintptr_t)tlv);
	}

#if defined(KBVAS_USE_BASE64)
	const size_t len =
		MIN(tlv_len - MIN_TLV_LEN, sizeof(info->base64_encoded));

	memset(info->base64_encoded, 0, sizeof(info->base64_encoded));
	const size_t encoded_len = lm_base64_encode(info->base64_encoded,
			sizeof(info->base64_encoded), &tlv[MIN_TLV_LEN], len);
	KBVAS_DEBUG("%lu bytes of data encoded to %lu bytes", len, encoded_len);
	(void)encoded_len; /* Suppress unused warning when debug is disabled */
#else
	KBVAS_DEBUG("Parsed battery info %lu: %.*s, %u.%u %u, %u %u",
			info->timestamp, 17, info->data.vin,
			info->data.soc / 2, info->data.soc * 10 / 2 % 10,
			info->data.soh,
			info->data.bpa, info->data.bpv);
#endif
	return KBVAS_ERROR_NONE;
}

static void clear_all(struct kbvas *self)
{
	if (!self->backend->clear) {
		KBVAS_ERROR("No support for clear()");
		return;
	}

	struct kbvas_backend *backend = (struct kbvas_backend *)self->backend;
	kbvas_error_t err = (*self->backend->clear)(backend, self->backend_ctx);
	if (err != KBVAS_ERROR_NONE) {
		KBVAS_ERROR("Failed to clear all: %d", err);
	}
}

static void clear_entries(struct kbvas *self, size_t n)
{
	if (!self->backend->drop) {
		KBVAS_ERROR("No support for drop()");
		return;
	}

	struct kbvas_backend *backend = (struct kbvas_backend *)self->backend;
	kbvas_error_t err =
		(*self->backend->drop)(backend, n, self->backend_ctx);
	if (err != KBVAS_ERROR_NONE) {
		KBVAS_ERROR("Failed to drop %zu entries: %d", n, err);
	}
}

static size_t count_entries(struct kbvas *self)
{
	if (!self->backend->count) {
		KBVAS_ERROR("No support for count()");
		return 0;
	}

	struct kbvas_backend *backend = (struct kbvas_backend *)self->backend;
	size_t count = 0;
	kbvas_error_t err =
		(*self->backend->count)(backend, &count, self->backend_ctx);
	if (err != KBVAS_ERROR_NONE) {
		KBVAS_ERROR("Failed to count entries: %d", err);
		return 0;
	}

	return count;
}

static bool is_batch_ready(struct kbvas *self)
{
	return count_entries(self) >= self->batch_count;
}

void kbvas_clear(struct kbvas *self)
{
	if (self == NULL) {
		return;
	}

	clear_all(self);
}

void kbvas_clear_batch(struct kbvas *self)
{
	if (self == NULL) {
		return;
	}

	clear_entries(self, MIN(self->batch_count, count_entries(self)));
}

bool kbvas_is_batch_ready(struct kbvas *self)
{
	if (self == NULL) {
		return false;
	}

	return is_batch_ready(self);
}

kbvas_batch_count_t kbvas_get_batch_count(const struct kbvas *self)
{
	if (self == NULL) {
		return 0;
	}

	return self->batch_count;
}

void kbvas_set_batch_count(struct kbvas *self, kbvas_batch_count_t batch_count)
{
	if (self == NULL) {
		return;
	}

	if (batch_count > KBVAS_MAX_BATCH_COUNT) {
		KBVAS_ERROR("Batch count out of range: %u", batch_count);
		return;
	}

	self->batch_count = batch_count;
	KBVAS_INFO("Batch count set to %u", self->batch_count);
}

kbvas_error_t kbvas_peek(struct kbvas *self,
		int entry_index, struct kbvas_entry *entry)
{
	if (self == NULL || entry == NULL) {
		return KBVAS_ERROR_MISSING_PARAM;
	}

	if (!self->backend->peek) {
		return KBVAS_ERROR_UNSUPPORTED;
	}

	struct kbvas_backend *backend = (struct kbvas_backend *)self->backend;
	return (*self->backend->peek)(backend, entry_index,
			entry, self->backend_ctx);
}

kbvas_error_t kbvas_enqueue(struct kbvas *self,
		const void *data, size_t datasize)
{
	if (self == NULL || data == NULL) {
		return KBVAS_ERROR_MISSING_PARAM;
	}

	if (datasize < MIN_TLV_LEN) {
		return KBVAS_ERROR_INVALID_FORMAT;
	}

	if (!self->backend->push) {
		return KBVAS_ERROR_UNSUPPORTED;
	}

	struct kbvas_entry *entry = (struct kbvas_entry *)
		calloc(1, sizeof(*entry));

	if (entry == NULL) {
		return KBVAS_ERROR_OOM;
	}

	kbvas_error_t err = process_tlv(data, datasize, entry);

	if (err == KBVAS_ERROR_NONE) {
		struct kbvas_backend *backend =
			(struct kbvas_backend *)self->backend;
		err = (*self->backend->push)(backend, entry, self->backend_ctx);

		if (err == KBVAS_ERROR_NONE && self->batch_cb != NULL &&
				is_batch_ready(self)) {
			(*self->batch_cb)(self, self->batch_cb_ctx);
		}
	}

	free(entry);

	return err;
}

kbvas_error_t kbvas_dequeue(struct kbvas *self, struct kbvas_entry *entry)
{
	if (self == NULL) {
		return KBVAS_ERROR_MISSING_PARAM;
	}

	if (!self->backend->pop) {
		return KBVAS_ERROR_UNSUPPORTED;
	}

	struct kbvas_backend *backend = (struct kbvas_backend *)self->backend;
	return (*self->backend->pop)(backend, entry, self->backend_ctx);
}

void kbvas_iterate(struct kbvas *self, kbvas_iterator_t iterator, void *ctx)
{
	if (self == NULL || iterator == NULL) {
		return;
	}

	if (!self->backend->iterate) {
		KBVAS_ERROR("No support for iterate()");
		return;
	}

	struct kbvas_backend *backend = (struct kbvas_backend *)self->backend;
	kbvas_error_t err = (*self->backend->iterate)(backend,
			iterator, ctx, self);

	if (err != KBVAS_ERROR_NONE) {
		KBVAS_ERROR("Failed to iterate entries: %d", err);
	}
}

size_t kbvas_count(struct kbvas *self)
{
	if (self == NULL) {
		return 0;
	}

	return count_entries(self);
}

kbvas_error_t kbvas_register_batch_callback(struct kbvas *self,
		kbvas_batch_callback_t cb, void *cb_ctx)
{
	if (self == NULL) {
		return KBVAS_ERROR_MISSING_PARAM;
	}

	self->batch_cb = cb;
	self->batch_cb_ctx = cb_ctx;

	return KBVAS_ERROR_NONE;
}

struct kbvas *kbvas_create(struct kbvas_backend_api *api, void *backend_ctx)
{
	struct kbvas *self;

	if (!api || !(self = (struct kbvas *)calloc(1, sizeof(*self)))) {
		return NULL;
	}

	self->backend = api;
	self->backend_ctx = backend_ctx;
	self->batch_count = 1;

	return self;
}

void kbvas_destroy(struct kbvas *self)
{
	if (self == NULL) {
		return;
	}

	clear_all(self);
	free(self);
}
