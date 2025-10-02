/*
 * SPDX-FileCopyrightText: 2025 권경환 Kyunghwan Kwon <k@pazzk.net>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef KOREA_BATTERY_VAS_MEMORY_BACKEND_H
#define KOREA_BATTERY_VAS_MEMORY_BACKEND_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "kbvas.h"

struct kbvas_backend_api *kbvas_memory_backend_create(void);
void kbvas_memory_backend_destroy(struct kbvas_backend_api *backend);

#if defined(__cplusplus)
}
#endif

#endif /* KOREA_BATTERY_VAS_MEMORY_BACKEND_H */
