/*
 * SPDX-FileCopyrightText: 2025 권경환 Kyunghwan Kwon <k@pazzk.net>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef KOREA_BATTERY_VAS_TLV_H
#define KOREA_BATTERY_VAS_TLV_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#define KBVAS_VENDOR_NAME			"kr.or.keco"

#if !defined(KBVAS_USE_RAW_ENCODING)
#define KBVAS_USE_BASE64
#endif

#if !defined(KBVAS_CELL_VOLTAGE_MAX_COUNT)
#define KBVAS_CELL_VOLTAGE_MAX_COUNT		192 /* up to 0xffff */
#endif

#if !defined(KBVAS_MODULE_TEMPERATURE_MAX_COUNT)
#define KBVAS_MODULE_TEMPERATURE_MAX_COUNT	20 /* up to 0xff */
#endif

typedef enum {
	KBVAS_ERROR_NONE			= 0,
	KBVAS_ERROR_INTERNAL			= 1,
	KBVAS_ERROR_MISSING_PARAM		= 2,
	KBVAS_ERROR_UNSUPPORTED_PARAM		= 3,
	KBVAS_ERROR_INVALID_TYPE		= 4,
	KBVAS_ERROR_OOM				= 5,
	KBVAS_ERROR_UNSPECIFIED			= 6,
	KBVAS_ERROR_UNSUPPORTED_REQUEST		= 7,
	KBVAS_ERROR_OUT_OF_RANGE_VALUE		= 8,
	KBVAS_ERROR_NOENT			= 9,
	KBVAS_ERROR_INVALID_FORMAT		= 10,
} kbvas_error_t;

typedef uint8_t kbvas_batch_count_t;

struct kbvas_data {
	uint8_t vin[17]; /* vehicle identification number */
	uint8_t soc; /* in the unit of 0.5% */
	uint8_t soh; /* in the unit of 1% */
	uint16_t bpa; /* battery pack current in the unit of 0.1A */
	uint16_t bpv; /* battery pack voltage in the unit of 0.1V */
	uint16_t bsv_count;
	uint8_t bsv[KBVAS_CELL_VOLTAGE_MAX_COUNT]; /* battery cell voltage
						in the unit of 0.02V */
	uint8_t bmt_count;
	uint8_t bmt[KBVAS_MODULE_TEMPERATURE_MAX_COUNT]; /* battery module
						temperature in the unit of 1C */
};

struct kbvas_entry {
	time_t timestamp;
#if defined(KBVAS_USE_BASE64)
	char base64_encoded[(sizeof(struct kbvas_data)+2)/3*4 + 1];
#else
	struct kbvas_data data;
#endif
};

struct kbvas;

/**
 * @brief Callback type for iterating kbvas entries.
 *
 * This function type is used as a callback for iterating over
 * the internal queue of kbvas entries. The callback receives each
 * entry and a user-defined context.
 *
 * @param[in] self   Pointer to the kbvas instance.
 * @param[in] entry  Pointer to the entry being visited.
 * @param[in] ctx    User-defined context provided to the iterate function.
 *
 * @retval true  Continue iteration.
 * @retval false Stop iteration early.
 */
typedef bool (*kbvas_iterator_t)(struct kbvas *self,
		const struct kbvas_entry *entry, void *ctx);

/**
 * @brief Creates and initializes a new kbvas instance.
 *
 * This function allocates memory for a new kbvas structure and
 * initializes its internal state. The caller is responsible for
 * managing the lifecycle of the created instance, including
 * freeing the allocated memory when it is no longer needed.
 *
 * @return A pointer to the newly created kbvas instance, or NULL
 *         if the allocation fails.
 */
struct kbvas *kbvas_create(void);

/**
 * @brief Destroys a kbvas instance and releases its resources.
 *
 * This function deallocates the memory associated with the given
 * kbvas instance and performs any necessary cleanup of its internal
 * state. After calling this function, the kbvas instance should no
 * longer be used.
 *
 * @param[in] self A pointer to the kbvas instance to be destroyed.
 */
void kbvas_destroy(struct kbvas *self);

