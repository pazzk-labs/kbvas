/*
 * SPDX-FileCopyrightText: 2025 권경환 Kyunghwan Kwon <k@pazzk.net>
 *
 * SPDX-License-Identifier: MIT
 */

#include "kbvas_memory_backend.h"
#include <stdlib.h>
#include <string.h>
#include "libmcu/list.h"

struct kbvas_backend {
	struct kbvas_backend_api api;
	struct list entries;
	size_t count;
};

struct entry {
	struct kbvas_entry entry;
	struct list link;
};

static void clear_all(struct kbvas_backend *self)
{
	struct list *p;
	struct list *t;

	list_for_each_safe(p, t, &self->entries) {
		struct entry *entry = list_entry(p, struct entry, link);
		list_del(&entry->link, &self->entries);
		free(entry);
	}
}

static void clear_entries(struct kbvas_backend *self, size_t n)
{
	struct list *p;
	struct list *t;

	if (n == 0) {
		return;
	}

	list_for_each_safe(p, t, &self->entries) {
		if (n-- == 0) {
			break;
		}

		struct entry *entry = list_entry(p, struct entry, link);
		list_del(&entry->link, &self->entries);
		free(entry);
	}
}

static size_t count_entries(const struct kbvas_backend *self)
{
	return (size_t)list_count(&self->entries);
}

static kbvas_error_t do_push(struct kbvas_backend *self,
		const struct kbvas_entry *entry, void *ctx)
{
	struct entry *p = (struct entry *)calloc(1, sizeof(*p));
	if (!p) {
		return KBVAS_ERROR_NOSPC;
	}
	memcpy(&p->entry, entry, sizeof(*entry));
	list_add_tail(&p->link, &self->entries);
	return KBVAS_ERROR_NONE;
}

static kbvas_error_t do_pop(struct kbvas_backend *self,
		struct kbvas_entry *entry, void *ctx)
{
	if (list_empty(&self->entries)) {
		return KBVAS_ERROR_NOENT;
	}

	struct entry *p = list_entry(list_first(&self->entries),
			struct entry, link);

	if (entry) {
		memcpy(entry, &p->entry, sizeof(*entry));
	}

	list_del(&p->link, &self->entries);
	free(p);

	return KBVAS_ERROR_NONE;
}

static kbvas_error_t do_peek(struct kbvas_backend *self,
		struct kbvas_entry *entry, void *ctx)
{
	if (list_empty(&self->entries)) {
		return KBVAS_ERROR_NOENT;
	}

	struct entry *p = list_entry(list_first(&self->entries),
			struct entry, link);

	memcpy(entry, &p->entry, sizeof(*entry));

	return KBVAS_ERROR_NONE;
}

static kbvas_error_t do_drop(struct kbvas_backend *self, size_t n, void *ctx)
{
	if (n == 0) {
		return KBVAS_ERROR_MISSING_PARAM;
	}

	clear_entries(self, n);

	return KBVAS_ERROR_NONE;
}

static kbvas_error_t do_clear(struct kbvas_backend *self, void *ctx)
{
	if (self) {
		clear_all(self);
	}
	return KBVAS_ERROR_NONE;
}

static kbvas_error_t do_count(struct kbvas_backend *self,
		size_t *count, void *ctx)
{
	(void)ctx;

	if (count == NULL) {
		return KBVAS_ERROR_MISSING_PARAM;
	}

	if (self) {
		*count = count_entries(self);
	} else {
		*count = 0;
	}

	return KBVAS_ERROR_NONE;
}

static kbvas_error_t do_iterate(struct kbvas_backend *self,
		kbvas_iterator_t iterator,
		void *iterator_ctx, struct kbvas *kbvas_instance)
{
	if (self == NULL || iterator == NULL) {
		return KBVAS_ERROR_MISSING_PARAM;
	}

	struct list *p;
	list_for_each(p, &self->entries) {
		struct entry *e = list_entry(p, struct entry, link);
		if (!(*iterator)(kbvas_instance, &e->entry, iterator_ctx)) {
			break;
		}
	}

	return KBVAS_ERROR_NONE;
}

struct kbvas_backend_api *kbvas_memory_backend_create(void)
{
	struct kbvas_backend *backend;

	if (!(backend = (struct kbvas_backend *)calloc(1, sizeof(*backend)))) {
		return NULL;
	}

	*backend = (struct kbvas_backend) {
		.api = {
			.push = do_push,
			.pop = do_pop,
			.peek = do_peek,
			.drop = do_drop,
			.clear = do_clear,
			.count = do_count,
			.iterate = do_iterate,
		},
	};

	list_init(&backend->entries);

	return &backend->api;
}

void kbvas_memory_backend_destroy(struct kbvas_backend_api *backend)
{
	if (backend) {
		struct kbvas_backend *self = (struct kbvas_backend *)backend;
		backend->clear(self, NULL);
		free(backend);
	}
}