/**
 * @brief Clears all data from the kbvas instance.
 *
 * This function resets the internal state of the kbvas instance,
 * removing all queued data and preparing it for reuse.
 *
 * @param[in] self A pointer to the kbvas instance to be cleared.
 */
void kbvas_clear(struct kbvas *self);

/**
 * @brief Removes a batch of entries from the kbvas instance.
 *
 * This function removes up to the configured batch count of entries
 * from the internal queue. It is typically called after a successful
 * transmission of the batched data.
 *
 * @note If fewer than the batch count of entries exist, all entries are removed.
 *
 * @param[in] self Pointer to the kbvas instance.
 */
void kbvas_clear_batch(struct kbvas *self);

/**
 * @brief Sets the batch count for the kbvas instance.
 *
 * This function configures the number of entries to be processed
 * as a single batch in the kbvas instance.
 *
 * @param[in] self A pointer to the kbvas instance.
 * @param[in] batch_count The number of entries per batch.
 */
void kbvas_set_batch_count(struct kbvas *self, kbvas_batch_count_t batch_count);

/**
 * @brief Retrieves the current batch count of the kbvas instance.
 *
 * This function returns the number of entries configured to be
 * processed as a single batch.
 *
 * @param[in] self A pointer to the kbvas instance.
 *
 * @return The current batch count.
 */
kbvas_batch_count_t kbvas_get_batch_count(const struct kbvas *self);

/**
 * @brief Checks if the current batch is ready for processing.
 *
 * This function determines whether the number of queued entries
 * has reached the configured batch count.
 *
 * @param[in] self A pointer to the kbvas instance.
 *
 * @return True if the batch is ready, false otherwise.
 */
bool kbvas_is_batch_ready(const struct kbvas *self);

/**
 * @brief Enqueues data into the kbvas instance.
 *
 * This function adds a new data entry to the queue in the kbvas
 * instance.
 *
 * @param[in] self A pointer to the kbvas instance.
 * @param[in] data A pointer to the data to be enqueued.
 * @param[in] datasize The size of the data in bytes.
 *
 * @return A kbvas_error_t indicating the result of the operation.
 */
kbvas_error_t kbvas_enqueue(struct kbvas *self,
		const void *data, size_t datasize);

/**
 * @brief Removes and retrieves the oldest entry from the kbvas queue.
 *
 * @param[in] self A pointer to the kbvas instance.
 * @param[out] entry A pointer to a kbvas_entry structure to store the dequeued
 *             data.
 *
 * @return A kbvas_error_t indicating the result of the operation.
 */
kbvas_error_t kbvas_dequeue(struct kbvas *self, struct kbvas_entry *entry);

/**
 * @brief Peeks at the next entry in the kbvas queue.
 *
 * This function retrieves the next entry in the queue without
 * removing it, allowing the caller to inspect the data.
 *
 * @param[in] self A pointer to the kbvas instance.
 * @param[out] entry A pointer to a kbvas_entry structure to store the peeked
 *             data.
 *
 * @return A kbvas_error_t indicating the result of the operation.
 */
kbvas_error_t kbvas_peek(struct kbvas *self, struct kbvas_entry *entry);

/**
 * @brief Iterates over queued entries in the kbvas instance.
 *
 * This function traverses the internal queue of kbvas entries, invoking
 * the provided callback function for each entry, up to the specified
 * maximum number of entries (`max_count`). Iteration stops if:
 *   - the queue is exhausted,
 *   - or the callback function returns `false`.
 *
 * The callback function receives a pointer to each entry and a user-defined
 * context. This allows for batch processing, filtering, or data aggregation.
 *
 * @note If `max_count` is 0, all entries in the queue will be iterated over.
 *
 * @param[in] self       Pointer to the kbvas instance.
 * @param[in] iterator   Callback function to invoke for each entry.
 * @param[in] max_count  Maximum number of entries to iterate over. If 0, no limit is applied.
 * @param[in] ctx        User-defined context passed to the callback.
 */
void kbvas_iterate(struct kbvas *self, kbvas_iterator_t iterator,
		kbvas_batch_count_t max_count, void *ctx);

#if defined(__cplusplus)
}
#endif

#endif /* KOREA_BATTERY_VAS_TLV_H */
